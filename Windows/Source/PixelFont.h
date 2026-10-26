#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>

namespace pixels
{
inline std::array<uint8_t, 7> glyph(char c)
{
    switch (c)
    {
        case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31}; case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
        case ':': return {0,4,4,0,4,4,0}; case '.': return {0,0,0,0,0,12,12};
        case '-': return {0,0,0,31,0,0,0}; case '/': return {1,2,2,4,8,8,16};
        case '+': return {0,4,4,31,4,4,0}; case '>': return {16,8,4,2,4,8,16};
        case '<': return {1,2,4,8,4,2,1}; case '?': return {14,17,1,2,4,0,4};
        case '*': return {0,21,14,31,14,21,0}; case '%': return {17,2,4,4,8,16,17};
        case '(': return {2,4,8,8,8,4,2}; case ')': return {8,4,2,2,2,4,8};
        default: return {};
    }
}
inline void text(juce::Graphics& g, juce::String value, int x, int y, int scale = 1)
{
    for (auto ch : value.toUpperCase())
    {
        auto data = glyph((char)ch);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
            if (data[(size_t)row] & (1 << (4 - col))) g.fillRect(x + col * scale, y + row * scale, scale, scale);
        x += 6 * scale;
    }
}
inline void centred(juce::Graphics& g, juce::String s, int cx, int y, int scale = 1, int maxChars = 100)
{
    s = s.substring(0, maxChars); text(g, s, cx - (s.length() * 6 - 1) * scale / 2, y, scale);
}
inline void line(juce::Graphics& g, int x0, int y0, int x1, int y1)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) { g.fillRect(x0, y0, 2, 2); if (x0 == x1 && y0 == y1) break; int e = err * 2; if (e >= dy) { err += dy; x0 += sx; } if (e <= dx) { err += dx; y0 += sy; } }
}
inline void circle(juce::Graphics& g, int cx, int cy, int r)
{
    int x = r, y = 0, error = 1 - r;
    while (x >= y)
    {
        for (auto p : {juce::Point<int>(x,y), {y,x}, {-y,x}, {-x,y}, {-x,-y}, {-y,-x}, {y,-x}, {x,-y}}) g.fillRect(cx + p.x, cy + p.y, 2, 2);
        ++y; if (error < 0) error += 2 * y + 1; else { --x; error += 2 * (y - x) + 1; }
    }
}
}
