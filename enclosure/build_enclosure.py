"""Gold LED display enclosure v2 - screwless body + snap-on back cover.

Hardware: MAX7219 4-in-1 (32x8) + ESP32 30-pin USB-C. Run inside Blender:
    exec(open(r"<path>/build_enclosure.py", encoding="utf-8").read())
It rebuilds collection CASE, exports STL to ./stl and saves ./gold_led_case.blend.

Units: 1 Blender unit = 1 mm. Axes: X = width (left->right seen from the front),
Y = depth (0 = LED face, + toward the back), Z = up. Display at x 0..132, y 0..14, z 0..32.

v2 changes (Codex review 2026-11-08): no screws (10 snap latches, ESP32 in a cradle
with end stops), spring-loaded display pegs, feet start at the front plane, 1 mm edge radius,
+2 mm wire margin, USB hole kept 1.25 mm clear of the seam, BOOT flexure button.

Sizes are owner caliper measurements unless marked EST.
"""
import math
import os

import bmesh
import bpy

OUT_DIR = r"C:\Users\supac\Documents\Claude\Projects\gold-led-display\gold-led-display\enclosure"

# ------------------------------------------------------------------ hardware (measured)
DISP_L, DISP_H, DISP_D = 132.0, 32.0, 14.1    # caliper, LED face -> solder tips
IN_ZONE = 26.0                     # IN header + jumper heads + bend (25 measured + 1 margin)
MOD_PITCH, HOLE_IN_MOD, HOLE_Z = 33.0, 12.95, 2.65
PCB_BACK_Y = 13.0                  # owner: LED face -> PCB back ~13 (solder tips at 14.1)

ESP_L, ESP_W, ESP_T = 50.0, 30.0, 1.6
ESP_HOLE_IN = 2.1
ESP_USB_OVR = 1.5
ESP_STACK = 14.5                   # PCB -> top of jumper heads
ESP_BEND = 10.0
WIRE_MARGIN = 2.0
ESP_COMP_H = 3.4                   # EST: tallest part on the component side (module)
BOOT_FROM_EDGE, BOOT_L = 1.5, 4.2
BOOT_OFF = 7.7                     # caliper: button centre offset from the USB centre line
BOOT_H = 1.9                       # caliper: button + board 3.5 minus PCB 1.6

# ------------------------------------------------------------------ case
CLR = 0.4                          # display clearance per side
FIT = 0.20                         # ESP32 cradle clearance per side (sand stops if tight)
WALL, FRONT = 2.5, 2.0
WIN_L, WIN_H, WIN_CHAMFER = 131.0, 31.0, 0.6   # 0.5 mm lip over the LED blocks
EDGE_R = 1.0
TILT, FOOT_MIN, FOOT_EMBED = 12.0, 2.0, 2.0
USB_W, USB_H = 13.0, 7.0           # outside recess for the plug overmold (depth USB_RECESS)
USB_RECESS = 1.0                   # leaves 1.5 mm wall: port face -> recess floor 2.1 < 3 mm plug metal
USB_PORT_W, USB_PORT_H = 9.6, 3.8  # snug racetrack hole around the plug metal shell (8.4 x 2.6)
PEG_PRELOAD = 0.3                  # spring leaf deflection at nominal (Codex: 0.25-0.35)
LATCH_X = (10.0, 45.0, 78.0, 114.0, 140.0)
LATCH_W, LATCH_L, LATCH_T, HOOK = 6.0, 6.0, 1.2, 0.5
LATCH_GAP = 0.22                   # arm-to-wall running gap; flex = HOOK - LATCH_GAP = 0.28 (~1.4% strain)
BOOT_GAP = 0.6                     # plunger-to-button gap (Codex: 0.45-0.60)

