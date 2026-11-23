#include "StrumlineEditor.h"

#include "I18n.h"

#include <algorithm>
#include <cmath>

// The look: soft relief on a charcoal ground (raised buttons and knobs, sunken wells), with a single gold accent.
namespace {
const juce::Colour kBack(0xff2a2b2e), kWell(0xff1f2023), kWellDeep(0xff17181a), kGold(0xffe0b24a), kGoldHi(0xfff0c868), kGoldLo(0xffb98a2c), kText(0xffe8e6e0),
    kLabel(0xff9b9a94), kBorder(0xff3d3e42), kGreen(0xff5ad26e), kKnob(0xff2f3033), kTrack(0xff1b1c1e), kSurfHi(0xff3c3d41), kSurfLo(0xff2e2f33),
    kOnText(0xff1a1608);

juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }

void caption(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(kLabel);
    g.setFont(font(10.5f, true));
    g.drawText(text, r, j);
}

// A raised surface: a dark shadow below and to the right, a light edge above and to the left, a gentle gradient.
void raised(juce::Graphics& g, juce::Rectangle<float> r, float radius) {
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.fillRoundedRectangle(r.translated(1.5f, 2.2f), radius);
    g.setColour(juce::Colour(0x1effffff));
    g.fillRoundedRectangle(r.translated(-0.8f, -0.8f), radius);
    g.setGradientFill(juce::ColourGradient(kSurfHi, r.getX(), r.getY(), kSurfLo, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
}

// A sunken well: darker than the ground, shadow along the top and left edges, a light line along the bottom.
void sunken(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill = kWell) {
    g.setColour(juce::Colour(0x14ffffff));
    g.fillRoundedRectangle(r.translated(0.0f, 1.2f), radius);
    g.setColour(fill);
    g.fillRoundedRectangle(r, radius);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(r.reduced(0.6f), radius, 1.2f);
}
}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------- look

StrumlineLook::StrumlineLook() {
    setColour(juce::Slider::textBoxTextColourId, kText);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, kSurfLo);
    setColour(juce::TextButton::buttonOnColourId, kGold);
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd0cec7));
    setColour(juce::TextButton::textColourOnId, kOnText);
    setColour(juce::ComboBox::textColourId, kText);
    setColour(juce::ComboBox::arrowColourId, kGold);
    setColour(juce::PopupMenu::backgroundColourId, kWell);
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kGoldLo);
    setColour(juce::PopupMenu::highlightedTextColourId, kOnText);
    setColour(juce::TooltipWindow::backgroundColourId, kWellDeep);
    setColour(juce::TooltipWindow::textColourId, kText);
    setColour(juce::TooltipWindow::outlineColourId, kBorder);
}

void StrumlineLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos, float startAngle, float endAngle, juce::Slider&) {
    const auto b = juce::Rectangle<float>(float(x), float(y), float(width), float(height)).reduced(2.0f);
    const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f, cx = b.getCentreX(), cy = b.getCentreY(), angle = startAngle + pos * (endAngle - startAngle);
    // The sunken dish with a ring of fine marks around it: the marks up to the value are gold.
    sunken(g, juce::Rectangle<float>(cx - r, cy - r, 2.0f * r, 2.0f * r), r, kWellDeep);
    constexpr int ticks = 31;
    for (int i = 0; i < ticks; ++i) {
        const float a = startAngle + (endAngle - startAngle) * float(i) / float(ticks - 1), s = std::sin(a), c = std::cos(a);
        const bool on = float(i) / float(ticks - 1) <= pos + 0.0001f;
        g.setColour(on ? kGold : juce::Colour(0xff45464b));
        g.drawLine(cx + s * (r - 5.5f), cy - c * (r - 5.5f), cx + s * (r - 2.5f), cy - c * (r - 2.5f), on ? 1.6f : 1.2f);
    }
    // The raised cap.
    const float k = r * 0.66f;
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - k + 1.0f, cy - k + 2.0f, 2.0f * k, 2.0f * k);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff45464b), cx, cy - k, juce::Colour(0xff2a2b2e), cx, cy + k, false));
    g.fillEllipse(cx - k, cy - k, 2.0f * k, 2.0f * k);
    g.setColour(juce::Colour(0x22ffffff));
    g.drawEllipse(cx - k + 0.5f, cy - k + 0.5f, 2.0f * k - 1.0f, 2.0f * k - 1.0f, 1.0f);
    const float s = std::sin(angle), c = std::cos(angle);
    g.setColour(kGold);
    g.fillEllipse(cx + s * k * 0.68f - 2.4f, cy - c * k * 0.68f - 2.4f, 4.8f, 4.8f);
}

void StrumlineLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(3.0f);
    const bool on = b.getToggleState();
    if (on || down) {
        sunken(g, r, 7.0f, on ? kGold : kWell);
        if (on) {
            g.setGradientFill(juce::ColourGradient(kGoldHi, r.getX(), r.getY(), kGoldLo, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
        }
    } else {
        raised(g, r, 7.0f);
        if (highlighted) {
            g.setColour(juce::Colour(0x12ffffff));
            g.fillRoundedRectangle(r, 7.0f);
        }
    }
}

void StrumlineLook::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) {
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, float(width), float(height)).reduced(2.0f);
    sunken(g, r, 6.0f);
    juce::Path arrow;
    const float cx = float(width) - 14.0f, cy = float(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(kGold);
    g.fillPath(arrow);
}

// ---------------------------------------------------------------------------------------------------------------------------------- segmented

Segmented::Segmented(juce::AudioProcessorValueTreeState& state, const char* id, juce::StringArray labels, bool toggle) : labels_(std::move(labels)), toggle_(toggle) {
    auto* param = state.getParameter(id);
    jassert(param != nullptr);
    attachment_ = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) {
        selected_ = int(v + 0.5f);
        repaint();
    });
    attachment_->sendInitialUpdate();
}

void Segmented::paint(juce::Graphics& g) {
    const int n = labels_.size();
    const float w = float(getWidth()) / float(std::max(1, n));
    for (int i = 0; i < n; ++i) {
        const auto r = juce::Rectangle<float>(w * float(i) + 3.0f, 3.0f, w - 6.0f, float(getHeight()) - 6.0f);
        const bool on = toggle_ ? selected_ != 0 : selected_ == i;
        if (on) {
            sunken(g, r, 7.0f, kGold);
            g.setGradientFill(juce::ColourGradient(kGoldHi, r.getX(), r.getY(), kGoldLo, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
        } else {
            raised(g, r, 7.0f);
        }
        g.setColour(on ? kOnText : juce::Colour(0xffd0cec7));
        g.setFont(font(12.0f, on));
        g.drawText(labels_[i], r.toNearestInt(), juce::Justification::centred);
    }
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
    const int n = std::max(1, labels_.size());
    const int index = juce::jlimit(0, n - 1, int(float(e.x) / (float(getWidth()) / float(n))));
    attachment_->setValueAsCompleteGesture(toggle_ ? (selected_ != 0 ? 0.0f : 1.0f) : float(index));
}

// ----------------------------------------------------------------------------------------------------------------------------------- editor

StrumlineEditor::StrumlineEditor(StrumlineProcessor& p)
    : AudioProcessorEditor(p), p_(p), mode_(p.apvts, ids::mode, {"Mono", "Poly", "Hybrid"}), bend_(p.apvts, ids::bend, {"Off", "Channel", "MPE"}),
      wave_(p.apvts, ids::wave, {"Sine", "Saw", "Square"}), thru_(p.apvts, ids::thru, {"THRU"}, true), synth_(p.apvts, ids::synth, {"SYNTH"}, true),
      perf_(p.apvts, ids::perf, {"Fast", "Saver"}) {
    setLookAndFeel(&look_);
    setSize(kWidth, kHeight);

    addKnob(sens_, ids::sens, sensAtt_);
    addKnob(lookahead_, ids::lookahead, lookAtt_);
    addKnob(velocity_, ids::velocity, velAtt_);
    addKnob(channel_, ids::channel, chanAtt_);
    addKnob(transpose_, ids::transpose, transAtt_);
    addKnob(level_, ids::level, levelAtt_);
    sens_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " dB"; };
    lookahead_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " ms"; };
    velocity_.textFromValueFunction = [](double v) { return tr(juce::StringArray{"soft", "normal", "hard"}[juce::jlimit(0, 2, int(v + 0.5))]); };
    channel_.textFromValueFunction = [](double v) { return juce::String(int(v)); };
    transpose_.textFromValueFunction = [](double v) { return (v > 0 ? "+" : "") + juce::String(int(v)) + " st"; };
    level_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " dB"; };

    for (auto* c : std::initializer_list<juce::Component*>{&mode_, &bend_, &wave_, &thru_, &synth_, &test_, &perf_, &preset_, &panic_, &language_}) addAndMakeVisible(c);

    // Presets: a click sets the mode and the detection knobs together, then the box goes back to its title.
    preset_.onChange = [this] {
        if (preset_.getSelectedId() > 0) applyPreset(preset_.getSelectedId());
        preset_.setSelectedId(0, juce::dontSendNotification);
    };
    panic_.onClick = [this] { p_.panic(); };

    // The language: Auto and the ten the interface has, each named in its own language. A change applies at once and is kept for the next time.
    for (int l = 0; l <= int(i18n::Language::Korean); ++l) language_.addItem(i18n::languageName(i18n::Language(l)), l + 1);
    language_.setSelectedId(int(i18n::preference()) + 1, juce::dontSendNotification);
    language_.onChange = [this] {
        i18n::setPreference(i18n::Language(language_.getSelectedId() - 1));
        applyTexts();
    };

    test_.setClickingTogglesState(true);
    test_.setToggleState(p_.testRunning(), juce::dontSendNotification);
    test_.onClick = [this] { p_.setTestRunning(test_.getToggleState()); };

    applyTexts();
    startTimerHz(30);
}

