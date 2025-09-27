#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required for convert_background.py") from exc


def convert_background(input_path: Path, convert_script: Path, output_image: Path, output_palette: Path, *, width: int, height: int, quantize: int) -> None:
    with TemporaryDirectory() as tmpdir:
        tmp_png = Path(tmpdir) / "background_resized.png"
        img = Image.open(input_path).convert("RGBA")
        img = img.resize((width, height), Image.LANCZOS)
        img.save(tmp_png)

        cmd = [
            sys.executable,
            str(convert_script),
            "-b",
            "4",
            str(tmp_png),
            str(output_image),
            str(output_palette),
        ]
        if quantize > 0:
            cmd[4:4] = ["-q", str(quantize)]
        subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Resize and convert a background image for the Picostation menu")
    parser.add_argument("input", type=Path, help="Source image (PNG/JPEG)")
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=240)
    parser.add_argument("--quantize", type=int, default=16, help="Number of colours; set 0 to skip explicit quantization")
    parser.add_argument("--convert-script", type=Path, default=Path("ps1-bare-metal/tools/convertImage.py"), help="Path to convertImage.py")
    parser.add_argument("--output-image", type=Path, required=True, help="Destination .dat file for image data")
    parser.add_argument("--output-palette", type=Path, required=True, help="Destination .dat file for palette data")
    args = parser.parse_args()

    convert_background(
        input_path=args.input,
        convert_script=args.convert_script,
        output_image=args.output_image,
        output_palette=args.output_palette,
        width=args.width,
        height=args.height,
        quantize=args.quantize,
    )


if __name__ == "__main__":
    main()
