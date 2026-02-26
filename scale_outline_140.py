#!/usr/bin/env python3
"""
Scale outline bitmap from 120x120 to 140x140
"""
from PIL import Image
import sys

def bitmap_to_c_array(img, name="outline_bitmap"):
    """Convert 1-bit PIL image to C array format (MSB first)"""
    width, height = img.size
    bytes_per_row = (width + 7) // 8

    # Convert to 1-bit if not already
    if img.mode != '1':
        img = img.convert('1')

    # Generate byte array
    byte_array = []
    for y in range(height):
        row_bytes = []
        for byte_x in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width:
                    # Get pixel (0 = black, 255 = white in PIL)
                    # We want 1 = black in output
                    pixel = img.getpixel((x, y))
                    if pixel == 0:  # Black pixel
                        byte_val |= (1 << (7 - bit))  # MSB first
            row_bytes.append(byte_val)
        byte_array.extend(row_bytes)

    # Format as C array
    lines = []
    lines.append(f"// {width}x{height} Terrain outline bitmap (coastline and markers)")
    lines.append(f"// 1-bit bitmap: 0 = draw outline, 1 = transparent")
    lines.append(f"const unsigned char {name}[] PROGMEM = {{ /* 0X00,0X01,0X8C,0X00,0X8C,0X00, */")

    # Output in rows of 16 bytes for readability
    for i in range(0, len(byte_array), 16):
        chunk = byte_array[i:i+16]
        hex_vals = ','.join(f'0X{b:02X}' for b in chunk)
        lines.append(hex_vals + ',')

    lines.append("};")
    return '\n'.join(lines)

def main():
    print("Scaling outline bitmap from 120x120 to 140x140...")

    # Read the original 120x120 bitmap
    try:
        img = Image.open("outline_bitmap_120.bmp")
        print(f"Loaded original image: {img.size}, mode: {img.mode}")
    except FileNotFoundError:
        print("Error: outline_bitmap_120.bmp not found!")
        print("Please save the 120x120 outline bitmap as 'outline_bitmap_120.bmp' first")
        return 1

    # Convert to 1-bit if needed
    if img.mode != '1':
        img = img.convert('1')

    # Scale to 140x140 using nearest neighbor (preserves sharp edges)
    img_scaled = img.resize((140, 140), Image.Resampling.NEAREST)
    print(f"Scaled to: {img_scaled.size}")

    # Save as BMP for verification
    img_scaled.save("outline_bitmap_140.bmp")
    print("Saved scaled bitmap as outline_bitmap_140.bmp")

    # Generate C array
    c_code = bitmap_to_c_array(img_scaled, "outline_bitmap")

    # Write to header file
    with open("Bitmaps_140.h", 'w') as f:
        f.write("#ifndef BITMAPS_H\n")
        f.write("#define BITMAPS_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(c_code)
        f.write("\n\n#endif // BITMAPS_H\n")

    print("Generated Bitmaps_140.h")
    print(f"Total size: {(140 * 140) // 8} bytes ({((140 + 7) // 8) * 140} with padding)")

    return 0

if __name__ == "__main__":
    sys.exit(main())