// Every text that is set once (labels, tooltips, the presets) in the language in use. Called when the editor is built and when the language changes; the
// texts that are drawn ask for their translation each time they are drawn, and the cached background is thrown away so that it is drawn again.
void StrumlineEditor::applyTexts() {
    mode_.setLabels(trAll({"Mono", "Poly", "Hybrid"}));
    bend_.setLabels(trAll({"Off", "Channel", "MPE"}));
    wave_.setLabels(trAll({"Sine", "Saw", "Square"}));
    thru_.setLabels(trAll({"THRU"}));
    synth_.setLabels(trAll({"SYNTH"}));
    perf_.setLabels(trAll({"Fast", "Saver"}));
    test_.setButtonText(tr("TEST"));
    panic_.setButtonText(tr("PANIC"));

    preset_.clear(juce::dontSendNotification);
    preset_.addItem(tr("Solo lines (one note, fastest)"), 1);
    preset_.addItem(tr("Fast hybrid (45 ms look-ahead)"), 2);
    preset_.addItem(tr("Chords (Poly, 60 ms)"), 3);
    preset_.addItem(tr("Low CPU (Hybrid, saver)"), 4);
    preset_.setTextWhenNothingSelected(tr("Preset..."));

    // What each control does, for the hover.
    sens_.setTooltip(tr("How loud the guitar must be to count as playing. The white mark on the input level shows it: set it just above the noise"));
    lookahead_.setTooltip(tr("How much of the future the chord converter may hear before it decides: less is faster but makes more mistakes (marks at 35, 45 and 60 ms)"));
    velocity_.setTooltip(tr("The curve from how hard you pluck to the MIDI velocity: soft lifts quiet notes, hard asks for more"));
    channel_.setTooltip(tr("The MIDI channel the notes go out on"));
    transpose_.setTooltip(tr("Shifts every note, in semitones"));
    level_.setTooltip(tr("The level of the monitor synth"));
    mode_.setTooltip(tr("Mono: one note at a time, the fastest. Poly: chords (Basic Pitch), about 80 to 100 ms. Hybrid: the first note fast, the rest of the chord after it"));
    bend_.setTooltip(tr("Off: no pitch wheel. Channel: one pitch wheel for the channel. MPE: not written yet (sends like Channel)"));
    perf_.setTooltip(tr("Chord rate. Fast: the chord model runs every 35 ms. Saver: every 46 ms, a third less CPU and about 8 ms later"));
    thru_.setTooltip(tr("The guitar goes on to the output, so Strumline can sit first in a chain"));
    synth_.setTooltip(tr("Plays the notes with the built-in synth, to hear the conversion without another instrument"));
    wave_.setTooltip(tr("The wave of the monitor synth"));
    panic_.setTooltip(tr("All notes off, and the converters start again (for a note that hangs)"));
    preset_.setTooltip(tr("Sets the mode and the detection knobs together"));
    test_.setTooltip(tr("Plays a built-in arpeggio and chord, to try the MIDI output and the monitor synth without the converter"));
    language_.setTooltip(tr("The language of the interface (Auto follows the language of the system)"));

    for (auto* s : {&sens_, &lookahead_, &velocity_, &channel_, &transpose_, &level_}) s->updateText();
    bg_ = juce::Image();  // the captions are in the cached background
    repaint();
}

