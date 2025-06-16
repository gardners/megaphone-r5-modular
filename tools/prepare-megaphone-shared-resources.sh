#!/bin/bash

SHRES=`which shres`

make fonts && sudo $SHRES /dev/sdb fonts/twemoji/twemoji.MRF=EmojiColour,0,1,2 \
	fonts/noto/NotoEmoji-VariableFont_wght.ttf.MRF=EmojiMono,0,1,2 \
	fonts/noto/NotoSans-VariableFont_wdth,wght.ttf.MRF=Sans,0,1,2 \
	fonts/nokia-pixel-large/nokia-pixel-large.otf.MRF=UI,0,1,2

sudo blockdev --flushbufs /dev/sdb
