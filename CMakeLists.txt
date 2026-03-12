cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_definitions(-D_WIN32_WINNT=0x0A00)

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

# Set build root for external builds
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build")

# Include compiler flags configuration
include(CompilerFlags)
include(ExternalDependencies)

# Collect source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Separate SIMD implementation files for custom compilation flags
set(SIMD_SSE2_SOURCES "src/Utils/SIMDImplementations_SSE2.cpp")
set(SIMD_AVX_SOURCES "src/Utils/SIMDImplementations_AVX.cpp")
set(SIMD_AVX2_SOURCES "src/Utils/SIMDImplementations_AVX2.cpp")

# Remove SIMD files from main source list (we'll add them back with custom flags)
list(REMOVE_ITEM SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/${SIMD_SSE2_SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${SIMD_AVX_SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${SIMD_AVX2_SOURCES}"
)

# Create source groups for IDE
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "Source Files"
    FILES ${SOURCES}
)

# Add the plugin using CommonLibSSE function
add_commonlibsse_plugin(${PROJECT_NAME}
    NAME "PrismaUI"
    AUTHOR "StarkMP <discord: starkmp>"
    EMAIL ""
    USE_ADDRESS_LIBRARY
    MINIMUM_SKSE_VERSION "2.0.1.04"
    VERSION ${PROJECT_VERSION}
    SOURCES ${SOURCES}
)

# Add SIMD implementation files with custom compilation flags
target_sources(${PROJECT_NAME} PRIVATE ${SIMD_SSE2_SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${SIMD_AVX_SOURCES})
target_sources(${PROJECT_NAME} PRIVATE ${SIMD_AVX2_SOURCES})

# Set SSE2 flags (baseline, but explicit for clarity)
set_source_files_properties(${SIMD_SSE2_SOURCES} PROPERTIES
    COMPILE_FLAGS "/arch:SSE2"
)

# Set AVX flags
set_source_files_properties(${SIMD_AVX_SOURCES} PROPERTIES
    COMPILE_FLAGS "/arch:AVX"
)

# Set AVX2 flags
set_source_files_properties(${SIMD_AVX2_SOURCES} PROPERTIES
    COMPILE_FLAGS "/arch:AVX2"
)

message(STATUS "SIMD implementations will be compiled with runtime dispatch:")
message(STATUS "  SSE2: ${SIMD_SSE2_SOURCES}")
message(STATUS "  AVX:  ${SIMD_AVX_SOURCES}")
message(STATUS "  AVX2: ${SIMD_AVX2_SOURCES}")

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
"${ULTRALIGHT_INCLUDE_DIR}"
    "${ULTRALIGHT_SDK_ROOT}/shaders/generated/headers"
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
)

# Find and link cpuinfo for runtime SIMD detection
find_package(cpuinfo CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cpuinfo::cpuinfo)

# Find and link ANGLE for WebGL support (OpenGL ES 3.0 -> D3D11 translation)
find_package(unofficial-angle CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::angle::libEGL
    unofficial::angle::libGLESv2
)

# Link WAMR for WebAssembly support (interpreter mode, fetched via cmake/wamr.cmake)
add_wamr_dependencies(${PROJECT_NAME})

# Link miniaudio for audio decoding (compiled library, fetched via cmake/miniaudio.cmake)
add_miniaudio_dependencies(${PROJECT_NAME})

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
add_external_dependencies(${PROJECT_NAME})

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

# ESP plugin serialization
#include(Spriggit) # on hold
