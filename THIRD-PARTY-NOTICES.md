# Third-party notices

AmpForge (GPL-3.0-or-later, see [LICENSE](LICENSE)) is built on top of
[DPF](https://github.com/DISTRHO/DPF) (the DISTRHO Plugin Framework),
fetched automatically at build time via CMake `FetchContent` rather
than vendored in this repository - so its source, and the source of
everything it bundles, isn't checked into this repo, but its compiled
code (including the components below, which DPF bundles for its own
UI toolkit and plugin-format interfaces) *is* statically compiled
into every AmpForge VST3/CLAP/LV2/standalone binary this project
builds and distributes. Each permissive license below requires the
copyright notice to travel with the software - this file is that
notice for the compiled binaries (source builds already get it via
DPF's own fetched `LICENSE` files).

None of this affects AmpForge's own license: all of the following are
GPL-compatible licenses (ISC, zlib, MIT, and - for Eigen below -
MPL-2.0, a weak/file-level copyleft that the FSF and the Mozilla
Foundation both document as GPLv2-or-later compatible), so they only
require attribution, not that AmpForge's own code be released under
them.

## DPF (DISTRHO Plugin Framework)

Copyright (C) 2012-2025 Filipe Coelho \<falktx@falktx.com\>

Includes `travesty`, DPF's own from-scratch VST3-compatible interface
headers (avoids depending on Steinberg's own VST3 SDK), Copyright (C)
2021-2022 Filipe Coelho, under the same terms.

> Permission to use, copy, modify, and/or distribute this software for any
> purpose with or without fee is hereby granted, provided that the above
> copyright notice and this permission notice appear in all copies.
>
> THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
> REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
> FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
> INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
> LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
> OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
> PERFORMANCE OF THIS SOFTWARE.

## pugl

DPF's UI windowing/event layer (window creation, mouse/keyboard input,
OpenGL context) - see `ChainUI.cpp`'s `onMouse`/`onKeyboard`/etc.
overrides, which are pugl events by way of DPF's `DGL` wrapper.

Copyright 2011-2022 David Robillard \<d@drobilla.net\>

> Permission to use, copy, modify, and/or distribute this software for any
> purpose with or without fee is hereby granted, provided that the above
> copyright notice and this permission notice appear in all copies.
>
> THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
> WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
> MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
> ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
> WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
> ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
> OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

## NanoVG

The 2D vector graphics renderer AmpForge's whole UI is drawn with
(`DISTRHO_UI_USE_NANOVG` in `DistrhoPluginInfo.h`) - every knob,
pedal card, and dropdown in `ChainUI.cpp` is NanoVG draw calls.

Copyright (c) 2013 Mikko Mononen memon@inside.org

> This software is provided 'as-is', without any express or implied
> warranty. In no event will the authors be held liable for any damages
> arising from the use of this software.
>
> Permission is granted to anyone to use this software for any purpose,
> including commercial applications, and to alter it and redistribute it
> freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not
>    claim that you wrote the original software. If you use this software
>    in a product, an acknowledgment in the product documentation would be
>    appreciated but is not required.
> 2. Altered source versions must be plainly marked as such, and must not be
>    misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

## DejaVu Sans (bundled UI fallback font)

NanoVG's built-in fallback font, embedded directly into every UI
binary as raw bytes (DPF's `dpf_dejavusans_ttf` resource).

Fonts are (c) Bitstream (see below). DejaVu changes are in the public domain.
Glyphs imported from Arev fonts are (c) Tavmjong Bah (see below).

> Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved. Bitstream Vera is
> a trademark of Bitstream, Inc.
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of the fonts accompanying this license ("Fonts") and associated
> documentation files (the "Font Software"), to reproduce and distribute the
> Font Software, including without limitation the rights to use, copy, merge,
> publish, distribute, and/or sell copies of the Font Software, and to permit
> persons to whom the Font Software is furnished to do so, subject to the
> conditions in the full license text (below).
>
> Copyright (c) 2006 by Tavmjong Bah. All Rights Reserved. (Arev Fonts,
> under the same terms, applying to the glyphs DejaVu imported from Arev.)

Full text: `LICENSE-DejaVuSans.ttf.txt` in DPF's own `dgl/src/resources/`
(fetched at build time, not vendored here) - condensed above since the
full text is long and mostly boilerplate; nothing in it is more
restrictive than "keep the notice, don't reuse the Bitstream Vera/Arev
names for a modified font."

## NeuralAmpModelerCore

The neural-network inference library the NAM block (`core/NamBlock.hpp`)
loads `.nam` capture files (both the original architecture and the
newer A2 architecture) into and runs - fetched at build time the same
way as DPF (CMake `FetchContent`, pinned commit, not vendored), and
statically compiled into `nam_core` (see the root `CMakeLists.txt`),
which every AmpForge binary links.

Copyright (c) 2023 Steven Atkinson

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Eigen

The linear-algebra library NeuralAmpModelerCore's neural network
layers are built on (fetched as NeuralAmpModelerCore's own git
submodule, `Dependencies/eigen`, pulled in by the same `FetchContent`
call above); header-only, so it's compiled directly into `nam_core`
rather than linked as a separate library.

Licensed under the Mozilla Public License 2.0 (MPL-2.0) - a
file-level/weak copyleft: modifications to Eigen's *own* source files
would need to stay MPL-2.0 and have their source made available, but
it explicitly does not extend that requirement to code (like
AmpForge's) that merely uses Eigen as a library, and the FSF lists
MPL-2.0 as GPLv2-or-later compatible. AmpForge doesn't modify Eigen's
source at all - it's used unmodified via `FetchContent`.

Full text: `Dependencies/eigen/LICENSE` (fetched at build time, not
vendored here) or <https://www.mozilla.org/MPL/2.0/>.

## nlohmann/json

The JSON parser NeuralAmpModelerCore uses to read `.nam` files'
metadata/weights (vendored by NeuralAmpModelerCore itself as a single
header, `Dependencies/nlohmann/json.hpp`, not a git submodule - it
comes along automatically with the `FetchContent` call above).

SPDX-FileCopyrightText: 2013-2025 Niels Lohmann <https://nlohmann.me>
- MIT License

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## CLAP headers

The plugin-format interface `ChainPlugin.cpp`/`ChainUI.cpp` are built
against for the CLAP target (via DPF's `distrho/src/clap/` wrapper).

MIT License - Copyright (c) 2021 Alexandre BIQUE

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## LV2 headers

The plugin-format interface `ChainPlugin.cpp`/`ChainUI.cpp` are built
against for the LV2 target.

Copyright 2006-2012 Steve Harris, David Robillard. Based on LADSPA,
Copyright 2000-2002 Richard W.E. Furse, Paul Barton-Davis, Stefan
Westerfeld.

> Permission to use, copy, modify, and/or distribute this software for any
> purpose with or without fee is hereby granted, provided that the above
> copyright notice and this permission notice appear in all copies.
>
> THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
> WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
> MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
> ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
> WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
> ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
> OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
