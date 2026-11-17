#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout PrismProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::AudioParameterChoice;
    using juce::ParameterID;
    using juce::NormalisableRange;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto names = [] (auto& arr) { juce::StringArray s; for (auto* n : arr) s.add (n); return s; };
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scene", 1 }, "Scene", names (kSceneNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lensStyle", 1 }, "Lens style", names (kLensNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "transition", 1 }, "Scene change", names (kTransitionNames), 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "palette", 1 }, "Palette", names (kPaletteNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", names (kPatternNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape", names (kShapeNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "material", 1 }, "Material", names (kMaterialNames), 0));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mxBlend", 1 }, "Image blend", names (kBlendNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mxFit", 1 }, "Image fit", names (kFitNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mxEdges", 1 }, "Image edges", names (kEdgeNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mxDrive", 1 }, "Image driven by", names (kDriveNames), 0));

    for (const auto& d : kFloatParams)
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.label,
                        NormalisableRange<float> (d.min, d.max, d.step), d.def));

    for (int i = 0; i < kNumEq; ++i)
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { kEqIds[i], 1 },
                        "Input EQ " + juce::String (i + 1) + (i == 0 ? " (bass)" : i == kNumEq - 1 ? " (treble)" : ""), NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    return { p.begin(), p.end() };
}

void PrismProcessor::randomizeLook (APVTS& apvts)
{
    juce::Random r;
    auto setv = [&] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    auto rnd = [&] (float a, float b) { return a + r.nextFloat() * (b - a); };
    auto chance = [&] (float p) { return r.nextFloat() < p; };

    setv ("chaos", std::round (rnd (15, 85)));   setv ("trails", std::round (rnd (30, 85)));
    setv ("heat", std::round (rnd (25, 80)));    setv ("decay", std::round (rnd (20, 70)));
    setv ("symmetry", chance (0.55f) ? std::round (rnd (2, 10)) : 0.0f);
    setv ("tiles", chance (0.25f) ? std::round (rnd (1, 3)) : 0.0f);
    setv ("twist", chance (0.6f) ? std::round (rnd (-60, 60)) : 0.0f);
    setv ("slice", chance (0.3f) ? std::round (rnd (8, 45)) : 0.0f);
    setv ("lens", chance (0.5f) ? std::round (rnd (15, 65)) : 0.0f);
    setv ("lensStyle", (float) r.nextInt (4));
    setv ("spin", std::round (rnd (-30, 30)));   setv ("hueDrift", std::round (rnd (0, 60)));
    setv ("vignette", std::round (rnd (10, 60)));
    setv ("palette", (float) r.nextInt ((int) std::size (kPaletteNames)));
    setv ("hue", std::round (rnd (0, 100)));
    setv ("pattern", (float) r.nextInt ((int) std::size (kPatternNames)));
    setv ("shape", (float) r.nextInt ((int) std::size (kShapeNames)));
    setv ("material", (float) r.nextInt ((int) std::size (kMaterialNames)));
    for (auto* id : kEqIds) setv (id, chance (0.5f) ? 0.0f : rnd (-0.5f, 0.5f));
}

namespace
{
struct FactoryPreset { const char* name; std::vector<std::pair<const char*, float>> v; };

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = {
        { "Init", {} },
        { "Neon Orbits",      { { "scene", 1 }, { "palette", 5 }, { "trails", 70 }, { "glow", 60 }, { "spin", 20 } } },
        { "Liquid Ink",       { { "scene", 3 }, { "palette", 3 }, { "trails", 80 }, { "decay", 30 }, { "heat", 65 }, { "twist", 25 } } },
        { "Sacred Flower",    { { "scene", 5 }, { "pattern", 0 }, { "palette", 1 }, { "symmetry", 6 }, { "glow", 55 }, { "spin", 10 } } },
        { "Asanoha Gold",     { { "scene", 5 }, { "pattern", 1 }, { "palette", 2 }, { "brightness", 55 }, { "contrast", 60 } } },
        { "Iridescent Blob",  { { "scene", 7 }, { "shape", 0 }, { "material", 0 }, { "palette", 1 }, { "trails", 40 } } },
        { "Chrome Crystal",   { { "scene", 7 }, { "shape", 4 }, { "material", 1 }, { "palette", 3 }, { "glow", 50 } } },
        { "Particle Storm",   { { "scene", 4 }, { "palette", 4 }, { "trails", 75 }, { "chaos", 60 } } },
        { "Kaleido Waves",    { { "scene", 2 }, { "symmetry", 8 }, { "tiles", 1 }, { "lens", 40 }, { "lensStyle", 3 }, { "palette", 7 } } },
        { "Pearl Tunnel",     { { "scene", 0 }, { "palette", 6 }, { "twist", 35 }, { "lens", 30 }, { "lensStyle", 1 }, { "vignette", 45 } } },
    };
    return list;
}
}

