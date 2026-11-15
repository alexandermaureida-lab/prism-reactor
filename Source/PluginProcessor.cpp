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
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "palette", 1 }, "Palette", names (kPaletteNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", names (kPatternNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape", names (kShapeNames), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "material", 1 }, "Material", names (kMaterialNames), 0));

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

PrismProcessor::PrismProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PRISM", createLayout())
{
    gainParam = apvts.getRawParameterValue ("gain");
}

void PrismProcessor::prepareToPlay (double sampleRate, int)
{
    analyzer.prepare (sampleRate);
}

bool PrismProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void PrismProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // The audio passes through untouched; we only listen to it.
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels());
    if (numChannels == 0) return;

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

juce::AudioProcessorEditor* PrismProcessor::createEditor()
{
    return new PrismEditor (*this);
}

void PrismProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PrismProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismProcessor();
}
