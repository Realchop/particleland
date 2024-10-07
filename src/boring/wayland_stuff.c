#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>

#include <wayland-client.h>
#include "../../protocols/wlr_shell.h"
#include "./shared_memory_boiler_plate.h"

#include "wayland_stuff.h"


// Wayland 
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

const struct wl_buffer_listener wl_buffer_listener = {
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

static struct wl_buffer * draw_frame(struct client_state *state) {
    // ----------------------- MEM ALLOC -----------------------
    int stride = state->width * 4;
    int size = stride * state->height;

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
            state->width, state->height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    // ------------------------- END -------------------------

    // ------------------------ DRAW -------------------------
    uint32_t time = state->callback_time;
    if(state->previous_time == 0) state->previous_time = time;
    state->frames += 1;
    if(time - state->previous_time >= 1000)
    {
        printf("FPS: %u\n", 1000 * state->frames / (time - state->previous_time));
        state->previous_time = time;
        state->frames = 0;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(data, state->width, state->height, 32, stride, 0, 0, 0, 0);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);

    state->animation(renderer, surface, state);

    SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);

    munmap(data, size);
    wl_buffer_add_listener(buffer, get_wl_buffer_listener(), NULL);
    // ------------------------- END -------------------------

    return buffer;
}

static const struct wl_callback_listener wl_surface_frame_listener;

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

// ZSHELL 
static void zwlr_layer_surface_configure(void* data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    struct client_state *state = data;
    zwlr_layer_surface_v1_ack_configure(surface, serial);

    struct wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(state->wl_surface);
}

static void zwlr_layer_surface_closed() {}

static const struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
    .configure = zwlr_layer_surface_configure, 
    .closed =  zwlr_layer_surface_closed
};

// New
int wayland_setup(struct client_state *state) {
    state->width = 1920;
    state->height = 1280;
    
    state->len = 1000;
    state->particles = malloc(sizeof(Particle) * state->len);
    if(state->particles == NULL)
    {
        fprintf(stderr, "Particles malloc!\n");
        return EXIT_FAILURE;
    }
    particle_multi_init(state->width, state->height, state->len, state->particles);

    state->frames = 0;
    state->previous_time = 0;
    
    state->wl_display = wl_display_connect(NULL);
    state->wl_registry = wl_display_get_registry(state->wl_display);
    wl_registry_add_listener(state->wl_registry, &wl_registry_listener, state);
    wl_display_roundtrip(state->wl_display);

    // Surface setup
    state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
    state->zsurface = zwlr_layer_shell_v1_get_layer_surface(state->zshell, state->wl_surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "particleland");
    
    zwlr_layer_surface_v1_set_anchor(state->zsurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_add_listener(state->zsurface, &zwlr_layer_surface_listener, state);

    struct wl_callback *cb = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

    // Surface draw
    wl_surface_commit(state->wl_surface);

    return 0;
}

void run_wayland(struct client_state *state) {
    // Event loop
    while (wl_display_dispatch(state->wl_display)) {};
}

const struct wl_buffer_listener *get_wl_buffer_listener(void) {
    return &wl_buffer_listener;
}

