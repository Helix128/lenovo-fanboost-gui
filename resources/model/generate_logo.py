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

mat_plate_base = create_metal_material("PlateBase", (0.16, 0.17, 0.19, 1.0), metallic=0.92, roughness=0.32, coat=0.5, coat_roughness=0.15)
mat_plate_border = create_metal_material("PlateBorder", (0.78, 0.80, 0.84, 1.0), metallic=0.96, roughness=0.15, coat=0.8, coat_roughness=0.06)
mat_arctic_silver = create_metal_material("ArcticSilver", (0.84, 0.86, 0.89, 1.0), metallic=0.96, roughness=0.18, coat=0.8, coat_roughness=0.08)
mat_mineral_grey = create_metal_material("MineralGrey", (0.38, 0.40, 0.45, 1.0), metallic=0.90, roughness=0.26, coat=0.5, coat_roughness=0.12)
mat_deep_titanium = create_metal_material("DeepTitanium", (0.22, 0.24, 0.27, 1.0), metallic=0.88, roughness=0.32, coat=0.4, coat_roughness=0.15)
mat_blade_alloy = create_metal_material("BladeAlloy", (0.75, 0.77, 0.81, 1.0), metallic=0.95, roughness=0.16, coat=0.7, coat_roughness=0.06)

# ----------------- ROUNDED RECTANGLE ICON BOX -----------------
# Create rounded rectangle squircle plate
plate_size = 4.2
plate_corner_radius = 0.85
plate_depth = 0.30

# Generate squircle geometry with bmesh
def create_squircle_plate():
    mesh = bpy.data.meshes.new("PlateMesh")
    bm = bmesh.new()

    half = plate_size / 2.0
    r = plate_corner_radius
    corner_steps = 12

    # Perimeter 2D points
    points_2d = []
    # 4 corners centers
    corners = [
        (half - r, half - r, 0),             # Top-Right
        (-half + r, half - r, math.pi/2),     # Top-Left
        (-half + r, -half + r, math.pi),     # Bottom-Left
        (half - r, -half + r, 3*math.pi/2)    # Bottom-Right
    ]

    for cx, cy, start_a in corners:
        for s in range(corner_steps):
            a = start_a + (s / float(corner_steps)) * (math.pi / 2.0)
            points_2d.append((cx + r * math.cos(a), cy + r * math.sin(a)))

    # Create top and bottom faces
    verts_bottom = [bm.verts.new((x, y, 0.0)) for x, y in points_2d]
    verts_top = [bm.verts.new((x, y, plate_depth)) for x, y in points_2d]

    # Create sides
    n = len(points_2d)
    for i in range(n):
        i_next = (i + 1) % n
        bm.faces.new([verts_bottom[i], verts_bottom[i_next], verts_top[i_next], verts_top[i]])

    # Create top and bottom caps
    bm.faces.new(verts_top)
    bm.faces.new(reversed(verts_bottom))

    bm.to_mesh(mesh)
    bm.free()
    return mesh

plate_mesh = create_squircle_plate()
plate_obj = bpy.data.objects.new("IconPlate", plate_mesh)
scene.collection.objects.link(plate_obj)
plate_obj.data.materials.append(mat_plate_base)

# Smooth bevel on plate edges
bev_p = plate_obj.modifiers.new(name="Bevel", type='BEVEL')
bev_p.width = 0.04
bev_p.segments = 4

# Outer raised metallic border lip for the squircle
border_mod = plate_obj.modifiers.new(name="Subsurf", type='SUBSURF')
border_mod.levels = 1
border_mod.render_levels = 1

