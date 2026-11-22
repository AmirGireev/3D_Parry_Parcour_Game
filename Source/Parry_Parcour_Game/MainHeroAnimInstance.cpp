#include "MainHeroAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
enum EClip { Idle, Walk, Run, Jump };
}

void UMainHeroAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Clips.Reset();
    for (const TCHAR* Name : {TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Jump")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/_Game/Art/Main/Anims/Anim_Main_%s.Anim_Main_%s"), Name, Name)));
    PlayClip(Clips[Idle], true);
}

void UMainHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Hero = Cast<ACharacter>(TryGetPawnOwner());
    if (!Hero || Clips.Num() < 4) return;

    const UCharacterMovementComponent* Move = Hero->GetCharacterMovement();
    const bool bFalling = Move && Move->IsFalling();
    if (bFalling)
    {
        // Restart the jump clip each time the character leaves the ground.
        PlayClip(Clips[Jump], false, JumpRate, !bWasFalling);
    }
    else
    {
        const float Speed = Hero->GetVelocity().Size2D();
        if (Speed <= IdleSpeed) PlayClip(Clips[Idle], true);
        else if (Speed < RunSpeed) PlayClip(Clips[Walk], true, WalkRate);
        else PlayClip(Clips[Run], true, RunRate);
    }
    bWasFalling = bFalling;
}
