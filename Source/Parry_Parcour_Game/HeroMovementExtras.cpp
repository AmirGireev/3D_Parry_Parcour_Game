#include "HeroMovementExtras.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

UHeroMovementExtras::UHeroMovementExtras()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ACharacter* UHeroMovementExtras::Hero() const { return Cast<ACharacter>(GetOwner()); }

bool UHeroMovementExtras::ReadBool(FName Name) const
{
    const AActor* Owner = GetOwner();
    const FBoolProperty* P = Owner ? FindFProperty<FBoolProperty>(Owner->GetClass(), Name) : nullptr;
    return P && P->GetPropertyValue_InContainer(Owner);
}

void UHeroMovementExtras::BeginPlay()
{
    Super::BeginPlay();
    if (ACharacter* H = Hero())
        if (UCharacterMovementComponent* Move = H->GetCharacterMovement())
        {
            // Read jump/move input before the movement component consumes it.
            Move->AddTickPrerequisiteComponent(this);
            bDefaultOrientToMovement = Move->bOrientRotationToMovement;
            bHaveDefaults = true;
        }
}

void UHeroMovementExtras::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ACharacter* H = Hero();
    if (!H) return;
    StateTime += DeltaTime;
    switch (LedgeState)
    {
    case EHeroLedgeState::None: TryGrab(); break;
    case EHeroLedgeState::Hanging: TickHanging(DeltaTime); break;
    default: TickClimb(DeltaTime); break;
    }
    UpdateLockOn(DeltaTime);
}

// ---------------------------------------------------------------- lock-on strafe
void UHeroMovementExtras::UpdateLockOn(float Dt)
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    AActor* Target = nullptr;
    bool bLocked = false;
    for (UActorComponent* C : H->GetComponents())
    {
        if (!C || !C->GetClass()->GetName().Contains(TEXT("UniversalLockOn"))) continue;
        if (const FBoolProperty* P = FindFProperty<FBoolProperty>(C->GetClass(), TEXT("LockedOn"))) bLocked = P->GetPropertyValue_InContainer(C);
        if (const FObjectProperty* P = FindFProperty<FObjectProperty>(C->GetClass(), TEXT("TargetActor"))) Target = Cast<AActor>(P->GetObjectPropertyValue_InContainer(C));
        break;
    }
    bStrafing = bLocked && IsValid(Target) && !IsOnLedge() && !ReadBool(TEXT("bIsDashing"));
    if (Move && bHaveDefaults) Move->bOrientRotationToMovement = bStrafing ? false : bDefaultOrientToMovement;
    if (bStrafing)
    {
        FVector To = Target->GetActorLocation() - H->GetActorLocation();
        To.Z = 0.f;
        if (!To.IsNearlyZero())
        {
            const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
            H->SetActorRotation(FMath::RInterpTo(H->GetActorRotation(), Want, Dt, LockFaceSpeed));
        }
    }
}

// ---------------------------------------------------------------- ledge
bool UHeroMovementExtras::FindLedge(const FVector& From, const FVector& Forward, FVector& OutWallPoint, FVector& OutNormal, float& OutTopZ) const
{
    UWorld* W = GetWorld();
    ACharacter* H = Hero();
    if (!W || !H) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroLedge), false, H);
    const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
    FVector Fwd = Forward; Fwd.Z = 0.f;
    if (!Fwd.Normalize()) return false;
    // Wall in front, probed at a few heights so short and tall ledges both register.
    FHitResult Wall;
    bool bWall = false;
    for (float Up : {MaxLedgeAboveCenter - 20.f, 60.f, MinLedgeAboveCenter})
    {
        const FVector S = From + FVector(0, 0, Up);
        if (W->LineTraceSingleByChannel(Wall, S, S + Fwd * (Radius + GrabReach), ECC_Visibility, Q)) { bWall = true; break; }
    }
    if (!bWall || FMath::Abs(Wall.ImpactNormal.Z) > .3f) return false;
    FVector N = Wall.ImpactNormal; N.Z = 0.f; N.Normalize();
    // Top surface just behind the wall face.
    const FVector TopStart = Wall.ImpactPoint - N * 15.f;
    FHitResult Top;
    const FVector A(TopStart.X, TopStart.Y, From.Z + MaxLedgeAboveCenter + 40.f);
    const FVector B(TopStart.X, TopStart.Y, From.Z + MinLedgeAboveCenter - 20.f);
    if (!W->LineTraceSingleByChannel(Top, A, B, ECC_Visibility, Q) || Top.bStartPenetrating || Top.ImpactNormal.Z < .7f) return false;
    const float Above = Top.ImpactPoint.Z - From.Z;
    if (Above < MinLedgeAboveCenter || Above > MaxLedgeAboveCenter) return false;
    OutWallPoint = Wall.ImpactPoint;
    OutNormal = N;
    OutTopZ = Top.ImpactPoint.Z;
    return true;
}

