#include "HeroMovementExtras.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"

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
            bDefaultControllerRotation = Move->bUseControllerDesiredRotation;
            bHaveDefaults = true;
        }
}

void UHeroMovementExtras::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ACharacter* H = Hero();
    if (!H) return;
    StateTime += DeltaTime;
    // Test input from the Parry.SimMove console command (lets automated tests "tap" a direction).
    if (GetWorld()->GetTimeSeconds() < SimMoveUntil) H->AddMovementInput(SimMoveDir, 1.f);
    switch (LedgeState)
    {
    case EHeroLedgeState::None: TryGrab(); break;
    case EHeroLedgeState::Grabbing:
    case EHeroLedgeState::Hanging: TickHanging(DeltaTime); break;
    default: TickClimb(DeltaTime); break;
    }
    UpdateLockOn(DeltaTime);
    UpdateTurnInPlace(DeltaTime);
    UpdateAirRules();
}

// Parry.SimMove <X> <Y> <Seconds>: pushes movement input (world direction) on every playing hero for a while.
static void SimMove(const TArray<FString>& Args, UWorld* World)
{
    if (Args.Num() < 3) return;
    const FVector Dir(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), 0.f);
    for (TObjectIterator<UHeroMovementExtras> It; It; ++It)
    {
        UWorld* W = It->GetWorld();
        if (!W || !W->IsGameWorld()) continue;
        It->SimMoveDir = Dir.GetSafeNormal();
        It->SimMoveUntil = W->GetTimeSeconds() + FCString::Atof(*Args[2]);
        UE_LOG(LogTemp, Display, TEXT("ParrySim move %s dir=%s until=%.2f (now %.2f)"), *It->GetPathName(), *It->SimMoveDir.ToString(), It->SimMoveUntil, W->GetTimeSeconds());
    }
}
static FAutoConsoleCommandWithWorldAndArgs SimMoveCmd(TEXT("Parry.SimMove"), TEXT("Test: push movement input X Y for Seconds."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SimMove));

// ---------------------------------------------------------------- turn in place
void UHeroMovementExtras::UpdateTurnInPlace(float Dt)
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    if (!Move) return;
    const float Now = GetWorld()->GetTimeSeconds();
    const float Speed = FVector(H->GetVelocity().X, H->GetVelocity().Y, 0.f).Size();
    if (Speed < 20.f) LastStandTime = Now;
    const bool bAllowed = !bStrafing && !IsOnLedge() && !ReadBool(TEXT("bIsDashing")) && Move->IsMovingOnGround();

    FVector Input = H->GetPendingMovementInputVector();
    if (Input.IsNearlyZero()) Input = H->GetLastMovementInputVector();
    Input.Z = 0.f;

    if (!bTurningInPlace)
    {
        // Short tap from standing: the input points well away from where the hero looks.
        if (!bAllowed || Input.Size() < .1f || Now - LastStandTime > .15f) return;
        const float Delta = FMath::FindDeltaAngleDegrees(H->GetActorRotation().Yaw, Input.Rotation().Yaw);
        if (FMath::Abs(Delta) < TurnMinAngle) return;
        bTurningInPlace = true;
        TurnDir = FMath::Sign(Delta);
        TurnTargetYaw = Input.Rotation().Yaw;
        TurnStartTime = Now;
    }
    // Keep the target following the stick while it is held.
    if (Input.Size() >= .1f) TurnTargetYaw = Input.Rotation().Yaw;
    const float Left = FMath::FindDeltaAngleDegrees(H->GetActorRotation().Yaw, TurnTargetYaw);
    const bool bDone = !bAllowed || FMath::Abs(Left) < 2.f || Speed > TurnAbortSpeed || Now - TurnStartTime > 1.f;
    if (bDone)
    {
        bTurningInPlace = false;
        if (bHaveDefaults && !bStrafing) { Move->bOrientRotationToMovement = bDefaultOrientToMovement; Move->bUseControllerDesiredRotation = bDefaultControllerRotation; }
        return;
    }
    // Rotate at the turn-clip pace instead of snapping round, so the clip and the body turn together.
    // Neither the run direction nor the camera may turn the hero while the turn plays.
    Move->bOrientRotationToMovement = false;
    Move->bUseControllerDesiredRotation = false;
    const float Step = FMath::Clamp(Left, -TurnDegPerSec * Dt, TurnDegPerSec * Dt);
    H->SetActorRotation(FRotator(0.f, H->GetActorRotation().Yaw + Step, 0.f));
}

