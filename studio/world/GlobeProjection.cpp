#include "world/GlobeProjection.hpp"

#include <algorithm>
#include <cmath>

namespace aur::world
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
}

GlobePoint projectToGlobe(double nx, double ny, double yaw, double pitch)
{
    GlobePoint point;
    const double r2 = nx * nx + ny * ny;
    if (r2 > 1.0) return point;
    const double nz = std::sqrt(std::max(0.0, 1.0 - r2));

    // Undo the view rotation: pitch about X, then yaw about Y.
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double y1 = ny * cp + nz * sp;
    const double z1 = -ny * sp + nz * cp;
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);
    const double ox = nx * cy - z1 * sy;
    const double oz = nx * sy + z1 * cy;
    const double oy = std::clamp(y1, -1.0, 1.0);

    const double lat = std::asin(oy);
    const double lon = std::atan2(ox, oz);
    double u = lon / (2.0 * kPi) + 0.5;
    if (u < 0.0) u += 1.0;
    if (u >= 1.0) u -= 1.0;
    point.visible = true;
    point.u = u;
    point.v = 0.5 - lat / kPi;
    return point;
}

double globeShade(double nx, double ny)
{
    const double r2 = nx * nx + ny * ny;
    if (r2 > 1.0) return 0.0;
    const double nz = std::sqrt(std::max(0.0, 1.0 - r2));
    return 0.45 + 0.55 * nz;
}

} // namespace aur::world
