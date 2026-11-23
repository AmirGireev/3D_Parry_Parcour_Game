#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HeroMovementExtras.generated.h"

class ACharacter;

UENUM(BlueprintType)
enum class EHeroLedgeState : uint8 { None, Hanging, ClimbUp, StandUp, Grabbing };

/**
 * Extra movement for the player hero:
 *  - while locked on, the hero faces the target and strafes (no turning into the run direction)
 *  - ledge grab on straight edges while falling, shimmy left/right with A/D, jump climbs up, S lets go
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PARRY_PARCOUR_GAME_API UHeroMovementExtras : public UActorComponent
{
    GENERATED_BODY()
public:
    UHeroMovementExtras();

    // --- Lock-on strafe ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lock-on") float LockFaceSpeed = 12.f;
    UPROPERTY(BlueprintReadOnly, Category="Lock-on") bool bStrafing = false;

    // --- Ledge grab ---
    /** How far in front of the capsule a wall is searched. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float GrabReach = 60.f;
    /** Ledge must be this far above / below the capsule center to be grabbed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float MinLedgeAboveCenter = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float MaxLedgeAboveCenter = 160.f;
    /** While hanging the capsule center sits this far below the ledge top (hands of the hang clip on the edge). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float HangBelowLedge = 132.f;
    /** Length of the grab animation (jumping to hanging) after catching the ledge; 0 = hang immediately. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float GrabTime = .4f;
    /** Gap between capsule and wall while hanging. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float HangWallGap = 0.f;
    /** How far the capsule moves forward (onto the ledge) while climbing up; matches the climb clip. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float ClimbForward = 56.f;
    /** Height of the ledge above the origin in the climb clip (hands of the hang pose), used to fit the clip to the real ledge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float ClipLedgeHeight = 234.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float ShimmySpeed = 150.f;
    /** Max angle (deg) between wall normals for the edge to count as straight while shimmying. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float StraightEdgeAngle = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float ClimbUpTime = 1.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float StandUpTime = .6f;
    /** No new grab right after letting go. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ledge") float RegrabDelay = .4f;

    UPROPERTY(BlueprintReadOnly, Category="Ledge") EHeroLedgeState LedgeState = EHeroLedgeState::None;
    /** -1 left, 0 still, 1 right (while hanging). */
    UPROPERTY(BlueprintReadOnly, Category="Ledge") float ShimmyDir = 0.f;
    /** Seconds since the current ledge state started. */
    UPROPERTY(BlueprintReadOnly, Category="Ledge") float StateTime = 0.f;

    UFUNCTION(BlueprintCallable, Category="Ledge") void ReleaseLedge();
    bool IsOnLedge() const { return LedgeState != EHeroLedgeState::None; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    ACharacter* Hero() const;
    bool ReadBool(FName Name) const;
    void UpdateLockOn(float Dt);
    bool FindLedge(const FVector& From, const FVector& Forward, FVector& OutWallPoint, FVector& OutNormal, float& OutTopZ) const;
    void TryGrab();
    void TickHanging(float Dt);
    void TickClimb(float Dt);
    void SetState(EHeroLedgeState S) { LedgeState = S; StateTime = 0.f; }

    bool bDefaultOrientToMovement = true;
    bool bHaveDefaults = false;
    FVector LedgeNormal = FVector::ZeroVector;
    float LedgeTopZ = 0.f;
    FVector ClimbFrom = FVector::ZeroVector, ClimbTo = FVector::ZeroVector;
    FVector MeshRelative = FVector::ZeroVector, MeshWorldDuringClimb = FVector::ZeroVector, StandFixOffset = FVector::ZeroVector;
    float ReleasedAt = -100.f;
};

UCLASS()
class PARRY_PARCOUR_GAME_API UHeroParryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Ends the hero's current parry window (one press = one parried hit) and turns the hero (not the camera) toward From. */
    UFUNCTION(BlueprintCallable, Category="Parry")
    static void ConsumeParry(ACharacter* InHero, AActor* From);
};
