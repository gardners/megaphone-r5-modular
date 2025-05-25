all:	tools/bomtool

tools/bomtool:	tools/bomtool.c tools/parts-library.c tools/parts-library.h
	gcc -Wall -o $@ tools/bomtool.c tools/parts-library.c

tools/showglyph:	tools/showglyph.c
	gcc -o tools/showglyph tools/showglyph.c -I/usr/include/freetype2 -lfreetype
