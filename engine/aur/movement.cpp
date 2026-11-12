#include "movement.hpp"

#include "project.hpp"

#include <algorithm>
#include <cmath>

namespace aur
{

    namespace
    {
        // The one-way platform: solid from above, open from below. The
        // flag is invisible to the top-down field, which reads SOLID and
        // WATER only.
        constexpr int kOneWay = 64;

        double approach(double value, double target, double step)
        {
            if (value < target)
                return std::min(value + step, target);
            if (value > target)
                return std::max(value - step, target);
            return target;
        }
    } // namespace

    int tileAt(const MoveWorld &world, double x, double y)
    {
        if (!world.flags || world.flags->empty())
            return 0;
        int tx = static_cast<int>(std::floor(x / world.tile_size));
        const int ty = static_cast<int>(std::floor(y / world.tile_size));
        if (ty < 0 || ty >= world.height)
            return SOLID;
        if (tx < 0 || tx >= world.width)
            return world.wraps_x ? 0 : SOLID;
        const auto& row = (*world.flags)[static_cast<size_t>(ty)];
        if (tx < 0 || tx >= static_cast<int>(row.size()))
            return SOLID;
        return row[static_cast<size_t>(tx)];
    }

    int flagsAt(const MoveWorld &world, double x, double y, double half_width,
                double half_height)
    {
        int flags = 0;
        const double xs[2] = {x - half_width, x + half_width};
        const double ys[2] = {y - half_height, y + half_height};
        for (int ix = 0; ix < 2; ++ix)
        {
            for (int iy = 0; iy < 2; ++iy)
            {
                flags |= tileAt(world, xs[ix], ys[iy]);
            }
        }
        return flags;
    }

    void moveTopDown(Body &body, const MoveWorld &world, double ax, double ay,
                     double speed, double dt)
    {
        const double len = std::hypot(ax, ay);
        if (len <= 0)
            return;
        const double nx = body.x + ax / len * speed * dt;
        if (!(tileAt(world, nx, body.y) & (SOLID | WATER)))
            body.x = nx;
        const double ny = body.y + ay / len * speed * dt;
        if (!(tileAt(world, body.x, ny) & (SOLID | WATER)))
            body.y = ny;
    }

    void movePlatformer(Body &body, const MoveWorld &world, const MoveRules &rules,
                         double ax, bool jump_pressed, bool jump_held, double dt)
    {
        // The horizontal: toward the target speed, with the ground's grip.
        const double target = ax * rules.speed;
        const double grip = (body.on_ground ? rules.friction : rules.air_friction) * dt;
        body.vx = approach(body.vx, target, grip);

        // The jump: the edge starts it, and letting go early cuts the rise.
        if (jump_pressed && body.on_ground)
        {
            body.vy = -rules.jump_speed;
            body.on_ground = false;
        }
        if (!jump_held && body.vy < 0)
            body.vy += rules.gravity * dt;
        body.vy = std::min(body.vy + rules.gravity * dt, rules.fall_speed);

        // The x, swept: the body stops at a wall rather than entering it.
        const double nx = body.x + body.vx * dt;
        if (flagsAt(world, nx, body.y, body.half_width, body.half_height) & SOLID)
            body.vx = 0;
        else
            body.x = nx;

        // The y, swept: the floor stops the fall, and a one-way platform
        // stops it only when the body is coming down and its feet were
        // above the platform.
        const double ny = body.y + body.vy * dt;
        const bool falling = body.vy >= 0;
        const int next_feet = falling ? tileAt(world, body.x, ny + body.half_height) : 0;
        const bool one_way_blocks = falling && (next_feet & kOneWay) &&
                                    !(body.feet_flags & kOneWay);
        const bool blocked = (flagsAt(world, body.x, ny, body.half_width,
                                      body.half_height) & SOLID) ||
                             one_way_blocks;
        if (blocked)
        {
            body.vy = 0;
            body.on_ground = true;
        }
        else
        {
            body.y = ny;
            body.on_ground = false;
        }
        body.feet_flags = tileAt(world, body.x, body.y + body.half_height);
    }

} // namespace aur
