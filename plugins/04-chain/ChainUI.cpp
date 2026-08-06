/*
 * AmpForge - Main Chain UI (Phase 9, Step 4: bypass, live values, presets, animation)
 *
 * Changes from the previous version, based on feedback:
 *   - The on/off switch now toggles a real *Bypass* parameter, separate
 *     from the pedal's presence in the rack (the *On* parameter). The
 *     card stays visible and dims when bypassed, instead of vanishing.
 *   - Dragging a knob shows a floating tooltip with the live numeric
 *     value (e.g. "-6.2 dB") right next to it.
 *   - A preset bar under the title lets you jump straight to any of
 *     the 6 factory presets without leaving this window.
 *   - Values now animate (ease) toward their target instead of jumping
 *     instantly - most noticeable when a preset changes many knobs at
 *     once, or when the host automates a parameter.
 */

#include "DistrhoUI.hpp"
#include "ChainParameters.hpp"
#include "ChainPresets.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

// --- Base style ---
static const Color kColorBackground(16, 16, 20);
static const Color kColorPanel(26, 26, 32);
static const Color kColorHeaderBar(24, 24, 29);
static const Color kColorKnobTrack(50, 50, 58);
static const Color kColorOn(90, 220, 140);
static const Color kColorOff(80, 80, 90);
static const Color kColorRemove(230, 90, 100);
static const Color kColorTextPrimary(235, 235, 240);
static const Color kColorTextMuted(150, 150, 160);
static const Color kColorTextDark(20, 20, 24);
static const Color kColorTooltipBg(10, 10, 12);
static const Color kColorPresetBar(22, 22, 27);
static const Color kColorPresetActive(90, 170, 255);

// --- Layout constants ---
static constexpr float kTopBarHeight    = 52.0f;
static constexpr float kPresetBarHeight = 40.0f;
static constexpr float kPaletteWidth    = 190.0f;
static constexpr float kPaletteItemH    = 42.0f;
static constexpr float kRackTop         = kTopBarHeight + kPresetBarHeight + 20.0f;
static constexpr float kModuleLeft      = kPaletteWidth + 20.0f;
static constexpr float kModuleHeaderH   = 42.0f;
static constexpr float kKnobAreaH       = 92.0f;
static constexpr float kModuleGap       = 12.0f;
static constexpr float kKnobRadius      = 24.0f;
static constexpr float kKnobSpacing     = 78.0f;
static constexpr float kSwitchSize      = 18.0f;
static constexpr float kRemoveSize      = 16.0f;
static constexpr float kKnobDragSensitivity = 220.0f;
static constexpr float kAnimEaseFactor  = 0.28f; // higher = snappier, lower = more glide
static constexpr float kAnimSnapEpsilon = 0.001f;

struct KnobDef
{
    const char* label;
    int paramIndex;
    float minVal;
    float maxVal;
    const char* unit;   // shown after the number, e.g. "dB", "ms", "Hz", "x"
    int decimals;       // digits after the decimal point
    bool asPercent;      // if true, display value*100 with a % suffix instead of unit/decimals
};

struct PedalDef
{
    const char* name;
    int onParam;
    int bypassParam; // -1 for Amp (can't be bypassed - it's the core of the chain)
    int positionParam;
    Color accent;
    std::vector<KnobDef> knobs;
};

