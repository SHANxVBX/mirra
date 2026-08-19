cmake_minimum_required(VERSION 3.28)
project(CastingCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(SDL3 CONFIG REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswscale
    )
endif()

if(NOT TARGET PkgConfig::FFMPEG)
    find_package(FFMPEG REQUIRED)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# ── Core Library Target ───────────────────────────────────────────────────────
set(CORE_LIB_SOURCES
    src/adb/AdbManager.cpp
    src/net/AdbSocketClient.cpp
    src/audio/AudioPlayer.cpp
    src/decoder/H264Decoder.cpp
    src/renderer/SdlRenderer.cpp
    src/ipc/PipeServer.cpp
    src/ipc/MessageFramer.cpp
    src/session/SessionStateMachine.cpp
    src/diag/DiagLogger.cpp
)

set(CORE_LIB_HEADERS
    src/adb/AdbManager.h
    src/net/AdbSocketClient.h
    src/decoder/H264Decoder.h
    src/renderer/SdlRenderer.h
    src/ipc/IPipeServer.h
    src/ipc/PipeServer.h
    src/ipc/MessageFramer.h
    src/ipc/IpcMessages.h
    src/session/SessionStateMachine.h
    src/session/SessionState.h
    src/diag/DiagLogger.h
)

add_library(CastingCoreLib STATIC ${CORE_LIB_SOURCES} ${CORE_LIB_HEADERS})
target_include_directories(CastingCoreLib PUBLIC src)
target_link_libraries(CastingCoreLib PUBLIC
    SDL3::SDL3
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(CastingCoreLib PUBLIC PkgConfig::FFMPEG)
elseif(TARGET FFMPEG::FFMPEG)
    target_link_libraries(CastingCoreLib PUBLIC FFMPEG::FFMPEG)
else()
    target_link_libraries(CastingCoreLib PUBLIC
        FFMPEG::avcodec
        FFMPEG::avformat
        FFMPEG::avutil
        FFMPEG::swscale
    )
endif()

if(WIN32)
    target_link_libraries(CastingCoreLib PUBLIC ws2_32 Dwmapi)
    target_compile_definitions(CastingCoreLib PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ── Main Executable ───────────────────────────────────────────────────────────
add_executable(CastingCore src/main.cpp)
target_link_libraries(CastingCore PRIVATE CastingCoreLib)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

set(TEST_SOURCES
    tests/test_message_framer.cpp
    tests/test_session_state_machine.cpp
    tests/test_adb_parser.cpp
)

add_executable(CastingCore.Tests ${TEST_SOURCES})
target_include_directories(CastingCore.Tests PRIVATE src)
target_link_libraries(CastingCore.Tests PRIVATE
    CastingCoreLib
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(CastingCore.Tests)

add_executable(IpcIntegrationTestServer tests/IpcIntegrationTestServer.cpp)
target_include_directories(IpcIntegrationTestServer PRIVATE src)
target_link_libraries(IpcIntegrationTestServer PRIVATE CastingCoreLib)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS CastingCore DESTINATION bin)
