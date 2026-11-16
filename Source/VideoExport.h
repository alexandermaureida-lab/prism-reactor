#pragma once
#include <juce_core/juce_core.h>
#include <memory>

// Encodes RGBA frames (bottom-up rows, as read from OpenGL) into an H.264 MP4 and muxes
// an audio file under it. macOS: AVFoundation (no external dependencies).
// Linux (development/testing only): pipes to an installed ffmpeg. Windows: not available yet.
class VideoSink
{
public:
    virtual ~VideoSink() = default;
    virtual bool begin (const juce::File& out, int width, int height, int fps, double megabitsPerSecond,
                        const juce::File& audioFile, juce::String& error) = 0;
    virtual bool addFrame (const juce::uint8* rgbaBottomUp, juce::String& error) = 0;
    virtual bool finish (juce::String& error) = 0;   // blocking (muxing); call off the GL / message thread
    virtual void abort() = 0;
    static std::shared_ptr<VideoSink> create();
    static bool isAvailable();
};
