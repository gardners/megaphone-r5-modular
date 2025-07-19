all:	tools/bomtool bin65/unicode-font-test.prg $(FONTS)

CC65=cc65 -t c64
CL65=cl65 -t c64

FONTS=fonts/twemoji/twemoji.MRF \
	fonts/noto/NotoEmoji-VariableFont_wght.ttf.MRF \
	fonts/noto/NotoSans-VariableFont_wdth,wght.ttf.MRF \
	fonts/nokia-pixel-large/nokia-pixel-large.otf.MRF

fonts/noto/NotoColorEmoji-Regular.ttf:
	echo "Read fonts/noto/README.txt"

fonts/twemoji/twemoji.MRF:
	python3 tools/twemoji2mega65font.py fonts/twemoji/assets/svg/ fonts/twemoji/twemoji.MRF

%.otf.MRF:	%.otf tools/showglyph
	tools/showglyph $<

%.ttf.MRF:	%.ttf tools/showglyph
	tools/showglyph $<

fonts:	$(FONTS)


tools/bomtool:	tools/bomtool.c tools/parts-library.c tools/parts-library.h
	gcc -Wall -o $@ tools/bomtool.c tools/parts-library.c

tools/showglyph:	tools/showglyph.c
	gcc -o tools/showglyph tools/showglyph.c -I/usr/include/freetype2 -lfreetype

tools/gen_attr_tables:	tools/gen_attr_tables.c
	gcc -o tools/gen_attr_tables tools/gen_attr_tables.c

src/telephony/attr_tables.c:	tools/gen_attr_tables
	$< > $@

src/telephony/linux/provision:	src/telephony/d81.c src/telephony/provision.c src/telephony/linux/includes.h src/telephony/linux/hal.c
	gcc -Wall -g -o $@ src/telephony/d81.c src/telephony/provision.c src/telephony/linux/includes.h src/telephony/linux/hal.c

bin65/unicode-font-test.prg:	src/telephony/unicode-font-test.c src/telephony/attr_tables.c
	mkdir -p bin65
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/telephony/unicode-font-test.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/telephony/attr_tables.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/shres.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/memory.c
	$(CC65) -Iinclude -Isrc/mega65-libc/include src/mega65-libc/src/hal.c
	$(CL65) -o bin65/unicode-font-test.prg -Iinclude -Isrc/mega65-libc/include src/telephony/unicode-font-test.s src/telephony/attr_tables.s src/mega65-libc/src/shres.s src/mega65-libc/src/cc65/shres_asm.s src/mega65-libc/src/memory.s src/mega65-libc/src/cc65/memory_asm.s src/mega65-libc/src/hal.s

