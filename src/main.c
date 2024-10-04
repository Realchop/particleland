#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
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

#include <wayland-client.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#include "../protocols/xdg-shell.h"
#include "../protocols/wlr_shell.h"

#include "shared_memory_boiler_plate.h"
#include "particle.h"
#include "drawing_utils.h"

// ----------------------- WAYLAND -----------------------
struct client_state {
    /* Globals */
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    /* Objects */
    struct wl_surface *wl_surface;
    // z-shell
    struct zwlr_layer_shell_v1 *zshell;
    struct zwlr_layer_surface_v1 *zsurface;
    //
    uint32_t callback_time;
};

struct animation_state {
    Particle *particles;
    int len;
    int width;
    int height;
    uint32_t previous_time;
    uint32_t frames;
};

struct animation_state ani_state = { NULL };

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

static void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
    struct client_state *state = data;
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(
                wl_registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = wl_registry_bind(
                wl_registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->zshell = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, 5);
    }
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
    /* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// ------------------------- DRAW ------------------------
static const struct wl_callback_listener wl_surface_frame_listener;

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
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    // ------------------------- END -------------------------

    return buffer;
}

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    wl_callback_destroy(cb);

    struct client_state *state = data;
    cb = wl_surface_frame(state->wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

    state->callback_time = time;

    struct wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(state->wl_surface);
}

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

// ------------------------ ZSHELL -----------------------
static void zwlr_layer_surface_configure(void* data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    struct client_state *state = data;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
   
    struct wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);
}

static void zwlr_layer_surface_closed() {
    if(ani_state.particles != NULL)
        free(ani_state.particles);
}

static const struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
    .configure = zwlr_layer_surface_configure, 
    .closed =  zwlr_layer_surface_closed
};


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
    struct client_state state = { 0 };
    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    // Surface setup
    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.zsurface = zwlr_layer_shell_v1_get_layer_surface(state.zshell, state.wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "particleland");
    zwlr_layer_surface_v1_set_anchor(state.zsurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_add_listener(state.zsurface, &zwlr_layer_surface_listener, &state);

    struct wl_callback *cb = wl_surface_frame(state.wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);

    // Surface draw
    wl_surface_commit(state.wl_surface);

    // Event loop
    while (wl_display_dispatch(state.wl_display)) {};

    SDL_Quit();

    return 0;
}

