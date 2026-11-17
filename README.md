# Prism Reactor

Plugin de visuales audio-reactivos para Ableton Live (VST3 en Windows/macOS, AU en macOS). Inspirado en el concepto de las apps de visuales reactivos (audio entra → escenas en vivo), pero es una implementación original: escenas, shaders y nombre propios.

El audio pasa intacto; el plugin solo lo escucha. Ponlo como efecto en cualquier pista (o en Master) y abre su ventana.

## Qué hace

- **Análisis en tiempo real**: FFT de 2048 puntos, 64 barras logarítmicas (30 Hz–16 kHz), 4 bandas, nivel y detector de beat.
- **8 escenas en GLSL**: Tunnel, Orbits, Waves, Ink (flujo orgánico con domain warping), Particles (malla de puntos sombreados con relieve en perspectiva), Vortex (túnel de geometría sagrada: los nodos son los vértices de la figura y los hilos sus líneas; parámetro `pattern`: flower of life, asanoha o metatron) Sparks (emisor de partículas lanzado por los graves) y Forms (formas 3D por raymarching: `shape` blob, torus, cube, metaballs, crystal; `material` iridescent, chrome, glass, pearl, matte, neon; el espectro deforma la superficie y los graves la hacen respirar). `complexity` regula la densidad y `fill` el tamaño de puntos, nodos e hilos. La paleta `rose` da el look rosa/blanco sobre negro.
- **Render en 3 pasadas**: escena → trails (feedback con ping-pong) → acabado final (lens, glow, color, viñeta).
- **Panel tune** (todo automatizable desde Live):
  - *Reacción y organicidad*: react, chaos, trails, heat, decay, brightness, glow, input gain, input EQ (6 nodos, ±12 dB sobre el análisis).
  - *Geometría*: symmetry (caleidoscopio 0–12), tiles, twist, slice, lens (glass ribs, fisheye, ripple, prism), spin.
  - *Color*: palette (nocturne, aurora, ember, ocean, sunset, acid, mono), hue, hue drift, vignette.
  - *Escena*: speed, complexity, fill, contrast, aplicados a la escena activa.
  - **Randomize look**: sortea una combinación nueva de look sin tocar la escena ni la ganancia.

Los shaders viven en `shaders/` y son la única fuente: `scripts/build_assets.py` genera `Source/Shaders.h` (plugin) y `web/index.html` desde `web/src.html`. Tras editar un shader, ejecuta `python3 scripts/build_assets.py` y vuelve a compilar.

## Compilar

