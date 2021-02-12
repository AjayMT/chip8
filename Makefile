chip8: chip8.c
	cc -o chip8 chip8.c `sdl2-config --cflags --libs`

.PHONY: clean
clean:
	rm -fr chip8
