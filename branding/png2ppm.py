#!/usr/bin/env python3
"""Convert an 8-bit RGB/RGBA PNG to the binary PPM (P6) the PBA draws.

Transparent pixels are blended onto the console background colour, so the
logo sits on the same navy as the text screen, and logos larger than 192 px
are scaled down. The PNG code lives in tools/cakes-builder.py.

    png2ppm.py logo.png logo.ppm [--bg 070c2b]
"""
import argparse
import importlib.util
import os


def builder():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "cakes-builder.py")
    spec = importlib.util.spec_from_file_location("cakes_builder", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("png")
    ap.add_argument("ppm")
    ap.add_argument("--bg", default="070c2b", help="background colour, hex RRGGBB")
    args = ap.parse_args()
    ppm, _ = builder().logo_ppm(args.png, args.bg)
    with open(args.ppm, "wb") as f:
        f.write(ppm)


if __name__ == "__main__":
    main()
