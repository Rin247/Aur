#include "mini_test.hpp"
#include "world/HdSurface.hpp"
#include "aur/project.hpp"

#include <cstdint>
#include <iostream>

void test_world_surface_checksum()
{
    aur::Project project;
    const aur::Json map = aur::generate_world_map(
        "checksum_world", "Checksum World", 32, 16, 24681357, "continents", 50, 60, 10);
    const QImage image = aur::world::build_hd_surface(map, &project, QRect());
    ASSERT(!image.isNull());
    ASSERT(image.format() == QImage::Format_RGB32);
    ASSERT(image.size() == QSize(1024, 512));

    std::uint64_t checksum = 14695981039346656037ull;
    const qsizetype byte_count = image.sizeInBytes();
    const uchar *bytes = image.constBits();
    for (qsizetype index = 0; index < byte_count; ++index)
    {
        checksum ^= bytes[index];
        checksum *= 1099511628211ull;
    }
    ASSERT(checksum == 7986464436153998528ull);
}
