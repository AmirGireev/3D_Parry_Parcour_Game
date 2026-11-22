#include "HeroLookAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
enum EClip { Idle, Run, Jump, Dash, Parry };

bool ReadBool(const UObject* Owner, FName Name)
{
    const FBoolProperty* P = Owner ? FindFProperty<FBoolProperty>(Owner->GetClass(), Name) : nullptr;
    return P && P->GetPropertyValue_InContainer(Owner);
}
}

void UHeroLookAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Clips.Reset();
    for (const TCHAR* Name : {TEXT("Idle"), TEXT("Run"), TEXT("Jump"), TEXT("Dash"), TEXT("Parry")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/_Game/Art/Hero/Anim_Hero_%s.Anim_Hero_%s"), Name, Name)));
    PlayClip(Clips[Idle], true);
}

void UHeroLookAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Hero = Cast<ACharacter>(TryGetPawnOwner());
    if (!Hero || Clips.Num() < 5) return;
    Clock += DeltaSeconds;

    const bool bParrying = ReadBool(Hero, TEXT("bIsParrying"));
    if (bParrying && !bWasParrying)
    {
        PoseUntil = Clock + ParryPoseTime;
        PlayClip(Clips[Parry], false, ParryRate, true);
    }
    bWasParrying = bParrying;

    if (ReadBool(Hero, TEXT("bIsDashing")))
    {
        PlayClip(Clips[Dash], false, DashRate);
        PoseUntil = Clock + .12f;
    }
    else if (Clock > PoseUntil && !bParrying)
    {
        const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
        if (Move && Move->IsFalling()) PlayClip(Clips[Jump], false, JumpRate);
        else PlayClip(Clips[Hero->GetVelocity().Size2D() > RunSpeedThreshold ? Run : Idle], true);
    }
}
