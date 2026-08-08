# AmpForge

**A free, open-source guitar amp simulator plugin, developed on Linux
and also cross-compiled for Windows.**

AmpForge is a single plugin — not a chain of separate plugins bolted
together in a DAW — that hosts a full, reorderable pedalboard-and-amp
chain internally, in the spirit of Guitar Rig / BIAS FX. On Linux it
ships as **VST3**, **LV2**, **CLAP**, and a **JACK/PipeWire standalone**
app, all built from one codebase; **VST3/LV2/CLAP for Windows** are
built from that same codebase by cross-compiling with mingw-w64 (see
[Building for Windows](#building-for-windows-cross-compile) below).

Linux has never really had a good, free, actively-developed answer to
Guitar Rig or BIAS FX. AmpForge is an attempt at exactly that.

## Features

- **12 internal modules**, freely reorderable at runtime: Noise Gate,
  Compressor, Wah, Screamer (overdrive), Distortion (a harder,
  asymmetric-clipping second gain stage), Amp (always present),
  Cabinet (convolves with a loaded speaker-cab impulse response),
  Chorus, Phaser, Tremolo, Delay, Reverb.
- **4 Amp Type voicings** (Modern, Vintage, Crunch, Hi-Gain) — distinct
  EQ centers, headroom, and clip character per type; "Modern" is the
  default and matches the Amp block's original fixed behavior exactly.
- **Tempo-syncable Delay, Tremolo, and Chorus** — lock their time/rate
  to the host's BPM (whole down to sixteenth notes, plus a couple of
  dotted/triplet divisions) instead of only a free-running ms/Hz knob.
- **Built-in tuner** — a standalone autocorrelation-based pitch
  detector with a note-name-and-cents overlay, reading the signal
  straight off the input so it's unaffected by whatever the pedalboard
  is doing further down the chain.
- **CPU load meter** in the top bar, especially handy once the Cabinet
  block's convolution is in the chain.
- **A/B compare** — snapshot the current chain into slot A or B and
  flip between them instantly while dialing in a tone.
- **Drag-and-drop pedalboard UI** — add pedals from a palette, drag
  them into the rack, reorder by dragging cards (other cards live-reflow
  around wherever you're about to drop it), real footswitch-style
  bypass separate from add/remove (bypassed pedals stay fully legible,
  just desaturated to a dead-metal look with their LED dark, instead of
  fading into a washed-out ghost of the card).
- **Noise Gate with an adjustable Range** — instead of slamming quiet
  passages to total silence, it attenuates by a Range you dial in
  (0–80dB), with hysteresis and a hold time so it doesn't choke a
  note's natural decay or chatter on signal hovering near the
  threshold.
- **7 factory presets** (Chimey Clean, British Crunch, Shredder Lead,
  Metal Rhythm, Ambient Shoegaze, Funk Clean, Blues Rock Sustain) plus
  **custom presets** you can save, update, delete, and export/import as
  files (to share a tone with someone else, or between machines) from
  within the plugin.
- **Every parameter is host-automatable**, with correct automation
  gesture handling (touch/undo work properly in your DAW) and proper
  boolean/integer/unit metadata for automation lanes.
- Works as a **VST3**, **LV2**, or **CLAP** plugin in any compatible
  host on Linux or Windows (Reaper, Carla, etc.), or as a **standalone
  JACK/PipeWire app** on Linux.

## Building from source

There are no prebuilt binaries yet — you build it yourself. It's a
standard CMake + Ninja project and only takes a few minutes. This
section covers the native Linux build; see [Building for Windows
(cross-compile)](#building-for-windows-cross-compile) further down if
you want the Windows VST3/CLAP/LV2 instead.

### 1. Install prerequisites

**Debian / Ubuntu:**

```bash
sudo apt install build-essential cmake ninja-build git \
    libx11-dev libxext-dev libxcursor-dev libxrandr-dev libgl1-mesa-dev
```

**Arch / CachyOS / Manjaro:**

```bash
sudo pacman -S --needed base-devel cmake ninja git \
    libx11 libxext libxcursor libxrandr mesa
```

For the standalone app you'll also want a running JACK server or
PipeWire with its JACK-compatibility layer (`pipewire-jack`) — most
modern distros already have this. VST3/LV2 use inside a DAW or Carla
don't need it.

### 2. Clone and build

```bash
git clone https://github.com/Loursy/AmpForge.git
cd AmpForge
mkdir build && cd build
cmake .. -G Ninja
ninja
```

CMake fetches the [DPF](https://github.com/DISTRHO/DPF) framework
automatically on first configure — no submodules, no extra flags, a
plain `git clone` is all you need.

This builds four plugin folders (`01-gain`, `02-amp`, `03-screamer`
are early scaffolding kept for reference; **`04-chain` is AmpForge
itself**, the actual product). All the output ends up under
`build/bin/`.

### 3. Install

**LV2** (for Carla, Ardour, Qtractor, and most Linux LV2 hosts):

```bash
mkdir -p ~/.lv2
cp -r build/bin/ampforge_main.lv2 ~/.lv2/
```

**VST3** (for Reaper and other VST3 hosts):

```bash
mkdir -p ~/.vst3
cp -r build/bin/ampforge_main.vst3 ~/.vst3/
```

Rescan plugins in your host afterward (in Reaper: *Options → Preferences
→ Plug-ins → VST → Re-scan*).

**CLAP** (for Reaper, Bitwig, and other CLAP hosts):

```bash
mkdir -p ~/.clap
cp -r build/bin/ampforge_main.clap ~/.clap/
```

**Standalone** (JACK/PipeWire, no host needed):

```bash
./build/bin/ampforge_main
```

## Building for Windows (cross-compile)

AmpForge doesn't need a Windows machine to build a Windows plugin -
mingw-w64 cross-compiles VST3, CLAP, and LV2 straight from the same
Linux checkout, into real PE32+ Windows binaries. **Steps 1 and 2 below
run on your Linux machine, same as the native build above** - the only
part that touches a Windows machine at all is copying the finished
files over in step 3.

### 1. Install the mingw-w64 toolchain (on your Linux machine)

**Arch / CachyOS / Manjaro:**

```bash
sudo pacman -S --needed mingw-w64-gcc mingw-w64-binutils \
    mingw-w64-headers mingw-w64-crt mingw-w64-winpthreads
```

**Debian / Ubuntu:**

```bash
sudo apt install mingw-w64
```

### 2. Configure and build (on your Linux machine)

Use a separate build directory and point `CMAKE_TOOLCHAIN_FILE` at
`cmake/toolchain-mingw64.cmake` (included in this repo):

```bash
mkdir build-windows && cd build-windows
cmake .. -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release
ninja ampforge_main-vst3 ampforge_main.clap ampforge_main-lv2
```

The output lands in `build-windows/bin/`, in the same
`ampforge_main.vst3` / `ampforge_main.clap` / `ampforge_main.lv2`
layout as the Linux build, just with `.dll`-based Windows binaries
inside instead of `.so`.

The standalone (`ampforge_main-jack`) target isn't part of this list -
it needs a Windows-native SDL2/audio-backend build that isn't set up
in this cross-compile path yet (see [Project status](#project-status)).
VST3/CLAP/LV2 inside a DAW don't need it.

### 3. Install on Windows

Copy the folders/files built in step 2 (from `build-windows/bin/` on
your Linux machine) over to the Windows machine, to wherever your host
looks for plugins, e.g.:

- **VST3**: `%COMMONPROGRAMFILES%\VST3\` (typically
  `C:\Program Files\Common Files\VST3\`)
- **CLAP**: `%COMMONPROGRAMFILES%\CLAP\` (typically
  `C:\Program Files\Common Files\CLAP\`)
- **LV2**: `%APPDATA%\LV2\` for a per-user install, or check your LV2
  host's own documentation for its scan paths

Rescan plugins in your host afterward, the same as on Linux.

## Using it

- **In Reaper**: add it from the FX browser like any other VST3 or CLAP
  plugin — it shows up as "AmpForge". Automate any knob, switch, or the
  pedal Position parameters directly from Reaper's automation lanes; the
  7 factory presets are also available from Reaper's own plugin preset
  dropdown.
- **In Carla / other LV2 hosts**: add it as an LV2 plugin named
  "AmpForge".
- **Standalone**: run `ampforge_main` with JACK or PipeWire running,
  and patch its input/output ports (e.g. with `qpwgraph` or
  `catia`/`carla-patchbay`) to your interface.

Custom presets you save from inside the plugin are stored at
`~/.config/ampforge/user_presets.txt` and are shared across every
instance and format (VST3/LV2/CLAP/standalone) on your machine. Note
that, by design, custom presets are a global, machine-local list rather
than DAW state — they don't travel *inside* a saved project file, though
every parameter value you've dialed in does (that's standard host
automation state, saved and restored with your project like any other
plugin).

### Exporting and importing presets

The preset dropdown's **Export** button writes the currently-loaded
preset to its own file under `~/.config/ampforge/exports/`, named after
the preset (the toast that pops up after exporting shows the exact
path) — hand that file to someone else, or copy it to another machine,
and their **Import** button's native file picker can load it straight
back in as a custom preset. A whole copied-over `user_presets.txt` can
be imported the same way. Imported presets are added to (or, if the
name matches one already in your list, update) your custom presets;
importing a file whose preset happens to be named after one of the 7
factory presets is skipped rather than silently shadowing it, since the
dropdown has no way to show two same-named entries as distinct.

### Loading a cabinet impulse response

The Cabinet block convolves your tone with a real speaker cabinet's
impulse response (a short WAV recording of how that cab + mic responds
to an impulse) — this is what gives a driven amp its "coming out of a
real speaker" character, rather than sounding thin or synthetic.

AmpForge doesn't bundle any IRs (most are copyrighted captures of real
gear, so shipping them isn't something a free/open plugin can do) —
bring your own. Plenty of free, permissively-licensed cabinet IRs exist
online; look for mono WAV files.

Add the Cabinet pedal to your board, click its **Load IR File...**
button, and pick a WAV file (8/16/24/32-bit PCM or 32-bit float, mono or
stereo — stereo files are downmixed). Unlike custom presets, the loaded
IR's file path *is* saved with your DAW session/project (it's DPF
plugin state, not a parameter, but it's still real host-persisted
state), so it survives a project reload as long as the file stays at
the same path on disk.

## Project status

The DSP engine (all 12 modules, the reorderable chain, factory
presets) and the pedalboard UI are both functional and usable today.
Known gaps, tracked for future work:

- No neural amp modeler (NAM) capture loading yet.
- The Cabinet block's convolution is a straightforward direct
  time-domain implementation, not an FFT-partitioned one — plenty fast
  enough for a single instance at the IR lengths a speaker cab actually
  needs (capped at 4096 samples), but a future optimization opportunity
  if that ever changes.
- The Windows cross-compile only covers VST3/CLAP/LV2 - the standalone
  app pulls in SDL2/RtAudio for its audio backend, and cross-compiling
  those against mingw-w64 isn't set up yet, so there's no Windows
  standalone `.exe` for now (VST3/CLAP/LV2 inside a DAW cover the
  overwhelming majority of Windows use anyway).
- No CI/release pipeline yet - Windows binaries are built locally by
  hand via the cross-compile steps above, not published as downloadable
  releases.

## Contributing

Issues and pull requests are welcome. The codebase is organized as:

- `core/` — header-only DSP blocks, one per effect, all implementing a
  small shared `AudioBlock` interface.
- `plugins/04-chain/` — the actual plugin: DSP (`ChainPlugin.cpp`), UI
  (`ChainUI.cpp`), and the shared parameter/preset definitions used by
  both.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).
