#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatPreviewAnimInstance.generated.h"

/** Small crossfading animation player used only by the sword collision test. */
UCLASS(Transient)
class PARRY_PARCOUR_GAME_API UCombatPreviewAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Clip;
    UPROPERTY(Transient) float Rate = 1.f;
    UPROPERTY(Transient) bool bLoop = true;
    UPROPERTY(Transient) int32 Revision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat test") FVector GuardDirection = FVector(-.65,.35,.7);
    /** When true, upper arms are rotated down so they never point higher than ArmMaxElevation (degrees, negative = below horizontal). */
    UPROPERTY(Transient) bool bLimitArms = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arms") float ArmMaxElevation = -25.f;
    void PlayClip(UAnimSequence* InClip, bool Loop = true, float PlayRate = 1.f, bool Restart = false);
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
