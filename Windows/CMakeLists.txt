cmake_minimum_required(VERSION 3.22)
project(DigitaktCompanionWindows VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(CompanionDSP STATIC ../Audio/AudioDSP.c)
target_include_directories(CompanionDSP PUBLIC ../Audio)
if(MSVC)
    target_compile_options(CompanionDSP PRIVATE /experimental:c11atomics)
else()
    target_link_libraries(CompanionDSP PUBLIC m)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(DSPTests Tests/DSPTests.cpp)
    target_link_libraries(DSPTests PRIVATE CompanionDSP)
    add_test(NAME DSPAudioAndMidi COMMAND DSPTests)
endif()

option(COMPANION_DSP_ONLY "Build shared DSP and tests without JUCE" OFF)
if(COMPANION_DSP_ONLY)
    return()
endif()

set(JUCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE missing. Run build.ps1 or set JUCE_SOURCE_DIR. See README.md.")
endif()
add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/clap-juce-extensions EXCLUDE_FROM_ALL)
juce_add_plugin(CompanionPlugin
    PRODUCT_NAME "DTX Companion"
    COMPANY_NAME "DTX Companion"
    BUNDLE_ID "org.digitaktcompanion.synth"
    PLUGIN_MANUFACTURER_CODE Dtxc
    PLUGIN_CODE Dtx2
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
    NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(CompanionPlugin PRIVATE Source/Processor.cpp Source/PixelEditor.cpp Source/ClapHost.cpp Source/PluginRack.cpp Source/HardwareMidi.cpp Source/HostedEditorWindow.cpp Source/ModulationCurve.cpp Source/SessionRecorder.cpp)
target_include_directories(CompanionPlugin PUBLIC Source "${JUCE_SOURCE_DIR}/modules" third_party/clap-juce-extensions/clap-libs/clap/include)
target_compile_definitions(CompanionPlugin PUBLIC
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_ASIO=1)
target_link_libraries(CompanionPlugin PRIVATE CompanionDSP juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
clap_juce_extensions_plugin(TARGET CompanionPlugin CLAP_ID "org.digitaktcompanion.synth"
    CLAP_FEATURES instrument synthesizer CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 1)

juce_add_gui_app(DigitaktCompanion
    PRODUCT_NAME "Digitakt Companion"
    COMPANY_NAME "Digitakt Companion"
    BUNDLE_ID "org.digitaktcompanion.windows"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)
target_sources(DigitaktCompanion PRIVATE
    Source/Main.cpp Source/MainComponent.cpp Source/MainComponent.h)
target_compile_definitions(DigitaktCompanion PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ASIO=1)
target_link_libraries(DigitaktCompanion PRIVATE CompanionPlugin CompanionDSP
    juce::juce_recommended_config_flags)
# JUCE 8's RC generator omits its Info.txt dependency. Keep Windows file-version
# resources current when upgrading an existing build directory.
if(WIN32)
    foreach(target CompanionPlugin DigitaktCompanion)
        get_target_property(generated_dir ${target} JUCE_GENERATED_SOURCES_DIRECTORY)
        get_target_property(info_file ${target} JUCE_INFO_FILE)
        add_custom_command(OUTPUT "${generated_dir}/${target}_resources.rc" APPEND DEPENDS "${info_file}")
    endforeach()
endif()
if(MSVC)
    target_compile_options(DigitaktCompanion PRIVATE /utf-8 /W4)
endif()

if(BUILD_TESTING)
    juce_add_console_app(PatchTests PRODUCT_NAME "Companion Patch Tests")
    target_sources(PatchTests PRIVATE Tests/PatchTests.cpp)
    target_include_directories(PatchTests PRIVATE Source)
    target_compile_definitions(PatchTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PatchTests PRIVATE CompanionDSP juce::juce_core juce::juce_recommended_config_flags)
    add_test(NAME PatchStateRoundTripAndValidation COMMAND PatchTests "${CMAKE_CURRENT_SOURCE_DIR}/Patches/Init.json")
    add_library(ClapTestEffect MODULE Tests/ClapTestEffect.cpp)
    target_include_directories(ClapTestEffect PRIVATE third_party/clap-juce-extensions/clap-libs/clap/include)
    set_target_properties(ClapTestEffect PROPERTIES PREFIX "" SUFFIX ".clap")
    juce_add_console_app(PluginTests PRODUCT_NAME "DTX Plugin Integration Tests")
    target_sources(PluginTests PRIVATE Tests/PluginTests.cpp)
    target_link_libraries(PluginTests PRIVATE CompanionPlugin CompanionDSP)
    add_dependencies(PluginTests CompanionPlugin_CLAP CompanionPlugin_VST3 ClapTestEffect)
    add_test(NAME PluginFormatsHostingStateAndPixelUI COMMAND PluginTests
        "$<TARGET_FILE:CompanionPlugin_CLAP>" "$<TARGET_FILE_DIR:CompanionPlugin_VST3>/../.."
        "$<TARGET_FILE:ClapTestEffect>" "${CMAKE_CURRENT_SOURCE_DIR}/previews")
    set_tests_properties(PluginFormatsHostingStateAndPixelUI PROPERTIES TIMEOUT 120)
    juce_add_console_app(RecorderTests PRODUCT_NAME "DTX Recorder and Rate Tests")
    target_sources(RecorderTests PRIVATE Tests/RecorderTests.cpp)
    target_link_libraries(RecorderTests PRIVATE CompanionPlugin CompanionDSP)
    add_test(NAME RecorderRatesMixerAndMidi COMMAND RecorderTests "${CMAKE_CURRENT_SOURCE_DIR}/build/recorder-checks")
    set_tests_properties(RecorderRatesMixerAndMidi PROPERTIES TIMEOUT 60)
endif()

# Avoid compiling every JUCE amalgamation at once on memory-limited laptops.
if(MSVC)
    foreach(target CompanionPlugin DigitaktCompanion PluginTests RecorderTests)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE /MP2 /utf-8)
        endif()
    endforeach()
endif()
