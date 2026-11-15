#pragma once
#include <juce_opengl/juce_opengl.h>
#include <list>
#include <map>
#include <string>
#include "PluginProcessor.h"

// Three-pass GLSL renderer: scene -> trails (ping-pong feedback) -> final grade.
class GLCanvas : public juce::Component, private juce::OpenGLRenderer
{
public:
    explicit GLCanvas (PrismProcessor& p);
    ~GLCanvas() override;

private:
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
    float phase = 0.0f, spinAng = 0.0f, huePhase = 0.0f;
    std::array<float, Analyzer::numBars> bars {};

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
    std::vector<Item> items;
    Row* lensStyleRow = nullptr;
    int contentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePanel)
};

class PrismEditor : public juce::AudioProcessorEditor
{
public:
    explicit PrismEditor (PrismProcessor&);
    ~PrismEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PrismProcessor& proc;
    GLCanvas canvas;
    TunePanel panel;
    juce::Viewport viewport;
    juce::Label title;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismEditor)
};
