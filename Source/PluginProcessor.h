#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Analyzer.h"
#include "Params.h"
#include "AutomationEngine.h"

class PrismProcessor : public juce::AudioProcessor
{
public:
    PrismProcessor();
    ~PrismProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Prism Reactor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // factory presets are exposed as host programs
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    static int numFactoryPresets();
    static juce::String factoryPresetName (int);
    bool savePresetFile (const juce::File&);
    bool loadPresetFile (const juce::File&);

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void randomizeLook (juce::AudioProcessorValueTreeState&);

    // ---- audio source: live input, or a file played by the plugin itself
    bool loadAudioFile (const juce::File&);
    juce::File getLoadedAudioFile() const { return loadedAudio; }
    bool hasAudioFile() const { return fileSource != nullptr; }
    void setUseFile (bool b) { useFile = b && hasAudioFile(); }
    bool isUsingFile() const { return useFile.load(); }
    void filePlay (bool play);
    bool fileIsPlaying() const { return transport.isPlaying(); }
    double fileTime() const { return transport.getCurrentPosition(); }
    double fileLength() const { return transport.getLengthInSeconds(); }
    void fileSeek (double s) { transport.setPosition (juce::jlimit (0.0, fileLength(), s)); }

    // ---- record what the plugin listens to (live input or file) into a WAV
    bool startRecording();
    juce::File stopRecording();
    bool isRecording() const { return recorder != nullptr; }
    juce::File getLastRecording() const { return lastRecording; }

    // ---- image layer
    bool setMediaFile (const juce::File&);
    void clearMedia();
    juce::File getMediaFile() const { return mediaFile; }
    int getMediaVersion() const { return mediaVersion.load(); }
    juce::Image getMediaImage() const { const juce::ScopedLock sl (mediaLock); return mediaImage; }

    // ---- host transport (read by the automation engine)
    bool readHostClock (double& seconds, bool& playing, double& bpm) const;

    juce::AudioProcessorValueTreeState apvts;
    AutomationEngine automation { apvts };
    Analyzer analyzer;
    std::atomic<float> previewScale { 1.0f };   // < 0 = automatic quality
    std::atomic<int> editorUseCount { 0 };

private:
    std::atomic<float>* gainParam = nullptr;
    int currentProgram = 0;

    juce::AudioFormatManager formats;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> fileSource;
    juce::File loadedAudio;
    std::atomic<bool> useFile { false };
    juce::AudioBuffer<float> fileBuf;

    juce::TimeSliceThread writerThread { "Prism recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recorder;
    juce::CriticalSection recorderLock;
    juce::File lastRecording, recordingFile;
    double currentRate = 44100.0;

    juce::CriticalSection mediaLock;
    juce::Image mediaImage;
    juce::File mediaFile;
    std::atomic<int> mediaVersion { 0 };

    std::atomic<bool> hostValid { false }, hostPlaying { false };
    std::atomic<double> hostTime { 0.0 }, hostBpm { 0.0 };
    std::atomic<juce::int64> lastBlockMs { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismProcessor)
};
