#!/usr/bin/env python3
"""
Extract authentic Makaimura (Revision G) sprites and Stage 1 BG tiles directly
from reference/makaimurg.zip and quantize them to NES 2bpp CHR-RAM format
in BANK10 (src/arcade-gfx-data.inc).
"""
import zipfile, os
from PIL import Image

z = zipfile.ZipFile("reference/makaimurg.zip")
spr_lo = b"".join(z.read(f) for f in ["gng13.n4", "gg16.bin", "gg15.bin"])
spr_hi = b"".join(z.read(f) for f in ["gng16.l4", "gg13.bin", "gg12.bin"])
tiles_rom = b"".join(z.read(f) for f in ["gg11.bin", "gg10.bin", "gg9.bin", "gg8.bin", "gg7.bin", "gg6.bin"])

def decode_sprite_16x16(code):
    base = code * 64
    px = [[15]*16 for _ in range(16)]
    for y in range(16):
        for half in range(2):
            off = base + half * 32 + y * 2
            for byte_i in range(2):
                a = spr_lo[off + byte_i]
                b = spr_hi[off + byte_i]
                for bit in range(4):
                    sh = 3 - bit
                    p0 = (a >> (sh + 4)) & 1
                    p1 = (a >> sh) & 1
                    p2 = (b >> (sh + 4)) & 1
                    p3 = (b >> sh) & 1
                    px[y][half * 8 + byte_i * 4 + bit] = (p0 << 3) | (p1 << 2) | (p2 << 1) | p3
    return px

def decode_bg_16x16(code):
    base = (code & 0x3ff) * 32
    p2 = tiles_rom[base : base + 32]
    p1 = tiles_rom[0x8000 + base : 0x8000 + base + 32]
    p0 = tiles_rom[0x10000 + base : 0x10000 + base + 32]
    px = [[0]*16 for _ in range(16)]
    for y in range(16):
        for half in range(2):
            b0 = p0[half * 16 + y]
            b1 = p1[half * 16 + y]
            b2 = p2[half * 16 + y]
            for x in range(8):
                bit = 7 - x
                px[y][half * 8 + x] = (((b0 >> bit) & 1) << 2) | (((b1 >> bit) & 1) << 1) | ((b2 >> bit) & 1)
    return px

def compose_32x32_spr(tl, tr, bl, br):
    grid = [[15]*32 for _ in range(32)]
    for (ox, oy, c) in [(0,0,tl), (16,0,tr), (0,16,bl), (16,16,br)]:
        if c is None: continue
        p = decode_sprite_16x16(c)
        for y in range(16):
            for x in range(16):
                grid[oy+y][ox+x] = p[y][x]
    return grid

def crop_center_16x32(grid32, dx=0, dy=0):
    """Find horizontal bounding box of non-transparent pixels, center into 16x32."""
    xs = [x for y in range(32) for x in range(32) if grid32[y][x] != 15]
    if not xs:
        return [[15]*16 for _ in range(32)]
    cx = (min(xs) + max(xs) + 1) // 2 + dx
    x0 = max(0, min(16, cx - 8))
    out = [[15]*16 for _ in range(32)]
    for y in range(32):
        sy = y + dy
        if 0 <= sy < 32:
            for x in range(16):
                out[y][x] = grid32[sy][x0 + x]
    return out

def crop_center_16x16(grid32, align_bottom=True):
    xs = [x for y in range(32) for x in range(32) if grid32[y][x] != 15]
    ys = [y for y in range(32) for x in range(32) if grid32[y][x] != 15]
    if not xs:
        return [[15]*16 for _ in range(16)]
    cx = (min(xs) + max(xs) + 1) // 2
    x0 = max(0, min(16, cx - 8))
    if align_bottom:
        y0 = max(0, min(16, max(ys) - 15))
    else:
        cy = (min(ys) + max(ys) + 1) // 2
        y0 = max(0, min(16, cy - 8))
    return [[grid32[y0+y][x0+x] for x in range(16)] for y in range(16)]

