#include "PluginEditor.h"
#include "Shaders.h"
#include <thread>

using namespace juce;

namespace
{
const Colour kAccent (0xff4de2c5);
const Colour kPanel (0xff080c0f);

String versioned (const char* body, bool withCommon)
{
    return String ("#version 150\n") + (withCommon ? String (Shaders::common) : String()) + body;
}
} // namespace

//==============================================================================
int GLCanvas::Pass::loc (const char* name)
{
    auto it = locs.find (name);
    if (it != locs.end()) return it->second;
    const int l = prog != nullptr ? (int) juce::gl::glGetUniformLocation (prog->getProgramID(), name) : -1;
    locs[name] = l;
    return l;
}

GLCanvas::GLCanvas (PrismProcessor& p) : proc (p)
{
    ctx.setOpenGLVersionRequired (OpenGLContext::openGL3_2);
    ctx.setRenderer (this);
    ctx.setContinuousRepainting (true);
    ctx.setComponentPaintingEnabled (false);
    ctx.attachTo (*this);
}

GLCanvas::~GLCanvas()
{
    ctx.detach();
}

bool GLCanvas::build (Pass& pass, const char* fragmentBody)
{
    auto prog = std::make_unique<OpenGLShaderProgram> (ctx);
    if (prog->addVertexShader (versioned (Shaders::vert, false))
        && prog->addFragmentShader (versioned (fragmentBody, true))
        && prog->link())
    {
        pass.locs.clear();
        pass.prog = std::move (prog);
        return true;
    }
    DBG ("Prism shader error: " << prog->getLastError());
    jassertfalse;
    return false;
}

