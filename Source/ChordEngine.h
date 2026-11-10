// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace lta {
struct Note {
    int pitch = 60;
    double start = 0.0;   // quarter-note beats
    double length = 3.85;
    int velocity = 96;
};
struct BarChord {
    int degree = 0;
    std::string name;
};
struct Settings {
    int key = 0;             // C = 0, C# = 1, ... B = 11
    bool minor = true;
    int bars = 8;            // 4, 8, 16
    int style = 0;           // 0 dream, 1 dark, 2 soul, 3 classic, 4 cinematic, 5 rage
    int complexity = 2;      // 0 triad, 1 sevenths, 2 ninths
    int inversion = 1;       // 0 root voicing, 1 smooth voice leading, 2 open
    int octave = 4;          // root range center
};
class ChordEngine {
public:
    Settings settings;
    std::vector<Note> notes;
    std::vector<BarChord> chords;

    explicit ChordEngine(uint32_t seed = std::random_device{}()) : rng(seed) { generate(); }
    static const char* keyName(int key) {
        static constexpr const char* names[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
        return names[(key % 12 + 12) % 12];
    }
    void generate() {
        notes.clear(); chords.clear();
        const auto pattern = choosePattern();
        std::vector<int> previous;
        for (int bar = 0; bar < settings.bars; ++bar) {
            int degree = pattern[static_cast<size_t>(bar) % pattern.size()];
            if (bar >= 4 && chance(17)) degree = weightedDegree();
            auto voice = makeChord(degree, previous);
            appendChord(bar, degree, voice);
            previous = voice;
        }
    }
    void regenerateBar(int bar) {
        if (bar < 0 || bar >= settings.bars) return;
        const auto previous = bar > 0 ? notesForBar(bar - 1) : std::vector<int>{};
        const int degree = weightedDegree();
        const auto voice = makeChord(degree, previous);
        eraseBar(bar);
        appendChord(bar, degree, voice);
        sortNotes();
    }
    void invertBar(int bar) {
        auto indices = indicesForBar(bar);
        if (indices.empty()) return;
        int lowIndex = indices.front();
        for (const auto i : indices)
            if (notes[static_cast<size_t>(i)].pitch < notes[static_cast<size_t>(lowIndex)].pitch) lowIndex = i;
        notes[static_cast<size_t>(lowIndex)].pitch = std::min(96, notes[static_cast<size_t>(lowIndex)].pitch + 12);
        sortNotes();
    }
    void shuffle() {
        if (settings.bars <= 1) return;
        auto oldNotes = notes;
        auto oldChords = chords;
        std::vector<int> order(static_cast<size_t>(settings.bars));
        for (int i = 0; i < settings.bars; ++i) order[static_cast<size_t>(i)] = i;
        std::shuffle(order.begin(), order.end(), rng);
        notes.clear();
        chords.assign(static_cast<size_t>(settings.bars), {});
        for (int newBar = 0; newBar < settings.bars; ++newBar) {
            const int oldBar = order[static_cast<size_t>(newBar)];
            if (oldBar < static_cast<int>(oldChords.size())) chords[static_cast<size_t>(newBar)] = oldChords[static_cast<size_t>(oldBar)];
            for (auto n : oldNotes) {
                if (static_cast<int>(std::floor(n.start / 4.0)) == oldBar) {
                    n.start += 4.0 * static_cast<double>(newBar - oldBar);
                    notes.push_back(n);
                }
            }
        }
        sortNotes();
    }
    void transpose(int semitones) {
        for (auto& n : notes) n.pitch = std::clamp(n.pitch + semitones, 0, 127);
        settings.key = (settings.key + semitones % 12 + 12) % 12;
        for (auto& ch : chords) ch.name = chordName(ch.degree);
    }
    int addNote(int pitch, double start, double length, int velocity = 92) {
        notes.push_back({std::clamp(pitch, 0, 127), std::clamp(start, 0.0, settings.bars * 4.0 - 0.25),
                         std::clamp(length, 0.25, 16.0), std::clamp(velocity, 1, 127)});
        return static_cast<int>(notes.size()) - 1;
    }
    void editNote(int index, int pitch, double start, double length) {
        if (index < 0 || index >= static_cast<int>(notes.size())) return;
        auto& n = notes[static_cast<size_t>(index)];
        n.pitch = std::clamp(pitch, 0, 127);
        n.start = std::clamp(start, 0.0, settings.bars * 4.0 - 0.25);
        n.length = std::clamp(length, 0.25, 16.0);
    }
    void deleteNote(int index) {
        if (index >= 0 && index < static_cast<int>(notes.size())) notes.erase(notes.begin() + index);
    }
    std::string chordName(int degree) const {
        const auto& steps = settings.minor ? minorSteps : majorSteps;
        const int pc = (settings.key + steps[static_cast<size_t>(degree % 7)]) % 12;
        std::string root = keyName(pc);
        const int third = (steps[static_cast<size_t>((degree + 2) % 7)] + ((degree + 2) / 7) * 12 - steps[static_cast<size_t>(degree % 7)]);
        const int fifth = (steps[static_cast<size_t>((degree + 4) % 7)] + ((degree + 4) / 7) * 12 - steps[static_cast<size_t>(degree % 7)]);
        if (third == 3 && fifth == 6) return root + "dim";
        const bool isMinorChord = third == 3;
        root += isMinorChord ? "m" : "";
        if (settings.complexity > 0) {
            int seventh = steps[static_cast<size_t>((degree + 6) % 7)] + ((degree + 6) / 7) * 12 - steps[static_cast<size_t>(degree % 7)];
            root += seventh == 11 ? "maj7" : "7";
            if (settings.complexity > 1) root += "(9)";
        }
        return root;
    }
private:
    std::mt19937 rng;
    static constexpr std::array<int, 7> majorSteps{0, 2, 4, 5, 7, 9, 11};
    static constexpr std::array<int, 7> minorSteps{0, 2, 3, 5, 7, 8, 10};
    bool chance(int pct) { return std::uniform_int_distribution<int>(0, 99)(rng) < pct; }
    int randomInt(int low, int high) { return std::uniform_int_distribution<int>(low, high)(rng); }
    std::vector<int> choosePattern() {
        static const std::vector<std::vector<int>> minorPatterns = {
            {0, 5, 2, 6}, {0, 6, 5, 4}, {0, 3, 6, 5}, {0, 4, 5, 3},
            {0, 6, 3, 5}, {5, 3, 0, 6}, {0, 2, 5, 6}, {0, 5, 6, 4}};
        static const std::vector<std::vector<int>> majorPatterns = {
            {0, 4, 5, 3}, {0, 5, 3, 4}, {5, 3, 0, 4}, {0, 3, 4, 0},
            {0, 2, 5, 3}, {1, 4, 0, 5}, {0, 4, 3, 4}, {5, 4, 3, 0}};
        const auto& patterns = settings.minor ? minorPatterns : majorPatterns;
        auto pattern = patterns[static_cast<size_t>(randomInt(0, static_cast<int>(patterns.size()) - 1))];
        if (settings.style == 1 || settings.style == 5) { // darker palettes
            if (settings.minor && chance(60)) pattern = {0, 6, 5, 6};
        } else if (settings.style == 2 && chance(60)) pattern = settings.minor ? std::vector<int>{5, 3, 0, 6} : std::vector<int>{1, 4, 0, 5};
        else if (settings.style == 4 && chance(60)) pattern = settings.minor ? std::vector<int>{0, 3, 5, 6} : std::vector<int>{0, 5, 3, 4};
        return pattern;
    }
    int weightedDegree() {
        static constexpr std::array<int, 12> weights{0, 0, 0, 0, 3, 3, 4, 5, 5, 5, 6, 6};
        return weights[static_cast<size_t>(randomInt(0, 11))];
    }
    std::vector<int> makeChord(int degree, const std::vector<int>& previous) {
        const auto& scale = settings.minor ? minorSteps : majorSteps;
        auto scalePitch = [&](int d) {
            const int diatonicIndex = (degree + d) % 7;
            const int octaves = (degree + d) / 7;
            return 12 * (settings.octave + 1) + settings.key + scale[static_cast<size_t>(diatonicIndex)] + 12 * octaves;
        };
        std::vector<int> v{scalePitch(0), scalePitch(2), scalePitch(4)};
        int complexity = std::clamp(settings.complexity, 0, 2);
        // The selected richness must be honoured for every style.
        if (complexity >= 1) v.push_back(scalePitch(6));
        if (complexity >= 2 && settings.style != 5) v.push_back(scalePitch(8));
        // Keep a predictable usable keyboard register for a DAW piano roll.
        for (auto& p : v) { while (p > 79) p -= 12; while (p < 42) p += 12; }
        std::sort(v.begin(), v.end());
        if (settings.inversion == 2) {
            if (v.size() > 2) v[1] -= 12;
            for (auto& p : v) while (p < 40) p += 12;
            std::sort(v.begin(), v.end());
        } else if (settings.inversion == 1 && !previous.empty()) {
            // Voice leading: move each note by octaves towards previous voicing.
            for (size_t i = 0; i < v.size(); ++i) {
                int target = previous[std::min(i, previous.size() - 1)];
                int best = v[i]; double score = 1e9;
                for (int o = -2; o <= 2; ++o) {
                    const int candidate = v[i] + 12 * o;
                    if (candidate < 40 || candidate > 82) continue;
                    double s = std::abs(candidate - target) + std::abs(candidate - 60) * 0.1;
                    if (s < score) { score = s; best = candidate; }
                }
                v[i] = best;
            }
            std::sort(v.begin(), v.end());
            for (size_t i = 1; i < v.size(); ++i)
                while (v[i] <= v[i - 1]) v[i] += 12;
        }
        return v;
    }
    void appendChord(int bar, int degree, const std::vector<int>& voice) {
        if (static_cast<int>(chords.size()) <= bar) chords.resize(static_cast<size_t>(bar + 1));
        chords[static_cast<size_t>(bar)] = {degree, chordName(degree)};
        const double begin = 4.0 * bar;
        double length = settings.style == 5 ? 3.6 : 3.9;
        for (int p : voice) notes.push_back({std::clamp(p, 0, 127), begin, length, randomInt(83, 109)});
        sortNotes();
    }
    std::vector<int> notesForBar(int bar) const {
        std::vector<int> found;
        for (const auto& note : notes)
            if (static_cast<int>(note.start / 4.0) == bar) found.push_back(note.pitch);
        std::sort(found.begin(), found.end());
        return found;
    }
    std::vector<int> indicesForBar(int bar) const {
        std::vector<int> found;
        for (size_t i = 0; i < notes.size(); ++i)
            if (static_cast<int>(notes[i].start / 4.0) == bar) found.push_back(static_cast<int>(i));
        return found;
    }
    void eraseBar(int bar) {
        notes.erase(std::remove_if(notes.begin(), notes.end(), [bar](const Note& n) {
            return static_cast<int>(std::floor(n.start / 4.0)) == bar;
        }), notes.end());
    }
    void sortNotes() {
        std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b) {
            return a.start == b.start ? a.pitch < b.pitch : a.start < b.start;
        });
    }
};
} // namespace lta
