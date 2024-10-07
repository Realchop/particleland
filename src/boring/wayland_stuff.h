#include <stdint.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>

#include "../animation/particle.h"

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
    // Window
    int width;
    int height;
    // Animation stuff
    Particle *particles;
    int len;
    uint32_t previous_time;
    int frames;
    void (*animation)(SDL_Renderer *renderer, SDL_Surface *surface, struct client_state *state);
    // Other
    uint32_t callback_time;
};

const struct wl_buffer_listener *get_wl_buffer_listener(void);

int wayland_setup(struct client_state *state);

void run_wayland(struct client_state *state);