void GLCanvas::newOpenGLContextCreated()
{
    using namespace juce::gl;

    build (scenePass, Shaders::scene);
    build (feedbackPass, Shaders::feedback);
    build (finalPass, Shaders::final);

    const GLfloat verts[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    lastTime = Time::getMillisecondCounterHiRes() * 0.001;
}

void GLCanvas::openGLContextClosing()
{
    using namespace juce::gl;
    scenePass.prog.reset(); feedbackPass.prog.reset(); finalPass.prog.reset();
    fbScene.release(); fbTrail[0].release(); fbTrail[1].release();
    if (job != nullptr) { job->fbOut.release(); job->sink->abort(); job.reset(); exportPhase = 0; }
    if (pendingJob != nullptr) { pendingJob->sink->abort(); pendingJob.reset(); exportPhase = 0; }
    if (vbo != 0) { glDeleteBuffers (1, &vbo); vbo = 0; }
}

void GLCanvas::ensureTargets (int w, int h)
{
    using namespace juce::gl;
    if (fbScene.isValid() && fbScene.getWidth() == w && fbScene.getHeight() == h) return;

    for (auto* fb : { &fbScene, &fbTrail[0], &fbTrail[1] })
    {
        fb->release();
        fb->initialise (ctx, w, h);
        glBindTexture (GL_TEXTURE_2D, fb->getTextureID());
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture (GL_TEXTURE_2D, 0);
        fb->makeCurrentRenderingTarget();
        glClearColor (0.f, 0.f, 0.f, 1.f);
        glClear (GL_COLOR_BUFFER_BIT);
        fb->releaseAsRenderingTarget();
    }
    current = 0;
}

void GLCanvas::drawQuad (Pass& pass)
{
    using namespace juce::gl;
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    const auto a = (GLuint) glGetAttribLocation (pass.prog->getProgramID(), "aPos");
    glEnableVertexAttribArray (a);
    glVertexAttribPointer (a, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray (a);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void GLCanvas::renderOpenGL()
{
    using namespace juce::gl;
    jassert (OpenGLHelpers::isContextActive());

    const float scale = (float) ctx.getRenderingScale();
    const int physW = roundToInt (scale * (float) getWidth());
    const int physH = roundToInt (scale * (float) getHeight());

    // pick up a newly requested export
    {
        const ScopedLock sl (jobLock);
        if (pendingJob != nullptr && job == nullptr)
        {
            job = std::move (pendingJob);
            phase = spinAng = huePhase = 0.0f;
            fbScene.release();                       // force fresh (cleared) targets
            ctx.setSwapInterval (0);                 // render as fast as the GPU allows
        }
    }
    const bool exporting = job != nullptr;
    if (exporting && exportCancel.load()) { endExportJob (true); return; }

    OpenGLHelpers::clear (Colour (0xff05070a));
    int w = physW, h = physH;
    if (exporting) { w = job->s.width; h = job->s.height; }
    else
    {
        const float ps = jlimit (0.2f, 1.0f, proc.previewScale.load());
        w = jmax (2, roundToInt ((float) physW * ps));
        h = jmax (2, roundToInt ((float) physH * ps));
    }
    if (scenePass.prog == nullptr || feedbackPass.prog == nullptr || finalPass.prog == nullptr || physW < 2 || physH < 2)
        return;

    ensureTargets (w, h);
    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);

    auto P = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    float dt = (float) jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    float clock = (float) std::fmod (now, 1000.0);

    const Analyzer* anPtr = &proc.analyzer;
    if (exporting)
    {
        auto& j = *job;
        dt = 1.0f / (float) j.s.fps;
        clock = (float) std::fmod ((double) j.frame / (double) j.s.fps, 1000.0);
        const juce::int64 target = jmin ((juce::int64) j.mono.getNumSamples(), (juce::int64) ((double) (j.frame + 1) * j.sampleRate / (double) j.s.fps));
        const float* src = j.mono.getReadPointer (0);
        for (; j.samplePos < target; ++j.samplePos) j.analyzer.pushSample (src[j.samplePos]);
        applyExportAutomation (j);
        anPtr = &j.analyzer;
    }

    // analysis + input EQ
    float eq[kNumEq];
    for (int i = 0; i < kNumEq; ++i) eq[i] = P (kEqIds[i]);
    const auto& an = *anPtr;
    for (int i = 0; i < Analyzer::numBars; ++i)
        bars[(size_t) i] = jlimit (0.0f, 1.0f, an.getBar (i) + eqAt (eq, (float) i / (Analyzer::numBars - 1)) * (12.0f / 66.0f));

    auto mean = [this] (int from, int to)
    {
        float s = 0.0f;
        for (int i = from; i < to; ++i) s += bars[(size_t) i];
        return s / (float) jmax (1, to - from);
    };
    const float bass = mean (0, 16), lowMid = mean (16, 33), mid = mean (33, 50), high = mean (50, Analyzer::numBars);
    const float level = mean (0, Analyzer::numBars);
    const float beat = an.getBeat();

    // parameters
    const float react = P ("react") / 50.0f;
    const float speedMul = std::pow (2.0f, (P ("speed") - 50.0f) / 50.0f * 1.7f);
    phase += dt * speedMul * (0.5f + level * react * 1.5f);
    spinAng += dt * (P ("spin") / 100.0f) * 1.2f;
    huePhase += dt * (P ("hueDrift") / 100.0f) * 0.06f;

    const int pal = jlimit (0, (int) std::size (kPalettes) - 1, (int) P ("palette"));
    const auto& pl = kPalettes[pal];
    const float keepBase = std::pow (P ("trails") / 100.0f, 0.6f) * (0.985f + (0.82f - 0.985f) * (P ("decay") / 100.0f));
    const float keep = std::pow (keepBase, dt * 60.0f);
    const float chaos = P ("chaos") / 100.0f, heat = P ("heat") / 100.0f;

    auto f1 = [] (Pass& p, const char* n, float v) { const int l = p.loc (n); if (l >= 0) glUniform1f ((GLint) l, v); };
    auto i1 = [] (Pass& p, const char* n, int v)   { const int l = p.loc (n); if (l >= 0) glUniform1i ((GLint) l, v); };
    auto f2 = [] (Pass& p, const char* n, float a, float b) { const int l = p.loc (n); if (l >= 0) glUniform2f ((GLint) l, a, b); };

    // pass 1: scene
    fbScene.makeCurrentRenderingTarget();
    {
        auto& S = scenePass; S.prog->use();
        f2 (S, "uRes", (float) w, (float) h); f1 (S, "uTime", phase); f1 (S, "uClock", clock);
        f1 (S, "uHue", P ("hue") / 100.0f + huePhase);
        if (const int l = S.loc ("uBands"); l >= 0) glUniform4f ((GLint) l, bass, lowMid, mid, high);
        f1 (S, "uLevel", level); f1 (S, "uBeat", beat);
        if (const int l = S.loc ("uBars"); l >= 0) glUniform1fv ((GLint) l, Analyzer::numBars, bars.data());
        i1 (S, "uScene", (int) P ("scene"));
        i1 (S, "uPattern", (int) P ("pattern"));
        i1 (S, "uShape", (int) P ("shape"));
        i1 (S, "uMat", (int) P ("material"));
        f1 (S, "uReact", react); f1 (S, "uChaos", chaos);
        f1 (S, "uSym", P ("symmetry")); f1 (S, "uTiles", P ("tiles"));
        f1 (S, "uTwist", P ("twist") / 100.0f * 3.0f); f1 (S, "uSlice", P ("slice") / 100.0f);
        f1 (S, "uSpinAng", spinAng); f1 (S, "uComplex", P ("complexity") / 100.0f);
        f1 (S, "uFill", P ("fill") / 100.0f); f1 (S, "uContrast", P ("contrast") / 100.0f);
        if (const int l = S.loc ("uPalA"); l >= 0) glUniform3fv ((GLint) l, 1, pl.a);
        if (const int l = S.loc ("uPalB"); l >= 0) glUniform3fv ((GLint) l, 1, pl.b);
        if (const int l = S.loc ("uPalC"); l >= 0) glUniform3fv ((GLint) l, 1, pl.c);
        if (const int l = S.loc ("uPalD"); l >= 0) glUniform3fv ((GLint) l, 1, pl.d);
        drawQuad (S);
    }
    fbScene.releaseAsRenderingTarget();

    // pass 2: trails (ping-pong)
    const int prev = current, next = 1 - current;
    fbTrail[next].makeCurrentRenderingTarget();
    {
        auto& F = feedbackPass; F.prog->use();
        glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, fbScene.getTextureID());
        glActiveTexture (GL_TEXTURE1); glBindTexture (GL_TEXTURE_2D, fbTrail[prev].getTextureID());
        i1 (F, "uScene", 0); i1 (F, "uPrev", 1);
        f1 (F, "uTime", phase); f1 (F, "uKeep", keep); f1 (F, "uChaos", chaos);
        f1 (F, "uBeat", beat * react); f1 (F, "uHeat", heat); f1 (F, "uTint", P ("decay") / 100.0f);
        f1 (F, "uDt60", dt * 60.0f);
        drawQuad (F);
    }
    fbTrail[next].releaseAsRenderingTarget();
    current = next;

    // pass 3: final grade (to the export frame buffer and/or to the screen)
    auto drawFinal = [&] (int vw, int vh)
    {
        glViewport (0, 0, vw, vh);
        auto& N = finalPass; N.prog->use();
        glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, fbTrail[current].getTextureID());
        i1 (N, "uTex", 0);
        f2 (N, "uRes", (float) vw, (float) vh); f1 (N, "uClock", clock);
        f1 (N, "uLevel", level * react); f1 (N, "uBeat", beat * react);
        f1 (N, "uGlow", P ("glow") / 100.0f * 1.5f); f1 (N, "uBright", P ("brightness") / 100.0f);
        f1 (N, "uHeat", heat); f1 (N, "uLens", P ("lens") / 100.0f);
        i1 (N, "uLensStyle", (int) P ("lensStyle")); f1 (N, "uVig", P ("vignette") / 100.0f);
        drawQuad (N);
    };

    if (exporting)
    {
        auto& j = *job;
        if (j.firstFrame)
        {
            j.fbOut.initialise (ctx, w, h);
            j.pixels.assign ((size_t) w * (size_t) h * 4, 0);
            j.firstFrame = false;
        }
        j.fbOut.makeCurrentRenderingTarget();
        drawFinal (w, h);
        glPixelStorei (GL_PACK_ALIGNMENT, 1);
        glReadPixels (0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, j.pixels.data());
        j.fbOut.releaseAsRenderingTarget();
        if (! j.sink->addFrame (j.pixels.data(), j.error)) { endExportJob (false); return; }
        ++j.frame;
        exportFrame = j.frame;
    }

    glBindFramebuffer (GL_FRAMEBUFFER, ctx.getFrameBufferID());
    drawFinal (physW, physH);
    glActiveTexture (GL_TEXTURE1); glBindTexture (GL_TEXTURE_2D, 0);
    glActiveTexture (GL_TEXTURE0); glBindTexture (GL_TEXTURE_2D, 0);

    if (exporting && job->frame >= job->total) endExportJob (false);
}

