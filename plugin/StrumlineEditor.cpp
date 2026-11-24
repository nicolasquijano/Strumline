#include "StrumlineEditor.h"

#include <algorithm>
#include <cmath>

#include "I18n.h"

// A flat dark look with one gold accent. Everything is drawn with paths on a 1000 x 640 canvas.
namespace {
const juce::Colour kBack(0xff0b0d10), kPanel(0xff12161b), kPanelLine(0xff232932), kWell(0xff0e1216), kWellLine(0xff171d24), kGold(0xffe0b24a), kGoldHi(0xfff6d27c),
    kGoldDeep(0xff2a2412), kText(0xffe6e8eb), kSecondary(0xffaab1bc), kMuted(0xff7d8590), kFaint(0xff566070), kGreen(0xff3ddc84), kTrack(0xff232932), kButton(0xff161b21),
    kButtonLine(0xff232932), kOnGold(0xff17130a);

juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }

// A small label in capitals with a little space between the letters.
void caption(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft, juce::Colour c = kMuted) {
    g.setColour(c);
    // Devanagari letters join into one shape: spacing them apart would split the words (and it has no capitals anyway).
    g.setFont(font(11.0f, false).withExtraKerningFactor(i18n::active() == i18n::Language::Hindi ? 0.0f : 0.07f));
    g.drawText(text.toUpperCase(), r, j);
}

void card(juce::Graphics& g, juce::Rectangle<int> r) {
    g.setColour(kPanel);
    g.fillRoundedRectangle(r.toFloat(), 14.0f);
    g.setColour(kPanelLine);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 14.0f, 1.0f);
}

juce::String capitalised(const juce::String& s) { return s.isEmpty() ? s : s.substring(0, 1).toUpperCase() + s.substring(1); }

float textWidth(const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth(f, s); }

const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
juce::String noteName(int midi) { return juce::String(kNoteNames[((midi % 12) + 12) % 12]) + juce::String(midi / 12 - 1); }
}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------- look

StrumlineLook::StrumlineLook() {
    setColour(juce::Slider::textBoxTextColourId, kText);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, kSecondary);
    setColour(juce::TextButton::textColourOnId, kGold);
    setColour(juce::ComboBox::textColourId, kSecondary);
    setColour(juce::ComboBox::arrowColourId, kGold);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff161b21));
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kGold);
    setColour(juce::PopupMenu::highlightedTextColourId, kOnGold);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff161b21));
    setColour(juce::TooltipWindow::textColourId, kText);
    setColour(juce::TooltipWindow::outlineColourId, kPanelLine);
}

