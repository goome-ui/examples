import os
import re
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN = os.path.join(ROOT, "build", "run")


def pick(out, test):
    frames = sorted(f for f in os.listdir(out) if re.fullmatch(r"\d{6}\.ppm", f))
    others = sorted(f for f in os.listdir(out) if f.endswith((".ppm", ".png")) and f not in frames)
    if not frames:
        return os.path.join(out, others[0]) if others else None
    want = None
    if os.path.exists(test):
        for line in open(test):
            m = re.match(r"\s*shot\s+(\d+)", line)
            if m:
                want = "%06d.ppm" % int(m.group(1))
    return os.path.join(out, want if want in frames else frames[-1])


def main():
    only = sys.argv[1:]
    for dirpath, dirs, files in sorted(os.walk(RUN)):
        if "log.txt" not in files:
            continue
        rel = os.path.relpath(dirpath, RUN).replace(os.sep, "/")
        if only and not any(rel.startswith(o) for o in only):
            continue
        folder = os.path.join(ROOT, rel)
        src = pick(dirpath, os.path.join(folder, "test.txt"))
        if not src:
            print("no picture for %s" % rel)
            continue
        img = Image.open(src).convert("RGB")
        if img.width > 1280:
            img = img.resize((img.width // 2, img.height // 2), Image.LANCZOS)
        dst = os.path.join(folder, "shot.png")
        img.save(dst, optimize=True)
        print("%s <- %s (%dx%d, %d KB)" % (rel + "/shot.png", os.path.basename(src), img.width, img.height, os.path.getsize(dst) // 1024))


if __name__ == "__main__":
    main()
