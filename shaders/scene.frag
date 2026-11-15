in vec2 vUV;
out vec4 fragColor;

uniform vec2  uRes;
uniform float uTime;      // scene phase (speed and level already applied)
uniform float uClock;     // wall clock
uniform float uHue;       // hue offset incl. drift
uniform vec4  uBands;     // bass, low-mid, mid, high
uniform float uLevel;
uniform float uBeat;
uniform float uBars[64];
uniform int   uScene;
uniform float uReact;     // 0..2
uniform float uChaos;     // 0..1
uniform float uSym;       // 0..12 segments
uniform float uTiles;     // 0..8
uniform float uTwist;     // radians per unit radius
uniform float uSlice;     // 0..1
uniform float uSpinAng;   // accumulated rotation
uniform float uComplex;   // 0..1
uniform float uFill;      // 0..1
uniform float uContrast;  // 0..1
uniform vec3  uPalA;
uniform vec3  uPalB;
uniform vec3  uPalC;
uniform vec3  uPalD;
uniform sampler2D uMedia;
uniform float uHasMedia;   // 0 = no image/video loaded
uniform float uMxScene, uMxOpacity, uMxGlobal, uMxZoom, uMxRot, uMxMirror, uMxTiles;
uniform float uMxSlice, uMxShatter, uMxPixel, uMxDots, uMxSplit, uMxWave, uMxTwist;
uniform float uMxPoster, uMxEdge, uMxTint, uMxBright, uMxContrast, uMxInvert, uMxReact, uMxClip;
uniform int   uMxFit, uMxDrive, uMxBlend;
uniform float uMediaAspect;
uniform int   uShape;      // 0 blob, 1 torus, 2 cube, 3 metaballs, 4 crystal
uniform int   uMat;        // 0 iridescent, 1 chrome, 2 glass, 3 pearl, 4 matte, 5 neon
uniform int   uPattern;    // 0 flower of life, 1 asanoha, 2 metatron

const float PI = 3.14159265;
float gBeat; float gLevel; vec4 gBands;

vec3 pal(float t) { return uPalA + uPalB * cos(6.28318 * (uPalC * (t + uHue) + uPalD)); }

float bar(float x)
{
    x = clamp(x, 0.0, 0.999) * 63.0;
    int i = int(x);
    return min(1.5, mix(uBars[i], uBars[min(i + 1, 63)], fract(x)) * uReact);
}

vec3 tunnel(vec2 p)
{
    float r = length(p) + 1e-3;
    float a = atan(p.y, p.x);
    float z = 0.25 / r * mix(0.6, 2.0, uComplex) + uTime * 0.5;
    float b = bar(abs(a) / PI);
    float sharp = mix(14.0, 2.5, uFill);
    float rings = pow(max(0.0, 1.0 - abs(fract(z) - 0.5) * 2.0), sharp);
    float nsp = floor(mix(2.0, 12.0, uComplex));
    float spokes = pow(abs(cos(a * nsp + z * 0.6)), sharp * 2.0);
    vec3 c = pal(z * 0.08 + b * 0.3)
           * (rings * (0.5 + 1.5 * gBands.x) + spokes * (0.3 + b * 2.0));
    c *= smoothstep(0.0, 0.35, r);
    c += pal(0.5) * exp(-r * 6.0) * gBeat * 1.5;
    return c;
}

vec3 orbits(vec2 p)
{
    vec3 c = vec3(0.0);
    float n = mix(5.0, 20.0, uComplex);
    for (int i = 0; i < 20; ++i)
    {
        float fi = float(i);
        if (fi >= n) break;
        float k = fi / n;
        float b = bar(k * 0.85 + 0.02);
        float ang = uTime * (0.3 + 0.07 * fi) + k * 6.28318;
        float rad = 0.2 + 0.6 * k + 0.15 * b;
        vec2 pos = vec2(cos(ang), sin(ang)) * rad;
        float size = (0.008 + 0.05 * b) * mix(0.5, 2.0, uFill);
        float d = length(p - pos);
        c += pal(k) * min(size * size / (d * d + 1e-4), 4.0);
    }
    float ring = exp(-abs(length(p) - (0.2 + gBeat * 0.5)) * 30.0) * gBeat;
    c += pal(0.6) * ring;
    return c * 0.8;
}