# ------------------------------------------------------------------ derived layout
X0, X1 = -CLR, DISP_L + IN_ZONE + CLR
Z0, Z1 = -CLR, DISP_H + CLR
Y_PIN = DISP_D + ESP_BEND + WIRE_MARGIN + ESP_STACK
Y_COMP = Y_PIN + ESP_T
USB_Y = Y_COMP + 1.6
Y_IN = max(Y_COMP + ESP_COMP_H + 1.0, USB_Y + USB_H / 2 + 1.25)   # cover inner face
OX0, OX1, OZ0, OZ1 = X0 - WALL, X1 + WALL, Z0 - WALL, Z1 + WALL
OY0, OY1 = -FRONT, Y_IN + WALL
ESP_X1 = X1 - 0.6 - ESP_USB_OVR
ESP_X0 = ESP_X1 - ESP_L
ESP_ZC = DISP_H / 2
EZ0, EZ1 = ESP_ZC - ESP_W / 2, ESP_ZC + ESP_W / 2
ESP_HOLES = [(ESP_X0 + ESP_HOLE_IN, EZ0 + ESP_HOLE_IN), (ESP_X1 - ESP_HOLE_IN, EZ0 + ESP_HOLE_IN),
             (ESP_X0 + ESP_HOLE_IN, EZ1 - ESP_HOLE_IN), (ESP_X1 - ESP_HOLE_IN, EZ1 - ESP_HOLE_IN)]
BOOT_X = ESP_X1 - BOOT_FROM_EDGE - BOOT_L / 2
BOOT_Z = ESP_ZC - BOOT_OFF
PEGS = [(16.5 + MOD_PITCH * i + HOLE_IN_MOD, z) for i in range(3) for z in (HOLE_Z, DISP_H - HOLE_Z)]


# ------------------------------------------------------------------ geometry helpers
def _obj(name, bm, col):
    me = bpy.data.meshes.new(name)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    return ob


def cube(name, mn, mx, col):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = [(mn[i] + mx[i]) / 2 + v.co[i] * (mx[i] - mn[i]) for i in range(3)]
    return _obj(name, bm, col)


def cyl_x(name, y, z, x0, x1, r, col, segs=40):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r, depth=x1 - x0)
    for v in bm.verts:
        cx, cy, cz = v.co
        v.co = ((x0 + x1) / 2 + cz, y + cx, z + cy)
    return _obj(name, bm, col)


