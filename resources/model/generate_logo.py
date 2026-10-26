import bpy
import bmesh
import math
from mathutils import Vector, Matrix

# Reset scene
bpy.ops.wm.read_factory_settings(use_empty=True)

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = 512
scene.render.resolution_y = 512
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'

# Color management
if hasattr(scene, 'view_settings'):
    scene.view_settings.view_transform = 'AgX' if 'AgX' in [v.name for v in scene.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Filmic'
    scene.view_settings.look = 'Medium High Contrast'

# ----------------- METALLIC FRESNEL MATERIALS -----------------
def create_metal_material(name, base_color, metallic=0.92, roughness=0.22, coat=0.6, coat_roughness=0.1):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs['Base Color'].default_value = base_color
        bsdf.inputs['Metallic'].default_value = metallic
        bsdf.inputs['Roughness'].default_value = roughness
        if 'Coat Weight' in bsdf.inputs:
            bsdf.inputs['Coat Weight'].default_value = coat
            bsdf.inputs['Coat Roughness'].default_value = coat_roughness
        elif 'Clearcoat' in bsdf.inputs:
            bsdf.inputs['Clearcoat'].default_value = coat
            bsdf.inputs['Clearcoat Roughness'].default_value = coat_roughness
    return mat

mat_plate_face = create_metal_material("PlateFace", (0.18, 0.19, 0.22, 1.0), metallic=0.92, roughness=0.28, coat=0.6, coat_roughness=0.10)
mat_plate_rim = create_metal_material("PlateRim", (0.80, 0.82, 0.86, 1.0), metallic=0.96, roughness=0.14, coat=0.8, coat_roughness=0.05)
mat_arctic_silver = create_metal_material("ArcticSilver", (0.82, 0.84, 0.88, 1.0), metallic=0.96, roughness=0.16, coat=0.8, coat_roughness=0.06)
mat_mineral_grey = create_metal_material("MineralGrey", (0.36, 0.38, 0.42, 1.0), metallic=0.90, roughness=0.25, coat=0.5, coat_roughness=0.12)
mat_deep_titanium = create_metal_material("DeepTitanium", (0.14, 0.15, 0.17, 1.0), metallic=0.90, roughness=0.32, coat=0.4, coat_roughness=0.15)
mat_blade_alloy = create_metal_material("BladeAlloy", (0.76, 0.78, 0.82, 1.0), metallic=0.95, roughness=0.14, coat=0.7, coat_roughness=0.05)

# ----------------- ROUNDED RECTANGLE SQUIRCLE TILE -----------------
plate_w = 3.6
plate_r = 0.75
plate_d = 0.28

def create_squircle_mesh(width, corner_r, depth):
    mesh = bpy.data.meshes.new("SquircleMesh")
    bm = bmesh.new()

    half = width / 2.0
    r = corner_r
    corner_steps = 14

    points_2d = []
    corners = [
        (half - r, half - r, 0),
        (-half + r, half - r, math.pi/2),
        (-half + r, -half + r, math.pi),
        (half - r, -half + r, 3*math.pi/2)
    ]
    for cx, cy, start_a in corners:
        for s in range(corner_steps):
            a = start_a + (s / float(corner_steps)) * (math.pi / 2.0)
            points_2d.append((cx + r * math.cos(a), cy + r * math.sin(a)))

    n = len(points_2d)
    v_bot = [bm.verts.new((x, y, 0.0)) for x, y in points_2d]
    v_top = [bm.verts.new((x, y, depth)) for x, y in points_2d]

    for i in range(n):
        i_next = (i + 1) % n
        bm.faces.new([v_bot[i], v_bot[i_next], v_top[i_next], v_top[i]])

    bm.faces.new(v_top)
    bm.faces.new(reversed(v_bot))

    bm.to_mesh(mesh)
    bm.free()
    return mesh

# 1. Base Squircle Plate
plate_obj = bpy.data.objects.new("IconSquirclePlate", create_squircle_mesh(plate_w, plate_r, plate_d))
scene.collection.objects.link(plate_obj)
plate_obj.data.materials.append(mat_plate_face)
bev_p = plate_obj.modifiers.new(name="Bevel", type='BEVEL')
bev_p.width = 0.04
bev_p.segments = 4

# 2. Outer Chamfered Metallic Border Frame for Squircle
border_obj = bpy.data.objects.new("PlateBorderFrame", create_squircle_mesh(plate_w + 0.12, plate_r + 0.04, plate_d * 0.90))
scene.collection.objects.link(border_obj)
border_obj.location = (0, 0, -0.02)
border_obj.data.materials.append(mat_plate_rim)
bev_b = border_obj.modifiers.new(name="Bevel", type='BEVEL')
bev_b.width = 0.05
bev_b.segments = 4

# ----------------- EMBOSSED CIRCULAR WELL & FAN ASSEMBLY -----------------
fan_z = plate_d + 0.01

# Recessed Circular Well Ring (Outer well radius 1.30, sits comfortably inside 3.6 plate!)
bpy.ops.mesh.primitive_cylinder_add(radius=1.30, depth=0.18, vertices=96, location=(0, 0, fan_z + 0.09))
well_ring = bpy.context.active_object
well_ring.name = "WellRim"
well_ring.data.materials.append(mat_mineral_grey)
bev_wr = well_ring.modifiers.new(name="Bevel", type='BEVEL')
bev_wr.width = 0.025
bev_wr.segments = 4

bpy.ops.mesh.primitive_cylinder_add(radius=1.18, depth=0.30, vertices=96, location=(0, 0, fan_z + 0.09))
cutter = bpy.context.active_object
cutter.display_type = 'WIRE'
cutter.hide_render = True
bool_wr = well_ring.modifiers.new(name="Hollow", type='BOOLEAN')
bool_wr.operation = 'DIFFERENCE'
bool_wr.object = cutter

# Thin Bright Silver Inner Well Lip
bpy.ops.mesh.primitive_cylinder_add(radius=1.28, depth=0.22, vertices=96, location=(0, 0, fan_z + 0.10))
lip = bpy.context.active_object
lip.name = "WellLip"
lip.data.materials.append(mat_arctic_silver)
bev_lip = lip.modifiers.new(name="Bevel", type='BEVEL')
bev_lip.width = 0.015
bev_lip.segments = 3

bpy.ops.mesh.primitive_cylinder_add(radius=1.22, depth=0.40, vertices=96, location=(0, 0, fan_z + 0.10))
cutter2 = bpy.context.active_object
cutter2.display_type = 'WIRE'
cutter2.hide_render = True
bool_lip = lip.modifiers.new(name="Hollow", type='BOOLEAN')
bool_lip.operation = 'DIFFERENCE'
bool_lip.object = cutter2

# Central Hub System
# Hub Base (Deep Titanium)
bpy.ops.mesh.primitive_cylinder_add(radius=0.42, depth=0.20, vertices=96, location=(0, 0, fan_z + 0.10))
hub_base = bpy.context.active_object
hub_base.name = "HubBase"
hub_base.data.materials.append(mat_deep_titanium)
bev_hb = hub_base.modifiers.new(name="Bevel", type='BEVEL')
bev_hb.width = 0.02
bev_hb.segments = 4

# Concentric Hub Ring (Arctic Silver)
bpy.ops.mesh.primitive_cylinder_add(radius=0.28, depth=0.24, vertices=96, location=(0, 0, fan_z + 0.12))
hub_mid = bpy.context.active_object
hub_mid.name = "HubMid"
hub_mid.data.materials.append(mat_arctic_silver)
bev_hm = hub_mid.modifiers.new(name="Bevel", type='BEVEL')
bev_hm.width = 0.018
bev_hm.segments = 4

# Center Hub Button (Mineral Grey)
bpy.ops.mesh.primitive_cylinder_add(radius=0.16, depth=0.28, vertices=96, location=(0, 0, fan_z + 0.14))
hub_cap = bpy.context.active_object
hub_cap.name = "HubCap"
hub_cap.data.materials.append(mat_mineral_grey)
bev_hc = hub_cap.modifiers.new(name="Bevel", type='BEVEL')
bev_hc.width = 0.025
bev_hc.segments = 4

# 11 Metallic Aerodynamic Blades
def create_blade_mesh():
    mesh = bpy.data.meshes.new("BladeMesh")
    bm = bmesh.new()

    r_in = 0.40
    r_out = 1.16
    steps = 14
    width_in = 0.13
    width_out = 0.23

    verts_front = []
    verts_back = []

    for i in range(steps + 1):
        t = i / float(steps)
        r = r_in + t * (r_out - r_in)
        angle_sweep = -0.48 * math.pow(t, 1.2)
        z_pitch = 0.02 + 0.16 * (1.0 - t * 0.35)
        w = width_in + t * (width_out - width_in)

        x0 = r * math.cos(angle_sweep)
        y0 = r * math.sin(angle_sweep)
        z0 = z_pitch

        x1 = r * math.cos(angle_sweep + w / r)
        y1 = r * math.sin(angle_sweep + w / r)
        z1 = 0.015

        verts_front.append(bm.verts.new((x0, y0, z0)))
        verts_back.append(bm.verts.new((x1, y1, z1)))

    bm.verts.ensure_lookup_table()
    for i in range(steps):
        bm.faces.new([
            verts_front[i],
            verts_front[i + 1],
            verts_back[i + 1],
            verts_back[i]
        ])

    bm.to_mesh(mesh)
    bm.free()
    return mesh

blade_base_mesh = create_blade_mesh()
num_blades = 11

blade_parent = bpy.data.objects.new("BladeAssembly", None)
scene.collection.objects.link(blade_parent)
blade_parent.location = (0, 0, fan_z)

for b_idx in range(num_blades):
    b_obj = bpy.data.objects.new(f"Blade_{b_idx}", blade_base_mesh)
    scene.collection.objects.link(b_obj)
    b_obj.parent = blade_parent
    b_obj.rotation_euler[2] = b_idx * (2.0 * math.pi / num_blades)
    b_obj.data.materials.append(mat_blade_alloy)

    sol = b_obj.modifiers.new(name="Solidify", type='SOLIDIFY')
    sol.thickness = 0.016
    sol.offset = -1.0

    bev = b_obj.modifiers.new(name="Bevel", type='BEVEL')
    bev.width = 0.004
    bev.segments = 2

# ----------------- CAMERA: FRONT-FACING PERFECT ICON FRAMING -----------------
cam_data = bpy.data.cameras.new("LogoCamera")
cam_data.lens = 95.0
cam_data.clip_start = 0.1
cam_data.clip_end = 50.0

cam_obj = bpy.data.objects.new("LogoCamera", cam_data)
scene.collection.objects.link(cam_obj)
scene.camera = cam_obj

# Camera looking straight onto the icon tile face with 2.5° elevation for specular edge glints
# Distance 8.6 leaves a clean, generous 14% padding around the squircle tile! ZERO CLIPPING!
cam_distance = 8.6
tilt_angle = math.radians(2.5)
cam_obj.location = (0.0, -cam_distance * math.sin(tilt_angle), cam_distance * math.cos(tilt_angle))

direction = Vector((0, 0, plate_d / 2.0)) - cam_obj.location
rot_quat = direction.to_track_quat('-Z', 'Y')
cam_obj.rotation_euler = rot_quat.to_euler()

# ----------------- SOFT STUDIO FRESNEL LIGHTING -----------------
# 1. Main Studio Key Light (Top-Left softbox)
light_key_data = bpy.data.lights.new(name="KeySoftbox", type='AREA')
light_key_data.energy = 280.0
light_key_data.size = 6.0
light_key_data.color = (0.97, 0.98, 1.0)
light_key = bpy.data.objects.new("KeySoftbox", light_key_data)
scene.collection.objects.link(light_key)
light_key.location = (-4.5, -4.5, 6.0)
light_key.rotation_euler = (math.radians(35), math.radians(-25), math.radians(-45))

# 2. Diffused Fill Light (Bottom-Right)
light_fill_data = bpy.data.lights.new(name="FillSoftbox", type='AREA')
light_fill_data.energy = 120.0
light_fill_data.size = 6.5
light_fill_data.color = (0.88, 0.90, 0.94)
light_fill = bpy.data.objects.new("FillSoftbox", light_fill_data)
scene.collection.objects.link(light_fill)
light_fill.location = (4.5, 4.0, 5.0)

# 3. Top Rim Light for Bevel Reflections
light_rim_data = bpy.data.lights.new(name="RimLight", type='AREA')
light_rim_data.energy = 160.0
light_rim_data.size = 5.0
light_rim_data.color = (1.0, 1.0, 1.0)
light_rim = bpy.data.objects.new("RimLight", light_rim_data)
scene.collection.objects.link(light_rim)
light_rim.location = (0.0, 5.0, 4.2)

# Save .blend file
blend_path = "/home/usuario/Proyectos/Otros/LenovoDrivers/lenovo-fanboost-gui/resources/model/lenovo-fanboost-logo.blend"
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

# Render high-res master
render_path = "/home/usuario/Proyectos/Otros/LenovoDrivers/lenovo-fanboost-gui/resources/icons/lenovo-fanboost-gui-master.png"
scene.render.filepath = render_path
bpy.ops.render.render(write_still=True)

print("Render complete:", render_path)
