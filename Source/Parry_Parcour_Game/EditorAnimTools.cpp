// Editor-only console helpers for cutting animation clips.
//   Parry.AnimInfo <AnimPath>                 logs pelvis height and lowest foot height per frame
//   Parry.TrimAnim <AnimPath> <Start> <End>   keeps only [Start, End] seconds of the clip (marks it dirty, save afterwards)
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimData/IAnimationDataController.h"

namespace ParryAnimTools
{
static UAnimSequence* LoadAnim(const FString& Path)
{
    FString Full = Path;
    if (!Full.Contains(TEXT("."))) Full += TEXT(".") + FPackageName::GetShortName(Path);
    UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *Full);
    if (!Seq) UE_LOG(LogTemp, Error, TEXT("ParryAnim: could not load %s"), *Full);
    return Seq;
}

// UE mannequin "pelvis" or Mixamo "Hips".
static FName PelvisName(const FReferenceSkeleton& Ref)
{
    return Ref.FindBoneIndex(TEXT("pelvis")) != INDEX_NONE ? FName(TEXT("pelvis")) : FName(TEXT("Hips"));
}

static FTransform ComponentTransform(const IAnimationDataModel* Model, const FReferenceSkeleton& Ref, int32 Bone, const FFrameNumber& Frame)
{
    FTransform Result = FTransform::Identity;
    while (Bone != INDEX_NONE)
    {
        const FName Name = Ref.GetBoneName(Bone);
        const FTransform Local = Model->IsValidBoneTrackName(Name) ? Model->GetBoneTrackTransform(Name, Frame) : Ref.GetRefBonePose()[Bone];
        Result = Result * Local;
        Bone = Ref.GetParentIndex(Bone);
    }
    return Result;
}

static void AnimInfo(const TArray<FString>& Args)
{
    if (Args.Num() < 1) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: Parry.AnimInfo <AnimPath>")); return; }
    UAnimSequence* Seq = LoadAnim(Args[0]);
    if (!Seq || !Seq->GetSkeleton()) return;
    const IAnimationDataModel* Model = Seq->GetDataModel();
    const FReferenceSkeleton& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
    const int32 Pelvis = Ref.FindBoneIndex(PelvisName(Ref));
    int32 FootL = Ref.FindBoneIndex(TEXT("foot_l")), FootR = Ref.FindBoneIndex(TEXT("foot_r"));
    if (FootL == INDEX_NONE) FootL = Ref.FindBoneIndex(TEXT("LeftFoot"));
    if (FootR == INDEX_NONE) FootR = Ref.FindBoneIndex(TEXT("RightFoot"));
    const FFrameRate Rate = Model->GetFrameRate();
    const int32 Frames = Model->GetNumberOfFrames();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim INFO %s frames=%d rate=%s length=%.3f"), *Seq->GetName(), Frames, *Rate.ToPrettyText().ToString(), Seq->GetPlayLength());
    for (int32 F = 0; F <= Frames; ++F)
    {
        const FFrameNumber N(F);
        const float P = Pelvis != INDEX_NONE ? ComponentTransform(Model, Ref, Pelvis, N).GetLocation().Z : 0.f;
        const float L = FootL != INDEX_NONE ? ComponentTransform(Model, Ref, FootL, N).GetLocation().Z : 0.f;
        const float R = FootR != INDEX_NONE ? ComponentTransform(Model, Ref, FootR, N).GetLocation().Z : 0.f;
        UE_LOG(LogTemp, Display, TEXT("ParryAnim F %d t=%.3f pelvis=%.4f footL=%.4f footR=%.4f"), F, Rate.AsSeconds(N), P, L, R);
    }
    // Full positions of pelvis and hands at the first frame (useful to place hanging poses).
    for (const TCHAR* Bone : {TEXT("Hips"), TEXT("pelvis"), TEXT("LeftHand"), TEXT("RightHand"), TEXT("hand_l"), TEXT("hand_r")})
    {
        const int32 B = Ref.FindBoneIndex(Bone);
        if (B == INDEX_NONE) continue;
        const FVector V = ComponentTransform(Model, Ref, B, FFrameNumber(0)).GetLocation();
        UE_LOG(LogTemp, Display, TEXT("ParryAnim POS %s x=%.4f y=%.4f z=%.4f"), Bone, V.X, V.Y, V.Z);
    }
}

