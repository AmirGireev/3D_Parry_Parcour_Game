#pragma once
#include "CoreMinimal.h"
#include "CombatPreviewAnimInstance.h"
#include "MainHeroAnimInstance.generated.h"

class UAnimSequence;

/** Main character (Art/Main): idle, walk, run by ground speed and jump while in the air. */
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
protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Clips;
    bool bWasFalling = false;
};
