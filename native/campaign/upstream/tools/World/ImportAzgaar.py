"""BR-042: convert Azgaar full JSON into inert, versioned geometry input.

No source formulas, notes, routes scripts, SVG markup or external URLs are executed.
The C++ campaign applies all terrain/population rules and revalidates the result.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON field: " + key)
        result[key] = value
    return result


def number(value, low, high):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and low <= value <= high


def integer(value, low, high):
    return isinstance(value, int) and not isinstance(value, bool) and low <= value <= high


def point(value, width, height):
    require(isinstance(value, list) and len(value) == 2 and
            number(value[0], -width, width * 2) and number(value[1], -height, height * 2), "Invalid coordinate pair")
    return tuple(value)


def quote(value):
    require(isinstance(value, str) and 0 < len(value.encode("utf-8")) <= 1024 and
            all(ord(c) >= 32 for c in value), "Invalid metadata text")
    return '"' + value.replace(chr(92), chr(92) * 2).replace('"', chr(92) + '"') + '"'


def clip_polygon(points, width, height):
    # Preserve convex Voronoi geometry, clipping boundary vertices to the map rectangle.
    for axis, bound, keep_low in [(0, 0, False), (0, width, True), (1, 0, False), (1, height, True)]:
        output = []
        for n, b in enumerate(points):
            a = points[n - 1]
            a_in = a[axis] <= bound if keep_low else a[axis] >= bound
            b_in = b[axis] <= bound if keep_low else b[axis] >= bound
            if a_in != b_in:
                fraction = (bound - a[axis]) / (b[axis] - a[axis])
                output.append(tuple(a[k] + fraction * (b[k] - a[k]) for k in (0, 1)))
            if b_in:
                output.append(b)
        points = output
    cleaned = []
    for p in points:
        if not cleaned or math.dist(p, cleaned[-1]) > 1e-9:
            cleaned.append(p)
    if len(cleaned) > 1 and math.dist(cleaned[0], cleaned[-1]) <= 1e-9:
        cleaned.pop()
    require(3 <= len(cleaned) <= 64, "Degenerate clipped polygon")
    return cleaned


def convert(document):
    require(isinstance(document, dict), "Expected JSON object")
    info, pack, settings = document.get("info", {}), document.get("pack", {}), document.get("settings", {})
    require(isinstance(info, dict) and isinstance(pack, dict) and isinstance(settings, dict), "Expected info, pack and settings objects")
    version = info.get("version", "")
    require(isinstance(version, str) and re.fullmatch(r"1[.][0-9]+([.][0-9]+)?", version), "Unsupported Azgaar major version")
    width, height = info.get("width"), info.get("height")
    require(number(width, 1, 1000000) and number(height, 1, 1000000), "Invalid dimensions")
    raw_cells, raw_vertices = pack.get("cells"), pack.get("vertices")
    require(isinstance(raw_cells, list) and 1 <= len(raw_cells) <= 20000, "Expected packed full-export cells, maximum 20000")
    require(isinstance(raw_vertices, list) and 3 <= len(raw_vertices) <= 100000, "Expected packed full-export vertices")
    vertices = {}
    for v in raw_vertices:
        require(isinstance(v, dict) and integer(v.get("i"), 0, 999999) and v["i"] not in vertices, "Invalid/duplicate vertex ID")
        vertices[v["i"]] = point(v.get("p"), width, height)
    cells = {}
    for c in raw_cells:
        require(isinstance(c, dict) and integer(c.get("i"), 0, 999999) and c["i"] not in cells, "Invalid/duplicate cell ID")
        require(integer(c.get("h"), 0, 100) and integer(c.get("biome"), 0, 12), "Invalid elevation/biome")
        center = point(c.get("p"), width, height)
        require(0 <= center[0] <= width and 0 <= center[1] <= height, "Cell center outside map")
        polygon_ids, neighbors = c.get("v"), c.get("c")
        require(isinstance(polygon_ids, list) and 3 <= len(polygon_ids) <= 64 and
                all(integer(i, 0, 999999) and i in vertices for i in polygon_ids), "Invalid polygon vertex IDs")
        require(isinstance(neighbors, list) and len(neighbors) <= 64 and len(set(neighbors)) == len(neighbors) and
                all(integer(i, 0, 999999) and i != c["i"] for i in neighbors), "Invalid neighbors")
        cells[c["i"]] = dict(c, polygon=clip_polygon([vertices[i] for i in polygon_ids], width, height), center=center)
    for i, c in cells.items():
        for n in c["c"]:
            require(n in cells and i in cells[n]["c"], "Missing/asymmetric neighbor at cell " + str(i))
    land = {i: c for i, c in cells.items() if c["h"] >= 20}
    require(0 < len(land) <= 10000, "Expected 1..10000 land areas")
    roads, skipped, sea_routes = {}, 0, 0
    routes = pack.get("routes", [])
    require(isinstance(routes, list) and len(routes) <= 100000, "Invalid routes")
    for route in routes:
        require(isinstance(route, dict), "Invalid route object")
        group = route.get("group")
        if group == "searoutes":
            sea_routes += 1
            continue
        if group not in ("roads", "trails") or route.get("removed"):
            continue
        points = route.get("points")
        require(isinstance(points, list) and len(points) <= 100000, "Invalid route points")
        ids = []
        for p in points:
            require(isinstance(p, list) and len(p) >= 3 and integer(p[2], 0, 999999) and p[2] in cells, "Invalid route cell")
            if not ids or ids[-1] != p[2]:
                ids.append(p[2])
        for a, b in zip(ids, ids[1:]):
            if a not in land or b not in land or b not in land[a]["c"]:
                skipped += 1
                continue
            key = tuple(sorted((a + 1, b + 1)))
            roads[key] = max(roads.get(key, 0), 1 if group == "roads" else 0.35)
    distance_scale = settings.get("distanceScale", 1)
    require(number(distance_scale, 0.000001, 1000000), "Invalid source distance scale")
    name = info.get("mapName", settings.get("mapName", "Imported world"))
    seed = str(info.get("seed", "unknown"))
    lines = ["BIRTHRIGHT_GEOGRAPHY 1", " ".join([
        "MAP_GEOMETRY", "1", quote(name), quote(version), quote(seed), quote(settings.get("distanceUnit", "map units")),
        str(width), str(height), str(distance_scale), str(len(cells))])]
    for i, c in sorted(cells.items()):
        values = [i + 1, *c["center"], c["h"], c["biome"], len(c["polygon"])]
        values += [v for p in c["polygon"] for v in p]
        lines.append(" ".join(str(v) for v in values))
    lines.append(str(len(land)))
    for i, c in sorted(land.items()):
        neighbors = sorted(n + 1 for n in c["c"] if n in land)
        lines.append(" ".join(str(v) for v in [i + 1, len(neighbors), *neighbors]))
    lines.append(str(len(roads)))
    for (a, b), quality in sorted(roads.items()):
        lines.append(f"{a} {b} {quality}")
    seen, components = set(), []
    for i in sorted(land):
        if i in seen:
            continue
        pending, count = [i], 0
        seen.add(i)
        while pending:
            key = pending.pop()
            count += 1
            for n in land[key]["c"]:
                if n in land and n not in seen:
                    seen.add(n)
                    pending.append(n)
        components.append(count)
    return "\n".join(lines) + "\n", dict(mapName=name, sourceVersion=version, landCells=len(land),
        waterCells=len(cells)-len(land), landComponents=sorted(components, reverse=True), roadLinks=len(roads),
        ignoredSeaRoutes=sea_routes, skippedNonadjacentRouteSteps=skipped)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    try:
        source, target = Path(args.input), Path(args.output)
        require(source.resolve() != target.resolve(), "Input and output must differ")
        require(source.stat().st_size <= 64 * 1024 * 1024, "Source exceeds 64 MiB limit")
        raw = source.read_bytes()
        document = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=unique_object,
                              parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite JSON constant " + value)))
        text, summary = convert(document)
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_suffix(target.suffix + ".tmp")
        temporary.write_text(text, encoding="utf-8", newline="\n")
        temporary.replace(target)
        summary.update(status="accepted", sourceSha256=hashlib.sha256(raw).hexdigest())
        print(json.dumps(summary))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(json.dumps(dict(status="rejected", code="azgaar_import_invalid", detail=str(error))), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
