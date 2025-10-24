import os
import xml.etree.ElementTree as ET
from xml.dom import minidom

# Paths
base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # go up one level
qrc_file = os.path.join(base_dir, "resources.qrc")
icons_folder = os.path.join(base_dir, "StratagemIcons")

# Parse existing .qrc
tree = ET.parse(qrc_file)
root = tree.getroot()

# Find or create the /thumbs <qresource> element
thumbs = None
for qresource in root.findall("qresource"):
    if qresource.attrib.get("prefix") == "/thumbs":
        thumbs = qresource
        break

if thumbs is None:
    thumbs = ET.SubElement(root, "qresource", prefix="/thumbs")

# Add PNG files to /thumbs if not already there
existing_files = {file_elem.text for file_elem in thumbs.findall("file")}

new_pngs = 0
existing_pngs = 0
for filename in sorted(os.listdir(icons_folder)):
    if filename.endswith(".png"):
        relative_path = os.path.join("StratagemIcons", filename).replace("\\", "/")
        if relative_path not in existing_files:
            new_pngs += 1
            ET.SubElement(thumbs, "file").text = relative_path
        else:
            existing_pngs += 1

# Convert XML tree to a pretty-printed string
xml_str = ET.tostring(root, encoding="utf-8")
pretty_xml = minidom.parseString(xml_str).toprettyxml(indent="    ")

# Remove extra blank lines introduced by minidom
pretty_xml = "\n".join([line for line in pretty_xml.splitlines() if line.strip()])

# Write back to the .qrc file
with open(qrc_file, "w", encoding="utf-8") as f:
    f.write(pretty_xml)

print(f"Added {new_pngs} PNGs to {qrc_file} ({existing_pngs} already existed)")
print(f"Total PNGs are now: {len(os.listdir(icons_folder))}")
input("\nPress Enter to exit...")