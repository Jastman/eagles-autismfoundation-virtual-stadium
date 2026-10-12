#include "World/LFFHairAssetBuilder.h"

#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Item/MetaHumanCrowdGroomEditorPipeline.h"
#include "Item/MetaHumanCrowdGroomPipeline.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MetaHumanWardrobeItem.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
FString Json(const TSharedRef<FJsonObject>& Object)
{
    FString Result;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

bool GatherSource(UObject* Item, USkeletalMesh* Face, TArray<int32>& LODs, FString& Error)
{
    const UMetaHumanWardrobeItem* Wardrobe = Cast<UMetaHumanWardrobeItem>(Item);
    if (!Wardrobe || !Face || !Face->GetSkeleton() || !Face->GetMeshDescription(0)
        || Face->GetRefSkeleton().FindBoneIndex(TEXT("head")) == INDEX_NONE)
    {
        Error = TEXT("A real Crowd wardrobe item and imported target face with a head bone are required.");
        return false;
    }
    const UMetaHumanItemEditorPipeline* Pipeline = Wardrobe->GetEditorPipeline();
    const UClass* StockClass = FindObject<UClass>(nullptr,
        TEXT("/Script/MetaHumanCrowdEditor.MetaHumanCrowdGroomEditorPipeline"));
    if (!Pipeline || !StockClass || !Pipeline->IsA(StockClass))
    {
        Error = TEXT("Wardrobe item does not use the installed stock CrowdGroom editor pipeline.");
        return false;
    }
    for (const TCHAR* Name : { TEXT("CardsMaterial"), TEXT("HelmetsMaterial") })
    {
        const FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Pipeline->GetClass(), Name);
        if (!Property || !Property->GetPropertyValue_InContainer(Pipeline).LoadSynchronous())
        {
            Error = FString::Printf(TEXT("Missing stock groom material: %s"), Name);
            return false;
        }
    }
    const UGroomBindingAsset* Binding = Cast<UGroomBindingAsset>(Wardrobe->PrincipalAsset.LoadSynchronous());
    const UGroomAsset* Groom = Binding ? Binding->GetGroom() : nullptr;
    if (!Groom || !Binding->GetTargetSkeletalMesh())
    {
        Error = TEXT("The wardrobe's actual GroomBinding, GroomAsset or source head dependency is missing.");
        return false;
    }
    for (const FHairGroupsCardsSourceDescription& Cards : Groom->GetHairGroupsCards())
    {
        if (Cards.LODIndex < 0 || !Cards.GetMesh())
        {
            continue;
        }
        if (!Cards.GetMesh()->GetMeshDescription(0)
            || (Cards.Textures.Layout != EHairTextureLayout::Layout2 && Cards.Textures.Layout != EHairTextureLayout::Layout3)
            || Cards.Textures.Textures.Num() < 2
            || !Cards.Textures.Textures[0] || !Cards.Textures.Textures[1])
        {
            Error = TEXT("A real card LOD lacks imported geometry or its two stock compact-layout textures.");
            return false;
        }
        LODs.AddUnique(Cards.LODIndex);
    }
    LODs.Sort();
    if (LODs.IsEmpty())
    {
        Error = TEXT("Source has no real hair-card geometry. Strands/baked scalp/helmet-only sources are not substituted.");
        return false;
    }
    return true;
}

bool HasHeadWeights(const FMeshDescription& Description)
{
    FSkeletalMeshConstAttributes Attributes(Description);
    if (!Attributes.HasBones())
    {
        return false;
    }
    const auto Names = Attributes.GetBoneNames();
    const auto Weights = Attributes.GetVertexSkinWeights();
    if (!Names.IsValid() || !Weights.IsValid())
    {
        return false;
    }
    for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
    {
        const FVertexBoneWeightsConst VertexWeights = Weights.Get(Vertex);
        if (VertexWeights.Num() != 1)
        {
            return false;
        }
        const FBoneID Bone(VertexWeights[0].GetBoneIndex());
        if (!Attributes.Bones().IsValid(Bone) || Names.Get(Bone) != TEXT("head")
            || VertexWeights[0].GetWeight() < 0.999f)
        {
            return false;
        }
    }
    return Description.Vertices().Num() > 0 && Description.Triangles().Num() > 0;
}

void FinishMesh(USkeletalMesh* Mesh)
{
    TArray<USkinnedAsset*> Assets = { Mesh };
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
}
}
#endif