//==============================================================================
bool GLCanvas::startExport (const ExportSettings& s, String& error)
{
    if (exportPhase.load() != 0) { error = "An export is already running"; return false; }
    if (! VideoSink::isAvailable()) { error = "Video export is only available on macOS for now."; return false; }

    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (s.audio));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    { error = "Cannot read that audio file. Use WAV, AIFF, MP3, M4A or FLAC."; return false; }

    auto j = std::make_unique<ExportJob>();
    j->s = s;
    j->s.width = jmax (2, s.width & ~1); j->s.height = jmax (2, s.height & ~1);
    j->sampleRate = reader->sampleRate;
    const auto len = (int) jmin ((int64) 0x7fffffff, reader->lengthInSamples);
    j->mono.setSize (1, len);
    j->mono.clear();
    {
        AudioBuffer<float> chunk ((int) reader->numChannels, 65536);
        for (int pos = 0; pos < len; pos += 65536)
        {
            const int n = jmin (65536, len - pos);
            reader->read (&chunk, 0, n, pos, true, true);
            for (int c = 0; c < chunk.getNumChannels(); ++c)
                j->mono.addFrom (0, pos, chunk, c, 0, n, 1.0f / (float) chunk.getNumChannels());
        }
    }
    j->total = (juce::int64) std::floor ((double) len / j->sampleRate * (double) s.fps);
    if (j->total < 1) { error = "The audio file is too short"; return false; }
    j->analyzer.prepare (j->sampleRate);

    j->sink = VideoSink::create();
    if (j->sink == nullptr) { error = "Video export is not available on this system"; return false; }
    if (! j->sink->begin (s.output, j->s.width, j->s.height, s.fps, s.mbps, s.audio, error)) return false;

    // freeze the automation lanes: the export follows them from t = 0
    auto& eng = proc.automation;
    eng.setPlaying (false); eng.setRecording (false);
    j->loopLen = eng.getLoopLength();
    for (auto& l : eng.getLanes())
        if (! l->mute && ! l->pts.empty())
            j->lanes.push_back ({ l->param, l->pts, l->discrete() });
    restoreValues.clear();
    for (auto* prm : eng.allParams()) restoreValues.push_back ({ prm, prm->getValue() });

    exportOutputPath = s.output.getFullPathName();
    exportCancel = false; exportFrame = 0; exportTotal = j->total; exportPhase = 1;
    const ScopedLock sl (jobLock);
    pendingJob = std::move (j);
    return true;
}

