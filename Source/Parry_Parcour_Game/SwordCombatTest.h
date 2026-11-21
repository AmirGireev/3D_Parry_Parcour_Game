#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwordCombatTest.generated.h"

class ACharacter;
class UAnimSequence;
class USoundBase;
class USkeletalMesh;
class UCombatPreviewAnimInstance;

/** Isolated adapter around the existing Blueprint player and enemy mechanics. */
UCLASS()
class PARRY_PARCOUR_GAME_API ASwordCombatTest : public AActor
{
    GENERATED_BODY()
public:
    ASwordCombatTest();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, Category="Combat test") bool bShowBladeTraces = true;
    UPROPERTY(EditAnywhere, Category="Combat test") bool bAutoParry = false;
    UPROPERTY(EditAnywhere, Category="Combat test") bool bTrainingMode = true;
    UPROPERTY(EditAnywhere, Category="Combat test", meta=(ClampMin="1")) float BladeRadius = 4.f;
    UPROPERTY(EditAnywhere, Category="Combat test") float AttackInterval = 3.f;
    UPROPERTY(EditAnywhere, Category="Combat test") float ApproachDistance = 175.f;
    UPROPERTY(EditAnywhere, Category="Combat test") float ActiveStart = .24f;
    UPROPERTY(EditAnywhere, Category="Combat test") float ActiveEnd = .67f;
    UPROPERTY(EditAnywhere, Category="Combat test") FVector HeroGuardDirection = FVector(-.65,.35,.7);
    UPROPERTY(VisibleAnywhere, Category="Combat test") int32 ParryCount = 0;
    UPROPERTY(VisibleAnywhere, Category="Combat test") int32 BodyHitCount = 0;
    UFUNCTION(BlueprintCallable, Category="Combat test") FString RunBladeContactChecks();
    UFUNCTION(BlueprintCallable, Category="Combat test") FString GetCombatDiagnostics();
private:
    UPROPERTY(Transient) TObjectPtr<ACharacter> Hero;
    UPROPERTY(Transient) TObjectPtr<ACharacter> Boss;
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> HeroClips;
    UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> BossClips;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> ParrySounds;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> ClashSounds;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> HeroMesh;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> BossMesh;
    float Clock = 0.f, NextAttack = 2.f, AttackTime = -1.f, HeroPoseUntil = 0.f;
    float StaggerUntil = 0.f, HeroAttackUntil = 0.f;
    float LastBossHealth = -1.f;
    int32 AttackIndex = 0;
    bool bInitialized = false, bSwingResolved = false, bWasParrying = false;
    bool bHavePrevious = false, bCastSpawned = false, bClashPlayed = false;
    FVector PrevHeroBase, PrevHeroTip, PrevBossBase, PrevBossTip;
    FString Feedback = TEXT("Close the distance. Right mouse to parry.");
    void InitializeFighters();
    void Animate(ACharacter* Fighter, UAnimSequence* Clip, bool Loop, float Rate=1.f, bool Restart=false);
    void ResolveBladeContact(bool bActive, bool bGuard);
    void Parry(const FVector& Point);
    void DrawHUD();
};