// An arc knob: a track, the value in gold with a soft glow, and a dark cap with a dot.
void StrumlineLook::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos, float startAngle, float endAngle, juce::Slider&) {
    const auto b = juce::Rectangle<float>(float(x), float(y), float(width), float(height)).reduced(5.0f);
    const float r = std::min(b.getWidth(), b.getHeight()) * 0.5f, cx = b.getCentreX(), cy = b.getCentreY(), angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = r - 3.0f;
    juce::Path track, value;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour(kTrack);
    g.strokePath(track, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pos > 0.001f) {
        g.setColour(kGoldHi.withAlpha(0.18f));
        g.strokePath(value, juce::PathStrokeType(11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(kGoldHi);
        g.strokePath(value, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(juce::Colour(0xff161b21));
    g.fillEllipse(cx - r * 0.55f, cy - r * 0.55f, r * 1.1f, r * 1.1f);
    const float s = std::sin(angle), c = std::cos(angle), d = r * 0.34f;
    g.setColour(kGoldHi);
    g.fillEllipse(cx + s * d - 2.6f, cy - c * d - 2.6f, 5.2f, 5.2f);
}

// A thin track filled in gold up to a white round thumb.
void StrumlineLook::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&) {
    const float cy = float(y) + float(height) * 0.5f, x0 = float(x) + 8.0f, x1 = float(x + width) - 8.0f;
    g.setColour(kTrack);
    g.fillRoundedRectangle(x0, cy - 2.0f, x1 - x0, 4.0f, 2.0f);
    const float px = juce::jlimit(x0, x1, sliderPos);
    g.setColour(kGold);
    g.fillRoundedRectangle(x0, cy - 2.0f, px - x0, 4.0f, 2.0f);
    g.setColour(kGoldHi.withAlpha(0.15f));
    g.fillEllipse(px - 11.0f, cy - 11.0f, 22.0f, 22.0f);
    g.setColour(kText);
    g.fillEllipse(px - 8.0f, cy - 8.0f, 16.0f, 16.0f);
}

void StrumlineLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) {
    const auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState() || down;
    g.setColour(on ? kGoldDeep : (highlighted ? juce::Colour(0xff1b2129) : kButton));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(on ? kGold : kButtonLine);
    g.drawRoundedRectangle(r.reduced(0.5f), 9.0f, 1.0f);
}

void StrumlineLook::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) {
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, float(width), float(height)).reduced(1.0f);
    g.setColour(kButton);
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(kButtonLine);
    g.drawRoundedRectangle(r.reduced(0.5f), 9.0f, 1.0f);
    juce::Path arrow;
    const float cx = float(width) - 14.0f, cy = float(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(kGold);
    g.fillPath(arrow);
}

// ---------------------------------------------------------------------------------------------------------------------------------- segmented

Segmented::Segmented(juce::AudioProcessorValueTreeState& state, const char* id, juce::StringArray labels, bool toggle, Style style)
    : labels_(std::move(labels)), toggle_(toggle), style_(style) {
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
    const auto all = getLocalBounds().toFloat().reduced(1.0f);
    if (style_ == Style::Primary) {
        g.setColour(kButton);
        g.fillRoundedRectangle(all, 11.0f);
        g.setColour(kButtonLine);
        g.drawRoundedRectangle(all.reduced(0.5f), 11.0f, 1.0f);
    }
    const float w = all.getWidth() / float(std::max(1, n));
    for (int i = 0; i < n; ++i) {
        const bool on = toggle_ ? selected_ != 0 : selected_ == i;
        juce::Rectangle<float> r(all.getX() + w * float(i), all.getY(), w, all.getHeight());
        if (style_ == Style::Primary) {
            r = r.reduced(3.0f);
            if (on) {
                g.setColour(kGold);
                g.fillRoundedRectangle(r, 8.0f);
            }
        } else {
            r = r.reduced(3.0f, 0.0f);
            g.setColour(on ? kGoldDeep : kButton);
            g.fillRoundedRectangle(r, 9.0f);
            g.setColour(on ? kGold : kButtonLine);
            g.drawRoundedRectangle(r.reduced(0.5f), 9.0f, 1.0f);
        }
        g.setColour(on ? (style_ == Style::Primary ? kOnGold : kGold) : kMuted.brighter(0.1f));
        g.setFont(font(13.0f, on && style_ == Style::Primary));
        g.drawText(labels_[i], r.toNearestInt(), juce::Justification::centred);
    }
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
    const int n = std::max(1, labels_.size());
    const int index = juce::jlimit(0, n - 1, int(float(e.x) / (float(getWidth()) / float(n))));
    attachment_->setValueAsCompleteGesture(toggle_ ? (selected_ != 0 ? 0.0f : 1.0f) : float(index));
}

// ----------------------------------------------------------------------------------------------------------------------------------------- view

StrumlineView::StrumlineView(StrumlineProcessor& p)
    : p_(p), mode_(p.apvts, ids::mode, {"Mono", "Poly", "Hybrid"}, false, Segmented::Style::Primary), velocity_(p.apvts, ids::velocity, {"soft", "normal", "hard"}),
      perf_(p.apvts, ids::perf, {"Fast", "Saver"}), bend_(p.apvts, ids::bend, {"Off", "Channel", "MPE"}), wave_(p.apvts, ids::wave, {"Sine", "Saw", "Square"}),
      thru_(p.apvts, ids::thru, {"THRU"}, true), synth_(p.apvts, ids::synth, {"SYNTH"}, true) {
    setLookAndFeel(&look_);
    setSize(kWidth, kHeight);

    addKnob(sens_, ids::sens, sensAtt_);
    addKnob(lookahead_, ids::lookahead, lookAtt_);
    addBar(channel_, ids::channel, chanAtt_);
    addBar(transpose_, ids::transpose, transAtt_);
    addBar(level_, ids::level, levelAtt_);
    sens_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " dB"; };
    lookahead_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " ms"; };
    channel_.textFromValueFunction = [](double v) { return juce::String(int(v)); };
    transpose_.textFromValueFunction = [](double v) { return (v > 0 ? "+" : "") + juce::String(int(v)) + " st"; };
    level_.textFromValueFunction = [](double v) { return juce::String(int(v)) + " dB"; };

    for (auto* c : std::initializer_list<juce::Component*>{&mode_, &velocity_, &perf_, &bend_, &wave_, &thru_, &synth_, &test_, &panic_, &preset_, &language_})
        addAndMakeVisible(c);

    preset_.onChange = [this] {
        if (preset_.getSelectedId() > 0) applyPreset(preset_.getSelectedId());
        preset_.setSelectedId(0, juce::dontSendNotification);
    };
    panic_.onClick = [this] { p_.panic(); };
    for (int l = 0; l <= int(i18n::Language::Hindi); ++l) language_.addItem(i18n::languageName(i18n::Language(l)), l + 1);
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

