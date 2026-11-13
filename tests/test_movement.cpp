#include "mini_test.hpp"

#include "aur/camera.hpp"
#include "aur/movement.hpp"
#include "aur/project.hpp"

#include <cmath>
#include <vector>

namespace
{
    // A map of solid ground at the bottom, open air above.
    std::vector<std::vector<int>> ground_map(int width, int height)
    {
        std::vector<std::vector<int>> flags(height, std::vector<int>(width, 0));
        for (int x = 0; x < width; ++x)
            flags[static_cast<size_t>(height - 1)][static_cast<size_t>(x)] = aur::SOLID;
        return flags;
    }
} // namespace

// The movement rules: the top-down field is unchanged, and a platformer
// falls, lands and jumps.
void test_movement_modes()
{
    using namespace aur;

    // --- the top-down field: today's rule, unchanged ---
    const std::vector<std::vector<int>> open_map = ground_map(20, 20);
    MoveWorld world;
    world.flags = &open_map;
    world.width = 20;
    world.height = 20;
    world.tile_size = 16;

    Body body;
    body.x = 5 * 16 + 8;
    body.y = 5 * 16 + 8;
    moveTopDown(body, world, 1, 0, 148.0, 1.0 / 60.0);
    // 148 px per second, a sixtieth of a second.
    ASSERT(std::abs(body.x - (5 * 16 + 8 + 148.0 / 60.0)) < 0.001);
    ASSERT(body.y == 5 * 16 + 8);

    // The top-down field does not fall.
    moveTopDown(body, world, 0, 0, 148.0, 1.0 / 60.0);
    ASSERT(body.y == 5 * 16 + 8);

    // --- the platformer: it falls ---
    MoveRules rules;
    const std::vector<std::vector<int>> ground = ground_map(20, 20);
    MoveWorld platform;
    platform.flags = &ground;
    platform.width = 20;
    platform.height = 20;
    platform.tile_size = 16;

    Body jumper;
    jumper.x = 5 * 16 + 8;
    jumper.y = 2 * 16 + 8;
    jumper.half_width = 4;
    jumper.half_height = 8;
    const double start_y = jumper.y;
    for (int frame = 0; frame < 10; ++frame)
        movePlatformer(jumper, platform, rules, 0, false, false, 1.0 / 60.0);
    ASSERT(jumper.y > start_y);          // it fell
    ASSERT(!jumper.on_ground);           // and it is still falling

    // It lands on the ground: the floor stops it.
    for (int frame = 0; frame < 240 && !jumper.on_ground; ++frame)
        movePlatformer(jumper, platform, rules, 0, false, false, 1.0 / 60.0);
    ASSERT(jumper.on_ground);
    ASSERT(jumper.vy == 0.0);

    // And it stays there.
    const double resting = jumper.y;
    for (int frame = 0; frame < 30; ++frame)
        movePlatformer(jumper, platform, rules, 0, false, false, 1.0 / 60.0);
    // The body settles by less than a pixel: the collision catches it
    // a step after the landing.
    ASSERT(std::abs(jumper.y - resting) < 1.0);

    // A jump: it leaves the ground and comes back.
    movePlatformer(jumper, platform, rules, 0, true, true, 1.0 / 60.0);
    ASSERT(!jumper.on_ground);
    ASSERT(jumper.vy < 0);
    double highest = jumper.y;
    for (int frame = 0; frame < 240; ++frame)
    {
        movePlatformer(jumper, platform, rules, 0, false, false, 1.0 / 60.0);
        highest = std::min(highest, jumper.y);
    }
    ASSERT(highest < resting);           // it went up
    ASSERT(jumper.on_ground);            // and it came back

    // The jump's height: letting go early cuts the rise.
    Body short_jump = jumper;
    movePlatformer(short_jump, platform, rules, 0, true, true, 1.0 / 60.0);
    for (int frame = 0; frame < 6; ++frame)
        movePlatformer(short_jump, platform, rules, 0, false, false, 1.0 / 60.0);
    const double short_height = resting - short_jump.y;

    Body full_jump = jumper;
    movePlatformer(full_jump, platform, rules, 0, true, true, 1.0 / 60.0);
    for (int frame = 0; frame < 6; ++frame)
        movePlatformer(full_jump, platform, rules, 0, false, true, 1.0 / 60.0);
    const double full_height = resting - full_jump.y;
    ASSERT(short_height < full_height);
}

