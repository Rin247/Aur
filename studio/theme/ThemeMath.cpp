#include "theme/ThemeMath.hpp"

#include <algorithm>
#include <cmath>

namespace aur::theme
{
    namespace
    {
        int hex_digit(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        double linear(double v)
        {
            return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        }

        struct Hsv
        {
            int h;
            int s;
            int v;
        };

        Hsv to_hsv(Rgb c)
        {
            const int mx = std::max({c.r, c.g, c.b});
            const int mn = std::min({c.r, c.g, c.b});
            const int delta = mx - mn;
            int h = 0;
            if (delta != 0)
            {
                if (mx == c.r) h = 60 * (c.g - c.b) / delta;
                else if (mx == c.g) h = 120 + 60 * (c.b - c.r) / delta;
                else h = 240 + 60 * (c.r - c.g) / delta;
                if (h < 0) h += 360;
            }
            return Hsv{h, mx == 0 ? 0 : delta * 255 / mx, mx};
        }

        Rgb from_hsv(Hsv c)
        {
            const double s = c.s / 255.0;
            const double v = c.v / 255.0;
            const double chroma = v * s;
            const double sector = (c.h % 360) / 60.0;
            const double x = chroma * (1.0 - std::fabs(std::fmod(sector, 2.0) - 1.0));
            double r = 0, g = 0, b = 0;
            switch (static_cast<int>(sector))
            {
            case 0: r = chroma; g = x; break;
            case 1: r = x; g = chroma; break;
            case 2: g = chroma; b = x; break;
            case 3: g = x; b = chroma; break;
            case 4: r = x; b = chroma; break;
            default: r = chroma; b = x; break;
            }
            const double m = v - chroma;
            auto byte = [m](double ch) {
                return static_cast<std::uint8_t>(std::lround((ch + m) * 255.0));
            };
            return Rgb{byte(r), byte(g), byte(b)};
        }
    }

    bool parse_hex(std::string_view text, Rgb &out)
    {
        if (text.size() != 7 || text[0] != '#') return false;
        int value = 0;
        for (std::size_t i = 1; i < 7; ++i)
        {
            const int d = hex_digit(text[i]);
            if (d < 0) return false;
            value = value * 16 + d;
        }
        out = rgb_from_int(static_cast<std::uint32_t>(value));
        return true;
    }

    std::string to_hex(Rgb c)
    {
        static const char digits[] = "0123456789abcdef";
        std::string s = "#";
        for (std::uint8_t byte : {c.r, c.g, c.b})
        {
            s += digits[byte >> 4];
            s += digits[byte & 15];
        }
        return s;
    }

    double relative_luminance(Rgb c)
    {
        return 0.2126 * linear(c.r / 255.0) + 0.7152 * linear(c.g / 255.0) +
               0.0722 * linear(c.b / 255.0);
    }

    double contrast_ratio(Rgb a, Rgb b)
    {
        const double la = relative_luminance(a);
        const double lb = relative_luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }

    Rgb on_accent(Rgb primary)
    {
        const Rgb near_black = rgb_from_int(0x0f0f1a);
        const double l = relative_luminance(primary);
        const double white = 1.05 / (l + 0.05);
        const double black = (l + 0.05) / (relative_luminance(near_black) + 0.05);
        return white >= black ? Rgb{255, 255, 255} : near_black;
    }

    Rgb mix(Rgb from, Rgb toward, double amount)
    {
        auto channel = [amount](int a, int b) {
            return static_cast<std::uint8_t>(std::lround(a + (b - a) * amount));
        };
        return Rgb{channel(from.r, toward.r), channel(from.g, toward.g), channel(from.b, toward.b)};
    }

    Rgb lighter(Rgb c, int percent)
    {
        Hsv hsv = to_hsv(c);
        hsv.v = hsv.v * percent / 100;
        if (hsv.v > 255)
        {
            hsv.s = std::max(0, hsv.s - (hsv.v - 255));
            hsv.v = 255;
        }
        return from_hsv(hsv);
    }

    Rgb darker(Rgb c, int percent)
    {
        Hsv hsv = to_hsv(c);
        hsv.v = hsv.v * 100 / percent;
        return from_hsv(hsv);
    }

    bool is_dark_background(Rgb background)
    {
        return relative_luminance(background) < 0.18;
    }

}
