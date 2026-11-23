#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "HeroKatanaComponent.generated.h"

/**
 * The hero's katana: attaches itself to the hand bone of the owner's skeletal mesh and keeps a world-size
 * independent of the mesh scale. Placement values can be tweaked live (they are applied every frame).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PARRY_PARCOUR_GAME_API UHeroKatanaComponent : public UStaticMeshComponent
{
    GENERATED_BODY()
public:
    UHeroKatanaComponent();
    /** Bone to hold the katana (Mixamo "RightHand", UE mannequin "hand_r"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Katana") FName HandBone = TEXT("RightHand");
    /** World length of the whole katana in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Katana") float Length = 75.f;
    /** Grip point on the katana mesh, as fraction along its long (Z) axis from the center (+0.5 = top end). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Katana") float GripAlongZ = .33f;
    /** Rotation of the katana relative to the hand bone. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Katana") FRotator GripRotation = FRotator(0.f, 0.f, 90.f);
    /** Offset of the grip from the hand bone, in cm, in hand bone space. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Katana") FVector GripOffset = FVector(0.f, 8.f, 0.f);
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    void Place();
};
