// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour canvas{0xfffaf6f9};
const juce::Colour white{0xffffffff};
const juce::Colour ink{0xff322333};
const juce::Colour muted{0xff847685};
const juce::Colour rose{0xffe76ca5};
const juce::Colour pale{0xfff9dce9};
const juce::Colour line{0xffe9dfe5};
const juce::Colour blackKey{0xfff0eaf0};
constexpr int rollLeft = 62, barWidth = 128, chordTop = 29;
const juce::StringArray keyNames{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
}
class LtaChordsEditor::FlatLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    FlatLookAndFeel() {
        setColour(juce::ComboBox::backgroundColourId, white);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::arrowColourId, rose);
        setColour(juce::PopupMenu::backgroundColourId, white);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, pale);
        setColour(juce::PopupMenu::highlightedTextColourId, ink);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& bg,
                              bool highlighted, bool down) override {
        const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        auto colour = bg;
        if (down) colour = colour.darker(0.16f);
        else if (highlighted) colour = colour.interpolatedWith(ink, 0.07f);
        g.setColour(colour);
        g.fillRoundedRectangle(r, 9.0f);
        if (bg == white) {
            g.setColour(line);
            g.drawRoundedRectangle(r, 9.0f, 1.0f);
        }
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool,
                      int, int, int, int, juce::ComboBox&) override {
        g.setColour(white); g.fillRoundedRectangle(0.5f, 0.5f, (float)w - 1, (float)h - 1, 8.0f);
        g.setColour(line); g.drawRoundedRectangle(0.5f, 0.5f, (float)w - 1, (float)h - 1, 8.0f, 1.0f);
        juce::Path arrow;
        arrow.startNewSubPath((float)w - 19, (float)h / 2 - 2);
        arrow.lineTo((float)w - 15, (float)h / 2 + 2);
        arrow.lineTo((float)w - 11, (float)h / 2 - 2);
        g.setColour(rose); g.strokePath(arrow, juce::PathStrokeType(1.7f));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {
        return juce::Font(juce::FontOptions(14.0f).withStyle("Bold"));
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override {
        return juce::Font(juce::FontOptions(13.0f).withStyle("Bold"));
    }
};
class LtaChordsEditor::PianoRoll final : public juce::Component {
public:
    explicit PianoRoll(LtaChordsEditor& e) : owner(e) { setInterceptsMouseClicks(true, false); }
    void paint(juce::Graphics& g) override {
        auto m = owner.processor.model();
        g.fillAll(white);
        const int bars = m.settings.bars;
        const auto grid = getGridRect();
        // Bar headers
        for (int b = 0; b < bars; ++b) {
            const int x = rollLeft + b * barWidth;
            g.setColour(b == owner.selectedBar ? pale : (b % 2 ? juce::Colour(0xfffcf6f9) : white));
            g.fillRect(x, 0, barWidth, chordTop);
            g.setColour(rose);
            g.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
            if (b < static_cast<int>(m.chords.size()))
                g.drawFittedText(juce::String(b + 1) + "  " + m.chords[static_cast<size_t>(b)].name,
                                 x + 7, 1, barWidth - 10, 26, juce::Justification::centredLeft, 1);
        }
        g.setColour(canvas); g.fillRect(0, 0, rollLeft, chordTop);
        g.setColour(muted); g.setFont(11.0f);
        g.drawText("NOTE", 10, 0, rollLeft - 13, chordTop, juce::Justification::centredLeft);
        // Keyboard and note rows
        for (int p = highPitch; p >= lowPitch; --p) {
            const float y = yForPitch(p);
            const float h = rowHeight();
            const int pc = p % 12;
            const bool sharp = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            g.setColour(sharp ? blackKey : white);
            g.fillRect(grid.getX(), y, grid.getWidth(), h);
            g.setColour(line.withAlpha(sharp ? 0.55f : 0.45f));
            g.drawHorizontalLine(static_cast<int>(y + h), 0, static_cast<float>(getWidth()));
            g.setColour(sharp ? juce::Colour(0xffded2dc) : canvas);
            g.fillRect(0.0f, y, (float)rollLeft, h);
            if (pc == 0 || pc == 5) {
                g.setColour(muted);
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.drawFittedText(juce::MidiMessage::getMidiNoteName(p, true, true, 3),
                                 3, (int)y, rollLeft - 7, (int)h + 1, juce::Justification::centredLeft, 1);
            }
        }
        // Rhythm subdivisions
        for (int k = 0; k <= bars * 4; ++k) {
            float x = (float)rollLeft + (float)k * (barWidth / 4.0f);
            g.setColour(k % 4 == 0 ? juce::Colour(0xffdfcbd7) : line.withAlpha(0.8f));
            g.drawVerticalLine(static_cast<int>(x), (float)chordTop, (float)getHeight());
        }
        for (size_t i = 0; i < m.notes.size(); ++i) {
            const auto& n = m.notes[i];
            if (n.pitch < lowPitch || n.pitch > highPitch) continue;
            auto rect = rectangleFor(n);
            const bool picked = (int)i == selectedNote;
            g.setColour(picked ? juce::Colour(0xffb73b77) : rose);
            g.fillRoundedRectangle(rect, 2.8f);
            g.setColour(white.withAlpha(0.7f));
            g.fillRoundedRectangle(rect.withWidth(3.0f), 1.0f);
            if (rect.getWidth() > 32.0f) {
                g.setColour(ink);
                g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
                g.drawFittedText(juce::MidiMessage::getMidiNoteName(n.pitch, true, true, 3),
                      rect.toNearestInt().reduced(6, 0), juce::Justification::centredLeft, 1);
            }
        }
        // Playhead from host or local preview
        const double played = owner.processor.beat();
        if (played > 0.0 && bars > 0) {
            const auto x = static_cast<float>(rollLeft + std::fmod(played, bars * 4.0) * barWidth / 4.0);
            g.setColour(juce::Colour(0xff925275));
            g.fillRect(x, (float)chordTop, 2.0f, (float)getHeight() - chordTop);
        }
        g.setColour(line);
        g.drawRect(getLocalBounds(), 1);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int x = e.x, y = e.y;
        if (x < rollLeft) return;
        const auto m = owner.processor.model();
        const int bar = juce::jlimit(0, m.settings.bars - 1, (x - rollLeft) / barWidth);
        owner.selectedBar = bar;
        if (y < chordTop) {
            owner.processor.modify([bar](lta::ChordEngine& en) { en.regenerateBar(bar); });
            owner.showStatus("Chord " + juce::String(bar + 1) + " regenerated");
            owner.refresh(); return;
        }
        selectedNote = findNoteAt(e.position);
        if (selectedNote >= 0 && e.mods.isRightButtonDown()) {
            const int index = selectedNote;
            owner.processor.modify([index](lta::ChordEngine& en) { en.deleteNote(index); });
            selectedNote = -1;
            owner.refresh(); return;
        }
        if (selectedNote < 0 && !e.mods.isRightButtonDown()) {
            const int pitch = pitchAt(y);
            const double at = beatAt(x);
            owner.processor.modify([=](lta::ChordEngine& en) { en.addNote(pitch, at, 1.0); });
            selectedNote = static_cast<int>(owner.processor.model().notes.size()) - 1;
            owner.showStatus("Note added - drag to move, Alt-drag to resize");
        } else if (selectedNote >= 0) {
            owner.processor.pushUndo();
        }
        if (selectedNote >= 0) {
            const auto now = owner.processor.model();
            const auto& n = now.notes[static_cast<size_t>(selectedNote)];
            dragOriginBeat = n.start;
            dragOriginPitch = n.pitch;
            dragOriginLength = n.length;
            downX = x;
            downY = y;
            resizing = e.mods.isAltDown() || (rectangleFor(n).getRight() - x) < 9;
        }
        owner.refresh();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (selectedNote < 0 || !e.mods.isLeftButtonDown()) return;
        const int index = selectedNote;
        const double dx = std::round((e.x - downX) / 16.0) * 0.5;
        const int dp = static_cast<int>(std::round((downY - e.y) / rowHeight()));
        const double length = resizing ? std::max(0.25, dragOriginLength + dx) : dragOriginLength;
        const double start = resizing ? dragOriginBeat : dragOriginBeat + dx;
        const int pitch = resizing ? dragOriginPitch : dragOriginPitch + dp;
        owner.processor.modify([=](lta::ChordEngine& en) { en.editNote(index, pitch, start, length); }, false);
        repaint();
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const int index = findNoteAt(e.position);
        if (index >= 0) {
            owner.processor.modify([index](lta::ChordEngine& en) { en.deleteNote(index); });
            selectedNote = -1;
            owner.refresh();
        }
    }
    void clearSelection() { selectedNote = -1; }
    static constexpr int lowPitch = 36;
    static constexpr int highPitch = 88;
private:
    LtaChordsEditor& owner;
    int selectedNote = -1, downX = 0, downY = 0, dragOriginPitch = 60;
    bool resizing = false;
    double dragOriginBeat = 0, dragOriginLength = 1;
    float rowHeight() const { return (getHeight() - (float)chordTop) / (highPitch - lowPitch + 1); }
    float yForPitch(int p) const { return chordTop + (highPitch - p) * rowHeight(); }
    juce::Rectangle<float> getGridRect() const { return { (float)rollLeft, (float)chordTop,
        (float)(getWidth() - rollLeft), (float)(getHeight() - chordTop) }; }
    juce::Rectangle<float> rectangleFor(const lta::Note& n) const {
        return { rollLeft + static_cast<float>(n.start * barWidth / 4.0) + 1.0f,
                 yForPitch(n.pitch) + 0.4f,
                 std::max(4.0f, static_cast<float>(n.length * barWidth / 4.0) - 2.0f),
                 std::max(3.0f, rowHeight() - 0.8f) };
    }
    int pitchAt(int y) const { return juce::jlimit(lowPitch, highPitch,
        highPitch - static_cast<int>((y - chordTop) / rowHeight())); }
    double beatAt(int x) const { return std::max(0.0, std::floor((x - rollLeft) / 16.0) * 0.5); }
    // Avoid hiding juce::Component::hitTest(int, int).
    int findNoteAt(juce::Point<float> pt) const {
        const auto m = owner.processor.model();
        for (int i = (int)m.notes.size() - 1; i >= 0; --i)
            if (rectangleFor(m.notes[static_cast<size_t>(i)]).contains(pt)) return i;
        return -1;
    }
};
class LtaChordsEditor::MidiDragArea final : public juce::Component {
public:
    explicit MidiDragArea(LtaChordsEditor& e) : owner(e) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    void paint(juce::Graphics& g) override {
        g.setColour(pale); g.fillRoundedRectangle(getLocalBounds().toFloat(), 9.0f);
        g.setColour(rose); g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        g.drawFittedText("DRAG MIDI TO FL STUDIO", getLocalBounds(), juce::Justification::centred, 1);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!started && e.getDistanceFromDragStart() > 4) {
            started = true;
            owner.startMidiDrag();
        }
    }
    void mouseUp(const juce::MouseEvent&) override { started = false; }
private:
    LtaChordsEditor& owner;
    bool started = false;
};
LtaChordsEditor::LtaChordsEditor(LtaChordsProcessor& p)
  : AudioProcessorEditor(&p), processor(p), theme(std::make_unique<FlatLookAndFeel>()),
    piano(std::make_unique<PianoRoll>(*this)), dragArea(std::make_unique<MidiDragArea>(*this)) {
    setLookAndFeel(theme.get());
    setSize(1050, 805);
    setResizable(true, true);
    setResizeLimits(860, 695, 1440, 1000);
    title.setText("LTA // CHORDS", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f).withStyle("Bold")));
    title.setColour(juce::Label::textColourId, ink);
    addAndMakeVisible(title);
    subtitle.setText("Progression lab  /  generate, edit, export", juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, muted);
    subtitle.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(subtitle);
    status.setColour(juce::Label::textColourId, muted);
    status.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status);
    tips.setText("Click to add  /  drag to move  /  Alt-drag edge to resize  /  right-click or double-click to delete  /  click a chord label to regenerate", juce::dontSendNotification);
    tips.setColour(juce::Label::textColourId, muted);
    tips.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(tips);
    setCombo(keySelect, keyNames);
    setCombo(modeSelect, {"Minor", "Major"});
    setCombo(barSelect, {"4 bars", "8 bars", "16 bars"});
    setCombo(styleSelect, {"Dreamy", "Dark", "Soul", "Classic", "Cinematic", "Rage"});
    setCombo(complexitySelect, {"Triads", "7th chords", "9th chords"});
    setCombo(voicingSelect, {"Root", "Smooth", "Open"});
    for (auto* combo : {&keySelect, &modeSelect, &barSelect, &styleSelect, &complexitySelect, &voicingSelect})
        combo->onChange = [this] { settingChanged(); };
    auto initButton = [this](juce::TextButton& b, const juce::String& text, juce::Colour bg, bool bright = false) {
        b.setButtonText(text);
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::textColourOffId, bright ? white : ink);
        addAndMakeVisible(b);
    };
    initButton(generate, "GENERATE NEW", rose, true);
    initButton(reroll, "REROLL CHORD", white);
    initButton(inversion, "INVERT", white);
    initButton(shuffle, "SHUFFLE", white);
    initButton(transposeDown, "- SEMITONE", white);
    initButton(transposeUp, "+ SEMITONE", white);
    initButton(undoBtn, "UNDO", white);
    initButton(redoBtn, "REDO", white);
    initButton(previewBtn, "PLAY PREVIEW", ink, true);
    initButton(saveBtn, "SAVE PRESET", white);
    initButton(loadBtn, "LOAD PRESET", white);
    initButton(exportBtn, "EXPORT MIDI", rose, true);
    generate.onClick = [this] { processor.modify([](lta::ChordEngine& e) { e.generate(); });
        piano->clearSelection(); showStatus("Generated a new progression"); refresh(); };
    reroll.onClick = [this] { processor.modify([this](lta::ChordEngine& e) { e.regenerateBar(selectedBar); });
        piano->clearSelection(); showStatus("Regenerated chord " + juce::String(selectedBar + 1)); refresh(); };
    inversion.onClick = [this] { processor.modify([this](lta::ChordEngine& e) { e.invertBar(selectedBar); });
        piano->clearSelection(); refresh(); };
    shuffle.onClick = [this] { processor.modify([](lta::ChordEngine& e) { e.shuffle(); });
        piano->clearSelection(); refresh(); };
    transposeDown.onClick = [this] { processor.modify([](lta::ChordEngine& e) { e.transpose(-1); });
        syncControls(); refresh(); };
    transposeUp.onClick = [this] { processor.modify([](lta::ChordEngine& e) { e.transpose(1); });
        syncControls(); refresh(); };
    undoBtn.onClick = [this] { if (processor.undo()) { syncControls(); piano->clearSelection(); refresh(); } };
    redoBtn.onClick = [this] { if (processor.redo()) { syncControls(); piano->clearSelection(); refresh(); } };
    previewBtn.onClick = [this] {
        const bool start = !processor.preview();
        processor.setPreview(start);
        if (start) processor.restartPreview();
        previewBtn.setButtonText(start ? "STOP PREVIEW" : "PLAY PREVIEW");
    };
    saveBtn.onClick = [this] { choosePreset(true); };
    loadBtn.onClick = [this] { choosePreset(false); };
    exportBtn.onClick = [this] { chooseExport(); };
    viewport.setScrollBarsShown(true, true);
    viewport.setViewedComponent(piano.get(), false);
    addAndMakeVisible(viewport);
    addAndMakeVisible(*dragArea);
    syncControls();
    startTimerHz(20);
    resized();
}
LtaChordsEditor::~LtaChordsEditor() {
    stopTimer();
    viewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}
