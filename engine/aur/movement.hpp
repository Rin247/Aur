#pragma once

// Movement.hpp - the engine's movement rules.
//
// The top-down field is one mode of them and a platformer is another.
// The mode owns the probe and the physics, so a map's "mode" chooses the
// rules and the shipped field keeps its feel exactly.
//
// The rules are a pure function of the hero's state and the tile flags:
// no widget, no clock, no game. That is what makes them testable.

#include <vector>

namespace aur
{

    enum class MovementMode { TopDown, Platformer };

    // The hero's body. The top-down field treats it as a point; the
    // platformer gives it a width, a height and a velocity.
    struct Body
    {
        double x = 0;
        double y = 0;
        double vx = 0;
        double vy = 0;
        double half_width = 4;
        double half_height = 8;
        bool on_ground = false;
        int feet_flags = 0;
    };

    // The map the hero moves in: the tile flags and the map's size.
    struct MoveWorld
    {
        const std::vector<std::vector<int>> *flags = nullptr;
        int width = 0;
        int height = 0;
        bool wraps_x = false;
        int tile_size = 16;
    };

    // The numbers the rules use.
    struct MoveRules
    {
        double speed = 148.0;       // the top-down speed, unchanged
        double gravity = 900.0;      // px per second squared
        double jump_speed = 320.0;   // px per second
        double fall_speed = 420.0;   // the terminal fall speed
        double friction = 700.0;     // the ground's grip
        double air_friction = 140.0; // the air's grip
    };

    // The tile flags under a point, or zero outside the map.
    int tileAt(const MoveWorld &world, double x, double y);

    // The tile flags under a box: its corners and its edges.
    int flagsAt(const MoveWorld &world, double x, double y, double half_width,
                double half_height);

    // The top-down movement: four directions, one speed, and a probe at
    // the hero's centre. This is the field's rule, unchanged.
    void moveTopDown(Body &body, const MoveWorld &world, double ax, double ay,
                     double speed, double dt);

    // The platformer's rules: gravity, a jump with a variable height, and
    // a body that is swept per axis (the corners are what get stuck).
    // `jump_pressed` is the edge, `jump_held` the level: letting go early
    // cuts the rise.
    void movePlatformer(Body &body, const MoveWorld &world, const MoveRules &rules,
                         double ax, bool jump_pressed, bool jump_held, double dt);

} // namespace aur
