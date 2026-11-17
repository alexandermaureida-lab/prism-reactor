#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Audio source strip under the visuals: live input or a file played by the plugin itself,
// plus a recorder for whatever the plugin is listening to (so it can be exported to video).
class TransportBar : public juce::Component, private juce::Timer
{
public:
    explicit TransportBar (PrismProcessor&);
    ~TransportBar() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
    void togglePlay();
    void openFile();
    static constexpr int preferredHeight = 40;

    std::function<void()> onRecordingStopped;    // message thread; a new recording is available

private:
    void timerCallback() override;
    PrismProcessor& proc;
    juce::TextButton openBtn { "OPEN AUDIO" }, playBtn { "PLAY" }, recBtn { "REC AUDIO" };
    juce::ComboBox sourceBox;
    juce::Slider pos;
    juce::Label timeLabel, nameLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::int64 recStartMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