vec3 waves(vec2 p)
{
    vec3 c = vec3(0.0);
    float n = mix(3.0, 9.0, uComplex);
    float th = 0.004 * mix(0.4, 3.0, uFill);
    for (int i = 0; i < 9; ++i)
    {
        float fi = float(i);
        if (fi >= n) break;
        float k = fi / n;
        float e = bar(0.06 + k * 0.85);
        float amp = 0.06 + 0.28 * e;
        float y = (k - 0.5) * 0.7 + sin(p.x * (2.5 + fi * 0.8) + uTime * (0.8 + fi * 0.25) + fi * 1.7) * amp;
        float d = abs(p.y - y);
        c += pal(k + p.x * 0.08) * (th / (d + th)) * (0.5 + e);
    }
    return c;
}

vec3 ink(vec2 p)
{
    float t = uTime * 0.15;
    vec2 q = p * mix(1.0, 3.2, uComplex);
    vec2 w = vec2(fbm(q + vec2(0.0, t)), fbm(q + vec2(5.2, 1.3) - t));
    float r = fbm(q + 3.0 * w + vec2(1.7, 9.2) + t + gBands.x * 0.6);
    float f = fbm(q + 2.0 * vec2(r, w.x));
    float v = abs(f - 0.5);
    float width = mix(0.08, 0.5, uFill);
    float veins = pow(1.0 - smoothstep(0.0, width, v), 2.0);
    float b = bar(f);
    vec3 c = pal(f * 1.3 + r * 0.7) * (0.12 + veins * (0.7 + b * 2.2));
    c += pal(r) * 0.25 * smoothstep(0.35, 0.75, r) * (0.4 + gLevel);
    c += pal(0.8) * gBeat * 0.35 * veins;
    return c;
}

vec3 sparks(vec2 p)
{
    vec3 c = vec3(0.0);

    // drifting dust: three parallax layers of hashed cells
    for (int l = 0; l < 3; ++l)
    {
        float fl = float(l);
        vec2 q = p * (2.6 + 2.2 * fl) + vec2(0.3 * fl, -uTime * (0.12 + 0.07 * fl));
        vec2 cell = floor(q);
        vec2 f = fract(q) - 0.5;
        float h = hash21(cell + fl * 13.7);
        float on = step(0.55 - 0.35 * uComplex, h);
        vec2 pos = (vec2(hash21(cell + 3.1), hash21(cell + 8.7)) - 0.5) * 0.5
                 + 0.12 * vec2(sin(uTime * (0.4 + h) + h * 40.0), cos(uTime * (0.5 + h) + h * 25.0));
        float d = length(f - pos);
        float b = bar(h);
        float s = (0.03 + 0.05 * b) * mix(0.6, 2.0, uFill);
        float tw = 0.6 + 0.4 * sin(uTime * 3.0 + h * 60.0);
        c += pal(h + 0.25 * fl) * min(s * s / (d * d + 2.0e-4), 2.5)
           * smoothstep(0.45, 0.15, d) * on * tw * (0.35 + b + 0.6 * gBeat);
    }

    // emitter: particles born at the centre, thrown outwards by the bass
    float n = mix(20.0, 64.0, uComplex);
    float sz = mix(0.006, 0.02, uFill);
    for (int i = 0; i < 64; ++i)
    {
        float fi = float(i);
        if (fi >= n) break;
        float k = fi / n;
        float h1 = hash11(fi + 1.0), h2 = hash11(fi + 17.3), h3 = hash11(fi + 91.7);
        float life = fract(uTime * (0.07 + 0.1 * h2) + h1);
        float b = bar(k * 0.9 + 0.02);
        float ang = h3 * 6.28318 + (h2 - 0.5) * life * 5.0;
        float r = life * (0.45 + 1.1 * h2 + 1.3 * gBands.x);
        vec2 pos = vec2(cos(ang), sin(ang)) * r;
        pos += (vec2(vnoise(vec2(fi, uTime * 0.3 + life * 3.0)),
                     vnoise(vec2(fi + 40.0, uTime * 0.3 - life * 3.0))) - 0.5) * 0.6 * life;
        float size = sz * (0.7 + 2.4 * b) * (1.0 - life) * (1.0 + 2.0 * gBeat * h2);
        float d = length(p - pos);
        c += pal(k + life * 0.35) * min(size * size / (d * d + 1e-5), 3.0) * (1.0 - life) * (0.6 + b);
    }
    return c * 0.8;
}

