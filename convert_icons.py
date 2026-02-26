#!/usr/bin/env python3
"""Convert BMP icons to C arrays for embedded display"""

from PIL import Image
import os

def bmp_to_c_array(bmp_path, array_name):
    """Convert a BMP to a 1-bit C array"""
    img = Image.open(bmp_path).convert('1')  # Convert to 1-bit monochrome
    width, height = img.size
    pixels = list(img.getdata())

    # Pack pixels into bytes (8 pixels per byte, MSB first)
    bytes_data = []
    for y in range(height):
        for x in range(0, width, 8):
            byte = 0
            for bit in range(8):
                if x + bit < width:
                    pixel_index = y * width + x + bit
                    if pixels[pixel_index] == 0:  # Black pixel
                        byte |= (1 << (7 - bit))
            bytes_data.append(byte)

    # Generate C array
    output = f"// {os.path.basename(bmp_path)} - {width}x{height}\n"
    output += f"const unsigned char {array_name}[] PROGMEM = {{\n"

    for i, byte in enumerate(bytes_data):
        if i % 12 == 0:
            output += "    "
        output += f"0x{byte:02x}"
        if i < len(bytes_data) - 1:
            output += ", "
        if (i + 1) % 12 == 0 and i < len(bytes_data) - 1:
            output += "\n"

    output += "\n};\n"
    return output, width, height

def main():
    icons_dir = "data/icons"
    output_file = "lib/WeatherModule/WeatherIcons.h"

    icons = {
        "clear.bmp": "icon_clear",
        "cloudy.bmp": "icon_cloudy",
        "rainy.bmp": "icon_rainy",
        "fog.bmp": "icon_fog",
        "snow.bmp": "icon_snow",
        "forecast_clear.bmp": "icon_forecast_clear",
        "forecast_rain.bmp": "icon_forecast_rain"
    }

    header = """#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <Arduino.h>

"""

    footer = "\n#endif // WEATHER_ICONS_H\n"

    arrays = []
    for filename, array_name in icons.items():
        bmp_path = os.path.join(icons_dir, filename)
        if os.path.exists(bmp_path):
            array_code, width, height = bmp_to_c_array(bmp_path, array_name)
            arrays.append(array_code)
            print(f"Converted {filename}: {width}x{height}")
        else:
            print(f"Warning: {bmp_path} not found")

    with open(output_file, 'w') as f:
        f.write(header)
        f.write('\n'.join(arrays))
        f.write(footer)

    print(f"\nGenerated {output_file}")

if __name__ == "__main__":
    main()
