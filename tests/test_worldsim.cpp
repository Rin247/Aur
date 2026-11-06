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
