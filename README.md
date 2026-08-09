# AmpForge

**A free, open-source guitar amp simulator plugin for Linux.**

AmpForge is a single plugin — not a chain of separate plugins bolted
together in a DAW — that hosts a full, reorderable pedalboard-and-amp
chain internally, in the spirit of Guitar Rig / BIAS FX. It ships as
**VST3**, **LV2**, **CLAP**, and a **JACK/PipeWire standalone** app,
all built from one codebase. A **Windows installer** (VST3/CLAP/LV2,
cross-compiled from the same codebase) is also available for Windows
users - see [Download](#download) below.

Linux has never really had a good, free, actively-developed answer to
Guitar Rig or BIAS FX. AmpForge is an attempt at exactly that.

## Download

Grab the latest tarball from the [Releases
page](https://github.com/Loursy/AmpForge/releases/latest), extract it,
and run `./install.sh` - installs VST3/CLAP/LV2 and the standalone
binary into your home directory (`~/.vst3`, `~/.clap`, `~/.lv2`,
`~/.local/bin`), no root/sudo needed. `./install.sh --uninstall`
removes them again. Prefer to build it yourself, or install each
format individually? See [Building from source](#building-from-source)
below.

(Windows users: an installer - `AmpForge-Setup.exe` - is on the
[Releases page](https://github.com/Loursy/AmpForge/releases) too,
though it isn't rebuilt for every release yet - see [Project
status](#project-status) - so grab it from the newest release that
actually has one attached if the latest doesn't.)

## Features

- **13 internal modules**, freely reorderable at runtime: Noise Gate,
  Compressor, Wah, Screamer (overdrive), Distortion (a harder,
  asymmetric-clipping second gain stage), Amp (always present),
  Cabinet (convolves with a loaded speaker-cab impulse response), NAM
  (loads a [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore)
  `.nam` capture of a real amp/pedal/cab), Chorus, Phaser, Tremolo,
  Delay, Reverb.
- **Neural Amp Modeler (NAM) support** — load a `.nam` capture file
  (both the original and newer A2 architectures) into its own block,
  independent from the modeled Amp/Cabinet chain — mix, input trim,
  and output trim knobs let you blend it in, and the status sidebar
  shows the loaded capture's filename and architecture (WaveNet, LSTM,
  ...). NAM inference is CPU-heavier than the rest of the chain
  combined, especially with larger WaveNet captures — expect a real
  jump in CPU usage once a model is loaded.
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

Prefer building it yourself over the [prebuilt
tarball](#download) - e.g. to install just one plugin format, or to
track the latest commit? It's a standard CMake + Ninja project and
only takes a few minutes.

**Planning to load it into a flatpak/snap-sandboxed host** (Bitwig's
flatpak build is the most common case)? The native steps below link
against *your own system's* glibc - on a rolling-release distro (Arch,
CachyOS, Manjaro) in particular, that's often newer than what an older
sandboxed host ships internally, so the plugin then fails to load
there with an error like `version 'GLIBC_2.43' not found`, even though
it built and runs fine outside the sandbox. Skip straight to the
[Docker build](#building-a-portable-release-build-docker) below
instead - it sidesteps this by pinning to a fixed, older glibc
baseline. Using a natively-installed host instead (not flatpak/snap),
or a non-rolling distro? The native steps below are simpler and just
as correct - no need for Docker at all.

### 1. Install prerequisites

**Debian / Ubuntu:**

```bash
sudo apt install build-essential cmake ninja-build git pkg-config \
    libx11-dev libxext-dev libxcursor-dev libxrandr-dev libgl1-mesa-dev
```

Needs **CMake 3.17+** (for `FetchContent`'s `GIT_SUBMODULES_RECURSE`) -
Ubuntu 22.04/Debian 12 and newer already have this in `apt`. Older releases
(Ubuntu 20.04's `cmake` is 3.16.3, too old) need a newer CMake from
elsewhere (e.g. [Kitware's apt repo](https://apt.kitware.com/)) - or just
use the [Docker build](#building-a-portable-release-build-docker) below,
which sidesteps this and every other toolchain-version question at once.

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

#### Building a portable release build (Docker)

A native build like the one above links against whatever glibc your
own distro ships. On a rolling-release distro that can be *too new*:
a plugin built against glibc 2.43, for example, fails to load in an
older sandboxed host - such as Bitwig's flatpak build on a
just-released Fedora - with an error like:

```
Failed to load CLAP plug-in ... version `GLIBC_2.43' not found
```

To avoid that, build inside `installer/linux/Dockerfile`'s Ubuntu
22.04 (glibc 2.35) environment instead, which covers essentially every
mainstream Linux distro and flatpak/snap runtime from the last several
years. That specific baseline isn't arbitrary - pushing it back further to
Debian 11 (glibc 2.31, still a meaningfully common target) was tried and
hits a real wall: DPF's own (unused - AmpForge's UI is OpenGL/DGL, not a
WebView) `WebViewImpl.cpp` always compiles in, and its `shm_open`/
`shm_unlink`/`pthread_create`/`pthread_join` calls link cleanly on glibc
2.34+ (those merged into `libc` itself around then) but fail with
`undefined reference` on anything older without patching DPF's own build
to add `-lrt` - out of scope here. Ubuntu 22.04 is the oldest base that
builds clean without touching DPF at all.

```bash
installer/linux/docker-build.sh
```

This mounts the repo into a container and runs the same CMake/Ninja
build as above, landing the result in `build-docker/bin/` instead of
`build/bin/` (so it never collides with a native build you already
have). `installer/linux/package.sh` and `install.sh` both work against
it the same way, just point them at it with `BUILD_DIR`:

```bash
BUILD_DIR=build-docker installer/linux/package.sh
```

This is also what CI (`.github/workflows/linux-build.yml`) runs on
every push, so a release built this way is reproducible on your own
machine too. Requires only [Docker](https://docs.docker.com/engine/install/)
itself - no other prerequisites from step 1.

**Which systems the Docker build actually loads on:** a Docker-built
`.clap`/`.vst3`/`.lv2` needs at most `GLIBC_2.34` and `GLIBCXX_3.4.29`.
Check your own system against that with:

```bash
ldd --version   # glibc version, first line
strings /usr/lib/x86_64-linux-gnu/libstdc++.so.6 | grep GLIBCXX | sort -V | tail -1
```

(that second path varies by distro - e.g. `/usr/lib64/libstdc++.so.6` on
Fedora). Verified for real below, not just compared version numbers -
actually resolved the built `.clap`'s dynamic symbols with `ld.so --list`
inside each distro's own container image:

| System | Status |
| --- | --- |
| Ubuntu 22.04 LTS and newer (24.04, ...) | ✅ Works |
| Debian 12 (bookworm) and newer | ✅ Works |
| Fedora (recent releases, native or flatpak-sandboxed hosts like Bitwig on a current runtime) | ✅ Works - this is the exact bug report that started this section |
| Arch / CachyOS / Manjaro (rolling) | ✅ Works (glibc is always current there) |
| Ubuntu 20.04 (focal) | ❌ Too old - `GLIBC_2.32/2.33/2.34 not found` |
| Debian 11 (bullseye) and older | ❌ Too old - same `GLIBC`/`GLIBCXX` errors, and can't even be used as the *build* environment either (see the DPF `-lrt` wall above) |

A native build (the non-Docker path above) inherits whatever's newer or
older about *your own* system's glibc instead of this table - it can be
strictly more restrictive (a rolling-release host, the original problem
this section exists for) or, in principle, less (building directly on an
even-older-than-Ubuntu-22.04 system that nonetheless has a new enough
compiler for everything else) than what the Docker build produces.

### 3. Install

Built it yourself and just want it installed, without copying each
format by hand below? `installer/linux/install.sh` does exactly the
same copies in one step - run it straight from the repo root
(`installer/linux/install.sh`) once `build/bin/` exists, or
`installer/linux/install.sh --uninstall` to remove it again.

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

### Loading a NAM capture

The NAM block runs a [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore)
`.nam` file — a neural network trained to reproduce one specific real
amp, pedal, or cabinet, rather than a hand-tuned emulation like the
Amp block. It's a separate, optional tone source you can mix in
alongside (or instead of) the modeled Amp/Cabinet chain, not something
you have to choose between.

AmpForge doesn't bundle any captures (most are shared under their own
terms by whoever trained them) — bring your own from
[ToneHunt](https://tonehunt.org) or [Tone3000](https://www.tone3000.com),
or train your own with the [official NAM trainer](https://github.com/sdatkinson/neural-amp-modeler).

Add the NAM pedal to your board, click its **Load NAM Model...**
button, and pick a `.nam` file. Like the Cabinet IR path above, the
loaded model's file path is saved with your DAW session/project and
survives a reload as long as the file stays at the same path on disk.
The status sidebar confirms what's loaded — filename and architecture
(WaveNet, LSTM, the A2 "SlimmableContainer" wrapper, ...).

## Project status

The DSP engine (all 13 modules, the reorderable chain, factory
presets) and the pedalboard UI are both functional and usable today.
Known gaps, tracked for future work:

- The NAM block runs inference one audio sample at a time rather than
  batched per-block (see `core/NamBlock.hpp`'s comment on why), which
  gives up some of NAM's own internal batching efficiency - fine for a
  single real-time instance, but a future optimization target if CPU
  cost ever becomes a problem.
- The Cabinet block's convolution is a straightforward direct
  time-domain implementation, not an FFT-partitioned one — plenty fast
  enough for a single instance at the IR lengths a speaker cab actually
  needs (capped at 4096 samples), but a future optimization opportunity
  if that ever changes.
- The Windows installer only covers VST3/CLAP/LV2 - the standalone app
  pulls in SDL2/RtAudio for its audio backend, and cross-compiling
  those for Windows isn't set up yet, so there's no Windows standalone
  `.exe` for now (VST3/CLAP/LV2 inside a DAW cover the overwhelming
  majority of Windows use anyway).
- CI ([GitHub Actions](.github/workflows/linux-build.yml)) builds and
  packages the Linux tarball on every push/PR/tag using the same
  portable Docker build described above, and on a `v*` tag push
  publishes it as a GitHub Release automatically - pushing a tag is
  the whole Linux release process now, no local Docker run or manual
  upload needed. The Windows installer isn't built by CI at all yet
  though (no Windows cross-build image exists), so it's still built
  and published by hand, on whatever cadence actually needs a new one.

## Contributing

Issues and pull requests are welcome. The codebase is organized as:

- `core/` — header-only DSP blocks, one per effect, all implementing a
  small shared `AudioBlock` interface.
- `plugins/04-chain/` — the actual plugin: DSP (`ChainPlugin.cpp`), UI
  (`ChainUI.cpp`), and the shared parameter/preset definitions used by
  both.

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).

Built on [DPF](https://github.com/DISTRHO/DPF) and what it bundles
(pugl, NanoVG, a fallback font, and the CLAP/LV2 interface headers) -
all permissively licensed, see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the full text of
each.