void GLCanvas::applyExportAutomation (const ExportJob& j)
{
    if (j.lanes.empty()) return;
    const double t = std::fmod ((double) j.frame / (double) j.s.fps, j.loopLen);
    for (auto& l : j.lanes)
    {
        const auto& p = l.pts;
        float v;
        if (t <= p.front().t) v = p.front().v;
        else if (t >= p.back().t) v = p.back().v;
        else
        {
            size_t lo = 0, hi = p.size() - 1;
            while (hi - lo > 1) { const size_t mid = (lo + hi) / 2; if (p[mid].t <= t) lo = mid; else hi = mid; }
            const double f = (t - p[lo].t) / jmax (1.0e-9, p[hi].t - p[lo].t);
            v = l.discrete ? p[lo].v : (float) (p[lo].v + (p[hi].v - p[lo].v) * f);
        }
        if (std::abs (l.param->getValue() - v) > 1.0e-5f) l.param->setValueNotifyingHost (v);
    }
}

void GLCanvas::endExportJob (bool cancelled)
{
    // GL thread
    auto finished = std::move (job);
    job.reset();
    ctx.setSwapInterval (1);
    fbScene.release();
    finished->fbOut.release();
    exportPhase = 2;
    auto sink = finished->sink;
    const String err = finished->error;
    const bool ok = ! cancelled && err.isEmpty();
    Component::SafePointer<GLCanvas> self (this);

    if (! ok)
    {
        sink->abort();
        const String msg = cancelled ? String ("Export cancelled") : err;
        MessageManager::callAsync ([self, msg] { if (self != nullptr) self->finishExportOnMessageThread (false, msg); });
        return;
    }
    std::thread ([sink, self]
    {
        String e;
        const bool done = sink->finish (e);
        MessageManager::callAsync ([self, done, e] { if (self != nullptr) self->finishExportOnMessageThread (done, e); });
    }).detach();
}

