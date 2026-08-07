/*
 * AmpForge - Main Chain UI (Phase 10: visual overhaul)
 *
 * Visual/feel pass, on top of the Phase 9 functionality:
 *   - Animation smoothing is now delta-time based (uiIdle() measures real
 *     elapsed time via std::chrono instead of assuming a fixed tick rate),
 *     so easing speed no longer depends on how often the host calls
 *     uiIdle() - this is what was making transitions feel inconsistent/
 *     janky before.
 *   - Every knob now has an always-visible "LCD" value readout under it
 *     (e.g. "-6.2dB"), instead of a floating tooltip that only appeared
 *     while dragging. The readout brightens while the knob is actively
 *     being adjusted, but is always present.
 *   - Pedal cards, knobs and switches got a gloss/gradient/drop-shadow
 *     pass (rack-hardware look, closer to Guitar Rig) using NanoVG's
 *     linear/radial/box gradients: glossy accent-colored header plates,
 *     metallic knob faces with detent ticks, glowing LEDs on active
 *     switches, drop shadows under cards for depth.
 *   - Removing a pedal now fades/slides it out instead of popping it
 *     away instantly (mirrors the existing add-in animation).
 */

#include "DistrhoUI.hpp"
#include "ChainParameters.hpp"
#include "ChainPresets.hpp"
#include "AmpBlock.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <map>
#include <cctype>
#include <cstring>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::kKeyBackspace;
using DGL_NAMESPACE::kKeyEnter;
using DGL_NAMESPACE::kKeyEscape;

// --- Base style ---
static const Color kColorBackground(16, 16, 20);
static const Color kColorPanel(26, 26, 32);
static const Color kColorHeaderBar(24, 24, 29);
static const Color kColorKnobTrack(48, 48, 56);
static const Color kColorOn(90, 220, 140);
static const Color kColorOff(80, 80, 90);
static const Color kColorRemove(230, 90, 100);
static const Color kColorTextPrimary(235, 235, 240);
static const Color kColorTextMuted(150, 150, 160);
static const Color kColorTextDark(20, 20, 24);
static const Color kColorTooltipBg(10, 10, 12);
static const Color kColorControlBar(22, 22, 27);
static const Color kColorButton(40, 40, 48);
static const Color kColorButtonAccent(90, 170, 255);
static const Color kColorDropdownBg(30, 30, 37);
static const Color kColorModalOverlay(0, 0, 0);
static const Color kColorModalBox(32, 32, 40);
static const Color kColorScrollbar(70, 70, 82);
// A cool, neutral steel tone for the Input panel - distinct from every
// pedal's own accent color, since Input is a utility trim stage rather
// than a tone-shaping effect.
static const Color kColorInputAccent(150, 165, 185);

// --- Layout constants ---
static constexpr float kTopBarHeight     = 52.0f;
static constexpr float kControlBarHeight = 44.0f;
static constexpr float kPaletteWidth     = 190.0f;
static constexpr float kPaletteItemH     = 42.0f;
static constexpr float kRackTop          = kTopBarHeight + kControlBarHeight + 20.0f;
static constexpr float kModuleLeft       = kPaletteWidth + 20.0f;
static constexpr float kModuleHeaderH    = 42.0f;
static constexpr float kKnobAreaH        = 108.0f;
static constexpr float kModuleGap        = 16.0f;
static constexpr float kKnobRadius       = 22.0f;
static constexpr float kKnobSpacing      = 84.0f;
static constexpr float kKnobCenterYOffset = 50.0f; // header bottom -> knob center
static constexpr float kValueChipW       = 70.0f;
static constexpr float kValueChipH       = 18.0f;
static constexpr float kValueChipGap     = 8.0f;
static constexpr float kFileLoaderW      = 132.0f;
static constexpr float kFileLoaderH      = 28.0f;
static constexpr float kSwitchSize       = 18.0f;
static constexpr float kRemoveSize       = 16.0f;
static constexpr float kKnobDragSensitivity = 220.0f;
static constexpr float kValueAnimSpeed   = 16.0f; // 1/s, exponential ease for knob values
static constexpr float kModuleAnimSpeed  = 11.0f; // 1/s, exponential ease for card add/remove
static constexpr float kDropdownAnimSpeed = 20.0f; // 1/s, exponential ease for the preset dropdown
static constexpr float kAnimSnapEpsilon  = 0.001f;
static constexpr float kDropdownRowH     = 28.0f;
static constexpr float kDragThreshold    = 6.0f;

// The Input panel sits above the reorderable pedal rack, fixed and never
// scrolled/dragged (see kInputPedalIndex below) - same card height as a
// normal pedal so it reads as part of the same rack visually, then the
// rack of actual pedals starts below it.
static constexpr float kInputPanelH      = kModuleHeaderH + kKnobAreaH;
static constexpr float kPedalRackTop     = kRackTop + kInputPanelH + kModuleGap;
static constexpr float kMeterW           = 22.0f;

struct KnobDef
{
    const char* label;
    int paramIndex;
    float minVal;
    float maxVal;
    const char* unit;
    int decimals;
    bool asPercent;
    // When true, this knob's value is an index into kSyncDivisions
    // (ChainParameters.hpp) rather than a plain number - it's drawn and
    // edited the same way as any other knob, but the LCD readout shows
    // the division's label ("1/8", "1/4.", ...) instead of a formatted
    // number. Defaults to false for every existing entry above.
    bool isSyncDivision = false;
    // Same idea as isSyncDivision, but for ampforge::kAmpVoicings
    // (core/AmpBlock.hpp) instead - the Amp card's "Type" knob.
    bool isAmpType = false;
};

struct PedalDef
{
    const char* name;
    int onParam;
    int bypassParam;
    int positionParam;
    // The pedal's canonical slot in the recommended signal chain, used to
    // place it when it's freshly added (see addPedalAtDefaultPosition()) -
    // kept as an explicit field, separate from this array's own index,
    // since new pedal types get appended at the end of this table (to
    // keep kAmpPedalIndex and existing entries' indices stable) but may
    // still belong earlier in the actual recommended chain order.
    int defaultPosition;
    Color accent;
    std::vector<KnobDef> knobs;
    // When true, the card shows a "Load..." file-picker button (wired to
    // a DPF State key, since a file path can't be a plain Parameter)
    // instead of assuming every knob is enough - see stateKey below and
    // ChainPlugin.cpp's initState()/getState()/setState().
    bool hasFileLoader = false;
    const char* stateKey = nullptr;
};

// clang-format off
static const PedalDef kPedalDefs[] =
{
    { "Noise Gate", kParamGateOn,     kParamGateBypass,     kParamGatePosition,  0, Color(130, 150, 210), {
        { "Thresh",  kParamGateThreshold, -80.0f, 0.0f,    "dB", 1, false },
        { "Attack",  kParamGateAttack,      0.5f, 50.0f,   "ms", 1, false },
        { "Release", kParamGateRelease,    10.0f, 1000.0f, "ms", 0, false },
        { "Range",   kParamGateRange,       0.0f, 80.0f,   "dB", 0, false },
    }},
    { "Compressor", kParamCompOn,     kParamCompBypass,     kParamCompPosition,  1, Color(175, 120, 225), {
        { "Thresh",  kParamCompThreshold, -60.0f, 0.0f,    "dB", 1, false },
        { "Ratio",   kParamCompRatio,       1.0f, 20.0f,   ":1", 1, false },
        { "Attack",  kParamCompAttack,      0.5f, 100.0f,  "ms", 1, false },
        { "Release", kParamCompRelease,    10.0f, 1000.0f, "ms", 0, false },
        { "Makeup",  kParamCompMakeup,      0.0f, 24.0f,   "dB", 1, false },
    }},
    { "Wah",        kParamWahOn,      kParamWahBypass,      kParamWahPosition,   2, Color(235, 155, 60), {
        { "Pedal",   kParamWahPedal,  0.0f, 1.0f,  "", 0, true },
        { "Q",       kParamWahQ,      0.5f, 10.0f, "", 1, false },
    }},
    { "Screamer",   kParamScreamerOn, kParamScreamerBypass, kParamScreamerPosition, 3, Color(235, 95, 70), {
        { "Drive",   kParamScreamerDrive,  1.0f, 20.0f,  "x", 1, false },
        { "Tone",    kParamScreamerTone,   0.05f, 1.0f,  "",  0, true },
        { "Level",   kParamScreamerLevel, -24.0f, 12.0f, "dB", 1, false },
    }},
    { "Amp",        -1,               -1,                   kParamAmpPosition,   5, Color(90, 170, 255), {
        { "Type",    kParamAmpType,     0.0f, float(ampforge::kAmpVoicingCount - 1), "", 0, false, false, true },
        { "Drive",   kParamAmpDrive,    0.0f, 36.0f,  "dB", 1, false },
        { "Bass",    kParamAmpBass,   -12.0f, 12.0f,  "dB", 1, false },
        { "Mid",     kParamAmpMid,    -12.0f, 12.0f,  "dB", 1, false },
        { "Treble",  kParamAmpTreble, -12.0f, 12.0f,  "dB", 1, false },
        { "Volume",  kParamAmpVolume, -24.0f, 12.0f,  "dB", 1, false },
    }},
    { "Chorus",     kParamChorusOn,   kParamChorusBypass,   kParamChorusPosition, 7, Color(70, 205, 195), {
        { "Rate",    kParamChorusRate,   0.05f, 5.0f,  "Hz", 2, false },
        { "Depth",   kParamChorusDepth,  0.5f, 20.0f,  "ms", 1, false },
        { "Mix",     kParamChorusMix,    0.0f, 1.0f,   "",   0, true },
        { "Sync",    kParamChorusSync,   0.0f, float(kSyncDivisionCount - 1), "", 0, false, true },
    }},
    { "Phaser",     kParamPhaserOn,   kParamPhaserBypass,   kParamPhaserPosition, 8, Color(185, 115, 235), {
        { "Rate",    kParamPhaserRate,  0.05f, 5.0f, "Hz", 2, false },
        { "Depth",   kParamPhaserDepth, 0.0f, 1.0f,  "",   0, true },
        { "Mix",     kParamPhaserMix,   0.0f, 1.0f,  "",   0, true },
    }},
    { "Tremolo",    kParamTremoloOn,  kParamTremoloBypass,  kParamTremoloPosition, 9, Color(235, 205, 60), {
        { "Rate",    kParamTremoloRate,  0.5f, 15.0f, "Hz", 1, false },
        { "Depth",   kParamTremoloDepth, 0.0f, 1.0f,  "",   0, true },
        { "Sync",    kParamTremoloSync,  0.0f, float(kSyncDivisionCount - 1), "", 0, false, true },
    }},
    { "Delay",      kParamDelayOn,    kParamDelayBypass,    kParamDelayPosition,  10, Color(95, 225, 145), {
        { "Time",     kParamDelayTime,      10.0f, 1500.0f, "ms", 0, false },
        { "Feedback", kParamDelayFeedback,   0.0f, 0.95f,   "",   0, true },
        { "Mix",      kParamDelayMix,        0.0f, 1.0f,    "",   0, true },
        { "Sync",     kParamDelaySync,       0.0f, float(kSyncDivisionCount - 1), "", 0, false, true },
    }},
    { "Reverb",     kParamReverbOn,   kParamReverbBypass,   kParamReverbPosition, 11, Color(115, 125, 235), {
        { "Room",     kParamReverbRoomSize, 0.0f, 1.0f, "", 0, true },
        { "Damping",  kParamReverbDamping,  0.0f, 1.0f, "", 0, true },
        { "Mix",      kParamReverbMix,      0.0f, 1.0f, "", 0, true },
    }},
    // Both appended at the end of the table (rather than where they
    // belong tonally) so every pedal above keeps the same array index -
    // notably kAmpPedalIndex below. Their defaultPosition fields are what
    // actually place them correctly when added.
    { "Distortion", kParamDistortionOn, kParamDistortionBypass, kParamDistortionPosition, 4, Color(220, 75, 120), {
        { "Drive",   kParamDistortionDrive,  1.0f, 30.0f,  "x", 1, false },
        { "Tone",    kParamDistortionTone,   0.05f, 1.0f,  "",  0, true },
        { "Level",   kParamDistortionLevel, -24.0f, 12.0f, "dB", 1, false },
    }},
    { "Cabinet",    kParamCabinetOn, kParamCabinetBypass, kParamCabinetPosition, 6, Color(200, 160, 90), {
        { "Mix",     kParamCabinetMix,    0.0f, 1.0f,   "",   0, true },
        { "Level",   kParamCabinetLevel, -24.0f, 12.0f, "dB", 1, false },
    }, true, kCabinetIRStateKey },
};
// clang-format on
static constexpr int kPedalDefCount = sizeof(kPedalDefs) / sizeof(kPedalDefs[0]);
static constexpr int kAmpPedalIndex = 4;

// The Input panel's Gain knob isn't part of kPedalDefs (it's a fixed
// pre-chain trim, not a reorderable/toggleable pedal - see
// ChainParameters.hpp's comment on kParamInputGain), but it still needs
// to plug into the same drag/type-to-edit knob code every pedal's knobs
// use. kInputPedalIndex is a sentinel "pedal index" one past the last
// real one - resolveKnob() below is what the shared code calls through
// instead of indexing into kPedalDefs directly, so it can hand back this
// one knob without kPedalDefs needing a matching entry.
static constexpr int kInputPedalIndex = kPedalDefCount;
static const KnobDef kInputGainKnob = { "Gain", kParamInputGain, -24.0f, 24.0f, "dB", 1, false };

