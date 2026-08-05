/*
 * AmpForge - Main Chain UI (Phase 9, Step 1: scaffold)
 *
 * This is the very first version of the GUI - just enough to prove the
 * window opens, draws with NanoVG, and receives parameter updates from
 * the DSP side. The actual pedal rack (drag-and-drop palette, cards,
 * on/off switches) comes in the next steps, built on top of this.
 */

#include "DistrhoUI.hpp"

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

// A dark, modern color palette - this is what the whole GUI will be
// built from going forward (pedal cards, palette, etc. all reuse these).
static const Color kColorBackground(18, 18, 22);
static const Color kColorPanel(28, 28, 34);
static const Color kColorAccent(90, 170, 255);
static const Color kColorTextPrimary(230, 230, 235);
static const Color kColorTextMuted(140, 140, 150);

class ChainUI : public UI
{
public:
    ChainUI()
        : UI(960, 640)
    {
        // Load DPF's bundled default font so text() calls work without
        // us having to ship or point to a system font file.
        loadSharedResources();
    }

protected:
    // Called by the host whenever a parameter changes (including when
    // the user picks a factory preset) - lets the GUI stay in sync with
    // the DSP side. We don't use these values for drawing yet (Step 1
    // is just the empty shell), but we store them so future steps can.
    void parameterChanged(uint32_t index, float value) override
    {
        (void)index;
        (void)value;
        repaint();
    }

    void onNanoDisplay() override
    {
        const float width  = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());

        // Background
        beginPath();
        rect(0.0f, 0.0f, width, height);
        fillColor(kColorBackground);
        fill();
        closePath();

        // Top title bar panel
        beginPath();
        rect(0.0f, 0.0f, width, 60.0f);
        fillColor(kColorPanel);
        fill();
        closePath();

        fontSize(22.0f);
        fillColor(kColorTextPrimary);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(20.0f, 30.0f, "AmpForge", nullptr);

        fontSize(13.0f);
        fillColor(kColorTextMuted);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        text(20.0f, height - 20.0f, "Pedal rack coming next - this window is just the scaffold.", nullptr);

        // A placeholder accent line under the title bar, so we can
        // already see the accent color we'll build the rest of the UI around.
        beginPath();
        rect(0.0f, 60.0f, width, 3.0f);
        fillColor(kColorAccent);
        fill();
        closePath();
    }

private:
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainUI)
};

UI* createUI()
{
    return new ChainUI();
}

END_NAMESPACE_DISTRHO
