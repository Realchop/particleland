cd src &&
gcc -g main.c particle.c shared_memory_boiler_plate.c drawing_utils.c wayland_stuff.c ../protocols/xdg-shell.c ../protocols/wlr_shell.c -l wayland-client -l SDL2 -l SDL2_gfx -o ../builds/debug &&
cd .. &&
gdb