// clang-format off
static const PedalDef kPedalDefs[] =
{
    { "Noise Gate", kParamGateOn,     kParamGateBypass,     kParamGatePosition,  Color(130, 150, 210), {
        { "Thresh",  kParamGateThreshold, -80.0f, 0.0f,    "dB", 1, false },
        { "Attack",  kParamGateAttack,      0.5f, 50.0f,   "ms", 1, false },
        { "Release", kParamGateRelease,    10.0f, 1000.0f, "ms", 0, false },
    }},
    { "Compressor", kParamCompOn,     kParamCompBypass,     kParamCompPosition,  Color(175, 120, 225), {
        { "Thresh",  kParamCompThreshold, -60.0f, 0.0f,    "dB", 1, false },
        { "Ratio",   kParamCompRatio,       1.0f, 20.0f,   ":1", 1, false },
        { "Attack",  kParamCompAttack,      0.5f, 100.0f,  "ms", 1, false },
        { "Release", kParamCompRelease,    10.0f, 1000.0f, "ms", 0, false },
        { "Makeup",  kParamCompMakeup,      0.0f, 24.0f,   "dB", 1, false },
    }},
    { "Wah",        kParamWahOn,      kParamWahBypass,      kParamWahPosition,   Color(235, 155, 60), {
        { "Pedal",   kParamWahPedal,  0.0f, 1.0f,  "", 0, true },
        { "Q",       kParamWahQ,      0.5f, 10.0f, "", 1, false },
    }},
    { "Screamer",   kParamScreamerOn, kParamScreamerBypass, kParamScreamerPosition, Color(235, 95, 70), {
        { "Drive",   kParamScreamerDrive,  1.0f, 20.0f,  "x", 1, false },
        { "Tone",    kParamScreamerTone,   0.05f, 1.0f,  "",  0, true },
        { "Level",   kParamScreamerLevel, -24.0f, 12.0f, "dB", 1, false },
    }},
    { "Amp",        -1,               -1,                   kParamAmpPosition,   Color(90, 170, 255), {
        { "Drive",   kParamAmpDrive,    0.0f, 36.0f,  "dB", 1, false },
        { "Bass",    kParamAmpBass,   -12.0f, 12.0f,  "dB", 1, false },
        { "Mid",     kParamAmpMid,    -12.0f, 12.0f,  "dB", 1, false },
        { "Treble",  kParamAmpTreble, -12.0f, 12.0f,  "dB", 1, false },
        { "Volume",  kParamAmpVolume, -24.0f, 12.0f,  "dB", 1, false },
    }},
    { "Chorus",     kParamChorusOn,   kParamChorusBypass,   kParamChorusPosition, Color(70, 205, 195), {
        { "Rate",    kParamChorusRate,   0.05f, 5.0f,  "Hz", 2, false },
        { "Depth",   kParamChorusDepth,  0.5f, 20.0f,  "ms", 1, false },
        { "Mix",     kParamChorusMix,    0.0f, 1.0f,   "",   0, true },
    }},
    { "Phaser",     kParamPhaserOn,   kParamPhaserBypass,   kParamPhaserPosition, Color(185, 115, 235), {
        { "Rate",    kParamPhaserRate,  0.05f, 5.0f, "Hz", 2, false },
        { "Depth",   kParamPhaserDepth, 0.0f, 1.0f,  "",   0, true },
        { "Mix",     kParamPhaserMix,   0.0f, 1.0f,  "",   0, true },
    }},
    { "Tremolo",    kParamTremoloOn,  kParamTremoloBypass,  kParamTremoloPosition, Color(235, 205, 60), {
        { "Rate",    kParamTremoloRate,  0.5f, 15.0f, "Hz", 1, false },
        { "Depth",   kParamTremoloDepth, 0.0f, 1.0f,  "",   0, true },
    }},
    { "Delay",      kParamDelayOn,    kParamDelayBypass,    kParamDelayPosition,  Color(95, 225, 145), {
        { "Time",     kParamDelayTime,      10.0f, 1500.0f, "ms", 0, false },
        { "Feedback", kParamDelayFeedback,   0.0f, 0.95f,   "",   0, true },
        { "Mix",      kParamDelayMix,        0.0f, 1.0f,    "",   0, true },
    }},
    { "Reverb",     kParamReverbOn,   kParamReverbBypass,   kParamReverbPosition, Color(115, 125, 235), {
        { "Room",     kParamReverbRoomSize, 0.0f, 1.0f, "", 0, true },
        { "Damping",  kParamReverbDamping,  0.0f, 1.0f, "", 0, true },
        { "Mix",      kParamReverbMix,      0.0f, 1.0f, "", 0, true },
    }},
};
// clang-format on
static constexpr int kPedalDefCount = sizeof(kPedalDefs) / sizeof(kPedalDefs[0]);
static constexpr int kAmpPedalIndex = 4;