// The palette lists pedals in recommended signal-chain order (matching
// where they actually land in the rack via defaultPosition), not in
// kPedalDefs's own array order - new pedal types get appended at the end
// of that array to keep existing indices stable (see PedalDef's comment
// above), which would otherwise leave them looking randomly tacked on at
// the bottom of the palette instead of near where they belong tonally.
static const std::vector<int> kPaletteOrder = []()
{
    std::vector<int> order;
    for (int i = 0; i < kPedalDefCount; ++i)
        if (i != kAmpPedalIndex)
            order.push_back(i);
    std::sort(order.begin(), order.end(), [](int a, int b)
    {
        return kPedalDefs[a].defaultPosition < kPedalDefs[b].defaultPosition;
    });
    return order;
}();

// A blank starting point for "+ New Preset" - only Amp present, flat
// and neutral, everything else off the board.
// clang-format off
static const float kBlankPresetValues[kParamCount] =
{
    /* Gate      on,pos,thr,atk,rel */          0.0f, 0.0f, -50.0f, 5.0f, 150.0f,
    /* Comp      on,pos,thr,ratio,atk,rel,mkup */ 0.0f, 1.0f, -18.0f, 4.0f, 10.0f, 100.0f, 0.0f,
    /* Wah       on,pos,pedal,q */               0.0f, 2.0f, 0.5f, 3.0f,
    /* Screamer  on,pos,drive,tone,level */      0.0f, 3.0f, 1.0f, 0.5f, 0.0f,
    /* Amp       pos,drive,bass,mid,treble,vol */ 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    /* Chorus    on,pos,rate,depth,mix */        0.0f, 7.0f, 1.0f, 5.0f, 0.5f,
    /* Phaser    on,pos,rate,depth,mix */        0.0f, 8.0f, 0.5f, 0.7f, 0.5f,
    /* Tremolo   on,pos,rate,depth */            0.0f, 9.0f, 5.0f, 0.5f,
    /* Delay     on,pos,time,fb,mix */           0.0f, 10.0f, 300.0f, 0.3f, 0.3f,
    /* Reverb    on,pos,room,damp,mix */         0.0f, 11.0f, 0.5f, 0.5f, 0.3f,
    /* Bypass x9 */                              0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    /* Distortion on,pos,drive,tone,level,bypass */ 0.0f, 4.0f, 4.0f, 0.5f, 0.0f, 0.0f,
    /* Cabinet    on,pos,mix,level,bypass */        0.0f, 6.0f, 1.0f, 0.0f, 0.0f,
    /* Gate Range */                                40.0f,
    /* Input Gain */                                0.0f,
};
// clang-format on

struct CustomPreset
{
    std::string name;
    float values[kParamCount];
};

class ChainUI : public UI
{
public:
    ChainUI()
        : UI(1000, 720)
    {
        loadSharedResources();
        std::fill(std::begin(paramValues), std::end(paramValues), 0.0f);
        std::fill(std::begin(displayValues), std::end(displayValues), 0.0f);
        std::fill(std::begin(moduleAlpha), std::end(moduleAlpha), 1.0f);
        std::fill(std::begin(moduleRemoving), std::end(moduleRemoving), false);
        lastFrameTime = std::chrono::steady_clock::now();
        loadCustomPresets();
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
            paramValues[index] = value;
        repaint();
    }

    void uiIdle() override
    {
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;
        dt = std::max(0.0f, std::min(dt, 0.05f)); // clamp so a stall doesn't cause a jump

        bool anyChanged = false;

        // Frame-rate independent easing: however often the host calls
        // uiIdle(), the animation converges at the same real-world speed.
        const float valueStep = 1.0f - std::exp(-dt * kValueAnimSpeed);
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            const float diff = paramValues[i] - displayValues[i];
            if (std::fabs(diff) > kAnimSnapEpsilon)
            {
                displayValues[i] += diff * valueStep;
                anyChanged = true;
            }
            else if (displayValues[i] != paramValues[i])
            {
                displayValues[i] = paramValues[i];
                anyChanged = true;
            }
        }

        const float moduleStep = 1.0f - std::exp(-dt * kModuleAnimSpeed);
        for (int i = 0; i < kPedalDefCount; ++i)
        {
            const float target = moduleRemoving[i] ? 0.0f : 1.0f;
            const float diff = target - moduleAlpha[i];
            if (std::fabs(diff) > kAnimSnapEpsilon)
            {
                moduleAlpha[i] += diff * moduleStep;
                anyChanged = true;
            }
            else if (moduleAlpha[i] != target)
            {
                moduleAlpha[i] = target;
                anyChanged = true;
            }

            if (moduleRemoving[i] && moduleAlpha[i] <= 0.02f)
            {
                // Fade-out finished - actually take the pedal off the board now.
                moduleRemoving[i] = false;
                moduleAlpha[i] = 1.0f;
                const int onParam = kPedalDefs[i].onParam;
                if (onParam >= 0)
                {
                    editParameter(onParam, true);
                    setParameterValue(onParam, 0.0f);
                    paramValues[onParam] = 0.0f;
                    editParameter(onParam, false);
                }
                anyChanged = true;
            }
        }

        if (saveToastAlpha > 0.0f)
        {
            saveToastAlpha = std::max(0.0f, saveToastAlpha - dt / 1.4f);
            anyChanged = true;
        }

        const float dropdownTarget = presetDropdownOpen ? 1.0f : 0.0f;
        const float dropdownDiff = dropdownTarget - dropdownAnim;
        if (std::fabs(dropdownDiff) > kAnimSnapEpsilon)
        {
            dropdownAnim += dropdownDiff * (1.0f - std::exp(-dt * kDropdownAnimSpeed));
            anyChanged = true;
        }
        else if (dropdownAnim != dropdownTarget)
        {
            dropdownAnim = dropdownTarget;
            anyChanged = true;
        }

