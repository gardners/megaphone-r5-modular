all:	tools/bomtool bin65/unicode-font-test.prg

CC65=cc65 -t c64
CL65=cl65 -t c64

tools/bomtool:	tools/bomtool.c tools/parts-library.c tools/parts-library.h
	gcc -Wall -o $@ tools/bomtool.c tools/parts-library.c

tools/showglyph:	tools/showglyph.c
	gcc -o tools/showglyph tools/showglyph.c -I/usr/include/freetype2 -lfreetype

bin65/unicode-font-test.prg:	src/telephony/unicode-font-test.c
	mkdir -p bin65
	$(CC65) -Isrc/mega65-libc/include src/telephony/unicode-font-test.c
	$(CC65) -Isrc/mega65-libc/include src/mega65-libc/src/shres.c
	$(CC65) -Isrc/mega65-libc/include src/mega65-libc/src/memory.c
	$(CC65) -Isrc/mega65-libc/include src/mega65-libc/src/hal.c
	$(CL65) -o bin65/unicode-font-test.prg -Isrc/mega65-libc/include src/telephony/unicode-font-test.s src/mega65-libc/src/shres.s src/mega65-libc/src/cc65/shres_asm.s src/mega65-libc/src/memory.s src/mega65-libc/src/cc65/memory_asm.s src/mega65-libc/src/hal.s