class ChainUI : public UI
{
public:
    ChainUI()
        : UI(1000, 720)
    {
        loadSharedResources();
        std::fill(std::begin(paramValues), std::end(paramValues), 0.0f);
        std::fill(std::begin(displayValues), std::end(displayValues), 0.0f);
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
            paramValues[index] = value;
        repaint();
    }

    // Called regularly by the host - this is what drives our value
    // animation. We ease displayValues toward paramValues every tick
    // and repaint while anything is still moving.
    void uiIdle() override
    {
        bool anyChanged = false;
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            const float diff = paramValues[i] - displayValues[i];
            if (std::fabs(diff) > kAnimSnapEpsilon)
            {
                displayValues[i] += diff * kAnimEaseFactor;
                anyChanged = true;
            }
            else if (displayValues[i] != paramValues[i])
            {
                displayValues[i] = paramValues[i];
                anyChanged = true;
            }
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
        fillColor(kColorBackground);
        fill();
        closePath();

        beginPath();
        rect(0.0f, 0.0f, width, kTopBarHeight);
        fillColor(kColorHeaderBar);
        fill();
        closePath();

        fontSize(20.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(20.0f, kTopBarHeight * 0.5f, "AmpForge", nullptr);

        drawPresetBar(width);
        drawPalette(height);
        drawRack(width, height);
        drawKnobTooltip();
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (!ev.press || ev.button != 1)
        {
            if (!ev.press)
            {
                if (draggingModuleIndex >= 0)
                {
                    editParameter(kPedalDefs[draggingModuleIndex].positionParam, false);
                    draggingModuleIndex = -1;
                }
                if (draggingKnobPedal >= 0)
                {
                    editParameter(kPedalDefs[draggingKnobPedal].knobs[draggingKnobIndex].paramIndex, false);
                    draggingKnobPedal = -1;
                    draggingKnobIndex = -1;
                }
            }
            return false;
        }

        const float mx = static_cast<float>(ev.pos.getX());
        const float my = static_cast<float>(ev.pos.getY());

        // --- Preset bar clicks ---
        if (my < kTopBarHeight + kPresetBarHeight && my >= kTopBarHeight)
        {
            const float presetW = 150.0f;
            for (uint32_t i = 0; i < kProgramCount; ++i)
            {
                const float px = 12.0f + i * (presetW + 8.0f);
                if (mx >= px && mx <= px + presetW)
                {
                    applyPreset(i);
                    return true;
                }
            }
            return false;
        }

        // --- Palette clicks ---
        if (mx < kPaletteWidth && my > kTopBarHeight + kPresetBarHeight)
        {
            for (int i = 0; i < kPedalDefCount; ++i)
            {
                if (i == kAmpPedalIndex)
                    continue;
                const float itemY = kRackTop + i * kPaletteItemH;
                if (my >= itemY && my < itemY + kPaletteItemH)
                {
                    togglePedalPresence(i);
                    return true;
                }
            }
            return false;
        }

        // --- Module clicks (knobs / switch / remove / drag handle) ---
        const std::vector<int> order = getActiveOrder();
        float y = kRackTop;

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
                    // Bypass switch (top-left of header)
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

                    // Remove button (top-right of header) - takes it off the board entirely
                    const float rx = kModuleLeft + moduleW - kRemoveSize - 12.0f;
                    const float ry = y + (kModuleHeaderH - kRemoveSize) * 0.5f;
                    if (mx >= rx && mx <= rx + kRemoveSize && my >= ry && my <= ry + kRemoveSize)
                    {
                        togglePedalPresence(pedalIndex);
                        return true;
                    }
                }

                // Knobs
                const float knobCenterY = y + kModuleHeaderH + kKnobAreaH * 0.5f - 8.0f;
                float knobX = kModuleLeft + 50.0f;
                for (size_t k = 0; k < def.knobs.size(); ++k)
                {
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

                // Otherwise: start dragging the whole module to reorder
                draggingModuleIndex = pedalIndex;
                editParameter(def.positionParam, true);
                return true;
            }

            y += moduleH + kModuleGap;
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const float my = static_cast<float>(ev.pos.getY());

        if (draggingKnobPedal >= 0)
        {
            const KnobDef& knob = kPedalDefs[draggingKnobPedal].knobs[draggingKnobIndex];
            const float deltaPixels = draggingKnobStartY - my;
            const float range = knob.maxVal - knob.minVal;
            float newValue = draggingKnobStartValue + (deltaPixels / kKnobDragSensitivity) * range;
            newValue = std::max(knob.minVal, std::min(knob.maxVal, newValue));

            setParameterValue(knob.paramIndex, newValue);
            paramValues[knob.paramIndex] = newValue;
            displayValues[knob.paramIndex] = newValue; // snap instantly while actively dragging
            repaint();
            return true;
        }

        if (draggingModuleIndex >= 0)
        {
            const std::vector<int> order = getActiveOrder();
            float y = kRackTop;
            int hoveredPedal = -1;

            for (int pedalIndex : order)
            {
                const float moduleH = kModuleHeaderH + kKnobAreaH;
                if (my >= y && my < y + moduleH)
                {
                    hoveredPedal = pedalIndex;
                    break;
                }
                y += moduleH + kModuleGap;
            }

            if (hoveredPedal >= 0 && hoveredPedal != draggingModuleIndex)
            {
                const int draggedPosParam = kPedalDefs[draggingModuleIndex].positionParam;
                const int hoveredPosParam = kPedalDefs[hoveredPedal].positionParam;
                const float draggedPos = paramValues[draggedPosParam];
                const float hoveredPos = paramValues[hoveredPosParam];

                setParameterValue(draggedPosParam, hoveredPos);
                setParameterValue(hoveredPosParam, draggedPos);
                paramValues[draggedPosParam] = hoveredPos;
                paramValues[hoveredPosParam] = draggedPos;
                displayValues[draggedPosParam] = hoveredPos;
                displayValues[hoveredPosParam] = draggedPos;
                repaint();
            }
            return true;
        }

        return false;
    }

private:
    // Adds or removes a pedal from the board (the *On* parameter).
    // This is distinct from Bypass - removing here makes the card
    // disappear entirely, bypassing just mutes it while it stays visible.
    void togglePedalPresence(int pedalIndex)
    {
        const int onParam = kPedalDefs[pedalIndex].onParam;
        if (onParam < 0)
            return;

        const bool currentlyOn = paramValues[onParam] > 0.5f;
        editParameter(onParam, true);

        if (currentlyOn)
        {
            setParameterValue(onParam, 0.0f);
            paramValues[onParam] = 0.0f;
        }
        else
        {
            setParameterValue(onParam, 1.0f);
            paramValues[onParam] = 1.0f;

            const std::vector<int> order = getActiveOrder();
            float maxPos = 0.0f;
            for (int p : order)
                maxPos = std::max(maxPos, paramValues[kPedalDefs[p].positionParam]);
            const int posParam = kPedalDefs[pedalIndex].positionParam;
            const float newPos = std::min(9.0f, maxPos + 1.0f);
            editParameter(posParam, true);
            setParameterValue(posParam, newPos);
            paramValues[posParam] = newPos;
            editParameter(posParam, false);
        }

        editParameter(onParam, false);
        repaint();
    }