void GLCanvas::finishExportOnMessageThread (bool ok, const String& msg)
{
    for (auto& r : restoreValues) r.first->setValueNotifyingHost (r.second);
    restoreValues.clear();
    exportPhase = 0;
    if (onExportFinished) onExportFinished (ok, ok ? exportOutputPath : msg);
}

//==============================================================================
EQCurve::EQCurve (PrismProcessor& p) : proc (p)
{
    for (int i = 0; i < kNumEq; ++i) params[i] = proc.apvts.getParameter (kEqIds[i]);
    setSize (300, 64);
    startTimerHz (30);
}

Point<float> EQCurve::nodePos (int i, float v) const
{
    const float x = 8.0f + (float) i / (kNumEq - 1) * ((float) getWidth() - 16.0f);
    const float y = (float) getHeight() * 0.5f - v * ((float) getHeight() * 0.5f - 8.0f);
    return { x, y };
}

void EQCurve::paint (Graphics& g)
{
    float v[kNumEq];
    for (int i = 0; i < kNumEq; ++i) v[i] = params[i]->convertFrom0to1 (params[i]->getValue());

    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.drawHorizontalLine (getHeight() / 2, 0.0f, (float) getWidth());

    Path curve;
    const int w = getWidth();
    for (int x = 0; x <= w; x += 3)
    {
        const float t = jlimit (0.0f, 1.0f, ((float) x - 8.0f) / ((float) w - 16.0f));
        const float y = nodePos (0, eqAt (v, t)).y;
        if (x == 0) curve.startNewSubPath ((float) x, y); else curve.lineTo ((float) x, y);
    }
    g.setColour (kAccent);
    g.strokePath (curve, PathStrokeType (2.0f));
    for (int i = 0; i < kNumEq; ++i)
    {
        const auto p = nodePos (i, v[i]);
        g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
    }
}

void EQCurve::setNode (int i, float v)
{
    params[i]->setValueNotifyingHost (params[i]->convertTo0to1 (jlimit (-1.0f, 1.0f, v)));
}

void EQCurve::mouseDown (const MouseEvent& e)
{
    const float x = ((float) e.x - 8.0f) / ((float) getWidth() - 16.0f);
    active = jlimit (0, kNumEq - 1, roundToInt (x * (kNumEq - 1)));
    params[active]->beginChangeGesture();
    mouseDrag (e);
}

void EQCurve::mouseDrag (const MouseEvent& e)
{
    if (active < 0) return;
    const float half = (float) getHeight() * 0.5f - 8.0f;
    setNode (active, ((float) getHeight() * 0.5f - (float) e.y) / half);
}

