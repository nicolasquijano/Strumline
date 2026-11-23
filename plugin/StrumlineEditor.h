#pragma once
// The interface of Strumline (drawn first as a mockup, see docs/PLAN_GUITAR_MIDI.md): mode, latency and activity on top, the input level, the six strings, a piano
// roll of the last twelve seconds with a lane for the pitch bend, and three panels of controls (detection, output, monitor synth).
#include <memory>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "StrumlineProcessor.h"

// The dark and gold look of Schematic Amp Sim: knobs with a ring that shows the value.
class StrumlineLook final : public juce::LookAndFeel_V4 {
public:
    StrumlineLook();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

// A row of buttons bound to a choice parameter (one lit), or a single button bound to a bool parameter (a switch).
class Segmented final : public juce::Component, public juce::SettableTooltipClient {
public:
    Segmented(juce::AudioProcessorValueTreeState&, const char* parameterId, juce::StringArray labels, bool toggle = false);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::StringArray labels_;
    bool toggle_;
    int selected_ = 0;
};

class StrumlineEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit StrumlineEditor(StrumlineProcessor&);
    ~StrumlineEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Takes the notes the processor made since the last call and brings the drawing up to `nowSeconds` (the timer calls it with the clock; a test can
    // call it with its own time).
    void updateFromProcessor(double nowSeconds);

    static constexpr int kWidth = 680, kHeight = 570;

private:
    struct Note { int midi; double start, end; int velocity; };  // end < 0: still sounding
    struct Bend { double at; int value; };

    void timerCallback() override;
    void drawHeader(juce::Graphics&);
    static void drawLogo(juce::Graphics&, juce::Rectangle<float> area);
    void drawLevel(juce::Graphics&);
    void drawStrings(juce::Graphics&);
    void drawRoll(juce::Graphics&);
    void paintStatic(juce::Graphics&);
    void applyPreset(int index);

    juce::Rectangle<int> rollArea() const { return {14, 146, kWidth - 28, 190}; }
    juce::Rectangle<int> panelArea(int index) const;
    void placeKnob(juce::Slider&, juce::Rectangle<int> panel, int slot, int slots);
    void addKnob(juce::Slider&, const char* parameterId, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>&);

    StrumlineProcessor& p_;
    StrumlineLook look_;
    juce::Slider sens_, lookahead_, velocity_, channel_, transpose_, level_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAtt_, lookAtt_, velAtt_, chanAtt_, transAtt_, levelAtt_;
    Segmented mode_, bend_, wave_, thru_, synth_, perf_;
    juce::ComboBox preset_;
    juce::TextButton panic_{"PANIC"};
    juce::TooltipWindow tooltips_{this, 500};
    juce::TextButton test_{"TEST"};

    juce::Image bg_;  // the parts that never change, drawn once
    std::vector<Note> notes_;
    std::vector<Bend> bends_;
    float lowShown_ = 45.0f, highShown_ = 69.0f;  // the range of the roll, eased towards what was played
    double loadAt_ = 0.0, loadBusy_ = 0.0;
    float loadShown_ = 0.0f;
    double now_ = 0.0, lastEvent_ = -10.0;
    float levelShown_ = -120.0f;
};