static void TrimAnim(const TArray<FString>& Args)
{
    if (Args.Num() < 3) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: Parry.TrimAnim <AnimPath> <StartSeconds> <EndSeconds>")); return; }
    UAnimSequence* Seq = LoadAnim(Args[0]);
    if (!Seq) return;
    const IAnimationDataModel* Model = Seq->GetDataModel();
    const FFrameRate Rate = Model->GetFrameRate();
    const int32 Frames = Model->GetNumberOfFrames();
    const int32 S = FMath::Clamp(Rate.AsFrameNumber(FCString::Atod(*Args[1])).Value, 0, Frames - 1);
    const int32 E = FMath::Clamp(Rate.AsFrameNumber(FCString::Atod(*Args[2])).Value, S + 1, Frames);
    bool bOk = true;
    // Keys run 0..Frames (inclusive), so drop keys E+1..Frames to make key E the new last key.
    if (E < Frames) bOk &= UE::Anim::AnimationData::Trim(Seq, TRange<FFrameNumber>(FFrameNumber(E + 1), FFrameNumber(Frames + 1)));
    if (S > 0) bOk &= UE::Anim::AnimationData::Trim(Seq, TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(S)));
    if (!bOk) UE_LOG(LogTemp, Error, TEXT("ParryAnim: trim failed for %s"), *Seq->GetName());
    Seq->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim TRIM %s frames %d..%d of %d -> length %.3f"), *Seq->GetName(), S, E, Frames, Seq->GetPlayLength());
}

// Parry.ClampPelvis <AnimPath> <MaxZ>: pelvis never goes higher than MaxZ (component space, clip units),
// removes the baked "body rises with the jump" offset while keeping crouches.
static void ClampPelvis(const TArray<FString>& Args)
{
    if (Args.Num() < 2) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: Parry.ClampPelvis <AnimPath> <MaxZ>")); return; }
    UAnimSequence* Seq = LoadAnim(Args[0]);
    if (!Seq || !Seq->GetSkeleton()) return;
    const float MaxZ = FCString::Atof(*Args[1]);
    const IAnimationDataModel* Model = Seq->GetDataModel();
    const FReferenceSkeleton& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
    const FName PelvisBone = PelvisName(Ref);
    const int32 Pelvis = Ref.FindBoneIndex(PelvisBone);
    if (Pelvis == INDEX_NONE || !Model->IsValidBoneTrackName(PelvisBone)) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: no pelvis track")); return; }
    const int32 Parent = Ref.GetParentIndex(Pelvis);
    const int32 Keys = Model->GetNumberOfKeys();
    TArray<FVector> Pos; TArray<FQuat> Rot; TArray<FVector> Scale;
    int32 Changed = 0;
    for (int32 K = 0; K < Keys; ++K)
    {
        const FFrameNumber N(K);
        const FTransform Local = Model->GetBoneTrackTransform(PelvisBone, N);
        const FTransform ParentCS = Parent != INDEX_NONE ? ComponentTransform(Model, Ref, Parent, N) : FTransform::Identity;
        FTransform PelvisCS = Local * ParentCS;
        FVector L = PelvisCS.GetLocation();
        FTransform NewLocal = Local;
        if (L.Z > MaxZ)
        {
            L.Z = MaxZ;
            PelvisCS.SetLocation(L);
            NewLocal = PelvisCS.GetRelativeTransform(ParentCS);
            ++Changed;
        }
        Pos.Add(NewLocal.GetLocation()); Rot.Add(NewLocal.GetRotation()); Scale.Add(NewLocal.GetScale3D());
    }
    IAnimationDataController& Controller = Seq->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Clamp pelvis")));
    const bool bOk = Controller.SetBoneTrackKeys(PelvisBone, Pos, Rot, Scale);
    Controller.CloseBracket();
    Seq->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim CLAMP %s keys=%d changed=%d ok=%d"), *Seq->GetName(), Keys, Changed, bOk ? 1 : 0);
}

