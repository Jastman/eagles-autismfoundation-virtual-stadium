#include "VirtualStadium.h"

#include "Cesium/LFFRetryingAssetAccessor.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogVirtualStadium);

void FVirtualStadiumModule::StartupModule()
{
    FDefaultGameModuleImpl::StartupModule();
    // Must run before L_LFF_Stadium's tileset is created; the game module loads before the startup map.
    FModuleManager::Get().LoadModuleChecked(TEXT("CesiumRuntime"));
    FLFFRetryingAssetAccessor::Install();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FVirtualStadiumModule, VirtualStadium, "VirtualStadium");