StrumlineEditor::~StrumlineEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void StrumlineEditor::addKnob(juce::Slider& s, const char* id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 16);
    s.setDoubleClickReturnValue(true, 0.0);
    s.setColour(juce::Slider::textBoxTextColourId, kText);  // (set here as well: the slider is made before the editor's look is applied)
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p_.apvts, id, s);
}

juce::Rectangle<int> StrumlineEditor::panelArea(int index) const {
    const int w = (kWidth - 28 - 20) / 3;
    return {14 + index * (w + 10), 360, w, kHeight - 360 - 14};
}

void StrumlineEditor::placeKnob(juce::Slider& s, juce::Rectangle<int> panel, int slot, int slots) {
    const int kw = 70, kh = 86, gap = (panel.getWidth() - kw * slots) / (slots + 1);
    s.setBounds(panel.getX() + gap + slot * (kw + gap), panel.getY() + 46, kw, kh);
}

void StrumlineEditor::resized() {
    mode_.setBounds(120, 14, 186, 26);
    thru_.setBounds(312, 14, 64, 26);
    test_.setBounds(382, 14, 56, 26);

    const auto detection = panelArea(0), output = panelArea(1), monitor = panelArea(2);
    placeKnob(sens_, detection, 0, 3);
    placeKnob(lookahead_, detection, 1, 3);
    placeKnob(velocity_, detection, 2, 3);
    placeKnob(channel_, output, 0, 2);
    placeKnob(transpose_, output, 1, 2);
    bend_.setBounds(output.getX() + 8, output.getBottom() - 40, output.getWidth() - 16, 26);
    perf_.setBounds(detection.getX() + 8, detection.getBottom() - 40, detection.getWidth() - 16, 26);
    preset_.setBounds(detection.getX() + 92, detection.getY() + 4, detection.getWidth() - 100, 20);
    panic_.setBounds(output.getRight() - 66, output.getY() + 5, 58, 20);
    language_.setBounds(monitor.getRight() - 86, monitor.getY() + 4, 78, 20);
    level_.setBounds(monitor.getX() + 16, monitor.getY() + 46, 70, 86);
    synth_.setBounds(monitor.getX() + 104, monitor.getY() + 68, 86, 26);
    wave_.setBounds(monitor.getX() + 8, monitor.getBottom() - 40, monitor.getWidth() - 16, 26);
}

void StrumlineEditor::timerCallback() { updateFromProcessor(juce::Time::getMillisecondCounterHiRes() * 0.001); }

void StrumlineEditor::updateFromProcessor(double nowSeconds) {
    now_ = nowSeconds;
    gm::NoteEvent e;
    while (p_.noteBus().pop(e)) {
        lastEvent_ = now_;
        if (e.bend14 >= 0) {
            bends_.push_back({now_, e.bend14});
            continue;
        }
        if (e.on) {
            notes_.push_back({e.midi, now_, -1.0, e.velocity});
        } else {
            for (auto it = notes_.rbegin(); it != notes_.rend(); ++it)
                if (it->midi == e.midi && it->end < 0.0) { it->end = now_; break; }
        }
    }
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [&](const Note& n) { return n.end >= 0.0 && n.end < now_ - 13.0; }), notes_.end());
    bends_.erase(std::remove_if(bends_.begin(), bends_.end(), [&](const Bend& b) { return b.at < now_ - 13.0; }), bends_.end());
    // The load of the chord model: the share of the time that it spent running, over the last second or so.
    if (now_ - loadAt_ >= 1.0) {
        const double busy = p_.chordBusyMs();
        if (loadAt_ > 0.0) loadShown_ = float(100.0 * (busy - loadBusy_) / (1000.0 * (now_ - loadAt_)));
        loadAt_ = now_;
        loadBusy_ = busy;
    }
    const float db = p_.inputLevelDb();
    levelShown_ = db > levelShown_ ? db : std::max(db, levelShown_ - 2.5f);
    test_.setToggleState(p_.testRunning(), juce::dontSendNotification);
    repaint();
}

// ----------------------------------------------------------------------------------------------------------------------------------- drawing