FString ULFFHairAssetBuilder::InspectSource(UObject* WardrobeItem, USkeletalMesh* TargetFace)
{
#if WITH_EDITOR
    TArray<int32> LODs;
    FString Error;
    const bool bValid = GatherSource(WardrobeItem, TargetFace, LODs, Error);
    const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("valid"), bValid);
    Report->SetStringField(TEXT("error"), Error);
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 LOD : LODs)
    {
        Values.Add(MakeShared<FJsonValueNumber>(LOD));
    }
    Report->SetArrayField(TEXT("card_lods"), Values);
    if (const UMetaHumanWardrobeItem* Wardrobe = Cast<UMetaHumanWardrobeItem>(WardrobeItem))
    {
        Report->SetStringField(TEXT("principal_asset"), Wardrobe->PrincipalAsset.ToString());
        if (const UGroomBindingAsset* Binding = Cast<UGroomBindingAsset>(Wardrobe->PrincipalAsset.LoadSynchronous()))
        {
            Report->SetStringField(TEXT("binding"), Binding->GetPathName());
            Report->SetStringField(TEXT("groom"), Binding->GetGroom() ? Binding->GetGroom()->GetPathName() : TEXT(""));
            Report->SetStringField(TEXT("source_head"), Binding->GetTargetSkeletalMesh()
                ? Binding->GetTargetSkeletalMesh()->GetPathName() : TEXT(""));
        }
    }
    return Json(Report);
#else
    return TEXT("{\"valid\":false,\"error\":\"Hair fitting is editor-only.\"}");
#endif
}