# Concentric chamfered inner border ring on the squircle plate
def create_border_ring():
    mesh = bpy.data.meshes.new("BorderRingMesh")
    bm = bmesh.new()
    half = (plate_size - 0.16) / 2.0
    r = plate_corner_radius - 0.08
    corner_steps = 12

    points_out = []
    corners = [
        (half - r, half - r, 0),
        (-half + r, half - r, math.pi/2),
        (-half + r, -half + r, math.pi),
        (half - r, -half + r, 3*math.pi/2)
    ]
    for cx, cy, start_a in corners:
        for s in range(corner_steps):
            a = start_a + (s / float(corner_steps)) * (math.pi / 2.0)
            points_out.append((cx + r * math.cos(a), cy + r * math.sin(a)))

    # Inner profile
    points_in = []
    half_in = half - 0.08
    r_in = r - 0.04
    corners_in = [
        (half_in - r_in, half_in - r_in, 0),
        (-half_in + r_in, half_in - r_in, math.pi/2),
        (-half_in + r_in, -half_in + r_in, math.pi),
        (half_in - r_in, -half_in + r_in, 3*math.pi/2)
    ]
    for cx, cy, start_a in corners_in:
        for s in range(corner_steps):
            a = start_a + (s / float(corner_steps)) * (math.pi / 2.0)
            points_in.append((cx + r_in * math.cos(a), cy + r_in * math.sin(a)))

    n = len(points_out)
    v_out = [bm.verts.new((x, y, plate_depth + 0.02)) for x, y in points_out]
    v_in = [bm.verts.new((x, y, plate_depth + 0.02)) for x, y in points_in]

    for i in range(n):
        i_next = (i + 1) % n
        bm.faces.new([v_out[i], v_out[i_next], v_in[i_next], v_in[i]])

    bm.to_mesh(mesh)
    bm.free()
    return mesh

border_obj = bpy.data.objects.new("PlateBorderBezel", create_border_ring())
scene.collection.objects.link(border_obj)
border_obj.data.materials.append(mat_plate_border)
sol_b = border_obj.modifiers.new(name="Solidify", type='SOLIDIFY')
sol_b.thickness = 0.03
bev_b = border_obj.modifiers.new(name="Bevel", type='BEVEL')
bev_b.width = 0.008
bev_b.segments = 3

# ----------------- METALLIC FAN ASSEMBLY -----------------
fan_z_offset = plate_depth + 0.02

# 1. Outer Circular Fan Well Rim (Deep Titanium)
bpy.ops.mesh.primitive_cylinder_add(radius=1.56, depth=0.20, vertices=96, location=(0, 0, fan_z_offset + 0.10))
outer_fan_rim = bpy.context.active_object
outer_fan_rim.name = "OuterFanRim"
outer_fan_rim.data.materials.append(mat_mineral_grey)
bev_ofr = outer_fan_rim.modifiers.new(name="Bevel", type='BEVEL')
bev_ofr.width = 0.025
bev_ofr.segments = 4

bpy.ops.mesh.primitive_cylinder_add(radius=1.42, depth=0.35, vertices=96, location=(0, 0, fan_z_offset + 0.10))
rim_cutter = bpy.context.active_object
rim_cutter.display_type = 'WIRE'
rim_cutter.hide_render = True
bool_ofr = outer_fan_rim.modifiers.new(name="Hollow", type='BOOLEAN')
bool_ofr.operation = 'DIFFERENCE'
bool_ofr.object = rim_cutter

# 2. Central Hub Base
bpy.ops.mesh.primitive_cylinder_add(radius=0.52, depth=0.22, vertices=96, location=(0, 0, fan_z_offset + 0.11))
hub_base = bpy.context.active_object
hub_base.name = "HubBase"
hub_base.data.materials.append(mat_deep_titanium)
bev_hb = hub_base.modifiers.new(name="Bevel", type='BEVEL')
bev_hb.width = 0.02
bev_hb.segments = 4

# 3. Concentric Hub Ring (Arctic Silver)
bpy.ops.mesh.primitive_cylinder_add(radius=0.36, depth=0.26, vertices=96, location=(0, 0, fan_z_offset + 0.13))
hub_mid = bpy.context.active_object
hub_mid.name = "HubMid"
hub_mid.data.materials.append(mat_arctic_silver)
bev_hm = hub_mid.modifiers.new(name="Bevel", type='BEVEL')
bev_hm.width = 0.02
bev_hm.segments = 4

# 4. Center Hub Cap (Brushed Alloy)
bpy.ops.mesh.primitive_cylinder_add(radius=0.20, depth=0.30, vertices=96, location=(0, 0, fan_z_offset + 0.15))
hub_cap = bpy.context.active_object
hub_cap.name = "HubCap"
hub_cap.data.materials.append(mat_mineral_grey)
bev_hc = hub_cap.modifiers.new(name="Bevel", type='BEVEL')
bev_hc.width = 0.03
bev_hc.segments = 4

