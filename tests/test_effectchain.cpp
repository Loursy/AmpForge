#include "test_harness.hpp"

#include "EffectChain.hpp"

/*
 * EffectChain is what makes runtime pedal reordering possible, so these
 * tests pin down the three things it promises: blocks run in ascending
 * position order, disabled blocks are skipped entirely, and a block
 * coming back from disabled gets reset() before it processes again.
 */

namespace {

// Multiplicative and additive blocks are used together so that order is
// observable in the output: (x + 1) * 2 != x * 2 + 1.
class MultiplyBlock : public ampforge::AudioBlock
{
public:
    explicit MultiplyBlock(float factor, const char* blockName = "multiply")
        : gain(factor), name(blockName) {}

    void setSampleRate(double) override {}
    float processSample(float input) override { return input * gain; }
    const char* getName() const override { return name; }
    void reset() override { ++resetCount; }

    float gain;
    const char* name;
    int resetCount = 0;
};

class AddBlock : public ampforge::AudioBlock
{
public:
    explicit AddBlock(float offset, const char* blockName = "add")
        : amount(offset), name(blockName) {}

    void setSampleRate(double) override {}
    float processSample(float input) override { return input + amount; }
    const char* getName() const override { return name; }
    void reset() override { ++resetCount; }

    float amount;
    const char* name;
    int resetCount = 0;
};

} // namespace

AMPFORGE_TEST(effect_chain_runs_blocks_in_ascending_position_order)
{
    // Added out of order on purpose: position, not insertion order,
    // decides what runs first.
    AddBlock addOne(1.0f);
    MultiplyBlock doubleIt(2.0f);

    ampforge::EffectChain chain;
    chain.addBlock(&doubleIt, 1); // second
    chain.addBlock(&addOne, 0);   // first
    chain.rebuildOrder();

    // (x + 1) * 2
    CHECK_NEAR(chain.processSample(3.0f), 8.0f, 1e-6f);
    CHECK_NEAR(chain.processSample(0.0f), 2.0f, 1e-6f);
}

AMPFORGE_TEST(effect_chain_position_changes_take_effect)
{
    MultiplyBlock doubleIt(2.0f);
    AddBlock addOne(1.0f);

    ampforge::EffectChain chain;
    chain.addBlock(&doubleIt, 0);
    chain.addBlock(&addOne, 1);
    chain.rebuildOrder();

    // doubleIt then addOne: x * 2 + 1
    CHECK_NEAR(chain.processSample(3.0f), 7.0f, 1e-6f);

    // Swapped: addOne then doubleIt: (x + 1) * 2
    chain.setPosition(&doubleIt, 1);
    chain.setPosition(&addOne, 0);
    CHECK_NEAR(chain.processSample(3.0f), 8.0f, 1e-6f);
}

AMPFORGE_TEST(effect_chain_skips_disabled_blocks)
{
    MultiplyBlock doubleIt(2.0f);
    AddBlock addOne(1.0f);

    ampforge::EffectChain chain;
    chain.addBlock(&doubleIt, 0);
    chain.addBlock(&addOne, 1);
    chain.rebuildOrder();

    chain.setEnabled(&doubleIt, false);
    CHECK_NEAR(chain.processSample(3.0f), 4.0f, 1e-6f); // only +1 applies

    chain.setEnabled(&doubleIt, true);
    CHECK_NEAR(chain.processSample(3.0f), 7.0f, 1e-6f);
}

AMPFORGE_TEST(effect_chain_resets_a_block_when_it_is_reenabled)
{
    MultiplyBlock doubleIt(2.0f);
    AddBlock addOne(1.0f);

    ampforge::EffectChain chain;
    chain.addBlock(&doubleIt, 0);
    chain.addBlock(&addOne, 1);
    chain.rebuildOrder();

    CHECK(doubleIt.resetCount == 0);

    chain.setEnabled(&doubleIt, false);
    CHECK(doubleIt.resetCount == 0); // disabling must not reset

    chain.setEnabled(&doubleIt, true);
    CHECK(doubleIt.resetCount == 1); // coming back from disabled must reset

    // Enabling an already-enabled block is a no-op, not another reset.
    chain.setEnabled(&doubleIt, true);
    CHECK(doubleIt.resetCount == 1);
}

AMPFORGE_TEST(effect_chain_ignores_blocks_that_were_never_added)
{
    // setEnabled()/setPosition() take a raw pointer, so a block that was
    // never added must simply find no slot rather than corrupt anything.
    MultiplyBlock doubleIt(2.0f);
    MultiplyBlock stranger(3.0f);

    ampforge::EffectChain chain;
    chain.addBlock(&doubleIt, 0);
    chain.rebuildOrder();

    chain.setEnabled(&stranger, false);
    chain.setPosition(&stranger, 5);

    CHECK_NEAR(chain.processSample(1.0f), 2.0f, 1e-6f);
}
