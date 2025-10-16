# Removes blackspace in images

import os
from PIL import Image

def trim_black(image: Image.Image, tolerance: int = 30) -> Image.Image:
    img = image.convert("RGBA")
    datas = img.getdata()

    new_data = []
    for item in datas:
        r, g, b, a = item
        if r <= tolerance and g <= tolerance and b <= tolerance:
            new_data.append((0, 0, 0, 0))
        else:
            new_data.append((r, g, b, a))

    img.putdata(new_data)
    return img 

def process_folder(folder_path: str, tolerance: int = 30):
    for filename in os.listdir(folder_path):
        if filename.lower().endswith(".png"):
            filepath = os.path.join(folder_path, filename)
            print(f"Processing {filename}...")

            img = Image.open(filepath)
            trimmed = trim_black(img, tolerance)

            trimmed.save(filepath, "PNG")

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # go up one level
    folder = os.path.join(base_dir, "StratagemIcons")
    if os.path.exists(folder):
        process_folder(folder, tolerance=30)  # tolerance here
        print("Finished trimming blackspace from all PNGs.")
    else:
        print("Folder 'StratagemIcons' not found.")
