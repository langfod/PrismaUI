#include "API/API.h"
#include "Platform/Logging.h"
#include "Platform/Runtime.h"
#include "Platform/SKSEHost.h"
#include "PrismaUI_API.h"
#include "Utils/DllLoader.h"

extern "C" DLLEXPORT bool __cdecl
SKSEPlugin_Load(const PrismaUI::Platform::SKSEHost::LoadInterface *a_skse) {

  if (!PrismaUI::Platform::SKSEHost::Initialize(a_skse)) {
    OutputDebugStringA("PrismaUI: failed to initialize raw SKSE interfaces\n");
    return false;
  }

  std::string runtimeError;
  if (!PrismaUI::Platform::InitializeRuntime(PrismaUI::Platform::SKSEHost::RuntimeVersion(), runtimeError)) {
    OutputDebugStringA(("PrismaUI: " + runtimeError + "\n").c_str());
    MessageBoxA(nullptr, runtimeError.c_str(), "PrismaUI could not load", MB_OK | MB_ICONERROR);
    return false;
  }
  if (!PrismaUI::Platform::Logging::Initialize(PrismaUI::Platform::GetRuntimeContext().Family(), runtimeError)) {
    OutputDebugStringA(("PrismaUI: " + runtimeError + "\n").c_str());
    return false;
  }

  logger::debug("---------------- PrismaUI {} by {} ----------------", PRISMAUI_VERSION,
               "StarkMP <discord: starkmp>");
  logger::debug("-------------------- Docs and Guides: https://prismaui.dev -------------------");
  logger::debug("------------------- Running on Skyrim v{} -------------------",
               PrismaUI::Platform::GetRuntimeContext().Version().String());
  logger::debug("Validated Address Library and ABI profile for Skyrim v{}",
               PrismaUI::Platform::GetRuntimeContext().Version().String());

  // Load Ultralight DLLs from Data/PrismaUI/libs before any Ultralight API usage
  if (!PrismaUI::Utils::DllLoader::GetSingleton().LoadUltralightLibraries()) {
    logger::critical("Failed to load Ultralight libraries! Plugin will not load.");
    return false;
  }

  return true;
}

extern "C" DLLEXPORT void *__cdecl
RequestPluginAPI(const PRISMA_UI_API::InterfaceVersion a_interfaceVersion) {
  auto api = PluginAPI::PrismaUIInterface::GetSingleton();

  switch (a_interfaceVersion) {
  case PRISMA_UI_API::InterfaceVersion::V1:
    logger::info("RequestPluginAPI returned V1 interface");
    return static_cast<PRISMA_UI_API::IVPrismaUI1*>(api);
  case PRISMA_UI_API::InterfaceVersion::V2:
    logger::info("RequestPluginAPI returned V2 interface");
    return static_cast<PRISMA_UI_API::IVPrismaUI2*>(api);
  default:
    logger::info("RequestPluginAPI requested unsupported interface version");
    return nullptr;
  }
}
