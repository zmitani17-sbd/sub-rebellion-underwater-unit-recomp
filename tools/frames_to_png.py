"""Convert the raw RGBA frames written by PS2X_FRAME_DUMP into PNGs.

    python3 tools/frames_to_png.py run/frames

File names look like frame_00391_512x448_fbp112.rgba (counter, size, framebuffer page).
Needs Pillow (pip install pillow).
"""
import glob
import os
import sys

from PIL import Image

folder = sys.argv[1] if len(sys.argv) > 1 else "run/frames"
for path in sorted(glob.glob(os.path.join(folder, "*.rgba"))):
    width, height = map(int, os.path.basename(path).split("_")[2].split("x"))
    data = open(path, "rb").read()
    image = Image.frombytes("RGBA", (width, height), data[: width * height * 4]).convert("RGB")
    image.save(path[: -len(".rgba")] + ".png")
    print("wrote", path[: -len(".rgba")] + ".png", "bbox:", image.getbbox())