    // Applies a full factory preset by replaying every parameter value
    // through setParameterValue(), exactly like ChainPlugin::loadProgram()
    // does on the DSP side - kept in sync because both read from the
    // same kPresets table in ChainPresets.hpp.
    void applyPreset(uint32_t index)
    {
        if (index >= kProgramCount)
            return;

        const PresetDefinition& preset = kPresets[index];
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            editParameter(i, true);
            setParameterValue(i, preset.values[i]);
            paramValues[i] = preset.values[i];
            editParameter(i, false);
        }
        activePreset = static_cast<int>(index);
        repaint();
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

    void formatKnobValue(const KnobDef& knob, float value, char* buf, size_t bufSize) const
    {
        if (knob.asPercent)
            std::snprintf(buf, bufSize, "%.0f%%", value * 100.0f);
        else
            std::snprintf(buf, bufSize, "%.*f%s", knob.decimals, value, knob.unit);
    }

    void drawPresetBar(float width)
    {
        (void)width;
        beginPath();
        rect(0.0f, kTopBarHeight, static_cast<float>(getWidth()), kPresetBarHeight);
        fillColor(kColorPresetBar);
        fill();
        closePath();

        const float presetW = 150.0f;
        for (uint32_t i = 0; i < kProgramCount; ++i)
        {
            const float px = 12.0f + i * (presetW + 8.0f);
            const float py = kTopBarHeight + 5.0f;
            const bool isActive = (activePreset == static_cast<int>(i));

            beginPath();
            roundedRect(px, py, presetW, kPresetBarHeight - 10.0f, 6.0f);
            fillColor(isActive ? kColorPresetActive : kColorPanel);
            fill();
            closePath();

            fontSize(13.0f);
            fillColor(isActive ? kColorTextDark : kColorTextPrimary);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            text(px + presetW * 0.5f, py + (kPresetBarHeight - 10.0f) * 0.5f, kPresets[i].name, nullptr);
        }
    }