void UHeroMovementExtras::TryGrab()
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    if (!Move || !Move->IsFalling() || ReadBool(TEXT("bIsDashing"))) return;
    if (GetWorld()->GetTimeSeconds() - ReleasedAt < RegrabDelay) return;
    FVector Fwd = H->GetActorForwardVector();
    const FVector Vel2D(H->GetVelocity().X, H->GetVelocity().Y, 0.f);
    if (Vel2D.Size() > 100.f) Fwd = Vel2D.GetSafeNormal();
    FVector WallPoint, N; float TopZ;
    if (!FindLedge(H->GetActorLocation(), Fwd, WallPoint, N, TopZ)) return;
    const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
    FVector Hang = WallPoint + N * (Radius + HangWallGap);
    Hang.Z = TopZ - HangBelowLedge;
    LedgeNormal = N;
    LedgeTopZ = TopZ;
    Move->StopMovementImmediately();
    Move->SetMovementMode(MOVE_None);
    H->SetActorLocation(Hang, false, nullptr, ETeleportType::TeleportPhysics);
    H->SetActorRotation(FRotator(0.f, (-N).Rotation().Yaw, 0.f));
    ShimmyDir = 0.f;
    SetState(EHeroLedgeState::Hanging);
}

void UHeroMovementExtras::ReleaseLedge()
{
    ACharacter* H = Hero();
    if (!H) return;
    if (UCharacterMovementComponent* Move = H->GetCharacterMovement())
        if (Move->MovementMode == MOVE_None) Move->SetMovementMode(MOVE_Falling);
    ShimmyDir = 0.f;
    ReleasedAt = GetWorld()->GetTimeSeconds();
    SetState(EHeroLedgeState::None);
}

void UHeroMovementExtras::TickHanging(float Dt)
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    // Something else took over movement (dash, knockback ...): let go.
    if (!Move || Move->MovementMode != MOVE_None) { ShimmyDir = 0.f; ReleasedAt = GetWorld()->GetTimeSeconds(); SetState(EHeroLedgeState::None); return; }
    Move->Velocity = FVector::ZeroVector;
    const FVector Input = H->GetLastMovementInputVector();
    const bool bJump = H->bPressedJump;
    H->bPressedJump = false;

    // S / stick back = let go.
    if (FVector::DotProduct(Input, LedgeNormal) > .6f) { ReleaseLedge(); return; }
    // Jump = climb up onto the ledge.
    if (bJump && StateTime > .15f)
    {
        const float HalfHeight = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
        ClimbFrom = H->GetActorLocation();
        ClimbTo = ClimbFrom - LedgeNormal * ClimbForward;
        ClimbTo.Z = LedgeTopZ + HalfHeight + 2.f;
        // Only climb if the capsule fits on top.
        FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroClimb), false, H);
        const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(ClimbTo, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Q);
        if (!bBlocked) { ShimmyDir = 0.f; SetState(EHeroLedgeState::ClimbUp); return; }
    }
    // Shimmy along the edge with A/D, only while the edge continues straight.
    const FVector Right = H->GetActorRightVector();
    const float Side = FVector::DotProduct(Input, Right);
    ShimmyDir = 0.f;
    if (FMath::Abs(Side) > .3f)
    {
        const float Dir = FMath::Sign(Side);
        const FVector Step = Right * Dir * ShimmySpeed * Dt;
        // Probe a bit ahead of the capsule edge so we stop before running off the ledge or into a corner.
        const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const FVector Probe = H->GetActorLocation() + Right * Dir * (Radius + 10.f) + Step;
        FVector WallPoint, N; float TopZ;
        const FVector ProbeCenter(Probe.X, Probe.Y, LedgeTopZ - HangBelowLedge);
        const bool bEdge = FindLedge(ProbeCenter, -LedgeNormal, WallPoint, N, TopZ)
            && FVector::DotProduct(N, LedgeNormal) > FMath::Cos(FMath::DegreesToRadians(StraightEdgeAngle))
            && FMath::Abs(TopZ - LedgeTopZ) < 12.f;
        if (bEdge)
        {
            FHitResult Hit;
            H->SetActorLocation(H->GetActorLocation() + Step, true, &Hit);
            if (!Hit.bBlockingHit) ShimmyDir = Dir;
        }
    }
}

void UHeroMovementExtras::TickClimb(float Dt)
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    if (Move) Move->Velocity = FVector::ZeroVector;
    H->bPressedJump = false;
    if (LedgeState == EHeroLedgeState::ClimbUp)
    {
        // Linear, matching the climb clip whose own travel was removed linearly (no collision while climbing).
        const float A = FMath::Clamp(StateTime / FMath::Max(.05f, ClimbUpTime), 0.f, 1.f);
        H->SetActorLocation(FMath::Lerp(ClimbFrom, ClimbTo, A), false, nullptr, ETeleportType::TeleportPhysics);
        if (A >= 1.f) SetState(EHeroLedgeState::StandUp);
    }
    else if (StateTime >= StandUpTime)
    {
        if (Move) Move->SetMovementMode(MOVE_Walking);
        ReleasedAt = GetWorld()->GetTimeSeconds();
        SetState(EHeroLedgeState::None);
    }
}

// ---------------------------------------------------------------- parry helper
void UHeroParryLibrary::ConsumeParry(ACharacter* InHero, AActor* From)
{
    if (!InHero) return;
    if (FBoolProperty* P = FindFProperty<FBoolProperty>(InHero->GetClass(), TEXT("bIsParrying")))
        P->SetPropertyValue_InContainer(InHero, false);
    if (From)
    {
        FVector To = From->GetActorLocation() - InHero->GetActorLocation();
        To.Z = 0.f;
        if (!To.IsNearlyZero()) InHero->SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
    }
}
