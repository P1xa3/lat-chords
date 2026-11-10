// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ChordEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>

static bool check(bool condition, const char* explanation) {
    if (!condition) std::cerr << "FAIL: " << explanation << '\n';
    return condition;
}

int main() {
    bool ok = true;
    lta::ChordEngine chords(12345);

    for (int bars : {4, 8, 16}) {
        chords.settings.bars = bars;
        for (int complexity = 0; complexity < 3; ++complexity) {
            chords.settings.complexity = complexity;
            chords.settings.style = 2; // Soul must respect triad/seventh/ninth selection.
            chords.generate();
            ok &= check(static_cast<int>(chords.chords.size()) == bars, "bar count");
            ok &= check(static_cast<int>(chords.notes.size()) == bars * (3 + complexity), "chord richness");
            for (const auto& note : chords.notes) {
                ok &= check(note.pitch >= 0 && note.pitch <= 127, "MIDI note bounds");
                ok &= check(std::isfinite(note.start) && note.start >= 0.0 && note.start < bars * 4.0, "note start bounds");
                ok &= check(note.length >= 0.25 && note.length <= 16.0, "note length bounds");
            }
        }
    }

    chords.settings.bars = 8;
    chords.settings.complexity = 0;
    chords.generate();
    const auto previousName = chords.chords[0].name;
    chords.transpose(2);
    ok &= check(chords.settings.key == 2, "transpose updates key");
    ok &= check(chords.chords[0].name != previousName, "transpose updates chord names");
    chords.regenerateBar(3);
    ok &= check(chords.chords.size() == 8, "regeneration preserves chord count");
    chords.shuffle();
    ok &= check(chords.chords.size() == 8, "shuffle preserves chord count");
    const int index = chords.addNote(60, 1.0, 2.0);
    ok &= check(index >= 0 && index < static_cast<int>(chords.notes.size()), "add note index");
    chords.editNote(index, 64, 2.0, 1.0);
    ok &= check(chords.notes[static_cast<size_t>(index)].pitch == 64, "edit note");
    const size_t count = chords.notes.size();
    chords.deleteNote(index);
    ok &= check(chords.notes.size() + 1 == count, "delete note");

    if (ok) std::cout << "All chord-engine tests passed\n";
    return ok ? 0 : 1;
}
