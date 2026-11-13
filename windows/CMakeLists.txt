# Leviathan for Windows (JUCE). Also builds on macOS, which is how it is tested alongside the Swift Mac app.
#
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# ASIO (optional, Windows): pass -DLEVIATHAN_ASIO_SDK=<path to the Steinberg ASIO SDK folder>.
cmake_minimum_required(VERSION 3.22)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LEVIATHAN_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")   # re-read on version bumps
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

# Neural Amp Modeler core (MIT) with Eigen (MPL-2.0), pinned; compiled with Leviathan's bridge (shared/nam)
FetchContent_Declare(nam
  GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
  GIT_TAG 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842   # v0.6.0
  GIT_SUBMODULES "Dependencies/eigen"
  SOURCE_SUBDIR _none)   # sources only: Leviathan builds the library itself, not NAM's own tools
FetchContent_MakeAvailable(nam)
file(GLOB NAM_SOURCES "${nam_SOURCE_DIR}/NAM/*.cpp" "${nam_SOURCE_DIR}/NAM/*/*.cpp")
# OBJECT, not STATIC: NAM registers its model types with static objects nothing references, which a linker
# drops from a static library (every capture would then fail to load).
add_library(levnam OBJECT ${NAM_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam/lev_nam.cpp")
target_include_directories(levnam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam"
  PRIVATE "${nam_SOURCE_DIR}" "${nam_SOURCE_DIR}/Dependencies/nlohmann")
target_include_directories(levnam SYSTEM PRIVATE "${nam_SOURCE_DIR}/Dependencies/eigen")
target_compile_definitions(levnam PUBLIC NAM_SAMPLE_FLOAT PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(levnam PRIVATE /bigobj /W0 /O2)
else()
  target_compile_options(levnam PRIVATE -w -O3)
endif()

# Windows: NAM twice, picked at run time (see nam/dispatch.cpp). MSVC targets SSE2 by default, which leaves NAM at
# about twice its cost on any CPU with AVX2 and FMA; /arch:AVX2 for everything would crash CPUs without them.
#  - levnam (above, SSE2) is linked into the app with its lev_nam_* functions renamed to levnam_sse2_*;
#  - levnam_avx2.dll is a second copy built with /arch:AVX2, shipped next to Leviathan.exe;
#  - levnam_dispatch defines the lev_nam_* the app calls and forwards them to one or the other.
if(MSVC)
  set(LEV_NAM_FUNCS load free reset process expected_rate has_loudness loudness measure_level measure_level_in
                    polarity has_input_level input_level)
  set(LEV_NAM_DEF "EXPORTS\n")
  foreach(f ${LEV_NAM_FUNCS})
    target_compile_definitions(levnam PRIVATE "lev_nam_${f}=levnam_sse2_${f}")
    string(APPEND LEV_NAM_DEF "  lev_nam_${f}\n")
  endforeach()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/levnam_avx2.def" "${LEV_NAM_DEF}")

  add_library(levnam_dispatch STATIC "${CMAKE_CURRENT_SOURCE_DIR}/nam/dispatch.cpp")
  target_include_directories(levnam_dispatch PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam")
  target_compile_options(levnam_dispatch PRIVATE /O2)
  target_link_libraries(levnam INTERFACE levnam_dispatch)

  add_library(levnam_avx2 SHARED ${NAM_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam/lev_nam.cpp"
                                 "${CMAKE_CURRENT_BINARY_DIR}/levnam_avx2.def")
  target_include_directories(levnam_avx2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam" "${nam_SOURCE_DIR}"
                                                 "${nam_SOURCE_DIR}/Dependencies/nlohmann")
  target_include_directories(levnam_avx2 SYSTEM PRIVATE "${nam_SOURCE_DIR}/Dependencies/eigen")
  target_compile_definitions(levnam_avx2 PRIVATE NAM_SAMPLE_FLOAT NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(levnam_avx2 PRIVATE /bigobj /W0 /O2 /arch:AVX2)
endif()

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

# bundled NAM captures next to the app (the installer ships this folder)
add_custom_command(TARGET Leviathan POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../shared/nam/captures"
          "$<IF:$<BOOL:${APPLE}>,$<TARGET_BUNDLE_CONTENT_DIR:Leviathan>/Resources/NAM,$<TARGET_FILE_DIR:Leviathan>/NAM>")
if(MSVC)   # the AVX2 build of NAM, next to the exe (the installer ships it)
  add_dependencies(Leviathan levnam_avx2)
  add_custom_command(TARGET Leviathan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:levnam_avx2>" "$<TARGET_FILE_DIR:Leviathan>")
endif()

# control explanations, shared with the Mac app
juce_add_binary_data(LeviathanData SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../shared/help/explanations.json"
  # Pitch > Play as: bands and their tunings
  "${CMAKE_CURRENT_SOURCE_DIR}/../shared/pitch/bands.json"
  # Inter, the app's typeface (OFL; shared with the Mac app)
  "${CMAKE_CURRENT_SOURCE_DIR}/../shared/fonts/Inter-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/../shared/fonts/Inter-SemiBold.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/../shared/fonts/Inter-Bold.ttf")

target_link_libraries(Leviathan PRIVATE
  LeviathanData
  levnam
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

# Fast math only for the audio engine (it vectorises the cab convolution). Not app-wide: file parsing relies on
# std::isfinite to reject malformed numbers, which fast math is allowed to optimise away.
if(MSVC)
  set_source_files_properties(Source/Dsp.cpp PROPERTIES COMPILE_OPTIONS "/fp:fast")
else()
  set_source_files_properties(Source/Dsp.cpp PROPERTIES COMPILE_OPTIONS "-ffast-math")
  target_compile_options(Leviathan PRIVATE -Wno-shadow -Wno-float-equal)
endif()

# Offline engine check: renders test signals through the C++ engine (compared against the Swift engine).
add_executable(dsp_render tools/dsp_render.cpp Source/Dsp.cpp)
target_include_directories(dsp_render PRIVATE Source)
target_link_libraries(dsp_render PRIVATE levnam)
if(NOT MSVC)
  target_compile_options(dsp_render PRIVATE -O2)
endif()

# Preset check: renders a DI take through every factory preset and reports loudness (level-matching new presets).
juce_add_console_app(preset_check PRODUCT_NAME "preset_check")
juce_generate_juce_header(preset_check)
target_sources(preset_check PRIVATE tools/preset_check.cpp Source/Dsp.cpp Source/Settings.cpp Source/Library.cpp)
target_include_directories(preset_check PRIVATE Source)
target_compile_definitions(preset_check PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 LEVIATHAN_VERSION="${LEVIATHAN_VERSION}")
target_link_libraries(preset_check PRIVATE levnam juce::juce_audio_formats juce::juce_gui_basics juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