void EQCurve::mouseUp (const MouseEvent&)
{
    if (active >= 0) params[active]->endChangeGesture();
    active = -1;
}

void EQCurve::mouseDoubleClick (const MouseEvent&)
{
    for (int i = 0; i < kNumEq; ++i)
    {
        params[i]->beginChangeGesture();
        setNode (i, 0.0f);
        params[i]->endChangeGesture();
    }
}

//==============================================================================
TunePanel::TunePanel (PrismProcessor& p) : proc (p), eqCurve (p)
{
    auto sectionHeader = [this] (const String& t) { addHeader (t); };

    addCombo ("scene", "scene");
    for (auto* id : { "react", "chaos", "trails", "heat", "decay", "brightness", "glow", "gain" }) addSlider (id);

    eqLabel.setText ("input EQ", dontSendNotification);
    eqLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.55f));
    addAndMakeVisible (eqLabel); addAndMakeVisible (eqCurve);
    items.push_back ({ &eqLabel, nullptr, 20, true });
    items.push_back ({ &eqCurve, nullptr, 68, true });

    for (auto* id : { "symmetry", "tiles", "twist", "slice", "lens" }) addSlider (id);
    addCombo ("lensStyle", "lens style", true);
    for (auto* id : { "spin", "hueDrift", "vignette" }) addSlider (id);
    addCombo ("palette", "palette");
    addSlider ("hue");

    randomButton.setColour (TextButton::textColourOffId, kAccent);
    randomButton.setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.04f));
    randomButton.onClick = [this] { PrismProcessor::randomizeLook (proc.apvts); };
    addAndMakeVisible (randomButton);
    items.push_back ({ &randomButton, nullptr, 40, true });

    sectionHeader ("scene");
    for (auto* id : { "speed", "complexity", "fill", "contrast" }) addSlider (id);
    addCombo ("pattern", "pattern");
    addCombo ("shape", "shape");
    addCombo ("material", "material");

    if (lensStyleRow != nullptr)
    {
        auto upd = [this]
        {
            const bool on = proc.apvts.getRawParameterValue ("lens")->load() >= 1.0f;
            lensStyleRow->label.setAlpha (on ? 1.0f : 0.4f);
            lensStyleRow->combo.setAlpha (on ? 1.0f : 0.4f);
        };
        for (auto& r : rows)
            if (r.slider.getName() == "lens") r.slider.onValueChange = upd;
        upd();
    }
}

void TunePanel::addSlider (const char* id)
{
    const auto& d = findParam (id);
    auto& r = rows.emplace_back();
    r.label.setText (d.label, dontSendNotification);
    r.label.setColour (Label::textColourId, Colours::white.withAlpha (0.55f));
    r.slider.setName (id);
    r.slider.setSliderStyle (Slider::LinearHorizontal);
    r.slider.setTextBoxStyle (Slider::TextBoxRight, false, 40, 20);
    r.slider.setNumDecimalPlacesToDisplay (0);
    r.slider.setColour (Slider::trackColourId, kAccent);
    r.slider.setColour (Slider::thumbColourId, Colours::white);
    r.slider.setColour (Slider::textBoxTextColourId, kAccent);
    r.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    r.sAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, r.slider);
    addAndMakeVisible (r.label); addAndMakeVisible (r.slider);
    items.push_back ({ &r.label, &r.slider, 28, false });
}

void TunePanel::addCombo (const char* id, const char* label, bool)
{
    auto& r = rows.emplace_back();
    r.label.setText (label, dontSendNotification);
    r.label.setColour (Label::textColourId, Colours::white.withAlpha (0.55f));
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        r.combo.addItemList (choice->choices, 1);
    r.cAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, r.combo);
    if (String (id) == "lensStyle") lensStyleRow = &r;
    addAndMakeVisible (r.label); addAndMakeVisible (r.combo);
    items.push_back ({ &r.label, &r.combo, 30, false });
}

