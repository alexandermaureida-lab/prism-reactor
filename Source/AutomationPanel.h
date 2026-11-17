#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutomationEngine.h"

class LaneGraph;

// Timeline strip: transport, record, editable lanes (click = add point, drag = move,
// double-click / right-click = delete). Works in the Standalone and inside a host.
class AutomationPanel : public juce::Component, private juce::Timer
{
public:
    explicit AutomationPanel (AutomationEngine&);
    ~AutomationPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int preferredHeight = 282;

private:
    class Ruler;
    struct Row;
    void timerCallback() override;
    void rebuild();
    void refreshAddList();
    void saveToFile();
    void loadFromFile();

    AutomationEngine& engine;
    juce::TextButton playButton { "PLAY" }, recButton { "REC" }, saveButton { "SAVE" }, loadButton { "LOAD" }, clearButton { "CLEAR" };
    juce::ComboBox loopBox, addBox, syncBox;
    juce::Label bpmLabel, bpmCaption;
    juce::Label timeLabel;
    std::unique_ptr<Ruler> ruler;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Label emptyLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    bool needsRebuild = true;
    int clearArm = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationPanel)
};
