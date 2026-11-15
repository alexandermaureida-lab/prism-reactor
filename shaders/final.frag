in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2  uRes;
uniform float uClock;
uniform float uLevel;
uniform float uBeat;
uniform float uGlow;
uniform float uBright;   // 0..1
uniform float uHeat;     // 0..1
uniform float uLens;     // 0..1
uniform int   uLensStyle;
uniform float uVig;      // 0..1

void main()
{
    float asp = uRes.x / uRes.y;
    vec2 uv = vUV;
    vec2 c = uv - 0.5;
    c.x *= asp;
    float r = length(c);

    // heat haze
    float haze = max(0.0, uHeat - 0.5) * 2.0;
    uv.x += haze * 0.006 * (0.3 + uLevel) * sin(uv.y * 50.0 + uClock * 3.0);
    uv.y += haze * 0.003 * (0.3 + uLevel) * sin(uv.x * 35.0 - uClock * 2.0);

    vec2 offR = vec2(0.0), offB = vec2(0.0);
    if (uLens > 0.01)
    {
        if (uLensStyle == 0)            // glass ribs
        {
            float ribs = 24.0;
            float f = fract(uv.x * ribs) - 0.5;
            uv.x -= f * uLens * 0.06;
            offR.x = f * uLens * 0.012;
            offB.x = -offR.x;
        }
        else if (uLensStyle == 1)       // fisheye
        {
            vec2 cc = c * mix(1.0, 0.55 + 0.9 * r, uLens);
            uv = cc / vec2(asp, 1.0) + 0.5;
        }
        else if (uLensStyle == 2)       // ripple
        {
            vec2 dir = c / (r + 1e-3) / vec2(asp, 1.0);
            uv += dir * sin(r * 45.0 - uClock * 4.0) * 0.012 * uLens * (0.4 + uLevel + uBeat);
        }
        else                            // prism
        {
            vec2 dir = c / (r + 1e-3) / vec2(asp, 1.0);
            offR = dir * uLens * 0.02 * (0.5 + uBeat * 2.0);
            offB = -offR;
        }
    }

    vec3 col = vec3(texture(uTex, uv + offR).r, texture(uTex, uv).g, texture(uTex, uv + offB).b);

    // glow: ring of taps
    if (uGlow > 0.01)
    {
        float rad = 0.004 + 0.012 * uGlow;
        vec3 blur = vec3(0.0);
        for (int i = 0; i < 8; ++i)
        {
            float a = float(i) * 0.785398;
            blur += texture(uTex, uv + vec2(cos(a) / asp, sin(a)) * rad).rgb;
            blur += texture(uTex, uv + vec2(cos(a) / asp, sin(a)) * rad * 2.2).rgb * 0.6;
        }
        col += blur * (1.0 / 12.8) * uGlow * 0.9;
    }

    col *= exp2((uBright - 0.5) * 3.0);

    float temp = (uHeat - 0.5) * 2.0;
    col *= vec3(1.0 + 0.25 * temp, 1.0, 1.0 - 0.25 * temp);
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col += vec3(0.30, 0.10, 0.0) * pow(lum, 3.0) * max(temp, 0.0);

    vec2 q = vUV - 0.5;
    col *= max(0.0, 1.0 - uVig * dot(q, q) * 2.2);
    col += 0.02 * uBeat;
    col += (hash21(vUV * uRes + uClock) - 0.5) / 255.0;
    col = clamp(col, 0.0, 1.0);
    fragColor = vec4(col, 1.0);
}
