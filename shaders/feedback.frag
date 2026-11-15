in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;
uniform sampler2D uPrev;
uniform float uTime;
uniform float uKeep;     // per-frame persistence (already dt-corrected)
uniform float uChaos;
uniform float uBeat;
uniform float uHeat;     // 0..1
uniform float uTint;     // 0..1 colour drift of the trails
uniform float uDt60;     // dt * 60

void main()
{
    vec2 c = vUV - 0.5;
    vec2 f = vec2(fbm(vUV * 3.0 + vec2(uTime * 0.07, 0.0)),
                  fbm(vUV * 3.0 + vec2(9.1, -uTime * 0.06))) - 0.47;
    float z = 1.0 - (0.0025 + 0.006 * uBeat) * uDt60;
    vec2 suv = c * z + f * uChaos * 0.02 * uDt60 + vec2(0.0, -(uHeat - 0.5) * 0.003 * uDt60) + 0.5;

    vec3 prev = texture(uPrev, suv).rgb;
    prev = mix(prev, prev.gbr, uTint * 0.03 * uDt60);
    prev = max(prev * uKeep - 0.002, vec3(0.0));

    vec3 s = texture(uScene, vUV).rgb;
    fragColor = vec4(max(s, prev), 1.0);
}
