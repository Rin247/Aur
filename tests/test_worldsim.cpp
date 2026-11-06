#include "mini_test.hpp"
#include "aur/worldsim.hpp"

void test_worldsim_shell()
{
    aur::worldsim::WorldSimParams params;
    params.width = 8;
    params.height = 4;
    params.seed = 42;
    aur::worldsim::WorldSim sim(params);
    ASSERT(sim.tick_count() == 0);

    const std::uint64_t initial = sim.hash();
    sim.advance(10);
    ASSERT(sim.tick_count() == 10);
    ASSERT(sim.hash() != initial);

    aur::worldsim::WorldSim same(params);
    same.advance(10);
    ASSERT(same.hash() == sim.hash());

    aur::worldsim::WorldSim other_seed(params);
    other_seed.advance(10);
    ASSERT(other_seed.hash() == sim.hash());

    sim.advance(0);
    ASSERT(sim.tick_count() == 10);
}

void test_worldsim_pins_and_values()
{
    aur::worldsim::WorldSimParams params;
    params.width = 4;
    params.height = 3;
    aur::worldsim::WorldSim sim(params);

    ASSERT(!sim.is_pinned(aur::worldsim::Field::Temperature, 2, 1));
    sim.pin(aur::worldsim::Field::Temperature, 2, 1, 19.5f);
    ASSERT(sim.is_pinned(aur::worldsim::Field::Temperature, 2, 1));
    ASSERT(sim.value(aur::worldsim::Field::Temperature, 2, 1) == 19.5f);

    sim.clear_pin(aur::worldsim::Field::Temperature, 2, 1);
    ASSERT(!sim.is_pinned(aur::worldsim::Field::Temperature, 2, 1));
}

void test_worldsim_json_roundtrip()
{
    aur::worldsim::WorldSimParams params;
    params.width = 6;
    params.height = 4;
    params.seed = 0xdeadbeefcafeULL;
    aur::worldsim::WorldSim sim(params);
    sim.pin(aur::worldsim::Field::Moisture, 1, 2, 0.75f);
    sim.advance(1234);

    aur::Json saved = sim.to_json();
    aur::worldsim::WorldSim loaded = aur::worldsim::WorldSim::from_json(saved);
    ASSERT(loaded.hash() == sim.hash());
    ASSERT(loaded.tick_count() == 1234);
    ASSERT(loaded.params().seed == params.seed);
}

void test_worldsim_kernel_determinism()
{
    aur::worldsim::WorldSimParams params;
    params.width = 8;
    params.height = 6;
    params.seed = 777;
    aur::worldsim::WorldSimParams other = params;

    aur::worldsim::WorldSim a(params);
    aur::worldsim::WorldSim b(other);
    std::vector<float> heights(static_cast<std::size_t>(params.width) * params.height, 0.5f);
    a.set_heights(heights);
    b.set_heights(heights);

    a.advance(1);
    a.advance(1);
    b.advance(2);
    ASSERT(a.hash() == b.hash());

    // Split advance is equivalent to one big advance.
    aur::worldsim::WorldSim c(params);
    c.set_heights(heights);
    c.advance(2);
    ASSERT(c.hash() == a.hash());
}

void test_worldsim_pin_immutable_through_kernels()
{
    aur::worldsim::WorldSimParams params;
    params.width = 8;
    params.height = 6;
    aur::worldsim::WorldSim sim(params);
    std::vector<float> heights(static_cast<std::size_t>(params.width) * params.height, 0.2f);
    sim.set_heights(heights);
    sim.pin(aur::worldsim::Field::Temperature, 3, 2, 99.0f);
    sim.pin(aur::worldsim::Field::Moisture, 0, 0, 0.25f);
    sim.pin(aur::worldsim::Field::Cloud, 7, 5, 1.0f);

    sim.advance(5);
    ASSERT(sim.value(aur::worldsim::Field::Temperature, 3, 2) == 99.0f);
    ASSERT(sim.value(aur::worldsim::Field::Moisture, 0, 0) == 0.25f);
    ASSERT(sim.value(aur::worldsim::Field::Cloud, 7, 5) == 1.0f);
}

void test_worldsim_wrap_continuity()
{
    // Same relative position modulo the width must produce the same value:
    // x = -1 reads/writes as x = W-1 across every kernel boundary.
    aur::worldsim::WorldSimParams params;
    params.width = 8;
    params.height = 4;
    params.seed = 31337;
    aur::worldsim::WorldSim sim(params);
    std::vector<float> heights(static_cast<std::size_t>(params.width) * params.height);
    for (std::size_t i = 0; i < heights.size(); ++i)
        heights[i] = static_cast<float>(i % 7) * 0.1f;
    sim.set_heights(heights);
    sim.advance(3);

    const float at_seven = sim.value(aur::worldsim::Field::Temperature, 7, 1);
    const float at_wrap = sim.value(
        aur::worldsim::Field::Temperature, aur::wrapX(-1, params.width) + params.width, 1);
    ASSERT(at_seven == at_wrap); // x = W-1 read directly and via out-of-range index
    ASSERT(sim.value(aur::worldsim::Field::Temperature, params.width, 1) ==
           sim.value(aur::worldsim::Field::Temperature, 0, 1));

    // Diffusion across the seam: force an extreme at x=0 and check the wrapped
    // neighbor at x=W-1 moved toward it, symmetric with the interior case.
    aur::worldsim::WorldSimParams hot = params;
    hot.seed = 5;
    aur::worldsim::WorldSim seam(hot);
    std::vector<float> plateau(static_cast<std::size_t>(hot.width) * hot.height, 0.0f);
    seam.set_heights(plateau);
    seam.pin(aur::worldsim::Field::Temperature, 0, 2, 10.0f);
    seam.pin(aur::worldsim::Field::Temperature, aur::wrapX(-1, hot.width), 2, 10.0f);
    seam.advance(1);
    const float left = seam.value(aur::worldsim::Field::Temperature,
                                  aur::wrapX(-1, hot.width), 2);
    const float right = seam.value(aur::worldsim::Field::Temperature, 0, 2);
    ASSERT(left == right);
    ASSERT(left == 10.0f); // pins survive the kernel
}
