#pragma once
#include <cmath>
#include <cstring>

// Single table of continuous parameters shared by processor, renderer and editor.
struct FloatParam { const char* id; const char* label; float min, max, def, step; };

inline constexpr FloatParam kFloatParams[] = {
    // look
    { "react",      "react",       0.0f, 100.0f, 50.0f, 1.0f },
    { "chaos",      "chaos",       0.0f, 100.0f, 35.0f, 1.0f },
    { "trails",     "trails",      0.0f, 100.0f, 55.0f, 1.0f },
    { "heat",       "heat",        0.0f, 100.0f, 50.0f, 1.0f },
    { "decay",      "decay",       0.0f, 100.0f, 45.0f, 1.0f },
    { "brightness", "brightness",  0.0f, 100.0f, 50.0f, 1.0f },
    { "glow",       "glow",        0.0f, 100.0f, 40.0f, 1.0f },
    { "gain",       "input gain",  0.0f, 100.0f, 50.0f, 1.0f },
    // shape
    { "symmetry",   "symmetry",    0.0f,  12.0f,  0.0f, 1.0f },
    { "tiles",      "tiles",       0.0f,   8.0f,  0.0f, 1.0f },
    { "twist",      "twist",    -100.0f, 100.0f,  0.0f, 1.0f },
    { "slice",      "slice",       0.0f, 100.0f,  0.0f, 1.0f },
    { "lens",       "lens",        0.0f, 100.0f,  0.0f, 1.0f },
    { "spin",       "spin",     -100.0f, 100.0f,  0.0f, 1.0f },
    { "hueDrift",   "hue drift",   0.0f, 100.0f, 20.0f, 1.0f },
    { "vignette",   "vignette",    0.0f, 100.0f, 25.0f, 1.0f },
    { "hue",        "hue",         0.0f, 100.0f,  0.0f, 1.0f },
    // scene
    { "speed",      "speed",       0.0f, 100.0f, 50.0f, 1.0f },
    { "complexity", "complexity",  0.0f, 100.0f, 50.0f, 1.0f },
    { "fill",       "fill",        0.0f, 100.0f, 50.0f, 1.0f },
    { "contrast",   "contrast",    0.0f, 100.0f, 50.0f, 1.0f },
};

inline const FloatParam& findParam (const char* id)
{
    for (const auto& p : kFloatParams)
        if (std::strcmp (p.id, id) == 0) return p;
    return kFloatParams[0];
}

inline constexpr int kNumEq = 6;
inline constexpr const char* kEqIds[kNumEq] = { "eq0", "eq1", "eq2", "eq3", "eq4", "eq5" };

// Smooth interpolation of the 6 EQ nodes, t in 0..1 (log-frequency left to right).
inline float eqAt (const float* eq, float t)
{
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const float x = t * (kNumEq - 1);
    int i = (int) x;
    if (i > kNumEq - 2) i = kNumEq - 2;
    float f = x - (float) i;
    f = f * f * (3.0f - 2.0f * f);
    return eq[i] * (1.0f - f) + eq[i + 1] * f;
}

inline constexpr const char* kSceneNames[]   = { "Tunnel", "Orbits", "Waves", "Ink", "Particles", "Vortex", "Sparks", "Forms" };
inline constexpr const char* kLensNames[]    = { "glass ribs", "fisheye", "ripple", "prism" };
inline constexpr const char* kPatternNames[] = { "flower of life", "asanoha", "metatron" };
inline constexpr const char* kShapeNames[]   = { "blob", "torus", "cube", "metaballs", "crystal" };
inline constexpr const char* kMaterialNames[] = { "iridescent", "chrome", "glass", "pearl", "matte", "neon" };
inline constexpr const char* kPaletteNames[] = { "nocturne", "aurora", "ember", "ocean", "sunset", "acid", "mono", "rose" };

struct Palette { float a[3], b[3], c[3], d[3]; };
inline constexpr Palette kPalettes[] = {
    { { 0.25f, 0.30f, 0.50f }, { 0.25f, 0.30f, 0.45f }, { 1, 1, 1 },       { 0.60f, 0.72f, 0.85f } },
    { { 0.20f, 0.50f, 0.45f }, { 0.30f, 0.50f, 0.35f }, { 1, 1, 0.8f },    { 0.35f, 0.10f, 0.55f } },
    { { 0.50f, 0.28f, 0.12f }, { 0.50f, 0.30f, 0.12f }, { 1, 1, 0.8f },    { 0.00f, 0.12f, 0.30f } },
    { { 0.10f, 0.40f, 0.55f }, { 0.15f, 0.35f, 0.35f }, { 1, 1, 1 },       { 0.50f, 0.60f, 0.75f } },
    { { 0.55f, 0.35f, 0.40f }, { 0.45f, 0.35f, 0.40f }, { 1, 1, 1 },       { 0.00f, 0.20f, 0.45f } },
    { { 0.5f, 0.5f, 0.5f },    { 0.5f, 0.5f, 0.5f },    { 1, 1, 1 },       { 0.00f, 0.33f, 0.67f } },
    { { 0.55f, 0.55f, 0.55f }, { 0.45f, 0.45f, 0.45f }, { 1, 1, 1 },       { 0, 0, 0 } },
    { { 0.62f, 0.30f, 0.34f }, { 0.30f, 0.28f, 0.26f }, { 0.6f, 0.6f, 0.7f }, { 0.00f, 0.05f, 0.12f } },
};