void LtaChordsEditor::setCombo(juce::ComboBox& cb, const juce::StringArray& labels, int initial) {
    for (int i = 0; i < labels.size(); ++i) cb.addItem(labels[i], i + 1);
    cb.setSelectedId(initial, juce::dontSendNotification);
    addAndMakeVisible(cb);
}
void LtaChordsEditor::syncControls() {
    const auto e = processor.model();
    keySelect.setSelectedId(e.settings.key + 1, juce::dontSendNotification);
    modeSelect.setSelectedId(e.settings.minor ? 1 : 2, juce::dontSendNotification);
    barSelect.setSelectedId(e.settings.bars == 4 ? 1 : e.settings.bars == 8 ? 2 : 3, juce::dontSendNotification);
    styleSelect.setSelectedId(e.settings.style + 1, juce::dontSendNotification);
    complexitySelect.setSelectedId(e.settings.complexity + 1, juce::dontSendNotification);
    voicingSelect.setSelectedId(e.settings.inversion + 1, juce::dontSendNotification);
}
void LtaChordsEditor::settingChanged() {
    processor.modify([this](lta::ChordEngine& e) {
        e.settings.key = keySelect.getSelectedId() - 1;
        e.settings.minor = modeSelect.getSelectedId() == 1;
        e.settings.bars = barSelect.getSelectedId() == 1 ? 4 : barSelect.getSelectedId() == 2 ? 8 : 16;
        e.settings.style = styleSelect.getSelectedId() - 1;
        e.settings.complexity = complexitySelect.getSelectedId() - 1;
        e.settings.inversion = voicingSelect.getSelectedId() - 1;
        e.generate();
    });
    piano->clearSelection();
    selectedBar = 0;
    showStatus("Generated with updated settings");
    resized();
    refresh();
}
void LtaChordsEditor::paint(juce::Graphics& g) {
    g.fillAll(canvas);
    g.setColour(pale);
    g.fillRect(0, 0, getWidth(), 6);
    g.setColour(line);
    g.drawHorizontalLine(82, 24.0f, (float)getWidth() - 24);
}
void LtaChordsEditor::resized() {
    const int w = getWidth();
    title.setBounds(24, 18, 315, 44);
    subtitle.setBounds(26, 57, 400, 18);
    status.setBounds(w - 330, 34, 302, 25);
    const int top = 104;
    const int widths[] = {105, 112, 112, 177, 160, 160};
    juce::ComboBox* combos[] = {&keySelect, &modeSelect, &barSelect, &styleSelect, &complexitySelect, &voicingSelect};
    int x = 24;
    constexpr int comboGap = 8;
    const int normalWidth = 105 + 112 + 112 + 177 + 160 + 160;
    const float scale = std::min(1.0f, static_cast<float>(w - 48 - 5 * comboGap) / normalWidth);
    for (int i = 0; i < 6; ++i) {
        const int comboWidth = static_cast<int>(widths[i] * scale);
        auto labelBounds = juce::Rectangle<int>(x, top - 18, comboWidth, 16);
        // Draw the headings in paintOverChildren to avoid allocating controls.
        comboLabelBounds[i] = labelBounds;
        combos[i]->setBounds(x, top, comboWidth, 34);
        x += comboWidth + comboGap;
    }
    const int btnY = 154, h = 36, gap = 8;
    x = 24;
    auto place = [&](juce::TextButton& b, int size) { b.setBounds(x, btnY, size, h); x += size + gap; };
    place(generate, 144); place(reroll, 125); place(inversion, 85); place(shuffle, 93);
    place(transposeDown, 100); place(transposeUp, 100);
    const bool compact = w < 1024;
    if (compact) {
        undoBtn.setBounds(24, 197, 82, h);
        redoBtn.setBounds(114, 197, 82, h);
        previewBtn.setBounds(204, 197, 150, h);
    } else {
        place(undoBtn, 64); place(redoBtn, 64); place(previewBtn, 136);
    }
    tips.setBounds(24, compact ? 239 : 211, w - 45, 25);
    const int bottom = getHeight() - 77;
    const int rollTop = compact ? 270 : 245;
    viewport.setBounds(24, rollTop, w - 48, bottom - rollTop);
    const int bars = processor.model().settings.bars;
    piano->setSize(std::max(w - 48, rollLeft + bars * barWidth + 6),
                   std::max(480, viewport.getHeight() - 10));
    int bottomX = 24;
    dragArea->setBounds(bottomX, bottom + 15, 298, 45);
    bottomX += 310;
    exportBtn.setBounds(bottomX, bottom + 15, 170, 45);
    bottomX += 182;
    saveBtn.setBounds(bottomX, bottom + 15, 145, 45);
    bottomX += 157;
    loadBtn.setBounds(bottomX, bottom + 15, 145, 45);
}
void LtaChordsEditor::paintOverChildren(juce::Graphics& g) {
    const juce::StringArray labels{"KEY", "SCALE", "LENGTH", "STYLE", "RICHNESS", "VOICING"};
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    for (int i = 0; i < 6; ++i) g.drawFittedText(labels[i], comboLabelBounds[i], juce::Justification::centredLeft, 1);
}
void LtaChordsEditor::refresh() { piano->repaint(); repaint(); }
void LtaChordsEditor::showStatus(const juce::String& s) { status.setText(s, juce::dontSendNotification); }
void LtaChordsEditor::timerCallback() {
    const auto tempo = processor.bpm();
    if (processor.preview()) {
        status.setText("PREVIEW  /  " + juce::String(tempo, 0) + " BPM", juce::dontSendNotification);
    }
    piano->repaint();
}
void LtaChordsEditor::startMidiDrag() {
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto file = folder.getChildFile("LTA_CHORDS_" +
        juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".mid");
    if (!processor.exportMidi(file)) { showStatus("Could not create temporary MIDI"); return; }
    const juce::StringArray files{file.getFullPathName()};
    if (!juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, dragArea.get()))
        showStatus("Drag unavailable in this host - use EXPORT MIDI");
}
void LtaChordsEditor::chooseExport() {
    chooser = std::make_unique<juce::FileChooser>("Export MIDI", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LTA Progression.mid"), "*.mid");
    juce::Component::SafePointer<LtaChordsEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& dialog) {
            if (safe == nullptr) return;
            auto file = dialog.getResult();
            if (file == juce::File()) return;
            if (file.getFileExtension().isEmpty()) file = file.withFileExtension(".mid");
            safe->showStatus(safe->processor.exportMidi(file) ? "Saved " + file.getFileName() : "MIDI export failed");
        });
}
void LtaChordsEditor::choosePreset(bool save) {
    const auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LTA Chords.ltac");
    chooser = std::make_unique<juce::FileChooser>(save ? "Save LTA preset" : "Load LTA preset", file, "*.ltac");
    juce::Component::SafePointer<LtaChordsEditor> safe(this);
    const auto flags = (save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode)
                       | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(flags, [safe, save](const juce::FileChooser& dialog) {
        if (safe == nullptr) return;
        auto file = dialog.getResult();
        if (file == juce::File()) return;
        if (save && file.getFileExtension().isEmpty()) file = file.withFileExtension(".ltac");
        const bool ok = save ? safe->processor.savePreset(file) : safe->processor.loadPreset(file);
        safe->showStatus(ok ? (save ? "Preset saved" : "Preset loaded") : "Preset operation failed");
        if (ok && !save) { safe->syncControls(); safe->resized(); safe->refresh(); }
    });
}