// ---------------------------------------------------------------- air rules (jumps / dash)
void UHeroMovementExtras::UpdateAirRules()
{
    ACharacter* H = Hero();
    UCharacterMovementComponent* Move = H->GetCharacterMovement();
    if (!Move) return;
    // Real ground check: the dash ends by switching to "walking" even in mid-air, which made the movement
    // component claim ground for a frame and refilled jumps and the air dash.
    const bool bOnGround = IsOnGround();
    const bool bDashing = ReadBool(TEXT("bIsDashing"));
    const bool bCounter = ReadBool(TEXT("bIsCounterDash"));
    const float Now = GetWorld()->GetTimeSeconds();

    // The Blueprint (coyote time, landing) sets JumpMaxCount itself; remember its value unless it is ours.
    if (H->JumpMaxCount != WrittenMaxCount) BaseMaxCount = H->JumpMaxCount;

    if (bOnGround || IsOnLedge())
    {
        // Back on the ground (or holding a ledge): air budget refilled.
        AirJumpsUsed = 0;
        bAirDashUsed = false;
        if (WrittenMaxCount != INDEX_NONE) { H->JumpMaxCount = BaseMaxCount; WrittenMaxCount = INDEX_NONE; }
    }
    else
    {
        // Every movement-mode change (the dash switches to flying and back) resets the jump counter in Unreal,
        // and the dash-jump does that in the very same moment it jumps. So besides restoring the counter, the
        // allowed jump count is lowered: after a reset Unreal counts the fall as one jump, so with N jumps used
        // the max becomes Base - N + 1 (both with and without reset exactly the remaining jumps are possible).
        AirJumpsUsed = FMath::Max(AirJumpsUsed, H->JumpCurrentCount);
        if (H->JumpCurrentCount < AirJumpsUsed) H->JumpCurrentCount = AirJumpsUsed;
        if (AirJumpsUsed > 0 || bAirDashUsed || bDashing)
        {
            const int32 Want = FMath::Max(1, BaseMaxCount - AirJumpsUsed + 1);
            H->JumpMaxCount = Want;
            WrittenMaxCount = Want;
        }
    }

    // A normal dash started in the air uses up the air dash until landing (checked by AirDashAllowed).
    if (bDashing && !bWasDashing)
    {
        LastDashStart = Now;
        if (!bWasOnGround && !bCounter) bAirDashUsed = true;
    }
    bWasDashing = bDashing;
    bWasOnGround = bOnGround;
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
    if (Move && bHaveDefaults)
    {
        // While strafing the hero faces the target; the camera direction must not pull it round either.
        Move->bOrientRotationToMovement = bStrafing ? false : bDefaultOrientToMovement;
        Move->bUseControllerDesiredRotation = bStrafing ? false : bDefaultControllerRotation;
    }
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
    SetState(GrabTime > 0.f ? EHeroLedgeState::Grabbing : EHeroLedgeState::Hanging);
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
    // This component ticks before the movement component, so this frame's input is still pending.
    FVector Input = H->GetPendingMovementInputVector();
    if (Input.IsNearlyZero()) Input = H->GetLastMovementInputVector();
    const bool bJump = H->bPressedJump;
    H->bPressedJump = false;
    // While the grab animation plays, only hang (the grab clip is lined up with the hang clip, no mesh shift needed).
    if (LedgeState == EHeroLedgeState::Grabbing)
    {
        if (StateTime >= GrabTime) SetState(EHeroLedgeState::Hanging);
        return;
    }

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
        if (!bBlocked)
        {
            ShimmyDir = 0.f;
            // The mesh stays where it is and plays the climb with its own (original) motion,
            // while the capsule (and camera) take a clean path: straight up, then over the edge.
            if (USkeletalMeshComponent* Mesh = H->GetMesh())
            {
                MeshRelative = Mesh->GetRelativeLocation();
                MeshWorldDuringClimb = Mesh->GetComponentLocation();
            }
            SetState(EHeroLedgeState::ClimbUp);
            return;
        }
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
        // Nothing in the way to the side (the capsule touches the wall, so no sweep; a side trace instead).
        FHitResult SideHit;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroShimmy), false, H);
        const FVector C = H->GetActorLocation();
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(SideHit, C, C + Right * Dir * (Radius + 15.f), ECC_Visibility, Q);
        if (bEdge && !bBlocked)
        {
            H->SetActorLocation(C + Step, false, nullptr, ETeleportType::TeleportPhysics);
            ShimmyDir = Dir;
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
        const float A = FMath::Clamp(StateTime / FMath::Max(.05f, ClimbUpTime), 0.f, 1.f);
        const float UpA = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(A / .65f, 0.f, 1.f), 2.f);
        const float OverA = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((A - .65f) / .35f, 0.f, 1.f), 2.f);
        FVector P = FMath::Lerp(ClimbFrom, ClimbTo, OverA);
        P.Z = FMath::Lerp(ClimbFrom.Z, ClimbTo.Z, UpA);
        H->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
        // The mesh stays where the hang pose was (climb clip is lined up with the hang clip).
        if (USkeletalMeshComponent* Mesh = H->GetMesh()) Mesh->SetWorldLocation(MeshWorldDuringClimb);
        if (A >= 1.f)
        {
            if (USkeletalMeshComponent* Mesh = H->GetMesh())
            {
                // Where the pelvis ends after the climb; the stand-up clip starts with its pelvis (0.004, 0.013, 0.508)
                // in mesh space. The difference is faded out while standing up instead of popping.
                const FName Hips = Mesh->GetBoneIndex(TEXT("Hips")) != INDEX_NONE ? FName(TEXT("Hips")) : FName(TEXT("pelvis"));
                const FVector EndHips = Mesh->GetBoneLocation(Hips);
                FTransform HomeT = Mesh->GetComponentTransform();
                HomeT.SetLocation(H->GetActorTransform().TransformPosition(MeshRelative));
                StandFixOffset = EndHips - HomeT.TransformPosition(FVector(.0043f, .0134f, .5075f));
            }
            SetState(EHeroLedgeState::StandUp);
        }
    }
    else if (StateTime < StandUpTime)
    {
        // Slide the mesh back under the capsule while standing up (same time as the clip crossfade and a bit more).
        if (USkeletalMeshComponent* Mesh = H->GetMesh())
        {
            const float B = FMath::Clamp(StateTime / .25f, 0.f, 1.f);
            const FVector Home = H->GetActorTransform().TransformPosition(MeshRelative);
            Mesh->SetWorldLocation(Home + StandFixOffset * (1.f - FMath::InterpEaseInOut(0.f, 1.f, B, 2.f)));
        }
    }
    else
    {
        if (USkeletalMeshComponent* Mesh = H->GetMesh()) Mesh->SetRelativeLocation(MeshRelative);
        if (Move) Move->SetMovementMode(MOVE_Walking);
        ReleasedAt = GetWorld()->GetTimeSeconds();
        SetState(EHeroLedgeState::None);
    }
}

