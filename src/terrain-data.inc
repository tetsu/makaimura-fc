; Four CHR indices per 16x16 cell, in TL, TR, BL, BR order.
metatile_sky = 0
metatile_gravel = 4
metatile_ledge = 8
metatile_rock = 12
metatile_rock_light = 16
metatile_rock_dark = 20

terrain_width = 48
cliff_height_mask = $0f

; A column descriptor packs solid floor height (bits 0–3, in 16 px
; cells) and a background palette ID (bits 4–5). Both columns of a
; horizontal attribute cell share the same palette in this course.
.macro cliff_pair left_height, right_height, palette
    .assert left_height > 0 .and left_height < 15, error, "Invalid cliff height"
    .assert right_height > 0 .and right_height < 15, error, "Invalid cliff height"
    .assert palette >= 0 .and palette < 4, error, "Invalid background palette"
    .byte (palette << 4) | left_height, (palette << 4) | right_height
.endmacro

.segment "RODATA"
terrain_columns:
    cliff_pair 12, 12, 0
    cliff_pair 11, 11, 0
    cliff_pair 10, 10, 1
    cliff_pair 11, 12, 1
    cliff_pair 12, 13, 3
    cliff_pair 11, 11, 3
    cliff_pair 11, 11, 2
    cliff_pair 11, 11, 2
    cliff_pair 11, 11, 3
    cliff_pair 12, 12, 3
    cliff_pair 12, 12, 2
    cliff_pair 12, 11, 1
    cliff_pair 11, 10, 1
    cliff_pair 9, 8, 0
    cliff_pair 8, 9, 3
    cliff_pair 10, 11, 3
    cliff_pair 12, 13, 1
    cliff_pair 12, 11, 1
    cliff_pair 10, 9, 2
    cliff_pair 8, 9, 2
    cliff_pair 9, 10, 3
    cliff_pair 11, 12, 0
    cliff_pair 11, 10, 1
    cliff_pair 10, 11, 1
.assert * - terrain_columns = terrain_width, error, "Unexpected terrain width"

cliff_gravel = (terrain_tiles - glyphs) / 16
cliff_ledge = cliff_gravel + 1
cliff_light = cliff_gravel + 2
cliff_dark = cliff_gravel + 3
terrain_metatiles:
    .byte 0, 0, 0, 0
    .byte 0, 0, cliff_gravel, cliff_gravel
    .byte cliff_ledge, cliff_ledge, cliff_light, cliff_light
    .byte cliff_ledge, cliff_ledge, cliff_light, cliff_light
    .byte cliff_light, cliff_light, cliff_light, cliff_light
    .byte cliff_dark, cliff_dark, cliff_dark, cliff_dark

terrain_colors:
    .byte $0f, $2a, $09, $19,  $0f, $38, $17, $27
    .byte $0f, $31, $11, $21,  $0f, $34, $14, $24

; Palette IDs expanded to the attribute quadrants used by each renderer.
terrain_palette_full: .byte $00, $55, $aa, $ff
terrain_palette_half: .byte $00, $05, $0a, $0f