    void drawPalette(float height)
    {
        const float paletteTop = kTopBarHeight + kPresetBarHeight;
        beginPath();
        rect(0.0f, paletteTop, kPaletteWidth, height - paletteTop);
        fillColor(kColorPanel);
        fill();
        closePath();

        for (int i = 0; i < kPedalDefCount; ++i)
        {
            if (i == kAmpPedalIndex)
                continue;

            const float itemY = kRackTop + i * kPaletteItemH;
            const bool isActive = paramValues[kPedalDefs[i].onParam] > 0.5f;

            if (isActive)
            {
                beginPath();
                roundedRect(6.0f, itemY + 3.0f, kPaletteWidth - 12.0f, kPaletteItemH - 6.0f, 6.0f);
                fillColor(kPedalDefs[i].accent);
                fill();
                closePath();
            }

            beginPath();
            circle(24.0f, itemY + kPaletteItemH * 0.5f, 5.0f);
            fillColor(isActive ? kPedalDefs[i].accent : kColorOff);
            fill();
            closePath();

            fontSize(15.0f);
            fillColor(isActive ? kColorTextDark : kColorTextMuted);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(42.0f, itemY + kPaletteItemH * 0.5f, kPedalDefs[i].name, nullptr);
        }

        fontSize(11.0f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_LEFT | ALIGN_TOP);
        text(12.0f, height - 24.0f, "Click a pedal to add/remove it", nullptr);
    }

