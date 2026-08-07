# AmpForge

**A free, open-source guitar amp simulator plugin for Linux.**

AmpForge is a single plugin — not a chain of separate plugins bolted
together in a DAW — that hosts a full, reorderable pedalboard-and-amp
chain internally, in the spirit of Guitar Rig / BIAS FX. It ships as
**VST3**, **LV2**, **CLAP**, and a **JACK/PipeWire standalone** app,
all built from one codebase.

Linux has never really had a good, free, actively-developed answer to
Guitar Rig or BIAS FX. AmpForge is an attempt at exactly that.

## Features

- **12 internal modules**, freely reorderable at runtime: Noise Gate,
  Compressor, Wah, Screamer (overdrive), Distortion (a harder,
  asymmetric-clipping second gain stage), Amp (always present),
  Cabinet (convolves with a loaded speaker-cab impulse response),
  Chorus, Phaser, Tremolo, Delay, Reverb.
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
- **6 factory presets** (Fender Clean, Marshall Rock, Shredder Lead,
  Metal Rhythm, Ambient Shoegaze, Funk Clean) plus **custom presets**
  you can save, update, and delete from within the plugin.
- **Every parameter is host-automatable**, with correct automation
  gesture handling (touch/undo work properly in your DAW) and proper
  boolean/integer/unit metadata for automation lanes.
- Works as a **VST3**, **LV2**, or **CLAP** plugin in any compatible
  host (Reaper, Carla, etc.), or as a **standalone JACK/PipeWire app**.

## Building from source

There are no prebuilt binaries yet — you build it yourself. It's a
standard CMake + Ninja project and only takes a few minutes.

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

## Using it

- **In Reaper**: add it from the FX browser like any other VST3 or CLAP
  plugin — it shows up as "AmpForge". Automate any knob, switch, or the
  pedal Position parameters directly from Reaper's automation lanes; the
  6 factory presets are also available from Reaper's own plugin preset
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

## Contributing

Issues and pull requests are welcome. The codebase is organized as:

- `core/` — header-only DSP blocks, one per effect, all implementing a
  small shared `AudioBlock` interface.
- `plugins/04-chain/` — the actual plugin: DSP (`ChainPlugin.cpp`), UI
  (`ChainUI.cpp`), and the shared parameter/preset definitions used by
  both.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).
