#include "SwordCombatTest.h"
#include "CombatPreviewAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace CombatTest
{
float Health(ACharacter* Character, FName Field)
{
    if (Character) for (UActorComponent* C : Character->GetComponents())
        if (C->GetClass()->GetName().Contains(TEXT("AC_Health")))
            if (auto* P=FindFProperty<FNumericProperty>(C->GetClass(),Field))
            {
                const void* Value=P->ContainerPtrToValuePtr<void>(C);
                return P->IsFloatingPoint()?float(P->GetFloatingPointPropertyValue(Value)):float(P->GetSignedIntPropertyValue(Value));
            }
    return 0.f;
}
bool Bool(UObject* O, FName N)
{
    if (O) if (auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N)) return P->GetPropertyValue_InContainer(O);
    return false;
}
void SetBool(UObject* O, FName N, bool V)
{
    if (O) if (auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N)) P->SetPropertyValue_InContainer(O,V);
}
void Call(UObject* O, FName N)
{
    if (O) if (auto* F=O->FindFunction(N)) if (F->ParmsSize==0) O->ProcessEvent(F,nullptr);
}
float Distance(const FVector& A,const FVector& B,const FVector& C,const FVector& D,FVector& Point)
{
    FVector P,Q;
    FMath::SegmentDistToSegmentSafe(A,B,C,D,P,Q);
    Point=(P+Q)*.5;
    return FVector::Distance(P,Q);
}
}