# 5. Aerodynamic Fan Blades (11 curved blades)
def create_blade_mesh():
    mesh = bpy.data.meshes.new("BladeMesh")
    bm = bmesh.new()

    r_in = 0.50
    r_out = 1.40
    steps = 14
    width_in = 0.16
    width_out = 0.28

    verts_front = []
    verts_back = []

    for i in range(steps + 1):
        t = i / float(steps)
        r = r_in + t * (r_out - r_in)
        angle_sweep = -0.50 * math.pow(t, 1.2)
        z_pitch = 0.03 + 0.18 * (1.0 - t * 0.35)
        w = width_in + t * (width_out - width_in)

        x0 = r * math.cos(angle_sweep)
        y0 = r * math.sin(angle_sweep)
        z0 = z_pitch

        x1 = r * math.cos(angle_sweep + w / r)
        y1 = r * math.sin(angle_sweep + w / r)
        z1 = 0.02

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
blade_parent.location = (0, 0, fan_z_offset)

for b_idx in range(num_blades):
    b_obj = bpy.data.objects.new(f"Blade_{b_idx}", blade_base_mesh)
    scene.collection.objects.link(b_obj)
    b_obj.parent = blade_parent
    b_obj.rotation_euler[2] = b_idx * (2.0 * math.pi / num_blades)
    b_obj.data.materials.append(mat_blade_alloy)

    sol = b_obj.modifiers.new(name="Solidify", type='SOLIDIFY')
    sol.thickness = 0.018
    sol.offset = -1.0

    bev = b_obj.modifiers.new(name="Bevel", type='BEVEL')
    bev.width = 0.005
    bev.segments = 2

# ----------------- CAMERA: DIRECT FRONT-FACING -----------------
cam_data = bpy.data.cameras.new("LogoCamera")
cam_data.lens = 92.0
cam_data.clip_start = 0.1
cam_data.clip_end = 50.0

cam_obj = bpy.data.objects.new("LogoCamera", cam_data)
scene.collection.objects.link(cam_obj)
scene.camera = cam_obj

# Camera looking straight into the face of the icon tile with subtle 3.5° elevation for specular highlights
cam_distance = 7.4
tilt_angle = math.radians(3.5)
cam_obj.location = (0.0, -cam_distance * math.sin(tilt_angle), cam_distance * math.cos(tilt_angle))

direction = Vector((0, 0, plate_depth / 2.0)) - cam_obj.location
rot_quat = direction.to_track_quat('-Z', 'Y')
cam_obj.rotation_euler = rot_quat.to_euler()

# ----------------- STUDIO FRESNEL METALLIC LIGHTING -----------------
# 1. Top-Left Key Softbox
light_key_data = bpy.data.lights.new(name="KeySoftbox", type='AREA')
light_key_data.energy = 260.0
light_key_data.size = 5.0
light_key_data.color = (0.98, 0.99, 1.0)
light_key = bpy.data.objects.new("KeySoftbox", light_key_data)
scene.collection.objects.link(light_key)
light_key.location = (-4.2, -4.5, 5.5)
light_key.rotation_euler = (math.radians(35), math.radians(-25), math.radians(-45))

# 2. Bottom-Right Fill Softbox
light_fill_data = bpy.data.lights.new(name="FillSoftbox", type='AREA')
light_fill_data.energy = 110.0
light_fill_data.size = 6.0
light_fill_data.color = (0.88, 0.90, 0.94)
light_fill = bpy.data.objects.new("FillSoftbox", light_fill_data)
scene.collection.objects.link(light_fill)
light_fill.location = (4.5, 4.0, 4.5)

# 3. Top Rim Light for Bevel Glint
light_rim_data = bpy.data.lights.new(name="RimLight", type='AREA')
light_rim_data.energy = 140.0
light_rim_data.size = 4.5
light_rim_data.color = (1.0, 1.0, 1.0)
light_rim = bpy.data.objects.new("RimLight", light_rim_data)
scene.collection.objects.link(light_rim)
light_rim.location = (0.0, 5.0, 3.8)

# Save .blend file
blend_path = "/home/usuario/Proyectos/Otros/LenovoDrivers/lenovo-fanboost-gui/resources/model/lenovo-fanboost-logo.blend"
bpy.ops.wm.save_as_mainfile(filepath=blend_path)

# Render high-res master
render_path = "/home/usuario/Proyectos/Otros/LenovoDrivers/lenovo-fanboost-gui/resources/icons/lenovo-fanboost-gui-master.png"
scene.render.filepath = render_path
bpy.ops.render.render(write_still=True)

print("Render complete:", render_path)
