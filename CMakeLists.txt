cmake_minimum_required(VERSION 3.22)
project(PrismReactor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PRISM_FORMATS VST3 AU Standalone)
else()
    set(PRISM_FORMATS VST3 Standalone)
endif()

juce_add_plugin(PrismReactor
    COMPANY_NAME "PrismLab"
    PLUGIN_MANUFACTURER_CODE Prsm
    PLUGIN_CODE Prx1
    FORMATS ${PRISM_FORMATS}
    PRODUCT_NAME "Prism Reactor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Prism Reactor necesita la entrada de audio para reaccionar a la música."
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Analyzer)

target_sources(PrismReactor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AutomationEngine.cpp
    Source/AutomationPanel.cpp
    Source/TransportBar.cpp)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(PrismReactor PRIVATE Source/VideoExport.mm)
    set_source_files_properties(Source/VideoExport.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(PrismReactor PRIVATE "-framework AVFoundation" "-framework CoreMedia" "-framework CoreVideo")
else()
    target_sources(PrismReactor PRIVATE Source/VideoExport.cpp)
endif()

target_compile_definitions(PrismReactor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PrismReactor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
