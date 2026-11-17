#include "VideoExport.h"

#if JUCE_LINUX
#include <cstdio>

// Development/testing backend: needs an ffmpeg executable on PATH.
class FfmpegSink : public VideoSink
{
public:
    ~FfmpegSink() override { abort(); }

    bool begin (const juce::File& out, int w, int h, int fps, double mbps, const juce::File& audio, int codec, juce::String& err) override
    {
        width = w; height = h;
        juce::String cmd = "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgba -s " + juce::String (w) + "x" + juce::String (h)
                         + " -r " + juce::String (fps) + " -i - ";
        if (audio.existsAsFile()) cmd += "-i \"" + audio.getFullPathName() + "\" -map 0:v -map 1:a -c:a aac -b:a 192k -shortest ";
        if (codec == 2) cmd += "-vf vflip -c:v prores_ks -profile:v 2 -pix_fmt yuv422p10le \"" + out.getFullPathName() + "\"";
        else cmd += juce::String ("-vf vflip -c:v ") + (codec == 1 ? "libx265 -tag:v hvc1" : "libx264") + " -pix_fmt yuv420p -b:v " + juce::String ((int) (mbps * 1000)) + "k \"" + out.getFullPathName() + "\"";
        pipe = popen (cmd.toRawUTF8(), "w");
        if (pipe == nullptr) { err = "ffmpeg not found"; return false; }
        return true;
    }
    bool addFrame (const juce::uint8* rgba, juce::String& err) override
    {
        if (pipe == nullptr) return false;
        if (fwrite (rgba, 1, (size_t) width * (size_t) height * 4, pipe) != (size_t) width * (size_t) height * 4)
        { err = "ffmpeg closed the pipe"; return false; }
        return true;
    }
    bool finish (juce::String& err) override
    {
        if (pipe == nullptr) { err = "not started"; return false; }
        const int rc = pclose (pipe); pipe = nullptr;
        if (rc != 0) { err = "ffmpeg failed"; return false; }
        return true;
    }
    void abort() override { if (pipe != nullptr) { pclose (pipe); pipe = nullptr; } }
private:
    FILE* pipe = nullptr;
    int width = 0, height = 0;
};

std::shared_ptr<VideoSink> VideoSink::create() { return std::make_shared<FfmpegSink>(); }
bool VideoSink::isAvailable() { return true; }

#else // Windows & others: not implemented yet

std::shared_ptr<VideoSink> VideoSink::create() { return nullptr; }
bool VideoSink::isAvailable() { return false; }

#endif