// Parry.InPlace <AnimPath>: removes the baked forward travel of the pelvis (linear drift in X/Y from first
// to last key), keeps the per-frame sway. Logs the drift it removed.
static void InPlace(const TArray<FString>& Args)
{
    if (Args.Num() < 1) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: Parry.InPlace <AnimPath>")); return; }
    UAnimSequence* Seq = LoadAnim(Args[0]);
    if (!Seq || !Seq->GetSkeleton()) return;
    const IAnimationDataModel* Model = Seq->GetDataModel();
    const FReferenceSkeleton& Ref = Seq->GetSkeleton()->GetReferenceSkeleton();
    const FName PelvisBone = PelvisName(Ref);
    const int32 Pelvis = Ref.FindBoneIndex(PelvisBone);
    if (Pelvis == INDEX_NONE || !Model->IsValidBoneTrackName(PelvisBone)) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: no pelvis track")); return; }
    const int32 Parent = Ref.GetParentIndex(Pelvis);
    const int32 Keys = Model->GetNumberOfKeys();
    if (Keys < 2) return;
    TArray<FTransform> Local, ParentCS, PelvisCS;
    for (int32 K = 0; K < Keys; ++K)
    {
        const FFrameNumber N(K);
        Local.Add(Model->GetBoneTrackTransform(PelvisBone, N));
        ParentCS.Add(Parent != INDEX_NONE ? ComponentTransform(Model, Ref, Parent, N) : FTransform::Identity);
        PelvisCS.Add(Local.Last() * ParentCS.Last());
    }
    // Optional extra words: "z" also removes height drift, "yaw" also removes turning around the up axis.
    bool bZ = false, bYaw = false;
    for (int32 I = 1; I < Args.Num(); ++I) { bZ |= Args[I].Equals(TEXT("z"), ESearchCase::IgnoreCase); bYaw |= Args[I].Equals(TEXT("yaw"), ESearchCase::IgnoreCase); }
    FVector Drift = PelvisCS.Last().GetLocation() - PelvisCS[0].GetLocation();
    if (!bZ) Drift.Z = 0.f;
    // "addz=<units>" removes this much extra height over the clip (e.g. to match how far the capsule climbs).
    for (int32 I = 1; I < Args.Num(); ++I)
        if (Args[I].StartsWith(TEXT("addz="), ESearchCase::IgnoreCase)) Drift.Z += FCString::Atof(*Args[I].Mid(5));
    // Yaw drift: heading of the pelvis forward axis in the horizontal plane, first vs last key.
    auto Heading = [](const FTransform& T) { FVector F = T.GetRotation().GetAxisX(); F.Z = 0.f; return FMath::Atan2(F.Y, F.X); };
    const float YawDrift = bYaw ? FMath::FindDeltaAngleRadians(Heading(PelvisCS[0]), Heading(PelvisCS.Last())) : 0.f;
    TArray<FVector> Pos; TArray<FQuat> Rot; TArray<FVector> Scale;
    for (int32 K = 0; K < Keys; ++K)
    {
        FTransform CS = PelvisCS[K];
        const float Alpha = float(K) / float(Keys - 1);
        CS.SetLocation(CS.GetLocation() - Drift * Alpha);
        if (bYaw)
        {
            // Rotate pelvis back around the vertical axis (through its first-frame position).
            const FQuat Undo(FVector::UpVector, -YawDrift * Alpha);
            const FVector Pivot = PelvisCS[0].GetLocation();
            CS.SetLocation(Pivot + Undo.RotateVector(CS.GetLocation() - Pivot));
            CS.SetRotation(Undo * CS.GetRotation());
        }
        const FTransform NewLocal = CS.GetRelativeTransform(ParentCS[K]);
        Pos.Add(NewLocal.GetLocation()); Rot.Add(NewLocal.GetRotation()); Scale.Add(NewLocal.GetScale3D());
    }
    IAnimationDataController& Controller = Seq->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Remove forward drift")));
    const bool bOk = Controller.SetBoneTrackKeys(PelvisBone, Pos, Rot, Scale);
    Controller.CloseBracket();
    Seq->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim INPLACE %s drift=(%.4f, %.4f, %.4f) yaw=%.1f ok=%d"), *Seq->GetName(), Drift.X, Drift.Y, Drift.Z, FMath::RadiansToDegrees(YawDrift), bOk ? 1 : 0);
}

