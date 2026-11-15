#!/usr/bin/env python3
"""Genera Source/Shaders.h (plugin) y web/prism-web.fragment.html + web/index.html
a partir de shaders/*.glsl y web/src.html. Ejecutar desde cualquier carpeta."""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
names = ["vert", "common", "scene", "feedback", "final"]
files = {n: (root / "shaders" / (n + (".glsl" if n in ("vert", "common") else ".frag"))).read_text() for n in names}

# --- C++ header
h = ["#pragma once", "// Generado por scripts/build_assets.py. No editar a mano.", "namespace Shaders {"]
for n in names:
    h.append('inline const char* const %s = R"GLSL(%s)GLSL";' % (n, files[n]))
h.append("}")
(root / "Source" / "Shaders.h").write_text("\n".join(h) + "\n")

# --- web
js = "var SH = {\n" + ",\n".join("  %s: `%s`" % (n, files[n]) for n in names) + "\n};"
src = (root / "web" / "src.html").read_text()
assert "/*@@SHADERS@@*/" in src
frag = src.replace("/*@@SHADERS@@*/", js)
(root / "web" / "prism-web.fragment.html").write_text(frag)
(root / "web" / "index.html").write_text(
    '<!doctype html>\n<html lang="es"><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>\n'
    + frag + "\n</body></html>\n")
print("ok")