// The parts that never change (the ground, the wells, the captions, the marks of the knob) are drawn once into an image; paint() only adds what moves.
void StrumlineEditor::paintStatic(juce::Graphics& g) {
    const auto all = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303134), 0.0f, 0.0f, kBack, 0.0f, 160.0f, false));
    g.fillRoundedRectangle(all, 10.0f);
    g.setColour(juce::Colour(0x18ffffff));
    g.drawRoundedRectangle(all.reduced(0.5f), 10.0f, 1.0f);

    drawLogo(g, {14.0f, 11.0f, 30.0f, 30.0f});
    g.setColour(kLabel);
    g.setFont(font(10.5f, true));
    g.drawText("STRUMLINE", 48, 11, 70, 30, juce::Justification::centredLeft);

    caption(g, tr("INPUT LEVEL"), {14, 50, 90, 16});
    sunken(g, {102.0f, 49.0f, float(kWidth) - 102.0f - 88.0f, 18.0f}, 9.0f, kWellDeep);

    const auto roll = rollArea();
    sunken(g, roll.toFloat(), 10.0f, kWellDeep);
    caption(g, tr("NOTES  (LAST 12 SECONDS)"), {roll.getX() + 4, roll.getBottom() + 3, 240, 14});
    caption(g, tr("PITCH BEND"), {roll.getRight() - 124, roll.getBottom() + 3, 120, 14}, juce::Justification::centredRight);

    const juce::String titles[3] = {tr("DETECTION"), tr("OUTPUT"), tr("MONITOR SYNTH")};
    for (int i = 0; i < 3; ++i) {
        const auto r = panelArea(i);
        sunken(g, r.toFloat(), 12.0f);
        caption(g, titles[i], {r.getX() + 14, r.getY() + 8, r.getWidth() - 28, 14});
    }
    auto label = [&](juce::Component& c, const juce::String& text) { caption(g, text, {c.getX(), c.getY() - 19, c.getWidth(), 14}, juce::Justification::centred); };
    label(sens_, tr("SENS"));
    label(lookahead_, tr("LOOK-AHEAD"));
    label(velocity_, tr("VELOCITY"));
    label(channel_, tr("CHANNEL"));
    label(transpose_, tr("TRANSPOSE"));
    label(level_, tr("LEVEL"));

    // Marks on the look-ahead knob at 35, 45 and 60 ms (the values that were measured; the tooltip says so).
    const auto kb = lookahead_.getBounds();
    const float cx = float(kb.getCentreX()), cy = float(kb.getY()) + 35.0f;
    for (const float ms : {35.0f, 45.0f, 60.0f}) {
        const float a = juce::MathConstants<float>::pi * (1.25f + 1.5f * ms / 120.0f), s = std::sin(a), c = std::cos(a);
        g.setColour(kLabel);
        g.drawLine(cx + s * 35.0f, cy - c * 35.0f, cx + s * 38.0f, cy - c * 38.0f, 1.5f);
    }
}

void StrumlineEditor::paint(juce::Graphics& g) {
    float scale = 1.0f;
    if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds())) scale = float(d->scale);
    const int w = std::max(1, int(std::lround(float(getWidth()) * scale))), h = std::max(1, int(std::lround(float(getHeight()) * scale)));
    if (!bg_.isValid() || bg_.getWidth() != w || bg_.getHeight() != h) {
        bg_ = juce::Image(juce::Image::ARGB, w, h, true);
        juce::Graphics ig(bg_);
        ig.addTransform(juce::AffineTransform::scale(scale));
        paintStatic(ig);
    }
    g.drawImage(bg_, 0, 0, getWidth(), getHeight(), 0, 0, w, h);
    drawHeader(g);
    drawLevel(g);
    drawStrings(g);
    drawRoll(g);
}

