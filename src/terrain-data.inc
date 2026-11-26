; Four CHR indices per 16x16 cell, in TL, TR, BL, BR order.
metatile_sky = 0
metatile_gravel = 4
metatile_ledge = 8
metatile_rock = 12
metatile_rock_light = 16
metatile_rock_dark = 20

; 2D metatile IDs stored in stage maps (column-major, 16 rows per column).
mt_sky = 0
mt_gravel = 1
mt_ledge = 2
mt_rock = 3
mt_oneway = 4
mt_ladder = 5
mt_hazard = 6
.if PRG_ROM_UNITS = 32
mt_cross = 7
mt_tomb_bg = 8
mt_tree_lo = 9
mt_tree_hi = 10
mt_mtn_l = 11
mt_mtn_c = 12
mt_mtn_r = 13
mt_castle = 14
; Revision G real map (tools/convert-revg-section.py).
mt_floor = 15       ; one-way floor, drawn as grass
mt_dirt = 16        ; no collision, drawn as dirt
mt_tomb = 17        ; tombstone art; its box is revg_tomb_* (ap_tomb_box)
; One-way floors whose surface is 1..14 px below the cell top (mt_floor_top + offset - 1),
; for arcade floor heights 13..0 (ap_probe_foot). Drawn as grass.
mt_floor_top = 18
mt_floor_last = mt_floor_top + 13
.endif

; Metatile collision attribute bits (bits 2-4 select terrain type).
tile_type_mask = $1c
tile_type_air = $00
tile_type_solid = $04
tile_type_oneway = $08
tile_type_ladder = $0c
tile_type_hazard = $10

map_col_stride = 16
default_stage_width = 48
default_stage_height = 15

; Stage header located immediately after the 1-byte bank tag at $8000.
; Every stage bank shares the same header layout in the $8000 window.
.macro stage_header width_cols, height_rows, cam_x_max_hi, cam_y_max_page, start_x, cp_x, cp_feet, cp_cam_hi, cp_cam_y, goal_x_val, goal_feet_val, map_label, pal_label
    .assert height_rows > 0 .and height_rows <= map_col_stride * 4, error, "Invalid stage height"
    .assert (cp_feet .mod 8) = 0, error, "Invalid checkpoint scroll"
    .byte width_cols
    .byte height_rows
    .byte cam_x_max_hi
    .byte cam_y_max_page
    .word start_x
    .word cp_x
    .byte cp_feet
    .byte cp_cam_hi
    .byte cp_cam_y
    .word goal_x_val
    .byte goal_feet_val
    .word map_label
    .word pal_label
.endmacro

; Emit one 16-row column of 2D metatiles from a solid floor height (1..14).
.macro cliff_col_2d height
    .assert height > 0 .and height < 15, error, "Invalid cliff height"
    .repeat map_col_stride, row
        .if row < height - 1
            .byte mt_sky
        .elseif row = height - 1
            .byte mt_gravel
        .elseif row = height
            .byte mt_ledge
        .else
            .byte mt_rock
        .endif
    .endrepeat
.endmacro

.macro cliff_pair_2d left_height, right_height
    cliff_col_2d left_height
    cliff_col_2d right_height
.endmacro

.macro pal_pair palette
    .assert palette >= 0 .and palette < 4, error, "Invalid background palette"
    .byte palette, palette
.endmacro

.segment "BANK0"
stage_header_base:
    stage_header default_stage_width, default_stage_height, 2, 2, 176, 704, 176, 2, 16, 760, 176, stage0_map, stage0_palettes
stage_width_cols     = stage_header_base + 0
stage_height_rows    = stage_header_base + 1
stage_cam_x_max_hi   = stage_header_base + 2
stage_cam_y_max_page = stage_header_base + 3
stage_start_x_lo     = stage_header_base + 4
stage_start_x_hi     = stage_header_base + 5
stage_cp_x_lo        = stage_header_base + 6
stage_cp_x_hi        = stage_header_base + 7
stage_cp_feet        = stage_header_base + 8
stage_cp_cam_hi      = stage_header_base + 9
stage_cp_cam_y       = stage_header_base + 10
stage_goal_x_lo      = stage_header_base + 11
stage_goal_x_hi      = stage_header_base + 12
stage_goal_feet      = stage_header_base + 13
stage_map_ptr        = stage_header_base + 14
stage_col_pal_ptr    = stage_header_base + 16

