#include "AutomationPanel.h"

using namespace juce;

namespace
{
const Colour kAccent (0xff4de2c5);
const Colour kRec (0xffff4d6a);
constexpr int kLabelW = 150, kRowH = 52, kHeaderH = 68, kRulerH = 18;

int gridDivisions (const AutomationEngine& e) { return e.getLoopBars() > 0 && e.getLoopBars() <= 32 ? e.getLoopBars() : 10; }

String fmtTime (double t)
{
    const int m = (int) (t / 60.0);
    return String (m) + ":" + String (t - m * 60.0, 1).paddedLeft ('0', 4);
}
}

//==============================================================================
class LaneGraph : public Component
{
public:
    LaneGraph (AutomationEngine& e, AutoLane* l) : engine (e), lane (l) {}

    void paint (Graphics& g) override
    {
        const auto w = (float) getWidth(), h = (float) getHeight();
        const double D = engine.getLoopLength();
        g.setColour (Colours::white.withAlpha (0.035f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (Colours::white.withAlpha (0.07f));
        const int nd = gridDivisions (engine);
        for (int i = 1; i < nd; ++i) g.drawVerticalLine (roundToInt (w * (float) i / (float) nd), 0.0f, h);

        auto toXY = [&] (const AutoPoint& p) { return Point<float> ((float) (p.t / D) * w, (1.0f - p.v) * (h - 10.0f) + 5.0f); };
        const auto& pts = lane->pts;
        g.setOpacity (lane->mute ? 0.35f : 1.0f);
        if (! pts.empty())
        {
            Path path;
            auto first = toXY (pts.front());
            path.startNewSubPath (0.0f, first.y);
            for (size_t i = 0; i < pts.size(); ++i)
            {
                auto xy = toXY (pts[i]);
                if (lane->discrete() && i > 0) path.lineTo (xy.x, toXY (pts[i - 1]).y);
                path.lineTo (xy);
            }
            path.lineTo (w, toXY (pts.back()).y);
            g.setColour (kAccent.withAlpha (lane->mute ? 0.4f : 1.0f));
            g.strokePath (path, PathStrokeType (1.6f));
            g.setColour (Colours::white.withAlpha (lane->mute ? 0.4f : 1.0f));
            for (auto& p : pts) { auto xy = toXY (p); g.fillEllipse (xy.x - 3.2f, xy.y - 3.2f, 6.4f, 6.4f); }
        }
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRect (jlimit (0.0f, w - 1.0f, (float) (engine.getTime() / D) * w), 0.0f, 1.0f, h);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { removeNear (e); return; }
        drag = nearest (e);
        if (drag < 0)
        {
            AutoPoint np = fromPos (e);
            lane->pts.push_back (np);
            std::sort (lane->pts.begin(), lane->pts.end(), [] (const AutoPoint& a, const AutoPoint& b) { return a.t < b.t; });
            for (size_t i = 0; i < lane->pts.size(); ++i) if (lane->pts[i].t == np.t && lane->pts[i].v == np.v) drag = (int) i;
        }
        engine.markChanged(); repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0 || drag >= (int) lane->pts.size()) return;
        auto np = fromPos (e);
        const double D = engine.getLoopLength();
        const double lo = drag > 0 ? lane->pts[(size_t) drag - 1].t + 0.005 : 0.0;
        const double hi = drag + 1 < (int) lane->pts.size() ? lane->pts[(size_t) drag + 1].t - 0.005 : D;
        lane->pts[(size_t) drag].t = jlimit (lo, jmax (lo, hi), np.t);
        lane->pts[(size_t) drag].v = np.v;
        engine.markChanged(); repaint();
    }

    void mouseUp (const MouseEvent&) override { drag = -1; }
    void mouseDoubleClick (const MouseEvent& e) override { removeNear (e); }

private:
    AutoPoint fromPos (const MouseEvent& e) const
    {
        const float h = (float) getHeight();
        const float v = 1.0f - jlimit (0.0f, 1.0f, ((float) e.y - 5.0f) / (h - 10.0f));
        return { jlimit (0.0, 1.0, (double) e.x / (double) getWidth()) * engine.getLoopLength(), engine.quantise (*lane, v) };
    }
    int nearest (const MouseEvent& e) const
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        const double D = engine.getLoopLength();
        int best = -1; float bd = 11.0f;
        for (size_t i = 0; i < lane->pts.size(); ++i)
        {
            Point<float> xy ((float) (lane->pts[i].t / D) * w, (1.0f - lane->pts[i].v) * (h - 10.0f) + 5.0f);
            const float d = xy.getDistanceFrom (e.position);
            if (d < bd) { bd = d; best = (int) i; }
        }
        return best;
    }
    void removeNear (const MouseEvent& e)
    {
        const int i = nearest (e);
        if (i >= 0) { lane->pts.erase (lane->pts.begin() + i); engine.markChanged(); repaint(); }
    }