void StrumlineEditor::drawLogo(juce::Graphics& g, juce::Rectangle<float> area) {
    // The mark of the logo (docs/logo): strings that start as waves and end as the notes of a strummed chord, drawn with paths so it needs no image.
    const float s = area.getWidth() / 30.0f;
    g.setColour(juce::Colour(0xff202125));
    g.fillRoundedRectangle(area, 7.0f * s);
    g.setColour(kBorder);
    g.drawRoundedRectangle(area.reduced(0.5f), 7.0f * s, 1.0f);
    for (int i = 0; i < 4; ++i) {
        const float y = 7.5f + 5.0f * float(i), bx = 14.0f + 3.0f * float(i);
        juce::Path line;
        line.startNewSubPath(3.0f, y);
        line.cubicTo(4.6f, y, 5.4f, y + 3.0f, 7.5f, y + 3.0f);
        line.cubicTo(9.6f, y + 3.0f, 10.2f, y, 12.5f, y);
        line.lineTo(bx - 1.5f, y);
        if (i < 3) {
            line.startNewSubPath(bx + 5.5f, y);
            line.lineTo(27.0f, y);
        }
        g.setColour(kGold);
        g.strokePath(line, juce::PathStrokeType(1.8f * s), juce::AffineTransform::scale(s).translated(area.getX(), area.getY()));
        g.fillRoundedRectangle(area.getX() + bx * s, area.getY() + (y - 1.5f) * s, 4.0f * s, 3.0f * s, 1.2f * s);
    }
}

void StrumlineEditor::drawHeader(juce::Graphics& g) {

    // Four figures: the voices, the latency of the first note (one-note converter), the latency of the chord (chord converter: from where the note began to the
    // note-on, with the run of the model) and how much of the time the chord model is running.
    auto stat = [&](int x, int w, const juce::String& title, const juce::String& text, bool known) {
        caption(g, title, {x, 8, w, 14}, juce::Justification::centredRight);
        g.setColour(known ? kText : kLabel);
        g.setFont(font(15.0f, true));
        g.drawText(text, x, 22, w, 20, juce::Justification::centredRight);
    };
    stat(444, 46, tr("VOICES"), juce::String(p_.voices()) + " / 6", true);
    const float note = p_.latencyMs(), chord = p_.chordLatencyMs();
    stat(494, 54, tr("NOTE"), note > 0.0f ? juce::String(int(std::lround(note))) + " ms" : juce::String("-- ms"), note > 0.0f);
    stat(552, 56, tr("CHORD"), chord > 0.0f && p_.chordsAvailable() ? juce::String(int(std::lround(chord))) + " ms" : juce::String("-- ms"), chord > 0.0f && p_.chordsAvailable());
    stat(612, 54, tr("MODEL"), p_.chordsAvailable() ? juce::String(int(std::lround(std::max(0.0f, loadShown_)))) + " %" : tr("off"), p_.chordsAvailable());
}

void StrumlineEditor::drawLevel(juce::Graphics& g) {
    const auto bar = juce::Rectangle<float>(108.0f, 54.0f, float(kWidth) - 108.0f - 94.0f, 8.0f);
    auto frac = [](float db) { return juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f); };
    const float sens = p_.apvts.getRawParameterValue(ids::sens)->load();
    const auto fill = bar.withWidth(std::max(3.0f, bar.getWidth() * frac(levelShown_)));
    g.setColour(levelShown_ >= sens ? kGold : juce::Colour(0xff5a5b60));
    g.fillRoundedRectangle(fill, 4.0f);
    {  // the stripes of a bar at work
        juce::Graphics::ScopedSaveState keep(g);
        juce::Path clip;
        clip.addRoundedRectangle(fill, 4.0f);
        g.reduceClipRegion(clip);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        for (float x = fill.getX() - 8.0f; x < fill.getRight(); x += 7.0f) g.drawLine(x, fill.getBottom(), x + 8.0f, fill.getY(), 2.0f);
    }
    const float x = bar.getX() + bar.getWidth() * frac(sens);
    g.setColour(kText);
    g.drawLine(x, bar.getY() - 4.0f, x, bar.getBottom() + 4.0f, 2.0f);

    // MIDI OUT: a light in its own socket that flashes with every note and bend that is sent and fades in a third of a second.
    const float glow = juce::jlimit(0.0f, 1.0f, 1.0f - float(now_ - lastEvent_) / 0.35f);
    const float cx = float(kWidth) - 78.0f, cy = 58.0f;
    sunken(g, {cx - 8.0f, cy - 8.0f, 16.0f, 16.0f}, 8.0f, kWellDeep);
    if (glow > 0.0f) {
        g.setColour(kGreen.withAlpha(0.28f * glow));
        g.fillEllipse(cx - 11.0f, cy - 11.0f, 22.0f, 22.0f);
    }
    g.setColour(juce::Colour(0xff34353a).interpolatedWith(kGreen, glow));
    g.fillEllipse(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
    caption(g, tr("MIDI OUT"), {int(cx) + 12, 51, 62, 14});
}

