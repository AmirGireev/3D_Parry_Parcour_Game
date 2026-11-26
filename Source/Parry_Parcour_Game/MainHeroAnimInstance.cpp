#include "MainHeroAnimInstance.h"
#include "HeroMovementExtras.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
enum EClip { Idle, Walk, Run, JumpStart, Fall, Land, DoubleJump, Dash, Parry,
             TurnL, TurnR, StrafeLWalk, StrafeRWalk, StrafeLRun, StrafeRRun, HangShimmy, HangToCrouch, CrouchToStand, JumpToHang, Count };

bool ReadBool(const UObject* Owner, FName Name)
{
    const FBoolProperty* P = Owner ? FindFProperty<FBoolProperty>(Owner->GetClass(), Name) : nullptr;
    return P && P->GetPropertyValue_InContainer(Owner);
}
}

void UMainHeroAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Clips.Reset();
    for (const TCHAR* Name : {TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("JumpStart"), TEXT("Fall"), TEXT("Land"), TEXT("DoubleJump"), TEXT("Dash"), TEXT("Parry"),
                              TEXT("TurnLeft"), TEXT("TurnRight"), TEXT("StrafeLeftWalk"), TEXT("StrafeRightWalk"), TEXT("StrafeLeftRun"), TEXT("StrafeRightRun"),
                              TEXT("HangShimmy"), TEXT("HangToCrouch"), TEXT("CrouchToStand"), TEXT("JumpToHang")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/_Game/Art/Main/Anims/Anim_Main_%s.Anim_Main_%s"), Name, Name)));
    // Upper-body layer starts at the first spine bone (UE mannequin "spine_01", Mixamo "Spine").
    OverlayRoot = TEXT("spine_01");
    if (const USkeletalMeshComponent* Mesh = GetSkelMeshComponent())
        if (Mesh->GetBoneIndex(OverlayRoot) == INDEX_NONE) OverlayRoot = TEXT("Spine");
    PlayClip(Clips[Idle], true);
}

void UMainHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Hero = Cast<ACharacter>(TryGetPawnOwner());
    if (!Hero || Clips.Num() < Count) return;
    Clock += DeltaSeconds;
    bSnapNextClip = false;
    // The hero always holds the katana in the right hand.
    // Fist from the sprint (dash) clip, where the hand already holds the weapon.
    GripPoseClip = Clips.IsValidIndex(Dash) ? Clips[Dash] : nullptr;
    GripPoseTime = 0.f;

    const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
    const bool bFalling = Move && Move->IsFalling();
    const bool bDashing = ReadBool(Hero, TEXT("bIsDashing"));
    const float Speed = Hero->GetVelocity().Size2D();

    // Second jump in the air (Character jump count goes up while falling) -> backflip until landing.
    const int32 JumpCount = Hero->JumpCurrentCount;
    if (!bFalling) bDoubleJump = false;
    // Only a real second jump: the counter must already have been >= 1. Jumping right after walking off a ledge
    // (coyote time) makes Unreal count the fall too, so the counter goes 0 -> 2 in one step; that is a first jump.
    const bool bStartDoubleJump = bFalling && JumpCount >= 2 && JumpCount > LastJumpCount && LastJumpCount >= 1;
    if (bStartDoubleJump) { bDoubleJump = true; DoubleJumpStart = Clock; }
    // The backflip plays once (its own Rate Scale included), afterwards the normal fall takes over.
    const float FlipLength = Clips[DoubleJump] ? Clips[DoubleJump]->GetPlayLength() / FMath::Max(.01f, FMath::Abs(DoubleJumpRate * Clips[DoubleJump]->RateScale)) : 0.f;
    const bool bFlipping = bDoubleJump && Clock - DoubleJumpStart < FlipLength;
    LastJumpCount = JumpCount;

    // Leaving the ground: jump start when going up, straight into the fall loop when walking off a ledge.
    if (bFalling && !bWasFalling)
    {
        AirStart = Clock;
        StartLength = (Hero->GetVelocity().Z > 100.f && Clips[JumpStart]) ? Clips[JumpStart]->GetPlayLength() / FMath::Max(.01f, JumpStartRate * Clips[JumpStart]->RateScale) : 0.f;
    }
    // Touching down after a real fall: landing clip.
    if (!bFalling && bWasFalling && !bDashing)
    {
        LandStart = Clock;
        LandLength = Clips[Land] ? Clips[Land]->GetPlayLength() / FMath::Max(.01f, LandRate * Clips[Land]->RateScale) : 0.f;
    }

    const bool bLanding = !bFalling && Clock - LandStart < LandLength && !(Speed > IdleSpeed && Clock - LandStart > LandMinTime);

    // Turning: yaw change per second (positive = turning right).
    const float Yaw = Hero->GetActorRotation().Yaw;
    const float YawRate = bHaveYaw && DeltaSeconds > 0.f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds : 0.f;
    LastYaw = Yaw; bHaveYaw = true;

    const UHeroMovementExtras* Extras = Hero->FindComponentByClass<UHeroMovementExtras>();
    const EHeroLedgeState Ledge = Extras ? Extras->LedgeState : EHeroLedgeState::None;
    const bool bStrafing = Extras && Extras->bStrafing;

    if (Ledge != EHeroLedgeState::None)
    {
        const bool bNewState = (uint8)Ledge != LastLedge;
        if (Ledge == EHeroLedgeState::Grabbing && Clips[JumpToHang])
            PlayClip(Clips[JumpToHang], false, Clips[JumpToHang]->GetPlayLength() / FMath::Max(.05f, Extras->GrabTime), bNewState);
        else if (Ledge == EHeroLedgeState::Hanging)
        {
            // Shimmy clip forwards for one side, backwards for the other; frozen while hanging still.
            const float Dir = Extras->ShimmyDir;
            PlayClip(Clips[HangShimmy], true, Dir != 0.f ? ShimmyRate * Dir : 0.f, bNewState);
        }
        else if (Ledge == EHeroLedgeState::ClimbUp && Clips[HangToCrouch])
            PlayClip(Clips[HangToCrouch], false, Clips[HangToCrouch]->GetPlayLength() / FMath::Max(.05f, Extras->ClimbUpTime), bNewState);
        else if (Ledge == EHeroLedgeState::StandUp && Clips[CrouchToStand])
        {
            // Cut (no crossfade): the mesh jumps to the stand-up position this frame, a blend would drag the climb pose along.
            bSnapNextClip = bNewState;
            PlayClip(Clips[CrouchToStand], false, Clips[CrouchToStand]->GetPlayLength() / FMath::Max(.05f, Extras->StandUpTime), bNewState);
        }
        LastLedge = (uint8)Ledge;
        bWasFalling = false;
        bLimitArms = false;
        bOverlayActive = false;
        return;
    }
    LastLedge = (uint8)Ledge;

    if (bDashing)
    {
        PlayClip(Clips[Dash], true, DashRate);
    }
    else if (bFalling)
    {
        if (bFlipping) PlayClip(Clips[DoubleJump], false, DoubleJumpRate, bStartDoubleJump); // negative Rate Scale on the clip = backwards
        else if (!bDoubleJump && Clock - AirStart < StartLength) PlayClip(Clips[JumpStart], false, JumpStartRate, Clock == AirStart);
        else if (Clips[Fall])
        {
            // Ping-pong: play forward to the end, then backward to the start, and so on until landing.
            const float Length = Clips[Fall]->GetPlayLength();
            const bool bEnterFall = Clips[Fall] != Clip;
            if (bEnterFall) { FallTime = 0.f; FallDir = 1.f; }
            else
            {
                // Include the clip's own Rate Scale (set in the animation asset), same as the player does.
                FallTime += DeltaSeconds * FallRate * Clips[Fall]->RateScale * FallDir;
                if (FallTime >= Length) { FallTime = Length; FallDir = -1.f; }
                else if (FallTime <= 0.f) { FallTime = 0.f; FallDir = 1.f; }
            }
            PlayClip(Clips[Fall], false, FallRate * FallDir, bEnterFall);
        }
    }
    else if (bLanding)
    {
        PlayClip(Clips[Land], false, LandRate, Clock == LandStart);
    }
    else if (bStrafing && Speed > IdleSpeed)
    {
        // Locked on: hero faces the target, movement relative to the facing picks the clip.
        const FVector V = Hero->GetVelocity();
        const float Fwd = FVector::DotProduct(V, Hero->GetActorForwardVector());
        const float Side = FVector::DotProduct(V, Hero->GetActorRightVector());
        const bool bRun = Speed >= RunSpeed;
        if (FMath::Abs(Side) > FMath::Abs(Fwd))
        {
            if (Side > 0.f) PlayClip(Clips[bRun ? StrafeRRun : StrafeRWalk], true, bRun ? StrafeRunRate : StrafeWalkRate);
            else PlayClip(Clips[bRun ? StrafeLRun : StrafeLWalk], true, bRun ? StrafeRunRate : StrafeWalkRate);
        }
        else
        {
            // Backwards = walk/run clip played in reverse.
            const float Sign = Fwd >= 0.f ? 1.f : -1.f;
            PlayClip(Clips[bRun ? Run : Walk], true, (bRun ? RunRate : WalkRate) * Sign);
        }
    }
    else
    {
        // Short tap on the stick / A / D: starting from standing, the hero swings round quickly -> turn clip.
        // (A tap still gives some speed, so the start is judged by "was standing a moment ago", and the clip
        // keeps playing until the hero really runs.)
        // The movement component turns the hero on the spot after a short tap; play the matching turn clip
        // (looping while the turn lasts, so longer turns keep stepping).
        const bool bTurning = Extras && Extras->bTurningInPlace;
        if (bTurning)
        {
            const bool bNewTurn = !bWasTurning || TurnSign != Extras->TurnDir;
            TurnSign = Extras->TurnDir;
            PlayClip(Clips[TurnSign > 0.f ? TurnR : TurnL], true, TurnRate, bNewTurn);
        }
        bWasTurning = bTurning;
        if (bTurning) {}
        else
        {
            TurnUntil = -1.f;
            if (Speed <= IdleSpeed) PlayClip(Clips[Idle], true);
            else if (Speed < RunSpeed) PlayClip(Clips[Walk], true, WalkRate);
            else PlayClip(Clips[Run], true, RunRate);
        }
    }
    bWasFalling = bFalling;

    // Keep the arms below shoulder height in the normal jump only.
    bLimitArms = bFalling;
    // Arms while falling: this many degrees lower than in the clip.
    ArmDrop = FallArmDrop;
    // Standing / turning: push the arms a little away from the body so the hands don't sink into the hips.
    ArmSpread = (!bFalling && !bDashing && Speed < RunSpeed && (Clip == Clips[Idle] || Clip == Clips[TurnL] || Clip == Clips[TurnR])) ? IdleArmSpread : 0.f;

    // Parry: guard pose on the upper body, legs keep their locomotion.
    const bool bParrying = ReadBool(Hero, TEXT("bIsParrying"));
    if (bParrying && !bWasParrying)
    {
        PlayOverlay(Clips[Parry], ParryRate);
        ParryUntil = Clock + ParryHoldTime;
    }
    bWasParrying = bParrying;
    bOverlayActive = bParrying || Clock < ParryUntil;
}