    AutomationEngine& engine;
    AutoLane* lane;
    int drag = -1;
};

//==============================================================================
class AutomationPanel::Ruler : public Component
{
public:
    explicit Ruler (AutomationEngine& e) : engine (e) {}
    void paint (Graphics& g) override
    {
        const double D = engine.getLoopLength();
        g.setColour (Colours::white.withAlpha (0.45f));
        g.setFont (11.0f);
        const int nd = gridDivisions (engine);
        const bool bars = engine.getLoopBars() > 0 && engine.getLoopBars() <= 32;
        const int labelEvery = (bars && nd > 16) ? 2 : 1;
        for (int i = 0; i <= nd; ++i)
        {
            const int x = roundToInt ((float) getWidth() * (float) i / (float) nd);
            g.drawVerticalLine (x, (float) getHeight() - 5.0f, (float) getHeight());
            if (i < nd && i % labelEvery == 0)
                g.drawText (bars ? String (i + 1) : fmtTime (D * i / (double) nd), x + 3, 0, 60, getHeight() - 2, Justification::centredLeft);
        }
        g.setColour (Colours::white);
        g.fillRect (jlimit (0, getWidth() - 1, roundToInt ((float) (engine.getTime() / D) * (float) getWidth())), 0, 2, getHeight());
    }
    void mouseDown (const MouseEvent& e) override { seek (e); }
    void mouseDrag (const MouseEvent& e) override { seek (e); }
private:
    void seek (const MouseEvent& e) { engine.setTime (jlimit (0.0, 1.0, (double) e.x / (double) getWidth()) * engine.getLoopLength()); repaint(); }
    AutomationEngine& engine;
};

struct AutomationPanel::Row
{
    Row (AutomationEngine& e, AutoLane* l, std::function<void()> onRemoved)
        : lane (l), graph (e, l)
    {
        name.setText (l->param->getName (24), dontSendNotification);
        name.setColour (Label::textColourId, Colours::white);
        name.setFont (Font (FontOptions (13.0f)));
        value.setColour (Label::textColourId, Colours::white.withAlpha (0.55f));
        value.setFont (Font (FontOptions (11.0f)));
        mute.setClickingTogglesState (true);
        mute.setToggleState (l->mute, dontSendNotification);
        mute.onClick = [this, &e] { lane->mute = mute.getToggleState(); graph.repaint(); e.markChanged(); };
        remove.onClick = [&e, id = l->id, onRemoved] { e.removeLane (id); };
        for (auto* c : std::initializer_list<Component*> { &name, &value, &mute, &remove, &graph }) holder.addAndMakeVisible (c);
    }
    AutoLane* lane;
    Component holder;
    Label name, value;
    TextButton mute { "M" }, remove { "X" };
    LaneGraph graph;
};

