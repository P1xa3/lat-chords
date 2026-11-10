// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>

class LtaChordsEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer {
public:
    explicit LtaChordsEditor(LtaChordsProcessor&);
    ~LtaChordsEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void refresh();
    void startMidiDrag();

private:
    class FlatLookAndFeel;
    class PianoRoll;
    class MidiDragArea;
    LtaChordsProcessor& processor;
    std::unique_ptr<FlatLookAndFeel> theme;
    std::unique_ptr<PianoRoll> piano;
    std::unique_ptr<MidiDragArea> dragArea;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Viewport viewport;
    juce::ComboBox keySelect, modeSelect, barSelect, styleSelect, complexitySelect, voicingSelect;
    juce::TextButton generate, reroll, inversion, shuffle, transposeDown, transposeUp,
        undoBtn, redoBtn, previewBtn, saveBtn, loadBtn, exportBtn;
    juce::Label title, subtitle, status, tips;
    int selectedBar = 0;
    std::array<juce::Rectangle<int>, 6> comboLabelBounds;
    void timerCallback() override;
    void setCombo(juce::ComboBox&, const juce::StringArray&, int initial = 1);
    void syncControls();
    void settingChanged();
    void chooseExport();
    void choosePreset(bool save);
    void showStatus(const juce::String&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LtaChordsEditor)
};
