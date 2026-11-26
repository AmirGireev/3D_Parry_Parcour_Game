#include "CombatPreviewAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Animation/AttributesRuntime.h"

struct FCombatPreviewNode : FAnimNode_Base
{
    FAnimNode_SequencePlayer_Standalone Current, Previous;
    UAnimSequence* Wanted = nullptr;
    float Rate = 1.f, Blend = 1.f;
    bool Loop = true, Pending = false, HasPrevious = false;
    FVector GuardDirection=FVector(-.65,.35,.7);
    bool LimitArms=false;
    bool SnapNext=false; // next clip switch happens without crossfade
    float ArmSpread=0.f, ArmSpreadNow=0.f;
    // Right-hand grip: the finger bones (everything below the right hand) are copied from a reference clip
    // frame where the hand holds the sword, so the fist looks like in the original animation.
    UAnimSequence* GripClip=nullptr;
    float GripTime=0.f;
    void GripRightHand(FPoseContext& Out)
    {
        if (!GripClip) return;
        const FBoneContainer& Bones=Out.Pose.GetBoneContainer();
        int32 HandRef=Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("RightHand"));
        if (HandRef==INDEX_NONE) HandRef=Bones.GetReferenceSkeleton().FindBoneIndex(TEXT("hand_r"));
        if (HandRef==INDEX_NONE) return;
        const FCompactPoseBoneIndex Hand=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandRef));
        if (Hand.GetInt()==INDEX_NONE) return;
        FPoseContext Ref(Out);
        FBlendedCurve Curve; Curve.InitFrom(Out.Curve);
        UE::Anim::FStackAttributeContainer Attr;
        FAnimationPoseData Data(Ref.Pose, Curve, Attr);
        GripClip->GetAnimationPose(Data, FAnimExtractContext(double(GripTime), false));
        for (FCompactPoseBoneIndex I : Out.Pose.ForEachBoneIndex())
            if (Bones.BoneIsChildOf(I, Hand)) Out.Pose[I]=Ref.Pose[I];
    }    // Rotates both upper arms away from the body (sideways) by ArmSpreadNow degrees.
    void SpreadArms(FPoseContext& Out)
    {
        if (ArmSpreadNow <= .01f) return;
        const FBoneContainer& Bones=Out.Pose.GetBoneContainer();
        const FReferenceSkeleton& Ref=Bones.GetReferenceSkeleton();
        FCSPose<FCompactPose> CS;
        CS.InitPose(Out.Pose);
        static const TCHAR* ArmBones[][2] = {
            {TEXT("upperarm_l"), TEXT("lowerarm_l")}, {TEXT("upperarm_r"), TEXT("lowerarm_r")},
            {TEXT("LeftArm"), TEXT("LeftForeArm")}, {TEXT("RightArm"), TEXT("RightForeArm")}};
        for (const auto& Pair : ArmBones)
        {
            const int32 UpperRef=Ref.FindBoneIndex(FName(Pair[0])), LowerRef=Ref.FindBoneIndex(FName(Pair[1]));
            if (UpperRef==INDEX_NONE || LowerRef==INDEX_NONE) continue;
            const FCompactPoseBoneIndex Upper=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(UpperRef));
            const FCompactPoseBoneIndex Lower=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(LowerRef));
            if (Upper.GetInt()==INDEX_NONE || Lower.GetInt()==INDEX_NONE) continue;
            FTransform UpperCS=CS.GetComponentSpaceTransform(Upper);
            const FVector Dir=(CS.GetComponentSpaceTransform(Lower).GetLocation()-UpperCS.GetLocation()).GetSafeNormal();
            // Mesh space: X is sideways (left arm at +X), Y forward, Z up.
            const FVector OutSide(FMath::Sign(UpperCS.GetLocation().X), 0.f, 0.f);
            FVector Axis=FVector::CrossProduct(Dir, OutSide);
            if (OutSide.IsZero() || !Axis.Normalize()) continue;
            UpperCS.SetRotation(FQuat(Axis, FMath::DegreesToRadians(ArmSpreadNow))*UpperCS.GetRotation());
            const FCompactPoseBoneIndex Parent=Out.Pose.GetParentBoneIndex(Upper);
            Out.Pose[Upper]=UpperCS.GetRelativeTransform(CS.GetComponentSpaceTransform(Parent));
        }
    }
    float ArmDrop=0.f;
    void LowerArms(FPoseContext& Out)
    {
        if (!LimitArms) return;
        const FBoneContainer& Bones=Out.Pose.GetBoneContainer();
        const FReferenceSkeleton& Ref=Bones.GetReferenceSkeleton();
        FCSPose<FCompactPose> CS;
        CS.InitPose(Out.Pose);
        // Rotation that turns direction Dir ArmDrop degrees further down than the clip
        // (never past almost straight down), keeping its heading.
        const float DropRad=FMath::DegreesToRadians(ArmDrop);
        const float FloorRad=FMath::DegreesToRadians(-85.f);
        if (DropRad<=0.f) return;
        auto DownTo=[DropRad,FloorRad](const FVector& Dir)->FQuat
        {
            if (Dir.IsNearlyZero()) return FQuat::Identity;
            const float Elev=FMath::Asin(FMath::Clamp(Dir.Z,-1.f,1.f));
            const float Want=FMath::Max(Elev-DropRad, FMath::Min(FloorRad, Elev));
            if (Want>=Elev-KINDA_SMALL_NUMBER) return FQuat::Identity;
            FVector Flat(Dir.X,Dir.Y,0.f);
            if (!Flat.Normalize()) return FQuat::Identity;
            return FQuat::FindBetweenNormals(Dir, Flat*FMath::Cos(Want)+FVector(0,0,FMath::Sin(Want)));
        };
        // Upper arm, forearm, hand: UE mannequin names or Mixamo names.
        static const TCHAR* ArmBones[][3] = {
            {TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l")}, {TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")},
            {TEXT("LeftArm"), TEXT("LeftForeArm"), TEXT("LeftHand")}, {TEXT("RightArm"), TEXT("RightForeArm"), TEXT("RightHand")}};
        for (const auto& Chain : ArmBones)
        {
            const int32 UpperRef=Ref.FindBoneIndex(FName(Chain[0]));
            const int32 LowerRef=Ref.FindBoneIndex(FName(Chain[1]));
            const int32 HandRef=Ref.FindBoneIndex(FName(Chain[2]));
            if (UpperRef==INDEX_NONE || LowerRef==INDEX_NONE || HandRef==INDEX_NONE) continue;
            const FCompactPoseBoneIndex Upper=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(UpperRef));
            const FCompactPoseBoneIndex Lower=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(LowerRef));
            const FCompactPoseBoneIndex Hand=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandRef));
            if (Upper.GetInt()==INDEX_NONE || Lower.GetInt()==INDEX_NONE || Hand.GetInt()==INDEX_NONE) continue;
            FTransform UpperCS=CS.GetComponentSpaceTransform(Upper);
            FTransform LowerCS=CS.GetComponentSpaceTransform(Lower);
            const FVector Shoulder=UpperCS.GetLocation();
            // Upper arm (shoulder -> elbow) lowered; the forearm and hand swing along.
            const FQuat DU=DownTo((LowerCS.GetLocation()-Shoulder).GetSafeNormal());
            UpperCS.SetRotation(DU*UpperCS.GetRotation());
            LowerCS.SetLocation(Shoulder+DU.RotateVector(LowerCS.GetLocation()-Shoulder));
            LowerCS.SetRotation(DU*LowerCS.GetRotation());
            const FCompactPoseBoneIndex Parent=Out.Pose.GetParentBoneIndex(Upper);
            Out.Pose[Upper]=UpperCS.GetRelativeTransform(CS.GetComponentSpaceTransform(Parent));
            Out.Pose[Lower]=LowerCS.GetRelativeTransform(UpperCS);
        }
    }
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
    // Upper-body overlay: blends OverlayRoot and all its children from a second clip on top of the base pose.
    FAnimNode_SequencePlayer_Standalone Overlay;
    UAnimSequence* OverlayWanted = nullptr;
    FName OverlayRoot = NAME_None;
    float OverlayRate = 1.f, OverlayAlpha = 0.f, OverlayBlendTime = .08f;
    bool OverlayActive = false, OverlayPending = false;
    void ApplyOverlay(FPoseContext& Out)
    {
        if (OverlayAlpha <= 0.f || !Overlay.GetSequence() || OverlayRoot.IsNone()) return;
        const FBoneContainer& Bones=Out.Pose.GetBoneContainer();
        const int32 RootRef=Bones.GetReferenceSkeleton().FindBoneIndex(OverlayRoot);
        if (RootRef==INDEX_NONE) return;
        const FCompactPoseBoneIndex Root=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RootRef));
        if (Root.GetInt()==INDEX_NONE) return;
        FPoseContext Layer(Out);
        Overlay.Evaluate_AnyThread(Layer);
        for (FCompactPoseBoneIndex I : Out.Pose.ForEachBoneIndex())
        {
            if (I!=Root && !Bones.BoneIsChildOf(I,Root)) continue;
            FTransform T=Out.Pose[I];
            T.BlendWith(Layer.Pose[I],OverlayAlpha);
            Out.Pose[I]=T;
        }
    }
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override
    {
        Current.Initialize_AnyThread(C);
        Previous.Initialize_AnyThread(C);
        Overlay.Initialize_AnyThread(C);
    }
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override
    {
        Current.CacheBones_AnyThread(C);
        Previous.CacheBones_AnyThread(C);
        Overlay.CacheBones_AnyThread(C);
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
            // Negative rate = play backwards, so start at the end of the clip.
            // (also when the clip's own Rate Scale is negative)
            Current.SetStartPosition(Wanted && Rate * Wanted->RateScale < 0.f ? Wanted->GetPlayLength() : 0.f);
            Current.Initialize_AnyThread(FAnimationInitializeContext(C.AnimInstanceProxy));
            Current.CacheBones_AnyThread(FAnimationCacheBonesContext(C.AnimInstanceProxy));
            Blend = (HasPrevious && !SnapNext) ? 0.f : 1.f;
            Pending = false;
        }
        Blend = FMath::Min(1.f, Blend + C.GetDeltaTime() / .12f);
        // Arm spread fades in/out over ~0.2 s so switching clips does not snap the arms.
        ArmSpreadNow = FMath::FInterpConstantTo(ArmSpreadNow, ArmSpread, C.GetDeltaTime(), 60.f);
        Current.SetPlayRate(Rate);
        Current.Update_AnyThread(C.FractionalWeight(Blend));
        if (HasPrevious && Blend < 1.f) Previous.Update_AnyThread(C.FractionalWeight(1.f-Blend));
        if (OverlayPending)
        {
            Overlay.SetSequence(OverlayWanted);
            Overlay.SetLoopAnimation(false);
            Overlay.SetStartPosition(0.f);
            Overlay.Initialize_AnyThread(FAnimationInitializeContext(C.AnimInstanceProxy));
            Overlay.CacheBones_AnyThread(FAnimationCacheBonesContext(C.AnimInstanceProxy));
            OverlayPending = false;
        }
        const float Step = C.GetDeltaTime() / FMath::Max(.01f, OverlayBlendTime);
        OverlayAlpha = FMath::Clamp(OverlayAlpha + (OverlayActive ? Step : -Step), 0.f, 1.f);
        if (OverlayAlpha > 0.f && Overlay.GetSequence())
        {
            Overlay.SetPlayRate(OverlayRate);
            Overlay.Update_AnyThread(C.FractionalWeight(OverlayAlpha));
        }
    }
    virtual void Evaluate_AnyThread(FPoseContext& Out) override
    {
        if (!Current.GetSequence()) { Out.ResetToRefPose(); return; }
        if (!HasPrevious || Blend >= 1.f) { Current.Evaluate_AnyThread(Out); AimGuard(Out); LowerArms(Out); SpreadArms(Out); ApplyOverlay(Out); GripRightHand(Out); return; }
        FPoseContext A(Out), B(Out);
        Previous.Evaluate_AnyThread(A);
        Current.Evaluate_AnyThread(B);
        FAnimationPoseData AD(A), BD(B), OD(Out);
        FAnimationRuntime::BlendTwoPosesTogether(AD, BD, 1.f-Blend, OD);
        AimGuard(Out);
        LowerArms(Out);
        SpreadArms(Out);
        ApplyOverlay(Out);
        GripRightHand(Out);
    }
};

