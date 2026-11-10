// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

LtaChordsProcessor::LtaChordsProcessor()
  : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
        .withOutput("Synth output", juce::AudioChannelSet::stereo(), true)) {
    const juce::ScopedLock lock(modelMutex);
    publish();
}
void LtaChordsProcessor::prepareToPlay(double rate, int) {
    sampleRateHz = std::max(8000.0, rate);
    for (auto& v : voices) v = {};
    previewBeat = 0;
    previouslyPlaying = false;
    previousEndBeat = 0;
}
void LtaChordsProcessor::releaseResources() {
    for (auto& v : voices) v = {};
}
bool LtaChordsProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}
void LtaChordsProcessor::publish() {
    std::shared_ptr<const std::vector<lta::Note>> copy =
        std::make_shared<const std::vector<lta::Note>>(engine.notes);
    std::atomic_store(&publishedNotes, std::move(copy));
    modelBars.store(engine.settings.bars);
    revision.fetch_add(1);
}
lta::ChordEngine LtaChordsProcessor::model() const {
    const juce::ScopedLock lock(modelMutex);
    return engine;
}
void LtaChordsProcessor::pushUndo() {
    const juce::ScopedLock lock(modelMutex);
    undoStack.push_back(engine);
    if (undoStack.size() > 50) undoStack.erase(undoStack.begin());
    redoStack.clear();
}
void LtaChordsProcessor::modify(const std::function<void(lta::ChordEngine&)>& f, bool saveHistory) {
    const juce::ScopedLock lock(modelMutex);
    if (saveHistory) {
        undoStack.push_back(engine);
        if (undoStack.size() > 50) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    f(engine);
    publish();
}
bool LtaChordsProcessor::undo() {
    const juce::ScopedLock lock(modelMutex);
    if (undoStack.empty()) return false;
    redoStack.push_back(engine);
    engine = undoStack.back();
    undoStack.pop_back();
    publish();
    return true;
}
bool LtaChordsProcessor::redo() {
    const juce::ScopedLock lock(modelMutex);
    if (redoStack.empty()) return false;
    undoStack.push_back(engine);
    engine = redoStack.back();
    redoStack.pop_back();
    publish();
    return true;
}
void LtaChordsProcessor::noteOn(int pitch, int velocity) {
    Voice* target = nullptr;
    for (auto& v : voices) if (v.pitch == -1) { target = &v; break; }
    if (!target) target = &*std::min_element(voices.begin(), voices.end(), [](const Voice& a, const Voice& b) {
        return a.releasing != b.releasing ? a.releasing : a.age < b.age;
    });
    target->pitch = pitch;
    target->phase = 0.0;
    const double f = 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
    target->step = juce::MathConstants<double>::twoPi * f / sampleRateHz;
    target->velocity = velocity / 127.0f;
    target->envelope = 0.0f;
    target->releasing = false;
    target->age = ++voiceCounter;
}
void LtaChordsProcessor::noteOff(int pitch) {
    for (auto& v : voices) if (v.pitch == pitch) v.releasing = true;
}
void LtaChordsProcessor::allNotesOff() {
    for (auto& v : voices) if (v.pitch >= 0) v.releasing = true;
}
float LtaChordsProcessor::nextAudioSample() {
    float mix = 0.0f;
    for (auto& v : voices) {
        if (v.pitch < 0) continue;
        if (v.releasing) v.envelope *= 0.9994f;
        else v.envelope += (1.0f - v.envelope) * 0.004f;
        if (v.envelope < 0.00008f && v.releasing) { v.pitch = -1; continue; }
        // Original, low-CPU warm electric-key preview; no copyrighted samples.
        const float sample = static_cast<float>(std::sin(v.phase) * 0.68
                       + std::sin(2.0 * v.phase) * 0.22
                       + std::sin(3.0 * v.phase) * 0.10);
        mix += sample * v.envelope * v.velocity * 0.080f;
        v.phase += v.step;
        if (v.phase >= juce::MathConstants<double>::twoPi) v.phase -= juce::MathConstants<double>::twoPi;
    }
    return std::tanh(mix * 1.25f) * 0.8f;
}
void LtaChordsProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int samples = audio.getNumSamples();
    audio.clear();
    if (samples <= 0) return;
    double tempo = 120.0;
    double start = 0.0;
    bool hostPlaying = false;
    if (auto* playHead = getPlayHead()) {
        if (const auto pos = playHead->getPosition()) {
            tempo = pos->getBpm().orFallback(120.0);
            hostPlaying = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
            if (hostPlaying) start = *pos->getPpqPosition();
        }
    }
    tempo = juce::jlimit(20.0, 400.0, tempo);
    displayedBpm.store(tempo, std::memory_order_relaxed);
    const double beatsPerSample = tempo / (60.0 * sampleRateHz);
    const bool previewPlaying = previewEnabled.load() && !hostPlaying;
    const bool playing = hostPlaying || previewPlaying;
    if (previewReset.exchange(false)) previewBeat = 0.0;
    if (previewPlaying) start = previewBeat;
    const double lastBeatInBlock = start + samples * beatsPerSample;

    auto pattern = std::atomic_load(&publishedNotes);
    const auto currentRevision = revision.load();
    const bool jumped = previouslyPlaying && playing
        && std::abs(start - previousEndBeat) > (beatsPerSample * 8.0 + 0.02);
    if ((previouslyPlaying && !playing) || jumped || (lastRevision != currentRevision && previouslyPlaying)) {
        allNotesOff();
        midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    }
    lastRevision = currentRevision;
    if (playing && pattern && !pattern->empty()) {
        // The loop is the pattern's full length. Editor changes reset timing cleanly.
        const auto patternModel = modelBars.load(std::memory_order_relaxed);
        const double loopLength = static_cast<double>(patternModel * 4);
        const int cycleFirst = static_cast<int>(std::floor(start / loopLength)) - 1;
        const int cycleLast = static_cast<int>(std::floor(lastBeatInBlock / loopLength)) + 1;
        for (int cycle = cycleFirst; cycle <= cycleLast; ++cycle) {
            const double offset = cycle * loopLength;
            for (const auto& n : *pattern) {
                const double t0 = offset + n.start;
                const double t1 = t0 + n.length;
                if (t0 >= start && t0 < lastBeatInBlock) {
                    const int idx = juce::jlimit(0, samples - 1, static_cast<int>((t0 - start) / beatsPerSample));
                    midi.addEvent(juce::MidiMessage::noteOn(1, n.pitch, static_cast<juce::uint8>(n.velocity)), idx);
                }
                if (t1 >= start && t1 < lastBeatInBlock) {
                    const int idx = juce::jlimit(0, samples - 1, static_cast<int>((t1 - start) / beatsPerSample));
                    midi.addEvent(juce::MidiMessage::noteOff(1, n.pitch), idx);
                }
            }
        }
    }
    // Apply MIDI to the embedded synth exactly at sample offsets.
    auto iterator = midi.begin();
    const auto midiEnd = midi.end();
    auto* left = audio.getWritePointer(0);
    auto* right = audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr;
    for (int i = 0; i < samples; ++i) {
        while (iterator != midiEnd && (*iterator).samplePosition <= i) {
            const auto m = (*iterator).getMessage();
            if (m.isNoteOn()) noteOn(m.getNoteNumber(), m.getVelocity());
            if (m.isNoteOff()) noteOff(m.getNoteNumber());
            if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
            ++iterator;
        }
        const float s = nextAudioSample();
        left[i] = s;
        if (right != nullptr) right[i] = s;
    }
    if (previewPlaying) previewBeat = lastBeatInBlock;
    displayedBeat.store(playing ? start : 0.0, std::memory_order_relaxed);
    previousEndBeat = lastBeatInBlock;
    previouslyPlaying = playing;
}
void LtaChordsProcessor::serialize(const lta::ChordEngine& e, juce::XmlElement& xml) {
    xml.setAttribute("version", 1);
    xml.setAttribute("key", e.settings.key);
    xml.setAttribute("minor", e.settings.minor);
    xml.setAttribute("bars", e.settings.bars);
    xml.setAttribute("style", e.settings.style);
    xml.setAttribute("complexity", e.settings.complexity);
    xml.setAttribute("inversion", e.settings.inversion);
    xml.setAttribute("octave", e.settings.octave);
    for (const auto& ch : e.chords) {
        auto* item = xml.createNewChildElement("chord");
        item->setAttribute("degree", ch.degree);
        item->setAttribute("name", juce::String(ch.name));
    }
    for (const auto& n : e.notes) {
        auto* item = xml.createNewChildElement("note");
        item->setAttribute("pitch", n.pitch);
        item->setAttribute("start", n.start);
        item->setAttribute("length", n.length);
        item->setAttribute("velocity", n.velocity);
    }
}
bool LtaChordsProcessor::deserialize(const juce::XmlElement& xml, lta::ChordEngine& dst) {
    if (!xml.hasTagName("LTA_CHORDS")) return false;
    dst.settings.key = juce::jlimit(0, 11, xml.getIntAttribute("key", 0));
    dst.settings.minor = xml.getBoolAttribute("minor", true);
    const int bars = xml.getIntAttribute("bars", 8);
    dst.settings.bars = bars == 4 || bars == 8 || bars == 16 ? bars : 8;
    dst.settings.style = juce::jlimit(0, 5, xml.getIntAttribute("style", 0));
    dst.settings.complexity = juce::jlimit(0, 2, xml.getIntAttribute("complexity", 2));
    dst.settings.inversion = juce::jlimit(0, 2, xml.getIntAttribute("inversion", 1));
    dst.settings.octave = juce::jlimit(2, 6, xml.getIntAttribute("octave", 4));
    dst.notes.clear(); dst.chords.clear();
    for (const auto* child = xml.getFirstChildElement(); child != nullptr; child = child->getNextElement()) {
        if (child->hasTagName("note") && dst.notes.size() < 1024) {
            lta::Note n;
            n.pitch = juce::jlimit(0, 127, child->getIntAttribute("pitch", 60));
            n.start = child->getDoubleAttribute("start", 0.0);
            n.length = child->getDoubleAttribute("length", 3.9);
            n.velocity = juce::jlimit(1, 127, child->getIntAttribute("velocity", 92));
            if (std::isfinite(n.start) && std::isfinite(n.length) && n.start >= 0
                && n.start < dst.settings.bars * 4 && n.length >= 0.25 && n.length <= 16.0)
                dst.notes.push_back(n);
        } else if (child->hasTagName("chord") && dst.chords.size() < static_cast<size_t>(dst.settings.bars)) {
            const int degree = juce::jlimit(0, 6, child->getIntAttribute("degree", 0));
            dst.chords.push_back({degree, dst.chordName(degree)});
        }
    }
    while (dst.chords.size() < static_cast<size_t>(dst.settings.bars)) dst.chords.push_back({0, dst.chordName(0)});
    return true;
}
void LtaChordsProcessor::getStateInformation(juce::MemoryBlock& destination) {
    juce::XmlElement xml("LTA_CHORDS");
    serialize(model(), xml);
    juce::AudioProcessor::copyXmlToBinary(xml, destination);
}
void LtaChordsProcessor::setStateInformation(const void* data, int bytes) {
    const auto xml = juce::AudioProcessor::getXmlFromBinary(data, bytes);
    if (!xml) return;
    const juce::ScopedLock lock(modelMutex);
    if (deserialize(*xml, engine)) {
        undoStack.clear(); redoStack.clear();
        modelBars.store(engine.settings.bars);
        publish();
    }
}
bool LtaChordsProcessor::savePreset(const juce::File& destination) {
    juce::MemoryBlock state;
    getStateInformation(state);
    return destination.replaceWithData(state.getData(), state.getSize());
}
bool LtaChordsProcessor::loadPreset(const juce::File& source) {
    juce::MemoryBlock state;
    if (!source.loadFileAsData(state)) return false;
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (!xml || !xml->hasTagName("LTA_CHORDS")) return false;
    setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    return true;
}
bool LtaChordsProcessor::exportMidi(const juce::File& destination) const {
    const auto e = model();
    juce::MidiMessageSequence track;
    auto add = [&](juce::MidiMessage msg, double beats) {
        msg.setTimeStamp(std::round(beats * 960.0));
        track.addEvent(msg);
    };
    add(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
    for (const auto& n : e.notes) {
        add(juce::MidiMessage::noteOn(1, n.pitch, static_cast<juce::uint8>(n.velocity)), n.start);
        add(juce::MidiMessage::noteOff(1, n.pitch), n.start + n.length);
    }
    track.sort();
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(960);
    file.addTrack(track);
    destination.deleteFile();
    juce::FileOutputStream stream(destination);
    return stream.openedOk() && file.writeTo(stream, 1);
}
juce::AudioProcessorEditor* LtaChordsProcessor::createEditor() { return new LtaChordsEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LtaChordsProcessor(); }
