#!/usr/bin/env python3
"""Extract the candidate's MODE and allocation bodies for the host test."""

import argparse
from pathlib import Path
import re


def body(path, signature):
    text = path.read_text()
    start = text.index(signature)
    end = text.index("\n}", start) + 2
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--graphics", type=Path, required=True, help="vdp-gl checkout")
    args = parser.parse_args()
    screen = args.firmware / "video/agon_screen.h"
    graphics = args.graphics / "src"
    mode = body(screen, "int8_t changeMode(")
    constants = sorted(set(re.findall(r", ([A-Z]+_[A-Za-z0-9_]+)", mode)))
    args.output.write_text("\n\n".join([
        "\n".join(f'#define {name} "{name}"' for name in constants),
        "namespace fabgl {\n" + "\n\n".join(
            body(graphics / "fabutils.cpp", signature) for signature in (
                "void LightMemoryPool::mark(", "LightMemoryPool::LightMemoryPool(",
                "LightMemoryPool::~LightMemoryPool(")) + "\n}",
        *(body(screen, signature) for signature in (
                "inline uint8_t getVGAColourDepth(",
            "std::unique_ptr<fabgl::VGABaseController> getVGAController(",
            "void releaseVGAController(", "bool updateVGAController(",
            "int8_t changeResolution(")),
        mode,
        body(args.firmware / "video/vdu.h", "void VDUStreamProcessor::vdu_mode("),
        body(args.firmware / "video/vdu_sys.h", "void VDUStreamProcessor::sendModeInformation("),
    ]) + "\n")


if __name__ == "__main__":
    main()