stage0_palettes:
    pal_pair 0
    pal_pair 0
    pal_pair 1
    pal_pair 1
    pal_pair 3
    pal_pair 3
    pal_pair 2
    pal_pair 2
    pal_pair 3
    pal_pair 3
    pal_pair 2
    pal_pair 1
    pal_pair 1
    pal_pair 0
    pal_pair 3
    pal_pair 3
    pal_pair 1
    pal_pair 1
    pal_pair 2
    pal_pair 2
    pal_pair 3
    pal_pair 0
    pal_pair 1
    pal_pair 1
.assert * - stage0_palettes = default_stage_width, error, "Unexpected stage0 palette width"
.res 255 - (* - stage_header_base), $ff

stage0_map:
    cliff_pair_2d 12, 12
    cliff_pair_2d 11, 11
    cliff_pair_2d 10, 10
    cliff_pair_2d 11, 12
    cliff_pair_2d 12, 13
    cliff_pair_2d 11, 11
    cliff_pair_2d 11, 11
    cliff_pair_2d 11, 11
    cliff_pair_2d 11, 11
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 11
    cliff_pair_2d 11, 10
    cliff_pair_2d 9, 8
    cliff_pair_2d 8, 9
    cliff_pair_2d 10, 11
    cliff_pair_2d 12, 13
    cliff_pair_2d 12, 11
    cliff_pair_2d 10, 9
    cliff_pair_2d 8, 9
    cliff_pair_2d 9, 10
    cliff_pair_2d 11, 12
    cliff_pair_2d 11, 10
    cliff_pair_2d 10, 11
.assert * - stage0_map = default_stage_width * map_col_stride, error, "Unexpected stage0 map size"

.segment "BANK1"
stage1_header_base:
    stage_header default_stage_width, default_stage_height, 2, 2, 160, 696, 192, 2, 32, 760, 176, stage1_map - $2000, stage1_palettes - $2000
stage1_palettes:
    pal_pair 2
    pal_pair 2
    pal_pair 3
    pal_pair 3
    pal_pair 0
    pal_pair 0
    pal_pair 1
    pal_pair 1
    pal_pair 0
    pal_pair 0
    pal_pair 1
    pal_pair 2
    pal_pair 2
    pal_pair 1
    pal_pair 0
    pal_pair 0
    pal_pair 2
    pal_pair 2
    pal_pair 3
    pal_pair 3
    pal_pair 0
    pal_pair 1
    pal_pair 2
    pal_pair 2
.assert * - stage1_palettes = default_stage_width, error, "Unexpected stage1 palette width"
.res 255 - (* - stage1_header_base), $ff

stage1_map:
    cliff_pair_2d 13, 13
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 11, 11
    cliff_pair_2d 11, 11
    cliff_pair_2d 11, 11
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 12
    cliff_pair_2d 12, 11
    cliff_pair_2d 11, 10
    cliff_pair_2d 9, 8
    cliff_pair_2d 8, 9
    cliff_pair_2d 10, 11
    cliff_pair_2d 12, 13
    cliff_pair_2d 12, 11
    cliff_pair_2d 10, 9
    cliff_pair_2d 8, 9
    cliff_pair_2d 9, 10
    cliff_pair_2d 11, 12
    cliff_pair_2d 11, 10
    cliff_pair_2d 10, 11
.assert * - stage1_map = default_stage_width * map_col_stride, error, "Unexpected stage1 map size"
.assert stage1_header_base = stage_header_base + $2000, lderror, "Stage header offsets differ"

.segment "CODE"
; A = metatile column (0..255). Sets map_ptr to the column start in $8000.
; Requires stage_map_ptr to be 256-byte aligned. Preserves X and Y.
set_map_col_ptr:
    cmp map_cached_col
    beq @cached
    sta map_cached_col
    sta map_tmp
    asl a
    asl a
    asl a
    asl a
    sta map_ptr
    lda map_tmp
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    adc stage_map_ptr+1
    sta map_ptr+1
