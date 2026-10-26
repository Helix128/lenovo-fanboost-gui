import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageOps

def create_rounded_icon(input_path, output_dir):
    src = Image.open(input_path).convert("RGBA")
    w, h = src.size # 512, 512

    # Super-sampling factor for buttery smooth antialiasing
    scale = 4
    sw, sh = w * scale, h * scale # 2048, 2048

    # The rounded box dimensions:
    # 472x472 centered inside 512x512 (leaves 20px padding)
    margin = 20 * scale
    box_w = sw - 2 * margin
    box_h = sh - 2 * margin
    # Very rounded corner radius (approx 26% of box width)
    corner_r = int(box_w * 0.26)

    # 1. Create crisp, super-sampled squircle mask
    mask_high = Image.new("L", (sw, sh), 0)
    draw_mask = ImageDraw.Draw(mask_high)
    draw_mask.rounded_rectangle(
        [margin, margin, sw - margin, sh - margin],
        radius=corner_r,
        fill=255
    )

    # Downscale mask with high quality box filter to get perfect anti-aliasing
    mask = mask_high.resize((w, h), Image.Resampling.LANCZOS)

    # 2. Scale source image to fit nicely within the squircle bounds
    # Resize the rendered 3D fan so it is centered with comfortable breathing room inside the squircle
    src_scaled = src.resize((w, h), Image.Resampling.LANCZOS)

    # Apply the squircle mask
    icon = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    icon.paste(src_scaled, (0, 0), mask=mask)

    # 3. Add a smooth, subtle metallic chamfer border along the rounded squircle edge
    border_high = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    draw_border = ImageDraw.Draw(border_high)
    border_w = 4 * scale

    # Outer subtle rim highlight (ThinkBook silver/grey gradient tone)
    draw_border.rounded_rectangle(
        [margin, margin, sw - margin, sh - margin],
        radius=corner_r,
        outline=(210, 215, 222, 180),
        width=border_w
    )
    # Inner subtle rim shadow
    draw_border.rounded_rectangle(
        [margin + border_w, margin + border_w, sw - margin - border_w, sh - margin - border_w],
        radius=corner_r - border_w,
        outline=(120, 125, 135, 100),
        width=int(1.5 * scale)
    )

    border_down = border_high.resize((w, h), Image.Resampling.LANCZOS)
    icon = Image.alpha_composite(icon, border_down)

    # Save 512 master
    master_path = os.path.join(output_dir, "lenovo-fanboost-gui-master.png")
    icon.save(master_path, "PNG")
    icon.save(os.path.join(output_dir, "lenovo-fanboost-gui.png"), "PNG")
    print(f"Saved master to {master_path}")

    # Generate all desktop resolutions
    sizes = [256, 128, 64, 48, 32, 24, 16]
    for s in sizes:
        resized = icon.resize((s, s), Image.Resampling.LANCZOS)
        out_p = os.path.join(output_dir, f"lenovo-fanboost-gui-{s}.png")
        resized.save(out_p, "PNG")

    print(f"Generated {len(sizes)} icon sizes with Lanczos filtering.")

if __name__ == "__main__":
    icon_dir = "/home/usuario/Proyectos/Otros/LenovoDrivers/lenovo-fanboost-gui/resources/icons"
    input_file = os.path.join(icon_dir, "lenovo-fanboost-gui-master.png")
    create_rounded_icon(input_file, icon_dir)