    void drawRack(float width, float height)
    {
        const std::vector<int> order = getActiveOrder();
        const float moduleW = width - kModuleLeft - 20.0f;
        float y = kRackTop;

        save();
        scissor(kModuleLeft - 10.0f, kTopBarHeight + kPresetBarHeight, moduleW + 20.0f, height - kTopBarHeight - kPresetBarHeight);

        for (int pedalIndex : order)
        {
            const PedalDef& def = kPedalDefs[pedalIndex];
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const bool isDraggingThis = (pedalIndex == draggingModuleIndex);
            const bool isBypassed = (def.bypassParam >= 0) && (paramValues[def.bypassParam] > 0.5f);
            const float bodyAlpha = isBypassed ? 0.55f : 1.0f;

            beginPath();
            roundedRect(kModuleLeft, y, moduleW, moduleH, 10.0f);
            fillColor(Color(kColorPanel.red, kColorPanel.green, kColorPanel.blue, bodyAlpha));
            fill();
            closePath();

            if (isDraggingThis)
            {
                beginPath();
                roundedRect(kModuleLeft, y, moduleW, moduleH, 10.0f);
                strokeColor(def.accent);
                strokeWidth(2.0f);
                stroke();
                closePath();
            }

            // Header strip
            const Color headerColor(def.accent.red, def.accent.green, def.accent.blue, bodyAlpha);
            beginPath();
            roundedRect(kModuleLeft, y, moduleW, kModuleHeaderH, 10.0f);
            fillColor(headerColor);
            fill();
            closePath();
            beginPath();
            rect(kModuleLeft, y + kModuleHeaderH * 0.5f, moduleW, kModuleHeaderH * 0.5f);
            fillColor(headerColor);
            fill();
            closePath();

            fontSize(16.0f);
            fillColor(kColorTextDark);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(kModuleLeft + (pedalIndex == kAmpPedalIndex ? 16.0f : 44.0f), y + kModuleHeaderH * 0.5f, def.name, nullptr);

            if (isBypassed)
            {
                fontSize(11.0f);
                fillColor(kColorTextDark);
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                text(kModuleLeft + moduleW * 0.5f - 10.0f, y + kModuleHeaderH * 0.5f, "bypassed", nullptr);
            }

            if (pedalIndex != kAmpPedalIndex)
            {
                const bool isOn = !isBypassed; // switch shows ON when NOT bypassed
                const float sx = kModuleLeft + 12.0f;
                const float sy = y + (kModuleHeaderH - kSwitchSize) * 0.5f;
                beginPath();
                roundedRect(sx, sy, kSwitchSize * 1.6f, kSwitchSize, kSwitchSize * 0.5f);
                fillColor(isOn ? kColorOn : kColorOff);
                fill();
                closePath();
                beginPath();
                circle(isOn ? sx + kSwitchSize * 1.1f : sx + kSwitchSize * 0.5f,
                       sy + kSwitchSize * 0.5f, kSwitchSize * 0.4f);
                fillColor(Color(255, 255, 255));
                fill();
                closePath();

                const float rx = kModuleLeft + moduleW - kRemoveSize - 12.0f;
                const float ry = y + (kModuleHeaderH - kRemoveSize) * 0.5f;
                beginPath();
                circle(rx + kRemoveSize * 0.5f, ry + kRemoveSize * 0.5f, kRemoveSize * 0.5f);
                fillColor(kColorRemove);
                fill();
                closePath();
                fontSize(11.0f);
                fillColor(Color(255, 255, 255));
                textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                text(rx + kRemoveSize * 0.5f, ry + kRemoveSize * 0.5f + 1.0f, "x", nullptr);
            }
            else
            {
                fontSize(11.0f);
                fillColor(kColorTextDark);
                textAlign(ALIGN_RIGHT | ALIGN_MIDDLE);
                text(kModuleLeft + moduleW - 12.0f, y + kModuleHeaderH * 0.5f, "always on", nullptr);
            }

            // Knobs - driven by displayValues (animated), not paramValues,
            // so preset switches and automation glide instead of jumping.
            const float knobCenterY = y + kModuleHeaderH + kKnobAreaH * 0.5f - 8.0f;
            float knobX = kModuleLeft + 50.0f;
            for (const KnobDef& knob : def.knobs)
            {
                drawKnob(knobX, knobCenterY, knob, displayValues[knob.paramIndex], def.accent, bodyAlpha);
                knobX += kKnobSpacing;
            }

            y += moduleH + kModuleGap;
        }

        restore();
    }

