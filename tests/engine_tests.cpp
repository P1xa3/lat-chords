#include "../Source/ChordEngine.h"
#include <cassert>
#include <iostream>
#include <set>
int main() {
    for (int key = 0; key < 12; ++key) {
        for (bool minor : {false, true}) {
            for (int style = 0; style < 6; ++style) {
                lta::ChordEngine engine(123456);
                engine.settings.key = key;
                engine.settings.minor = minor;
                engine.settings.style = style;
                engine.settings.bars = 16;
                engine.generate();
                assert(engine.chords.size() == 16);
                assert(engine.notes.size() >= 16 * 3);
                for (const auto& note : engine.notes) {
                    assert(note.pitch >= 0 && note.pitch <= 127);
                    assert(note.start >= 0.0 && note.start < 64.0);
                    assert(note.length > 0.0);
                }
                const auto before = engine.notes.size();
                engine.regenerateBar(3);
                assert(engine.notes.size() >= before - 3);
                engine.invertBar(3);
                engine.shuffle();
                assert(engine.chords.size() == 16);
                engine.transpose(-2);
                const auto count = engine.notes.size();
                engine.addNote(60, 0, 1);
                assert(engine.notes.size() == count + 1);
                engine.deleteNote(static_cast<int>(engine.notes.size() - 1));
                assert(engine.notes.size() == count);
            }
        }
    }
    std::cout << "ChordEngine tests passed across keys, styles and modes.\n";
}
