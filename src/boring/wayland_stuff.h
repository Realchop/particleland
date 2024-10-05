#include <stdint.h>

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
    // Other
    uint32_t callback_time;
    struct wl_buffer * (*draw_frame)(struct client_state *state);
};

const struct wl_buffer_listener *get_wl_buffer_listener(void);

void wayland_setup(struct client_state *state);

void run_wayland(struct client_state *state);

