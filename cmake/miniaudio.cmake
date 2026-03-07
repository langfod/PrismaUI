# miniaudio.cmake
# Fetches and configures miniaudio as a compiled static library for PrismaUI

include(FetchContent)

FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio
    GIT_TAG        0.11.25
    GIT_SHALLOW    TRUE
)

# Only the decoder is used (ma_decoder_*); no playback, engine, or graph.
set(MINIAUDIO_NO_DEVICE_IO        ON  CACHE BOOL "" FORCE)  # No audio output; Skyrim/XAudio2 handles playback
set(MINIAUDIO_NO_RESOURCE_MANAGER ON  CACHE BOOL "" FORCE)  # Buffers managed manually
set(MINIAUDIO_NO_NODE_GRAPH       ON  CACHE BOOL "" FORCE)  # Not used
set(MINIAUDIO_NO_ENGINE           ON  CACHE BOOL "" FORCE)  # Not used
set(MINIAUDIO_NO_GENERATION       ON  CACHE BOOL "" FORCE)  # No waveform synthesis
set(MINIAUDIO_NO_EXTRA_NODES      ON  CACHE BOOL "" FORCE)  # Extra nodes require node graph
set(MINIAUDIO_BUILD_TESTS         OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_EXAMPLES      OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL             OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(miniaudio)

# Helper function to add miniaudio to a target
function(add_miniaudio_dependencies target_name)
    target_link_libraries(${target_name} PRIVATE miniaudio)
endfunction()
