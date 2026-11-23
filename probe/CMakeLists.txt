# SAS MIDI Probe: a throw-away VST3 effect for phase 0 of docs/PLAN_GUITAR_MIDI.md. It passes the audio through and plays a C major arpeggio as MIDI
# (a note every half second), so you can see whether a host routes the MIDI that an audio effect emits to a synth.
#   build.cmd target SasMidiProbe_VST3    (the bundle ends up in build/probe/SasMidiProbe_artefacts/Release/VST3)
juce_add_plugin(SasMidiProbe
  VERSION 0.0.1
  COMPANY_NAME "Nico"
  PLUGIN_MANUFACTURER_CODE Nico
  PLUGIN_CODE Mprb
  FORMATS VST3
  PRODUCT_NAME "SAS MIDI Probe"
  PLUGIN_NAME "SAS MIDI Probe"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx)
target_sources(SasMidiProbe PRIVATE ProbeProcessor.cpp)
target_compile_definitions(SasMidiProbe PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(SasMidiProbe PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
