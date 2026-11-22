#include "MainHeroAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
enum EClip { Idle, Walk, Run, JumpStart, Fall, Land, DoubleJump, Dash, Parry, Count };

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
    for (const TCHAR* Name : {TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("JumpStart"), TEXT("Fall"), TEXT("Land"), TEXT("DoubleJump"), TEXT("Dash"), TEXT("Parry")})
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
    const float Speed = Hero->GetVelocity().Size2D();

    // Second jump in the air (Character jump count goes up while falling) -> backflip until landing.
    const int32 JumpCount = Hero->JumpCurrentCount;
    if (!bFalling) bDoubleJump = false;
    const bool bStartDoubleJump = bFalling && JumpCount >= 2 && JumpCount > LastJumpCount;
    if (bStartDoubleJump) bDoubleJump = true;
    LastJumpCount = JumpCount;

    // Leaving the ground: jump start when going up, straight into the fall loop when walking off a ledge.
    if (bFalling && !bWasFalling)
    {
        AirStart = Clock;
        StartLength = (Hero->GetVelocity().Z > 100.f && Clips[JumpStart]) ? Clips[JumpStart]->GetPlayLength() / FMath::Max(.01f, JumpStartRate) : 0.f;
    }
    // Touching down after a real fall: landing clip.
    if (!bFalling && bWasFalling && !bDashing)
    {
        LandStart = Clock;
        LandLength = Clips[Land] ? Clips[Land]->GetPlayLength() / FMath::Max(.01f, LandRate) : 0.f;
    }

    const bool bLanding = !bFalling && Clock - LandStart < LandLength && !(Speed > IdleSpeed && Clock - LandStart > LandMinTime);

    if (bDashing)
    {
        PlayClip(Clips[Dash], true, DashRate);
    }
    else if (bFalling)
    {
        if (bDoubleJump) PlayClip(Clips[DoubleJump], false, DoubleJumpRate, bStartDoubleJump);
        else if (Clock - AirStart < StartLength) PlayClip(Clips[JumpStart], false, JumpStartRate, Clock == AirStart);
        else if (Clips[Fall])
        {
            // Ping-pong: play forward to the end, then backward to the start, and so on until landing.
            const float Length = Clips[Fall]->GetPlayLength();
            const bool bEnterFall = Clips[Fall] != Clip;
            if (bEnterFall) { FallTime = 0.f; FallDir = 1.f; }
            else
            {
                FallTime += DeltaSeconds * FallRate * FallDir;
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
    else
    {
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