int PrismProcessor::numFactoryPresets() { return (int) factoryPresets().size(); }
juce::String PrismProcessor::factoryPresetName (int i) { return factoryPresets()[(size_t) juce::jlimit (0, numFactoryPresets() - 1, i)].name; }
int PrismProcessor::getNumPrograms() { return numFactoryPresets(); }
const juce::String PrismProcessor::getProgramName (int i) { return factoryPresetName (i); }

void PrismProcessor::setCurrentProgram (int idx)
{
    idx = juce::jlimit (0, numFactoryPresets() - 1, idx);
    currentProgram = idx;
    auto setv = [this] (const juce::String& id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    // reset the look (not the input gain, EQ or image layer), then apply the preset
    for (const auto& d : kFloatParams)
        if (! isMediaParam (d.id) && std::strcmp (d.id, "gain") != 0 && std::strcmp (d.id, "transTime") != 0) setv (d.id, d.def);
    for (auto* id : { "scene", "lensStyle", "palette", "pattern", "shape", "material" }) setv (id, 0.0f);
    for (auto& kv : factoryPresets()[(size_t) idx].v) setv (kv.first, kv.second);
}

bool PrismProcessor::savePresetFile (const juce::File& f)
{
    auto state = apvts.copyState();
    state.removeProperty ("automation", nullptr);
    state.removeProperty ("previewScale", nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (f.withFileExtension ("prism"));
    return false;
}

bool PrismProcessor::loadPresetFile (const juce::File& f)
{
    if (auto xml = juce::parseXML (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                auto c = tree.getChild (i);
                if (auto* prm = apvts.getParameter (c.getProperty ("id").toString()))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) c.getProperty ("value")));
                    prm->endChangeGesture();
                }
            }
            return true;
        }
    return false;
}

PrismProcessor::PrismProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PRISM", createLayout())
{
    gainParam = apvts.getRawParameterValue ("gain");
    formats.registerBasicFormats();
    writerThread.startThread();
    automation.hostClock = [this] (double& t, bool& pl, double& b) { return readHostClock (t, pl, b); };
}

PrismProcessor::~PrismProcessor()
{
    stopRecording();
    writerThread.stopThread (1000);
    transport.setSource (nullptr);
}

bool PrismProcessor::readHostClock (double& seconds, bool& playing, double& bpm) const
{
    const auto age = juce::Time::currentTimeMillis() - lastBlockMs.load();
    if (! hostValid.load() || age > 700) return false;
    seconds = hostTime.load(); playing = hostPlaying.load(); bpm = hostBpm.load();
    return true;
}

void PrismProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    analyzer.prepare (sampleRate);
    transport.prepareToPlay (samplesPerBlock, sampleRate);
    fileBuf.setSize (2, juce::jmax (samplesPerBlock, 4096));
}

void PrismProcessor::releaseResources()
{
    transport.releaseResources();
}

bool PrismProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

bool PrismProcessor::loadAudioFile (const juce::File& f)
{
    auto* reader = formats.createReaderFor (f);
    if (reader == nullptr) return false;
    const double sr = reader->sampleRate;
    auto src = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transport.setSource (src.get(), 0, nullptr, sr);
    fileSource = std::move (src);
    loadedAudio = f;
    transport.setPosition (0.0);
    return true;
}

void PrismProcessor::filePlay (bool play)
{
    if (play && fileSource != nullptr)
    {
        if (transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.05) transport.setPosition (0.0);
        transport.start();
        useFile = true;
    }
    else transport.stop();
}

