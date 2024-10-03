if [ $# -lt 1 ]; then
 echo "Missing 1 positional arguments: version"
 exit 1
fi

mkdir -p builds &&
cd src &&
gcc -g3 main.c particle.c shared_memory_boiler_plate.c ../protocols/xdg-shell.c ../protocols/wlr_shell.c -l wayland-client -l SDL2 -l SDL2_gfx -o ../builds/$1 &&
cd .. &&
./builds/$1