# Map 4bpp arcade sprite color indices (0..14, 15=trans) to NES 2bpp (0=trans, 1=dark, 2=mid, 3=bright)
# Arcade makaimurg sprite palettes have specific semantic roles per index:
# Let's define clean per-palette index maps tuned to makaimurg's actual sprite indices!
# In makaimurg:
#   0 = brightest/white, 1-3 = silver/gray/highlights, 4-5 = red/dark, 6-7 = skin/gold,
#   8-9 = dark outline/shadow, 10-11 = blue/mid, 12-13 = purple/accent, 14 = flesh/light, 15 = transparent
SPR_MAP_ARMOR = {
    15: 0,
    8: 1, 9: 1, 3: 1, 5: 1, 11: 1, 13: 1,      # dark outline/steel shadow
    2: 2, 6: 2, 7: 2, 10: 2, 12: 2, 14: 2, 4: 2,# mid silver/skin/belt
    0: 3, 1: 3,                                  # bright steel highlight
}
SPR_MAP_NAKED = {
    15: 0,
    8: 1, 9: 1, 4: 1, 5: 1, 3: 1, 11: 1,        # dark/red boxers & outline
    6: 2, 7: 2, 12: 2, 13: 2, 14: 2, 2: 2,      # skin tone
    0: 3, 1: 3, 10: 3,                           # white highlights/beard
}
SPR_MAP_ZOMBIE = {
    15: 0,
    8: 1, 9: 1, 10: 1, 11: 1, 12: 1, 13: 1, 3: 1,# blue coat / shadow
    4: 2, 5: 2, 6: 2, 7: 2, 2: 2,                # red hair/eyes/detail
    0: 3, 1: 3, 14: 3,                           # pale white face/hands
}
SPR_MAP_MONSTER = {
    15: 0,
    8: 1, 9: 1, 11: 1, 12: 1, 13: 1, 5: 1,      # green/dark body
    2: 2, 3: 2, 4: 2, 6: 2, 7: 2, 10: 2, 14: 2, # mid skin/tan/accent
    0: 3, 1: 3,                                  # bright white/eye/horn
}
SPR_MAP_BRIGHT = {
    15: 0,
    8: 1, 9: 1, 5: 1, 11: 1, 13: 1,             # red/dark outline
    2: 2, 3: 2, 4: 2, 6: 2, 10: 2, 12: 2, 14: 2,# orange/gold mid
    0: 3, 1: 3, 7: 3,                            # bright yellow/white
}

def map_2bpp(grid, cmap):
    return [[cmap.get(v, 1) for v in row] for row in grid]

def encode_8x8(grid, ox, oy):
    """Encode 8x8 2bpp tile at (ox,oy) into 16 NES CHR bytes."""
    p0 = []
    p1 = []
    for y in range(8):
        b0 = 0
        b1 = 0
        for x in range(8):
            v = grid[oy + y][ox + x] & 3
            b0 = (b0 << 1) | (v & 1)
            b1 = (b1 << 1) | ((v >> 1) & 1)
        p0.append(b0)
        p1.append(b1)
    return bytes(p0 + p1)

def encode_8x16_pair(grid16x16):
    """Encode 16x16 into 2 adjacent 8x16 sprites (left 8x16 = 32 B, right 8x16 = 32 B -> 64 B)."""
    return (
        encode_8x8(grid16x16, 0, 0) + encode_8x8(grid16x16, 0, 8) +
        encode_8x8(grid16x16, 8, 0) + encode_8x8(grid16x16, 8, 8)
    )

def encode_8x16_single(grid16x16, cx=4):
    """Encode 8x16 slice centered horizontally at cx into 32 B."""
    sub = [[grid16x16[y][cx+x] for x in range(8)] for y in range(16)]
    return encode_8x8(sub, 0, 0) + encode_8x8(sub, 0, 8)

