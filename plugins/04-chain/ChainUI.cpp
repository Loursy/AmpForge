/*
 * AmpForge - Main Chain UI (Phase 9, Step 3: distinct modules + real knobs)
 *
 * Redesigned after feedback: each pedal now has its own accent color
 * and its own row of real, functional knobs (drag up/down to change
 * value) - no more identical gray boxes. Modules stack vertically and
 * scroll if they don't fit, instead of running off the right edge of
 * the window. Reordering is now done by dragging a module up or down.
 */

#include "DistrhoUI.hpp"
#include "ChainParameters.hpp"
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

// --- Layout constants ---
static constexpr float kTopBarHeight    = 60.0f;
static constexpr float kPaletteWidth    = 190.0f;
static constexpr float kPaletteItemH    = 42.0f;
static constexpr float kRackTop         = kTopBarHeight + 20.0f;
static constexpr float kModuleLeft      = kPaletteWidth + 20.0f;
static constexpr float kModuleHeaderH   = 42.0f;
static constexpr float kKnobAreaH       = 92.0f;
static constexpr float kModuleGap       = 12.0f;
static constexpr float kKnobRadius      = 24.0f;
static constexpr float kKnobSpacing     = 78.0f;
static constexpr float kSwitchSize      = 18.0f;
static constexpr float kRemoveSize      = 16.0f;
static constexpr float kKnobDragSensitivity = 220.0f; // pixels of drag for full range sweep

struct KnobDef
{
    const char* label;
    int paramIndex;
    float minVal;
    float maxVal;
};

// One entry per block. Amp has onParam = -1 because it's always part
// of the chain (no add/remove, no on/off switch).
struct PedalDef
{
    const char* name;
    int onParam;
    int positionParam;
    Color accent;
    std::vector<KnobDef> knobs;
};

// clang-format off
static const PedalDef kPedalDefs[] =
{
    { "Noise Gate", kParamGateOn,     kParamGatePosition,  Color(130, 150, 210), {
        { "Thresh",  kParamGateThreshold, -80.0f, 0.0f },
        { "Attack",  kParamGateAttack,      0.5f, 50.0f },
        { "Release", kParamGateRelease,    10.0f, 1000.0f },
    }},
    { "Compressor", kParamCompOn,     kParamCompPosition,  Color(175, 120, 225), {
        { "Thresh",  kParamCompThreshold, -60.0f, 0.0f },
        { "Ratio",   kParamCompRatio,       1.0f, 20.0f },
        { "Attack",  kParamCompAttack,      0.5f, 100.0f },
        { "Release", kParamCompRelease,    10.0f, 1000.0f },
        { "Makeup",  kParamCompMakeup,      0.0f, 24.0f },
    }},
    { "Wah",        kParamWahOn,      kParamWahPosition,   Color(235, 155, 60), {
        { "Pedal",   kParamWahPedal,  0.0f, 1.0f },
        { "Q",       kParamWahQ,      0.5f, 10.0f },
    }},
    { "Screamer",   kParamScreamerOn, kParamScreamerPosition, Color(235, 95, 70), {
        { "Drive",   kParamScreamerDrive,  1.0f, 20.0f },
        { "Tone",    kParamScreamerTone,   0.05f, 1.0f },
        { "Level",   kParamScreamerLevel, -24.0f, 12.0f },
    }},
    { "Amp",        -1,               kParamAmpPosition,   Color(90, 170, 255), {
        { "Drive",   kParamAmpDrive,    0.0f, 36.0f },
        { "Bass",    kParamAmpBass,   -12.0f, 12.0f },
        { "Mid",     kParamAmpMid,    -12.0f, 12.0f },
        { "Treble",  kParamAmpTreble, -12.0f, 12.0f },
        { "Volume",  kParamAmpVolume, -24.0f, 12.0f },
    }},
    { "Chorus",     kParamChorusOn,   kParamChorusPosition, Color(70, 205, 195), {
        { "Rate",    kParamChorusRate,   0.05f, 5.0f },
        { "Depth",   kParamChorusDepth,  0.5f, 20.0f },
        { "Mix",     kParamChorusMix,    0.0f, 1.0f },
    }},
    { "Phaser",     kParamPhaserOn,   kParamPhaserPosition, Color(185, 115, 235), {
        { "Rate",    kParamPhaserRate,  0.05f, 5.0f },
        { "Depth",   kParamPhaserDepth, 0.0f, 1.0f },
        { "Mix",     kParamPhaserMix,   0.0f, 1.0f },
    }},
    { "Tremolo",    kParamTremoloOn,  kParamTremoloPosition, Color(235, 205, 60), {
        { "Rate",    kParamTremoloRate,  0.5f, 15.0f },
        { "Depth",   kParamTremoloDepth, 0.0f, 1.0f },
    }},
    { "Delay",      kParamDelayOn,    kParamDelayPosition,  Color(95, 225, 145), {
        { "Time",     kParamDelayTime,      10.0f, 1500.0f },
        { "Feedback", kParamDelayFeedback,   0.0f, 0.95f },
        { "Mix",      kParamDelayMix,        0.0f, 1.0f },
    }},
    { "Reverb",     kParamReverbOn,   kParamReverbPosition, Color(115, 125, 235), {
        { "Room",     kParamReverbRoomSize, 0.0f, 1.0f },
        { "Damping",  kParamReverbDamping,  0.0f, 1.0f },
        { "Mix",      kParamReverbMix,      0.0f, 1.0f },
    }},
};
// clang-format on
static constexpr int kPedalDefCount = sizeof(kPedalDefs) / sizeof(kPedalDefs[0]);
static constexpr int kAmpPedalIndex = 4; // index of "Amp" within kPedalDefs

