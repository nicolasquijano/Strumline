#pragma once
// The interface of Strumline: a flat dark design with one gold accent (docs/mockups: option A with the arc knobs and the soft glow of B).
// It is drawn on a fixed 1000 x 640 canvas that the editor scales to whatever size the window has (the window keeps its proportions).
//   top:    logo, mode, thru and test, presets; the latency of the note and of the chord, the load of the chord model, the voices
//   middle: the notes that sound now with a tuner and the input level; a piano roll of the last 12 seconds with the pitch bend lane
//   bottom: three panels of controls (detection, output, monitor synth)
#include <memory>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "StrumlineProcessor.h"

class StrumlineLook final : public juce::LookAndFeel_V4 {
public:
    StrumlineLook();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle,
                          juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(12.5f)); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(13.0f)); }
    void drawCornerResizer(juce::Graphics&, int width, int height, bool mouseOver, bool dragging) override;
};

// A row of buttons bound to a choice parameter (one lit), or a single button bound to a bool parameter (a switch).
// Primary: a dark pill with the chosen segment filled in gold (the mode). Quiet: separate buttons, the chosen one outlined in gold.
class Segmented final : public juce::Component, public juce::SettableTooltipClient {
public:
    enum class Style { Primary, Quiet };
    Segmented(juce::AudioProcessorValueTreeState&, const char* parameterId, juce::StringArray labels, bool toggle = false, Style style = Style::Quiet);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void setLabels(juce::StringArray labels) {
        labels_ = std::move(labels);
        repaint();
    }

private:
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::StringArray labels_;
    bool toggle_;
    Style style_;
    int selected_ = 0;
};

// The whole interface on its 1000 x 640 canvas.
class StrumlineView final : public juce::Component, private juce::Timer {
public:
    explicit StrumlineView(StrumlineProcessor&);
    ~StrumlineView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    // Takes the notes the processor made since the last call and brings the drawing up to `nowSeconds`.
    void update(double nowSeconds);
    StrumlineLook& look() { return look_; }

    static constexpr int kWidth = 1000, kHeight = 640;

private:
    struct Note { int midi; double start, end; int velocity; };  // end < 0: still sounding
    struct Bend { double at; int value; };

    void timerCallback() override;
    void applyTexts();
    void applyPreset(int index);
    void addKnob(juce::Slider&, const char* parameterId, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>&);
    void addBar(juce::Slider&, const char* parameterId, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>&);

    static void drawLogo(juce::Graphics&, juce::Rectangle<float> area);
    void drawHeader(juce::Graphics&);
    void drawHero(juce::Graphics&, juce::Rectangle<int> area);
    void drawRoll(juce::Graphics&, juce::Rectangle<int> area);
    void drawControls(juce::Graphics&);
    void field(juce::Graphics&, juce::Slider&, const juce::String& label);

    juce::Rectangle<int> heroArea() const { return {24, 84, 240, 296}; }
    juce::Rectangle<int> rollArea() const { return {280, 84, 696, 296}; }
    juce::Rectangle<int> detectionArea() const { return {24, 396, 362, 220}; }
    juce::Rectangle<int> outputArea() const { return {402, 396, 279, 220}; }
    juce::Rectangle<int> monitorArea() const { return {697, 396, 279, 220}; }

    StrumlineProcessor& p_;
    StrumlineLook look_;
    juce::Slider sens_, lookahead_, channel_, transpose_, level_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAtt_, lookAtt_, chanAtt_, transAtt_, levelAtt_;
    Segmented mode_, velocity_, perf_, bend_, wave_, thru_, synth_;
    juce::ComboBox preset_, language_;
    juce::TextButton test_{"TEST"}, panic_{"PANIC"};
    juce::TooltipWindow tooltips_{this, 500};

    std::vector<Note> notes_;
    std::vector<Bend> bends_;
    float lowShown_ = 45.0f, highShown_ = 69.0f;  // the range of the roll, eased towards what was played
    double now_ = 0.0, lastEvent_ = -10.0, loadAt_ = 0.0, loadBusy_ = 0.0;
    float levelShown_ = -120.0f, loadShown_ = 0.0f;
};

// The window: scales the canvas to its size and keeps the proportions.
class StrumlineEditor final : public juce::AudioProcessorEditor {
public:
    explicit StrumlineEditor(StrumlineProcessor&);
    ~StrumlineEditor() override { setLookAndFeel(nullptr); }
    void resized() override;
    void paint(juce::Graphics&) override {}
    // Brings the drawing up to `nowSeconds` (the view's timer calls it with the clock; a test can call it with its own time).
    void updateFromProcessor(double nowSeconds) { view_.update(nowSeconds); }

    static constexpr int kWidth = StrumlineView::kWidth, kHeight = StrumlineView::kHeight;

private:
    StrumlineView view_;
};
