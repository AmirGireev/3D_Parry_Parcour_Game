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
    const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis")), FootL = Ref.FindBoneIndex(TEXT("foot_l")), FootR = Ref.FindBoneIndex(TEXT("foot_r"));
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
    const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
    if (Pelvis == INDEX_NONE || !Model->IsValidBoneTrackName(TEXT("pelvis"))) { UE_LOG(LogTemp, Error, TEXT("ParryAnim: no pelvis track")); return; }
    const int32 Parent = Ref.GetParentIndex(Pelvis);
    const int32 Keys = Model->GetNumberOfKeys();
    TArray<FVector> Pos; TArray<FQuat> Rot; TArray<FVector> Scale;
    int32 Changed = 0;
    for (int32 K = 0; K < Keys; ++K)
    {
        const FFrameNumber N(K);
        const FTransform Local = Model->GetBoneTrackTransform(TEXT("pelvis"), N);
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
    const bool bOk = Controller.SetBoneTrackKeys(TEXT("pelvis"), Pos, Rot, Scale);
    Controller.CloseBracket();
    Seq->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("ParryAnim CLAMP %s keys=%d changed=%d ok=%d"), *Seq->GetName(), Keys, Changed, bOk ? 1 : 0);
}

static FAutoConsoleCommand ClampCmd(TEXT("Parry.ClampPelvis"), TEXT("Pelvis never higher than MaxZ (removes baked jump height)."), FConsoleCommandWithArgsDelegate::CreateStatic(&ClampPelvis));
static FAutoConsoleCommand InfoCmd(TEXT("Parry.AnimInfo"), TEXT("Logs pelvis/foot height per frame of an animation."), FConsoleCommandWithArgsDelegate::CreateStatic(&AnimInfo));
static FAutoConsoleCommand TrimCmd(TEXT("Parry.TrimAnim"), TEXT("Keeps only [Start, End] seconds of an animation."), FConsoleCommandWithArgsDelegate::CreateStatic(&TrimAnim));
}
#endif