void StrumlineEditor::drawStrings(juce::Graphics& g) {
    static constexpr int open[6] = {40, 45, 50, 55, 59, 64};
    static const char* names[6] = {"E2", "A2", "D3", "G3", "B3", "E4"};
    bool lit[6] = {};
    int litCount = 0;
    for (const auto& n : notes_) {
        if (n.end >= 0.0) continue;
        int s = -1;  // the highest string whose open note is not above the note, within two octaves of frets
        for (int i = 0; i < 6; ++i)
            if (open[i] <= n.midi && n.midi - open[i] <= 24) s = i;
        if (s >= 0) lit[s] = true;
    }
    for (const bool l : lit) litCount += l ? 1 : 0;
    // With a single note held by the one-note converter the string shows how far from the note the pitch is, in cents (a tuner for free).
    const int modeNow = int(p_.apvts.getRawParameterValue(ids::mode)->load() + 0.5f);
    const bool tuner = litCount == 1 && p_.voices() == 1 && modeNow != 1;  // not in Poly: there the pitch bend is not followed
    const float cents = p_.centsNow();
    for (int i = 0; i < 6; ++i) {
        const float cx = float(kWidth) * 0.5f + (float(i) - 2.5f) * 66.0f, cy = 98.0f, r = 22.0f;
        const auto disc = juce::Rectangle<float>(cx - r, cy - r, 2.0f * r, 2.0f * r);
        if (lit[i]) {  // sunken, with a gold ring
            sunken(g, disc, r, juce::Colour(0xff2a2412));
            g.setColour(kGold);
            g.drawEllipse(disc.reduced(1.5f), 2.2f);
        } else {
            raised(g, disc, r);
        }
        g.setColour(lit[i] ? kGold : juce::Colour(0xffbdbbb4));
        g.setFont(font(14.0f, true));
        g.drawText(names[i], juce::Rectangle<float>(cx - r, cy - 10.0f, 2.0f * r, 20.0f), juce::Justification::centred);
        const auto below = juce::Rectangle<float>(cx - 30.0f, cy + r + 1.0f, 60.0f, 12.0f);
        if (lit[i] && tuner) {
            const bool inTune = std::abs(cents) < 8.0f;
            g.setColour(inTune ? kGreen : kGold);
            g.setFont(font(10.5f, true));
            g.drawText((cents > 0.5f ? "+" : "") + juce::String(int(std::lround(cents))) + " ct", below, juce::Justification::centred);
        } else {
            g.setColour(kLabel);
            g.setFont(font(10.0f));
            g.drawText(juce::String(6 - i), below, juce::Justification::centred);
        }
    }
}