# ---------------------------------------------------------------------------
# Build Sprite CHR Table ($1000..$17FF = 2,048 bytes = 64 x 8x16 sprites!)
# OAM tile index for 8x16 sprite #k (0..63) at $1000 is: (k*2 + 1) = $01, $03, $05, ...
# ---------------------------------------------------------------------------
spr_chr = bytearray(2048)

def put_pair(tile_idx_odd, grid16x16):
    """Store 64 bytes (2 x 8x16) starting at odd OAM tile index (e.g. 1 -> offset 0)."""
    off = ((tile_idx_odd - 1) // 2) * 32
    spr_chr[off : off + 64] = encode_8x16_pair(grid16x16)

def put_single(tile_idx_odd, grid16x16, cx=4):
    off = ((tile_idx_odd - 1) // 2) * 32
    spr_chr[off : off + 32] = encode_8x16_single(grid16x16, cx)

def put_pair_32(top_tile_odd, bot_tile_odd, grid16x32):
    put_pair(top_tile_odd, grid16x32[0:16])
    put_pair(bot_tile_odd, grid16x32[16:32])

# 1. Arthur Armored (16x32: top + bottom)
ar_stand = map_2bpp(crop_center_16x32(compose_32x32_spr(0x010, 0x011, 0x018, 0x019)), SPR_MAP_ARMOR)
ar_walk1 = map_2bpp(crop_center_16x32(compose_32x32_spr(0x000, 0x001, 0x008, 0x009)), SPR_MAP_ARMOR)
ar_walk2 = map_2bpp(crop_center_16x32(compose_32x32_spr(0x002, 0x003, 0x00a, 0x00b)), SPR_MAP_ARMOR)
ar_jump  = map_2bpp(crop_center_16x32(compose_32x32_spr(0x012, 0x013, 0x01a, 0x01b)), SPR_MAP_ARMOR)
ar_crouch= map_2bpp(crop_center_16x16(compose_32x32_spr(0x016, 0x017, 0x01e, 0x01f), True), SPR_MAP_ARMOR)
ar_ladder= map_2bpp(crop_center_16x32(compose_32x32_spr(0x022, 0x023, 0x02a, 0x02b)), SPR_MAP_ARMOR)

# Legacy/default slots $01/$03 = Arthur stand bottom, $11/$13 = Arthur stand top!
put_pair_32(0x11, 0x01, ar_stand)
put_pair_32(0x15, 0x19, ar_walk1)
put_pair_32(0x1d, 0x21, ar_walk2)
put_pair_32(0x25, 0x29, ar_jump)
put_pair(0x2d, ar_crouch)
put_pair_32(0x31, 0x35, ar_ladder)

# 2. Arthur Naked (16x32)
nk_stand = map_2bpp(crop_center_16x32(compose_32x32_spr(0x110, 0x111, 0x118, 0x119)), SPR_MAP_NAKED)
nk_walk1 = map_2bpp(crop_center_16x32(compose_32x32_spr(0x100, 0x101, 0x108, 0x109)), SPR_MAP_NAKED)
nk_jump  = map_2bpp(crop_center_16x32(compose_32x32_spr(0x112, 0x113, 0x11a, 0x11b)), SPR_MAP_NAKED)
nk_crouch= map_2bpp(crop_center_16x16(compose_32x32_spr(0x116, 0x117, 0x11e, 0x11f), True), SPR_MAP_NAKED)

put_pair_32(0x39, 0x3d, nk_stand)
put_pair_32(0x41, 0x45, nk_walk1)
put_pair_32(0x49, 0x4d, nk_jump)
put_pair(0x51, nk_crouch)

# 3. Zombie (16x32, facing left) -> bot=$05/$07, top=$55/$57; frame2 bot=$59/$5b, top=$5d/$5f
zb1 = map_2bpp(crop_center_16x32(compose_32x32_spr(0x0b0, 0x0b1, 0x0b8, 0x0b9)), SPR_MAP_ZOMBIE)
zb2 = map_2bpp(crop_center_16x32(compose_32x32_spr(0x0b2, 0x0b3, 0x0ba, 0x0bb)), SPR_MAP_ZOMBIE)
put_pair_32(0x55, 0x05, zb1)
put_pair_32(0x5d, 0x59, zb2)

# 4. Weapons & Projectiles (single 8x16 or 16x16):
#    $09 = Lance (horizontal spear from $054)
#    $0f = Enemy fireball/bolt ($07c)
#    $61 = Dagger ($05f)
#    $63 = Torch ($040)
#    $65 = Flame ($05c)
#    $67 = Cross/Shield ($05e)
put_single(0x09, map_2bpp(decode_sprite_16x16(0x054), SPR_MAP_ARMOR), 4)
put_single(0x0f, map_2bpp(decode_sprite_16x16(0x07c), SPR_MAP_BRIGHT), 4)
put_single(0x61, map_2bpp(decode_sprite_16x16(0x05f), SPR_MAP_ARMOR), 4)
put_single(0x63, map_2bpp(decode_sprite_16x16(0x040), SPR_MAP_BRIGHT), 4)
put_single(0x65, map_2bpp(decode_sprite_16x16(0x05c), SPR_MAP_BRIGHT), 4)
put_single(0x67, map_2bpp(decode_sprite_16x16(0x05e), SPR_MAP_BRIGHT), 4)

# 5. Moving platform ($0b/$0d), Tombstone ($69/$6b), Plant Turret ($6d/$6f)
plat_g = map_2bpp(crop_center_16x16(compose_32x32_spr(0x160, 0x161, None, None), False), SPR_MAP_MONSTER)
tomb_g = map_2bpp(crop_center_16x16(compose_32x32_spr(0x172, 0x173, 0x17a, 0x17b), False), SPR_MAP_ARMOR)
plant_g= map_2bpp(crop_center_16x16(compose_32x32_spr(0x152, 0x153, None, None), False), SPR_MAP_MONSTER)
put_pair(0x0b, plat_g)
put_pair(0x69, tomb_g)
put_pair(0x6d, plant_g)

# 6. Items: $71 = Key ($0d7), $73 = Armor ($13c), $75 = Pot/Weapon ($0d2)
put_single(0x71, map_2bpp(decode_sprite_16x16(0x0d7), SPR_MAP_BRIGHT), 4)
put_single(0x73, map_2bpp(decode_sprite_16x16(0x13c), SPR_MAP_ARMOR), 4)
put_single(0x75, map_2bpp(decode_sprite_16x16(0x0d2), SPR_MAP_BRIGHT), 4)

# 7. Unicorn Cyclops Boss (16x32): compose full 32x48 body ($200,$201,$202 / $210,$211,$212 / $220,$221,$222) and downsample cleanly to 16x32!
def build_cyclops_16x32():
    # Compose 32x32 from head/shoulders ($201,$202) and legs/armor ($211,$212) + center ($200,$210)
    # In strip_boss.png: $200=left arm, $201=head+horn+torso, $202=right arm; $210=left leg, $211=legs/crotch, $212=right leg
    full = [[15]*48 for _ in range(32)]
    for (ox, oy, c) in [(0,0,0x200),(16,0,0x201),(32,0,0x202),(0,16,0x210),(16,16,0x211),(32,16,0x212)]:
        p = decode_sprite_16x16(c)
        for y in range(16):
            for x in range(16):
                full[oy+y][ox+x] = p[y][x]
    # Sample center 32 columns (x=8..39) -> 16 columns by 2x1 nearest-non-transparent
    out = [[15]*16 for _ in range(32)]
    for y in range(32):
        for x in range(16):
            a = full[y][8 + x*2]
            b = full[y][8 + x*2 + 1]
            out[y][x] = a if a != 15 else b
    return out

boss_g = map_2bpp(build_cyclops_16x32(), SPR_MAP_MONSTER)
put_pair_32(0x77, 0x7b, boss_g)

# ---------------------------------------------------------------------------
# Build Stage 1 BG Tile CHR Set (authentic Makaimura Stage 1 tiles!)
# 11 authentic 16x16 metatiles -> 44 x 8x8 tiles (704 bytes) uploaded to CHR $0400+!
# ---------------------------------------------------------------------------
# Map 3bpp arcade BG tile color indices (0..7) -> NES 2bpp (0..3)
# In Stage 1:
#   $006 (grass+dirt): 0=black, 1-2=dark brown, 3-4=tan dirt, 5-7=bright green grass
#   $0b0 (soil):       0=black, 1-2=dark brown, 3-4=tan dirt, 5-7=highlight
#   $341/$347/$344 (fence/cross/tomb): 0=black, 1-3=dark gray, 4-5=mid gray, 6-7=bright/grass
#   $34b/$34c (dead tree): 0=black, 1-3=dark brown, 4-5=mid brown, 6-7=green moss
#   $0f0/$0f4/$0f6/$0fb (mountain/castle): 0=black, 1-3=dark gray, 4-5=mid gray, 6-7=light gray/blue
BG_MAP_GROUND = {0: 0, 1: 1, 2: 1, 3: 2, 4: 3, 5: 3, 6: 3, 7: 3}
BG_MAP_ROCK   = {0: 0, 1: 1, 2: 1, 3: 2, 4: 2, 5: 1, 6: 2, 7: 3}
BG_MAP_DECO   = {0: 0, 1: 1, 2: 1, 3: 2, 4: 2, 5: 2, 6: 3, 7: 3}

BG_TILES = [
    ("grass_ground", 0x006, BG_MAP_GROUND), # mt_ledge (grassy earth)
    ("dirt_soil",    0x0b0, BG_MAP_ROCK),   # mt_rock (underground soil)
    ("fence",        0x341, BG_MAP_DECO),   # mt_gravel / graveyard fence
    ("cross",        0x347, BG_MAP_DECO),   # mt_cross (graveyard cross + fence)
    ("tomb_bg",      0x344, BG_MAP_DECO),   # mt_tomb_bg (background tombstone)
    ("tree_lo",      0x34b, BG_MAP_GROUND), # mt_tree_lo (dead tree trunk)
    ("tree_hi",      0x34c, BG_MAP_GROUND), # mt_tree_hi (dead tree branches)
    ("mtn_l",        0x0f4, BG_MAP_DECO),   # mt_mtn_l (mountain left slope)
    ("mtn_c",        0x0f0, BG_MAP_DECO),   # mt_mtn_c (mountain peak)
    ("mtn_r",        0x0f6, BG_MAP_DECO),   # mt_mtn_r (mountain right slope)
    ("castle",       0x0fb, BG_MAP_DECO),   # mt_castle (demon castle spire)
]

bg_chr = bytearray()
bg_grids = []
for name, code, cmap in BG_TILES:
    g = map_2bpp(decode_bg_16x16(code), cmap)
    bg_grids.append((name, g))
    # Emit 4 x 8x8 tiles in TL, TR, BL, BR order (matching metatile_tiles layout!)
    bg_chr += encode_8x8(g, 0, 0)
    bg_chr += encode_8x8(g, 8, 0)
    bg_chr += encode_8x8(g, 0, 8)
    bg_chr += encode_8x8(g, 8, 8)

# Render NES-colored preview PNGs to verify visual quality!
NES_SPR_PAL = [(0,0,0), (92,92,92), (184,184,184), (255,255,255)]
NES_BG_PAL  = [(0,0,0), (120,60,0), (200,120,40), (0,168,0)]

# Preview sprites (Arthur stand/walk/jump/crouch, Naked stand, Zombie, Tombstone, Plant, Cyclops)
prev_s = Image.new("RGB", (10 * 18, 36), (16, 16, 40))
spr_previews = [
    (ar_stand, [(0,0,0),(90,90,110),(180,180,200),(255,255,255)]),
    (ar_walk1, [(0,0,0),(90,90,110),(180,180,200),(255,255,255)]),
    (ar_jump,  [(0,0,0),(90,90,110),(180,180,200),(255,255,255)]),
    (nk_stand, [(0,0,0),(180,20,20),(240,160,110),(255,240,220)]),
    (zb1,      [(0,0,0),(30,80,220),(200,30,30),(245,245,245)]),
    (zb2,      [(0,0,0),(30,80,220),(200,30,30),(245,245,245)]),
    (boss_g,   [(0,0,0),(20,140,30),(200,120,50),(255,255,255)]),
]
for i, (g, pal) in enumerate(spr_previews):
    for y in range(len(g)):
        for x in range(16):
            v = g[y][x]
            if v:
                prev_s.putpixel((i * 18 + 1 + x, 2 + y), pal[v])
for i, (g, pal) in enumerate([(tomb_g, [(0,0,0),(90,90,110),(180,180,200),(255,255,255)]),
                              (plant_g, [(0,0,0),(20,140,30),(200,120,50),(255,255,255)]),
                              (plat_g, [(0,0,0),(120,60,0),(200,120,40),(255,255,255)])]):
    for y in range(16):
        for x in range(16):
            v = g[y][x]
            if v:
                prev_s.putpixel(((7 + i) * 18 + 1 + x, 18 + y), pal[v])
prev_s.resize((10 * 18 * 3, 36 * 3), Image.NEAREST).save("build/preview_fc_sprites.png")

prev_b = Image.new("RGB", (len(bg_grids) * 18, 20), (0, 0, 0))
for i, (name, g) in enumerate(bg_grids):
    pal = NES_BG_PAL if name in ("grass_ground", "dirt_soil", "tree_lo", "tree_hi") else [(0,0,0),(80,80,80),(150,150,150),(240,240,240)]
    for y in range(16):
        for x in range(16):
            prev_b.putpixel((i * 18 + 1 + x, 2 + y), pal[g[y][x]])
prev_b.resize((len(bg_grids) * 18 * 3, 20 * 3), Image.NEAREST).save("build/preview_fc_bg.png")

# Write src/arcade-gfx-data.inc into BANK10 ($8000-$9FFF)
def fmt(data):
    lines = []
    for i in range(0, len(data), 16):
        lines.append("    .byte " + ", ".join(f"${b:02x}" for b in data[i:i+16]))
    return "\n".join(lines)

out = []
out.append("; Auto-generated by tools/convert-arcade-gfx.py from reference/makaimurg.zip.")
out.append("; Authentic Makaimura (Revision G) sprites (2048 B) and Stage 1 BG tiles (704 B).")
out.append("")
out.append("arcade_gfx_bank = 10")
out.append("arcade_spr_chr_bytes = 2048")
out.append("arcade_bg_chr_bytes = " + str(len(bg_chr)))
out.append("arcade_bg_chr_vram = $0400   ; Tile $40..$6B in PPU $0000 pattern table")
out.append("")
out.append('.segment "BANK10"')
out.append("    .byte bank_tag_base + arcade_gfx_bank")
out.append("")
out.append("arcade_spr_chr_data:")
out.append(fmt(spr_chr))
out.append("")
out.append("arcade_bg_chr_data:")
out.append(fmt(bg_chr))
out.append("")

with open("src/arcade-gfx-data.inc", "w", newline="\n") as f:
    f.write("\n".join(out) + "\n")
print(f"Generated src/arcade-gfx-data.inc (SPR={len(spr_chr)} B, BG={len(bg_chr)} B, total={len(spr_chr)+len(bg_chr)} B)")