bool UHeroMovementExtras::IsOnGround() const
{
    const ACharacter* H = Hero();
    const UCharacterMovementComponent* Move = H ? H->GetCharacterMovement() : nullptr;
    if (!Move || Move->Velocity.Z > 50.f) return false;
    // Short trace below the capsule bottom: walkable floor there = on the ground, whatever the movement mode says.
    const float Half = H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = H->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector C = H->GetActorLocation();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroGround), false, H);
    // Center and four points near the capsule edge, so standing on a ledge edge still counts.
    for (const FVector& O : {FVector::ZeroVector, FVector(Radius * .7f, 0, 0), FVector(-Radius * .7f, 0, 0), FVector(0, Radius * .7f, 0), FVector(0, -Radius * .7f, 0)})
    {
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, C + O, C + O - FVector(0, 0, Half + 12.f), ECC_Visibility, Q) && Hit.ImpactNormal.Z > .7f)
            return true;
    }
    return false;
}

bool UHeroParryLibrary::AirDashAllowed(ACharacter* InHero)
{
    const UHeroMovementExtras* X = InHero ? InHero->FindComponentByClass<UHeroMovementExtras>() : nullptr;
    if (!X) return true;
    // Starting a dash from the ground is always fine; in the air only once per air time.
    return X->IsOnGround() || !X->bAirDashUsed;
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
