#!/bin/bash

SHRES=`which shres`

make fonts && sudo $SHRES /dev/sdb fonts/noto/NotoColorEmoji-Regular.ttf.MRF=NotoColorEmoji,7 \
	fonts/noto/NotoEmoji-VariableFont_wght.ttf.MRF=NotoEmoji,7 \
	fonts/noto/NotoSans-VariableFont_wdth,wght.ttf.MRF=NotoSans,7 \
	fonts/nokia-pixel-large/nokia-pixel-large.otf.MRF=Nokia\ Pixel\ Large,7