StrumlineView::~StrumlineView() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void StrumlineView::addKnob(juce::Slider& s, const char* id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
    s.setColour(juce::Slider::textBoxTextColourId, kText);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p_.apvts, id, s);
}

void StrumlineView::addBar(juce::Slider& s, const char* id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(s);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p_.apvts, id, s);
}

void StrumlineView::resized() {
    mode_.setBounds(188, 24, 216, 36);
    thru_.setBounds(414, 24, 64, 36);
    test_.setBounds(482, 24, 64, 36);
    preset_.setBounds(556, 25, 150, 34);

    const auto det = detectionArea(), out = outputArea(), mon = monitorArea();
    sens_.setBounds(det.getX() + 34, det.getY() + 38, 120, 104);
    lookahead_.setBounds(det.getX() + 206, det.getY() + 38, 120, 104);
    velocity_.setBounds(det.getX() + 14, det.getBottom() - 54, 196, 36);
    perf_.setBounds(det.getX() + 214, det.getBottom() - 54, 134, 36);

    channel_.setBounds(out.getX() + 12, out.getY() + 64, out.getWidth() - 24, 26);
    transpose_.setBounds(out.getX() + 12, out.getY() + 126, out.getWidth() - 24, 26);
    bend_.setBounds(out.getX() + 12, out.getBottom() - 54, out.getWidth() - 24, 36);

    level_.setBounds(mon.getX() + 12, mon.getY() + 64, mon.getWidth() - 24, 26);
    wave_.setBounds(mon.getX() + 12, mon.getY() + 108, mon.getWidth() - 24, 36);
    synth_.setBounds(mon.getX() + 12, mon.getBottom() - 54, 110, 36);
    panic_.setBounds(mon.getX() + 128, mon.getBottom() - 54, mon.getWidth() - 140, 36);
    language_.setBounds(mon.getRight() - 104, mon.getY() + 12, 92, 26);
}

// Every text that is set once (labels, tooltips, the presets) in the language in use. Called when the view is built and when the language changes.
void StrumlineView::applyTexts() {
    mode_.setLabels(trAll({"Mono", "Poly", "Hybrid"}));
    bend_.setLabels(trAll({"Off", "Channel", "MPE"}));
    wave_.setLabels(trAll({"Sine", "Saw", "Square"}));
    thru_.setLabels(trAll({"THRU"}));
    synth_.setLabels(trAll({"SYNTH"}));
    perf_.setLabels(trAll({"Fast", "Saver"}));
    {
        juce::StringArray v;
        for (const auto& s : trAll({"soft", "normal", "hard"})) v.add(capitalised(s));
        velocity_.setLabels(v);
    }
    test_.setButtonText(tr("TEST"));
    panic_.setButtonText(tr("PANIC"));

    preset_.clear(juce::dontSendNotification);
    preset_.addItem(tr("Solo lines (one note, fastest)"), 1);
    preset_.addItem(tr("Fast hybrid (45 ms look-ahead)"), 2);
    preset_.addItem(tr("Chords (Poly, 60 ms)"), 3);
    preset_.addItem(tr("Low CPU (Hybrid, saver)"), 4);
    preset_.setTextWhenNothingSelected(tr("Preset..."));

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

    for (auto* s : {&sens_, &lookahead_, &channel_, &transpose_, &level_}) s->updateText();
    repaint();
}