class ChainUI : public UI
{
public:
    ChainUI()
        : UI(1000, 700)
    {
        loadSharedResources();
        std::fill(std::begin(paramValues), std::end(paramValues), 0.0f);
    }

protected:
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
            paramValues[index] = value;
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

        fontSize(22.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(20.0f, 30.0f, "AmpForge", nullptr);

        drawPalette(height);
        drawRack(width, height);
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

        // --- Palette clicks ---
        if (mx < kPaletteWidth && my > kTopBarHeight)
        {
            for (int i = 0; i < kPedalDefCount; ++i)
            {
                if (i == kAmpPedalIndex)
                    continue;
                const float itemY = kRackTop + i * kPaletteItemH;
                if (my >= itemY && my < itemY + kPaletteItemH)
                {
                    togglePedal(i);
                    return true;
                }
            }
            return false;
        }

        // --- Module clicks (knobs / switch / remove / drag handle) ---
        const std::vector<int> order = getActiveOrder();
        float y = kRackTop + scrollOffset;

        for (size_t slot = 0; slot < order.size(); ++slot)
        {
            const int pedalIndex = order[slot];
            const PedalDef& def = kPedalDefs[pedalIndex];
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const float moduleW = getWidth() - kModuleLeft - 20.0f;

            if (my >= y && my < y + moduleH && mx >= kModuleLeft && mx <= kModuleLeft + moduleW)
            {
                // On/off switch (top-left of header) - not for Amp
                if (pedalIndex != kAmpPedalIndex)
                {
                    const float sx = kModuleLeft + 12.0f;
                    const float sy = y + (kModuleHeaderH - kSwitchSize) * 0.5f;
                    if (mx >= sx && mx <= sx + kSwitchSize * 1.6f && my >= sy && my <= sy + kSwitchSize)
                    {
                        const int onParam = def.onParam;
                        const bool currentlyOn = paramValues[onParam] > 0.5f;
                        editParameter(onParam, true);
                        setParameterValue(onParam, currentlyOn ? 0.0f : 1.0f);
                        paramValues[onParam] = currentlyOn ? 0.0f : 1.0f;
                        editParameter(onParam, false);
                        repaint();
                        return true;
                    }

                    // Remove button (top-right of header)
                    const float rx = kModuleLeft + moduleW - kRemoveSize - 12.0f;
                    const float ry = y + (kModuleHeaderH - kRemoveSize) * 0.5f;
                    if (mx >= rx && mx <= rx + kRemoveSize && my >= ry && my <= ry + kRemoveSize)
                    {
                        togglePedal(pedalIndex);
                        return true;
                    }
                }

                // Knobs (in the body area below the header)
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

        // --- Knob drag: vertical mouse movement changes the value ---
        if (draggingKnobPedal >= 0)
        {
            const KnobDef& knob = kPedalDefs[draggingKnobPedal].knobs[draggingKnobIndex];
            const float deltaPixels = draggingKnobStartY - my; // up = increase
            const float range = knob.maxVal - knob.minVal;
            float newValue = draggingKnobStartValue + (deltaPixels / kKnobDragSensitivity) * range;
            newValue = std::max(knob.minVal, std::min(knob.maxVal, newValue));

            setParameterValue(knob.paramIndex, newValue);
            paramValues[knob.paramIndex] = newValue;
            repaint();
            return true;
        }

        // --- Module drag: reorder by swapping with whichever module the
        // cursor is currently over ---
        if (draggingModuleIndex >= 0)
        {
            const std::vector<int> order = getActiveOrder();
            float y = kRackTop + scrollOffset;
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
                repaint();
            }
            return true;
        }

        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const float contentHeight = totalContentHeight();
        const float visibleHeight = static_cast<float>(getHeight()) - kRackTop;
        const float maxScroll = std::max(0.0f, contentHeight - visibleHeight);

        scrollOffset -= static_cast<float>(ev.delta.getY()) * 24.0f;
        scrollOffset = std::max(-maxScroll, std::min(0.0f, scrollOffset));
        repaint();
        return true;
    }

private:
    // Adds or removes a pedal from the active chain. Adding appends it
    // at the end of the current visible order (max active position + 1).
    void togglePedal(int pedalIndex)
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

    void drawPalette(float height)
    {
        beginPath();
        rect(0.0f, kTopBarHeight, kPaletteWidth, height - kTopBarHeight);
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
                fillColor(Color(kPedalDefs[i].accent.red, kPedalDefs[i].accent.green, kPedalDefs[i].accent.blue, 0.18f));
                fill();
                closePath();
            }

            beginPath();
            circle(24.0f, itemY + kPaletteItemH * 0.5f, 5.0f);
            fillColor(isActive ? kPedalDefs[i].accent : kColorOff);
            fill();
            closePath();

            fontSize(15.0f);
            fillColor(isActive ? kColorTextPrimary : kColorTextMuted);
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
        float y = kRackTop + scrollOffset;

        // Clip drawing to the rack area so modules don't paint over the
        // top bar or palette while scrolling.
        save();
        scissor(kModuleLeft - 10.0f, kTopBarHeight, moduleW + 20.0f, height - kTopBarHeight);

        for (int pedalIndex : order)
        {
            const PedalDef& def = kPedalDefs[pedalIndex];
            const float moduleH = kModuleHeaderH + kKnobAreaH;
            const bool isDraggingThis = (pedalIndex == draggingModuleIndex);

            // Module background
            beginPath();
            roundedRect(kModuleLeft, y, moduleW, moduleH, 10.0f);
            fillColor(kColorPanel);
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

            // Header strip, tinted with the pedal's accent color
            beginPath();
            roundedRect(kModuleLeft, y, moduleW, kModuleHeaderH, 10.0f);
            fillColor(def.accent);
            fill();
            closePath();
            // square off the bottom corners of the header so it reads as a bar
            beginPath();
            rect(kModuleLeft, y + kModuleHeaderH * 0.5f, moduleW, kModuleHeaderH * 0.5f);
            fillColor(def.accent);
            fill();
            closePath();

            fontSize(16.0f);
            fillColor(kColorTextDark);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            text(kModuleLeft + (pedalIndex == kAmpPedalIndex ? 16.0f : 44.0f), y + kModuleHeaderH * 0.5f, def.name, nullptr);

            if (pedalIndex != kAmpPedalIndex)
            {
                const bool isOn = paramValues[def.onParam] > 0.5f;
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

            // Knobs
            const float knobCenterY = y + kModuleHeaderH + kKnobAreaH * 0.5f - 8.0f;
            float knobX = kModuleLeft + 50.0f;
            for (const KnobDef& knob : def.knobs)
            {
                drawKnob(knobX, knobCenterY, knob, paramValues[knob.paramIndex], def.accent);
                knobX += kKnobSpacing;
            }

            y += moduleH + kModuleGap;
        }

        restore();

        // Scrollbar hint if content overflows
        const float contentHeight = totalContentHeight();
        const float visibleHeight = height - kRackTop;
        if (contentHeight > visibleHeight)
        {
            const float trackH = visibleHeight - 10.0f;
            const float thumbH = std::max(30.0f, trackH * (visibleHeight / contentHeight));
            const float scrollFrac = (-scrollOffset) / std::max(1.0f, contentHeight - visibleHeight);
            const float thumbY = kRackTop + 5.0f + scrollFrac * (trackH - thumbH);

            beginPath();
            roundedRect(width - 6.0f, thumbY, 4.0f, thumbH, 2.0f);
            fillColor(kColorKnobTrack);
            fill();
            closePath();
        }
    }

    void drawKnob(float cx, float cy, const KnobDef& knob, float value, Color accent)
    {
        const float t = (value - knob.minVal) / (knob.maxVal - knob.minVal);
        const float startAngle = 0.75f * static_cast<float>(M_PI);
        const float endAngle   = 2.25f * static_cast<float>(M_PI);
        const float valueAngle = startAngle + t * (endAngle - startAngle);

        // Track (background arc)
        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, endAngle, CW);
        strokeColor(kColorKnobTrack);
        strokeWidth(4.0f);
        stroke();
        closePath();

        // Value arc
        beginPath();
        arc(cx, cy, kKnobRadius, startAngle, valueAngle, CW);
        strokeColor(accent);
        strokeWidth(4.0f);
        stroke();
        closePath();

        // Center dot + pointer line
        beginPath();
        circle(cx, cy, kKnobRadius - 8.0f);
        fillColor(kColorPanel);
        fill();
        closePath();

        const float px = cx + std::cos(valueAngle) * (kKnobRadius - 8.0f);
        const float py = cy + std::sin(valueAngle) * (kKnobRadius - 8.0f);
        beginPath();
        moveTo(cx, cy);
        lineTo(px, py);
        strokeColor(kColorTextPrimary);
        strokeWidth(2.0f);
        stroke();
        closePath();

        fontSize(11.0f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_CENTER | ALIGN_TOP);
        text(cx, cy + kKnobRadius + 6.0f, knob.label, nullptr);
    }

    float paramValues[kParamCount];

    int draggingModuleIndex = -1;

    int draggingKnobPedal = -1;
    int draggingKnobIndex = -1;
    float draggingKnobStartY = 0.0f;
    float draggingKnobStartValue = 0.0f;

    float scrollOffset = 0.0f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainUI)
};

UI* createUI()
{
    return new ChainUI();
}

END_NAMESPACE_DISTRHO