// Reference pose of a bone in component space.
static FTransform RefComponentTransform(const FReferenceSkeleton& Ref, int32 Bone)
{
    FTransform Result = FTransform::Identity;
    while (Bone != INDEX_NONE) { Result = Result * Ref.GetRefBonePose()[Bone]; Bone = Ref.GetParentIndex(Bone); }
    return Result;
}

// Parry.Retarget <SourceAnim> <TargetAnim>: copies the motion of a UE-mannequin-named clip (pelvis, spine_01,
// upperarm_l ...) onto a clip of a Mixamo skeleton (Hips, Spine, LeftArm ...). Rotations are transferred as
// component-space deltas from each skeleton's reference pose; the target clip gets the source's length.
static void Retarget(const TArray<FString>& Args)
{
    if (Args.Num() < 2) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: Parry.Retarget <SourceAnim> <TargetAnim>")); return; }
    UAnimSequence* Src = LoadAnim(Args[0]);
    UAnimSequence* Dst = LoadAnim(Args[1]);
    if (!Src || !Dst || !Src->GetSkeleton() || !Dst->GetSkeleton()) return;
    static const TCHAR* Map[][2] = {
        {TEXT("pelvis"), TEXT("Hips")}, {TEXT("spine_01"), TEXT("Spine")}, {TEXT("spine_02"), TEXT("Spine1")}, {TEXT("spine_03"), TEXT("Spine2")},
        {TEXT("neck_01"), TEXT("Neck")}, {TEXT("head"), TEXT("Head")},
        {TEXT("clavicle_l"), TEXT("LeftShoulder")}, {TEXT("upperarm_l"), TEXT("LeftArm")}, {TEXT("lowerarm_l"), TEXT("LeftForeArm")}, {TEXT("hand_l"), TEXT("LeftHand")},
        {TEXT("clavicle_r"), TEXT("RightShoulder")}, {TEXT("upperarm_r"), TEXT("RightArm")}, {TEXT("lowerarm_r"), TEXT("RightForeArm")}, {TEXT("hand_r"), TEXT("RightHand")},
        {TEXT("thigh_l"), TEXT("LeftUpLeg")}, {TEXT("calf_l"), TEXT("LeftLeg")}, {TEXT("foot_l"), TEXT("LeftFoot")}, {TEXT("ball_l"), TEXT("LeftToeBase")},
        {TEXT("thigh_r"), TEXT("RightUpLeg")}, {TEXT("calf_r"), TEXT("RightLeg")}, {TEXT("foot_r"), TEXT("RightFoot")}, {TEXT("ball_r"), TEXT("RightToeBase")}};
    const IAnimationDataModel* SM = Src->GetDataModel();
    const FReferenceSkeleton& SR = Src->GetSkeleton()->GetReferenceSkeleton();
    const FReferenceSkeleton& DR = Dst->GetSkeleton()->GetReferenceSkeleton();
    TMap<int32, int32> DstToSrc;
    for (const auto& Pair : Map)
    {
        const int32 S = SR.FindBoneIndex(Pair[0]), D = DR.FindBoneIndex(Pair[1]);
        if (S != INDEX_NONE && D != INDEX_NONE) DstToSrc.Add(D, S);
    }
    const int32 Keys = SM->GetNumberOfKeys();
    const int32 NumDst = DR.GetNum();
    TArray<FTransform> DstRefCS; DstRefCS.SetNum(NumDst);
    for (int32 B = 0; B < NumDst; ++B) DstRefCS[B] = RefComponentTransform(DR, B);
    // Per bone key arrays for the target.
    TArray<TArray<FVector>> Pos; TArray<TArray<FQuat>> Rot; TArray<TArray<FVector>> Scl;
    Pos.SetNum(NumDst); Rot.SetNum(NumDst); Scl.SetNum(NumDst);
    for (int32 K = 0; K < Keys; ++K)
    {
        const FFrameNumber N(K);
        TArray<FTransform> DstCS; DstCS.SetNum(NumDst);
        for (int32 B = 0; B < NumDst; ++B) // parents come before children in a reference skeleton
        {
            const int32 Parent = DR.GetParentIndex(B);
            const FTransform ParentCS = Parent != INDEX_NONE ? DstCS[Parent] : FTransform::Identity;
            FTransform Local = DR.GetRefBonePose()[B];
            if (const int32* S = DstToSrc.Find(B))
            {
                const FTransform SrcCS = ComponentTransform(SM, SR, *S, N);
                const FTransform SrcRefCS = RefComponentTransform(SR, *S);
                const FQuat Delta = SrcCS.GetRotation() * SrcRefCS.GetRotation().Inverse();
                FTransform CS(Delta * DstRefCS[B].GetRotation(), (ParentCS.TransformPosition(Local.GetLocation())), FVector::OneVector);
                if (Parent == INDEX_NONE || !DstToSrc.Contains(Parent)) // root of the mapped chain: carry the pelvis offset
                    CS.SetLocation(DstRefCS[B].GetLocation() + (SrcCS.GetLocation() - SrcRefCS.GetLocation()));
                Local = CS.GetRelativeTransform(ParentCS);
                Local.SetScale3D(DR.GetRefBonePose()[B].GetScale3D());
            }
            DstCS[B] = Local * ParentCS;
            Pos[B].Add(Local.GetLocation()); Rot[B].Add(Local.GetRotation()); Scl[B].Add(Local.GetScale3D());
        }
    }
    IAnimationDataController& C = Dst->GetController();
    C.OpenBracket(FText::FromString(TEXT("Retarget")));
    C.SetFrameRate(SM->GetFrameRate());
    C.SetNumberOfFrames(FFrameNumber(SM->GetNumberOfFrames()));
    int32 Written = 0;
    for (int32 B = 0; B < NumDst; ++B)
    {
        const FName Name = DR.GetBoneName(B);
        if (!Dst->GetDataModel()->IsValidBoneTrackName(Name)) C.AddBoneCurve(Name);
        if (C.SetBoneTrackKeys(Name, Pos[B], Rot[B], Scl[B])) ++Written;
    }
    C.CloseBracket();
    Dst->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim RETARGET %s -> %s keys=%d mapped=%d tracks=%d length=%.3f"), *Src->GetName(), *Dst->GetName(), Keys, DstToSrc.Num(), Written, Dst->GetPlayLength());
}

static FAutoConsoleCommand RetargetCmd(TEXT("Parry.Retarget"), TEXT("Copies a UE-mannequin clip onto a Mixamo-skeleton clip."), FConsoleCommandWithArgsDelegate::CreateStatic(&Retarget));
static FAutoConsoleCommand InPlaceCmd(TEXT("Parry.InPlace"), TEXT("Removes baked forward travel of the pelvis."), FConsoleCommandWithArgsDelegate::CreateStatic(&InPlace));
static FAutoConsoleCommand ClampCmd(TEXT("Parry.ClampPelvis"), TEXT("Pelvis never higher than MaxZ (removes baked jump height)."), FConsoleCommandWithArgsDelegate::CreateStatic(&ClampPelvis));
static FAutoConsoleCommand InfoCmd(TEXT("Parry.AnimInfo"), TEXT("Logs pelvis/foot height per frame of an animation."), FConsoleCommandWithArgsDelegate::CreateStatic(&AnimInfo));
static FAutoConsoleCommand TrimCmd(TEXT("Parry.TrimAnim"), TEXT("Keeps only [Start, End] seconds of an animation."), FConsoleCommandWithArgsDelegate::CreateStatic(&TrimAnim));
}
#endif
