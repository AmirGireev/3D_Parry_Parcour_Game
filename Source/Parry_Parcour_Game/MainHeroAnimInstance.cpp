#include "MainHeroAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
enum EClip { Idle, Walk, Run, Jump, DoubleJump, Dash, Parry, Count };

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
    for (const TCHAR* Name : {TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Jump"), TEXT("DoubleJump"), TEXT("Dash"), TEXT("Parry")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/_Game/Art/Main/Anims/Anim_Main_%s.Anim_Main_%s"), Name, Name)));
    OverlayRoot = TEXT("spine_01");
    PlayClip(Clips[Idle], true);
}

void UMainHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Hero = Cast<ACharacter>(TryGetPawnOwner());
    if (!Hero || Clips.Num() < Count) return;
    Clock += DeltaSeconds;

    const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
    const bool bFalling = Move && Move->IsFalling();
    const bool bDashing = ReadBool(Hero, TEXT("bIsDashing"));

    // Second jump in the air (Character jump count goes up while falling) -> backflip until landing.
    const int32 JumpCount = Hero->JumpCurrentCount;
    if (!bFalling) bDoubleJump = false;
    const bool bStartDoubleJump = bFalling && JumpCount >= 2 && JumpCount > LastJumpCount;
    if (bStartDoubleJump) bDoubleJump = true;
    LastJumpCount = JumpCount;

    if (bDashing)
    {
        PlayClip(Clips[Dash], true, DashRate);
    }
    else if (bFalling)
    {
        if (bDoubleJump) PlayClip(Clips[DoubleJump], false, DoubleJumpRate, bStartDoubleJump);
        else PlayClip(Clips[Jump], false, JumpRate, !bWasFalling);
    }
    else
    {
        const float Speed = Hero->GetVelocity().Size2D();
        if (Speed <= IdleSpeed) PlayClip(Clips[Idle], true);
        else if (Speed < RunSpeed) PlayClip(Clips[Walk], true, WalkRate);
        else PlayClip(Clips[Run], true, RunRate);
    }
    bWasFalling = bFalling;

    // Keep the arms below shoulder height in the normal jump only.
    bLimitArms = bFalling && !bDoubleJump && !bDashing;

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
