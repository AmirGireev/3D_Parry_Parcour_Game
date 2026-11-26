#pragma once
#include "CoreMinimal.h"
#include "CombatPreviewAnimInstance.h"
#include "MainHeroAnimInstance.generated.h"

class UAnimSequence;

/**
 * Main character (Art/Main): idle, walk, run by ground speed; jump split into
 * start -> looping fall -> landing; backflip on the double jump; lean-forward
 * sprint while dashing; upper-body parry pose on top of whatever the legs do.
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float JumpStartRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float FallRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float LandRate = 1.f;
    /** Landing is cut short once the player moves again after this many seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float LandMinTime = .15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float DoubleJumpRate = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float DashRate = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float ParryRate = 1.f;
    /** How long the parry pose stays on the upper body after the parry button. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float ParryHoldTime = .5f;
    /** Turn in place: plays when the hero turns faster than this (deg/s) while slower than TurnMaxSpeed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float TurnMinYawRate = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float TurnMaxSpeed = 450.f;
    /** A turn only starts if the hero was standing within this time (s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float TurnStartWindow = .2f;
    /** Degrees the upper arms are pushed away from the body while standing (hands out of the hips). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float IdleArmSpread = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float TurnRate = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float StrafeWalkRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float StrafeRunRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Main hero") float ShimmyRate = 1.f;
protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;
    bool bWasFalling = false, bDoubleJump = false, bWasParrying = false;
    int32 LastJumpCount = 0;
    float FallTime = 0.f, FallDir = 1.f, DoubleJumpStart = -10.f;
    float LastYaw = 0.f, TurnUntil = -1.f, TurnSign = 0.f, LastIdleTime = 0.f;
    bool bHaveYaw = false, bWasTurning = false;
    uint8 LastLedge = 0;
    float Clock = 0.f, ParryUntil = -1.f, AirStart = 0.f, LandStart = -10.f, StartLength = 0.f, LandLength = 0.f;
};
