#!/usr/bin/env python3
import os
import sys
import io
from PIL import Image
import cairosvg

OUTPUT_WIDTH = 16
OUTPUT_HEIGHT = 16

def dump_bytes(label, data):
    print(f"{label}:")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hexs = ' '.join(f"{b:02X}" for b in chunk)
        print(f"{i:04X}: {hexs}")

def record_glyph(codepoint, data, outfile):
    outfile.seek(codepoint << 8)
    outfile.write(bytes(data))

def select_best_pixel_index(data, is_color):
    candidates = [
        data[254],   # 0: pixel to the left
        data[127],   # 1: pixel above
        0x00,        # 2: black
        0xFF         # 3: white
    ]
    target = data[255]  # This is the pixel that must be replaced
    best_index = 0
    best_error = float('inf')

    for i, val in enumerate(candidates):
        err = calc_colour_error(is_color, val, target)
        if err < best_error:
            best_index = i
            best_error = err

    return best_index

def calc_colour_error(is_color, a, b):
    if not is_color:
        return abs(a - b)

    # Expand 3:3:2 format into full RGB for distance comparison
    def expand(v):
        r = (v >> 5) & 0x07
        g = (v >> 2) & 0x07
        b = v & 0x03
        return (r << 5, g << 5, b << 6)

    ar, ag, ab = expand(a)
    br, bg, bb = expand(b)
    return (ar - br) ** 2 + (ag - bg) ** 2 + (ab - bb) ** 2

def pack_into_tiles(pixel_data):
    data = [0] * 256
    for y in range(16):
        even_row = (y % 2 == 0)
        row_in_tile = y // 2
        for x in range(16):
            col_in_tile = x % 8
            if x < 8:
                if even_row:
                    # Tile 0
                    tile_base = 0
                else:
                    # Tile 1
                    tile_base = 64
            else:
                if even_row:
                    # Tile 2
                    tile_base = 128
                else:
                    # Tile 3
                    tile_base = 192

            offset = tile_base + row_in_tile * 8 + col_in_tile
            data[offset] = pixel_data[y][x]
    return data
    
    
def intensity_to_ascii(value):
    ramp = " .:-=+*#%@"
    index = (value * (len(ramp) - 1)) // 256
    return ramp[min(index, len(ramp) - 1)]

def render_svg_to_glyph(svg_path):
    png_data = cairosvg.svg2png(url=svg_path, output_width=OUTPUT_WIDTH, output_height=OUTPUT_HEIGHT)
    img = Image.open(io.BytesIO(png_data)).convert("RGBA")

    pixel_data = [[0] * OUTPUT_WIDTH for _ in range(OUTPUT_HEIGHT)]
    for y in range(OUTPUT_HEIGHT):
        for x in range(OUTPUT_WIDTH):
            r, g, b, a = img.getpixel((x, y))
            if a < 16:
                continue
            r >>= 5  # 3 bits
            g >>= 5  # 3 bits
            b >>= 6  # 2 bits
            value = (r << 5) | (g << 2) | b
            if value == 0:
                value = 1
            pixel_data[y][x] = value

    # Flatten into 256 bytes
    packed = pack_into_tiles(pixel_data)

    # Add glyph flags: 0x80 (color) + 0x0F (width=16px-1)
    glyph_width = img.width
    if glyph_width > 32:
        print(f"Warning: glyph width {glyph_width}px exceeds 32, truncating.")
        glyph_width = 32


    pixel_selector = select_best_pixel_index(packed, 1)
        
    flags = (
        0x80 |                          # Bit 7: color
        ((glyph_width - 1) & 0x1F) |    # Bits 0–4: width - 1
        (pixel_selector << 5)          # Bits 5–6: selector
    )

    packed[255] = flags

    # Debug output
    codepoint_label = os.path.basename(svg_path).split('.')[0].upper()
    print(f"\nU+{codepoint_label}: width=16px")
    for row in pixel_data:
        print("".join(intensity_to_ascii(px) for px in row))
    dump_bytes("Packed glyph", packed)

    return int(codepoint_label, 16), packed

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <twemoji_svg_folder> <output_file.mrf>", file=sys.stderr)
        sys.exit(1)

    svg_folder = sys.argv[1]
    out_path = sys.argv[2]

    with open(out_path, "wb") as outfile:
        for fname in sorted(os.listdir(svg_folder)):
            if not fname.endswith(".svg") or "-" in fname:
                continue  # Skip composite/multi-codepoint emojis
            try:
                codepoint_hex = fname.split(".")[0]
                codepoint = int(codepoint_hex, 16)
                svg_path = os.path.join(svg_folder, fname)
                cp, data = render_svg_to_glyph(svg_path)
                record_glyph(cp, data, outfile)
            except Exception as e:
                print(f"Skipping {fname}: {e}")

if __name__ == "__main__":
    main()