struct FCombatPreviewProxy : FAnimInstanceProxy
{
    FCombatPreviewNode Node;
    int32 SeenRevision = -1;
    int32 SeenOverlayRevision = -1;
    FCombatPreviewProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Node; }
    virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Delta);
        auto* Anim = CastChecked<UCombatPreviewAnimInstance>(Instance);
        Node.Rate = Anim->Rate;
        Node.Loop = Anim->bLoop;
        Node.SnapNext = Anim->bSnapNextClip;
        Node.ArmSpread = Anim->ArmSpread;
        Node.GripClip = Anim->GripPoseClip;
        Node.GripTime = Anim->GripPoseTime;
        Node.GuardDirection=Anim->GuardDirection;
        Node.LimitArms=Anim->bLimitArms;
        Node.ArmDrop=Anim->ArmDrop;
        Node.OverlayActive=Anim->bOverlayActive;
        Node.OverlayRate=Anim->OverlayRate;
        Node.OverlayRoot=Anim->OverlayRoot;
        Node.OverlayBlendTime=Anim->OverlayBlendTime;
        if (SeenOverlayRevision != Anim->OverlayRevision)
        {
            Node.OverlayWanted = Anim->OverlayClip;
            Node.OverlayPending = Anim->OverlayClip != nullptr;
            SeenOverlayRevision = Anim->OverlayRevision;
        }
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