void TunePanel::addHeader (const String& text)
{
    auto& l = headers.emplace_back();
    l.setText (text.toUpperCase(), dontSendNotification);
    l.setFont (Font (FontOptions (11.0f)));
    l.setColour (Label::textColourId, kAccent);
    addAndMakeVisible (l);
    items.push_back ({ &l, nullptr, 34, true });
}

void TunePanel::resized()
{
    int y = 6;
    const int w = getWidth();
    for (auto& it : items)
    {
        if (it.fullWidth || it.b == nullptr)
            it.a->setBounds (12, y, w - 24, it.height - 4);
        else
        {
            it.a->setBounds (12, y, 92, it.height - 4);
            it.b->setBounds (108, y, w - 120, it.height - 4);
        }
        y += it.height;
    }
    contentHeight = y + 8;
    setSize (w, contentHeight);
}

//==============================================================================
PrismEditor::PrismEditor (PrismProcessor& p)
    : AudioProcessorEditor (&p), proc (p), canvas (p), panel (p), autoPanel (p.automation)
{
    addAndMakeVisible (canvas);

    title.setText ("tune", dontSendNotification);
    title.setColour (Label::textColourId, Colours::white);
    addAndMakeVisible (title);

    autoButton.setClickingTogglesState (true);
    autoButton.setColour (TextButton::buttonColourId, Colours::white.withAlpha (0.05f));
    autoButton.setColour (TextButton::buttonOnColourId, kAccent.withAlpha (0.3f));
    autoButton.setColour (TextButton::textColourOffId, Colours::white.withAlpha (0.85f));
    autoButton.onClick = [this] { autoPanel.setVisible (autoButton.getToggleState()); resized(); };
    addAndMakeVisible (autoButton);
    addChildComponent (autoPanel);

    // preview resolution (lower = lighter on the GPU; export is unaffected)
    const char* pvNames[] = { "Preview 100%", "Preview 75%", "Preview 50%", "Preview 35%", "Preview 25%" };
    const float pvScales[] = { 1.0f, 0.75f, 0.5f, 0.35f, 0.25f };
    for (int i = 0; i < 5; ++i) previewBox.addItem (pvNames[i], i + 1);
    int pvSel = 1;
    for (int i = 0; i < 5; ++i) if (std::abs (pvScales[i] - proc.previewScale.load()) < 0.02f) pvSel = i + 1;
    previewBox.setSelectedId (pvSel, dontSendNotification);
    previewBox.setTooltip ("Preview render resolution. Lower it to use less GPU. Exports are not affected.");
    previewBox.onChange = [this, pvScales] { proc.previewScale = pvScales[jlimit (0, 4, previewBox.getSelectedId() - 1)]; };
    addAndMakeVisible (previewBox);

    exportButton.setColour (TextButton::buttonColourId, kAccent.withAlpha (0.16f));
    exportButton.setColour (TextButton::textColourOffId, kAccent);
    exportButton.onClick = [this] { onExportClicked(); };
    addAndMakeVisible (exportButton);
    canvas.onExportFinished = [this] (bool ok, String msg)
    {
        exportButton.setEnabled (true);
        exportButton.setButtonText ("EXPORT VIDEO");
        if (ok)
        {
            File (msg).revealToUser();
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Export finished", "Saved: " + msg);
        }
        else
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Export", msg);
    };
    startTimerHz (5);

    viewport.setViewedComponent (&panel, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (760, 460, 2200, 1400);
    setSize (1180, 700);
}

void PrismEditor::paint (Graphics& g)
{
    g.fillAll (kPanel);
}

void PrismEditor::resized()
{
    auto area = getLocalBounds();
    auto right = area.removeFromRight (344);
    if (autoPanel.isVisible()) autoPanel.setBounds (area.removeFromBottom (AutomationPanel::preferredHeight));
    canvas.setBounds (area);
    auto headArea = right.removeFromTop (68).reduced (14, 6);
    auto header = headArea.removeFromTop (24);
    autoButton.setBounds (header.removeFromRight (110));
    title.setBounds (header);
    headArea.removeFromTop (6);
    exportButton.setBounds (headArea.removeFromRight (130));
    headArea.removeFromRight (8);
    previewBox.setBounds (headArea);
    viewport.setBounds (right);
    panel.setSize (right.getWidth() - viewport.getScrollBarThickness(), panel.getContentHeight());
    panel.resized();
}

//==============================================================================
void PrismEditor::timerCallback()
{
    const int phase = canvas.getExportPhase();
    if (phase == 1)
        exportButton.setButtonText ("CANCEL " + String (roundToInt (canvas.getExportProgress() * 100.0f)) + "%");
    else if (phase == 2)
    {
        exportButton.setButtonText ("FINISHING...");
        exportButton.setEnabled (false);
    }
}

void PrismEditor::onExportClicked()
{
    if (canvas.getExportPhase() == 1) { canvas.cancelExport(); return; }
    if (canvas.getExportPhase() != 0) return;
    if (! VideoSink::isAvailable())
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Export",
                                          "Video export is only available in the macOS version for now. Use the web version to export on other systems.");
        return;
    }
    askExportOptions();
}

