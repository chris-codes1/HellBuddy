# Completes the 'Obtaining new stratagem icons' instructions:
# 1. Checks AddedStratagems.json against all .svg files in the
#    nvigneux/Helldivers-2-Stratagems-icons-svg GitHub repo and downloads any missing ones
#    into StratagemIcons, recording each name in AddedStratagems.json
# 2. Adds each new stratagem to JsonData/stratagems.json with a blank sequence
# 3. Runs BuildQrcResources.py

import json
import os
import subprocess
import sys
import urllib.parse
import urllib.request

REPO = "nvigneux/Helldivers-2-Stratagems-icons-svg"
BRANCH = "master"

dev_tools_dir = os.path.dirname(os.path.abspath(__file__))
base_dir = os.path.dirname(dev_tools_dir)
icons_folder = os.path.join(base_dir, "StratagemIcons")
stratagems_file = os.path.join(base_dir, "JsonData", "stratagems.json")
documents_dir = os.path.dirname(os.path.dirname(base_dir))  # ...\Documents
added_file = os.path.join(documents_dir, "Claude", "HellBuddyClaude", "AddedStratagems.json")
build_qrc_script = os.path.join(dev_tools_dir, "BuildQrcResources.py")


def fetch_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "HellBuddy-icon-updater"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.load(resp)


def main():
    # Load AddedStratagems.json (name -> bool)
    with open(added_file, encoding="utf-8") as f:
        added = json.load(f)

    # List every .svg in the upstream repo
    print(f"Fetching file list from {REPO}...")
    tree = fetch_json(f"https://api.github.com/repos/{REPO}/git/trees/{BRANCH}?recursive=1")
    upstream = {}  # name -> repo path
    for entry in tree["tree"]:
        if entry["path"].endswith(".svg"):
            name = os.path.basename(entry["path"])[:-4]
            upstream[name] = entry["path"]

    missing = sorted(n for n in upstream if not added.get(n))
    print(f"Upstream has {len(upstream)} icons, {len(missing)} not yet added.")

    if not missing:
        print("Nothing new to download.")
        input("\nPress Enter to exit...")
        return

    # Download each missing icon
    downloaded = []
    for name in missing:
        url = f"https://raw.githubusercontent.com/{REPO}/{BRANCH}/" + urllib.parse.quote(upstream[name])
        dest = os.path.join(icons_folder, name + ".svg")
        print(f"Downloading {name}...")
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "HellBuddy-icon-updater"})
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = resp.read()
            with open(dest, "wb") as f:
                f.write(data)
            added[name] = True
            downloaded.append(name)
        except Exception as e:
            print(f"  FAILED: {e}")
            added[name] = False

    # Save AddedStratagems.json
    with open(added_file, "w", encoding="utf-8") as f:
        json.dump({n: added[n] for n in sorted(added)}, f, indent=2, ensure_ascii=False)

    # Add new stratagems to stratagems.json with a blank sequence
    with open(stratagems_file, encoding="utf-8") as f:
        stratagems = json.load(f)
    existing_names = {s["name"] for s in stratagems}
    new_entries = [n for n in downloaded if n not in existing_names]
    for name in new_entries:
        stratagems.append({"name": name, "sequence": []})
    with open(stratagems_file, "w", encoding="utf-8") as f:
        json.dump(stratagems, f, indent=2, ensure_ascii=False)

    print(f"\nDownloaded {len(downloaded)} icons, added {len(new_entries)} entries to stratagems.json.")
    if new_entries:
        print("Remember to record their combo sequences (StratagemComboType.ahk can help):")
        for name in new_entries:
            print(f"  {name}")

    # Rebuild the qrc so the new icons get built into the .exe
    print("\nRunning BuildQrcResources.py...")
    subprocess.run([sys.executable, build_qrc_script], input=b"\n", check=True)


if __name__ == "__main__":
    main()
