#include "Inspector.h"

#include <filesystem>

#include "Utils/DllLoader.h"

namespace PrismaUI::Inspector {
    using namespace Core;

    namespace {
        std::once_flag gInspectorAssetCheckFlag;
        std::atomic<bool> gInspectorAssetsAvailable{false};
    }

    void EnsureInspectorAssetsAvailability() {
        const auto inspectorPath = Utils::GetBasePath() / "inspector" / "Main.html";
        std::call_once(gInspectorAssetCheckFlag, [inspectorPath]() {
            try {
                if (std::filesystem::exists(inspectorPath)) {
                    gInspectorAssetsAvailable.store(true);
                    logger::info("Ultralight inspector assets detected at {}", inspectorPath.string());
                } else {
                    logger::warn(
                        "Ultralight inspector assets were not found at {}. Inspector view will not render unless the "
                        "SDK inspector folder is copied next to the DLL.",
                        inspectorPath.string());
                }
            } catch (const std::exception& e) {
                logger::warn("Failed to verify Ultralight inspector asset directory at {}: {}", inspectorPath.string(),
                             e.what());
            }
        });
    }

    bool AreInspectorAssetsAvailable() {
        EnsureInspectorAssetsAvailability();
        return gInspectorAssetsAvailable.load();
    }

}  // namespace PrismaUI::Inspector
