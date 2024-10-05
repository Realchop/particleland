ifeq ($(debug),1)
  CFLAGS = -Wall -g
else
  CFLAGS = 
endif

all: main.o animation boring protocols 
	gcc -o builds/$(version) main.o drawing_utils.o particle.o wayland_stuff.o shared_memory_boiler_plate.o wlr_shell.o xdg-shell.o -lSDL2 -lSDL2_gfx -lwayland-client

main.o: 
	gcc $(CFLAGS) -c src/main.c -o main.o

animation: drawing_utils.o particle.o

drawing_utils.o:
	gcc $(CFLAGS) -c src/animation/drawing_utils.c -o drawing_utils.o

particle.o:
	gcc $(CFLAGS) -c src/animation/particle.c -o particle.o 

boring: shared_memory_boiler_plate.o wayland_stuff.o

shared_memory_boiler_plate.o:
	gcc $(CFLAGS) -c src/boring/shared_memory_boiler_plate.c -o shared_memory_boiler_plate.o

wayland_stuff.o:
	gcc $(CFLAGS) -c src/boring/wayland_stuff.c -o wayland_stuff.o

protocols: wlr_shell.o xdg-shell.o

wlr_shell.o:
	gcc $(CFLAGS) -c protocols/wlr_shell.c -o wlr_shell.o

xdg-shell.o:
	gcc $(CFLAGS) -c protocols/xdg-shell.c -o xdg-shell.o

clean: 
	rm *.o

