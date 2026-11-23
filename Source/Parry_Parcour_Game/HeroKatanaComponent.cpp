#include "HeroKatanaComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

UHeroKatanaComponent::UHeroKatanaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(true);
}

void UHeroKatanaComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const ACharacter* C = Cast<ACharacter>(GetOwner()))
        if (USkeletalMeshComponent* Mesh = C->GetMesh())
        {
            FName Bone = HandBone;
            if (Mesh->GetBoneIndex(Bone) == INDEX_NONE && Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE) Bone = TEXT("hand_r");
            AttachToComponent(Mesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false), Bone);
        }
    Place();
}

void UHeroKatanaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Place();
}

void UHeroKatanaComponent::Place()
{
    const USceneComponent* Parent = GetAttachParent();
    if (!Parent || !GetStaticMesh()) return;
    // Size: the mesh's Z extent should become Length cm in the world.
    const FBox Box = GetStaticMesh()->GetBoundingBox();
    const float MeshLen = FMath::Max(1.f, Box.Max.Z - Box.Min.Z);
    const float Scale = Length / MeshLen;
    SetWorldScale3D(FVector(Scale));
    // Rotation relative to the hand bone; grip point (on the mesh) lands at GripOffset from the bone.
    SetRelativeRotation(GripRotation);
    const float ParentScale = FMath::Max(KINDA_SMALL_NUMBER, Parent->GetComponentScale().X);
    const FVector GripOnMesh(0.f, 0.f, Box.GetCenter().Z + GripAlongZ * MeshLen);
    const FVector GripInBoneCm = GripRotation.RotateVector(GripOnMesh * Scale);
    SetRelativeLocation((GripOffset - GripInBoneCm) / ParentScale);
}
