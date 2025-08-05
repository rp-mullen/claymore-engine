#pragma once
/*
 * packrect.h - Very simple rectangle packing utility sufficient for particle sprite atlas.
 * NOTE: This is *not* an optimal bin-pack implementation. It simply places rectangles sequentially
 * in rows. For production use you should replace with a more efficient algorithm.
 */
#include <cstdint>
#include <array>

struct Pack2D
{
    uint16_t m_x = 0;
    uint16_t m_y = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
};

// Simple free-rectangle packer using fixed grid.
// It keeps a cursor position and advances.

template<uint16_t MaxFreeRects>
class RectPack2DT
{
public:
    RectPack2DT(uint16_t atlasWidth, uint16_t atlasHeight)
        : m_atlasWidth(atlasWidth)
        , m_atlasHeight(atlasHeight)
    {
        m_cursorX = 0; m_cursorY = 0; m_rowHeight = 0;
    }

    bool find(uint16_t width, uint16_t height, Pack2D& out)
    {
        // new row if not enough space
        if (m_cursorX + width > m_atlasWidth)
        {
            m_cursorX = 0;
            m_cursorY += m_rowHeight;
            m_rowHeight = 0;
        }
        if (m_cursorY + height > m_atlasHeight)
            return false; // no space

        out.m_x = m_cursorX;
        out.m_y = m_cursorY;
        out.m_width = width;
        out.m_height = height;

        m_cursorX += width;
        if (height > m_rowHeight) m_rowHeight = height;

        return true;
    }

    void clear(const Pack2D& /*pack*/)
    {
        // No-op for simplistic implementation (memory not reclaimed).
    }

private:
    uint16_t m_atlasWidth;
    uint16_t m_atlasHeight;

    uint16_t m_cursorX = 0;
    uint16_t m_cursorY = 0;
    uint16_t m_rowHeight = 0;
};