void StrumlineEditor::drawRoll(juce::Graphics& g) {
    const auto area = rollArea();
    sunken(g, area.toFloat(), 10.0f, kWellDeep);
    const auto inner = area.reduced(6).toFloat();
    const float bendH = 34.0f, notesH = inner.getHeight() - bendH;

    // The range of the roll follows what was played (at least two octaves, so a single note does not fill the picture), easing from one range to the next.
    int lo = 127, hi = 0;
    for (const auto& n : notes_) {
        lo = std::min(lo, n.midi);
        hi = std::max(hi, n.midi);
    }
    float wantLo = 45.0f, wantHi = 69.0f;
    if (hi >= lo) {
        const float centre = 0.5f * float(lo + hi), half = std::max(12.0f, 0.5f * float(hi - lo) + 2.0f);
        wantLo = centre - half;
        wantHi = centre + half;
    }
    lowShown_ += 0.12f * (wantLo - lowShown_);
    highShown_ += 0.12f * (wantHi - highShown_);
    const int low = std::max(21, int(std::floor(lowShown_))), high = std::min(108, std::max(low + 12, int(std::ceil(highShown_)))), rows = high - low + 1;
    const float rh = notesH / float(rows), x0 = inner.getX(), w = inner.getWidth();
    auto yOf = [&](int m) { return inner.getY() + notesH - float(m - low + 1) * rh; };
    for (int m = low; m <= high; ++m) {
        const int pc = m % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour(black ? juce::Colour(0xff141517) : juce::Colour(0xff1a1b1d));
        g.fillRect(x0, yOf(m), w, rh);
        if (pc == 0) {  // the Cs, an octave apart, to read the heights
            g.setColour(juce::Colour(0xff7b7a74));
            g.setFont(font(10.0f));
            g.drawText("C" + juce::String(m / 12 - 1), int(x0 + 4.0f), int(yOf(m) - 4.0f), 24, 12, juce::Justification::centredLeft);
        }
    }
    g.setColour(juce::Colour(0xff26272b));
    for (int i = 0; i <= 12; ++i) g.drawVerticalLine(int(x0 + w * float(i) / 12.0f), inner.getY(), inner.getBottom());
    const double span = 12.0;
    // The notes, thicker the taller the rows are, and brighter the harder they were plucked.
    const juce::Colour soft(0xff7a6126);
    for (const auto& n : notes_) {
        if (n.midi < low || n.midi > high) continue;
        const double t0 = n.start, t1 = n.end < 0.0 ? now_ : n.end;
        const float xa = x0 + w * float(1.0 - (now_ - t0) / span), xb = x0 + w * float(1.0 - (now_ - t1) / span);
        const float age = float(juce::jlimit(0.0, 1.0, (now_ - t1) / span));
        const float hard = juce::jlimit(0.0f, 1.0f, float(n.velocity - 20) / 100.0f);
        g.setColour(soft.interpolatedWith(kGold, hard).withAlpha(1.0f - 0.55f * age));
        g.fillRoundedRectangle(juce::Rectangle<float>(std::max(x0, xa), yOf(n.midi) + 0.5f, std::max(3.0f, xb - std::max(x0, xa) - 1.0f), std::max(2.0f, rh - 1.0f)), 1.5f);
    }

    // The pitch bend lane: what is sent (the pitch wheel, two semitones each way), as a line that holds its value until the next one.
    const float by = inner.getBottom() - bendH;
    g.setColour(juce::Colour(0xff101114));
    g.fillRect(x0, by, w, bendH);
    g.setColour(kBorder);
    g.drawHorizontalLine(int(by + bendH * 0.5f), x0, x0 + w);
    if (!bends_.empty()) {
        auto yBend = [&](int v) { return by + bendH * 0.5f - float(v - 8192) / 8191.0f * (bendH * 0.5f - 3.0f); };
        juce::Path line;
        float lastY = by + bendH * 0.5f;
        line.startNewSubPath(x0, lastY);
        for (const auto& b : bends_) {
            const float x = std::max(x0, x0 + w * float(1.0 - (now_ - b.at) / span));
            line.lineTo(x, lastY);
            lastY = yBend(b.value);
            line.lineTo(x, lastY);
        }
        line.lineTo(x0 + w, lastY);
        g.setColour(kGold);
        g.strokePath(line, juce::PathStrokeType(1.5f));
    }
    g.setColour(kText);
    g.drawVerticalLine(int(x0 + w - 3.0f), inner.getY(), inner.getBottom());

    // onnxruntime.dll or the model is missing: Poly and Hybrid play as Mono. A strip over the roll, not a line of small print.
    if (p_.apvts.getRawParameterValue(ids::mode)->load() > 0.5f && !p_.chordsAvailable()) {
        const auto strip = juce::Rectangle<float>(inner.getX() + 40.0f, inner.getY() + 8.0f, inner.getWidth() - 80.0f, 22.0f);
        g.setColour(juce::Colour(0xe0201a08));
        g.fillRoundedRectangle(strip, 6.0f);
        g.setColour(kGold);
        g.drawRoundedRectangle(strip, 6.0f, 1.2f);
        g.setFont(font(11.5f, true));
        g.drawText(tr("CHORD MODEL NOT LOADED (ONNX Runtime is missing): ONE NOTE AT A TIME"), strip.toNearestInt(), juce::Justification::centred);
    }
}

void StrumlineEditor::applyPreset(int index) {
    auto set = [&](const char* id, float value) {
        if (auto* prm = p_.apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(value));
    };
    switch (index) {
        case 1: set(ids::mode, 0.0f); set(ids::lookahead, 60.0f); break;                         // Mono: the fastest, one note at a time
        case 2: set(ids::mode, 2.0f); set(ids::lookahead, 45.0f); set(ids::perf, 0.0f); break;  // Hybrid with a short look-ahead
        case 3: set(ids::mode, 1.0f); set(ids::lookahead, 60.0f); set(ids::perf, 0.0f); break;  // Poly: the most exact
        case 4: set(ids::mode, 2.0f); set(ids::lookahead, 60.0f); set(ids::perf, 1.0f); break;  // Hybrid with the saver rate
        default: break;
    }
}