USkeletalMesh* ULFFHairAssetBuilder::BuildFittedCards(UObject* WardrobeItem, USkeletalMesh* TargetFace,
    const FString& AssetPackage, FString& Error)
{
    Error.Reset();
#if WITH_EDITOR
    FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("MetaHumanCrowdEditor"));
    TArray<int32> SourceLODs;
    if (!GatherSource(WardrobeItem, TargetFace, SourceLODs, Error))
    {
        return nullptr;
    }
    if (!AssetPackage.StartsWith(TEXT("/Game/LFF/People/Hair/"))
        || !FPackageName::IsValidLongPackageName(AssetPackage)
        || FPackageName::DoesPackageExist(AssetPackage))
    {
        Error = TEXT("Destination must be a new owned /Game/LFF/People/Hair package; existing assets are never overwritten.");
        return nullptr;
    }
    UMetaHumanWardrobeItem* Wardrobe = DuplicateObject(CastChecked<UMetaHumanWardrobeItem>(WardrobeItem), GetTransientPackage());
    UMetaHumanItemEditorPipeline* Pipeline = const_cast<UMetaHumanItemEditorPipeline*>(Wardrobe->GetEditorPipeline());
    FStructProperty* Conversion = FindFProperty<FStructProperty>(Pipeline->GetClass(), TEXT("CardConversionParams"));
    FIntProperty* TextureLOD = FindFProperty<FIntProperty>(Pipeline->GetClass(), TEXT("GroomTextureMinLOD"));
    UScriptStruct* InputType = FindObject<UScriptStruct>(nullptr,
        TEXT("/Script/MetaHumanCrowdEditor.MetaHumanCrowdGroomBuildInput"));
    UScriptStruct* OutputType = FindObject<UScriptStruct>(nullptr,
        TEXT("/Script/MetaHumanCrowd.MetaHumanCrowdGroomBuildOutput"));
    if (!Conversion || Conversion->Struct->GetStructureSize() != sizeof(FGroomCardConversionParams)
        || !TextureLOD || !InputType || InputType->GetStructureSize() != sizeof(FMetaHumanCrowdGroomBuildInput)
        || !OutputType || OutputType->GetStructureSize() != sizeof(FMetaHumanCrowdGroomBuildOutput))
    {
        Error = TEXT("Installed CrowdGroom schema differs from this editor adapter.");
        return nullptr;
    }
    auto* ConversionParams = Conversion->ContainerPtrToValuePtr<FGroomCardConversionParams>(Pipeline);
    ConversionParams->SkinWeightMethod = EMetaHumanCrowdGroomSkinWeightCopyMethod::SingleBone;
    ConversionParams->TargetBoneName = TEXT("head");
    TextureLOD->SetPropertyValue_InContainer(Pipeline, INT32_MAX);

    // Crowd payload types are not DLL-exported. Initialize their registered schemas, then use the checked native layout.
    FInstancedStruct Input;
    Input.InitializeAs(InputType);
    auto* GroomInput = reinterpret_cast<FMetaHumanCrowdGroomBuildInput*>(Input.GetMutableMemory());
    const FMetaHumanPaletteItemKey TargetKey = FMetaHumanPaletteItemKey::FromPrincipalAsset(
        TSoftObjectPtr<UObject>(TargetFace), NAME_None);
    GroomInput->FitTargets.Add(TargetKey).TargetMesh = TargetFace;
    GroomInput->ActorFaceLODs = SourceLODs;
    GroomInput->PipelineSlotName = TEXT("Hair");
    const FMetaHumanPaletteItemPath ItemPath(FMetaHumanPaletteItemKey::FromExternalWardrobeItem(
        TSoftObjectPtr<UMetaHumanWardrobeItem>(CastChecked<UMetaHumanWardrobeItem>(WardrobeItem)), NAME_None));
    UPackage* Package = CreatePackage(*AssetPackage);
    UMetaHumanItemEditorPipeline::FBuildItemParams Params { ItemPath, Wardrobe,
        EMetaHumanCharacterPaletteBuildQuality::Production, Package };
    Params.BuildInput = MoveTemp(Input);
    Params.BuildCacheGuid = FGuid(0x7E772B15, 0x66184B91, 0xB8733888, 0x5A9FD138);
    FMetaHumanPaletteBuiltData Built;
    Pipeline->BuildItemSynchronous(Params, Built);
    const FMetaHumanPipelineBuiltData* ItemData = Built.ItemBuiltData.View().Find(ItemPath);
    if (!ItemData || ItemData->BuildOutput.GetScriptStruct() != OutputType)
    {
        Error = TEXT("Stock CrowdGroom fitting produced no compatible output.");
        return nullptr;
    }
    const auto* Output = reinterpret_cast<const FMetaHumanCrowdGroomBuildOutput*>(ItemData->BuildOutput.GetMemory());
    const FMetaHumanCrowdMeshGeometryBundle* Bundle = Output->ItemToGroomGeometryMap.Find(TargetKey);
    if (!Bundle || Bundle->Materials.IsEmpty())
    {
        Error = TEXT("Stock CrowdGroom fitting produced no real target geometry/materials.");
        return nullptr;
    }
    for (int32 LOD : SourceLODs)
    {
        if (!Bundle->MeshDescriptions.IsValidIndex(LOD) || !HasHeadWeights(Bundle->MeshDescriptions[LOD])
            || !Bundle->LODMaterialMaps.IsValidIndex(LOD))
        {
            Error = FString::Printf(TEXT("Fitted card LOD %d is absent or not genuinely head-weighted."), LOD);
            return nullptr;
        }
    }
    USkeletalMesh* Mesh = NewObject<USkeletalMesh>(Package, *FPackageName::GetShortName(AssetPackage),
        RF_Public | RF_Standalone | RF_Transactional);
    Mesh->SetSkeleton(TargetFace->GetSkeleton());
    Mesh->SetRefSkeleton(Bundle->RefSkeleton);
    Mesh->CalculateInvRefMatrices();
    Mesh->SetMaterials(Bundle->Materials);
    for (int32 Index = 0; Index < SourceLODs.Num(); ++Index)
    {
        const int32 SourceLOD = SourceLODs[Index];
        FMeshDescription Description = Bundle->MeshDescriptions[SourceLOD];
        auto SlotNames = FStaticMeshAttributes(Description).GetPolygonGroupMaterialSlotNames();
        for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
        {
            const TArray<int32>& Map = Bundle->LODMaterialMaps[SourceLOD];
            if (!Map.IsValidIndex(Group.GetValue()) || !Bundle->Materials.IsValidIndex(Map[Group.GetValue()]))
            {
                Error = TEXT("Stock groom polygon-group material remapping is invalid.");
                return nullptr;
            }
            SlotNames.Set(Group, Bundle->Materials[Map[Group.GetValue()]].ImportedMaterialSlotName);
        }
        FSkeletalMeshLODInfo& Info = Mesh->AddLODInfo();
        Mesh->GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel());
        Info.BuildSettings.bRecomputeNormals = false;
        Info.BuildSettings.bRecomputeTangents = false;
        Info.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
        Info.ReductionSettings.NumOfVertPercentage = 1.0f;
        Info.ScreenSize.Default = Index == 0 ? 1.0f : 0.025f / FMath::Pow(2.5f, Index - 1);
        Info.LODHysteresis = 0.002f;
        Mesh->CreateMeshDescription(Index, MoveTemp(Description));
        Mesh->CommitMeshDescription(Index);
    }
    Mesh->Build();
    FinishMesh(Mesh);
    for (int32 Index = 1; Index < SourceLODs.Num(); ++Index)
    {
        FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(Index);
        const FSkeletalMeshLODModel& Model = Mesh->GetImportedModel()->LODModels[Index];
        const TArray<int32>& Map = Bundle->LODMaterialMaps[SourceLODs[Index]];
        for (const FSkelMeshSection& Section : Model.Sections)
        {
            if (!Map.IsValidIndex(Section.MaterialIndex) || !Mesh->GetMaterials().IsValidIndex(Map[Section.MaterialIndex]))
            {
                Error = TEXT("Compiled card section does not match the stock material map.");
                return nullptr;
            }
            Info->LODMaterialMap.Add(Map[Section.MaterialIndex]);
        }
    }
    Mesh->Build();
    FinishMesh(Mesh);
    Mesh->CalculateExtendedBounds();
    Mesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Mesh);
    return Mesh;