ASwordCombatTest::ASwordCombatTest()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("TestOrigin"));
}
void ASwordCombatTest::BeginPlay()
{
    Super::BeginPlay();
    HeroMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/_Game/Art/CombatTest/Hero/SK_Hero.SK_Hero"));
    BossMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/_Game/Art/CombatTest/Boss/SK_Boss.SK_Boss"));
    for (const TCHAR* Name : {TEXT("Idle"),TEXT("Run"),TEXT("Jump"),TEXT("Dash"),TEXT("Parry")})
        HeroClips.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/_Game/Art/CombatTest/Hero/Anim_Hero_%s.Anim_Hero_%s"),Name,Name)));
    for (const TCHAR* Name : {TEXT("Idle"),TEXT("Combo"),TEXT("Sweep"),TEXT("Walk"),TEXT("Cast")})
        BossClips.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/_Game/Art/CombatTest/Boss/Anim_Boss_%s.Anim_Boss_%s"),Name,Name)));
    for (int32 I=1;I<=4;++I)
    {
        ParrySounds.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/_Game/Audio/CombatTest/steel_parry_%02d.steel_parry_%02d"),I,I)));
        ClashSounds.Add(LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/_Game/Audio/CombatTest/sword_clash_%02d.sword_clash_%02d"),I,I)));
    }
    UE_LOG(LogTemp,Display,TEXT("SwordCombatTest geometry checks: %s"),*RunBladeContactChecks());
}
void ASwordCombatTest::InitializeFighters()
{
    Hero=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Hero || !HeroMesh || !BossMesh) return;
    if (bTrainingMode)
    {
        for (UActorComponent* C : Hero->GetComponents()) if (C->GetClass()->GetName().Contains(TEXT("AC_Health")))
            for (FName N : {FName("MaxHealth"),FName("Health")})
                if (auto* P=FindFProperty<FNumericProperty>(C->GetClass(),N))
                {
                    void* Value=P->ContainerPtrToValuePtr<void>(C);
                    if (P->IsFloatingPoint()) P->SetFloatingPointPropertyValue(Value,1000.0);
                    else P->SetIntPropertyValue(Value,int64(1000));
                }
    }
    auto* Capsule=Hero->GetCapsuleComponent();
    Capsule->SetCapsuleSize(24.f,52.f);
    auto* Mesh=Hero->GetMesh();
    Mesh->SetSkeletalMeshAsset(HeroMesh);
    Mesh->SetRelativeLocation(FVector(0,0,-52));
    Mesh->SetRelativeRotation(FRotator(0,-90,0));
    Mesh->SetRelativeScale3D(FVector::OneVector);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetAnimInstanceClass(UCombatPreviewAnimInstance::StaticClass());
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    if (auto* Arm=Hero->FindComponentByClass<USpringArmComponent>())
    {
        Arm->TargetArmLength=580;
        Arm->SocketOffset=FVector(0,0,70);
        Arm->bEnableCameraLag=true;
        Arm->CameraLagSpeed=12;
    }
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (PC) PC->SetControlRotation(FRotator(-12,0,0));
    UClass* EnemyClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/_Game/Enemies/BP_EnemyBase.BP_EnemyBase_C"));
    if (!EnemyClass) return;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Boss=GetWorld()->SpawnActor<ACharacter>(EnemyClass,GetActorLocation()+FVector(180,0,154),FRotator(0,180,0),Spawn);
    Boss->Tags.AddUnique(TEXT("Enemy"));
    Boss->GetCapsuleComponent()->SetCapsuleSize(48,150);
    auto* BM=Boss->GetMesh();
    BM->SetSkeletalMeshAsset(BossMesh);
    BM->SetRelativeScale3D(FVector::OneVector);
    BM->SetRelativeLocation(FVector(0,0,-150));
    BM->SetRelativeRotation(FRotator(0,-90,0));
    BM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BM->SetAnimInstanceClass(UCombatPreviewAnimInstance::StaticClass());
    BM->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Boss->GetCharacterMovement()->GravityScale=2.8f;
    Boss->GetCharacterMovement()->bRunPhysicsWithNoController=true;
    // The existing enemy owns damage, marking, stagger and death; this adapter owns only its test attack schedule.
    for (UActorComponent* C : Boss->GetComponents())
    {
        if (C->GetClass()->GetName().Contains(TEXT("AC_Health")))
        {
            for (FName N : {FName("MaxHealth"),FName("Health")})
                if (auto* P=FindFProperty<FFloatProperty>(C->GetClass(),N)) P->SetPropertyValue_InContainer(C,40.f);
        }
        if (auto* Scene=Cast<USceneComponent>(C); Scene && C->GetName().Contains(TEXT("AttackWarning"))) Scene->SetRelativeLocation(FVector(0,0,195));
    }
    if (auto* P=FindFProperty<FObjectProperty>(Boss->GetClass(),TEXT("StaggerMontage"))) P->SetObjectPropertyValue_InContainer(Boss,nullptr);
    Animate(Hero,HeroClips[0],true);
    Animate(Boss,BossClips[0],true);
    LastBossHealth=CombatTest::Health(Boss,TEXT("Health"));
    bInitialized=true;
    UE_LOG(LogTemp,Display,TEXT("SwordCombatTest ready: hero 100cm, boss 300cm; exact 3:1 reference mesh height. Blueprint player preserved."));
}
void ASwordCombatTest::Animate(ACharacter* Fighter,UAnimSequence* Clip,bool Loop,float Rate,bool Restart)
{
    if (Fighter) if (auto* Anim=Cast<UCombatPreviewAnimInstance>(Fighter->GetMesh()->GetAnimInstance())) Anim->PlayClip(Clip,Loop,Rate,Restart);
}
void ASwordCombatTest::Tick(float Delta)
{
    Super::Tick(Delta);
    Clock+=Delta;
    if (!bInitialized) { InitializeFighters(); return; }
    if (!IsValid(Hero) || !IsValid(Boss)) { DrawHUD(); return; }
    const float BossHealth=CombatTest::Health(Boss,TEXT("Health"));
    if (LastBossHealth>=0.f && BossHealth<LastBossHealth)
    {
        Feedback=FString::Printf(TEXT("COUNTER-DASH HIT: %.0f damage"),LastBossHealth-BossHealth);
        UE_LOG(LogTemp,Display,TEXT("SwordCombatTest COUNTER_DAMAGE %.0f; boss HP %.0f"),LastBossHealth-BossHealth,BossHealth);
    }
    LastBossHealth=BossHealth;
    if (CombatTest::Bool(Boss,TEXT("bIsDead"))) { Feedback=TEXT("BOSS DEFEATED - R to restart the test"); DrawHUD(); return; }
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!PC) return;
    if (auto* Anim=Cast<UCombatPreviewAnimInstance>(Hero->GetMesh()->GetAnimInstance())) Anim->GuardDirection=HeroGuardDirection;
    if (PC->WasInputKeyJustPressed(EKeys::F1)) bShowBladeTraces=!bShowBladeTraces;
    if (PC->WasInputKeyJustPressed(EKeys::F2)) bAutoParry=!bAutoParry;
    if (PC->WasInputKeyJustPressed(EKeys::R)) { UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true))); return; }
    bool Guard=CombatTest::Bool(Hero,TEXT("bIsParrying"));
    if (bAutoParry && AttackTime>=0 && Clock<StaggerUntil+.01f) Guard=false;
    if (bAutoParry && AttackTime>=0 && Clock-AttackTime>.15f) Guard=true;
    if (Guard && !bWasParrying)
    {
        HeroPoseUntil=Clock+.55f;
        Animate(Hero,HeroClips[4],false,3.f,true);
    }
    bWasParrying=Guard;
    if (CombatTest::Bool(Hero,TEXT("bIsDashing"))) { Animate(Hero,HeroClips[3],false,2.5f); HeroPoseUntil=Clock+.12f; }
    else if (bAutoParry && Guard && Clock>HeroPoseUntil-.35f) Animate(Hero,HeroClips[4],false,0.f);
    else if (Clock>HeroPoseUntil && !Guard)
    {
        if (Hero->GetCharacterMovement()->IsFalling()) Animate(Hero,HeroClips[2],false,1.5f);
        else Animate(Hero,HeroClips[Hero->GetVelocity().Size2D()>15?1:0],true,1.f);
    }
    FVector Toward=Hero->GetActorLocation()-Boss->GetActorLocation(); Toward.Z=0;
    if (AttackTime<0 && Clock>StaggerUntil)
    {
        Boss->SetActorRotation(Toward.Rotation());
        if (Toward.Size()>ApproachDistance && Toward.Size()<1400)
        {
            FVector Move=Toward.GetSafeNormal()*FMath::Min(120.f*Delta,Toward.Size()-ApproachDistance);
            Boss->AddActorWorldOffset(Move,true);
            Animate(Boss,BossClips[3],true);
        }
        else Animate(Boss,BossClips[0],true);
        if (Clock>=NextAttack && Toward.Size()<400)
        {
            AttackTime=Clock+.5f;
            bSwingResolved=false; bCastSpawned=false; bHavePrevious=false; bClashPlayed=false;
            CombatTest::Call(Boss,TEXT("PlayTelegraph"));
            Feedback=TEXT("Incoming attack: red ! = wind-up; red blade = live hitbox");
        }
    }
    const float Elapsed=Clock-AttackTime;
    const bool Casting=(AttackIndex%4)==3;
    const int32 ClipIndex=Casting?4:((AttackIndex%2)?2:1);
    const float Duration=BossClips[ClipIndex]?BossClips[ClipIndex]->GetPlayLength()/1.8f:2.f;
    if (AttackTime>=0 && Elapsed>=0)
    {
        Animate(Boss,BossClips[ClipIndex],false,1.8f);
        const float Progress=Elapsed/FMath::Max(.1f,Duration);
        if (Casting && Progress>.4f && !bCastSpawned)
        {
            UClass* Projectile=LoadClass<AActor>(nullptr,TEXT("/Game/_Game/Projectiles/BP_projectileBase.BP_projectileBase_C"));
            if (Projectile)
            {
                FVector Start=Boss->GetActorLocation()+Boss->GetActorForwardVector()*90+FVector(0,0,-65);
                FActorSpawnParameters Spawn; Spawn.Owner=Boss; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                GetWorld()->SpawnActor<AActor>(Projectile,Start,(Hero->GetActorLocation()-Start).Rotation(),Spawn);
            }
            bCastSpawned=true;
        }
        ResolveBladeContact(!Casting && Progress>=ActiveStart && Progress<=ActiveEnd && !bSwingResolved,Guard);
        if (Elapsed>Duration+.2f && AttackTime>=0)
        {
            AttackTime=-1; NextAttack=Clock+AttackInterval; ++AttackIndex;
            bHavePrevious=false;
        }
    }
    else ResolveBladeContact(false,Guard);
    if (Hero->GetActorLocation().Z<GetActorLocation().Z-650) UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)));
    DrawHUD();
}
void ASwordCombatTest::ResolveBladeContact(bool Active,bool Guard)
{
    auto* HM=Hero->GetMesh(); auto* BM=Boss->GetMesh();
    FVector HB=HM->GetSocketLocation(TEXT("BladeBase")),HT=HM->GetSocketLocation(TEXT("BladeTip"));
    FVector BB=BM->GetSocketLocation(TEXT("BladeBase")),BT=BM->GetSocketLocation(TEXT("BladeTip"));
    const float Motion=bHavePrevious?FMath::Max(FVector::Distance(BT,PrevBossTip),FVector::Distance(HT,PrevHeroTip)):0.f;
    int32 Steps=FMath::Clamp(FMath::CeilToInt(Motion/FMath::Max(1.f,BladeRadius)),1,64);
    FVector Point;
    if (Active)
    {
        for (int32 I=1;I<=Steps;++I)
        {
            float T=float(I)/Steps;
            FVector A=bHavePrevious?FMath::Lerp(PrevBossBase,BB,T):BB;
            FVector B=bHavePrevious?FMath::Lerp(PrevBossTip,BT,T):BT;
            FVector C=bHavePrevious?FMath::Lerp(PrevHeroBase,HB,T):HB;
            FVector D=bHavePrevious?FMath::Lerp(PrevHeroTip,HT,T):HT;
            if (CombatTest::Distance(A,B,C,D,Point)<=BladeRadius*2)
            {
                if (Guard) { Parry(Point); break; }
                if (!bClashPlayed)
                {
                    bClashPlayed=true;
                    if (ClashSounds.Num() && ClashSounds[0]) UGameplayStatics::PlaySoundAtLocation(this,ClashSounds[FMath::RandRange(0,ClashSounds.Num()-1)],Point,.6f);
                    UE_LOG(LogTemp,Display,TEXT("SwordCombatTest BLADE_CLASH outside parry window"));
                }
            }
            const float Half=Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            const float Radius=Hero->GetCapsuleComponent()->GetScaledCapsuleRadius();
            const FVector Center=Hero->GetActorLocation();
            const FVector Axis(0,0,Half-Radius);
            if (!CombatTest::Bool(Hero,TEXT("bIsDashing")) && CombatTest::Distance(A,B,Center-Axis,Center+Axis,Point)<=Radius+BladeRadius)
            {
                ++BodyHitCount; bSwingResolved=true;
                UGameplayStatics::ApplyDamage(Hero,1.f,Boss->GetController(),Boss,nullptr);
                Feedback=TEXT("BODY HIT - no successful blade parry");
                if (ClashSounds.Num() && ClashSounds[0]) UGameplayStatics::PlaySoundAtLocation(this,ClashSounds[FMath::RandRange(0,ClashSounds.Num()-1)],Point,.45f);
                UE_LOG(LogTemp,Display,TEXT("SwordCombatTest BODY_HIT %d"),BodyHitCount);
                break;
            }
        }
    }
    if (bShowBladeTraces)
    {
        DrawDebugLine(GetWorld(),HB,HT,Guard?FColor::Cyan:FColor::Green,false,0,0,2.5f);
        DrawDebugLine(GetWorld(),BB,BT,Active?FColor::Red:FColor::Yellow,false,0,0,3.f);
        DrawDebugSphere(GetWorld(),HT,BladeRadius,8,Guard?FColor::Cyan:FColor::Green,false,0);
        DrawDebugSphere(GetWorld(),BT,BladeRadius,8,Active?FColor::Red:FColor::Yellow,false,0);
    }
    PrevHeroBase=HB; PrevHeroTip=HT; PrevBossBase=BB; PrevBossTip=BT; bHavePrevious=true;
}
void ASwordCombatTest::Parry(const FVector& Point)
{
    ++ParryCount; bSwingResolved=true;
    if (auto* P=FindFProperty<FObjectProperty>(Hero->GetClass(),TEXT("DashTarget"))) P->SetObjectPropertyValue_InContainer(Hero,Boss);
    CombatTest::Call(Hero,TEXT("OnParrySuccess"));
    CombatTest::Call(Boss,TEXT("OnParried"));
    StaggerUntil=Clock+1.5f; AttackTime=-1; NextAttack=StaggerUntil+AttackInterval; ++AttackIndex;
    Animate(Boss,BossClips[0],true);
    if (ParrySounds.Num()) if (USoundBase* Sound=ParrySounds[FMath::RandRange(0,ParrySounds.Num()-1)].Get()) UGameplayStatics::PlaySoundAtLocation(this,Sound,Point,.8f,FMath::FRandRange(.96f,1.04f));
    DrawDebugSphere(GetWorld(),Point,14,12,FColor::Cyan,false,.4f,0,3.f);
    for(int32 I=0;I<12;++I) DrawDebugLine(GetWorld(),Point,Point+FMath::VRand()*FMath::FRandRange(20.f,55.f),FColor(255,210,90),false,.18f,0,1.5f);
    Feedback=TEXT("PARRY - actual blade contact. Left click to counter-dash.");
    UE_LOG(LogTemp,Display,TEXT("SwordCombatTest BLADE_PARRY %d at %s"),ParryCount,*Point.ToString());
}
void ASwordCombatTest::DrawHUD()
{
    if (!GEngine) return;
    GEngine->AddOnScreenDebugMessage(7101,0.f,FColor(230,220,185),TEXT("MOSSBOUND FORT | Hero 1 m : Boss 3 m | sword collision test"));
    GEngine->AddOnScreenDebugMessage(7102,0.f,FColor::White,TEXT("WASD move | Space double jump | RMB parry | Shift dash | LMB counter-dash | R reset"));
    GEngine->AddOnScreenDebugMessage(7103,0.f,FColor::White,FString::Printf(TEXT("F1 blade traces | F2 auto guard: %s | Blade contacts %d | Body hits %d"),bAutoParry?TEXT("ON"):TEXT("OFF"),ParryCount,BodyHitCount));
    GEngine->AddOnScreenDebugMessage(7104,0.f,FColor::Cyan,Feedback);
    if (IsValid(Boss)) GEngine->AddOnScreenDebugMessage(7105,0.f,FColor(255,200,110),FString::Printf(TEXT("Boss HP: %.0f / %.0f | Parry, then LEFT CLICK to dash through and deal damage"),CombatTest::Health(Boss,TEXT("Health")),CombatTest::Health(Boss,TEXT("MaxHealth"))));
}
FString ASwordCombatTest::RunBladeContactChecks()
{
    FVector P;
    const bool Crossing=CombatTest::Distance(FVector(-20,0,0),FVector(20,0,0),FVector(0,-20,0),FVector(0,20,0),P)<.01f;
    const bool Apart=CombatTest::Distance(FVector(-20,0,0),FVector(20,0,0),FVector(-20,30,0),FVector(20,30,0),P)>BladeRadius*2;
    const bool Touching=CombatTest::Distance(FVector(-20,0,0),FVector(20,0,0),FVector(-20,BladeRadius*1.5,0),FVector(20,BladeRadius*1.5,0),P)<=BladeRadius*2;
    return FString::Printf(TEXT("crossing=%s separated=%s grazing=%s"),Crossing?TEXT("PASS"):TEXT("FAIL"),Apart?TEXT("PASS"):TEXT("FAIL"),Touching?TEXT("PASS"):TEXT("FAIL"));
}
FString ASwordCombatTest::GetCombatDiagnostics()
{
    return FString::Printf(TEXT("time=%.2f attack=%.2f next=%.2f hero=%s boss=%s parries=%d hits=%d bladeBones=%d/%d"),Clock,AttackTime,NextAttack,IsValid(Hero)?*Hero->GetActorLocation().ToString():TEXT("missing"),IsValid(Boss)?*Boss->GetActorLocation().ToString():TEXT("missing"),ParryCount,BodyHitCount,IsValid(Hero)&&Hero->GetMesh()->DoesSocketExist(TEXT("BladeTip")),IsValid(Boss)&&Boss->GetMesh()->DoesSocketExist(TEXT("BladeTip")));
}
