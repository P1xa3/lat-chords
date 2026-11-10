// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <JuceHeader.h>
#include "ChordEngine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class LtaChordsProcessor final : public juce::AudioProcessor {
public:
    LtaChordsProcessor();
    ~LtaChordsProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.45; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current progression"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    lta::ChordEngine model() const;
    void modify(const std::function<void(lta::ChordEngine&)>&, bool saveHistory = true);
    void pushUndo();
    bool undo();
    bool redo();
    bool exportMidi(const juce::File& destination) const;
    bool savePreset(const juce::File& destination);
    bool loadPreset(const juce::File& source);
    void setPreview(bool enabled) { previewEnabled.store(enabled); }
    bool preview() const { return previewEnabled.load(); }
    double beat() const { return displayedBeat.load(); }
    double bpm() const { return displayedBpm.load(); }
    void restartPreview() { previewReset.store(true); }

private:
    struct Voice {
        int pitch = -1;
        double phase = 0;
        double step = 0;
        float envelope = 0;
        float velocity = 0;
        bool releasing = false;
        uint64_t age = 0;
    };
    mutable juce::CriticalSection modelMutex;
    lta::ChordEngine engine;
    std::vector<lta::ChordEngine> undoStack, redoStack;
    std::shared_ptr<const std::vector<lta::Note>> publishedNotes;
    std::atomic<uint64_t> revision{0};
    std::atomic<int> modelBars{8};
    std::atomic<bool> previewEnabled{false}, previewReset{false};
    std::atomic<double> displayedBeat{0.0}, displayedBpm{120.0};
    std::array<Voice, 32> voices{};
    double sampleRateHz = 44100.0, previewBeat = 0.0, previousEndBeat = 0.0;
    bool previouslyPlaying = false;
    uint64_t lastRevision = 0, voiceCounter = 0;

    void publish(); // must be called with modelMutex held
    void noteOn(int pitch, int velocity);
    void noteOff(int pitch);
    void allNotesOff();
    float nextAudioSample();
    static void serialize(const lta::ChordEngine&, juce::XmlElement&);
    static bool deserialize(const juce::XmlElement&, lta::ChordEngine&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LtaChordsProcessor)
};
