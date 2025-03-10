all:	tools/bomtool

tools/bomtool:	tools/bomtool.c tools/parts-library.c tools/parts-library.h
	gcc -Wall -o $@ tools/bomtool.c tools/parts-library.c
