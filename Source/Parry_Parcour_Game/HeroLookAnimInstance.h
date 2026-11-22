#pragma once
#include "CoreMinimal.h"
#include "CombatPreviewAnimInstance.h"
#include "HeroLookAnimInstance.generated.h"

class UAnimSequence;

/** Plays the CombatTest hero clips (idle, run, jump, dash, parry) on the player hero, driven by its movement and Blueprint state. */
UCLASS()
class PARRY_PARCOUR_GAME_API UHeroLookAnimInstance : public UCombatPreviewAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hero look") float RunSpeedThreshold = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hero look") float ParryPoseTime = .55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hero look") float ParryRate = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hero look") float DashRate = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hero look") float JumpRate = 1.5f;
protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;
    float Clock = 0.f, PoseUntil = 0.f;
    bool bWasParrying = false;
};