    void drawKnob(float cx, float cy, const KnobDef& knob, float value, Color accent, float alpha)
    {
        const float t = (value - knob.minVal) / (knob.maxVal - knob.minVal);
        const float startAngle = 0.75f * static_cast<float>(M_PI);
        const float endAngle   = 2.25f * static_cast<float>(M_PI);
        const float valueAngle = startAngle + t * (endAngle - startAngle);

        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, endAngle, CW);
        strokeColor(Color(kColorKnobTrack.red, kColorKnobTrack.green, kColorKnobTrack.blue, alpha));
        strokeWidth(4.0f);
        stroke();
        closePath();

        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, valueAngle, CW);
        strokeColor(Color(accent.red, accent.green, accent.blue, alpha));
        strokeWidth(4.0f);
        stroke();
        closePath();

        beginPath();
        circle(cx, cy, kKnobRadius - 8.0f);
        fillColor(Color(kColorPanel.red, kColorPanel.green, kColorPanel.blue, alpha));
        fill();
        closePath();

        const float px = cx + std::cos(valueAngle) * (kKnobRadius - 8.0f);
        const float py = cy + std::sin(valueAngle) * (kKnobRadius - 8.0f);
        beginPath();
        moveTo(cx, cy);
        lineTo(px, py);
        strokeColor(Color(kColorTextPrimary.red, kColorTextPrimary.green, kColorTextPrimary.blue, alpha));
        strokeWidth(2.0f);
        stroke();
        closePath();

        fontSize(11.0f);
        fillColor(Color(kColorTextMuted.red, kColorTextMuted.green, kColorTextMuted.blue, alpha));
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(cx, cy + kKnobRadius + 6.0f, knob.label, nullptr);
    }

    // Floating tooltip showing the live numeric value while a knob is
    // being dragged - drawn last so it sits above everything else.
    void drawKnobTooltip()
    {
        if (draggingKnobPedal < 0)
            return;

        const std::vector<int> order = getActiveOrder();
        float y = kRackTop;
        float knobX = 0.0f, knobY = 0.0f;
        bool found = false;

        for (int pedalIndex : order)
        {
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            if (pedalIndex == draggingKnobPedal)
            {
                const PedalDef& def = kPedalDefs[pedalIndex];
                const float knobCenterY = y + kModuleHeaderH + kKnobAreaH * 0.5f - 8.0f;
                knobX = kModuleLeft + 50.0f + draggingKnobIndex * kKnobSpacing;
                knobY = knobCenterY;
                found = true;
                break;
            }
            y += moduleH + kModuleGap;
        }
        if (!found)
            return;

        const KnobDef& knob = kPedalDefs[draggingKnobPedal].knobs[draggingKnobIndex];
        char buf[32];
        formatKnobValue(knob, paramValues[knob.paramIndex], buf, sizeof(buf));

        const float boxW = 70.0f;
        const float boxH = 26.0f;
        const float boxX = knobX - boxW * 0.5f;
        const float boxY = knobY - kKnobRadius - boxH - 12.0f;

        beginPath();
        roundedRect(boxX, boxY, boxW, boxH, 5.0f);
        fillColor(kColorTooltipBg);
        fill();
        closePath();

        fontSize(14.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        text(boxX + boxW * 0.5f, boxY + boxH * 0.5f, buf, nullptr);
    }

    float paramValues[kParamCount];
    float displayValues[kParamCount]; // animated/eased version of paramValues, used for drawing

    int draggingModuleIndex = -1;

    int draggingKnobPedal = -1;
    int draggingKnobIndex = -1;
    float draggingKnobStartY = 0.0f;
    float draggingKnobStartValue = 0.0f;

    int activePreset = -1;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainUI)
};

UI* createUI()
{
    return new ChainUI();
}

END_NAMESPACE_DISTRHO