#else
    Error = TEXT("Hair fitting is editor-only; packaged visitors use saved fitted skeletal card assets.");
    return nullptr;
#endif
}

FString ULFFHairAssetBuilder::InspectCards(USkeletalMesh* Cards, USkeletalMesh* TargetFace)
{
#if WITH_EDITOR
    const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    bool bValid = Cards && TargetFace && Cards->GetSkeleton() == TargetFace->GetSkeleton()
        && Cards->GetLODNum() > 0 && Cards->GetRefSkeleton().FindBoneIndex(TEXT("head")) != INDEX_NONE;
    TArray<TSharedPtr<FJsonValue>> LODs;
    if (Cards)
    {
        FinishMesh(Cards);
        const int32 Head = Cards->GetRefSkeleton().FindBoneIndex(TEXT("head"));
        const int32 TargetHead = TargetFace ? TargetFace->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
        const bool bHeadMatches = Head != INDEX_NONE && TargetHead != INDEX_NONE
            && Cards->GetRefSkeleton().GetRefBonePose()[Head].Equals(
                TargetFace->GetRefSkeleton().GetRefBonePose()[TargetHead], 1e-6);
        bValid &= bHeadMatches && Cards->GetImportedBounds().SphereRadius > 0.0;
        Report->SetBoolField(TEXT("head_bind_pose_matches"), bHeadMatches);
        const FBoxSphereBounds Bounds = Cards->GetImportedBounds();
        TArray<TSharedPtr<FJsonValue>> Center, Extent;
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            Center.Add(MakeShared<FJsonValueNumber>(Bounds.Origin[Axis]));
            Extent.Add(MakeShared<FJsonValueNumber>(Bounds.BoxExtent[Axis]));
        }
        Report->SetArrayField(TEXT("bounds_center_cm"), Center);
        Report->SetArrayField(TEXT("bounds_half_extent_cm"), Extent);
        const auto& Materials = Cards->GetMaterials();
        for (const FSkeletalMaterial& Material : Materials)
        {
            bValid &= Material.MaterialInterface != nullptr;
        }
        for (int32 Index = 0; Index < Cards->GetLODNum(); ++Index)
        {
            const FMeshDescription* Description = Cards->GetMeshDescription(Index);
            const bool bWeighted = Description && HasHeadWeights(*Description);
            const FSkeletalMeshLODModel& Model = Cards->GetImportedModel()->LODModels[Index];
            const FSkeletalMeshLODInfo* Info = Cards->GetLODInfo(Index);
            bool bMaterialValid = !Model.Sections.IsEmpty();
            for (int32 SectionIndex = 0; SectionIndex < Model.Sections.Num(); ++SectionIndex)
            {
                int32 Material = Model.Sections[SectionIndex].MaterialIndex;
                if (Info && Info->LODMaterialMap.IsValidIndex(SectionIndex))
                {
                    Material = Info->LODMaterialMap[SectionIndex];
                }
                bMaterialValid &= Materials.IsValidIndex(Material) && Materials[Material].MaterialInterface != nullptr;
            }
            bValid &= bWeighted && bMaterialValid;
            const TSharedRef<FJsonObject> LOD = MakeShared<FJsonObject>();
            LOD->SetNumberField(TEXT("vertices"), Description ? Description->Vertices().Num() : 0);
            LOD->SetNumberField(TEXT("triangles"), Description ? Description->Triangles().Num() : 0);
            LOD->SetBoolField(TEXT("all_vertices_follow_head"), bWeighted);
            LOD->SetBoolField(TEXT("render_materials_valid"), bMaterialValid);
            LOD->SetNumberField(TEXT("screen_size"), Info ? Info->ScreenSize.Default : 0);
            LODs.Add(MakeShared<FJsonValueObject>(LOD));
        }
        Report->SetStringField(TEXT("mesh"), Cards->GetPathName());
        Report->SetStringField(TEXT("target_face"), TargetFace ? TargetFace->GetPathName() : TEXT(""));
        Report->SetNumberField(TEXT("material_count"), Materials.Num());
    }
    Report->SetBoolField(TEXT("valid"), bValid);
    Report->SetArrayField(TEXT("lods"), LODs);
    return Json(Report);
#else
    return TEXT("{\"valid\":false,\"error\":\"Card authoring evidence is editor-only.\"}");
#endif
}
