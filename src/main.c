// clib
#include <stdint.h>
#include <stdio.h>

// SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>

// SDL2_gfx
#include <SDL2/SDL2_gfxPrimitives.h>

// wayland
#include <wayland-client.h>

// Local
#include "boring/wayland_stuff.h"

#include "animation/particle.h"
#include "animation/drawing_utils.h"

void animation(SDL_Renderer *renderer, SDL_Surface *surface, struct client_state *state)
{
    for(int i=0; i<state->len; ++i)
    {
        if(!particle_rule_circle_cutoff(2, state->particles+i)) 
            particle_rule_geometric_approach(30.0, state->particles+i);

        uint32_t color = getGradientForCoordinates(0xFF69B4FF, 0XFF0072FF, 0, state->particles[i].x, state->particles[i].y, state->width, state->height); 

        filledCircleColor(renderer, state->particles[i].x, state->particles[i].y, state->particles[i].r, color);
    }
}

int main(int argc, char *argv[]) {
    // Wayland setup
    struct client_state state;
    if(wayland_setup(&state) != 0)
    {
        printf("Setup failed.\n");

        return EXIT_FAILURE;
    }

    // Animation hook here
    state.animation = animation;

    run_wayland(&state);

    return 0;
}