bool PrismProcessor::startRecording()
{
    if (recorder != nullptr || currentRate <= 0.0) return false;
    recordingFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("prism-live-" + juce::Time::getCurrentTime().formatted ("%H%M%S"), ".wav");
    std::unique_ptr<juce::FileOutputStream> stream (recordingFile.createOutputStream());
    if (stream == nullptr) return false;
    juce::WavAudioFormat wav;
    if (auto* w = wav.createWriterFor (stream.get(), currentRate, 2, 16, {}, 0))
    {
        stream.release();
        const juce::ScopedLock sl (recorderLock);
        recorder.reset (new juce::AudioFormatWriter::ThreadedWriter (w, writerThread, 32768));
        return true;
    }
    return false;
}

juce::File PrismProcessor::stopRecording()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> r;
    {
        const juce::ScopedLock sl (recorderLock);
        r = std::move (recorder);
    }
    if (r == nullptr) return {};
    r.reset();   // flushes
    lastRecording = recordingFile;
    return lastRecording;
}

void PrismProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // host transport for the automation clock
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSeconds()) { hostTime = *t; hostValid = true; } else hostValid = false;
            if (auto b = pos->getBpm()) hostBpm = *b;
            lastBlockMs = juce::Time::currentTimeMillis();
        }
        else hostValid = false;
    else hostValid = false;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0) return;

    // audio from the built-in file player replaces the input (so it is heard and analysed)
    if (useFile.load() && fileSource != nullptr && fileBuf.getNumSamples() >= numSamples)
    {
        fileBuf.clear (0, numSamples);
        juce::AudioSourceChannelInfo info (&fileBuf, 0, numSamples);
        transport.getNextAudioBlock (info);
        for (int c = 0; c < numChannels; ++c)
            buffer.copyFrom (c, 0, fileBuf, juce::jmin (c, 1), 0, numSamples);
    }
    else if (getTotalNumInputChannels() == 0)
        buffer.clear();

    if (const juce::ScopedTryLock sl (recorderLock); sl.isLocked() && recorder != nullptr)
    {
        const float* ch[2] = { buffer.getReadPointer (0), buffer.getReadPointer (juce::jmin (1, numChannels - 1)) };
        recorder->write (ch, numSamples);
    }

    const float gain = juce::Decibels::decibelsToGain ((gainParam->load() - 50.0f) * 0.48f);
    const float norm = gain / (float) numChannels;

    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            s += buffer.getReadPointer (c)[i];
        analyzer.pushSample (s * norm);
    }
}

bool PrismProcessor::setMediaFile (const juce::File& f)
{
    auto img = juce::ImageFileFormat::loadFrom (f);
    if (! img.isValid()) return false;
    const int lim = 4096;
    if (juce::jmax (img.getWidth(), img.getHeight()) > lim)
    {
        const float k = (float) lim / (float) juce::jmax (img.getWidth(), img.getHeight());
        img = img.rescaled (juce::roundToInt ((float) img.getWidth() * k), juce::roundToInt ((float) img.getHeight() * k));
    }
    {
        const juce::ScopedLock sl (mediaLock);
        mediaImage = img.convertedToFormat (juce::Image::ARGB);
        mediaFile = f;
    }
    ++mediaVersion;
    return true;
}

void PrismProcessor::clearMedia()
{
    {
        const juce::ScopedLock sl (mediaLock);
        mediaImage = juce::Image();
        mediaFile = juce::File();
    }
    ++mediaVersion;
}

juce::AudioProcessorEditor* PrismProcessor::createEditor()
{
    return new PrismEditor (*this);
}

void PrismProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    apvts.state.setProperty ("automation", juce::JSON::toString (automation.toVar(), true), nullptr);
    apvts.state.setProperty ("previewScale", previewScale.load(), nullptr);
    apvts.state.setProperty ("mediaPath", mediaFile.getFullPathName(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PrismProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto saved = apvts.state.getProperty ("automation").toString();
            if (saved.isNotEmpty()) automation.fromVar (juce::JSON::parse (saved));
            if (apvts.state.hasProperty ("previewScale"))
            {
                const float ps = (float) apvts.state.getProperty ("previewScale");
                previewScale = ps < 0.0f ? -1.0f : juce::jlimit (0.2f, 1.0f, ps);
            }
            const auto mp = apvts.state.getProperty ("mediaPath").toString();
            if (mp.isNotEmpty() && juce::File (mp).existsAsFile()) setMediaFile (juce::File (mp));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismProcessor();
}
