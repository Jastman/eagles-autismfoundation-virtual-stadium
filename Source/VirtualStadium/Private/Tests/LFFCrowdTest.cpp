#include "World/LFFStadiumCrowdSpawner.h"
#include "World/LFFCrowdFanActor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFStaffExpressionTest, "LFF.Crowd.StaffExpression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFStaffExpressionTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* FaceAsset = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/MetaHumanCrowdContent/Characters/Assembled/AverageMale_Head02/Face/SKM_MHC_AverageMale_Head02_FaceMesh"));
    UAnimSequence* Smile = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/LFF/People/AS_LFF_StaffSmile"));
    if (!TestNotNull(TEXT("Actual staff face"), FaceAsset) || !TestNotNull(TEXT("Baked staff smile"), Smile))
    {
        return false;
    }
    TestTrue(TEXT("The facial clip uses the assembled face's exact skeleton"), Smile->GetSkeleton() == FaceAsset->GetSkeleton());
    const FSkeletalMeshRenderData* Data = FaceAsset->GetResourceForRendering();
    if (!TestTrue(TEXT("Staff face has renderable geometry"), Data && !Data->LODRenderData.IsEmpty()))
    {
        return false;
    }
    const FSkeletalMeshLODRenderData& LOD = Data->LODRenderData[0];
    const FSkinWeightVertexBuffer* Weights = LOD.GetSkinWeightVertexBuffer();
    if (!TestNotNull(TEXT("Staff face skin weights"), Weights))
    {
        return false;
    }
    int32 SmileVertices = 0;
    for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
    {
        for (uint32 Vertex = Section.BaseVertexIndex; Vertex < Section.BaseVertexIndex + Section.NumVertices; ++Vertex)
        {
            for (uint32 Influence = 0; Influence < Weights->GetMaxBoneInfluences(); ++Influence)
            {
                const uint32 LocalBone = Weights->GetBoneIndex(Vertex, Influence);
                if (Weights->GetBoneWeight(Vertex, Influence) > 0 && Section.BoneMap.IsValidIndex(LocalBone)
                    && FaceAsset->GetRefSkeleton().GetBoneName(Section.BoneMap[LocalBone]).ToString().Contains(TEXT("LipCorner")))
                {
                    ++SmileVertices;
                    break;
                }
            }
        }
    }
    AddInfo(FString::Printf(TEXT("Staff LOD0 lip-corner weighted vertices: %d"), SmileVertices));
    TestTrue(TEXT("The expression deforms actual visible lip geometry, not unweighted control bones"), SmileVertices > 0);
    const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    ALFFPersonActor* Staff = World->SpawnActor<ALFFPersonActor>();
    if (!TestNotNull(TEXT("Staff fixture spawned"), Staff))
    {
        World->DestroyWorld(false);
        return false;
    }
    Staff->FaceMesh = FaceAsset;
    Staff->FacialAnimation = Smile;
    Staff->OnConstruction(FTransform::Identity);
    Staff->DispatchBeginPlay();
    USkeletalMeshComponent* Face = nullptr;
    Staff->ForEachComponent<USkeletalMeshComponent>(false, [&Face](USkeletalMeshComponent* Component)
    {
        if (Component->GetFName() == TEXT("Face"))
        {
            Face = Component;
        }
    });
    if (TestNotNull(TEXT("Independent facial component"), Face))
    {
        TestFalse(TEXT("Body pose must not override the facial expression"), Face->LeaderPoseComponent.IsValid());
        TestEqual(TEXT("The greeting retains facial joints at both display sizes"), Face->GetForcedLOD(), 1);
        TestTrue(TEXT("The actual facial component plays the saved smile"), Face->GetSingleNodeInstance()
            && Face->GetSingleNodeInstance()->GetCurrentAsset() == Smile);
        Face->TickAnimation(0.5f, false);
        Face->RefreshBoneTransforms();
        for (const FName Corner : { FName(TEXT("FACIAL_L_LipCorner")), FName(TEXT("FACIAL_R_LipCorner")) })
        {
            const int32 Index = FaceAsset->GetRefSkeleton().FindBoneIndex(Corner);
            const int32 Parent = FaceAsset->GetRefSkeleton().GetParentIndex(Index);
            const FTransform Actual = Face->GetBoneTransform(Index).GetRelativeTransform(Face->GetBoneTransform(Parent));
            const double Displacement = (Actual.GetTranslation() - FaceAsset->GetRefSkeleton().GetRefBonePose()[Index].GetTranslation()).Length();
            AddInfo(FString::Printf(TEXT("Rendered %s local displacement %.3f cm"), *Corner.ToString(), Displacement));
            TestTrue(TEXT("The complete facial animation graph retains the visible smile"), Displacement > 0.5);
        }
    }
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFClothingCoverageTest, "LFF.Crowd.ClothingCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFClothingCoverageTest::RunTest(const FString& Parameters)
{
    const TArray<FName> Covered = { TEXT("torso"), TEXT("legs"), TEXT("hands") };
    const TArray<FName> Exposed = { TEXT("hands"), TEXT("head") };
    TestTrue(TEXT("Only authored covered skin sections are suppressed"), LFFClothingRules::CanHideSlot(TEXT("torso"), Covered, Exposed));
    TestTrue(TEXT("Covered trousers suppress the leg section"), LFFClothingRules::CanHideSlot(TEXT("legs"), Covered, Exposed));
    TestFalse(TEXT("Explicit exposed hands survive even incorrect garment coverage"), LFFClothingRules::CanHideSlot(TEXT("hands"), Covered, Exposed));
    TestFalse(TEXT("The head is preserved"), LFFClothingRules::CanHideSlot(TEXT("head"), Covered, Exposed));
    TestFalse(TEXT("Mixed body/hands material cannot be guessed from its name"),
        LFFClothingRules::CanHideSlot(TEXT("body"), Covered, Exposed));
    TestFalse(TEXT("A removed garment restores uncovered sections"), LFFClothingRules::CanHideSlot(TEXT("torso"), {}, Exposed));
    TestFalse(TEXT("An unnamed section is never hidden"), LFFClothingRules::CanHideSlot(NAME_None, { NAME_None }, {}));
    const USkeletalMesh* FaceA = NewObject<USkeletalMesh>();
    const USkeletalMesh* FaceB = NewObject<USkeletalMesh>();
    TestTrue(TEXT("Hair cards require their actual fitted face"), LFFClothingRules::IsHairFitValid(FaceA, FaceA));
    TestFalse(TEXT("A different face must not inherit an incompatible groom"), LFFClothingRules::IsHairFitValid(FaceA, FaceB));
    TestFalse(TEXT("Unreviewed hair binding is not guessed"), LFFClothingRules::IsHairFitValid(nullptr, FaceA));
    TestFalse(TEXT("Missing heads cannot accept a hair binding"), LFFClothingRules::IsHairFitValid(FaceA, nullptr));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCirculationTest, "LFF.Crowd.Circulation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCirculationTest::RunTest(const FString& Parameters)
{
    const TArray<FVector> Route = { FVector(0, 0, 0), FVector(300, 0, 0), FVector(300, 400, 0) };
    TestEqual(TEXT("Length follows corners, not a shortcut through a wall"), LFFCirculationRules::RouteLength(Route), 700.0);
    const auto Corner = LFFCirculationRules::SampleRoute(Route, 500.0);
    TestTrue(TEXT("Outbound visitor remains on the authored segment"), Corner.bValid && Corner.Location.Equals(FVector(300, 200, 0)));
    const auto Return = LFFCirculationRules::SampleRoute(Route, 900.0);
    TestTrue(TEXT("Return retraces the route instead of teleporting"), Return.bValid && Return.Location.Equals(Corner.Location));
    TestTrue(TEXT("Return orientation follows travel"), Return.Direction.Equals(-Corner.Direction));
    TestTrue(TEXT("Complete loop returns to the start"), LFFCirculationRules::SampleRoute(Route, 1400.0).Location.IsNearlyZero());
    TestTrue(TEXT("Endpoint stays in bounds"), LFFCirculationRules::SampleRoute(Route, 700.0).Location.Equals(Route.Last()));
    TestFalse(TEXT("No route is safely stationary"), LFFCirculationRules::SampleRoute({}, 20.0).bValid);
    TestFalse(TEXT("Coincident endpoints are safely stationary"),
        LFFCirculationRules::SampleRoute({ FVector::ZeroVector, FVector::ZeroVector }, 20.0).bValid);
    TestTrue(TEXT("Zero length segments do not break usable paths"),
        LFFCirculationRules::SampleRoute({ FVector::ZeroVector, FVector::ZeroVector, FVector(100, 0, 0) }, 50.0).Location.Equals(FVector(50, 0, 0)));
    TestTrue(TEXT("Visible near visitors animate"), LFFCirculationRules::ShouldAnimate(true, true, true));
    TestFalse(TEXT("Crowd off / Calm stops movement and posing"), LFFCirculationRules::ShouldAnimate(false, true, true));
    TestFalse(TEXT("Reduced motion freezes visible visitors"), LFFCirculationRules::ShouldAnimate(true, false, true));
    TestFalse(TEXT("Distant visitors do no skeletal work"), LFFCirculationRules::ShouldAnimate(true, true, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdLifecycleTest, "LFF.Crowd.PersonLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdLifecycleTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* BodyAsset = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/MetaHumanCrowdContent/Characters/Assembled/AverageMale_Head01/Body/SKM_MHC_AverageMale_Head01_BodyMesh"));
    USkeletalMesh* FaceAsset = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/MetaHumanCrowdContent/Characters/Assembled/AverageMale_Head01/Face/SKM_MHC_AverageMale_Head01_FaceMesh"));
    UChaosClothAssetBase* Garment = LoadObject<UChaosClothAssetBase>(nullptr,
        TEXT("/MetaHumanCrowdContent/Outfits/Tops/OA_TshirtLngSlv/TshirtLngSlv/ClothAssets/CA_TshirtLngSlv_m_med_nrw"));
    UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Stand_Idle_Loop"));
    UAnimSequence* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/MetaHumanCrowdContent/Animations/AS_MH_Neutral_Walk_Loop_F"));
    if (!TestNotNull(TEXT("Installed body fixture"), BodyAsset) || !TestNotNull(TEXT("Installed face fixture"), FaceAsset)
        || !TestNotNull(TEXT("Installed fitted garment fixture"), Garment)
        || !TestNotNull(TEXT("Installed idle fixture"), Idle) || !TestNotNull(TEXT("Installed walk fixture"), Walk))
    {
        return false;
    }
    if (!TestTrue(TEXT("Body fixture has material slots"), BodyAsset->GetMaterials().Num() > 0)
        || !TestTrue(TEXT("Face fixture has separate skin and dental slots"), FaceAsset->GetMaterials().Num() > 1))
    {
        return false;
    }
    const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    ALFFCrowdFanActor* Fan = World->SpawnActor<ALFFCrowdFanActor>();
    if (!TestNotNull(TEXT("Crowd actor spawned"), Fan))
    {
        World->DestroyWorld(false);
        return false;
    }
    Fan->BodyMesh = BodyAsset;
    Fan->FaceMesh = FaceAsset;
    // This isolated component-contract fixture is not a hairstyle or a saved scene asset.
    Fan->HairStyle.CardMesh = BodyAsset;
    Fan->HairStyle.FittedFaceMesh = FaceAsset;
    Fan->StillAnimation = Idle;
    Fan->MovingAnimation = Walk;
    Fan->WalkRoute = { FVector::ZeroVector, FVector(500, 0, 0) };
    Fan->WalkSpeed = 80.0f;
    FLFFOutfitPiece Piece;
    Piece.Cloth = Garment;
    Piece.FittedBodyMesh = BodyAsset;
    Fan->Outfit = { Piece };
    const FName SkinSlot = BodyAsset->GetMaterials()[0].MaterialSlotName;
    Fan->BodyMaterialOverrides.Add(SkinSlot, UMaterial::GetDefaultMaterial(MD_Surface));
    Fan->FaceMaterialOverrides.Add(FaceAsset->GetMaterials()[0].MaterialSlotName, UMaterial::GetDefaultMaterial(MD_Surface));
    Fan->OnConstruction(FTransform::Identity);
    Fan->DispatchBeginPlay();
    Fan->SetCrowdActive(true, true);
    USkeletalMeshComponent* Hair = nullptr;
    USkeletalMeshComponent* VisibleFace = nullptr;
    Fan->ForEachComponent<USkeletalMeshComponent>(false, [&Hair, &VisibleFace](USkeletalMeshComponent* Component)
    {
        if (Component->ComponentHasTag(TEXT("LFF_Hair")))
        {
            Hair = Component;
        }
        if (Component->GetFName() == TEXT("Face"))
        {
            VisibleFace = Component;
        }
    });
    TestTrue(TEXT("A complete outfit applies its lower-neck mask"),
        VisibleFace != nullptr && VisibleFace->GetMaterial(0) == UMaterial::GetDefaultMaterial(MD_Surface));
    TestTrue(TEXT("Face coverage preserves non-skin materials"),
        VisibleFace != nullptr && VisibleFace->GetMaterial(1) == FaceAsset->GetMaterials()[1].MaterialInterface);
    TestTrue(TEXT("Fitted hair component retains its geometry"), Hair != nullptr && Hair->GetSkeletalMeshAsset() == BodyAsset);
    TestTrue(TEXT("Hair uses unit-scale head-following bone mapping"), Hair != nullptr
        && Hair->GetRelativeTransform().Equals(FTransform::Identity) && Hair->LeaderPoseComponent.IsValid());
    if (Hair)
    {
        TestEqual(TEXT("Nearby fitted hair cards may use their real high-detail LOD"), Hair->ComputeMinLOD(), 0);
    }
    TestTrue(TEXT("An active measured route ticks"), Fan->IsActorTickEnabled());
    const FVector Before = Fan->GetActorLocation();
    Fan->Tick(0.1f);
    TestTrue(TEXT("Walking advances on the authored route"), Fan->GetActorLocation().X > Before.X);
    TInlineComponentArray<UChaosClothComponent*> Garments(Fan);
    TestEqual(TEXT("Construction is idempotent: exactly one garment"), Garments.Num(), 1);
    if (Garments.Num() == 1)
    {
        TestTrue(TEXT("Fitted cloth always has unit scale"), Garments[0]->GetRelativeScale3D().Equals(FVector::OneVector));
        TestEqual(TEXT("Nearby cuffs/collars may use their fitted high-detail LOD"), Garments[0]->ComputeMinLOD(), 0);
    }
    Fan->SetCrowdActive(true, false);
    const FVector Frozen = Fan->GetActorLocation();
    Fan->Tick(0.1f);
    TestFalse(TEXT("Reduced motion disables actor tick"), Fan->IsActorTickEnabled());
    TestTrue(TEXT("Reduced motion leaves a stationary visible person"), !Fan->IsHidden() && Fan->GetActorLocation().Equals(Frozen));
    TInlineComponentArray<USkinnedMeshComponent*> Meshes(Fan);
    for (USkinnedMeshComponent* Mesh : Meshes)
    {
        TestFalse(TEXT("Reduced motion pauses every body/face/cloth/pose/hair component"), Mesh->IsComponentTickEnabled());
    }
    Fan->SetCrowdActive(false, true);
    TestTrue(TEXT("Crowd Off and Calm hide the visitor"), Fan->IsHidden());
    TestFalse(TEXT("Crowd Off stops route ticking"), Fan->IsActorTickEnabled());
    Fan->SetCrowdActive(true, true);
    TestTrue(TEXT("Crowd resumes without respawning the actor"), !Fan->IsHidden() && Fan->IsActorTickEnabled());
    Fan->Outfit.Reset();
    Fan->OnConstruction(FTransform::Identity);
    Garments.Reset();
    Fan->GetComponents(Garments);
    TestEqual(TEXT("Removing an outfit removes its transient components"), Garments.Num(), 0);
    USkeletalMeshComponent* VisibleBody = nullptr;
    Fan->ForEachComponent<USkeletalMeshComponent>(false, [&VisibleBody](USkeletalMeshComponent* Component)
    {
        if (Component->GetFName() == TEXT("Body"))
        {
            VisibleBody = Component;
        }
    });
    TestTrue(TEXT("Removing the outfit restores unmasked skin"),
        VisibleBody != nullptr && VisibleBody->GetMaterial(0) == BodyAsset->GetMaterials()[0].MaterialInterface);
    TestTrue(TEXT("Removing the outfit restores the unmasked face and neck"),
        VisibleFace != nullptr && VisibleFace->GetMaterial(0) == FaceAsset->GetMaterials()[0].MaterialInterface);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdRulesTest, "LFF.Crowd.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdRulesTest::RunTest(const FString& Parameters)
{
    const FTransform Camera(FRotator(-6.0f, 0.0f, 0.0f), FVector(0.0, 0.0, 270.0));
    TestTrue(TEXT("Only the visitor's immediate space is cleared"),
        LFFCrowdRules::IsCameraSpace(FVector(20.0, 0.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("The next row remains populated"), LFFCrowdRules::IsCameraSpace(FVector(92.0, 0.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("No large empty cone in front"), LFFCrowdRules::IsCameraSpace(FVector(500.0, 20.0, 0.0), Camera, 50.0f, 0.0f));
    TestFalse(TEXT("Seats beside the visitor remain populated"), LFFCrowdRules::IsCameraSpace(FVector(0.0, 62.0, 0.0), Camera, 50.0f, 0.0f));
    const LFFCrowdRules::FCounts Counts = LFFCrowdRules::RepresentationCounts(48, 1000, 10000, 0.7f, 300);
    TestEqual(TEXT("Detailed fans are bounded"), Counts.Detailed, 34);
    TestEqual(TEXT("Mass fans respect the cap"), Counts.Mass, 300);
    TestEqual(TEXT("Capped near fans are not silently discarded"), Counts.Overflow, 400);
    TestEqual(TEXT("Overflow keeps its occupied seat as a billboard"), Counts.Far, 7400);
    const auto Empty = LFFCrowdRules::RepresentationCounts(48, 1000, 10000, 0.0f, 300);
    TestEqual(TEXT("Zero occupancy makes every representation empty"), Empty.Detailed + Empty.Mass + Empty.Far, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCirculationDataTest, "LFF.Crowd.CirculationData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCirculationDataTest::RunTest(const FString& Parameters)
{
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!TestTrue(TEXT("Staged circulation authoring data reads"),
        FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("LFF/Crowd/circulation.json")))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Circulation data version"), Root->GetIntegerField(TEXT("version")), 1);
    TestEqual(TEXT("Circulation is scoped to the saved stadium map"), Root->GetStringField(TEXT("map")), FString(TEXT("/Game/LFF/Levels/L_LFF_Stadium")));
    int32 Walking = 0, Waiting = 0;
    TSet<FString> Identifiers;
    for (const FString& Kind : { FString(TEXT("walking")), FString(TEXT("waiting")) })
    {
        for (const auto& Value : Root->GetArrayField(Kind))
        {
            const auto& Route = Value->AsObject();
            const FString Id = Route->GetStringField(TEXT("id"));
            TestFalse(TEXT("Route ids are unique for idempotent actor labels"), Identifiers.Contains(Id));
            Identifiers.Add(Id);
            const int32 Count = Route->GetIntegerField(TEXT("count"));
            TestTrue(TEXT("Each population is bounded"), Count > 0 && Count <= 8);
            (Kind == TEXT("walking") ? Walking : Waiting) += Count;
        }
    }
    TestTrue(TEXT("Walking additions respect the hard skeletal budget"), Walking <= 24);
    TestTrue(TEXT("Waiting additions respect the hard skeletal budget"), Waiting <= 24);
    TestTrue(TEXT("Minimum visitor spacing preserves navigation"), Root->GetNumberField(TEXT("minimum_spacing_cm")) >= 125.0);
    TestNotNull(TEXT("Walking clip is staged and loadable"),
        LoadObject<UAnimSequence>(nullptr, *Root->GetStringField(TEXT("walk_clip"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLFFCrowdTreadsTest, "LFF.Crowd.MeasuredTreads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLFFCrowdTreadsTest::RunTest(const FString& Parameters)
{
    FString SeatText, TreadText;
    TArray<uint8> Bytes;
    TSharedPtr<FJsonObject> Seats, Treads;
    const FString Folder = FPaths::ProjectContentDir() / TEXT("LFF/Crowd");
    if (!TestTrue(TEXT("Seat geometry and measured treads read"),
        FFileHelper::LoadFileToString(SeatText, *(Folder / TEXT("seats.json")))
        && FFileHelper::LoadFileToString(TreadText, *(Folder / TEXT("seat_treads.json")))
        && FFileHelper::LoadFileToArray(Bytes, *(Folder / TEXT("seats.json")))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SeatText), Seats)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TreadText), Treads)))
    {
        return false;
    }
    TestTrue(TEXT("Sidecar matches the exact seat file"),
        Treads->GetStringField(TEXT("seats_sha1")).Equals(FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString(), ESearchCase::IgnoreCase));
    const auto& Chairs = Seats->GetArrayField(TEXT("seats"));
    const auto& Floors = Treads->GetArrayField(TEXT("tread_z_cm"));
    if (!TestEqual(TEXT("One tread row per source seat"), Floors.Num(), Chairs.Num()))
    {
        return false;
    }
    int32 Measured = 0;
    for (int32 Index = 0; Index < Floors.Num(); ++Index)
    {
        const auto& Floor = Floors[Index]->AsArray();
        if (Floor[0]->IsNull() || Floor[1]->IsNull())
        {
            continue;
        }
        const double ChairZ = Chairs[Index]->AsArray()[2]->AsNumber();
        for (const auto& Z : Floor)
        {
            TestTrue(TEXT("Floor is measured below the chair, within trace depth"), Z->AsNumber() < ChairZ && ChairZ - Z->AsNumber() <= 151.0);
        }
        ++Measured;
    }
    TestTrue(TEXT("At least 95 percent have measured floors"), Measured >= Chairs.Num() * 0.95);
    ULFFSeatSpawnDataGenerator* Generator = NewObject<ULFFSeatSpawnDataGenerator>();
    TestEqual(TEXT("Runtime validates and consumes all measured seats"), Generator->GetSeatCount(), Measured);
    return true;
}

#endif
