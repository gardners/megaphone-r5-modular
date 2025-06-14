#!/bin/bash

SHRES=`which shres`

make fonts && sudo $SHRES /dev/sdb fonts/noto/NotoColorEmoji-Regular.ttf.MRF=NotoColorEmoji,0,1,2 \
	fonts/noto/NotoEmoji-VariableFont_wght.ttf.MRF=NotoEmoji,0,1,2 \
	fonts/noto/NotoSans-VariableFont_wdth,wght.ttf.MRF=NotoSans,0,1,2 \
	fonts/nokia-pixel-large/nokia-pixel-large.otf.MRF=Nokia\ Pixel\ Large,0,1,2

sudo blockdev --flushbufs /dev/sdb