// Height field the dot sheet rides on. x, z are world units.
float sheetHeight(float x, float z)
{
    float t = uTime;
    float h = 0.5 * sin(x * 1.1 + z * 0.8 + t * 0.7)
            + 0.35 * sin(x * 0.6 - z * 1.3 + t * 0.5)
            + 0.2 * sin(x * 2.1 + z * 2.4 - t * 1.1);
    float q = (x - 1.2 * sin(z * 0.35 + t * 0.3)) * 0.8;
    float ridge = exp(-q * q);
    float e = bar(clamp(abs(x) * 0.11 + 0.02, 0.0, 1.0));
    return (0.5 + 0.5 * h) * (0.3 + 0.6 * gBands.x + 0.45 * e) * 1.4
         + ridge * (0.45 + 0.8 * gBands.x + 0.5 * gBeat);
}

// Dot-matrix surface seen in perspective: rows of shaded discs riding a moving height field.
vec3 particles(vec2 p)
{
    float dx = mix(0.2, 0.07, uComplex);
    float rf = mix(0.22, 0.48, uFill);
    float nRows = floor(mix(34.0, 62.0, uComplex));
    float T = uTime * 0.5;
    float base = floor(T), fr = fract(T);
    const float hz = 1.05;
    const float H = 1.7;
    vec3 L = normalize(vec3(-0.4, 0.6, 0.7));
    vec3 col = vec3(0.0);
    float sw = 0.1 * sin(uTime * 0.25);
    p = mat2(cos(sw), -sin(sw), sin(sw), cos(sw)) * p;

    for (int jj = 63; jj >= 0; --jj)      // far to near, so near rows cover far ones
    {
        float j = float(jj);
        if (j >= nRows) continue;
        float id = j + base;
        float z = 0.45 + (j - fr) * dx;
        if (z < 0.3) continue;
        float s = 1.0 / z;
        float stag = 0.5 * mod(id, 2.0);
        float ci = floor(p.x / s / dx - stag + 0.5);
        float cx = (ci + stag) * dx;
        float h = sheetHeight(cx, id * dx);
        float yc = hz - (H - h) * s;
        vec2 q = vec2(p.x - cx * s, (p.y - yc) / 0.62);
        float rad = rf * dx * s;
        float dd = length(q) / rad;
        if (dd > 1.0) continue;

        vec2 n2 = q / rad;
        vec3 N = vec3(n2, sqrt(max(0.0, 1.0 - dd * dd)));
        float dif = max(dot(N, L), 0.0);
        float spec = pow(max(dot(reflect(-L, N), vec3(0.0, 0.0, 1.0)), 0.0), 18.0);
        float hn = clamp(h * 0.9, 0.0, 1.0);
        float fog = exp(-z * 0.2);
        vec3 alb = mix(pal(0.05 + 0.1 * hn + 0.02 * mod(id, 7.0)), vec3(1.0), hn * 0.7);
        vec3 sh = (alb * (0.25 + 0.9 * dif) + spec * 0.8) * fog * (0.5 + 1.3 * hn + 0.6 * gBeat * hn);
        col = mix(col, sh, smoothstep(1.0, 0.82, dd));
    }
    return col * 1.3;
}

// ---- sacred geometry on a triangular lattice (unit spacing) ----
const float S3 = 0.8660254;

