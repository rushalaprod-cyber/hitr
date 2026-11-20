cmake_minimum_required(VERSION 3.22)
# macOS : binaire universel (Apple Silicon + Intel)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(hitr VERSION 1.2.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE : dossier local (-DJUCE_PATH=... ou external/JUCE-8.0.12), sinon téléchargement automatique
set(JUCE_PATH "" CACHE PATH "Dossier JUCE local (optionnel)")
if(NOT JUCE_PATH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12/CMakeLists.txt")
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE-8.0.12")
endif()
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.12.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(HITR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HITR_FORMATS AU)
endif()

juce_add_plugin(hitr
    COMPANY_NAME "hitr"
    BUNDLE_ID "com.hitr.hitr"
    PLUGIN_MANUFACTURER_CODE Hitr
    PLUGIN_CODE Hitr
    FORMATS ${HITR_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "h.i.t.r"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Modulation
    ICON_BIG assets/icon.png
    ICON_SMALL assets/icon.png
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Logo : le corbeau de ravenn
juce_add_binary_data(hitrAssets SOURCES assets/raven.png)

juce_generate_juce_header(hitr)

target_sources(hitr PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp)

target_compile_definitions(hitr PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(hitr PRIVATE /utf-8)
endif()

target_link_libraries(hitr
    PRIVATE
        hitrAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test des 50 substances hors DAW
juce_add_console_app(hitrTest PRODUCT_NAME "hitrTest")
target_sources(hitrTest PRIVATE Tests/ModeTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PresetManager.cpp)
juce_generate_juce_header(hitrTest)
target_compile_definitions(hitrTest PRIVATE JucePlugin_Name="h.i.t.r" JucePlugin_VersionString="${PROJECT_VERSION}" JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    target_compile_options(hitrTest PRIVATE /utf-8)
endif()
target_link_libraries(hitrTest PRIVATE hitrAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