// A one-way platform: the hero comes up through it and lands on it from
// above, and the top-down field does not see it at all.
void test_movement_one_way()
{
    using namespace aur;

    // A platform two rows above the ground, and the ground below it.
    std::vector<std::vector<int>> flags(20, std::vector<int>(20, 0));
    for (int x = 0; x < 20; ++x)
        flags[19][static_cast<size_t>(x)] = aur::SOLID;
    for (int x = 0; x < 20; ++x)
        flags[17][static_cast<size_t>(x)] = aur::ONE_WAY;

    MoveWorld world;
    world.flags = &flags;
    world.width = 20;
    world.height = 20;
    world.tile_size = 16;
    MoveRules rules;

    // Falling onto the platform from above: the hero lands on it.
    Body body;
    body.x = 5 * 16 + 8;
    body.y = 15 * 16 + 8;
    body.half_width = 4;
    body.half_height = 8;
    for (int frame = 0; frame < 120 && !body.on_ground; ++frame)
        movePlatformer(body, world, rules, 0, false, false, 1.0 / 60.0);
    ASSERT(body.on_ground);
    // It rests on the platform, not on the ground below it.
    ASSERT(body.y + body.half_height <= 17 * 16 + 1.0);

    // A hero on the ground jumps up through the platform and comes back
    // down onto it.
    Body jumper;
    jumper.x = 5 * 16 + 8;
    jumper.y = 18 * 16 + 8;
    jumper.half_width = 4;
    jumper.half_height = 8;
    for (int frame = 0; frame < 120 && !jumper.on_ground; ++frame)
        movePlatformer(jumper, world, rules, 0, false, false, 1.0 / 60.0);
    ASSERT(jumper.on_ground);

    movePlatformer(jumper, world, rules, 0, true, true, 1.0 / 60.0);
    bool passed = false;
    for (int frame = 0; frame < 120; ++frame)
    {
        // The jump is held for the rise and released after it.
        movePlatformer(jumper, world, rules, 0, false, frame < 20, 1.0 / 60.0);
        if (jumper.y + jumper.half_height < 17 * 16) passed = true;
    }
    ASSERT(passed);   // it went up through the platform
    ASSERT(jumper.on_ground);
    // And it came back down onto it, not through it.
    ASSERT(jumper.y + jumper.half_height <= 17 * 16 + 1.0);

    // The top-down field does not see the flag: it walks over it.
    std::vector<std::vector<int>> open_map(20, std::vector<int>(20, 0));
    MoveWorld flat;
    flat.flags = &open_map;
    flat.width = 20;
    flat.height = 20;
    flat.tile_size = 16;
    Body walker;
    walker.x = 5 * 16 + 8;
    walker.y = 17 * 16 + 8;
    moveTopDown(walker, flat, 1, 0, 148.0, 1.0 / 60.0);
    ASSERT(walker.x > 5 * 16 + 8);
}

// The cameras: the field's is the hero's place, clamped and wrap-aware,
// and the platformer's scrolls sideways with a dead zone.
void test_camera_modes()
{
    using namespace aur;

    MoveWorld world;
    world.width = 100;
    world.height = 50;
    world.tile_size = 16;

    // The field's camera: the hero in the middle of a big map.
    Body hero;
    hero.x = 50 * 16;
    hero.y = 25 * 16;
    const Camera field = fieldCamera(hero, world, 320, 240);
    ASSERT(std::abs(field.x - (50 * 16 - 160)) < 0.001);
    ASSERT(std::abs(field.y - (25 * 16 - 24 - 120)) < 0.001);

    // The clamp: the hero at the left edge, the camera stops at zero.
    hero.x = 0;
    const Camera edge = fieldCamera(hero, world, 320, 240);
    ASSERT(edge.x == 0.0);

    // The wrap: a world map does not clamp in x.
    world.wraps_x = true;
    hero.x = 50 * 16;
    const Camera wrapped = fieldCamera(hero, world, 320, 240);
    ASSERT(std::abs(wrapped.x - (50 * 16 - 160)) < 0.001);

    // The platformer's camera: it scrolls sideways and holds its height
    // until the hero leaves the dead zone.
    world.wraps_x = false;
    Body jumper;
    jumper.x = 50 * 16;
    jumper.y = 25 * 16;
    Camera camera{0, 0};
    camera = platformerCamera(jumper, world, 320, 240, 48.0, camera);
    ASSERT(std::abs(camera.x - (50 * 16 - 160)) < 0.001);
    const double held = camera.y;

    // The hero is inside the dead zone: the camera holds.
    jumper.y = held + 120 + 10;
    camera = platformerCamera(jumper, world, 320, 240, 48.0, camera);
    ASSERT(camera.y == held);

    // The hero leaves the zone: the camera follows.
    jumper.y = held + 120 + 60;
    camera = platformerCamera(jumper, world, 320, 240, 48.0, camera);
    ASSERT(camera.y != held);
    ASSERT(camera.y <= jumper.y);
}
