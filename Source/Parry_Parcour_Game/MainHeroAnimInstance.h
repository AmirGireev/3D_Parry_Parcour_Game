#pragma once
#include "CoreMinimal.h"
#include "CombatPreviewAnimInstance.h"
#include "MainHeroAnimInstance.generated.h"

class UAnimSequence;

/**
 * Main character (Art/Main): idle, walk, run by ground speed, jump in the air,
 * backflip on the double jump, lean-forward sprint while dashing and an
 * upper-body parry pose on top of whatever the legs are doing.
 */
UCLASS()
class PARRY_PARCOUR_GAME_API UMainHeroAnimInstance : public UCombatPreviewAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float IdleSpeed = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float RunSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float WalkRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float RunRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float JumpRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float DoubleJumpRate = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float DashRate = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float ParryRate = 1.f;
    /** How long the parry pose stays on the upper body after the parry button. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float ParryHoldTime = .5f;
protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;
    bool bWasFalling = false, bDoubleJump = false, bWasParrying = false;
    int32 LastJumpCount = 0;
    float Clock = 0.f, ParryUntil = -1.f;
};
