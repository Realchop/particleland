#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>

#include <wayland-client.h>
#include "wayland_stuff.h"

#include "shared_memory_boiler_plate.h"
#include "particle.h"
#include "drawing_utils.h"

// ----------------------- WIP -----------------------
struct animation_state {
    Particle *particles;
    int len;
    int width;
    int height;
    uint32_t previous_time;
    uint32_t frames;
};

struct animation_state ani_state = { NULL };

// FIX:
// Vidi da li ovo map i unmap djubre moze da se drzi u memoriji
// Kao NEMA dobrog razloga da ne moze 
static struct wl_buffer * draw_frame(struct client_state *state) {
    // ----------------------- MEM ALLOC -----------------------
    int stride = ani_state.width * 4;
    int size = stride * ani_state.height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    uint32_t *data = mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
            ani_state.width, ani_state.height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    // ------------------------- END -------------------------

    // ------------------------ DRAW -------------------------
    uint32_t time = state->callback_time;
    if(ani_state.previous_time == 0) ani_state.previous_time = time;
    ani_state.frames += 1;
    if(time - ani_state.previous_time >= 1000)
    {
        printf("FPS: %u\n", 1000 * ani_state.frames / (time - ani_state.previous_time));
        ani_state.previous_time = time;
        ani_state.frames = 0;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(data, ani_state.width, ani_state.height, 32, stride, 0, 0, 0, 0);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);

    for(int i=0; i<ani_state.len; ++i)
    {
        if(!particle_rule_circle_cutoff(2, ani_state.particles+i)) 
            particle_rule_geometric_approach(30.0, ani_state.particles+i);

        uint32_t color = getGradientForCoordinates(0xFF69B4FF, 0XFF0072FF, 0, ani_state.particles[i].x, ani_state.particles[i].y, ani_state.width, ani_state.height); 

        filledCircleColor(renderer, ani_state.particles[i].x, ani_state.particles[i].y, ani_state.particles[i].r, color);
    }

    SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);

    munmap(data, size);
    wl_buffer_add_listener(buffer, get_wl_buffer_listener(), NULL);
    // ------------------------- END -------------------------

    return buffer;
}

int main(int argc, char *argv[]) {
    ani_state.len = 1000;
    ani_state.particles = malloc(sizeof(Particle) * ani_state.len);
    if(ani_state.particles == NULL)
    {
        fprintf(stderr, "Particles malloc!\n");
        return EXIT_FAILURE;
    }
    ani_state.width = 1920;
    ani_state.height = 1280;
    particle_multi_init(ani_state.width, ani_state.height, ani_state.len, ani_state.particles);

    ani_state.frames = 0;
    ani_state.previous_time = 0;

    // Wayland setup
    struct client_state state = {0};
    wayland_setup(&state);

    state.draw_frame = draw_frame;

    run_wayland(&state);

    SDL_Quit();

    return 0;
}

