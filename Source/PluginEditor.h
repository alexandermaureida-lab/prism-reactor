#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_opengl/juce_opengl.h>
#include <deque>
#include <list>
#include <map>
#include <string>
#include "PluginProcessor.h"
#include "AutomationPanel.h"
#include "TransportBar.h"
#include "VideoExport.h"

// Three-pass GLSL renderer: scene -> trails (ping-pong feedback) -> final grade.
class GLCanvas : public juce::Component, private juce::OpenGLRenderer
{
public:
    explicit GLCanvas (PrismProcessor& p);
    ~GLCanvas() override;

    // Offline video export: renders the visuals frame by frame from an audio file.
    struct ExportSettings { int width = 1920, height = 1080, fps = 30, codec = 0; double mbps = 16.0; juce::File audio, output; };
    bool startExport (const ExportSettings&, juce::String& error);   // message thread
    void cancelExport() { exportCancel = true; }
    int  getExportPhase() const { return exportPhase.load(); }        // 0 idle, 1 rendering, 2 finishing
    float getExportProgress() const
    {
        const auto t = exportTotal.load();
        return t > 0 ? (float) exportFrame.load() / (float) t : 0.0f;
    }
    std::function<void (bool ok, juce::String message)> onExportFinished;   // message thread

private:
    struct SnapLane { juce::RangedAudioParameter* param = nullptr; std::vector<AutoPoint> pts; bool discrete = false; };
    struct ExportJob
    {
        ExportSettings s;
        juce::AudioBuffer<float> mono;
        double sampleRate = 44100.0, loopLen = 60.0;
        juce::int64 total = 0, frame = 0, samplePos = 0;
        Analyzer analyzer;
        std::shared_ptr<VideoSink> sink;
        std::vector<SnapLane> lanes;
        juce::OpenGLFrameBuffer fbOut;
        std::vector<juce::uint8> pixels;
        juce::String error;
        bool firstFrame = true;
    };
    void applyExportAutomation (const ExportJob&);
    void endExportJob (bool cancelled);
    void finishExportOnMessageThread (bool ok, const juce::String& msg);

    struct Pass
    {
        std::unique_ptr<juce::OpenGLShaderProgram> prog;
        std::map<std::string, int> locs;
        int loc (const char* name);
    };

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    bool build (Pass& pass, const char* fragmentBody);
    void ensureTargets (int w, int h);
    void drawQuad (Pass& pass);

    PrismProcessor& proc;
    juce::OpenGLContext ctx;
    Pass scenePass, feedbackPass, finalPass;
    juce::OpenGLFrameBuffer fbScene, fbTrail[2];
    int current = 0;
    unsigned int vbo = 0;

    double lastTime = 0.0;
    float phase = 0.0f, spinAng = 0.0f, huePhase = 0.0f, mxSpinAng = 0.0f;
    // automatic preview quality
    float autoScale = 1.0f, frameEma = 0.0166f;
    double lastScaleChange = 0.0;
    // image layer
    unsigned int mediaTex = 0, dummyTex = 0;
    int shownScene = -1, prevScene = 0;
    float transT = 0.0f;
    bool transOn = false;
    int mediaSeen = -1;
    bool hasMedia = false;
    float mediaAspect = 1.0f;
    void updateMediaTexture();
    std::array<float, Analyzer::numBars> bars {};

    juce::CriticalSection jobLock;
    std::unique_ptr<ExportJob> pendingJob, job;
    std::atomic<bool> exportCancel { false };
    std::atomic<int> exportPhase { 0 };
    std::atomic<juce::int64> exportFrame { 0 }, exportTotal { 0 };
    std::vector<std::pair<juce::RangedAudioParameter*, float>> restoreValues;
    juce::String exportOutputPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GLCanvas)
};

// Six draggable nodes shaping the analysis input (log-frequency, +/-12 dB).
class EQCurve : public juce::Component, private juce::Timer
{
public:
    explicit EQCurve (PrismProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Point<float> nodePos (int i, float v) const;
    void setNode (int i, float v);

    PrismProcessor& proc;
    juce::RangedAudioParameter* params[kNumEq] {};
    int active = -1;
};

// Scrollable "tune" column: sliders, combos, EQ, randomize.
class TunePanel : public juce::Component
{
public:
    explicit TunePanel (PrismProcessor&);
    void resized() override;
    int getContentHeight() const { return contentHeight; }

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        juce::ComboBox combo;
        std::unique_ptr<APVTS::SliderAttachment> sAtt;
        std::unique_ptr<APVTS::ComboBoxAttachment> cAtt;
    };
    struct Item { juce::Component* a = nullptr; juce::Component* b = nullptr; int height = 28; bool fullWidth = false; };

    void addSlider (const char* id);
    void addCombo (const char* id, const char* label, bool dimWhenLensOff = false);
    void addHeader (const juce::String& text);
    void timerTick();

    PrismProcessor& proc;
    std::list<Row> rows;
    std::list<juce::Label> headers;
    EQCurve eqCurve;
    juce::Label eqLabel;
    juce::TextButton randomButton { "RANDOMIZE LOOK" };
    std::unique_ptr<juce::Component> mediaRow;
    std::vector<Item> items;
    Row* lensStyleRow = nullptr;
    int contentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePanel)
};

class FullscreenWindow;

class PrismEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismEditor (PrismProcessor&);
    ~PrismEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    void timerCallback() override;
    void askExportOptions (bool preferRecording = false);
    void chooseAudioThenOutput (int audioKind);
    void chooseOutput();
    void startNextExport();
    void toggleFullscreen();
    void setPanelVisible (bool);

    PrismProcessor& proc;
    GLCanvas canvas;
    TransportBar transportBar;
    TunePanel panel;
    juce::Viewport viewport;
    juce::ComboBox presetBox, previewBox;
    juce::TextButton presetSave { "SAVE" }, presetLoad { "LOAD" };
    juce::TextButton autoButton { "AUTOMATION" }, exportButton { "EXPORT VIDEO" }, fullButton { "FULLSCREEN" }, cancelButton { "CANCEL" };
    juce::Label statusLabel;
    AutomationPanel autoPanel;

    GLCanvas::ExportSettings exportSettings;
    std::deque<GLCanvas::ExportSettings> exportQueue;
    juce::StringArray exportReport;
    juce::int64 exportStartMs = 0;
    bool panelVisible = true;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<FullscreenWindow> fullscreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismEditor)
};
