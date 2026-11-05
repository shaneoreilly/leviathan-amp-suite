# Leviathan for Windows (JUCE). Also builds on macOS, which is how it is tested alongside the Swift Mac app.
#
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# ASIO (optional, Windows): pass -DLEVIATHAN_ASIO_SDK=<path to the Steinberg ASIO SDK folder>.
cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LEVIATHAN_VERSION)
string(STRIP "${LEVIATHAN_VERSION}" LEVIATHAN_VERSION)
project(Leviathan VERSION ${LEVIATHAN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5            # pinned: includes the malformed WAV / JSON input hardening
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(Leviathan
  PRODUCT_NAME "Leviathan"
  COMPANY_NAME "Leviathan"
  BUNDLE_ID "local.leviathan.ampsuite.juce"
  VERSION ${LEVIATHAN_VERSION}
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../docs/icon.png"
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Leviathan needs your audio interface input to process your guitar.")

juce_generate_juce_header(Leviathan)

file(GLOB_RECURSE LEVIATHAN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
target_sources(Leviathan PRIVATE ${LEVIATHAN_SOURCES})
target_include_directories(Leviathan PRIVATE Source)

set(LEVIATHAN_ASIO_SDK "" CACHE PATH "Steinberg ASIO SDK folder (Windows, optional)")
set(LEVIATHAN_ASIO 0)
if(WIN32 AND LEVIATHAN_ASIO_SDK)
  target_include_directories(Leviathan PRIVATE "${LEVIATHAN_ASIO_SDK}/common")
  set(LEVIATHAN_ASIO 1)
endif()

target_compile_definitions(Leviathan PRIVATE
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Leviathan,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Leviathan,JUCE_VERSION>"
  LEVIATHAN_VERSION="${LEVIATHAN_VERSION}"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_ASIO=${LEVIATHAN_ASIO}
  JUCE_WASAPI=1
  JUCE_DIRECTSOUND=1
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Leviathan PRIVATE
  juce::juce_gui_extra
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_dsp
  juce::juce_cryptography
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)

if(MSVC)
  target_compile_options(Leviathan PRIVATE /fp:fast)
else()
  target_compile_options(Leviathan PRIVATE -ffast-math -Wno-shadow -Wno-float-equal)
endif()

# Offline engine check: renders test signals through the C++ engine (compared against the Swift engine).
add_executable(dsp_render tools/dsp_render.cpp Source/Dsp.cpp)
target_include_directories(dsp_render PRIVATE Source)
if(NOT MSVC)
  target_compile_options(dsp_render PRIVATE -O2)
endif()
