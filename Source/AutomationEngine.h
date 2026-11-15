#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>

// Own timeline for the Standalone (and optional inside a host): records parameter moves
// and plays them back on an internal looping clock. All access is on the message thread.
struct AutoPoint { double t; float v; };   // v is the normalised 0..1 parameter value

struct AutoLane
{
    juce::RangedAudioParameter* param = nullptr;
    juce::String id;
    std::vector<AutoPoint> pts;
    bool mute = false;
    bool discrete() const { return param != nullptr && param->isDiscrete(); }
};

class AutomationEngine : private juce::Timer, private juce::AudioProcessorParameter::Listener
{
public:
    explicit AutomationEngine (juce::AudioProcessorValueTreeState&);
    ~AutomationEngine() override;

    // transport
    void setPlaying (bool);
    bool isPlaying() const { return playing; }
    void setRecording (bool r) { recording = r; }
    bool isRecording() const { return recording; }
    double getTime() const { return time; }
    void setTime (double t);
    double getLoopLength() const { return loopLen; }
    void setLoopLength (double s);

    // lanes
    std::vector<std::unique_ptr<AutoLane>>& getLanes() { return lanes; }
    AutoLane* find (const juce::String& id);
    AutoLane* addLane (const juce::String& id, bool seedWithCurrent);
    void removeLane (const juce::String& id);
    void clearAll();
    float evaluate (const AutoLane&, double t) const;
    float quantise (const AutoLane&, float v) const;
    void markChanged();                       // after UI edits: reapply + notify
    std::function<void()> onLanesChanged;     // structure changed (add/remove/load)

    // persistence
    juce::var toVar() const;
    void fromVar (const juce::var&);

    juce::RangedAudioParameter* paramFor (const juce::String& id) const;
    juce::Array<juce::RangedAudioParameter*> allParams() const { return params; }

private:
    struct Touch { double last = 0.0, lastPtT = -1.0; juce::int64 untilMs = 0; };

    void timerCallback() override;
    void parameterValueChanged (int index, float newValue) override;
    void parameterGestureChanged (int index, bool starting) override;
    void write (AutoLane&, float v, double t);
    void apply();
    static bool onMessageThread() { return juce::MessageManager::getInstance()->isThisTheMessageThread(); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<juce::RangedAudioParameter*> params;
    std::map<int, juce::String> idByIndex;
    std::vector<std::unique_ptr<AutoLane>> lanes;
    std::map<juce::String, Touch> touch;
    std::map<juce::String, bool> holding;

    bool playing = false, recording = false, applying = false, dirty = false;
    double time = 0.0, loopLen = 60.0, lastTick = 0.0;
};
