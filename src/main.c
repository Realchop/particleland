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

#include "../protocols/xdg-shell.h"
#include "../protocols/wlr_shell.h"
#include "/usr/include/cairo/cairo.h"

#include "shared_memory_boiler_plate.h"
#include "./particle.h"

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
    cairo_pattern_t *grad;
    cairo_surface_t *surface;
    cairo_t *cr;
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

void fill_buffer_from_cairo(cairo_surface_t *surface, uint32_t *data, int width, int height) {
    cairo_surface_flush(surface);
    unsigned char *cairo_data = cairo_image_surface_get_data(surface);

    for(int y=0; y<height; ++y)
    {
        for(int x=0; x<width; ++x)
        {
            uint32_t curr = 0;
            for(int o=0; o<4; ++o)
            {
               curr |= cairo_data[y * width * 4 + x*4 + o] << (8*o);
            }

            data[y * width + x] = curr;
        }
    }
}

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

    // ------------------------ CAIRO ------------------------
    // Clear surface here
    uint32_t time = state->callback_time;
    if(ani_state.previous_time == 0) ani_state.previous_time = time;
    ani_state.frames += 1;
    if(time - ani_state.previous_time >= 1000)
    {
        printf("FPS: %u\n", 1000 * ani_state.frames / (time - ani_state.previous_time));
        ani_state.previous_time = time;
        ani_state.frames = 0;
    }

    ani_state.surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ani_state.width, ani_state.height);
    ani_state.cr = cairo_create(ani_state.surface);
    cairo_set_source(ani_state.cr, ani_state.grad);

    for(int i=0; i<ani_state.len; ++i)
    {
        if(!particle_rule_circle_cutoff(2, ani_state.particles+i)) 
            particle_rule_geometric_approach(30.0, ani_state.particles+i);

        cairo_arc(ani_state.cr, ani_state.particles[i].x, ani_state.particles[i].y, ani_state.particles[i].r, 0, 2 * 3.1415);
        cairo_fill(ani_state.cr);
    }

    // Have to convert from cairo surface data to our raw buffer
    fill_buffer_from_cairo(ani_state.surface, data, ani_state.width, ani_state.height);

    cairo_destroy(ani_state.cr);
    cairo_surface_destroy(ani_state.surface);
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

    /* cairo_destroy(ani_state.cr); */
    /* cairo_surface_destroy(ani_state.surface); */
}

static const struct zwlr_layer_surface_v1_listener zwlr_layer_surface_listener = {
    .configure = zwlr_layer_surface_configure, 
    .closed =  zwlr_layer_surface_closed
};


// Move shit to ani_state init function
// Also move everything animation related out of main
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

    ani_state.grad = cairo_pattern_create_linear(0, 0, ani_state.width, ani_state.height);
    cairo_pattern_add_color_stop_rgb(ani_state.grad, 0, 1.0, (6*16+9)/255.0, (11*16+4)/255.0);
    cairo_pattern_add_color_stop_rgb(ani_state.grad, 0, 1.0, 0.0, (7*16+2)/255.0);

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

    return 0;
}

