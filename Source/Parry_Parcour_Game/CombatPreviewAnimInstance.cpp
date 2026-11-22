#include "CombatPreviewAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"

struct FCombatPreviewNode : FAnimNode_Base
{
    FAnimNode_SequencePlayer_Standalone Current, Previous;
    UAnimSequence* Wanted = nullptr;
    float Rate = 1.f, Blend = 1.f;
    bool Loop = true, Pending = false, HasPrevious = false;
    FVector GuardDirection=FVector(-.65,.35,.7);
    void AimGuard(FPoseContext& Out)
    {
        if (!Wanted || !Wanted->GetName().Contains(TEXT("Hero_Parry"))) return;
        const FBoneContainer& Bones=Out.Pose.GetBoneContainer();
        const FReferenceSkeleton& Ref=Bones.GetReferenceSkeleton();
        int32 HandRef=INDEX_NONE;
        for (int32 I=0;I<Ref.GetNum();++I) if (Ref.GetBoneName(I).ToString().EndsWith(TEXT("RightHand"))) { HandRef=I; break; }
        const int32 BaseRef=Ref.FindBoneIndex(TEXT("BladeBase")), TipRef=Ref.FindBoneIndex(TEXT("BladeTip"));
        if (HandRef==INDEX_NONE || BaseRef==INDEX_NONE || TipRef==INDEX_NONE) return;
        const FCompactPoseBoneIndex Hand=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandRef));
        const FCompactPoseBoneIndex Base=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(BaseRef));
        const FCompactPoseBoneIndex Tip=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(TipRef));
        if (Hand.GetInt()==INDEX_NONE || Base.GetInt()==INDEX_NONE || Tip.GetInt()==INDEX_NONE) return;
        FCSPose<FCompactPose> CS;
        CS.InitPose(Out.Pose);
        const FVector Axis=(CS.GetComponentSpaceTransform(Tip).GetLocation()-CS.GetComponentSpaceTransform(Base).GetLocation()).GetSafeNormal();
        const FVector GuardAxis=GuardDirection.GetSafeNormal();
        FTransform HandCS=CS.GetComponentSpaceTransform(Hand);
        const FQuat Turn=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(Axis,GuardAxis),Blend);
        HandCS.SetRotation(Turn*HandCS.GetRotation());
        const FCompactPoseBoneIndex Parent=Out.Pose.GetParentBoneIndex(Hand);
        Out.Pose[Hand]=HandCS.GetRelativeTransform(CS.GetComponentSpaceTransform(Parent));
    }
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override
    {
        Current.Initialize_AnyThread(C);
        Previous.Initialize_AnyThread(C);
    }
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override
    {
        Current.CacheBones_AnyThread(C);
        Previous.CacheBones_AnyThread(C);
    }
    virtual void Update_AnyThread(const FAnimationUpdateContext& C) override
    {
        if (Pending)
        {
            HasPrevious = Current.GetSequence() != nullptr;
            Previous = Current;
            Current.SetSequence(Wanted);
            Current.SetLoopAnimation(Loop);
            Current.SetPlayRate(Rate);
            Current.SetStartPosition(0.f);
            Current.Initialize_AnyThread(FAnimationInitializeContext(C.AnimInstanceProxy));
            Current.CacheBones_AnyThread(FAnimationCacheBonesContext(C.AnimInstanceProxy));
            Blend = HasPrevious ? 0.f : 1.f;
            Pending = false;
        }
        Blend = FMath::Min(1.f, Blend + C.GetDeltaTime() / .12f);
        Current.SetPlayRate(Rate);
        Current.Update_AnyThread(C.FractionalWeight(Blend));
        if (HasPrevious && Blend < 1.f) Previous.Update_AnyThread(C.FractionalWeight(1.f-Blend));
    }
    virtual void Evaluate_AnyThread(FPoseContext& Out) override
    {
        if (!Current.GetSequence()) { Out.ResetToRefPose(); return; }
        if (!HasPrevious || Blend >= 1.f) { Current.Evaluate_AnyThread(Out); AimGuard(Out); return; }
        FPoseContext A(Out), B(Out);
        Previous.Evaluate_AnyThread(A);
        Current.Evaluate_AnyThread(B);
        FAnimationPoseData AD(A), BD(B), OD(Out);
        FAnimationRuntime::BlendTwoPosesTogether(AD, BD, 1.f-Blend, OD);
        AimGuard(Out);
    }
};

struct FCombatPreviewProxy : FAnimInstanceProxy
{
    FCombatPreviewNode Node;
    int32 SeenRevision = -1;
    FCombatPreviewProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Node; }
    virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Delta);
        auto* Anim = CastChecked<UCombatPreviewAnimInstance>(Instance);
        Node.Rate = Anim->Rate;
        Node.Loop = Anim->bLoop;
        Node.GuardDirection=Anim->GuardDirection;
        if (SeenRevision != Anim->Revision)
        {
            Node.Wanted = Anim->Clip;
            Node.Pending = true;
            SeenRevision = Anim->Revision;
        }
    }
};

void UCombatPreviewAnimInstance::PlayClip(UAnimSequence* InClip, bool Loop, float PlayRate, bool Restart)
{
    if (!InClip) return;
    if (Clip != InClip || Restart) { Clip = InClip; ++Revision; }
    bLoop = Loop;
    Rate = PlayRate;
}
FAnimInstanceProxy* UCombatPreviewAnimInstance::CreateAnimInstanceProxy() { return new FCombatPreviewProxy(this); }
void UCombatPreviewAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
