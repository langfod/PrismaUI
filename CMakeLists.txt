cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Limit build parallelism (configurable via -DCMAKE_COMPILE_JOBS and -DCMAKE_LINK_JOBS)
if(NOT DEFINED CMAKE_COMPILE_JOBS)
    set(CMAKE_COMPILE_JOBS 8)
endif()

if(NOT DEFINED CMAKE_LINK_JOBS)
    set(CMAKE_LINK_JOBS 2)
endif()

set_property(GLOBAL PROPERTY JOB_POOLS "compile=${CMAKE_COMPILE_JOBS}" "link=${CMAKE_LINK_JOBS}")
set(CMAKE_JOB_POOL_COMPILE compile)
set(CMAKE_JOB_POOL_LINK link)
set(CMAKE_INSTALL_MESSAGE LAZY)

project(PrismaUI VERSION 1.5.0 LANGUAGES CXX)

include(CTest)
option(PRISMAUI_BUILD_PLATFORM_TESTS "Build standalone PrismaUI platform tests" ON)

# Set build root for external builds
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build")

# Include compiler flags configuration
include(CompilerFlags)
include(ExternalDependencies)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

# Collect source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Create source groups for IDE
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${SOURCES}
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_compile_definitions(${PROJECT_NAME} PRIVATE PRISMAUI_VERSION="${PROJECT_VERSION}")

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${ULTRALIGHT_INCLUDE_DIR}"
)

# Link directories for Ultralight
target_link_directories(${PROJECT_NAME}
    PRIVATE
    "${ULTRALIGHT_LIBRARY_DIR}"
)

# Link Ultralight libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    AppCore
    Ultralight
    UltralightCore
    WebCore
    comctl32  # Required for SetWindowSubclass/RemoveWindowSubclass
    imm32     # Required for IME (Input Method Editor) support
    shell32   # Required for SHGetKnownFolderPath (log directory)
    D3D11
    DXGI
    ole32
    version
    fmt::fmt
    spdlog::spdlog
    Microsoft::DirectXTK
)

# Delay-load Ultralight DLLs so they can be loaded from custom path at runtime
target_link_options(${PROJECT_NAME}
    PRIVATE
    "/DELAYLOAD:UltralightCore.dll"
    "/DELAYLOAD:WebCore.dll"
    "/DELAYLOAD:Ultralight.dll"
    "/DELAYLOAD:AppCore.dll"
)

# Required for delay loading
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    delayimp
)
# Precompiled header
target_precompile_headers(${PROJECT_NAME}
    PRIVATE
    "src/PCH.h"
)
if(PRISMAUI_BUILD_PLATFORM_TESTS)
    add_executable(PrismaUIPlatformTests
        tests/Platform/AddressLibraryTests.cpp
        src/Platform/AddressLibrary.cpp
        src/Platform/RuntimeContext.cpp
    )
    target_include_directories(PrismaUIPlatformTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(PrismaUIPlatformTests PRIVATE fmt::fmt spdlog::spdlog)
    target_compile_definitions(PrismaUIPlatformTests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        PRISMAUI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME PrismaUI.Platform COMMAND PrismaUIPlatformTests)
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Post-build: Copy Ultralight DLLs to output directory (for development)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ULTRALIGHT_BINARY_DIR}"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E echo "Copying Ultralight DLLs to output directory"
)

# Post-build: Create distribution folder structure
set(DIST_ROOT "${CMAKE_SOURCE_DIR}/dist")
set(DIST_VERSION_DIR "${DIST_ROOT}/PrismaUI_${PROJECT_VERSION}")
set(DIST_PRISMAUI_DIR "${DIST_VERSION_DIR}/PrismaUI")
set(DIST_SKSE_PLUGINS_DIR "${DIST_VERSION_DIR}/SKSE/plugins")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Create distribution directories
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_PRISMAUI_DIR}/libs"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_SKSE_PLUGINS_DIR}"
    
    # Copy assets folder contents to PrismaUI folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/" "${DIST_PRISMAUI_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_RESOURCES_DIR}" "${DIST_PRISMAUI_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_INSPECTOR_DIR}" "${DIST_PRISMAUI_DIR}/inspector"
    
    # Copy Ultralight DLLs to libs folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_BINARY_DIR}" "${DIST_PRISMAUI_DIR}/libs"
    
    # Copy PrismaUI DLL to SKSE/plugins
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_SKSE_PLUGINS_DIR}/"
    
    # Copy PrismaUI PDB to SKSE/plugins (if exists)
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:$<TARGET_PDB_FILE:${PROJECT_NAME}>>,copy,true>
        "$<$<BOOL:$<TARGET_PDB_FILE:${PROJECT_NAME}>>:$<TARGET_PDB_FILE:${PROJECT_NAME}>>"
        "${DIST_SKSE_PLUGINS_DIR}/"
    
    COMMAND "${CMAKE_COMMAND}" -E echo "Creating distribution folder: dist/PrismaUI_${PROJECT_VERSION}"
)

