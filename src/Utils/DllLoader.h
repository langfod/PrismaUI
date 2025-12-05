#pragma once

#include <Windows.h>
#include <filesystem>
#include <vector>
#include <string>

namespace PrismaUI::Utils
{
    class DllLoader
    {
    public:
        static DllLoader& GetSingleton()
        {
            static DllLoader instance;
            return instance;
        }

        // Load Ultralight DLLs from the specified path
        // Must be called before any Ultralight API usage
        bool LoadUltralightLibraries()
        {
            if (m_loaded) {
                return true;
            }

            auto libsPath = std::filesystem::current_path() / "Data" / "PrismaUI" / "libs";

            if (!std::filesystem::exists(libsPath)) {
                logger::error("Ultralight libs path does not exist: {}", libsPath.string());
                return false;
            }

            // Load order matters due to dependencies:
            // UltralightCore -> WebCore -> Ultralight -> AppCore
            const std::vector<std::wstring> dllNames = {
                L"UltralightCore.dll",
                L"WebCore.dll",
                L"Ultralight.dll",
                L"AppCore.dll"
            };

            for (const auto& dllName : dllNames) {
                auto dllPath = libsPath / dllName;
                
                if (!std::filesystem::exists(dllPath)) {
                    logger::error("DLL not found: {}", dllPath.string());
                    UnloadAll();
                    return false;
                }

                HMODULE handle = LoadLibraryW(dllPath.c_str());
                
                if (!handle) {
                    DWORD error = GetLastError();
                    logger::error("Failed to load DLL: {} (Error: {})", dllPath.string(), error);
                    UnloadAll();
                    return false;
                }

                m_loadedModules.push_back(handle);
                logger::info("Loaded Ultralight DLL: {}", dllPath.filename().string());
            }

            m_loaded = true;
            logger::info("All Ultralight DLLs loaded successfully from: {}", libsPath.string());
            return true;
        }

        // Unload all loaded DLLs (in reverse order)
        void UnloadAll()
        {
            for (auto it = m_loadedModules.rbegin(); it != m_loadedModules.rend(); ++it) {
                if (*it) {
                    FreeLibrary(*it);
                }
            }
            m_loadedModules.clear();
            m_loaded = false;
        }

        bool IsLoaded() const { return m_loaded; }

    private:
        DllLoader() = default;
        ~DllLoader() { UnloadAll(); }

        DllLoader(const DllLoader&) = delete;
        DllLoader& operator=(const DllLoader&) = delete;

        std::vector<HMODULE> m_loadedModules;
        bool m_loaded = false;
    };
}