        if (anyChanged)
            repaint();
    }

    void onNanoDisplay() override
    {
        const float width  = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());

        beginPath();
        rect(0.0f, 0.0f, width, height);
        fillPaint(linearGradient(0.0f, 0.0f, 0.0f, height, Color(23, 23, 28), Color(13, 13, 16)));
        fill();
        closePath();

        drawTopBar(width);
        drawControlBar();
        drawPalette(height);
        drawRack(width, height);
        drawPaletteDragGhost();
        drawPresetDropdown();
        drawSaveModal(width, height);
        drawTunerOverlay(width, height);
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (savingPreset)
        {
            // Control characters (Backspace, Delete, Enter, Escape, Tab...)
            // can arrive here too on some platforms alongside their own
            // onKeyboard event - appending them as literal bytes fought with
            // onKeyboard's Backspace handling (it looked like Backspace did
            // nothing, since the control byte it had just added got popped
            // right back off). Only printable text belongs in the name.
            if (ev.character >= 0x20 && ev.character != 0x7f && nameInputBuffer.size() < 40)
                nameInputBuffer += ev.string;
            repaint();
            return true;
        }

        if (editingKnobPedal >= 0)
        {
            const char c = ev.string[0];
            const bool isDigit = (c >= '0' && c <= '9');
            const bool isDot = (c == '.' && knobEditBuffer.find('.') == std::string::npos);
            const bool isMinus = (c == '-' && knobEditBuffer.empty());
            if ((isDigit || isDot || isMinus) && knobEditBuffer.size() < 12)
                knobEditBuffer += c;
            repaint();
            return true;
        }

        return false;
    }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (paramValues[kParamTunerOn] > 0.5f)
        {
            if (ev.press && ev.key == kKeyEscape)
                closeTuner();
            return true;
        }

        if (savingPreset)
        {
            if (!ev.press)
                return true;

            if (ev.key == kKeyBackspace)
            {
                if (!nameInputBuffer.empty())
                    nameInputBuffer.pop_back();
                repaint();
            }
            else if (ev.key == kKeyEnter)
            {
                confirmSavePreset();
            }
            else if (ev.key == kKeyEscape)
            {
                savingPreset = false;
                nameInputBuffer.clear();
                repaint();
            }
            return true;
        }

        if (editingKnobPedal >= 0)
        {
            if (!ev.press)
                return true;

            if (ev.key == kKeyBackspace)
            {
                if (!knobEditBuffer.empty())
                    knobEditBuffer.pop_back();
                repaint();
            }
            else if (ev.key == kKeyEnter)
            {
                confirmKnobEdit();
            }
            else if (ev.key == kKeyEscape)
            {
                cancelKnobEdit();
            }
            return true;
        }

        return false;
    }

    bool onMouse(const MouseEvent& ev) override
    {
        const float mx = static_cast<float>(ev.pos.getX());
        const float my = static_cast<float>(ev.pos.getY());

        // A fresh click anywhere closes an in-progress manual value edit
        // (discarding it) - if the click actually landed on a value chip,
        // the hit-test further below immediately opens a new edit there.
        if (editingKnobPedal >= 0 && ev.press && ev.button == 1)
            cancelKnobEdit();

        // --- Tuner overlay: while open, any click closes it ---
        if (paramValues[kParamTunerOn] > 0.5f)
        {
            if (ev.press && ev.button == 1)
                closeTuner();
            return true;
        }

        // --- Naming modal: while open, it owns all mouse input ---
        if (savingPreset)
        {
            if (ev.press && ev.button == 1)
                handleModalClick(mx, my);
            return true;
        }

        if (!ev.press || ev.button != 1)
        {
            if (!ev.press)
            {
                if (draggingModuleIndex >= 0)
                {
                    editParameter(kPedalDefs[draggingModuleIndex].positionParam, false);
                    draggingModuleIndex = -1;
                    repaint();
                }
                if (draggingKnobPedal >= 0)
                {
                    editParameter(resolveKnob(draggingKnobPedal, draggingKnobIndex).paramIndex, false);
                    draggingKnobPedal = -1;
                    draggingKnobIndex = -1;
                    repaint();
                }
                if (paletteDraggingPedal >= 0)
                {
                    finishPaletteDrag(mx, my);
                    repaint();
                }
            }
            return false;
        }

        // --- Dropdown: if open, clicks either land on it or close it ---
        if (presetDropdownOpen)
        {
            const bool handled = handleDropdownClick(mx, my);
            if (!handled)
                presetDropdownOpen = false;
            repaint();
            return true;
        }

        // --- Control bar (preset button / + New / Save) ---
        if (my >= kTopBarHeight && my < kTopBarHeight + kControlBarHeight)
        {
            return handleControlBarClick(mx, my);
        }

        // --- Palette: press-and-hold starts a potential drag ---
        if (mx < kPaletteWidth && my > kTopBarHeight + kControlBarHeight)
        {
            for (size_t row = 0; row < kPaletteOrder.size(); ++row)
            {
                const float itemY = kRackTop + static_cast<float>(row) * kPaletteItemH;
                if (my >= itemY && my < itemY + kPaletteItemH)
                {
                    paletteDraggingPedal = kPaletteOrder[row];
                    paletteDragMoved = false;
                    paletteDragStartX = mx;
                    paletteDragStartY = my;
                    paletteDragCurrentX = mx;
                    paletteDragCurrentY = my;
                    return true;
                }
            }
            return false;
        }

        // --- Input panel: fixed, not scrolled - just the Gain knob ---
        {
            const float moduleW = getWidth() - kModuleLeft - 20.0f;
            if (my >= kRackTop && my < kRackTop + kInputPanelH && mx >= kModuleLeft && mx <= kModuleLeft + moduleW)
            {
                const float knobCenterY = kRackTop + kModuleHeaderH + kKnobCenterYOffset;
                const float knobX = kModuleLeft + 50.0f;

                const float chipX = knobX - kValueChipW * 0.5f;
                const float chipY = knobCenterY + kKnobRadius + kValueChipGap;
                if (mx >= chipX && mx <= chipX + kValueChipW && my >= chipY && my <= chipY + kValueChipH)
                {
                    startKnobEdit(kInputPedalIndex, 0);
                    return true;
                }

                const float dx = mx - knobX;
                const float dy = my - knobCenterY;
                if (dx * dx + dy * dy <= kKnobRadius * kKnobRadius)
                {
                    draggingKnobPedal = kInputPedalIndex;
                    draggingKnobIndex = 0;
                    draggingKnobStartY = my;
                    draggingKnobStartValue = paramValues[kParamInputGain];
                    editParameter(kParamInputGain, true);
                    return true;
                }

                return true; // swallow clicks elsewhere on the panel (e.g. the meter)
            }
        }

        // --- Module clicks (knobs / switch / remove / drag handle) ---
        const std::vector<int> order = getActiveOrder();
        float y = kPedalRackTop + scrollOffset;

        for (size_t slot = 0; slot < order.size(); ++slot)
        {
            const int pedalIndex = order[slot];
            const PedalDef& def = kPedalDefs[pedalIndex];
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const float moduleW = getWidth() - kModuleLeft - 20.0f;

            if (my >= y && my < y + moduleH && mx >= kModuleLeft && mx <= kModuleLeft + moduleW)
            {
                if (pedalIndex != kAmpPedalIndex)
                {
                    const float sx = kModuleLeft + 12.0f;
                    const float sy = y + (kModuleHeaderH - kSwitchSize) * 0.5f;
                    if (mx >= sx && mx <= sx + kSwitchSize * 1.6f && my >= sy && my <= sy + kSwitchSize)
                    {
                        const int bypassParam = def.bypassParam;
                        const bool currentlyBypassed = paramValues[bypassParam] > 0.5f;
                        editParameter(bypassParam, true);
                        setParameterValue(bypassParam, currentlyBypassed ? 0.0f : 1.0f);
                        paramValues[bypassParam] = currentlyBypassed ? 0.0f : 1.0f;
                        editParameter(bypassParam, false);
                        repaint();
                        return true;
                    }

                    const float rx = kModuleLeft + moduleW - kRemoveSize - 12.0f;
                    const float ry = y + (kModuleHeaderH - kRemoveSize) * 0.5f;
                    if (mx >= rx && mx <= rx + kRemoveSize && my >= ry && my <= ry + kRemoveSize)
                    {
                        togglePedalPresence(pedalIndex);
                        return true;
                    }
                }

                const float knobCenterY = y + kModuleHeaderH + kKnobCenterYOffset;
                float knobX = kModuleLeft + 50.0f;
                for (size_t k = 0; k < def.knobs.size(); ++k)
                {
                    // The value readout chip - click it to type an exact value.
                    const float chipX = knobX - kValueChipW * 0.5f;
                    const float chipY = knobCenterY + kKnobRadius + kValueChipGap;
                    if (mx >= chipX && mx <= chipX + kValueChipW && my >= chipY && my <= chipY + kValueChipH)
                    {
                        startKnobEdit(pedalIndex, static_cast<int>(k));
                        return true;
                    }

                    const float dx = mx - knobX;
                    const float dy = my - knobCenterY;
                    if (dx * dx + dy * dy <= kKnobRadius * kKnobRadius)
                    {
                        draggingKnobPedal = pedalIndex;
                        draggingKnobIndex = static_cast<int>(k);
                        draggingKnobStartY = my;
                        draggingKnobStartValue = paramValues[def.knobs[k].paramIndex];
                        editParameter(def.knobs[k].paramIndex, true);
                        return true;
                    }
                    knobX += kKnobSpacing;
                }

                if (def.hasFileLoader)
                {
                    const float btnY = knobCenterY - kFileLoaderH * 0.5f;
                    if (mx >= knobX && mx <= knobX + kFileLoaderW && my >= btnY && my <= btnY + kFileLoaderH)
                    {
                        requestStateFile(def.stateKey);
                        return true;
                    }
                }

                draggingModuleIndex = pedalIndex;
                dragModuleGrabOffsetY = my - y;
                dragModuleCurrentY = y;
                editParameter(def.positionParam, true);
                return true;
            }

            y += moduleH + kModuleGap;
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float mx = static_cast<float>(ev.pos.getX());
        const float my = static_cast<float>(ev.pos.getY());
        lastMouseX = mx;
        lastMouseY = my;

        if (paletteDraggingPedal >= 0)
        {
            paletteDragCurrentX = mx;
            paletteDragCurrentY = my;
            if (!paletteDragMoved)
            {
                const float dx = mx - paletteDragStartX;
                const float dy = my - paletteDragStartY;
                if (dx * dx + dy * dy > kDragThreshold * kDragThreshold)
                    paletteDragMoved = true;
            }
            repaint();
            return true;
        }

        if (draggingKnobPedal >= 0)
        {
            const KnobDef& knob = resolveKnob(draggingKnobPedal, draggingKnobIndex);
            const float deltaPixels = draggingKnobStartY - my;
            const float range = knob.maxVal - knob.minVal;
            float newValue = draggingKnobStartValue + (deltaPixels / kKnobDragSensitivity) * range;
            newValue = std::max(knob.minVal, std::min(knob.maxVal, newValue));

            setParameterValue(knob.paramIndex, newValue);
            paramValues[knob.paramIndex] = newValue;
            displayValues[knob.paramIndex] = newValue;
            repaint();
            return true;
        }

        if (draggingModuleIndex >= 0)
        {
            // The card itself floats and follows the cursor (drawRack()
            // draws it separately, on top, at dragModuleCurrentY) instead of
            // staying pinned in its slot until a swap happens - that's what
            // made the old behavior not actually feel like drag-and-drop.
            dragModuleCurrentY = my - dragModuleGrabOffsetY;

            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const float draggedCenterY = dragModuleCurrentY + moduleH * 0.5f;

            // Where the floating card's center currently sits among the
            // OTHER cards (stacked as if the dragged one weren't there)
            // decides where it will land - the rest of the rack reflows
            // live to open up a gap there, instead of a single pairwise swap.
            std::vector<int> others;
            for (int p : getActiveOrder())
                if (p != draggingModuleIndex)
                    others.push_back(p);

            int insertAt = static_cast<int>(others.size());
            float oy = kPedalRackTop + scrollOffset;
            for (size_t i = 0; i < others.size(); ++i)
            {
                if (draggedCenterY < oy + moduleH * 0.5f)
                {
                    insertAt = static_cast<int>(i);
                    break;
                }
                oy += moduleH + kModuleGap;
            }

            others.insert(others.begin() + insertAt, draggingModuleIndex);

            for (size_t i = 0; i < others.size(); ++i)
            {
                const int posParam = kPedalDefs[others[i]].positionParam;
                const float newPos = static_cast<float>(i);
                if (paramValues[posParam] != newPos)
                {
                    setParameterValue(posParam, newPos);
                    paramValues[posParam] = newPos;
                }
            }

            repaint();
            return true;
        }

        // Nothing is being dragged - just repaint so hover-driven visuals
        // (dropdown row highlight, per-card remove-button hover) stay live.
        repaint();
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float contentHeight = totalContentHeight();
        const float visibleHeight = static_cast<float>(getHeight()) - kPedalRackTop;
        const float maxScroll = std::max(0.0f, contentHeight - visibleHeight);

        scrollOffset += static_cast<float>(ev.delta.getY()) * 24.0f;
        scrollOffset = std::max(-maxScroll, std::min(0.0f, scrollOffset));
        repaint();
        return true;
    }

    // The host informs us here whenever a DPF State value changes -
    // whether from session restore or from our own requestStateFile()
    // (see the file-loader button handling below). We just keep a local
    // copy so the pedal card can display e.g. the loaded IR's filename.
    void stateChanged(const char* key, const char* value) override
    {
        stateValues[key] = value;

        // See kPresetImportStateKey's comment in ChainParameters.hpp - this
        // fires once the user picks a file via requestStateFile() in
        // handleControlBarClick()'s Import button. value[0]=='\0' means the
        // dialog was cancelled, not a file with an empty name.
        if (std::strcmp(key, kPresetImportStateKey) == 0 && value[0] != '\0')
            importPresetsFromFile(value);

        repaint();
    }

private:
    // ---------------- Presets: file I/O ----------------

    std::string getPresetsFilePath() const
    {
        const char* home = std::getenv("HOME");
        const std::string dir = std::string(home != nullptr ? home : ".") + "/.config/ampforge";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir + "/user_presets.txt";
    }

    // Shared by loadCustomPresets() (reading our own user_presets.txt) and
    // importPresetsFromFile() (reading a file the user picked, which may
    // be a single exported preset or another copy of user_presets.txt
    // wholesale - both use this exact PRESET/name/values/ENDPRESET format,
    // so nothing distinguishes "our file" from "an imported file" here).
    static void parsePresetsFromStream(std::istream& f, std::vector<CustomPreset>& out)
    {
        std::string line;
        while (std::getline(f, line))
        {
            if (line != "PRESET")
                continue;

            CustomPreset p;
            std::getline(f, p.name);

            // Read until ENDPRESET rather than a fixed kParamCount lines,
            // so a preset saved by an older build (before a new parameter
            // was appended - new parameters always go at the end, never
            // inserted, precisely to keep old preset files loadable) still
            // parses instead of desyncing every preset that follows it in
            // the file. Anything the old file didn't have falls back to
            // the same defaults a blank preset uses.
            //
            // foundEnd distinguishes "older file, fewer values than
            // kParamCount, terminated properly" (fine, fill the rest with
            // defaults below) from "file truncated/corrupted mid-write,
            // ENDPRESET never appears before EOF" (discard - otherwise a
            // crash mid-save would leave a bogus empty/default preset
            // permanently stuck in the list).
            uint32_t i = 0;
            bool foundEnd = false;
            while (std::getline(f, line))
            {
                if (line == "ENDPRESET")
                {
                    foundEnd = true;
                    break;
                }
                if (i < kParamCount)
                    p.values[i] = std::strtof(line.c_str(), nullptr);
                ++i;
            }

            if (foundEnd)
            {
                for (uint32_t j = i; j < kParamCount; ++j)
                    p.values[j] = kBlankPresetValues[j];
                out.push_back(p);
            }
        }
    }

    void loadCustomPresets()
    {
        customPresets.clear();
        std::ifstream f(getPresetsFilePath());
        if (!f.is_open())
            return;
        parsePresetsFromStream(f, customPresets);
    }

    // Where a "Export" writes a preset to - separate from the main
    // user_presets.txt (getPresetsFilePath()) since that file is our own
    // append-only store, not meant to be handed to another person/machine
    // one preset at a time. The exported file uses the exact same
    // PRESET/.../ENDPRESET format, so it (and, for that matter, a whole
    // copied-over user_presets.txt) can be handed straight back to Import.
    std::string getExportsDirPath() const
    {
        const char* home = std::getenv("HOME");
        const std::string dir = std::string(home != nullptr ? home : ".") + "/.config/ampforge/exports";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    static std::string sanitizeFilename(const std::string& name)
    {
        std::string out;
        for (const char c : name)
            out += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == ' ') ? c : '_';
        return out.empty() ? "preset" : out;
    }

    // Writes whatever's currently loaded (paramValues) - the same "current
    // live state" Save already captures - to its own file under
    // getExportsDirPath(), named after activePresetName. There's no native
    // "Save As" dialog available here (see importPresetsFromFile()'s
    // comment on requestStateFile() being open-only), so the exported
    // file always lands in that fixed, predictable folder; the toast tells
    // the user exactly where so they can find/rename/share it themselves.
    void exportActivePreset()
    {
        const std::string path = getExportsDirPath() + "/" + sanitizeFilename(activePresetName) + ".ampforgepreset";

        std::ofstream f(path, std::ios::trunc);
        if (!f.is_open())
        {
            triggerSaveToast("Export failed");
            return;
        }

        f << "PRESET\n" << activePresetName << "\n";
        for (uint32_t i = 0; i < kParamCount; ++i)
            f << paramToString(paramValues[i]) << "\n";
        f << "ENDPRESET\n";

        triggerSaveToast("Exported to " + path);
    }

    // Called from stateChanged() once the user picks a file via
    // requestStateFile() (see handleControlBarClick()'s Import button).
    // Imported presets whose name collides with an existing custom preset
    // overwrite it in place, the same "same name = update" rule
    // confirmSavePreset() already applies to Save.
    void importPresetsFromFile(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        {
            triggerSaveToast("Import failed: couldn't open file");
            return;
        }

        std::vector<CustomPreset> imported;
        parsePresetsFromStream(f, imported);

        if (imported.empty())
        {
            triggerSaveToast("No presets found in that file");
            return;
        }

        for (const CustomPreset& p : imported)
        {
            bool replaced = false;
            for (CustomPreset& existing : customPresets)
            {
                if (existing.name == p.name)
                {
                    existing = p;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                customPresets.push_back(p);
        }

        saveCustomPresetsToFile();
        triggerSaveToast(imported.size() == 1
            ? ("Imported \"" + imported[0].name + "\"")
            : ("Imported " + std::to_string(imported.size()) + " presets"));
        repaint();
    }

    void saveCustomPresetsToFile() const
    {
        std::ofstream f(getPresetsFilePath(), std::ios::trunc);
        if (!f.is_open())
            return;
        for (const CustomPreset& p : customPresets)
        {
            f << "PRESET\n" << p.name << "\n";
            for (uint32_t i = 0; i < kParamCount; ++i)
                f << paramToString(p.values[i]) << "\n";
            f << "ENDPRESET\n";
        }
    }

    static std::string paramToString(float v)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6f", v);
        return buf;
    }

    // True if the currently active preset is a saved custom preset (as
    // opposed to a factory preset, or the blank "Untitled" canvas) - that's
    // exactly the case where Save can update it in place.
    bool isActivePresetCustom() const
    {
        for (const CustomPreset& p : customPresets)
            if (p.name == activePresetName)
                return true;
        return false;
    }

    void triggerSaveToast(const std::string& text)
    {
        saveToastText = text;
        saveToastAlpha = 1.0f;
        repaint();
    }

    // The Save button's actual behavior: if we're already on a saved custom
    // preset, just overwrite it immediately - no dialog, no risk of ending
    // up with two entries sharing a name. Only prompt for a name when there
    // isn't an existing custom preset to update (a factory preset or the
    // blank canvas is active).
    void handleSaveButtonClick()
    {
        for (CustomPreset& existing : customPresets)
        {
            if (existing.name == activePresetName)
            {
                for (uint32_t i = 0; i < kParamCount; ++i)
                    existing.values[i] = paramValues[i];
                saveCustomPresetsToFile();
                triggerSaveToast("Saved \"" + existing.name + "\"");
                return;
            }
        }

        savingPreset = true;
        nameInputBuffer = (activePresetName == "Untitled") ? "" : activePresetName;
    }

    void deleteActiveCustomPreset()
    {
        for (size_t i = 0; i < customPresets.size(); ++i)
        {
            if (customPresets[i].name == activePresetName)
            {
                const std::string deletedName = customPresets[i].name;
                customPresets.erase(customPresets.begin() + static_cast<long>(i));
                saveCustomPresetsToFile();
                activePresetName = "Untitled";
                triggerSaveToast("Deleted \"" + deletedName + "\"");
                return;
            }
        }
    }

    void confirmSavePreset()
    {
        if (nameInputBuffer.empty())
            return;

        // Saving under a name that already exists in the custom list updates
        // that preset in place instead of piling up duplicate entries - this
        // is a safety net for the naming dialog; the common "just update
        // what I already had loaded" case is handled instantly by
        // handleSaveButtonClick() without ever opening this dialog.
        for (CustomPreset& existing : customPresets)
        {
            if (existing.name == nameInputBuffer)
            {
                for (uint32_t i = 0; i < kParamCount; ++i)
                    existing.values[i] = paramValues[i];
                saveCustomPresetsToFile();

                activePresetName = existing.name;
                savingPreset = false;
                nameInputBuffer.clear();
                triggerSaveToast("Saved \"" + existing.name + "\"");
                return;
            }
        }

        CustomPreset p;
        p.name = nameInputBuffer;
        for (uint32_t i = 0; i < kParamCount; ++i)
            p.values[i] = paramValues[i];
        customPresets.push_back(p);
        saveCustomPresetsToFile();

        activePresetName = p.name;
        savingPreset = false;
        nameInputBuffer.clear();
        triggerSaveToast("Saved \"" + p.name + "\"");
    }

    void applyValuesArray(const float* values)
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            editParameter(i, true);
            setParameterValue(i, values[i]);
            paramValues[i] = values[i];
            editParameter(i, false);
        }
        // A preset switch is an instant snapshot, not a per-pedal add/remove -
        // make sure no card is left mid-fade from before the switch.
        for (int i = 0; i < kPedalDefCount; ++i)
        {
            moduleRemoving[i] = false;
            moduleAlpha[i] = 1.0f;
        }
        repaint();
    }

    // Switches the "active" A/B slot: saves the current live chain into
    // whichever slot we're leaving (so in-progress tweaks are never lost,
    // matching how DAWs' own A/B compare buttons behave), then loads the
    // target slot - or, the first time a slot is visited, just seeds it
    // as a copy of what's currently playing, so the user starts from an
    // identical-sounding B and can tweak away before comparing back to A.
    void switchABSlot(char target)
    {
        if (target == abActiveSlot)
            return;

        float* fromSnap = (abActiveSlot == 'A') ? abSnapshotA : abSnapshotB;
        for (uint32_t i = 0; i < kParamCount; ++i)
            fromSnap[i] = paramValues[i];
        if (abActiveSlot == 'A') abHasSnapshotA = true; else abHasSnapshotB = true;

        float* toSnap = (target == 'A') ? abSnapshotA : abSnapshotB;
        bool& toHasSnapshot = (target == 'A') ? abHasSnapshotA : abHasSnapshotB;
        if (toHasSnapshot)
        {
            applyValuesArray(toSnap);
        }
        else
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
                toSnap[i] = fromSnap[i];
            toHasSnapshot = true;
        }

        abActiveSlot = target;
    }

    void applyBlankPreset()
    {
        applyValuesArray(kBlankPresetValues);
        activePresetName = "Untitled";
        presetDropdownOpen = false;
    }

    // ---------------- Control bar / dropdown / modal ----------------

    bool handleControlBarClick(float mx, float my)
    {
        (void)my;
        const float presetBtnW = 220.0f;
        const float newBtnW = 70.0f;
        const float saveBtnW = 70.0f;
        const float deleteBtnW = 70.0f;
        const float tunerBtnW = 70.0f;
        const float abBtnW = 32.0f;
        const float exportBtnW = 70.0f;
        const float importBtnW = 70.0f;
        const float gap = 8.0f;
        float x = 12.0f;

        if (mx >= x && mx <= x + presetBtnW)
        {
            presetDropdownOpen = !presetDropdownOpen;
            return true;
        }
        x += presetBtnW + gap;

        if (mx >= x && mx <= x + newBtnW)
        {
            applyBlankPreset();
            return true;
        }
        x += newBtnW + gap;

        if (mx >= x && mx <= x + saveBtnW)
        {
            handleSaveButtonClick();
            return true;
        }
        x += saveBtnW + gap;

        if (mx >= x && mx <= x + deleteBtnW)
        {
            if (isActivePresetCustom())
                deleteActiveCustomPreset();
            return true;
        }
        x += deleteBtnW + gap;

        // Unreachable while the tuner's already open - the overlay's own
        // click-anywhere-to-close gate in onMouse() intercepts first.
        if (mx >= x && mx <= x + tunerBtnW)
        {
            openTuner();
            return true;
        }
        x += tunerBtnW + gap;

        if (mx >= x && mx <= x + abBtnW)
        {
            switchABSlot('A');
            return true;
        }
        x += abBtnW + 2.0f;

        if (mx >= x && mx <= x + abBtnW)
        {
            switchABSlot('B');
            return true;
        }
        x += abBtnW + gap;

        if (mx >= x && mx <= x + exportBtnW)
        {
            exportActivePreset();
            return true;
        }
        x += exportBtnW + gap;

        if (mx >= x && mx <= x + importBtnW)
        {
            requestStateFile(kPresetImportStateKey);
            return true;
        }

        return false;
    }

    bool handleDropdownClick(float mx, float my)
    {
        const float dropX = 12.0f;
        const float dropY = kTopBarHeight + kControlBarHeight;
        const float dropW = 220.0f;
        const uint32_t customRows = customPresets.empty() ? 0 : static_cast<uint32_t>(1 + customPresets.size());
        const float dropH = kDropdownRowH * (1 + kProgramCount + customRows);

        // Bounding box check on both axes - previously only X was checked,
        // so any click below the dropdown but still under its X range (e.g.
        // on a palette pedal, since the palette sits at the same X) was
        // silently swallowed as "inside the dropdown" without closing it,
        // making the menu feel stuck/unresponsive.
        if (mx < dropX || mx > dropX + dropW || my < dropY || my >= dropY + dropH)
            return false;

        float rowY = dropY + kDropdownRowH; // skip "Factory" header row
        for (uint32_t i = 0; i < kProgramCount; ++i)
        {
            if (my >= rowY && my < rowY + kDropdownRowH)
            {
                applyValuesArray(kPresets[i].values);
                activePresetName = kPresets[i].name;
                presetDropdownOpen = false;
                return true;
            }
            rowY += kDropdownRowH;
        }

        if (!customPresets.empty())
        {
            rowY += kDropdownRowH; // "Custom" header row
            for (size_t ci = 0; ci < customPresets.size(); ++ci)
            {
                if (my >= rowY && my < rowY + kDropdownRowH)
                {
                    applyValuesArray(customPresets[ci].values);
                    activePresetName = customPresets[ci].name;
                    presetDropdownOpen = false;
                    return true;
                }
                rowY += kDropdownRowH;
            }
        }

        return true; // inside the box, but on a header row / gap - consume
    }

    void handleModalClick(float mx, float my)
    {
        const float boxW = 360.0f, boxH = 150.0f;
        const float boxX = (static_cast<float>(getWidth()) - boxW) * 0.5f;
        const float boxY = (static_cast<float>(getHeight()) - boxH) * 0.5f;

        const float btnW = 100.0f, btnH = 34.0f;
        const float saveX = boxX + boxW - btnW - 18.0f;
        const float cancelX = saveX - btnW - 12.0f;
        const float btnY = boxY + boxH - btnH - 18.0f;

        if (mx >= saveX && mx <= saveX + btnW && my >= btnY && my <= btnY + btnH)
        {
            confirmSavePreset();
        }
        else if (mx >= cancelX && mx <= cancelX + btnW && my >= btnY && my <= btnY + btnH)
        {
            savingPreset = false;
            nameInputBuffer.clear();
        }
        repaint();
    }

    // Looks up a knob's definition given the same (pedalIndex, knobIndex)
    // pair the drag/type-to-edit code already threads through everywhere -
    // this is the one place that also knows about kInputPedalIndex, so the
    // shared drag/edit code above doesn't need to.
    const KnobDef& resolveKnob(int pedalIndex, int knobIndex) const
    {
        if (pedalIndex == kInputPedalIndex)
            return kInputGainKnob;
        return kPedalDefs[pedalIndex].knobs[knobIndex];
    }

    // ---------------- Manual knob value entry ----------------

    void startKnobEdit(int pedalIndex, int knobIndex)
    {
        editingKnobPedal = pedalIndex;
        editingKnobIndex = knobIndex;

        const KnobDef& knob = resolveKnob(pedalIndex, knobIndex);
        float shown = paramValues[knob.paramIndex];
        if (knob.asPercent)
            shown *= 100.0f;

        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.*f", knob.asPercent ? 0 : knob.decimals, shown);
        knobEditBuffer = buf;
        repaint();
    }

    void confirmKnobEdit()
    {
        if (editingKnobPedal < 0)
            return;

        const KnobDef& knob = resolveKnob(editingKnobPedal, editingKnobIndex);
        if (!knobEditBuffer.empty() && knobEditBuffer != "-")
        {
            float typed = std::strtof(knobEditBuffer.c_str(), nullptr);
            if (knob.asPercent)
                typed *= 0.01f;
            typed = std::max(knob.minVal, std::min(knob.maxVal, typed));

            editParameter(knob.paramIndex, true);
            setParameterValue(knob.paramIndex, typed);
            paramValues[knob.paramIndex] = typed;
            displayValues[knob.paramIndex] = typed;
            editParameter(knob.paramIndex, false);
        }

        editingKnobPedal = -1;
        editingKnobIndex = -1;
        knobEditBuffer.clear();
        repaint();
    }

    void cancelKnobEdit()
    {
        editingKnobPedal = -1;
        editingKnobIndex = -1;
        knobEditBuffer.clear();
        repaint();
    }

    // ---------------- Pedal chain logic ----------------

    void togglePedalPresence(int pedalIndex)
    {
        const int onParam = kPedalDefs[pedalIndex].onParam;
        if (onParam < 0)
            return;

        const bool currentlyOn = paramValues[onParam] > 0.5f;

        if (currentlyOn)
        {
            // Start a fade-out; uiIdle() clears the parameter once the
            // animation finishes (see the moduleRemoving handling there).
            moduleRemoving[pedalIndex] = true;
        }
        else
        {
            editParameter(onParam, true);
            addPedalAtDefaultPosition(pedalIndex);
            editParameter(onParam, false);
        }

        repaint();
    }

    // Turns a pedal on and drops it into its canonical slot in the
    // recommended signal chain (PedalDef::defaultPosition), rather than
    // appending it after whatever happens to be on the board already -
    // that "append at the end" behavior was what made the order feel like
    // it got scrambled after a few add/remove cycles. The user can still
    // drag it anywhere afterward.
    void addPedalAtDefaultPosition(int pedalIndex)
    {
        moduleRemoving[pedalIndex] = false;

        const int onParam = kPedalDefs[pedalIndex].onParam;
        setParameterValue(onParam, 1.0f);
        paramValues[onParam] = 1.0f;

        const int posParam = kPedalDefs[pedalIndex].positionParam;
        const float newPos = static_cast<float>(kPedalDefs[pedalIndex].defaultPosition);
        editParameter(posParam, true);
        setParameterValue(posParam, newPos);
        paramValues[posParam] = newPos;
        editParameter(posParam, false);

        moduleAlpha[pedalIndex] = 0.0f; // animate it fading/sliding in
    }

    void swapPositions(int pedalA, int pedalB)
    {
        const int posParamA = kPedalDefs[pedalA].positionParam;
        const int posParamB = kPedalDefs[pedalB].positionParam;
        const float posA = paramValues[posParamA];
        const float posB = paramValues[posParamB];

        setParameterValue(posParamA, posB);
        setParameterValue(posParamB, posA);
        paramValues[posParamA] = posB;
        paramValues[posParamB] = posA;
        displayValues[posParamA] = posB;
        displayValues[posParamB] = posA;
    }

    // Called when the mouse is released while a palette drag was active.
    void finishPaletteDrag(float mx, float my)
    {
        const int pedalIndex = paletteDraggingPedal;
        paletteDraggingPedal = -1;

        if (!paletteDragMoved)
        {
            // Just a click, no real drag - keep the old quick add/remove behavior.
            togglePedalPresence(pedalIndex);
            return;
        }

        const bool alreadyOn = paramValues[kPedalDefs[pedalIndex].onParam] > 0.5f;
        if (alreadyOn)
            return; // dragging an already-active pedal is a no-op for now

        if (mx < kModuleLeft)
            return; // dropped outside the rack - cancel

        editParameter(kPedalDefs[pedalIndex].onParam, true);
        addPedalAtDefaultPosition(pedalIndex);
        editParameter(kPedalDefs[pedalIndex].onParam, false);

        // If dropped over a specific module, move the new pedal there
        // instead of leaving it appended at the very end.
        const std::vector<int> order = getActiveOrder();
        float y = kPedalRackTop + scrollOffset;
        for (int p : order)
        {
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            if (p != pedalIndex && my >= y && my < y + moduleH)
            {
                swapPositions(pedalIndex, p);
                break;
            }
            y += moduleH + kModuleGap;
        }
    }

    std::vector<int> getActiveOrder() const
    {
        std::vector<int> active;
        for (int i = 0; i < kPedalDefCount; ++i)
        {
            const int onParam = kPedalDefs[i].onParam;
            if (onParam < 0 || paramValues[onParam] > 0.5f)
                active.push_back(i);
        }
        std::sort(active.begin(), active.end(), [this](int a, int b)
        {
            return paramValues[kPedalDefs[a].positionParam] < paramValues[kPedalDefs[b].positionParam];
        });
        return active;
    }

    float totalContentHeight() const
    {
        const std::vector<int> order = getActiveOrder();
        const float moduleH = kModuleHeaderH + kKnobAreaH;
        if (order.empty())
            return 0.0f;
        return static_cast<float>(order.size()) * moduleH + static_cast<float>(order.size() - 1) * kModuleGap;
    }

    // ---------------- Tuner ----------------

    struct NoteInfo
    {
        const char* name;
        int octave;
        float cents; // -50..+50, distance from this note's exact pitch
    };

    // Standard equal-temperament conversion: how many semitones (possibly
    // fractional) hz is from A4 (440Hz, MIDI note 69) says both which
    // note is closest and how far off it is - the fractional remainder,
    // in cents (1/100 semitone), is exactly what the tuner needle shows.
    static NoteInfo frequencyToNote(float hz)
    {
        static const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        const float midiFloat = 69.0f + 12.0f * std::log2(hz / 440.0f);
        const int midiRounded = static_cast<int>(std::round(midiFloat));
        const float cents = (midiFloat - static_cast<float>(midiRounded)) * 100.0f;
        const int noteIndex = ((midiRounded % 12) + 12) % 12;
        const int octave = midiRounded / 12 - 1; // MIDI note 0 = C-1, so note 60 (C4) needs -1 here
        return { kNoteNames[noteIndex], octave, cents };
    }

    void openTuner()
    {
        editParameter(kParamTunerOn, true);
        setParameterValue(kParamTunerOn, 1.0f);
        paramValues[kParamTunerOn] = 1.0f;
        editParameter(kParamTunerOn, false);
        repaint();
    }

    void closeTuner()
    {
        editParameter(kParamTunerOn, true);
        setParameterValue(kParamTunerOn, 0.0f);
        paramValues[kParamTunerOn] = 0.0f;
        editParameter(kParamTunerOn, false);
        repaint();
    }

    void formatKnobValue(const KnobDef& knob, float value, char* buf, size_t bufSize) const
    {
        if (knob.isSyncDivision)
        {
            const int idx = std::clamp(static_cast<int>(std::round(value)), 0, kSyncDivisionCount - 1);
            std::snprintf(buf, bufSize, "%s", kSyncDivisions[idx].label);
        }
        else if (knob.isAmpType)
        {
            const int idx = std::clamp(static_cast<int>(std::round(value)), 0, ampforge::kAmpVoicingCount - 1);
            std::snprintf(buf, bufSize, "%s", ampforge::kAmpVoicings[idx].name);
        }
        else if (knob.asPercent)
            std::snprintf(buf, bufSize, "%.0f%%", value * 100.0f);
        else
            std::snprintf(buf, bufSize, "%.*f%s", knob.decimals, value, knob.unit);
    }

    // ---------------- Drawing ----------------

    // Vertical gradient used by every "button-like" rectangle so the whole
    // UI reads as one consistent, slightly glossy material.
    Paint buttonGradient(float x, float y, float h, bool active)
    {
        return active
            ? linearGradient(x, y, x, y + h, Color(kColorButtonAccent, Color(255, 255, 255), 0.25f), kColorButtonAccent)
            : linearGradient(x, y, x, y + h, Color(kColorButton, Color(255, 255, 255), 0.07f), Color(kColorButton, Color(0, 0, 0), 0.15f));
    }

    void drawButton(float x, float y, float w, float h, const char* label, bool active, float alpha = 1.0f)
    {
        save();
        globalAlpha(alpha);

        beginPath();
        roundedRect(x, y, w, h, 6.0f);
        fillPaint(buttonGradient(x, y, h, active));
        fill();
        closePath();
        beginPath();
        roundedRect(x, y, w, h, 6.0f);
        strokeWidth(1.0f);
        strokeColor(Color(255, 255, 255, active ? 0.18f : 0.06f));
        stroke();
        closePath();

        fontSize(13.0f);
        fillColor(active ? kColorTextDark : kColorTextPrimary);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(x + w * 0.5f, y + h * 0.5f, label, nullptr);

        restore();
    }

    void drawTopBar(float width)
    {
        beginPath();
        rect(0.0f, 0.0f, width, kTopBarHeight);
        fillPaint(linearGradient(0.0f, 0.0f, 0.0f, kTopBarHeight, Color(36, 36, 43), Color(19, 19, 23)));
        fill();
        closePath();

        // A thin glowing accent line under the top bar - a bit of brand
        // identity, and a visual anchor separating chrome from content.
        beginPath();
        rect(0.0f, kTopBarHeight - 2.0f, width, 2.0f);
        fillColor(kColorButtonAccent.withAlpha(0.9f));
        fill();
        closePath();
        beginPath();
        rect(0.0f, kTopBarHeight - 2.0f, width, 16.0f);
        fillPaint(boxGradient(0.0f, kTopBarHeight - 2.0f, width, 4.0f, 0.0f, 10.0f,
                               kColorButtonAccent.withAlpha(0.35f), kColorButtonAccent.withAlpha(0.0f)));
        fill();
        closePath();

        const float logoSize = 30.0f, logoX = 16.0f, logoY = (kTopBarHeight - logoSize) * 0.5f;
        beginPath();
        roundedRect(logoX, logoY, logoSize, logoSize, 7.0f);
        fillPaint(linearGradient(logoX, logoY, logoX, logoY + logoSize,
                                  Color(kColorButtonAccent, Color(255, 255, 255), 0.25f),
                                  Color(kColorButtonAccent, Color(0, 0, 0), 0.15f)));
        fill();
        closePath();
        fontSize(15.0f);
        fillColor(kColorTextDark);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(logoX + logoSize * 0.5f, logoY + logoSize * 0.5f + 1.0f, "AF", nullptr);

        fontSize(19.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        textLetterSpacing(0.6f);
        text(logoX + logoSize + 12.0f, kTopBarHeight * 0.5f, "AmpForge", nullptr);
        textLetterSpacing(0.0f);

        drawCpuMeter(width - 20.0f, kTopBarHeight * 0.5f, displayValues[kParamCpuLoad]);
    }

    // A small "CPU xx%" readout with a status dot, right-aligned in the
    // top bar - especially useful since the Cabinet block's convolution
    // can be the single most expensive thing in the chain. Green/amber/red
    // thresholds are a rough, deliberately generous heuristic (a plugin
    // sitting at 50% of one block's budget is already worth noticing, not
    // just the point right before an actual dropout).
    void drawCpuMeter(float rightX, float centerY, float load)
    {
        const Color dotColor = (load > 0.85f) ? kColorRemove
                              : (load > 0.5f)  ? Color(230, 175, 60)
                                                : kColorOn;

        // Fixed-width layout (dot, then "CPU xx%" text) rather than
        // measuring the text and working backwards from rightX - simpler,
        // and immune to font-metric differences between platforms.
        const float dotX = rightX - 66.0f;
        const float textX = dotX + 10.0f;

        beginPath();
        circle(dotX, centerY, 4.0f);
        fillColor(dotColor);
        fill();
        closePath();

        char buf[16];
        std::snprintf(buf, sizeof(buf), "CPU %.0f%%", load * 100.0f);

        fontSize(12.0f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(textX, centerY, buf, nullptr);
    }

    void drawControlBar()
    {
        const float width = static_cast<float>(getWidth());

        beginPath();
        rect(0.0f, kTopBarHeight, width, kControlBarHeight);
        fillPaint(linearGradient(0.0f, kTopBarHeight, 0.0f, kTopBarHeight + kControlBarHeight, Color(26, 26, 32), Color(19, 19, 23)));
        fill();
        closePath();
        beginPath();
        rect(0.0f, kTopBarHeight + kControlBarHeight - 1.0f, width, 1.0f);
        fillColor(Color(0, 0, 0, 0.4f));
        fill();
        closePath();

        const float presetBtnW = 220.0f;
        const float newBtnW = 70.0f;
        const float saveBtnW = 70.0f;
        const float deleteBtnW = 70.0f;
        const float tunerBtnW = 70.0f;
        const float abBtnW = 32.0f;
        const float exportBtnW = 70.0f;
        const float importBtnW = 70.0f;
        const float gap = 8.0f;
        const float btnY = kTopBarHeight + 6.0f;
        const float btnH = kControlBarHeight - 12.0f;
        float x = 12.0f;

        // Preset dropdown button
        beginPath();
        roundedRect(x, btnY, presetBtnW, btnH, 6.0f);
        fillPaint(buttonGradient(x, btnY, btnH, presetDropdownOpen));
        fill();
        closePath();
        beginPath();
        roundedRect(x, btnY, presetBtnW, btnH, 6.0f);
        strokeWidth(1.0f);
        strokeColor(Color(255, 255, 255, presetDropdownOpen ? 0.18f : 0.06f));
        stroke();
        closePath();

        fontSize(13.0f);
        fillColor(presetDropdownOpen ? kColorTextDark : kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(x + 12.0f, btnY + btnH * 0.5f, ("Preset: " + activePresetName).c_str(), nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        text(x + presetBtnW - 12.0f, btnY + btnH * 0.5f, presetDropdownOpen ? "^" : "v", nullptr);
        x += presetBtnW + gap;

        drawButton(x, btnY, newBtnW, btnH, "+ New", false);
        x += newBtnW + gap;

        drawButton(x, btnY, saveBtnW, btnH, "Save", false);
        x += saveBtnW + gap;

        // Only meaningful for a saved custom preset - dimmed otherwise so it
        // doesn't invite deleting a factory preset (which isn't possible).
        const bool canDelete = isActivePresetCustom();
        drawButton(x, btnY, deleteBtnW, btnH, "Delete", false, canDelete ? 1.0f : 0.35f);
        x += deleteBtnW + gap;

        drawButton(x, btnY, tunerBtnW, btnH, "Tuner", paramValues[kParamTunerOn] > 0.5f);
        x += tunerBtnW + gap;

        // A/B compare - see switchABSlot()'s comment. Drawn as a tight
        // pair (2px gap) rather than two independent buttons, so it reads
        // as one segmented control instead of two unrelated ones.
        drawButton(x, btnY, abBtnW, btnH, "A", abActiveSlot == 'A');
        x += abBtnW + 2.0f;
        drawButton(x, btnY, abBtnW, btnH, "B", abActiveSlot == 'B');
        x += abBtnW + gap;

        drawButton(x, btnY, exportBtnW, btnH, "Export", false);
        x += exportBtnW + gap;

        drawButton(x, btnY, importBtnW, btnH, "Import", false);
        x += importBtnW + gap;

        if (saveToastAlpha > 0.0f)
        {
            fontSize(12.0f);
            fillColor(kColorOn.withAlpha(std::min(1.0f, saveToastAlpha * 3.0f)));
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(x, btnY + btnH * 0.5f, saveToastText.c_str(), nullptr);
        }
    }

    void drawPresetDropdown()
    {
        if (dropdownAnim < 0.001f)
            return;

        // Fade in/out and settle down into place, instead of popping
        // instantly - and everything below rides on one global alpha
        // instead of every fillColor/strokeColor needing its own.
        save();
        globalAlpha(dropdownAnim);

        const float dropX = 12.0f;
        const float dropY = kTopBarHeight + kControlBarHeight + (1.0f - dropdownAnim) * -8.0f;
        const float dropW = 220.0f;
        const uint32_t customRows = customPresets.empty() ? 0 : static_cast<uint32_t>(1 + customPresets.size());
        const float dropH = kDropdownRowH * (1 + kProgramCount + customRows);

        beginPath();
        rect(dropX - 20.0f, dropY - 10.0f, dropW + 40.0f, dropH + 40.0f);
        fillPaint(boxGradient(dropX, dropY + 4.0f, dropW, dropH, 6.0f, 16.0f, Color(0, 0, 0, 0.5f), Color(0, 0, 0, 0.0f)));
        fill();
        closePath();

        beginPath();
        roundedRect(dropX, dropY, dropW, dropH, 6.0f);
        fillColor(kColorDropdownBg);
        fill();
        closePath();

        // Clip everything below (row highlights, text) to the rounded box so
        // a highlighted first/last row doesn't square off the corners.
        save();
        scissor(dropX, dropY, dropW, dropH);

        float rowY = dropY;

        fontSize(10.5f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        textLetterSpacing(1.0f);
        text(dropX + 10.0f, rowY + kDropdownRowH * 0.5f, "FACTORY", nullptr);
        textLetterSpacing(0.0f);
        rowY += kDropdownRowH;

        for (uint32_t i = 0; i < kProgramCount; ++i)
        {
            drawPresetRow(dropX, rowY, dropW, kPresets[i].name, kPresets[i].name == activePresetName);
            rowY += kDropdownRowH;
        }

        if (!customPresets.empty())
        {
            beginPath();
            rect(dropX + 10.0f, rowY, dropW - 20.0f, 1.0f);
            fillColor(Color(255, 255, 255, 0.08f));
            fill();
            closePath();

            fontSize(10.5f);
            fillColor(kColorTextMuted);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            textLetterSpacing(1.0f);
            text(dropX + 10.0f, rowY + kDropdownRowH * 0.5f, "CUSTOM", nullptr);
            textLetterSpacing(0.0f);
            rowY += kDropdownRowH;

            for (const CustomPreset& p : customPresets)
            {
                drawPresetRow(dropX, rowY, dropW, p.name.c_str(), p.name == activePresetName);
                rowY += kDropdownRowH;
            }
        }

        restore(); // matches the scissor save()

        beginPath();
        roundedRect(dropX, dropY, dropW, dropH, 6.0f);
        strokeWidth(1.0f);
        strokeColor(Color(255, 255, 255, 0.08f));
        stroke();
        closePath();

        restore(); // matches the globalAlpha save()
    }

    void drawPresetRow(float rowX, float rowY, float rowW, const char* name, bool isActive)
    {
        const bool isHovered = (lastMouseX >= rowX && lastMouseX < rowX + rowW &&
                                 lastMouseY >= rowY && lastMouseY < rowY + kDropdownRowH);

        if (isActive)
        {
            beginPath();
            rect(rowX, rowY, rowW, kDropdownRowH);
            fillPaint(linearGradient(rowX, rowY, rowX, rowY + kDropdownRowH,
                                      Color(kColorButtonAccent, Color(255, 255, 255), 0.2f), kColorButtonAccent));
            fill();
            closePath();
            beginPath();
            rect(rowX, rowY, 3.0f, kDropdownRowH);
            fillColor(Color(255, 255, 255, 0.5f));
            fill();
            closePath();
        }
        else if (isHovered)
        {
            // Hover feedback - the dropdown previously gave no indication of
            // which row the cursor was over.
            beginPath();
            rect(rowX, rowY, rowW, kDropdownRowH);
            fillColor(Color(255, 255, 255, 0.07f));
            fill();
            closePath();
        }

        fontSize(13.0f);
        fillColor(isActive ? kColorTextDark : kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(rowX + 14.0f, rowY + kDropdownRowH * 0.5f, name, nullptr);
    }

    void drawSaveModal(float width, float height)
    {
        if (!savingPreset)
            return;

        beginPath();
        rect(0.0f, 0.0f, width, height);
        fillColor(kColorModalOverlay.withAlpha(0.6f));
        fill();
        closePath();

        const float boxW = 360.0f, boxH = 150.0f;
        const float boxX = (width - boxW) * 0.5f;
        const float boxY = (height - boxH) * 0.5f;

        beginPath();
        rect(boxX - 30.0f, boxY - 20.0f, boxW + 60.0f, boxH + 60.0f);
        fillPaint(boxGradient(boxX, boxY + 6.0f, boxW, boxH, 10.0f, 22.0f, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.0f)));
        fill();
        closePath();

        beginPath();
        roundedRect(boxX, boxY, boxW, boxH, 10.0f);
        fillPaint(linearGradient(boxX, boxY, boxX, boxY + boxH, Color(38, 38, 46), kColorModalBox));
        fill();
        closePath();
        beginPath();
        roundedRect(boxX, boxY, boxW, boxH, 10.0f);
        strokeWidth(1.0f);
        strokeColor(Color(255, 255, 255, 0.08f));
        stroke();
        closePath();

        fontSize(15.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(boxX + 18.0f, boxY + 16.0f, "Save preset as:", nullptr);
        beginPath();
        rect(boxX + 18.0f, boxY + 38.0f, 32.0f, 2.0f);
        fillColor(kColorButtonAccent.withAlpha(0.8f));
        fill();
        closePath();

        const float fieldX = boxX + 18.0f, fieldY = boxY + 48.0f, fieldW = boxW - 36.0f, fieldH = 34.0f;
        beginPath();
        roundedRect(fieldX, fieldY, fieldW, fieldH, 5.0f);
        fillColor(kColorBackground);
        fill();
        closePath();
        beginPath();
        roundedRect(fieldX, fieldY, fieldW, fieldH, 5.0f);
        strokeWidth(1.0f);
        strokeColor(kColorButtonAccent.withAlpha(0.6f));
        stroke();
        closePath();

        fontSize(14.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        if (nameInputBuffer.empty())
        {
            fillColor(kColorTextMuted.withAlpha(0.7f));
            text(fieldX + 10.0f, fieldY + fieldH * 0.5f, "e.g. My Custom Tone|", nullptr);
        }
        else
        {
            const std::string displayText = nameInputBuffer + "|";
            fillColor(kColorTextPrimary);
            text(fieldX + 10.0f, fieldY + fieldH * 0.5f, displayText.c_str(), nullptr);
        }

        const float btnW = 100.0f, btnH = 34.0f;
        const float saveX = boxX + boxW - btnW - 18.0f;
        const float cancelX = saveX - btnW - 12.0f;
        const float btnY = boxY + boxH - btnH - 18.0f;

        drawButton(cancelX, btnY, btnW, btnH, "Cancel", false);
        drawButton(saveX, btnY, btnW, btnH, "Save", true);
    }

    // Full-screen overlay shown while the Tuner is engaged (kParamTunerOn) -
    // a big note name, a +-50 cent needle bar, and the raw Hz reading.
    // Closes on any click or Escape (see onMouse()/onKeyboard()'s gating),
    // there's nothing to type here so it doesn't need drawSaveModal's
    // input-field machinery.
    void drawTunerOverlay(float width, float height)
    {
        if (paramValues[kParamTunerOn] < 0.5f)
            return;

        beginPath();
        rect(0.0f, 0.0f, width, height);
        fillColor(kColorModalOverlay.withAlpha(0.82f));
        fill();
        closePath();

        const float freq = displayValues[kParamTunerFrequency];
        const bool hasSignal = freq > 1.0f;
        const NoteInfo note = hasSignal ? frequencyToNote(freq) : NoteInfo{ "-", 0, 0.0f };
        const bool inTune = hasSignal && std::fabs(note.cents) < 5.0f;

        const float cx = width * 0.5f;
        const float cy = height * 0.5f;

        char noteBuf[8];
        if (hasSignal)
            std::snprintf(noteBuf, sizeof(noteBuf), "%s%d", note.name, note.octave);
        else
            std::snprintf(noteBuf, sizeof(noteBuf), "--");

        fontSize(110.0f);
        fillColor(hasSignal ? (inTune ? kColorOn : kColorTextPrimary) : kColorTextMuted);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(cx, cy - 70.0f, noteBuf, nullptr);

        // Cents bar: -50..+50, with a highlighted "in tune" zone in the
        // middle and a needle that only appears once a pitch is locked.
        const float barW = 340.0f, barH = 10.0f;
        const float barX = cx - barW * 0.5f, barY = cy + 20.0f;

        beginPath();
        roundedRect(barX, barY, barW, barH, barH * 0.5f);
        fillColor(kColorKnobTrack);
        fill();
        closePath();

        const float zoneW = barW * 0.1f; // +-5 cents
        beginPath();
        roundedRect(cx - zoneW * 0.5f, barY, zoneW, barH, barH * 0.5f);
        fillColor(kColorOn.withAlpha(0.35f));
        fill();
        closePath();

        beginPath();
        rect(cx - 1.0f, barY - 6.0f, 2.0f, barH + 12.0f);
        fillColor(Color(255, 255, 255, 0.25f));
        fill();
        closePath();

        if (hasSignal)
        {
            const float clampedCents = std::clamp(note.cents, -50.0f, 50.0f);
            const float needleX = cx + (clampedCents / 50.0f) * (barW * 0.5f);

            beginPath();
            circle(needleX, barY + barH * 0.5f, 9.0f);
            fillColor(inTune ? kColorOn : Color(230, 175, 60));
            fill();
            closePath();
            beginPath();
            circle(needleX, barY + barH * 0.5f, 9.0f);
            strokeWidth(1.5f);
            strokeColor(Color(0, 0, 0, 0.3f));
            stroke();
            closePath();
        }

        fontSize(15.0f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        char statusBuf[40];
        if (hasSignal)
            std::snprintf(statusBuf, sizeof(statusBuf), "%.1f Hz  %+.0f cents", freq, note.cents);
        else
            std::snprintf(statusBuf, sizeof(statusBuf), "Play a note...");
        text(cx, barY + 40.0f, statusBuf, nullptr);

        fontSize(12.0f);
        fillColor(kColorTextMuted.withAlpha(0.6f));
        text(cx, barY + 66.0f, "Click anywhere (or press Esc) to close", nullptr);
    }

    void drawPalette(float height)
    {
        const float paletteTop = kTopBarHeight + kControlBarHeight;

        beginPath();
        rect(0.0f, paletteTop, kPaletteWidth, height - paletteTop);
        fillPaint(linearGradient(0.0f, paletteTop, 0.0f, height, Color(28, 28, 34), Color(20, 20, 25)));
        fill();
        closePath();
        beginPath();
        rect(kPaletteWidth - 1.0f, paletteTop, 1.0f, height - paletteTop);
        fillColor(Color(255, 255, 255, 0.05f));
        fill();
        closePath();

        for (size_t row = 0; row < kPaletteOrder.size(); ++row)
        {
            const int i = kPaletteOrder[row];
            const float itemY = kRackTop + static_cast<float>(row) * kPaletteItemH;
            const Color& accent = kPedalDefs[i].accent;
            const bool isActive = paramValues[kPedalDefs[i].onParam] > 0.5f;
            const bool isBeingDragged = (i == paletteDraggingPedal && paletteDragMoved);

            if (isActive)
            {
                beginPath();
                rect(0.0f, itemY, kPaletteWidth, kPaletteItemH);
                fillPaint(radialGradient(24.0f, itemY + kPaletteItemH * 0.5f, 2.0f, 30.0f,
                                          accent.withAlpha(0.25f), accent.withAlpha(0.0f)));
                fill();
                closePath();

                beginPath();
                roundedRect(6.0f, itemY + 3.0f, kPaletteWidth - 12.0f, kPaletteItemH - 6.0f, 6.0f);
                fillPaint(linearGradient(6.0f, itemY + 3.0f, 6.0f, itemY + kPaletteItemH - 3.0f,
                                          Color(accent, Color(255, 255, 255), 0.22f), accent));
                fill();
                closePath();
            }
            else if (lastMouseX >= 0.0f && lastMouseX < kPaletteWidth && lastMouseY >= itemY && lastMouseY < itemY + kPaletteItemH)
            {
                beginPath();
                roundedRect(6.0f, itemY + 3.0f, kPaletteWidth - 12.0f, kPaletteItemH - 6.0f, 6.0f);
                fillColor(Color(255, 255, 255, 0.05f));
                fill();
                closePath();
            }

            beginPath();
            circle(24.0f, itemY + kPaletteItemH * 0.5f, 5.0f);
            fillColor(isActive ? Color(accent, Color(255, 255, 255), 0.3f) : kColorOff);
            fill();
            closePath();

            fontSize(14.0f);
            fillColor(isBeingDragged ? kColorTextMuted : (isActive ? kColorTextDark : kColorTextMuted));
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(42.0f, itemY + kPaletteItemH * 0.5f, kPedalDefs[i].name, nullptr);
        }

        fontSize(10.5f);
        fillColor(kColorTextMuted.withAlpha(0.85f));
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(12.0f, height - 22.0f, "Click or drag a pedal into the rack", nullptr);
    }

    void drawPaletteDragGhost()
    {
        if (paletteDraggingPedal < 0 || !paletteDragMoved)
            return;

        const PedalDef& def = kPedalDefs[paletteDraggingPedal];
        const float ghostW = 130.0f, ghostH = 52.0f;
        const float gx = paletteDragCurrentX - ghostW * 0.5f;
        const float gy = paletteDragCurrentY - ghostH * 0.5f;

        beginPath();
        rect(gx - 16.0f, gy - 10.0f, ghostW + 32.0f, ghostH + 32.0f);
        fillPaint(boxGradient(gx, gy + 4.0f, ghostW, ghostH, 8.0f, 12.0f, Color(0, 0, 0, 0.5f), Color(0, 0, 0, 0.0f)));
        fill();
        closePath();

        beginPath();
        roundedRect(gx, gy, ghostW, ghostH, 8.0f);
        fillPaint(linearGradient(gx, gy, gx, gy + ghostH,
                                  Color(def.accent, Color(255, 255, 255), 0.2f).withAlpha(0.92f),
                                  def.accent.withAlpha(0.92f)));
        fill();
        closePath();

        fontSize(14.0f);
        fillColor(kColorTextDark);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(gx + ghostW * 0.5f, gy + ghostH * 0.5f, def.name, nullptr);
    }

    void drawRack(float width, float height)
    {
        const float moduleW = width - kModuleLeft - 20.0f;
        drawInputPanel(moduleW);

        const std::vector<int> order = getActiveOrder();
        const float moduleH = kModuleHeaderH + kKnobAreaH;
        float y = kPedalRackTop + scrollOffset;

        save();
        // Clipped to start below the fixed Input panel above, so a
        // scrolled-up pedal card can't visually draw over it.
        scissor(kModuleLeft - 10.0f, kPedalRackTop, moduleW + 20.0f, height - kPedalRackTop);

        for (int pedalIndex : order)
        {
            // The dragged card is drawn separately, floating on top, after
            // the rest of the stack - and skipped here without advancing y,
            // so the other cards close up around wherever it would land.
            if (pedalIndex == draggingModuleIndex)
                continue;

            drawModuleCard(pedalIndex, y, moduleW);
            y += moduleH + kModuleGap;
        }

        restore();

        if (draggingModuleIndex >= 0)
            drawModuleCard(draggingModuleIndex, dragModuleCurrentY, moduleW);

        const float contentHeight = totalContentHeight();
        const float visibleHeight = height - kPedalRackTop;
        if (contentHeight > visibleHeight)
        {
            const float trackH = visibleHeight - 10.0f;
            const float thumbH = std::max(30.0f, trackH * (visibleHeight / contentHeight));
            const float scrollFrac = (-scrollOffset) / std::max(1.0f, contentHeight - visibleHeight);
            const float thumbY = kPedalRackTop + 5.0f + scrollFrac * (trackH - thumbH);

            beginPath();
            roundedRect(width - 7.0f, kPedalRackTop + 5.0f, 4.0f, trackH, 2.0f);
            fillColor(Color(255, 255, 255, 0.04f));
            fill();
            closePath();

            beginPath();
            roundedRect(width - 7.0f, thumbY, 4.0f, thumbH, 2.0f);
            fillColor(kColorScrollbar);
            fill();
            closePath();
        }
    }

    // The fixed Input panel - a Gain trim knob plus a peak meter, always
    // drawn at kRackTop and never part of the scrollable/reorderable pedal
    // rack below it (see kInputPedalIndex's comment). Visually it borrows
    // the same card chrome as drawModuleCard (so it reads as part of the
    // same rack) but skips everything that assumes a toggleable/
    // reorderable pedal: no on/off switch, no bypass chip, no remove
    // button, no drag handle.
    void drawInputPanel(float moduleW)
    {
        const float y = kRackTop;
        const float moduleH = kInputPanelH;

        beginPath();
        rect(kModuleLeft - 30.0f, y - 15.0f, moduleW + 60.0f, moduleH + 60.0f);
        fillPaint(boxGradient(kModuleLeft, y + 5.0f, moduleW, moduleH, 10.0f, 14.0f,
                               Color(0, 0, 0, 0.5f), Color(0, 0, 0, 0.0f)));
        fill();
        closePath();

        beginPath();
        roundedRect(kModuleLeft, y, moduleW, moduleH, 10.0f);
        fillPaint(linearGradient(kModuleLeft, y, kModuleLeft, y + moduleH,
                                  Color(kColorPanel, Color(255, 255, 255), 0.05f),
                                  Color(kColorPanel, Color(0, 0, 0), 0.2f)));
        fill();
        closePath();
        beginPath();
        roundedRect(kModuleLeft, y, moduleW, moduleH, 10.0f);
        strokeWidth(1.0f);
        strokeColor(Color(255, 255, 255, 0.07f));
        stroke();
        closePath();

        const Paint headerGrad = linearGradient(kModuleLeft, y, kModuleLeft, y + kModuleHeaderH,
                                                  Color(kColorInputAccent, Color(255, 255, 255), 0.3f),
                                                  Color(kColorInputAccent, Color(0, 0, 0), 0.1f));
        beginPath();
        roundedRect(kModuleLeft, y, moduleW, kModuleHeaderH, 10.0f);
        fillPaint(headerGrad);
        fill();
        closePath();
        beginPath();
        rect(kModuleLeft, y + kModuleHeaderH * 0.5f, moduleW, kModuleHeaderH * 0.5f);
        fillPaint(headerGrad);
        fill();
        closePath();
        beginPath();
        moveTo(kModuleLeft, y + kModuleHeaderH);
        lineTo(kModuleLeft + moduleW, y + kModuleHeaderH);
        strokeWidth(1.0f);
        strokeColor(Color(0, 0, 0, 0.3f));
        stroke();
        closePath();

        fontSize(16.0f);
        fillColor(kColorTextDark);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        textLetterSpacing(0.3f);
        text(kModuleLeft + 16.0f, y + kModuleHeaderH * 0.5f, "Input", nullptr);
        textLetterSpacing(0.0f);

        fontSize(10.5f);
        fillColor(kColorTextDark.withAlpha(0.7f));
        textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
        textLetterSpacing(0.4f);
        text(kModuleLeft + moduleW - 12.0f, y + kModuleHeaderH * 0.5f, "ALWAYS ON", nullptr);
        textLetterSpacing(0.0f);

        const float knobCenterY = y + kModuleHeaderH + kKnobCenterYOffset;
        const float knobX = kModuleLeft + 50.0f;
        const bool isDraggingKnob = (draggingKnobPedal == kInputPedalIndex);
        const bool isEditingKnob = (editingKnobPedal == kInputPedalIndex);
        drawKnob(knobX, knobCenterY, kInputGainKnob, displayValues[kParamInputGain], kColorInputAccent, 1.0f,
                 isDraggingKnob, isEditingKnob);

        drawInputMeter(knobX + kKnobSpacing, knobCenterY, displayValues[kParamInputLevel]);
    }

    // A small peak meter next to the Input panel's Gain knob - green up
    // through yellow into red near 0dBFS, with a dark mask over the unlit
    // portion (same idea as DPF's own Meters example) so it reads as an
    // LED bargraph rather than a plain filled bar. Helps players set Gain
    // so they're not clipping into the Noise Gate/rest of the chain.
    void drawInputMeter(float cx, float centerY, float level)
    {
        const float meterH = kKnobRadius * 2.0f;
        const float x = cx - kMeterW * 0.5f;
        const float y = centerY - meterH * 0.5f;
        const float lit = std::max(0.0f, std::min(1.0f, level));

        fontSize(10.5f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        textLetterSpacing(0.4f);
        text(cx, y - 6.0f, "Level", nullptr);
        textLetterSpacing(0.0f);

        beginPath();
        roundedRect(x, y, kMeterW, meterH, 3.0f);
        fillColor(Color(0, 0, 0, 0.4f));
        fill();
        closePath();

        beginPath();
        roundedRect(x + 1.5f, y + 1.5f, kMeterW - 3.0f, meterH - 3.0f, 2.0f);
        fillPaint(linearGradient(x, y + meterH, x, y,
                                  kColorOn, Color(230, 70, 70)));
        fill();
        closePath();

        // Mask the unlit portion, top-down, in the panel's own background
        // shade so it blends with the card instead of reading as pure black.
        const float unlitH = (meterH - 3.0f) * (1.0f - lit);
        if (unlitH > 0.5f)
        {
            beginPath();
            rect(x + 1.5f, y + 1.5f, kMeterW - 3.0f, unlitH);
            fillColor(Color(kColorPanel, Color(0, 0, 0), 0.35f));
            fill();
            closePath();
        }

        beginPath();
        roundedRect(x, y, kMeterW, meterH, 3.0f);
        strokeWidth(1.0f);
        strokeColor(Color(0, 0, 0, 0.4f));
        stroke();
        closePath();

        // Same LCD-readout language as a knob's value chip below it.
        char buf[16];
        if (level > 0.0003f)
            std::snprintf(buf, sizeof(buf), "%.1fdB", 20.0f * std::log10(level));
        else
            std::snprintf(buf, sizeof(buf), "-inf");

        const bool hot = level >= 0.98f;
        const float chipY = y + meterH + kValueChipGap;
        beginPath();
        roundedRect(x - (kValueChipW - kMeterW) * 0.5f, chipY, kValueChipW, kValueChipH, 4.0f);
        fillColor(kColorTooltipBg);
        fill();
        closePath();
        beginPath();
        roundedRect(x - (kValueChipW - kMeterW) * 0.5f, chipY, kValueChipW, kValueChipH, 4.0f);
        strokeWidth(1.0f);
        strokeColor((hot ? Color(230, 90, 90) : Color(70, 70, 82)).withAlpha(hot ? 0.9f : 0.5f));
        stroke();
        closePath();

        fontSize(11.5f);
        fillColor(hot ? Color(230, 120, 120) : kColorInputAccent);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(cx, chipY + kValueChipH * 0.5f, buf, nullptr);
    }

    // Draws one pedal card at the given top-Y. Used both for the normal
    // stacked rack and (with a live, cursor-following y) for whichever card
    // is currently being dragged to reorder.
    void drawModuleCard(int pedalIndex, float y, float moduleW)
    {
            const PedalDef& def = kPedalDefs[pedalIndex];
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const bool isDraggingThis = (pedalIndex == draggingModuleIndex);
            const bool isBypassed = (def.bypassParam >= 0) && (paramValues[def.bypassParam] > 0.5f);
            const float alpha = moduleAlpha[pedalIndex];
            const float drawY = y + (1.0f - alpha) * 16.0f; // slide in/out

            // While bypassed, the card stays fully opaque and legible - only
            // its accent color desaturates to a neutral metal tone (like a
            // stomp box with its LED dark) and the knob area dims slightly,
            // rather than the whole card fading into a washed-out ghost of
            // itself. Much clearer at a glance than a flat opacity cut.
            const Color cardAccent = isBypassed ? def.accent.asGrayscale() : def.accent;
            const float knobAlpha = isBypassed ? alpha * 0.72f : alpha;

            // Drop shadow, for a bit of depth against the rack background -
            // bigger and darker while being dragged, so the card reads as
            // physically "picked up" off the rack rather than just outlined.
            const float shadowOffset = isDraggingThis ? 12.0f : 5.0f;
            const float shadowFeather = isDraggingThis ? 26.0f : 14.0f;
            const float shadowAlpha = isDraggingThis ? 0.75f : 0.5f;
            beginPath();
            rect(kModuleLeft - 30.0f, drawY - 15.0f, moduleW + 60.0f, moduleH + 60.0f);
            fillPaint(boxGradient(kModuleLeft, drawY + shadowOffset, moduleW, moduleH, 10.0f, shadowFeather,
                                   Color(0, 0, 0, shadowAlpha * alpha), Color(0, 0, 0, 0.0f)));
            fill();
            closePath();

            // Body - a subtle brushed-metal gradient rather than a flat fill.
            beginPath();
            roundedRect(kModuleLeft, drawY, moduleW, moduleH, 10.0f);
            fillPaint(linearGradient(kModuleLeft, drawY, kModuleLeft, drawY + moduleH,
                                      Color(kColorPanel, Color(255, 255, 255), 0.05f).withAlpha(alpha),
                                      Color(kColorPanel, Color(0, 0, 0), 0.2f).withAlpha(alpha)));
            fill();
            closePath();
            beginPath();
            roundedRect(kModuleLeft, drawY, moduleW, moduleH, 10.0f);
            strokeWidth(1.0f);
            strokeColor(Color(255, 255, 255, 0.07f * alpha));
            stroke();
            closePath();

            if (isDraggingThis)
            {
                beginPath();
                roundedRect(kModuleLeft, drawY, moduleW, moduleH, 10.0f);
                strokeColor(cardAccent.withAlpha(alpha));
                strokeWidth(2.5f);
                stroke();
                closePath();
            }

            // Header - glossy top plate in the pedal's accent color (grayed
            // out via cardAccent while bypassed).
            const Paint headerGrad = linearGradient(kModuleLeft, drawY, kModuleLeft, drawY + kModuleHeaderH,
                                                      Color(cardAccent, Color(255, 255, 255), 0.3f).withAlpha(alpha),
                                                      Color(cardAccent, Color(0, 0, 0), 0.1f).withAlpha(alpha));
            beginPath();
            roundedRect(kModuleLeft, drawY, moduleW, kModuleHeaderH, 10.0f);
            fillPaint(headerGrad);
            fill();
            closePath();
            beginPath();
            rect(kModuleLeft, drawY + kModuleHeaderH * 0.5f, moduleW, kModuleHeaderH * 0.5f);
            fillPaint(headerGrad);
            fill();
            closePath();
            beginPath();
            moveTo(kModuleLeft, drawY + kModuleHeaderH);
            lineTo(kModuleLeft + moduleW, drawY + kModuleHeaderH);
            strokeWidth(1.0f);
            strokeColor(Color(0, 0, 0, 0.3f * alpha));
            stroke();
            closePath();

            fontSize(16.0f);
            fillColor(kColorTextDark.withAlpha(alpha));
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            textLetterSpacing(0.3f);
            text(kModuleLeft + (pedalIndex == kAmpPedalIndex ? 16.0f : 44.0f), drawY + kModuleHeaderH * 0.5f, def.name, nullptr);
            textLetterSpacing(0.0f);

            if (pedalIndex != kAmpPedalIndex)
            {
                const bool isOn = !isBypassed;
                const float sx = kModuleLeft + 12.0f;
                const float sy = drawY + (kModuleHeaderH - kSwitchSize) * 0.5f;
                const float trackW = kSwitchSize * 1.6f;

                // Soft halo bleeding off the switch while lit, like a real
                // LED - the main "this is active" cue, so off (no halo) is
                // unambiguous without needing to dim anything else.
                if (isOn)
                {
                    beginPath();
                    rect(sx - 14.0f, sy - 14.0f, trackW + 28.0f, kSwitchSize + 28.0f);
                    fillPaint(radialGradient(sx + trackW * 0.5f, sy + kSwitchSize * 0.5f, 2.0f, 22.0f,
                                              kColorOn.withAlpha(0.45f * alpha), kColorOn.withAlpha(0.0f)));
                    fill();
                    closePath();
                }

                // Contact shadow under the track, matching the knobs' depth
                // treatment so the switch feels like the same hardware family.
                beginPath();
                roundedRect(sx, sy + 1.5f, trackW, kSwitchSize, kSwitchSize * 0.5f);
                fillColor(Color(0, 0, 0, 0.35f * alpha));
                fill();
                closePath();

                // Track - glossy when lit, a duller inset groove when not,
                // so "off" reads as an unpowered switch rather than a faded
                // copy of the "on" one.
                beginPath();
                roundedRect(sx, sy, trackW, kSwitchSize, kSwitchSize * 0.5f);
                fillPaint(linearGradient(sx, sy, sx, sy + kSwitchSize,
                                          Color(isOn ? kColorOn : kColorOff, Color(255, 255, 255), isOn ? 0.3f : 0.1f).withAlpha(alpha),
                                          Color(isOn ? kColorOn : kColorOff, Color(0, 0, 0), isOn ? 0.05f : 0.3f).withAlpha(alpha)));
                fill();
                closePath();
                beginPath();
                roundedRect(sx, sy, trackW, kSwitchSize, kSwitchSize * 0.5f);
                strokeWidth(1.0f);
                strokeColor(Color(0, 0, 0, 0.4f * alpha));
                stroke();
                closePath();

                // Thumb - a small glossy puck (radial highlight + its own
                // contact shadow) instead of a flat disc, echoing the same
                // glossy-metal language as the knob faces.
                const float thumbCx = isOn ? sx + trackW - kSwitchSize * 0.5f : sx + kSwitchSize * 0.5f;
                const float thumbCy = sy + kSwitchSize * 0.5f;
                beginPath();
                circle(thumbCx, thumbCy + 0.8f, kSwitchSize * 0.42f);
                fillColor(Color(0, 0, 0, 0.3f * alpha));
                fill();
                closePath();
                beginPath();
                circle(thumbCx, thumbCy, kSwitchSize * 0.4f);
                fillPaint(radialGradient(thumbCx - kSwitchSize * 0.12f, thumbCy - kSwitchSize * 0.12f, 0.5f, kSwitchSize * 0.5f,
                                          Color(255, 255, 255, alpha),
                                          Color(isOn ? 232 : 200, isOn ? 232 : 200, isOn ? 238 : 208, alpha)));
                fill();
                closePath();
                beginPath();
                circle(thumbCx, thumbCy, kSwitchSize * 0.4f);
                strokeWidth(0.75f);
                strokeColor(Color(0, 0, 0, 0.25f * alpha));
                stroke();
                closePath();

                // A neutral dark chip rather than a permanently red one - a
                // fixed bright red clashed with pedals whose own accent is
                // already in the red family (e.g. Screamer). It only turns
                // red, as a clear destructive-action cue, on hover.
                const float rx = kModuleLeft + moduleW - kRemoveSize - 12.0f;
                const float ry = drawY + (kModuleHeaderH - kRemoveSize) * 0.5f;
                const bool removeHovered = (lastMouseX >= rx - 4.0f && lastMouseX <= rx + kRemoveSize + 4.0f &&
                                             lastMouseY >= ry - 4.0f && lastMouseY <= ry + kRemoveSize + 4.0f);

                // Bypass status chip, sitting between the name and the
                // remove button - a proper pill badge (dark fill, faint
                // border, unlit dot echoing the switch's own LED language)
                // instead of bare text crammed into the header.
                if (isBypassed)
                {
                    const float chipH = kSwitchSize;
                    const float chipW = 78.0f;
                    const float chipX = rx - chipW - 10.0f;
                    const float chipY = drawY + (kModuleHeaderH - chipH) * 0.5f;

                    beginPath();
                    roundedRect(chipX, chipY, chipW, chipH, chipH * 0.5f);
                    fillColor(Color(0, 0, 0, 0.3f * alpha));
                    fill();
                    closePath();
                    beginPath();
                    roundedRect(chipX, chipY, chipW, chipH, chipH * 0.5f);
                    strokeWidth(1.0f);
                    strokeColor(Color(255, 255, 255, 0.12f * alpha));
                    stroke();
                    closePath();

                    beginPath();
                    circle(chipX + 14.0f, chipY + chipH * 0.5f, 3.0f);
                    fillColor(Color(255, 255, 255, 0.25f * alpha));
                    fill();
                    closePath();

                    fontSize(9.5f);
                    fillColor(kColorTextMuted.withAlpha(alpha));
                    textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                    textLetterSpacing(0.5f);
                    text(chipX + 23.0f, chipY + chipH * 0.5f + 0.5f, "BYPASS", nullptr);
                    textLetterSpacing(0.0f);
                }

                beginPath();
                circle(rx + kRemoveSize * 0.5f, ry + kRemoveSize * 0.5f, kRemoveSize * 0.5f);
                fillPaint(removeHovered
                    ? radialGradient(rx + kRemoveSize * 0.35f, ry + kRemoveSize * 0.35f, 0.5f, kRemoveSize * 0.7f,
                                      Color(kColorRemove, Color(255, 255, 255), 0.3f).withAlpha(alpha),
                                      kColorRemove.withAlpha(alpha))
                    : radialGradient(rx + kRemoveSize * 0.35f, ry + kRemoveSize * 0.35f, 0.5f, kRemoveSize * 0.7f,
                                      Color(0, 0, 0, 0.35f * alpha), Color(0, 0, 0, 0.55f * alpha)));
                fill();
                closePath();
                beginPath();
                circle(rx + kRemoveSize * 0.5f, ry + kRemoveSize * 0.5f, kRemoveSize * 0.5f);
                strokeWidth(1.0f);
                strokeColor(Color(255, 255, 255, (removeHovered ? 0.4f : 0.22f) * alpha));
                stroke();
                closePath();
                fontSize(11.0f);
                fillColor(Color(255, 255, 255, alpha * (removeHovered ? 1.0f : 0.8f)));
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                text(rx + kRemoveSize * 0.5f, ry + kRemoveSize * 0.5f + 1.0f, "x", nullptr);
            }
            else
            {
                fontSize(10.5f);
                fillColor(kColorTextDark.withAlpha(alpha));
                textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
                textLetterSpacing(0.4f);
                text(kModuleLeft + moduleW - 12.0f, drawY + kModuleHeaderH * 0.5f, "ALWAYS ON", nullptr);
                textLetterSpacing(0.0f);
            }

            const float knobCenterY = drawY + kModuleHeaderH + kKnobCenterYOffset;
            float knobX = kModuleLeft + 50.0f;
            for (size_t k = 0; k < def.knobs.size(); ++k)
            {
                const KnobDef& knob = def.knobs[k];
                const bool isDraggingKnob = (pedalIndex == draggingKnobPedal && static_cast<int>(k) == draggingKnobIndex);
                const bool isEditingKnob = (pedalIndex == editingKnobPedal && static_cast<int>(k) == editingKnobIndex);
                drawKnob(knobX, knobCenterY, knob, displayValues[knob.paramIndex], cardAccent, knobAlpha, isDraggingKnob, isEditingKnob);
                knobX += kKnobSpacing;
            }

            if (def.hasFileLoader)
                drawFileLoaderButton(knobX, knobCenterY, def, knobAlpha);
    }

    // The "Load..." button on a file-loader pedal card (currently just
    // Cabinet) - shows the loaded file's name once one has been picked,
    // or an invitation to load one otherwise.
    void drawFileLoaderButton(float x, float centerY, const PedalDef& def, float alpha)
    {
        const float y = centerY - kFileLoaderH * 0.5f;

        std::string label = "Load IR File...";
        bool loaded = false;
        const auto it = stateValues.find(def.stateKey != nullptr ? def.stateKey : "");
        if (it != stateValues.end() && !it->second.empty())
        {
            label = std::filesystem::path(it->second).filename().string();
            loaded = true;
        }

        beginPath();
        roundedRect(x, y, kFileLoaderW, kFileLoaderH, 5.0f);
        fillPaint(linearGradient(x, y, x, y + kFileLoaderH,
                                  Color(kColorButton, Color(255, 255, 255), 0.08f).withAlpha(alpha),
                                  Color(kColorButton, Color(0, 0, 0), 0.15f).withAlpha(alpha)));
        fill();
        closePath();
        beginPath();
        roundedRect(x, y, kFileLoaderW, kFileLoaderH, 5.0f);
        strokeWidth(1.0f);
        strokeColor((loaded ? def.accent : Color(255, 255, 255, 0.15f)).withAlpha(alpha));
        stroke();
        closePath();

        fontSize(11.5f);
        fillColor((loaded ? kColorTextPrimary : kColorTextMuted).withAlpha(alpha));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(x + kFileLoaderW * 0.5f, y + kFileLoaderH * 0.5f, label.c_str(), nullptr);
    }

    void drawKnob(float cx, float cy, const KnobDef& knob, float value, const Color& accent, float alpha, bool isDragging, bool isEditing)
    {
        const float t = (value - knob.minVal) / (knob.maxVal - knob.minVal);
        const float startAngle = 0.75f * static_cast<float>(M_PI);
        const float endAngle   = 2.25f * static_cast<float>(M_PI);
        const float valueAngle = startAngle + t * (endAngle - startAngle);

        // Soft contact shadow for a bit of depth under the pot.
        beginPath();
        circle(cx, cy + 3.0f, kKnobRadius + 9.0f);
        fillPaint(radialGradient(cx, cy + 3.0f, kKnobRadius * 0.5f, kKnobRadius + 9.0f,
                                  Color(0, 0, 0, 0.45f * alpha), Color(0, 0, 0, 0.0f)));
        fill();
        closePath();

        // Detent ticks around the travel arc, like a real potentiometer.
        for (int i = 0; i < 11; ++i)
        {
            const float a = startAngle + (static_cast<float>(i) / 10.0f) * (endAngle - startAngle);
            beginPath();
            moveTo(cx + std::cos(a) * (kKnobRadius + 3.0f), cy + std::sin(a) * (kKnobRadius + 3.0f));
            lineTo(cx + std::cos(a) * (kKnobRadius + 6.0f), cy + std::sin(a) * (kKnobRadius + 6.0f));
            strokeWidth(1.4f);
            strokeColor(kColorTextMuted.withAlpha(0.35f * alpha));
            stroke();
            closePath();
        }

        // Track groove.
        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, endAngle, CW);
        strokeColor(kColorKnobTrack.withAlpha(alpha));
        strokeWidth(5.0f);
        stroke();
        closePath();

        // Value arc, with a faint glow behind it in the pedal's accent color.
        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, valueAngle, CW);
        strokeColor(accent.withAlpha(0.35f * alpha));
        strokeWidth(9.0f);
        stroke();
        closePath();
        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, valueAngle, CW);
        strokeColor(accent.withAlpha(alpha));
        strokeWidth(5.0f);
        stroke();
        closePath();

        // Glossy metal face.
        beginPath();
        circle(cx, cy, kKnobRadius - 7.0f);
        fillPaint(radialGradient(cx - kKnobRadius * 0.35f, cy - kKnobRadius * 0.4f, 1.0f, kKnobRadius * 1.1f,
                                  Color(kColorPanel, Color(255, 255, 255), 0.22f).withAlpha(alpha),
                                  Color(kColorPanel, Color(0, 0, 0), 0.25f).withAlpha(alpha)));
        fill();
        closePath();
        beginPath();
        circle(cx, cy, kKnobRadius - 7.0f);
        strokeWidth(1.0f);
        strokeColor(Color(0, 0, 0, 0.35f * alpha));
        stroke();
        closePath();

        // Pointer, with a small witness-mark dot at the tip.
        const float pointerInner = kKnobRadius * 0.35f;
        const float pointerOuter = kKnobRadius - 9.0f;
        beginPath();
        moveTo(cx + std::cos(valueAngle) * pointerInner, cy + std::sin(valueAngle) * pointerInner);
        lineTo(cx + std::cos(valueAngle) * pointerOuter, cy + std::sin(valueAngle) * pointerOuter);
        strokeColor(Color(255, 255, 255, 0.9f * alpha));
        strokeWidth(2.2f);
        lineCap(ROUND);
        stroke();
        lineCap(BUTT);
        closePath();
        beginPath();
        circle(cx + std::cos(valueAngle) * pointerOuter, cy + std::sin(valueAngle) * pointerOuter, 2.2f);
        fillColor(Color(255, 255, 255, 0.9f * alpha));
        fill();
        closePath();

        // Label above the knob.
        fontSize(10.5f);
        fillColor(kColorTextMuted.withAlpha(alpha));
        textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
        textLetterSpacing(0.4f);
        text(cx, cy - kKnobRadius - 6.0f, knob.label, nullptr);
        textLetterSpacing(0.0f);

        // Value readout - always visible (not just while dragging), brightens
        // in the pedal's accent color while actively being adjusted, and
        // turns into a live text field while a manual value is being typed.
        std::string valueText;
        if (isEditing)
        {
            valueText = knobEditBuffer + "|";
        }
        else
        {
            char buf[32];
            formatKnobValue(knob, value, buf, sizeof(buf));
            valueText = buf;
        }

        const bool highlighted = isDragging || isEditing;
        const float chipY = cy + kKnobRadius + kValueChipGap;
        const float chipX = cx - kValueChipW * 0.5f;

        beginPath();
        roundedRect(chipX, chipY, kValueChipW, kValueChipH, 4.0f);
        fillColor((isEditing ? kColorBackground : kColorTooltipBg).withAlpha(alpha));
        fill();
        closePath();
        beginPath();
        roundedRect(chipX, chipY, kValueChipW, kValueChipH, 4.0f);
        strokeWidth(1.0f);
        strokeColor((highlighted ? accent : Color(70, 70, 82)).withAlpha((highlighted ? 0.9f : 0.5f) * alpha));
        stroke();
        closePath();

        fontSize(11.5f);
        fillColor((isEditing ? kColorTextPrimary : (isDragging ? Color(accent, Color(255, 255, 255), 0.4f) : accent)).withAlpha(alpha));
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(cx, chipY + kValueChipH * 0.5f, valueText.c_str(), nullptr);
    }

    float paramValues[kParamCount];
    float displayValues[kParamCount];
    float moduleAlpha[kPedalDefCount];
    bool moduleRemoving[kPedalDefCount];
    std::chrono::steady_clock::time_point lastFrameTime;

    float lastMouseX = -1.0f;
    float lastMouseY = -1.0f;

    int draggingModuleIndex = -1;
    float dragModuleGrabOffsetY = 0.0f;
    float dragModuleCurrentY = 0.0f;

    int draggingKnobPedal = -1;
    int draggingKnobIndex = -1;
    float draggingKnobStartY = 0.0f;
    float draggingKnobStartValue = 0.0f;

    int editingKnobPedal = -1;
    int editingKnobIndex = -1;
    std::string knobEditBuffer;

    int paletteDraggingPedal = -1;
    bool paletteDragMoved = false;
    float paletteDragStartX = 0.0f, paletteDragStartY = 0.0f;
    float paletteDragCurrentX = 0.0f, paletteDragCurrentY = 0.0f;

    float scrollOffset = 0.0f;

    bool presetDropdownOpen = false;
    float dropdownAnim = 0.0f;
    std::string activePresetName = "Chimey Clean";

    bool savingPreset = false;
    std::string nameInputBuffer;

    float saveToastAlpha = 0.0f;
    std::string saveToastText;

    std::vector<CustomPreset> customPresets;

    // A/B comparison - two full parameter snapshots the user can flip
    // between instantly (see switchABSlot()), independent of the preset
    // system above (switching A/B doesn't touch activePresetName/undo any
    // preset-related state, it's purely "what does the chain sound like
    // right now" bookkeeping). UI-only: there's no DSP or parameter
    // involved, applyValuesArray() (already used for presets) is what
    // actually pushes a snapshot's values back into the chain.
    char abActiveSlot = 'A';
    bool abHasSnapshotA = false, abHasSnapshotB = false;
    float abSnapshotA[kParamCount] = {};
    float abSnapshotB[kParamCount] = {};

    // Local mirror of DPF State key/value pairs (currently just the
    // Cabinet block's loaded IR path), kept in sync via stateChanged().
    std::map<std::string, std::string> stateValues;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainUI)
};

UI* createUI()
{
    return new ChainUI();
}

END_NAMESPACE_DISTRHO