//==============================================================================
AutomationPanel::AutomationPanel (AutomationEngine& e) : engine (e), ruler (std::make_unique<Ruler> (e))
{
    for (auto* b : { &playButton, &recButton, &saveButton, &loadButton, &clearButton })
    {
        b->setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.05f));
        b->setColour (TextButton::textColourOffId, Colours::white.withAlpha (0.85f));
        addAndMakeVisible (b);
    }
    playButton.setClickingTogglesState (true);
    recButton.setClickingTogglesState (true);
    playButton.onClick = [this] { engine.setPlaying (playButton.getToggleState()); playButton.setButtonText (engine.isPlaying() ? "STOP" : "PLAY"); };
    recButton.onClick = [this] { engine.setRecording (recButton.getToggleState()); };
    recButton.setColour (TextButton::buttonOnColourId, kRec.withAlpha (0.35f));
    playButton.setColour (TextButton::buttonOnColourId, kAccent.withAlpha (0.3f));
    saveButton.onClick = [this] { saveToFile(); };
    loadButton.onClick = [this] { loadFromFile(); };
    clearButton.onClick = [this]
    {
        if (clearArm == 0) { clearArm = 90; clearButton.setButtonText ("SURE?"); return; }
        clearArm = 0; clearButton.setButtonText ("CLEAR"); engine.clearAll();
    };

    const double lens[] = { 15, 30, 60, 120, 240, 480 };
    const char* names[] = { "loop 0:15", "loop 0:30", "loop 1:00", "loop 2:00", "loop 4:00", "loop 8:00" };
    for (int i = 0; i < 6; ++i) loopBox.addItem (names[i], i + 1);
    const int barOpts[] = { 1, 2, 4, 8, 16, 32 };
    for (int i = 0; i < 6; ++i) loopBox.addItem (String (barOpts[i]) + (barOpts[i] == 1 ? " bar" : " bars"), 11 + i);
    loopBox.onChange = [this, lens, barOpts]
    {
        const int id = loopBox.getSelectedId();
        if (id >= 11) engine.setLoopBars (barOpts[id - 11]);
        else if (id >= 1) engine.setLoopLength (lens[id - 1]);
        repaint();
    };
    addAndMakeVisible (loopBox);

    syncBox.addItem ("Internal clock", 1);
    syncBox.addItem ("Host transport", 2);
    syncBox.setTooltip ("Internal: own loop and tempo. Host: follows the DAW play/stop and position (plugin only).");
    syncBox.setSelectedId (engine.isHostSync() ? 2 : 1, dontSendNotification);
    syncBox.onChange = [this] { engine.setHostSync (syncBox.getSelectedId() == 2); };
    addAndMakeVisible (syncBox);

    bpmCaption.setText ("BPM", dontSendNotification);
    bpmCaption.setColour (Label::textColourId, Colours::white.withAlpha (0.55f));
    bpmLabel.setEditable (true);
    bpmLabel.setColour (Label::textColourId, kAccent);
    bpmLabel.setColour (Label::outlineColourId, Colours::white.withAlpha (0.18f));
    bpmLabel.setJustificationType (Justification::centred);
    bpmLabel.onTextChange = [this] { engine.setBpm (bpmLabel.getText().getDoubleValue()); repaint(); };
    addAndMakeVisible (bpmCaption); addAndMakeVisible (bpmLabel);

    addBox.onChange = [this]
    {
        const int id = addBox.getSelectedId();
        if (id <= 1) return;
        auto params = engine.allParams();
        if (auto* p = params[id - 2]) engine.addLane (p->paramID, true);
        addBox.setSelectedId (1, dontSendNotification);
    };
    addAndMakeVisible (addBox);

    timeLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.8f));
    timeLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (timeLabel);

    emptyLabel.setText ("No lanes yet. Press PLAY, then REC, and move any control: one lane is created per parameter. "
                        "Or add one with '+ add parameter' and click on the lane to draw points. "
                        "Double-click or right-click deletes a point.", dontSendNotification);
    emptyLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.5f));
    emptyLabel.setMinimumHorizontalScale (1.0f);
    addChildComponent (emptyLabel);

    addAndMakeVisible (*ruler);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    engine.onLanesChanged = [this] { needsRebuild = true; };
    startTimerHz (30);
}

AutomationPanel::~AutomationPanel()
{
    engine.onLanesChanged = nullptr;
}

void AutomationPanel::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b1119));
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void AutomationPanel::refreshAddList()
{
    addBox.clear (dontSendNotification);
    addBox.addItem ("+ add parameter...", 1);
    auto params = engine.allParams();
    for (int i = 0; i < params.size(); ++i)
        addBox.addItem (params[i]->getName (28), i + 2);
    addBox.setSelectedId (1, dontSendNotification);
}

void AutomationPanel::rebuild()
{
    needsRebuild = false;
    rows.clear();
    content.removeAllChildren();
    int y = 0;
    for (auto& l : engine.getLanes())
    {
        auto row = std::make_unique<Row> (engine, l.get(), [] {});
        row->holder.setBounds (0, y, 100, kRowH);
        content.addAndMakeVisible (row->holder);
        rows.push_back (std::move (row));
        y += kRowH;
    }
    emptyLabel.setVisible (rows.empty());
    content.setSize (jmax (100, viewport.getWidth() - viewport.getScrollBarThickness()), jmax (y, 10));
    resized();
}

void AutomationPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    auto head = area.removeFromTop (28);
    playButton.setBounds (head.removeFromLeft (64)); head.removeFromLeft (6);
    recButton.setBounds (head.removeFromLeft (56)); head.removeFromLeft (10);
    timeLabel.setBounds (head.removeFromLeft (120));
    loopBox.setBounds (head.removeFromRight (110)); head.removeFromRight (8);
    bpmLabel.setBounds (head.removeFromRight (56)); head.removeFromRight (4);
    bpmCaption.setBounds (head.removeFromRight (34)); head.removeFromRight (8);
    syncBox.setBounds (head.removeFromRight (140));
    area.removeFromTop (4);
    auto head2 = area.removeFromTop (28);
    addBox.setBounds (head2.removeFromLeft (220));
    clearButton.setBounds (head2.removeFromRight (64)); head2.removeFromRight (6);
    loadButton.setBounds (head2.removeFromRight (56)); head2.removeFromRight (6);
    saveButton.setBounds (head2.removeFromRight (56));
    area.removeFromTop (4);

    auto rulerRow = area.removeFromTop (kRulerH);
    rulerRow.removeFromLeft (kLabelW + 8);
    rulerRow.removeFromRight (viewport.getScrollBarThickness());
    ruler->setBounds (rulerRow);

    viewport.setBounds (area);
    emptyLabel.setBounds (area.reduced (4, 4));
    const int w = jmax (100, viewport.getWidth() - viewport.getScrollBarThickness());
    content.setSize (w, jmax ((int) rows.size() * kRowH, 10));
    for (auto& r : rows)
    {
        r->holder.setSize (w, kRowH);
        r->name.setBounds (0, 4, kLabelW - 60, 18);
        r->value.setBounds (0, 24, kLabelW - 10, 16);
        r->mute.setBounds (kLabelW - 58, 5, 24, 20);
        r->remove.setBounds (kLabelW - 30, 5, 24, 20);
        r->graph.setBounds (kLabelW + 8, 3, w - kLabelW - 8, kRowH - 6);
    }
}

void AutomationPanel::timerCallback()
{
    if (needsRebuild) { rebuild(); refreshAddList(); }
    if (clearArm > 0 && --clearArm == 0) clearButton.setButtonText ("CLEAR");
    if (playButton.getToggleState() != engine.isPlaying()) playButton.setToggleState (engine.isPlaying(), dontSendNotification);
    {
        const bool host = engine.isHostSync();
        playButton.setEnabled (! host);
        if (! bpmLabel.isBeingEdited()) bpmLabel.setText (String (engine.getBpm(), 1), dontSendNotification);
        bpmLabel.setEditable (! host);
        const int want = engine.getLoopBars() > 0 ? 0 : 0;
        ignoreUnused (want);
        int sel = 0;
        const int bars = engine.getLoopBars();
        if (bars > 0) { const int opts[] = { 1, 2, 4, 8, 16, 32 }; for (int i = 0; i < 6; ++i) if (opts[i] == bars) sel = 11 + i; }
        else { const double lens[] = { 15, 30, 60, 120, 240, 480 }; for (int i = 0; i < 6; ++i) if (std::abs (lens[i] - engine.getLoopLength()) < 0.5) sel = i + 1; }
        if (sel != 0 && loopBox.getSelectedId() != sel) loopBox.setSelectedId (sel, dontSendNotification);
        if ((syncBox.getSelectedId() == 2) != host) syncBox.setSelectedId (host ? 2 : 1, dontSendNotification);
    }
    timeLabel.setText (fmtTime (engine.getTime()) + " / " + fmtTime (engine.getLoopLength()), dontSendNotification);
    ruler->repaint();
    for (auto& r : rows)
    {
        r->graph.repaint();
        r->value.setText (r->lane->param->getCurrentValueAsText(), dontSendNotification);
    }
}

void AutomationPanel::saveToFile()
{
    chooser = std::make_unique<FileChooser> ("Save automation", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("prism-automation.json"), "*.json");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f != File()) f.withFileExtension ("json").replaceWithText (JSON::toString (engine.toVar()));
                          });
}

void AutomationPanel::loadFromFile()
{
    chooser = std::make_unique<FileChooser> ("Load automation", File::getSpecialLocation (File::userDocumentsDirectory), "*.json");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile())
                              {
                                  auto v = JSON::parse (f);
                                  if (v.isObject()) engine.fromVar (v);
                              }
                          });
}