void StrumlineView::applyPreset(int index) {
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

void StrumlineView::timerCallback() { update(juce::Time::getMillisecondCounterHiRes() * 0.001); }

void StrumlineView::update(double nowSeconds) {
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

// ---------------------------------------------------------------------------------------------------------------------------------------- drawing

void StrumlineView::paint(juce::Graphics& g) {
    g.fillAll(kBack);
    drawHeader(g);
    drawHero(g, heroArea());
    drawRoll(g, rollArea());
    drawControls(g);
}

// The mark of the logo (docs/logo): strings that start as waves and end as the notes of a strummed chord.
void StrumlineView::drawLogo(juce::Graphics& g, juce::Rectangle<float> area) {
    const float s = area.getWidth() / 597.0f;
    const auto t = juce::AffineTransform::translation(-213.0f, -213.0f).scaled(s).translated(area.getX(), area.getY());
    g.setColour(juce::Colour(0xff1d1e21));
    g.fillRoundedRectangle(area, 140.0f * s);
    g.setColour(kGold);
    for (const char* d : {"M213 379H260C320 379 340 440 400 440C450 440 460 379 480 379", "M213 480H260C320 480 340 541 400 541C450 541 520 480 560 480"}) {
        auto path = juce::Drawable::parseSVGPath(d);
        g.strokePath(path, juce::PathStrokeType(22.0f * s), t);
    }
    for (const auto& r : {juce::Rectangle<float>(500, 355, 80, 48), juce::Rectangle<float>(580, 456, 80, 48), juce::Rectangle<float>(660, 560, 80, 48)}) {
        juce::Path rr;
        rr.addRoundedRectangle(r, 14.0f);
        g.fillPath(rr, t);
    }
}

void StrumlineView::drawHeader(juce::Graphics& g) {
    drawLogo(g, {24.0f, 27.0f, 30.0f, 30.0f});
    g.setColour(kText);
    g.setFont(font(17.0f, true));
    g.drawText("Strumline", 62, 22, 120, 40, juce::Justification::centredLeft);

    // The latency of the first note (one-note converter), of the chord (from where the note began to the note-on, with the model's run), the share of the time the
    // chord model runs, and the voices.
    auto stat = [&](int x, const juce::String& title, const juce::String& number, const juce::String& unit, bool known) {
        caption(g, title, {x, 20, 70, 14});
        const auto big = font(20.0f, true);
        g.setColour(known ? kText : kFaint);
        g.setFont(big);
        g.drawText(number, x, 34, 60, 28, juce::Justification::centredLeft);
        if (unit.isNotEmpty()) {
            g.setColour(kMuted);
            g.setFont(font(12.0f));
            g.drawText(unit, x + int(textWidth(big, number)) + 3, 38, 40, 24, juce::Justification::centredLeft);
        }
    };
    const float note = p_.latencyMs(), chord = p_.chordLatencyMs();
    const bool chords = p_.chordsAvailable();
    stat(716, tr("NOTE"), note > 0.0f ? juce::String(int(std::lround(note))) : juce::String("--"), "ms", note > 0.0f);
    stat(782, tr("CHORD"), chord > 0.0f && chords ? juce::String(int(std::lround(chord))) : juce::String("--"), "ms", chord > 0.0f && chords);
    stat(850, tr("MODEL"), chords ? juce::String(int(std::lround(std::max(0.0f, loadShown_)))) : tr("off"), chords ? "%" : "", chords);
    stat(912, tr("VOICES"), juce::String(p_.voices()), "/ 6", true);

    // MIDI out: a dot that lights with every note and bend sent and fades in a third of a second.
    const float glow = juce::jlimit(0.0f, 1.0f, 1.0f - float(now_ - lastEvent_) / 0.35f);
    const float cx = 976.0f, cy = 28.0f;
    if (glow > 0.0f) {
        g.setColour(kGreen.withAlpha(0.25f * glow));
        g.fillEllipse(cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
    }
    g.setColour(juce::Colour(0xff252d37).interpolatedWith(kGreen, glow));
    g.fillEllipse(cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);
}

// The notes that sound now, the tuner of the one-note converter and the input level.
void StrumlineView::drawHero(juce::Graphics& g, juce::Rectangle<int> area) {
    card(g, area);
    caption(g, tr("PLAYING NOW"), {area.getX() + 18, area.getY() + 14, area.getWidth() - 36, 16});

    std::vector<int> sounding;
    for (const auto& n : notes_)
        if (n.end < 0.0) sounding.push_back(n.midi);
    std::sort(sounding.begin(), sounding.end());
    sounding.erase(std::unique(sounding.begin(), sounding.end()), sounding.end());
    const auto chipFont = font(22.0f, true);
    int cx = area.getX() + 18, cy = area.getY() + 40;
    const int right = area.getRight() - 18;
    if (sounding.empty()) {
        g.setColour(kFaint);
        g.setFont(chipFont);
        g.drawText(juce::String::fromUTF8("\xe2\x80\x94"), cx, cy, 60, 40, juce::Justification::centredLeft);
    }
    for (int i = 0; i < int(sounding.size()) && i < 6; ++i) {
        const auto name = noteName(sounding[std::size_t(i)]);
        const int w = std::max(52, int(textWidth(chipFont, name)) + 24);
        if (cx + w > right) { cx = area.getX() + 18; cy += 48; }
        const auto r = juce::Rectangle<float>(float(cx), float(cy), float(w), 40.0f);
        g.setColour(kGoldHi.withAlpha(0.07f));
        g.fillRoundedRectangle(r.expanded(3.0f), 13.0f);
        g.setColour(juce::Colour(0xff1a2028));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(kGold);
        g.setFont(chipFont);
        g.drawText(name, r.toNearestInt(), juce::Justification::centred);
        cx += w + 8;
    }

    // The tuner: with one note held by the one-note converter (not in Poly, where the bend is not followed) the bar shows the cents.
    const int modeNow = int(p_.apvts.getRawParameterValue(ids::mode)->load() + 0.5f);
    const bool tuner = sounding.size() == 1 && p_.voices() == 1 && modeNow != 1;
    const float cents = juce::jlimit(-50.0f, 50.0f, p_.centsNow());
    const int ty = area.getY() + 176;
    caption(g, tr("TUNER (ONE NOTE)"), {area.getX() + 18, ty, area.getWidth() - 36, 16});
    const auto bar = juce::Rectangle<float>(float(area.getX() + 18), float(ty + 26), float(area.getWidth() - 36), 6.0f);
    g.setColour(juce::Colour(0xff1a2028));
    g.fillRoundedRectangle(bar, 3.0f);
    g.setColour(juce::Colour(0xff2f3944));
    g.fillRect(bar.getCentreX() - 0.5f, bar.getY() - 4.0f, 1.0f, 14.0f);
    if (tuner) {
        const bool inTune = std::abs(cents) < 8.0f;
        const float x = bar.getCentreX() + cents / 50.0f * (bar.getWidth() * 0.5f);
        g.setColour(inTune ? kGreen : kGold);
        g.fillRoundedRectangle(x - 2.0f, bar.getY() - 4.0f, 4.0f, 14.0f, 2.0f);
    }
    g.setFont(font(12.0f));
    g.setColour(kMuted);
    g.drawText("-50", area.getX() + 18, ty + 38, 60, 16, juce::Justification::centredLeft);
    g.drawText("+50", area.getRight() - 78, ty + 38, 60, 16, juce::Justification::centredRight);
    if (tuner) {
        g.setColour(std::abs(cents) < 8.0f ? kGreen : kGold);
        g.drawText(std::abs(cents) < 8.0f ? tr("in tune") : (cents > 0.5f ? "+" : "") + juce::String(int(std::lround(cents))) + " ct", area.getX() + 78, ty + 38, area.getWidth() - 156, 16,
                   juce::Justification::centred);
    }

    // The input level: twelve segments from -60 to 0 dB and a mark where the sensitivity is.
    const int my = area.getY() + 248;
    caption(g, tr("INPUT LEVEL"), {area.getX() + 18, my, area.getWidth() - 36, 16});
    const int segs = 12;
    const float sw = (float(area.getWidth() - 36) - 3.0f * float(segs - 1)) / float(segs);
    const float sens = p_.apvts.getRawParameterValue(ids::sens)->load();
    for (int i = 0; i < segs; ++i) {
        const float db = -60.0f + 5.0f * float(i + 1);
        const bool lit = levelShown_ >= db - 2.5f;
        g.setColour(!lit ? juce::Colour(0xff1a2028) : (db > -10.0f ? juce::Colour(0xffe5645a) : (db > -25.0f ? kGold : kGreen)));
        g.fillRoundedRectangle(float(area.getX() + 18) + float(i) * (sw + 3.0f), float(my + 24), sw, 8.0f, 2.5f);
    }
    const float sx = float(area.getX() + 18) + juce::jlimit(0.0f, 1.0f, (sens + 60.0f) / 60.0f) * float(area.getWidth() - 36);
    g.setColour(kText);
    g.fillRect(sx - 1.0f, float(my + 20), 2.0f, 16.0f);
}

void StrumlineView::drawRoll(juce::Graphics& g, juce::Rectangle<int> area) {
    card(g, area);
    caption(g, tr("NOTES  (LAST 12 SECONDS)"), {area.getX() + 18, area.getY() + 14, 300, 16});
    caption(g, tr("PITCH BEND"), {area.getRight() - 218, area.getY() + 14, 200, 16}, juce::Justification::centredRight);

    const auto bendBox = juce::Rectangle<float>(float(area.getX() + 16), float(area.getBottom() - 16 - 46), float(area.getWidth() - 32), 46.0f);
    const auto box = juce::Rectangle<float>(float(area.getX() + 16), float(area.getY() + 40), float(area.getWidth() - 32), bendBox.getY() - 10.0f - float(area.getY() + 40));
    g.setColour(kWell);
    g.fillRoundedRectangle(box, 9.0f);
    g.fillRoundedRectangle(bendBox, 9.0f);

    // The range of the roll follows what was played (at least two octaves), easing from one range to the next.
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
    const float rh = box.getHeight() / float(rows), x0 = box.getX(), w = box.getWidth();
    auto yOf = [&](int m) { return box.getBottom() - float(m - low + 1) * rh; };
    {
        juce::Graphics::ScopedSaveState keep(g);
        g.reduceClipRegion(box.toNearestInt());
        for (int m = low; m <= high; ++m) {
            if (m % 12 != 0) continue;  // a line at every C, to read the heights
            g.setColour(kWellLine.brighter(0.25f));
            g.fillRect(x0, yOf(m) + rh, w, 1.0f);
            g.setColour(kFaint);
            g.setFont(font(10.0f));
            g.drawText("C" + juce::String(m / 12 - 1), int(x0 + 8.0f), int(yOf(m) + rh - 14.0f), 30, 12, juce::Justification::centredLeft);
        }
        const double span = 12.0;
        const juce::Colour soft(0xff9a7a2a);
        for (const auto& n : notes_) {
            if (n.midi < low || n.midi > high) continue;
            const double t0 = n.start, t1 = n.end < 0.0 ? now_ : n.end;
            const float xa = std::max(x0, x0 + w * float(1.0 - (now_ - t0) / span)), xb = x0 + w * float(1.0 - (now_ - t1) / span);
            const float age = float(juce::jlimit(0.0, 1.0, (now_ - t1) / span));
            const float hard = juce::jlimit(0.0f, 1.0f, float(n.velocity - 20) / 100.0f);
            const auto r = juce::Rectangle<float>(xa, yOf(n.midi) + 0.5f, std::max(4.0f, xb - xa - 1.0f), std::max(4.0f, std::min(14.0f, rh - 1.5f)));
            const auto c = soft.interpolatedWith(kGold, hard).withAlpha(1.0f - 0.5f * age);
            g.setColour(kGoldHi.withAlpha(0.16f * (1.0f - age)));  // the soft glow
            g.fillRoundedRectangle(r.expanded(2.5f, 2.5f), r.getHeight() * 0.5f + 2.0f);
            g.setColour(c);
            g.fillRoundedRectangle(r, std::min(4.0f, r.getHeight() * 0.5f));
        }
        g.setColour(kText.withAlpha(0.8f));  // "now" at the right edge
        g.fillRect(x0 + w - 2.0f, box.getY(), 1.5f, box.getHeight());
    }

    // The pitch bend lane: what is sent (the pitch wheel, two semitones each way), a line that holds its value until the next one.
    g.setColour(kWellLine.brighter(0.4f));
    g.fillRect(bendBox.getX(), bendBox.getCentreY(), bendBox.getWidth(), 1.0f);
    if (!bends_.empty()) {
        auto yBend = [&](int v) { return bendBox.getCentreY() - float(v - 8192) / 8191.0f * (bendBox.getHeight() * 0.5f - 5.0f); };
        juce::Path line;
        float lastY = bendBox.getCentreY();
        line.startNewSubPath(bendBox.getX(), lastY);
        for (const auto& b : bends_) {
            const float x = std::max(bendBox.getX(), bendBox.getX() + bendBox.getWidth() * float(1.0 - (now_ - b.at) / 12.0));
            line.lineTo(x, lastY);
            lastY = yBend(b.value);
            line.lineTo(x, lastY);
        }
        line.lineTo(bendBox.getRight(), lastY);
        g.setColour(kGold);
        g.strokePath(line, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved));
    }

    // ONNX Runtime is missing: Poly and Hybrid play as Mono. A strip over the roll, not a line of small print.
    if (p_.apvts.getRawParameterValue(ids::mode)->load() > 0.5f && !p_.chordsAvailable()) {
        const auto strip = juce::Rectangle<float>(box.getX() + 40.0f, box.getY() + 12.0f, box.getWidth() - 80.0f, 26.0f);
        g.setColour(juce::Colour(0xf0201a08));
        g.fillRoundedRectangle(strip, 8.0f);
        g.setColour(kGold);
        g.drawRoundedRectangle(strip, 8.0f, 1.2f);
        g.setFont(font(12.0f, true));
        g.drawText(tr("CHORD MODEL NOT LOADED (ONNX Runtime is missing): ONE NOTE AT A TIME"), strip.toNearestInt(), juce::Justification::centred);
    }
}

// A linear slider's label on the left and its value on the right, above the track.
void StrumlineView::field(juce::Graphics& g, juce::Slider& s, const juce::String& label) {
    const auto b = s.getBounds();
    g.setFont(font(13.0f));
    g.setColour(kSecondary);
    g.drawText(label, b.getX() + 6, b.getY() - 24, b.getWidth() - 80, 18, juce::Justification::centredLeft);
    g.setColour(kText);
    g.setFont(font(13.0f, true));
    g.drawText(s.getTextFromValue(s.getValue()), b.getRight() - 90, b.getY() - 24, 84, 18, juce::Justification::centredRight);
}

void StrumlineView::drawControls(juce::Graphics& g) {
    const auto det = detectionArea(), out = outputArea(), mon = monitorArea();
    for (const auto& r : {det, out, mon}) card(g, r);
    caption(g, tr("DETECTION"), {det.getX() + 18, det.getY() + 14, 200, 16});
    caption(g, tr("OUTPUT"), {out.getX() + 18, out.getY() + 14, 200, 16});
    caption(g, tr("MONITOR SYNTH"), {mon.getX() + 18, mon.getY() + 14, 160, 16});

    g.setFont(font(13.0f));
    g.setColour(kSecondary);
    for (auto* k : {&sens_, &lookahead_}) {
        const auto b = k->getBounds();
        g.drawText(k == &sens_ ? tr("Sensitivity") : tr("Look-ahead"), b.getX(), b.getBottom() - 4, b.getWidth(), 18, juce::Justification::centred);
    }
    // Marks on the look-ahead knob at 35, 45 and 60 ms (the values that were measured; the tooltip says so).
    {
        const auto kb = lookahead_.getBounds();
        const float cx = float(kb.getCentreX()), cy = float(kb.getY()) + 41.0f;
        g.setColour(kFaint);
        for (const float ms : {35.0f, 45.0f, 60.0f}) {
            const float a = juce::MathConstants<float>::pi * (1.25f + 1.5f * ms / 120.0f), s = std::sin(a), c = std::cos(a);
            g.drawLine(cx + s * 40.0f, cy - c * 40.0f, cx + s * 44.0f, cy - c * 44.0f, 1.5f);
        }
    }
    field(g, channel_, tr("MIDI channel"));
    field(g, transpose_, tr("Transpose"));
    field(g, level_, tr("Level"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------- editor

StrumlineEditor::StrumlineEditor(StrumlineProcessor& p) : AudioProcessorEditor(p), view_(p) {
    setLookAndFeel(&view_.look());
    addAndMakeVisible(view_);
    setResizable(true, true);
    setResizeLimits(700, 448, 2000, 1280);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(double(kWidth) / double(kHeight));
    setSize(kWidth, kHeight);
}

void StrumlineEditor::resized() {
    view_.setTransform(juce::AffineTransform::scale(float(getWidth()) / float(kWidth)));
    view_.setBounds(0, 0, kWidth, kHeight);
}

// The grip in the corner of the window: three small dots on a diagonal.
void StrumlineLook::drawCornerResizer(juce::Graphics& g, int width, int height, bool mouseOver, bool dragging) {
    g.setColour((mouseOver || dragging) ? kGold : kFaint);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j <= i; ++j) g.fillEllipse(float(width) - 5.0f - float(i) * 4.5f, float(height) - 5.0f - float(j) * 4.5f, 2.2f, 2.2f);
}
