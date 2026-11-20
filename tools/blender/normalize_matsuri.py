"""Normalise the Tripo props for Suzune's Night Festival: one mesh, real-world size, pivot, triangle budget.

  blender -b --factory-startup --python tools/blender/normalize_matsuri.py -- \
      --spec tools/tripo/assets_matsuri.json --src tools/tripo/out/props --dst SuzuneMatsuri/SourceArt/Tripo [--only A,B]

Per asset in the spec: fit ("height" | "max"), size_m, pivot ("bottom" | "center"), faces (decimation target).
Textures and PBR materials survive: decimation only collapses geometry, UVs are kept.
Writes <dst>/<name>.glb plus <dst>/norm_meta.json (final size and triangle count per asset).
"""
import argparse
import json
import os
import sys

import bpy
from mathutils import Matrix, Vector


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec", required=True)
    parser.add_argument("--src", required=True)
    parser.add_argument("--dst", required=True)
    parser.add_argument("--only", default="")
    return parser.parse_args(argv)


def import_as_single_mesh(path, name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"{path}: no mesh")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.parent = None
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for other in list(bpy.context.scene.objects):
        if other is not obj:
            bpy.data.objects.remove(other, do_unlink=True)
    obj.name = obj.data.name = name
    return obj


def bounds(mesh):
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    zs = [v.co.z for v in mesh.vertices]
    return Vector((min(xs), min(ys), min(zs))), Vector((max(xs), max(ys), max(zs)))


def decimate(obj, target_faces):
    faces = len(obj.data.polygons)
    if not target_faces or faces <= target_faces:
        return
    modifier = obj.modifiers.new("Decimate", "DECIMATE")
    modifier.ratio = max(0.01, target_faces / faces)
    modifier.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def align_long_axis(mesh):
    """Turn a long thin prop (an arrow) so its length runs along +X with its head forward.

    Tripo lays such props at an angle. The long axis is the vertices' principal axis; the head is the end
    whose thin tip runs further back before the shape widens (an arrowhead is longer than a nock).
    """
    import numpy as np
    points = np.array([v.co[:] for v in mesh.vertices])
    centre = points.mean(axis=0)
    _, vectors = np.linalg.eigh(np.cov((points - centre).T))
    axis = vectors[:, -1]
    along = (points - centre) @ axis
    length = along.max() - along.min()

    def thin_run(sign):
        s = sign * along
        tip = s.max()
        for k in range(1, 30):
            band = (s > tip - length * k / 60) & (s <= tip - length * (k - 1) / 60)
            if band.any():
                across = (points[band] - centre) - np.outer(along[band], axis)
                if np.linalg.norm(across, axis=1).max() > length * 0.03:
                    return k
        return 30

    head = axis if thin_run(1) >= thin_run(-1) else -axis
    turn = Vector(head).rotation_difference(Vector((1.0, 0.0, 0.0))).to_matrix().to_4x4()
    mesh.transform(Matrix.Translation(Vector(centre)) @ turn @ Matrix.Translation(-Vector(centre)))
    mesh.update()


def align_bow(mesh):
    """Stand a bow up: its length along +Z, the handle's bulge toward +X, the string on the -X side.

    The view model then knows where things are: the grip near +X, the string's middle (the nock) and its
    two ends (the tips) at -X. The long axis is the principal axis; the bulge is the direction from the
    middle of the line between the tips to the centre of the vertices around the handle.
    """
    import numpy as np
    points = np.array([v.co[:] for v in mesh.vertices])
    centre = points.mean(axis=0)
    _, vectors = np.linalg.eigh(np.cov((points - centre).T))
    length_axis = vectors[:, -1]
    along = (points - centre) @ length_axis
    span = along.max() - along.min()
    tips = points[(along > along.max() - span * 0.05) | (along < along.min() + span * 0.05)].mean(axis=0)
    handle = points[np.abs(along) < span * 0.08].mean(axis=0)
    bulge = handle - tips
    bulge -= length_axis * (bulge @ length_axis)
    bulge /= np.linalg.norm(bulge)
    side = np.cross(length_axis, bulge)
    # Rows: where each old direction should go. Map length -> Z, bulge -> X, side -> Y.
    basis = Matrix((Vector(bulge), Vector(side), Vector(length_axis)))
    mesh.transform(Matrix.Translation(Vector(centre)) @ basis.to_4x4() @ Matrix.Translation(-Vector(centre)))
    mesh.update()


def scale_and_pivot(mesh, asset):
    lo, hi = bounds(mesh)
    dims = hi - lo
    reference = dims.z if asset.get("fit", "height") == "height" else max(dims)
    k = asset["size_m"] / reference
    pivot_z = lo.z if asset.get("pivot", "bottom") == "bottom" else (lo.z + hi.z) / 2
    offset = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, pivot_z))
    mesh.transform(Matrix.Diagonal(Vector((k, k, k))).to_4x4() @ Matrix.Translation(-offset))
    mesh.update()
    lo, hi = bounds(mesh)
    return hi - lo


def export_glb(obj, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_apply=True, export_yup=True)


def main():
    args = parse_args()
    with open(args.spec, encoding="utf-8") as f:
        assets = [a for a in json.load(f)["assets"] if "size_m" in a]
    only = set(filter(None, args.only.split(",")))
    os.makedirs(args.dst, exist_ok=True)
    meta_path = os.path.join(args.dst, "norm_meta.json")
    meta = json.load(open(meta_path, encoding="utf-8")) if os.path.exists(meta_path) else {}

    for asset in assets:
        name = asset["name"]
        src = os.path.join(args.src, f"{name}.glb")
        if (only and name not in only) or not os.path.exists(src):
            continue
        obj = import_as_single_mesh(src, name)
        before = len(obj.data.polygons)
        decimate(obj, asset.get("faces"))
        if asset.get("align") == "long_x":
            align_long_axis(obj.data)
        elif asset.get("align") == "bow":
            align_bow(obj.data)
        dims = scale_and_pivot(obj.data, asset)
        export_glb(obj, os.path.join(args.dst, f"{name}.glb"))
        meta[name] = {"dims_m": [round(d, 3) for d in dims], "faces": len(obj.data.polygons), "faces_before": before}
        print(f"[norm] {name}: {before} -> {len(obj.data.polygons)} faces, size {tuple(round(d, 2) for d in dims)} m")
        with open(meta_path, "w", encoding="utf-8") as f:  # write as we go: a crash keeps earlier results
            json.dump(meta, f, indent=2)


main()