void PrismEditor::askExportOptions()
{
    auto* aw = new AlertWindow ("Export video",
                                "The visuals are rendered offline, frame by frame, from an audio file you choose "
                                "(WAV, AIFF, MP3, M4A, FLAC). Automation lanes are included. Your current look is used.",
                                MessageBoxIconType::NoIcon, this);
    aw->addComboBox ("res", { "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160 (4K)" }, "Resolution");
    aw->addComboBox ("fps", { "24", "30", "60" }, "Frame rate");
    aw->addComboBox ("mbps", { "8 Mbps - good", "16 Mbps - high", "30 Mbps - very high", "60 Mbps - maximum" }, "Quality");
    aw->getComboBoxComponent ("res")->setSelectedItemIndex (1);
    aw->getComboBoxComponent ("fps")->setSelectedItemIndex (1);
    aw->getComboBoxComponent ("mbps")->setSelectedItemIndex (1);
    aw->addButton ("Choose audio...", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int r)
    {
        if (r != 1) return;
        const int sizes[][2] = { { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } };
        const int fpss[] = { 24, 30, 60 };
        const double rates[] = { 8.0, 16.0, 30.0, 60.0 };
        const int ri = jlimit (0, 3, aw->getComboBoxComponent ("res")->getSelectedItemIndex());
        exportSettings.width = sizes[ri][0]; exportSettings.height = sizes[ri][1];
        exportSettings.fps = fpss[jlimit (0, 2, aw->getComboBoxComponent ("fps")->getSelectedItemIndex())];
        exportSettings.mbps = rates[jlimit (0, 3, aw->getComboBoxComponent ("mbps")->getSelectedItemIndex())];
        MessageManager::callAsync ([this] { chooseAudioThenOutput(); });
    }), true);
}

void PrismEditor::chooseAudioThenOutput()
{
    chooser = std::make_unique<FileChooser> ("Choose the audio file to render", File::getSpecialLocation (File::userMusicDirectory),
                                             "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.caf");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto audio = fc.getResult();
        if (! audio.existsAsFile()) return;
        exportSettings.audio = audio;
        MessageManager::callAsync ([this, audio]
        {
            chooser = std::make_unique<FileChooser> ("Save the video as",
                          File::getSpecialLocation (File::userMoviesDirectory).getChildFile (audio.getFileNameWithoutExtension() + " - prism.mp4"), "*.mp4");
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const FileChooser& fc2)
            {
                auto out = fc2.getResult();
                if (out == File()) return;
                exportSettings.output = out.withFileExtension ("mp4");
                String err;
                if (! canvas.startExport (exportSettings, err))
                {
                    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Export", err);
                    return;
                }
                exportButton.setButtonText ("CANCEL 0%");
            });
        });
    });
}
