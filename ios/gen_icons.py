#!/usr/bin/env python3
"""Generates the 1024x1024 iOS app icon from a source image."""
from PIL import Image

src  = "media/images/Image_9_ios_logo.png"
dest = "ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png"

img = Image.open(src).convert("RGBA")
w, h = img.size
square = min(w, h)
left = (w - square) // 2
top  = (h - square) // 2
img = img.crop((left, top, left + square, top + square))

bg = Image.new("RGB", img.size, (255, 255, 255))
bg.paste(img, mask=img.split()[3])
bg = bg.resize((1024, 1024), Image.LANCZOS)
bg.save(dest)
print(f"AppIcon-1024.png: mode={bg.mode}, size={bg.size}")