Requisitos: CMake ≥ 3.22, compilador C++17. JUCE 8.0.6 se descarga solo.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target PrismReactor_VST3
```

Linux necesita: `libasound2-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev`.

Salida: `build/PrismReactor_artefacts/Release/VST3/Prism Reactor.vst3`

- **Windows**: copiar a `C:\Program Files\Common Files\VST3`, luego *Options → Plug-ins → Rescan* en Live.
- **macOS**: `~/Library/Audio/Plug-Ins/VST3` (o el AU en `.../Components`). Para distribuir hay que firmar/notarizar.
- **Probar sin Live**: target `PrismReactor_Standalone`.

### Solo en la versión web

- **Audio de pestaña / Spotify**: «Audio de una pestaña» usa la captura de pestaña del navegador (Chrome o Edge de escritorio). Abre `open.spotify.com` en otra pestaña, elige esa pestaña y activa «Compartir audio de la pestaña». Spotify no entrega su audio a otras apps, por eso se captura la pestaña. Para la app de escritorio de Spotify en Mac, enrútala con BlackHole y elígelo en el selector de entrada.
- **Transporte y exportación**: barra inferior con play/pausa, tiempo, forma de onda (clic o arrastre para saltar; espacio y ←/→ también) y «Exportar video». Con un archivo de audio, exportar reinicia la pista y graba hasta el final; con fuentes en vivo graba hasta que pulses detener. El botón ⚙ junto a «Exportar video» permite elegir resolución (ventana, 720p, 1080p, 1440p, 4K), fps (24/30/60), calidad (4–60 Mbps) y formato (auto, MP4, WebM). Sale con el visual y el audio.
- **Automatización (solo web)**: botón *Automation* (tecla A). Con **● rec** activo, reproduce la pista y mueve cualquier control (incluidas escena y EQ): se crea una línea por parámetro y se repite sincronizada con el audio. Las líneas se editan a mano: clic para añadir un punto, arrastrar para moverlo, doble clic o clic derecho para borrarlo; **M** silencia una línea. Se guardan solas en el navegador por archivo de audio, y se pueden guardar/cargar como `.json`. La exportación de video reproduce la automatización desde el inicio de la pista.
- **Image · video**: sección propia del panel con controles independientes de la escena (opacity, blend, zoom, rotate, spin, twist, mirror, tiles, slice, shatter, pixelate, halftone, wave, rgb split, edges glow, posterize, palette tint, brillo, contraste, invert, reactividad por banda). `scene fx` deja que symmetry/twist/chaos globales también deformen la imagen.
- **Media (antiguo)**: sección `media` del panel. Carga una imagen o un video (o suéltalos en la página) y pasa por los mismos efectos que las escenas: symmetry, tiles, twist, slice, chaos, trails, lens, color. `media mix` regula cuánto pesa.
- Si el panel está oculto (botón Ocultar o tecla H), aparece «Mostrar controles · H» abajo a la derecha para volver a abrirlo.
- Si un archivo no carga como audio normal, la página lo decodifica y lo reproduce en bucle; si falla, muestra el error en la tarjeta de fuente.
- La captura de pestaña y el micrófono solo funcionan al abrir `web/index.html` directamente en el navegador (o desde localhost/https), no dentro de una vista incrustada.

## Instalar en Mac (Ableton Live)

Requisitos: macOS 10.15+, Xcode Command Line Tools (`xcode-select --install`) y CMake (`brew install cmake`).

```bash
./scripts/build-mac.sh
```

El script compila una versión universal (Apple Silicon + Intel), quita la cuarentena, firma ad-hoc e instala el VST3 en `~/Library/Audio/Plug-Ins/VST3` y el AU en `~/Library/Audio/Plug-Ins/Components`. Después, en Live: *Settings → Plug-Ins*, activa los system folders y pulsa *Rescan*. Si el plugin no aparece, haz un rescan completo (Option + Rescan) y revisa *Settings → Plug-Ins* por plugins desactivados.

## Versión web

`web/index.html` es una página autónoma con el mismo motor de shaders. Ábrela en Chrome/Safari/Firefox; sirve para ver el resultado sin Live (demo integrada, archivo de audio, micrófono o audio de una pestaña). Para el micrófono ábrela como archivo local o desde un servidor `localhost`/https.

## Estructura

```
Source/Analyzer.h        FFT, barras, bandas, beat (audio thread → atómicos)
Source/PluginProcessor   passthrough + parámetros (APVTS)
Source/Params.h          tabla de parámetros, paletas, EQ
Source/PluginEditor      renderer OpenGL de 3 pasadas + panel tune
shaders/                 GLSL compartido por plugin y web
```

## Añadir una escena

1. Escribe una función `vec3 miEscena(vec2 p)` en `shaders/scene.frag`. Dispones de `bar(x)`, `gBands`, `gBeat`, `gLevel`, `uTime`, `pal(t)`, `uComplex`, `uFill`.
2. Añádela a `scene()`.
3. Añade el nombre en `kSceneNames` (`Source/Params.h`), en los botones de `web/src.html` y regenera con `build_assets.py`.

## Notas

- Las ventanas OpenGL en plugins pueden dar problemas en algunos hosts con GPUs integradas; si ves pantalla negra, actualiza drivers o prueba el Standalone.
- Parpadeos intensos: el efecto Glitch con beats rápidos puede ser molesto para personas fotosensibles. Considera advertirlo si lo distribuyes.
- Para alimentar visuales con otra pista, usa *sidechain* o un Send hacia una pista con el plugin.
- Pendiente (ideas): editor de automatización propio, export de video, más escenas, entrada de color por MIDI.

## Automation in the Standalone / plugin editor
Press **AUTOMATION** (top right) to open the timeline strip: PLAY, ● REC (move any control while playing to record a lane), click on a lane to add points, drag to move, double‑click / right‑click to delete, M = mute lane, SAVE/LOAD JSON. Lanes are stored with the plugin state. The clock is an internal loop (not host‑synced); in Live, map parameters to macros for timeline automation.

## Preview resolution & video export in the Standalone / plugin (macOS)
- **Preview NN%** (top right, all versions): renders the on‑screen preview at a lower resolution to use less GPU (100/75/50/35/25 %). It does not change exports. Saved with the state (web: in the browser).
- **EXPORT VIDEO** (Standalone/plugin, macOS): renders the visuals *offline* frame by frame from an audio file you pick (WAV/AIFF/MP3/M4A/FLAC) to an H.264 + AAC MP4 (720p–4K, 24/30/60 fps, 8–60 Mbps) using AVFoundation. Automation lanes are baked in (from t = 0). The live audio input is not used for export.

## Standalone / plugin: more tools
- **Audio source strip** (under the visuals): *Live input* or *Audio file* played by the plugin itself (OPEN AUDIO, PLAY/PAUSE, seek). The file is heard and analysed, so the Standalone works without BlackHole or an unmuted mic. **REC AUDIO** records whatever the plugin is listening to into a WAV; when you stop, the export dialog opens with that recording preselected.
- **Automation clock**: *Internal clock* (own loop, BPM and loop lengths in bars) or *Host transport* (follows the DAW play/stop/position and tempo; plugin only). Scene changes are just a step lane on the *Scene* parameter.
- **Image layer** (PNG/JPG/GIF) with its own controls, same as the web version (video stays web-only for now).
- **Presets**: factory presets (also exposed to the host as programs) and SAVE/LOAD `.prism` files.
- **FULLSCREEN** window with only the visuals (Esc/F to close), **Preview auto** (adapts resolution to the frame rate) and keyboard shortcuts: H hide panel, A automation, F fullscreen, Space play/pause the file, 1-8 scenes, R randomize.
- **Export**: H.264 / H.265 (MP4) or ProRes 422 (MOV), export queue, progress with time remaining.

## Scene transitions
Changing scene (by hand, by shortcut or from an automation lane) now blends between the old and the new scene: **scene change** = *cut (jump)* / *crossfade* / *dissolve* / *iris*, with **change time** (0.1–8 s). Pick *cut* for the old instant jump. Works in the web version, the plugin/Standalone and the video export.