vec2 toLat(vec2 q)   { float j = q.y / S3; return vec2(q.x - 0.5 * j, j); }
vec2 fromLat(vec2 l) { return vec2(l.x + 0.5 * l.y, l.y * S3); }
float fr1(float x)   { return abs(fract(x + 0.5) - 0.5); }
float latLines(vec2 l) { return min(min(fr1(l.x), fr1(l.y)), fr1(l.x + l.y)) * S3; }
float segDist(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

// Distance to the nearest line or arc of the chosen figure.
// dn = distance to the nearest vertex, vid = an id for that vertex (for hashing).
float sacred(vec2 q, out float dn, out vec2 vid)
{
    vec2 l = toLat(q);
    vec2 b = floor(l);
    vec2 f = l - b;
    vec2 n0 = fromLat(b);
    vec2 n1 = fromLat(b + vec2(1.0, 0.0));
    vec2 n2 = fromLat(b + vec2(0.0, 1.0));
    vec2 n3 = fromLat(b + vec2(1.0, 1.0));
    dn = length(q - n0); vid = b;
    float t = length(q - n1); if (t < dn) { dn = t; vid = b + vec2(1.0, 0.0); }
    t = length(q - n2);       if (t < dn) { dn = t; vid = b + vec2(0.0, 1.0); }
    t = length(q - n3);       if (t < dn) { dn = t; vid = b + vec2(1.0, 1.0); }
    float dp = dn;                                   // nearest lattice node

    float de = 1.0e9;
    if (uPattern == 0)                               // flower of life: unit circles on every node
    {
        for (int i = -1; i <= 2; ++i)
            for (int j = -1; j <= 2; ++j)
                de = min(de, abs(length(q - fromLat(b + vec2(float(i), float(j)))) - 1.0));
        return de;
    }

    bool up = (f.x + f.y) < 1.0;
    vec2 A = up ? n0 : n3;
    vec2 G = (A + n1 + n2) / 3.0;                    // triangle centroid: second vertex type
    float dg = length(q - G);
    if (dg < dn) { dn = dg; vid = b * 2.0 + vec2(7.0, up ? 3.0 : 4.0); }

    if (uPattern == 1)                               // asanoha: lattice + centroid-to-corner spokes
        return min(latLines(l), min(segDist(q, G, A), min(segDist(q, G, n1), segDist(q, G, n2))));

    // metatron: both line families (the second is the lattice turned 30 degrees) + tangent circles
    vec2 qr = vec2(S3 * q.x + 0.5 * q.y, -0.5 * q.x + S3 * q.y) * 1.7320508;
    de = min(latLines(l), latLines(toLat(qr)) / 1.7320508);
    return min(de, abs(dp - 0.5));
}

// Geometric tunnel: the figure is laid on (angle, log radius), so it repeats and shrinks towards the centre.
vec3 vortex(vec2 p)
{
    float r = length(p) + 1e-4;
    float a = atan(p.y, p.x);
    float M = floor(10.0 + 34.0 * uComplex * uComplex + 0.5);
    float s0 = M / 6.28318;
    float d = -log(r) * s0 + uTime * 0.3;
    vec2 q = vec2(a / 6.28318 * M + d * 0.2 + uTime * 0.04, d);

    float dn; vec2 vid;
    float de = sacred(q, dn, vid);

    float px = (2.0 / uRes.y) * s0 / r;              // one screen pixel, in lattice units
    float fade = 1.0 - smoothstep(0.12, 0.3, px);    // hide cells that are only a few pixels wide
    float hN = hash21(vid);
    float e = bar(hN * 0.85 + 0.02);

    float lw = mix(0.006, 0.022, uFill) * (1.0 + 0.8 * gBeat) * (uPattern == 2 ? 0.7 : 1.0);
    float line = 1.0 - smoothstep(lw - px, lw + px, de);
    float nodeR = mix(0.04, 0.095, uFill) * (0.7 + 0.7 * e + 0.4 * gBeat);
    float node = 1.0 - smoothstep(nodeR - px, nodeR + px, dn);
    float halo = exp(-dn * dn / (nodeR * nodeR * 3.0)) * 0.5;

    float pulse = pow(0.5 + 0.5 * sin(d * 2.2 - uTime * 2.0), 3.0);
    float lineB = 0.7 + 0.9 * gBands.y + pulse * (gBands.x + gBeat);
    vec3 c = pal(d * 0.05 + 0.3 + 0.2 * hN) * line * lineB;
    c += mix(pal(hN * 0.3 + d * 0.04), vec3(1.0), 0.6) * (node * (1.0 + 2.2 * e) + halo * e) * 1.3;

    c *= fade * (0.6 + 0.9 * exp(-r * 2.0)) * smoothstep(0.03, 0.12, r);
    c += pal(0.7) * exp(-abs(r - 0.06) * 35.0) * (0.4 + gBands.x);
    return c;
}

// ---- Forms: raymarched 3D shapes with different materials ----
mat3 gRot;

float formWarp(vec3 q, float t)
{
    float f = mix(1.2, 3.2, uComplex);
    float w  = sin(q.x * f + t) * sin(q.y * f * 1.1 - t * 0.8) * sin(q.z * f * 0.9 + t * 0.6);
    float w2 = sin(q.x * f * 2.1 - t * 1.3 + q.y * 1.7) * sin(q.z * f * 2.3 + t);
    return w * 0.7 + w2 * 0.3;
}

float formSD(vec3 q0)
{
    vec3 q = gRot * q0;
    float t = uTime * 0.6;
    float s = mix(0.7, 1.15, uFill);
    float d;
    if (uShape == 0)      d = length(q) - s;
    else if (uShape == 1) { vec2 k = vec2(length(q.xz) - s * 0.95, q.y); d = length(k) - s * 0.38; }
    else if (uShape == 2) { vec3 a = abs(q) - vec3(s * 0.7); d = length(max(a, 0.0)) + min(max(a.x, max(a.y, a.z)), 0.0) - s * 0.18; }
    else if (uShape == 3)
    {
        float k = 0.45;
        d = length(q - 0.55 * s * vec3(sin(t * 0.9), cos(t * 0.7), sin(t * 0.5))) - s * 0.55;
        for (int i = 1; i < 4; ++i)
        {
            float fi = float(i);
            vec3 c = 0.6 * s * vec3(sin(t * (0.7 + 0.3 * fi) + fi * 2.1), cos(t * (0.9 + 0.2 * fi) + fi), sin(t * (0.5 + 0.4 * fi) + fi * 1.3));
            float di = length(q - c) - s * 0.5;
            float h = clamp(0.5 + 0.5 * (di - d) / k, 0.0, 1.0);
            d = mix(di, d, h) - k * h * (1.0 - h);
        }
    }
    else { vec3 a = abs(q); d = (a.x + a.y + a.z - s * 1.05) * 0.57735 - 0.04; }

    float spec = bar(abs(atan(q.z, q.x)) / 3.14159) * 0.14 * uReact;
    float disp = formWarp(q, t) * (0.04 + 0.34 * uChaos) * (0.5 + gBands.x * 1.4 + gBeat * 0.5) + spec;
    return (d - disp) * 0.6;
}

vec3 formEnv(vec3 d)
{
    float h = d.y * 0.5 + 0.5;
    vec3 c = mix(pal(0.1 + uTime * 0.01), pal(0.62), h) * 0.6 + 0.12;
    float l1 = pow(max(dot(d, normalize(vec3(0.6, 0.7, 0.5))), 0.0), 18.0);
    float l2 = pow(max(dot(d, normalize(vec3(-0.7, 0.2, 0.6))), 0.0), 8.0);
    float l3 = pow(max(dot(d, normalize(vec3(0.1, -0.8, 0.4))), 0.0), 6.0);
    return c + l1 * vec3(1.5) + l2 * pal(0.3) * 0.9 + l3 * pal(0.85) * 0.5 + gBeat * 0.08;
}

vec3 forms(vec2 p)
{
    float yaw = uTime * 0.25 + uSpinAng, pitch = 0.45 * sin(uTime * 0.13) + gBands.x * 0.12;
    float cy = cos(yaw), sy = sin(yaw), cx = cos(pitch), sx = sin(pitch);
    gRot = mat3(1.0, 0.0, 0.0, 0.0, cx, -sx, 0.0, sx, cx) * mat3(cy, 0.0, -sy, 0.0, 1.0, 0.0, sy, 0.0, cy);

    vec3 ro = vec3(0.0, 0.0, 4.0);
    vec3 rd = normalize(vec3(p, -2.6));
    vec3 bg = formEnv(rd) * 0.55;

    float b = dot(ro, rd), cc = dot(ro, ro) - 6.8;
    if (b * b - cc < 0.0) return bg;

    float t = 0.5, d = 1.0;
    for (int i = 0; i < 72; ++i)
    {
        d = formSD(ro + rd * t);
        if (d < 0.002 || t > 7.0) break;
        t += d;
    }
    if (d > 0.01 || t > 7.0) return bg;

    vec3 pos = ro + rd * t;
    vec2 e = vec2(0.003, -0.003);
    vec3 n = normalize(e.xyy * formSD(pos + e.xyy) + e.yyx * formSD(pos + e.yyx) + e.yxy * formSD(pos + e.yxy) + e.xxx * formSD(pos + e.xxx));
    vec3 v = -rd;
    float ndv = clamp(dot(n, v), 0.0, 1.0);
    float fres = pow(1.0 - ndv, 3.0);
    vec3 refl = formEnv(reflect(rd, n));
    vec3 L = normalize(vec3(0.6, 0.7, 0.5));
    float diff = max(dot(n, L), 0.0);
    float spc = pow(max(dot(reflect(-L, n), v), 0.0), 60.0);
    float sw = formWarp(gRot * pos * 1.3, uTime * 0.3);
    vec3 col;

    if (uMat == 0)        // iridescent: thin-film colours that shift with the angle and the surface swirl
    {
        float ph = (1.0 - ndv) * 1.7 + dot(n, vec3(1.0, 0.7, 0.4)) * 0.8 + sw * 0.9 + uTime * 0.05 + gBands.y * 0.3;
        vec3 film = 0.5 + 0.5 * cos(6.28318 * (ph + vec3(0.0, 0.33, 0.67)));
        film = mix(film, vec3(dot(film, vec3(0.333))), 0.25);
        col = refl * 0.55 + film * (0.45 + 0.6 * fres) + vec3(0.75 + 0.5 * gBeat) * (spc + 0.18 * fres);
        col = mix(col, vec3(1.0), 0.1 * diff);
    }
    else if (uMat == 1)   // chrome
    {
        col = pow(refl, vec3(1.6)) * 1.5 * mix(vec3(1.0), pal(0.2 + sw * 0.2), 0.25) + spc * 1.4 + fres * 0.3;
    }
    else if (uMat == 2)   // glass with dispersion
    {
        vec3 r = vec3(formEnv(refract(rd, n, 0.70)).r, formEnv(refract(rd, n, 0.67)).g, formEnv(refract(rd, n, 0.64)).b);
        col = r * (0.7 + 0.5 * pal(0.4 + sw * 0.3)) + refl * (0.2 + 1.1 * fres) + spc * 1.3;
    }
    else if (uMat == 3)   // pearl / satin
    {
        vec3 base = mix(vec3(0.9, 0.88, 0.92), pal(0.1 + sw * 0.4), 0.35);
        col = base * (0.35 + 0.7 * diff) + refl * (0.2 + 0.6 * fres) + pal(0.6 + ndv) * fres * 0.7 + spc * 0.6;
    }
    else if (uMat == 4)   // matte clay
    {
        vec3 base = pal(0.15 + sw * 0.25 + pos.y * 0.1);
        col = base * (0.18 + 0.95 * diff) + base * 0.25 * (0.5 + 0.5 * n.y) + fres * pal(0.7) * 0.35;
    }
    else                  // neon: dark body, glowing palette rim
    {
        float rim = pow(1.0 - ndv, 2.0);
        col = vec3(0.02) + pal(rim * 1.2 + sw * 0.5 + uTime * 0.05) * (rim * 2.6 + 0.15 * (0.5 + 0.5 * sw)) * (1.0 + gBeat);
    }
    return col;
}

vec3 scene(vec2 p)
{
    if (uScene == 0) return tunnel(p);
    if (uScene == 1) return orbits(p);
    if (uScene == 2) return waves(p);
    if (uScene == 3) return ink(p);
    if (uScene == 4) return particles(p);
    if (uScene == 5) return vortex(p);
    if (uScene == 6) return sparks(p);
    return forms(p);
}

// ---- image / video layer with its own effects ("decompose") ----
vec2 rot2(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }
float lum3(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec4 mediaLayer(vec2 q, float asp)
{
    float dB = uMxDrive == 1 ? uBands.x : (uMxDrive == 2 ? 0.5 * (uBands.y + uBands.z) : (uMxDrive == 3 ? uBands.w : 0.25 * (uBands.x + uBands.y + uBands.z + uBands.w)));
    float k = uMxReact * (dB * 1.1 + uBeat * 0.7);
    float mask = 1.0;

    q += vec2(sin(q.y * 7.0 + uTime * 1.6), cos(q.x * 6.0 - uTime * 1.3)) * 0.04 * uMxWave * (1.0 + 2.5 * k);
    q = rot2(q, uMxTwist * 2.5 * length(q) * (1.0 + k));
    q = rot2(q, uMxRot);
    q /= max(0.05, uMxZoom) * (1.0 + 0.12 * k);

    if (uMxMirror > 1.5)
    {
        float seg = 6.28318531 / floor(uMxMirror + 0.5);
        float r = length(q);
        float a = mod(atan(q.y, q.x), seg);
        a = abs(a - seg * 0.5);
        q = r * vec2(cos(a), sin(a));
    }
    if (uMxTiles > 0.5)
    {
        vec2 t = q / vec2(asp, 1.0);
        t = fract(t * 0.5 * (uMxTiles + 1.0) + 0.5) * 2.0 - 1.0;
        q = t * vec2(asp, 1.0);
    }
    if (uMxSlice > 0.01)
    {
        float rows = floor(6.0 + uMxSlice * 40.0);
        float r = floor((q.y * 0.5 + 0.5) * rows) + floor(uClock * (2.0 + k * 6.0));
        q.x += (hash11(r) - 0.5) * uMxSlice * (0.08 + 0.9 * k);
    }
    if (uMxShatter > 0.01)
    {
        float cs = mix(0.45, 0.12, uMxShatter);
        vec2 cid = floor(q / cs), cc = (cid + 0.5) * cs, loc = q - cc;
        float h1 = hash21(cid), h2 = hash21(cid + 19.7);
        float ex = uMxShatter * (0.1 + 1.3 * k);
        vec2 a = abs(loc) - cs * 0.5 * (1.0 - min(0.5, 0.35 * ex));
        mask *= 1.0 - smoothstep(-0.004, 0.0, max(a.x, a.y));
        q = cc + rot2(loc, (h1 - 0.5) * ex * 2.5) + (vec2(h1, h2) - 0.5) * ex * cs * 3.0;
    }
    if (uMxPixel > 0.01)
    {
        float c = mix(0.004, 0.16, uMxPixel * uMxPixel) * (1.0 + 0.6 * k * uMxPixel);
        q = (floor(q / c) + 0.5) * c;
    }
    vec2 dloc = vec2(0.0); float dc = 0.0;
    if (uMxDots > 0.01)
    {
        dc = mix(0.1, 0.018, uMxDots);
        vec2 id = floor(q / dc) + 0.5; dloc = q - id * dc; q = id * dc;
    }

    vec2 s = q / vec2(asp, 1.0) * 0.5;
    if (uMxFit == 0) { if (asp > uMediaAspect) s.y *= uMediaAspect / asp; else s.x *= asp / uMediaAspect; }
    else             { if (asp > uMediaAspect) s.x *= asp / uMediaAspect; else s.y *= uMediaAspect / asp; }
    vec2 uv = s + 0.5;
    if (uMxClip > 0.5) mask *= step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
    uv = 1.0 - abs(1.0 - mod(uv, 2.0));
    uv.y = 1.0 - uv.y;

    float an = uTime * 0.3;
    vec2 sd = vec2(cos(an), sin(an)) * uMxSplit * (0.004 + 0.035 * k);
    vec3 m = vec3(texture(uMedia, uv + sd).r, texture(uMedia, uv).g, texture(uMedia, uv - sd).b);

    if (uMxEdge > 0.01)
    {
        float e = 0.004;
        float gx = lum3(texture(uMedia, uv + vec2(e, 0.0)).rgb) - lum3(texture(uMedia, uv - vec2(e, 0.0)).rgb);
        float gy = lum3(texture(uMedia, uv + vec2(0.0, e)).rgb) - lum3(texture(uMedia, uv - vec2(0.0, e)).rgb);
        float ed = clamp(length(vec2(gx, gy)) * 7.0, 0.0, 1.5);
        m = mix(m, pal(lum3(m) * 0.5 + 0.3) * ed * 2.2 + m * 0.12, uMxEdge);
    }
    if (uMxPoster > 0.01)
    {
        float lv = mix(32.0, 3.0, uMxPoster);
        m = floor(m * lv + 0.5) / lv;
    }
    if (uMxTint > 0.01)
    {
        float l = lum3(m);
        m = mix(m, pal(l * 0.9 + 0.1) * (0.35 + 1.5 * l), uMxTint);
    }
    m = (m - 0.5) * mix(0.4, 2.2, uMxContrast) + 0.5;
    m = mix(m, 1.0 - m, uMxInvert);
    m = max(m, vec3(0.0)) * mix(0.3, 2.2, uMxBright) * (1.0 + 0.5 * k);

    if (uMxDots > 0.01)
    {
        float r = dc * 0.5 * sqrt(clamp(lum3(m) * 1.1, 0.0, 1.0)) * (1.0 + 0.35 * k);
        mask *= 1.0 - smoothstep(r - dc * 0.06, r, length(dloc));
        m *= 1.4;
    }
    return vec4(m, mask);
}

void main()
{
    gBeat = uBeat * uReact;
    gLevel = uLevel * uReact;
    gBands = uBands * uReact;

    float asp = uRes.x / uRes.y;
    vec2 p = (vUV * 2.0 - 1.0) * vec2(asp, 1.0);
    vec2 pRaw = p;

    // chaos: organic domain warp
    if (uChaos > 0.01)
    {
        float t = uTime * 0.25;
        vec2 w = vec2(fbm(p * 1.6 + vec2(t, 0.0)), fbm(p * 1.6 + vec2(7.3, -t))) - 0.47;
        p += w * uChaos * (0.35 + 0.5 * gLevel + 0.4 * gBeat);
    }

    // spin
    float cs = cos(uSpinAng), sn = sin(uSpinAng);
    p = mat2(cs, -sn, sn, cs) * p;

    // twist
    float tw = uTwist * length(p);
    cs = cos(tw); sn = sin(tw);
    p = mat2(cs, -sn, sn, cs) * p;

    // tiles
    if (uTiles > 0.5)
    {
        float n = uTiles + 1.0;
        vec2 q = p / vec2(asp, 1.0);
        q = fract(q * 0.5 * n + 0.5) * 2.0 - 1.0;
        p = q * vec2(asp, 1.0);
    }

    // symmetry (kaleidoscope)
    if (uSym > 1.5)
    {
        float seg = 6.28318531 / floor(uSym + 0.5);
        float r = length(p);
        float a = mod(atan(p.y, p.x), seg);
        a = abs(a - seg * 0.5);
        p = r * vec2(cos(a), sin(a));
    }

    // slice: horizontal bands that slide with the beat
    if (uSlice > 0.01)
    {
        float rows = floor(6.0 + uSlice * 30.0);
        float s = floor(vUV.y * rows) + floor(uClock * (3.0 + gBeat * 10.0));
        p.x += (hash11(s) - 0.5) * uSlice * (0.1 + 0.7 * gBeat + 0.4 * gBands.w);
    }

    vec3 col = scene(p);

    // image / video layer (own controls; uMxGlobal lets the scene warps bend it too)
    if (uHasMedia > 0.5)
    {
        vec4 mm = mediaLayer(mix(pRaw, p, uMxGlobal), asp);
        float a = mm.a * uMxOpacity;
        vec3 sc = col * uMxScene;
        if (uMxBlend == 1)      col = sc + mm.rgb * a;
        else if (uMxBlend == 2) col = sc * mix(vec3(1.0), mm.rgb * 2.0, a);
        else                    col = mix(sc, mm.rgb, a);
    }
    col = pow(max(col, vec3(0.0)), vec3(mix(0.6, 1.4, uContrast)));
    col += pal(0.1) * 0.03 * (1.0 + gBeat * 2.0);
    col = 1.0 - exp(-col * 1.3);
    fragColor = vec4(col, 1.0);
}
