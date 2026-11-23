// strumline_shot <file.png>: draws the editor after six seconds of the built-in test script, to see the interface without opening a host.
#include <cmath>

#include <juce_audio_utils/juce_audio_utils.h>

#include "StrumlineEditor.h"
#include "StrumlineProcessor.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "interface.png");

    StrumlineProcessor p;
    p.setPlayConfigDetails(2, 2, 48000.0, 512);
    p.prepareToPlay(48000.0, 512);
    p.setTestRunning(true);
    std::unique_ptr<juce::AudioProcessorEditor> base(p.createEditor());
    auto* editor = dynamic_cast<StrumlineEditor*>(base.get());
    editor->setSize(StrumlineEditor::kWidth, StrumlineEditor::kHeight);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    double t = 0.0;
    int phase = 0;
    for (int block = 0; block < int(6.0 * 48000.0 / 512.0); ++block) {
        for (int i = 0; i < 512; ++i, ++phase) {  // a guitar-like signal for the level meter
            const float s = 0.12f * std::sin(2.0f * 3.14159265f * 110.0f * float(phase) / 48000.0f);
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        p.processBlock(buffer, midi);
        t += 512.0 / 48000.0;
        editor->updateFromProcessor(t);
    }
    const juce::Image shot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk()) return 1;
    png.writeImageToStream(shot, stream);
    return 0;
}
