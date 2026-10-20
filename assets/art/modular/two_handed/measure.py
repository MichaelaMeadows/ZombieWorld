"""Read-only image measurements; emit registration metadata, never edit artwork.

Match a separated gear layer to its original dressed pose reference. The output
is a candidate for visual inspection, not an automatic approval of an asset.
Usage: python tools/measure_layer_registration.py reference.png armor.png out.json
       --min-scale 0.5 --max-scale 0.85
"""
import argparse
import json
import numpy as np
from PIL import Image


def measure(reference, gear, lo, hi):
    width, height = reference.size
    cell_w, cell_h = width // 8, height // 7
    answer = []
    for index in range(56):
        x, y = index % 8 * cell_w, index // 8 * cell_h
        target = np.asarray(reference.crop((x, y, x + cell_w, y + cell_h)), dtype=np.float32)
        source = np.asarray(gear.crop((x, y, x + cell_w, y + cell_h)), dtype=np.float32)
        ys, xs = np.nonzero(source[:, :, 3] > 200)
        if not len(xs):
            answer.append({"index": index, "empty": True})
            continue
        # Deterministic coverage sampling includes the gear silhouette.
        selected = np.linspace(0, len(xs) - 1, min(180, len(xs))).astype(int)
        points = np.stack((xs[selected], ys[selected]), axis=1)
        colors = source[ys[selected], xs[selected], :3] / 255
        bbox_min = np.array([xs.min(), ys.min()])
        bbox_max = np.array([xs.max(), ys.max()])
        best = (float("inf"), None, None)
        for scale in np.linspace(lo, hi, 13):
            dimensions = (bbox_max - bbox_min) * scale
            if dimensions[0] >= cell_w or dimensions[1] >= cell_h:
                continue
            grid_x, grid_y = np.meshgrid(
                np.arange(0, cell_w - dimensions[0], 3),
                np.arange(0, cell_h - dimensions[1], 3),
            )
            origins = np.stack((grid_x.ravel(), grid_y.ravel()), axis=1)
            relative = (points - bbox_min) * scale
            samples = np.rint(origins[:, None, :] + relative[None, :, :]).astype(int)
            pixels = target[np.clip(samples[:, :, 1], 0, cell_h-1), np.clip(samples[:, :, 0], 0, cell_w-1)]
            difference = np.abs(pixels[:, :, :3] / 255 - colors[None, :, :]).mean(axis=2)
            # An opaque item cannot match transparent background.
            difference += (1 - pixels[:, :, 3] / 255) * 1.5
            losses = difference.mean(axis=1)
            winner = int(losses.argmin())
            if losses[winner] < best[0]:
                best = (float(losses[winner]), float(scale), origins[winner] - bbox_min * scale)
        loss, scale, offset = best
        if scale is None:
            answer.append({"index": index, "unmatched": True})
            continue
        # Runtime mapping: world = scale*(point-pivot). Target mapping:
        # target = scale*point+offset, with the reference foot at (128,220).
        foot = np.array([cell_w * .5, cell_h * .859375])
        pivot = (foot - offset) / scale
        answer.append({"index": index, "scale": scale, "pivot": pivot.tolist(), "score": loss})
    return answer


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("reference")
    parser.add_argument("layer")
    parser.add_argument("output")
    parser.add_argument("--min-scale", type=float, default=.5)
    parser.add_argument("--max-scale", type=float, default=.85)
    args = parser.parse_args()
    reference = Image.open(args.reference).convert("RGBA")
    layer = Image.open(args.layer).convert("RGBA")
    if reference.size != layer.size:
        raise SystemExit("Normalize coordinate spaces before measurement")
    measurements = measure(reference, layer, args.min_scale, args.max_scale)
    with open(args.output, "w", encoding="utf8") as output:
        json.dump(measurements, output, indent=2)
    print(f"Measured {len(measurements)} frames; inspect all candidate transforms.")

