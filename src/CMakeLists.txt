# Vessel plugin targets (VST3 + CLAP + Standalone; AU added automatically on macOS).

# Embedded font (SIL OFL 1.1, see resources/fonts/OFL-Barlow.txt), so the UI
# renders identically on every OS instead of using each system's default.
juce_add_binary_data(VesselAssets
    NAMESPACE VesselAssets
    SOURCES
        ${PROJECT_SOURCE_DIR}/resources/fonts/BarlowSemiCondensed-Medium.ttf
        ${PROJECT_SOURCE_DIR}/resources/fonts/BarlowSemiCondensed-SemiBold.ttf)
set_target_properties(VesselAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(VESSEL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VESSEL_FORMATS AU)
endif()

juce_add_plugin(Vessel
    PRODUCT_NAME             "Vessel"
    COMPANY_NAME             "LowKei Audio"
    BUNDLE_ID                "com.lowkeiaudio.vessel"
    PLUGIN_MANUFACTURER_CODE Lwka          # 4 chars, at least one uppercase
    PLUGIN_CODE              Vssl          # unique per plugin; never change once shipped
    FORMATS                  ${VESSEL_FORMATS}
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          Fx Modulation Delay Reverb
    AU_MAIN_TYPE             kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD  FALSE)        # CI and local builds never touch system folders

# CLAP wrapper (FL Studio, Bitwig, Reaper). The ID is stored in every host
# project that uses the plugin, so never change it once shipped.
if(VESSEL_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET Vessel
        CLAP_ID       "com.lowkeiaudio.vessel"
        CLAP_FEATURES audio-effect multi-effects filter delay reverb chorus distortion)
endif()

# Shared with the headless checker (tools/Check.cpp).
set(VESSEL_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presets/FactoryPresets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presets/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/VesselLookAndFeel.cpp
    PARENT_SCOPE)

target_sources(Vessel PRIVATE
    Parameters.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    presets/FactoryPresets.cpp
    presets/PresetManager.cpp
    ui/VesselLookAndFeel.cpp)

target_compile_definitions(Vessel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Vessel
    PRIVATE
        vessel_dsp
        VesselAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
