#include "drawing_utils.h"

// TODO: Implement directional calculation
// NOTE: Returns colors in RRBBGGAA format but expects them in AAGGBBRR due to a sdl2_gfx issue
unsigned int getGradientForCoordinates(unsigned int start, unsigned int end, Directions dir, int x, int y, int width, int height)
{
    int r_a, g_a, b_a, a_a;
    r_a = (start & (255 << 24)) >> 24;
    g_a = (start & (255 << 16)) >> 16;
    b_a = (start & (255 << 8)) >> 8;
    a_a = start & (255);

    int r_b, g_b, b_b, a_b;
    r_b = (end & (255 << 24)) >> 24;
    g_b = (end & (255 << 16)) >> 16;
    b_b = (end & (255 << 8)) >> 8;
    a_b = end & (255);

    int r, g, b, a;
    double tx, ty;

    tx = (double) x / width;
    ty = (double) y / height;

    r = r_a + (r_b - r_a) * tx;
    g = g_a + (g_b - g_a) * tx;
    b = b_a + (b_b - b_a) * tx;
    a = a_a + (a_b - a_a) * tx;

    unsigned int final_r, final_g, final_b, final_a;
    final_r = r;
    final_g = g << 8;
    final_b = b << 16;
    final_a = a << 24;

    return final_r | final_g | final_b | final_a;
}