@cached:
    rts

; A = metatile column (0..255), Y = metatile row (0..59).
; Returns metatile ID in A (mt_rock below the map). Preserves X and Y.
read_map_cell:
    cpy stage_height_rows
    bcs @below_map
    pha
    lda stage_height_rows
    cmp #32
    bcs @read_nibble_map
    pla
    cpy #map_col_stride
    bcs @read_lower_page
    jsr set_map_col_ptr
    lda (map_ptr),y
    rts
@read_nibble_map:
    pla
    sta map_tmp
    asl a
    asl a
    asl a
    asl a
    asl a
    sta map_ptr
    lda map_tmp
    lsr a
    lsr a
    lsr a
    clc
    adc stage_map_ptr+1
    sta map_ptr+1
    lda #$ff
    sta map_cached_col
    txa
    pha
    tya
    pha
    lsr a
    tay
    lda (map_ptr),y
    sta map_tmp
    pla
    tay
    lsr a
    lda map_tmp
    bcc @nib_lo
    lsr a
    lsr a
    lsr a
    lsr a
@nib_lo:
    and #$0f
    tax
    lda nibble_mt,x
    sta map_tmp
    pla
    tax
    lda map_tmp
    rts
@read_lower_page:
    clc
    adc stage_width_cols
    bcc @lower_no_carry
    jsr set_map_col_ptr
    lda map_ptr+1
    clc
    adc #$10
    sta map_ptr+1
    lda #$ff
    sta map_cached_col
    bne @lower_read
@lower_no_carry:
    jsr set_map_col_ptr
@lower_read:
    tya
    sec
    sbc #map_col_stride
    tay
    lda (map_ptr),y
    pha
    tya
    clc
    adc #map_col_stride
    tay
    pla
    rts
@below_map:
    lda #mt_rock
    rts

.segment "RODATA"
nibble_mt: .byte mt_sky, mt_ladder, mt_floor, mt_dirt, mt_tomb
metatile_attr:
    .byte tile_type_air     ; mt_sky
    .byte tile_type_air     ; mt_gravel
    .byte tile_type_solid   ; mt_ledge
    .byte tile_type_solid   ; mt_rock
    .byte tile_type_oneway  ; mt_oneway
    .byte tile_type_ladder  ; mt_ladder
    .byte tile_type_hazard  ; mt_hazard
.if PRG_ROM_UNITS = 32
    .byte tile_type_air     ; mt_cross
    .byte tile_type_air     ; mt_tomb_bg
    .byte tile_type_air     ; mt_tree_lo
    .byte tile_type_air     ; mt_tree_hi
    .byte tile_type_air     ; mt_mtn_l
    .byte tile_type_air     ; mt_mtn_c
    .byte tile_type_air     ; mt_mtn_r
    .byte tile_type_air     ; mt_castle
    .byte tile_type_oneway  ; mt_floor
    .byte tile_type_air     ; mt_dirt
    .byte tile_type_air     ; mt_tomb
    .repeat mt_floor_last - mt_floor_top + 1
    .byte tile_type_oneway  ; mt_floor_top..mt_floor_last
    .endrepeat
.endif

cliff_gravel = (terrain_tiles - glyphs) / 16
cliff_ledge = cliff_gravel + 1
cliff_light = cliff_gravel + 2
cliff_dark = cliff_gravel + 3
terrain_metatiles:
    .byte 0, 0, 0, 0
    .byte 0, 0, cliff_gravel, cliff_gravel
    .byte cliff_ledge, cliff_ledge, cliff_light, cliff_light
    .byte cliff_light, cliff_light, cliff_light, cliff_light
    .byte cliff_light, cliff_light, cliff_light, cliff_light
    .byte cliff_dark, cliff_dark, cliff_dark, cliff_dark

terrain_colors:
    .byte $0f, $2a, $09, $19,  $0f, $38, $17, $27
    .byte $0f, $31, $11, $21,  $0f, $34, $14, $24

; Palette IDs expanded to the attribute quadrants used by each renderer.
terrain_palette_full: .byte $00, $55, $aa, $ff
terrain_palette_half: .byte $00, $05, $0a, $0f
