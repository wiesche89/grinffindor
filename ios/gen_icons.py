#!/usr/bin/env python3
"""Generates all required iOS app icon sizes from a source image."""
from PIL import Image

src  = "media/images/Image_9_ios_logo.png"
dest = "ios/Assets.xcassets/AppIcon.appiconset"
sizes = [20, 29, 40, 58, 60, 76, 80, 87, 120, 152, 167, 180, 1024]

img = Image.open(src).convert("RGBA")
w, h = img.size
square = min(w, h)
left = (w - square) // 2
top  = (h - square) // 2
img = img.crop((left, top, left + square, top + square))

bg = Image.new("RGB", img.size, (255, 255, 255))
bg.paste(img, mask=img.split()[3])

for size in sizes:
    out = bg.resize((size, size), Image.LANCZOS)
    out.save(f"{dest}/AppIcon-{size}.png")
    print(f"  {size}x{size} -> AppIcon-{size}.png")
