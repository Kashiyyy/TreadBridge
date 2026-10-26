"""Joins the rendered PPM screens into docs/images/display-portrait.png and display-landscape.png."""
import sys
from PIL import Image, ImageDraw, ImageFont

def font(bold, size):
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if bold else ""),
                 "C:/Windows/Fonts/%s.ttf" % ("arialbd" if bold else "arial"),
                 "/System/Library/Fonts/Supplemental/Arial%s.ttf" % (" Bold" if bold else "")):
        try: return ImageFont.truetype(path, size)
        except OSError: pass
    return ImageFont.load_default()

NAMES = [("profile", "Profile selection"), ("live", "LIVE"), ("device", "DEVICE"), ("foot", "FOOT"), ("sys", "SYS")]
BG = (13, 15, 20)

def framed(im, scale, pad=14):
    im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    out = Image.new("RGB", (im.width + 2 * pad, im.height + 2 * pad), (0, 0, 0))
    mask = Image.new("L", out.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, out.width - 1, out.height - 1], radius=22, fill=255)
    out.paste(im, (pad, pad))
    res = Image.new("RGBA", out.size)
    res.paste(out.convert("RGBA"), (0, 0), mask)
    return res

def sheet(prefix, scale, cols, outfile, title):
    tiles = [(framed(Image.open("%s_%s.ppm" % (prefix, k)).convert("RGB"), scale), lab) for k, lab in NAMES]
    tw = max(t.width for t, _ in tiles); th = max(t.height for t, _ in tiles)
    gx, gy, lab_h, top = 28, 24, 34, 70
    rows = (len(tiles) + cols - 1) // cols
    img = Image.new("RGB", (cols * tw + (cols + 1) * gx, top + rows * (th + lab_h + gy) + gy), BG)
    d = ImageDraw.Draw(img)
    d.text((gx, 20), title, font=font(True, 28), fill=(235, 238, 242))
    for i, (t, lab) in enumerate(tiles):
        r, c = divmod(i, cols)
        x = gx + c * (tw + gx) + (tw - t.width) // 2
        y = top + r * (th + lab_h + gy)
        img.paste(t, (x, y), t)
        w = d.textlength(lab, font=font(True, 20))
        d.text((gx + c * (tw + gx) + (tw - w) / 2, y + t.height + 6), lab, font=font(True, 20), fill=(0, 200, 140))
    img.save(outfile, optimize=True)
    print(outfile, img.size)

out = sys.argv[1] if len(sys.argv) > 1 else "."
sheet("port", 2, 5, out + "/display-portrait.png", "TreadBridge display - portrait")
sheet("land", 2, 2, out + "/display-landscape.png", "TreadBridge display - landscape")