def engrave_text(text, x, z, size, col, depth=0.4):
    """Text cutter on the cover outer face (+Y), readable when looking at the back."""
    from mathutils import Matrix
    cu = bpy.data.curves.new("txt", "FONT")
    cu.body = text
    cu.size = size
    cu.align_x = "CENTER"
    cu.align_y = "CENTER"
    cu.extrude = (depth + 1.0) / 2
    tob = bpy.data.objects.new("txt", cu)
    col.objects.link(tob)
    bpy.context.view_layer.update()
    me = bpy.data.meshes.new_from_object(tob.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    bpy.data.objects.remove(tob, do_unlink=True)
    bpy.data.curves.remove(cu)
    # text x -> -X (viewer at the back faces -Y), text y -> +Z, text z (normal) -> +Y
    rot = Matrix(((-1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    me.transform(Matrix.Translation((x, OY1 - depth + (depth + 1.0) / 2, z)) @ rot)
    ob = bpy.data.objects.new("engrave", me)
    col.objects.link(ob)
    return ob


def cyl_y(name, x, z, y0, y1, r0, r1=None, col=None, segs=40):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r0,
                          radius2=r0 if r1 is None else r1, depth=y1 - y0)
    for v in bm.verts:
        cx, cy, cz = v.co
        v.co = (x + cx, (y0 + y1) / 2 + cz, z + cy)
    return _obj(name, bm, col)


def prism(name, pts, axis, a0, a1, col):
    """Extrude a 2D polygon along an axis. axis 'x': pts are (y, z); 'z': pts are (x, y)."""
    bm = bmesh.new()
    def p3(p, a):
        return (a, p[0], p[1]) if axis == "x" else (p[0], p[1], a)
    a = [bm.verts.new(p3(p, a0)) for p in pts]
    b = [bm.verts.new(p3(p, a1)) for p in pts]
    bm.faces.new(a)
    bm.faces.new(b)
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((a[i], a[j], b[j], b[i]))
    return _obj(name, bm, col)


def frustum_y(name, y0, y1, s0, s1, cx, cz, col):
    bm = bmesh.new()
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    a = [bm.verts.new((cx + sx * s0[0] / 2, y0, cz + sz * s0[1] / 2)) for sx, sz in corners]
    b = [bm.verts.new((cx + sx * s1[0] / 2, y1, cz + sz * s1[1] / 2)) for sx, sz in corners]
    bm.faces.new(a)
    bm.faces.new(b)
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((a[i], a[j], b[j], b[i]))
    return _obj(name, bm, col)


def bevel_except_plane_y(ob, y_plane, r):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    edges = [e for e in bm.edges if not all(abs(v.co.y - y_plane) < 1e-4 for v in e.verts)]
    bmesh.ops.bevel(bm, geom=edges, offset=r, segments=3, affect="EDGES", profile=0.5)
    bm.to_mesh(ob.data)
    bm.free()


def boolean(target, tools, op):
    for tool in tools:            # one exact boolean per tool (joined tools proved unreliable)
        m = target.modifiers.new(op, "BOOLEAN")
        m.operation = op
        m.solver = "EXACT"
        m.object = tool
        bpy.context.view_layer.objects.active = target
        bpy.ops.object.modifier_apply(modifier=m.name)
        bpy.data.objects.remove(tool, do_unlink=True)


def get_col(name):
    col = bpy.data.collections.get(name)
    if col is None:
        col = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(col)
    for ob in list(col.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    return col


# ------------------------------------------------------------------ latch pieces
def latch_arm(i, x, top, tmp):
    """Cantilever arm on the cover with an outward hook; 45 deg in, 45 deg out (pull to open)."""
    s = 1 if top else -1
    wall_in = Z1 if top else Z0
    face = wall_in - s * LATCH_GAP                 # arm outer face
    inner = face - s * LATCH_T
    za, zb = sorted((inner, face))
    parts = [cube(f"arm{i}", (x - LATCH_W / 2, Y_IN - LATCH_L, za), (x + LATCH_W / 2, Y_IN + 0.1, zb), tmp)]
    y0 = Y_IN - LATCH_L
    hook = [(y0, face - s * 0.5), (y0, face), (y0 + HOOK, face + s * HOOK), (y0 + HOOK + 0.6, face + s * HOOK),
            (y0 + 2 * HOOK + 0.6, face), (y0 + 2 * HOOK + 0.6, face - s * 0.5)]
    parts.append(prism(f"hook{i}", hook, "x", x - LATCH_W / 2, x + LATCH_W / 2, tmp))
    return parts


def latch_pocket(i, x, top, tmp):
    wall_in = Z1 if top else Z0
    s = 1 if top else -1
    y0 = Y_IN - LATCH_L
    depth = HOOK - LATCH_GAP + 0.28                # hook tip + radial relief
    za, zb = sorted((wall_in - s * 0.01, wall_in + s * depth))
    return cube(f"pocket{i}", (x - LATCH_W / 2 - 0.3, y0 - 0.28, za),
                (x + LATCH_W / 2 + 0.3, y0 + 2 * HOOK + 0.6 + 0.28, zb), tmp)


# ------------------------------------------------------------------ build
def build_body(col, tmp):
    body = cube("CASE_Body", (OX0, OY0, OZ0), (OX1, Y_IN, OZ1), col)
    bevel_except_plane_y(body, Y_IN, EDGE_R)
    boolean(body, [cube("inner", (X0, 0, Z0), (X1, Y_IN + 1, Z1), tmp)], "DIFFERENCE")

    adds = [cube("rib_bot", (DISP_L + CLR, 0, Z0 - 0.5), (DISP_L + CLR + 2, 10, 3.7), tmp),
            cube("rib_top", (DISP_L + CLR, 0, 28.3), (DISP_L + CLR + 2, 10, Z1 + 0.5), tmp)]
    t = math.tan(math.radians(TILT))
    def z_desk(y):
        return OZ0 - FOOT_MIN - (Y_IN - y) * t
    foot = [(OY0, OZ0 + FOOT_EMBED), (Y_IN, OZ0 + FOOT_EMBED), (Y_IN, z_desk(Y_IN)), (OY0, z_desk(OY0))]
    adds += [prism("foot_L", foot, "x", OX0 + 4, OX0 + 26, tmp),
             prism("foot_R", foot, "x", OX1 - 26, OX1 - 4, tmp)]
    # ESP_CLAMP: ribs on top/bottom walls that press the ESP32 pin-side corners against the
    # cover posts (0.1 mm play). 45 deg underside so they print without support.
    for j, (xa, xb) in enumerate(((ESP_X0 + 0.3, ESP_X0 + 4), (ESP_X1 - 4, ESP_X1 - 0.3))):
        for top in (False, True):
            s = 1 if top else -1
            w = Z1 if top else Z0
            tip = (EZ1 - 0.6) if top else (EZ0 + 0.6)
            d = abs(tip - w)
            pts = [(Y_PIN - 2.5 - d, w + s * 0.5), (Y_PIN - 0.1, w + s * 0.5), (Y_PIN - 0.1, tip),
                   (Y_PIN - 2.5, tip), (Y_PIN - 2.5 - d, w)]
            adds.append(prism(f"clamp{j}{int(top)}", pts, "x", xa, xb, tmp))
    boolean(body, adds, "UNION")

    e = 0.5
    k = WIN_CHAMFER / FRONT
    cuts = [frustum_y("window", OY0 - e, e,
                      (WIN_L + 2 * k * (FRONT + e), WIN_H + 2 * k * (FRONT + e)),
                      (WIN_L - 2 * k * e, WIN_H - 2 * k * e), DISP_L / 2, DISP_H / 2, tmp)]
    for i in range(7):
        x = DISP_L + 3.5 + i * 3.2
        cuts.append(cube(f"vent_f{i}", (x, OY0 - 1, 5), (x + 1.6, 1, 27), tmp))
    for i in range(11):
        x = ESP_X0 + 5 + i * 4
        cuts.append(cube(f"vent_t{i}", (x, 22, Z1 - 1), (x + 1.6, 38, OZ1 + 1), tmp))
    # USB: shallow outside recess for the overmold + snug racetrack hole around the port
    cuts.append(cube("usb_recess", (OX1 - USB_RECESS, USB_Y - USB_H / 2, ESP_ZC - USB_W / 2),
                     (OX1 + 1, USB_Y + USB_H / 2, ESP_ZC + USB_W / 2), tmp))
    r = USB_PORT_H / 2
    cuts.append(cube("usb_port", (X1 - 1, USB_Y - r, ESP_ZC - USB_PORT_W / 2 + r),
                     (OX1 + 1, USB_Y + r, ESP_ZC + USB_PORT_W / 2 - r), tmp))
    for k, zc in enumerate((ESP_ZC - USB_PORT_W / 2 + r, ESP_ZC + USB_PORT_W / 2 - r)):
        cuts.append(cyl_x(f"usb_end{k}", USB_Y, zc, X1 - 1, OX1 + 1, r, tmp))
    n = 0
    for x in LATCH_X:
        for top in (False, True):
            cuts.append(latch_pocket(n, x, top, tmp))
            n += 1
    cuts.append(cube("pry", (60, Y_IN - 1.2, OZ0 - 1), (72, Y_IN + 0.1, Z0 + 0.01), tmp))  # pry notch
    boolean(body, cuts, "DIFFERENCE")
    return body


def build_cover(col, tmp):
    cover = cube("CASE_BackCover", (OX0, Y_IN, OZ0), (OX1, OY1, OZ1), col)
    bevel_except_plane_y(cover, Y_IN, EDGE_R)

    cuts = []
    # spring leaves for the 6 display pegs (thinned to 1.4 mm, U-slot 1 mm)
    for i, (px, pz) in enumerate(PEGS):
        s = -1 if pz < DISP_H / 2 else 1           # free end toward the nearest edge
        free, root = pz + s * 2.6, pz - s * 9.4
        za, zb = sorted((free, root))
        cuts.append(cube(f"leaf_thin{i}", (px - 3, Y_IN - 0.5, za), (px + 3, Y_IN + 1.1, zb), tmp))
        cuts.append(cube(f"leaf_sl{i}", (px - 4, Y_IN - 0.5, za), (px - 3, OY1 + 0.5, zb), tmp))
        cuts.append(cube(f"leaf_sr{i}", (px + 3, Y_IN - 0.5, za), (px + 4, OY1 + 0.5, zb), tmp))
        ea, eb = sorted((free, free + s * 1.0))
        cuts.append(cube(f"leaf_se{i}", (px - 4, Y_IN - 0.5, ea), (px + 4, OY1 + 0.5, eb), tmp))
    # BOOT flexure tab (6 x ~13 mm, 1.4 mm thick)
    tz0, tz1, tx0, tx1 = BOOT_Z - 1.0, BOOT_Z + 12.0, BOOT_X - 3.0, BOOT_X + 3.0
    cuts.append(cube("boot_thin", (tx0, Y_IN - 0.5, tz0), (tx1, Y_IN + 1.1, tz1), tmp))
    cuts.append(cube("boot_sl", (tx0 - 1, Y_IN - 0.5, tz0 - 1), (tx0, OY1 + 0.5, tz1), tmp))
    cuts.append(cube("boot_sr", (tx1, Y_IN - 0.5, tz0 - 1), (tx1 + 1, OY1 + 0.5, tz1), tmp))
    cuts.append(cube("boot_se", (tx0 - 1, Y_IN - 0.5, tz0 - 1), (tx1 + 1, OY1 + 0.5, tz0), tmp))
    boolean(cover, cuts, "DIFFERENCE")

    adds = []
    n = 0
    for x in LATCH_X:
        for top in (False, True):
            adds += latch_arm(n, x, top, tmp)
            n += 1
    adds.append(cube("tongue_L", (X0 + 0.22, Y_IN - 3, 8), (X0 + 1.5, Y_IN + 0.1, 24), tmp))
    # display pegs on the spring leaves
    for i, (px, pz) in enumerate(PEGS):
        shoulder = PCB_BACK_Y - PEG_PRELOAD
        adds.append(cyl_y(f"peg{i}", px, pz, shoulder, Y_IN + 1.6, 2.25, col=tmp))
        adds.append(cyl_y(f"pegtip{i}", px, pz, shoulder - 1.8, shoulder + 0.1, 1.0, 1.2, col=tmp))
    # ESP32 cradle: posts under the 4 holes, end stops (take USB plug/unplug force) and
    # side legs (hold it in Z). The board is clamped in Y by ESP_CLAMP ribs in the body
    # when the cover closes, so nothing has to flex.
    for i, (x, z) in enumerate(ESP_HOLES):
        adds.append(cyl_y(f"post{i}", x, z, Y_COMP, Y_IN + 0.1, 2.0, col=tmp))
    for j, (za, zb) in enumerate(((EZ0, EZ0 + 5.0), (EZ1 - 5.0, EZ1))):
        adds.append(cube(f"stopR{j}", (ESP_X1 + FIT, Y_PIN, za), (ESP_X1 + 1.95, Y_IN + 0.1, zb), tmp))
        adds.append(cube(f"stopL{j}", (ESP_X0 - 2.0, Y_PIN, za), (ESP_X0 - FIT, Y_IN + 0.1, zb), tmp))
    for j, (xa, xb) in enumerate(((ESP_X0, ESP_X0 + 4), (ESP_X1 - 4, ESP_X1))):
        adds.append(cube(f"legB{j}", (xa, Y_PIN, Z0 + 0.2), (xb, Y_IN + 0.1, EZ0 - FIT), tmp))
        adds.append(cube(f"legT{j}", (xa, Y_PIN, EZ1 + FIT), (xb, Y_IN + 0.1, Z1 - 0.2), tmp))
    # BOOT plunger on the flexure tab
    adds.append(cube("boot_post", (BOOT_X - 1.2, Y_COMP + BOOT_H + BOOT_GAP, BOOT_Z - 0.8),
                     (BOOT_X + 1.2, Y_IN + 1.6, BOOT_Z + 2.6), tmp))
    boolean(cover, adds, "UNION")
    boolean(cover, [engrave_text("BOOT", BOOT_X, BOOT_Z + 15.0, 3.5, tmp)], "DIFFERENCE")
    return cover


def test_piece(src, name, x_from, col, tmp, y_from=-50.0):
    ob = src.copy()
    ob.data = src.data.copy()
    ob.name = name
    col.objects.link(ob)
    boolean(ob, [cube("keep", (x_from, y_from, -50), (300, 100, 100), tmp)], "INTERSECT")
    return ob


def mesh_report(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    nm = sum(1 for e in bm.edges if not e.is_manifold)
    vol = abs(bm.calc_volume())
    bm.free()
    return {"non_manifold_edges": nm, "volume_cm3": round(vol / 1000, 2),
            "dims": [round(d, 2) for d in ob.dimensions]}


def export_stl(names, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for name in names:
        ob = bpy.data.objects[name]
        ob.hide_set(False)                 # export_selected skips objects that are hidden
        ob.hide_viewport = False
        bpy.context.view_layer.update()
        bpy.ops.object.select_all(action="DESELECT")
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.wm.stl_export(filepath=os.path.join(out_dir, f"{name}.stl"), export_selected_objects=True,
                              global_scale=1.0, use_scene_unit=False, apply_modifiers=True)


def export_print_ready(name, rot_x_deg, out_dir):
    """Export a copy already lying in its print orientation on z=0 (drag-drop into the slicer).

    rot_x_deg=+90 puts the -Y face (front / cut face) on the bed; -90 puts the +Y face
    (cover outer face) on the bed.
    """
    from mathutils import Matrix, Vector
    os.makedirs(out_dir, exist_ok=True)
    src = bpy.data.objects[name]
    me = src.data.copy()
    me.transform(src.matrix_world)
    me.transform(Matrix.Rotation(math.radians(rot_x_deg), 4, "X"))
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    me.transform(Matrix.Translation(Vector((-(min(xs) + max(xs)) / 2, -(min(ys) + max(ys)) / 2, -min(zs)))))
    ob = bpy.data.objects.new(name + "_print", me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.wm.stl_export(filepath=os.path.join(out_dir, f"{name}.stl"), export_selected_objects=True,
                          global_scale=1.0, use_scene_unit=False, apply_modifiers=True)
    dims = [round(d, 1) for d in ob.dimensions]
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(me)
    return dims


def main(save=True):
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.001
    sc.unit_settings.length_unit = "MILLIMETERS"
    col = get_col("CASE")
    test = get_col("CASE_TEST")
    test.hide_viewport = False             # booleans and export skip hidden collections
    test.hide_render = False
    tmp = get_col("CASE_tmp")
    body = build_body(col, tmp)
    cover = build_cover(col, tmp)
    tb = test_piece(body, "TEST_Body_rightend", 100.0, test, tmp, y_from=30.0)  # ESP/USB/latch zone only
    tc = test_piece(cover, "TEST_Cover_rightend", 100.0, test, tmp)
    bpy.data.collections.remove(tmp)
    rep = {o.name: mesh_report(o) for o in (body, cover, tb, tc)}
    if save:
        old = bpy.path.abspath("//stl") if bpy.data.filepath else None
        out = os.path.join(OUT_DIR, "stl")
        for f in ("CASE_Body.stl", "CASE_BackCover.stl"):
            p = os.path.join(out, f)
            if os.path.exists(p):
                os.remove(p)                      # drop v1 files so they cannot be printed by mistake
        # print-ready: already oriented for the bed, no supports needed
        rep["print_dims_mm"] = {
            "CASE_Body": export_print_ready("CASE_Body", 90, os.path.join(out, "v2")),
            "CASE_BackCover": export_print_ready("CASE_BackCover", -90, os.path.join(out, "v2")),
            "TEST_Body_rightend": export_print_ready("TEST_Body_rightend", 90, os.path.join(out, "v2_test")),
            "TEST_Cover_rightend": export_print_ready("TEST_Cover_rightend", -90, os.path.join(out, "v2_test")),
        }
        test.hide_viewport = True          # hide only after export (hidden objects cannot be selected)
        test.hide_render = True
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "gold_led_case.blend"))
    rep["outer_mm"] = [round(OX1 - OX0, 1), round(OY1 - OY0, 1), round(OZ1 - OZ0, 1)]
    rep["Y_IN"] = round(Y_IN, 2)
    return rep


result = main(save=True)
