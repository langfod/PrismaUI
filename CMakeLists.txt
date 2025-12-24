cmake_minimum_required(VERSION 4.1)

project(
    PrismaUI
    VERSION 1.1.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build root for external builds
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build")
# Include compiler flags configuration
include(CompilerFlags)
include(ExternalDependencies)


# Ultralight SDK paths
set(ULTRALIGHT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/ultralight")
set(ULTRALIGHT_INCLUDE_DIR "${ULTRALIGHT_SDK_ROOT}/include")
set(ULTRALIGHT_BINARY_DIR "${ULTRALIGHT_SDK_ROOT}/bin")
set(ULTRALIGHT_LIBRARY_DIR "${ULTRALIGHT_SDK_ROOT}/lib")
set(ULTRALIGHT_RESOURCES_DIR "${ULTRALIGHT_SDK_ROOT}/resources")
set(ULTRALIGHT_INSPECTOR_DIR "${ULTRALIGHT_SDK_ROOT}/inspector")

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

# Add the plugin using CommonLibSSE function
add_commonlibsse_plugin(${PROJECT_NAME}
    NAME "PrismaUI"
    AUTHOR "StarkMP <discord: starkmp>"
    VERSION ${PROJECT_VERSION}
    SOURCES ${SOURCES}
)

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
link_external_dependencies(${PROJECT_NAME})

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
    COMMENT "Copying Ultralight DLLs to output directory"
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
    
    COMMENT "Creating distribution folder: dist/PrismaUI_${PROJECT_VERSION}"
)

