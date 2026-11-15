#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

// Real-time audio analysis: log-spaced spectrum bars, 4 bands, level and a
// beat pulse. Fed from the audio thread, read (lock-free) from the GL thread.
class Analyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    static constexpr int hopSize  = 512;
    static constexpr int numBars  = 64;

    Analyzer()
        : fft (fftOrder),
          window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        prepare (44100.0);
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        ring.fill (0.0f);
        fftData.fill (0.0f);
        smoothed.fill (0.0f);
        writePos = 0;
        hopCounter = 0;
        prevBass = avgFlux = 0.0f;
        cooldown = 0;
        beatValue = 0.0f;

        for (auto& b : bars) b.store (0.0f);
        bass.store (0.0f); lowMid.store (0.0f); mid.store (0.0f); high.store (0.0f);
        level.store (0.0f); beat.store (0.0f);

        // bar -> bin range (log spaced 30 Hz .. 16 kHz)
        const double fMin = 30.0, fMax = std::min (16000.0, sampleRate * 0.45);
        for (int b = 0; b < numBars; ++b)
        {
            const double f0 = fMin * std::pow (fMax / fMin, (double) b / numBars);
            const double f1 = fMin * std::pow (fMax / fMin, (double) (b + 1) / numBars);
            binLo[(size_t) b] = juce::jlimit (1, fftSize / 2 - 1, (int) std::floor (f0 * fftSize / sampleRate));
            binHi[(size_t) b] = juce::jlimit (binLo[(size_t) b] + 1, fftSize / 2,
                                              (int) std::ceil (f1 * fftSize / sampleRate));
        }

        auto barOf = [&] (double f) { return (int) std::round (numBars * std::log (f / fMin) / std::log (fMax / fMin)); };
        bassEnd    = juce::jlimit (1, numBars - 3, barOf (150.0));
        lowMidEnd  = juce::jlimit (bassEnd + 1, numBars - 2, barOf (800.0));
        midEnd     = juce::jlimit (lowMidEnd + 1, numBars - 1, barOf (4000.0));
        cooldownHops = std::max (1, (int) (0.12 * sampleRate / hopSize));
    }

    // audio thread
    void pushSample (float x) noexcept
    {
        ring[(size_t) writePos] = x;
        writePos = (writePos + 1) & (fftSize - 1);
        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            analyse();
        }
    }

    // any thread
    float getBar (int i) const noexcept { return bars[(size_t) i].load (std::memory_order_relaxed); }
    float getBass()   const noexcept { return bass.load (std::memory_order_relaxed); }
    float getLowMid() const noexcept { return lowMid.load (std::memory_order_relaxed); }
    float getMid()    const noexcept { return mid.load (std::memory_order_relaxed); }
    float getHigh()   const noexcept { return high.load (std::memory_order_relaxed); }
    float getLevel()  const noexcept { return level.load (std::memory_order_relaxed); }
    float getBeat()   const noexcept { return beat.load (std::memory_order_relaxed); }

private:
    void analyse() noexcept
    {
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((writePos + i) & (fftSize - 1))];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        constexpr float refMag = fftSize * 0.25f; // full-scale sine with Hann window
        std::array<float, numBars> target {};

        for (int b = 0; b < numBars; ++b)
        {
            float m = 0.0f;
            for (int k = binLo[(size_t) b]; k < binHi[(size_t) b]; ++k)
                m = std::max (m, fftData[(size_t) k]);

            const float db = 20.0f * std::log10 (m / refMag + 1.0e-9f);
            target[(size_t) b] = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 66.0f);
        }

        float total = 0.0f;
        for (int b = 0; b < numBars; ++b)
        {
            auto& s = smoothed[(size_t) b];
            const float t = target[(size_t) b];
            s = t > s ? s + (t - s) * 0.75f : s * 0.90f + t * 0.10f;
            bars[(size_t) b].store (s, std::memory_order_relaxed);
            total += s;
        }

        auto avg = [&] (const std::array<float, numBars>& a, int from, int to)
        {
            float acc = 0.0f;
            for (int i = from; i < to; ++i) acc += a[(size_t) i];
            return acc / (float) std::max (1, to - from);
        };

        bass.store   (avg (smoothed, 0, bassEnd));
        lowMid.store (avg (smoothed, bassEnd, lowMidEnd));
        mid.store    (avg (smoothed, lowMidEnd, midEnd));
        high.store   (avg (smoothed, midEnd, numBars));
        level.store  (total / numBars);

        // beat = bass spectral flux above its running average
        const float bassNow = avg (target, 0, bassEnd);
        const float flux = std::max (0.0f, bassNow - prevBass);
        prevBass = bassNow;

        if (cooldown > 0) --cooldown;
        if (flux > avgFlux * 2.2f + 0.04f && bassNow > 0.35f && cooldown == 0)
        {
            beatValue = 1.0f;
            cooldown = cooldownHops;
        }
        avgFlux = avgFlux * 0.95f + flux * 0.05f;
        beatValue *= 0.90f;
        beat.store (beatValue, std::memory_order_relaxed);
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBars> smoothed {};
    std::array<int, numBars> binLo {}, binHi {};

    int writePos = 0, hopCounter = 0;
    int bassEnd = 16, lowMidEnd = 33, midEnd = 50;
    int cooldown = 0, cooldownHops = 11;
    float prevBass = 0.0f, avgFlux = 0.0f, beatValue = 0.0f;
    double sampleRate = 44100.0;

    std::array<std::atomic<float>, numBars> bars;
    std::atomic<float> bass { 0.0f }, lowMid { 0.0f }, mid { 0.0f }, high { 0.0f },
                       level { 0.0f }, beat { 0.0f };
};
