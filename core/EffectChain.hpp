#pragma once
#include "AudioBlock.hpp"
#include <vector>
#include <algorithm>

/*
 * EffectChain - manages a set of AudioBlocks and the order they should
 * be processed in. Each block occupies a "slot": it can be enabled or
 * disabled, and it has a position (0 = processed first, 1 = next, ...).
 *
 * This is what makes true reordering possible: instead of a hardcoded
 * "Screamer -> Amp -> Delay -> Reverb" sequence in code, the order is
 * data that can change at runtime - later driven by dragging blocks
 * around in the GUI, or by host automation.
 *
 * IMPORTANT: add all blocks with addBlock() first, then call
 * rebuildOrder() once before audio processing starts. After that,
 * setPosition()/setEnabled() are safe to call at any time (including
 * from the audio thread's parameter updates) because no more blocks
 * are being added - the underlying storage no longer resizes, so the
 * pointers we hand out stay valid.
 */

namespace ampforge {

struct ChainSlot
{
    AudioBlock* block = nullptr;
    bool enabled = true;
    int position = 0; // lower = processed earlier
};

class EffectChain
{
public:
    // Call this for every block during setup, before rebuildOrder().
    void addBlock(AudioBlock* block, int initialPosition)
    {
        ChainSlot slot;
        slot.block = block;
        slot.position = initialPosition;
        slots.push_back(slot);
    }

    // Call once after all addBlock() calls, and again any time a
    // position changes.
    void rebuildOrder()
    {
        orderedSlots.clear();
        for (auto& slot : slots)
            orderedSlots.push_back(&slot);

        std::sort(orderedSlots.begin(), orderedSlots.end(),
                   [](const ChainSlot* a, const ChainSlot* b)
                   {
                       return a->position < b->position;
                   });
    }

    void setEnabled(AudioBlock* block, bool enabled)
    {
        for (auto& slot : slots)
        {
            if (slot.block == block)
            {
                slot.enabled = enabled;
                return;
            }
        }
    }

    void setPosition(AudioBlock* block, int position)
    {
        for (auto& slot : slots)
        {
            if (slot.block == block)
            {
                slot.position = position;
                rebuildOrder();
                return;
            }
        }
    }

    float processSample(float input)
    {
        float x = input;
        for (auto* slot : orderedSlots)
        {
            if (slot->enabled)
                x = slot->block->processSample(x);
        }
        return x;
    }

private:
    std::vector<ChainSlot> slots;
    std::vector<ChainSlot*> orderedSlots;
};

} // namespace ampforge
