; Bank 13: Arcade Stage 2 (Vertical-climbing 16x30 Cursed Watchtower prototype).
; Mapped into the $8000 window (bank 13).
; Map layout uses two 16-column pages:
;   - Columns 0..15  = vertical page 0 (rows 0..15, world Y = 0..255)
;   - Columns 16..31 = vertical page 1 (rows 16..29, world Y = 256..479)

arcade_stage2_bank = 13
stage2_width = 16
stage2_height = 30

.macro tower_wall_col
    .repeat map_col_stride
        .byte mt_rock
    .endrepeat
.endmacro

; Upper vertical page (rows 0..15, world Y = 0..255)
; Floor at row 9 (Y=144, boss arena) and row 13 (Y=208, checkpoint tier).
; Ladder at col 3 (rows 9..12) and col 12 (rows 13..15, continuing from page 1).
.macro tower_upper_col col_idx
    .repeat map_col_stride, row
        .if col_idx = 0 .or col_idx = 15
            .byte mt_rock
        .elseif col_idx = 3 .and row >= 9 .and row <= 12
            .byte mt_ladder
        .elseif col_idx = 12 .and row >= 13 .and row <= 15
            .byte mt_ladder
        .elseif row = 9
            .byte mt_oneway
        .elseif row = 13
            .byte mt_oneway
        .else
            .byte mt_sky
        .endif
    .endrepeat
.endmacro

; Lower vertical page (rows 16..29 mapped to local 0..13, world Y = 256..479)
; Local rows:
;   local 0..1 (global 16..17): ladder at col 12 (connects to upper page row 13..15)
;   local 2    (global 18, Y=288): upper-mid floor (oneway), ladder at col 3 (local 2..6)
;   local 7    (global 23, Y=368): lower-mid floor (oneway), ladder at col 12 (local 7..11)
;   local 12   (global 28, Y=448): ground floor (ledge)
;   local 13..15 (global 29+): solid rock foundation
.macro tower_lower_col col_idx
    .repeat map_col_stride, lrow
        .if col_idx = 0 .or col_idx = 15
            .byte mt_rock
        .elseif lrow >= 13
            .byte mt_rock
        .elseif lrow = 12
            .byte mt_ledge
        .elseif col_idx = 12 .and (lrow <= 1 .or (lrow >= 7 .and lrow <= 11))
            .byte mt_ladder
        .elseif col_idx = 3 .and lrow >= 2 .and lrow <= 6
            .byte mt_ladder
        .elseif lrow = 2 .or lrow = 7
            .byte mt_oneway
        .else
            .byte mt_sky
        .endif
    .endrepeat
.endmacro

.segment "BANK13"
    .byte bank_tag_base + arcade_stage2_bank
.ifdef COURSE_SEQUENCE
.scope sequence_stage2
    course_is_scroll = 1
    .include "original-course.inc"
.endscope
.else
arcade_stage2_header:
    stage_header stage2_width, stage2_height, 0, 1, 48, 200, 208, 0, 0, 208, 144, arcade_stage2_map, arcade_stage2_palettes
.assert arcade_stage2_header = stage_header_base, lderror, "Stage 2 header offset mismatch"

arcade_stage2_palettes:
    .repeat stage2_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage2_palettes = stage2_width, error, "Unexpected stage 2 palette width"
.res 127 - (* - arcade_stage2_header), $ff

arcade_stage2_desc:
.assert arcade_stage2_desc = sd_base, lderror, "Stage 2 descriptor offset mismatch"
    .byte sd_flag_vertical | sd_flag_has_boss | sd_flag_start_page1 ; sd_flags
    .word 448                           ; sd_start_y (bottom floor, page 1)
    .word 208                           ; sd_cp_y (upper page mid floor)
    .byte 0                             ; sd_cp_cam_y_page (page 0)
    .word 144                           ; sd_goal_y (top boss arena)
    .byte snd_song_stage                ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage2_bg_pal          ; sd_bg_pal_ptr (tower slate/crimson/gold)
    .word arcade_stage2_spr_pal         ; sd_spr_pal_ptr (warden violet/flame)
    .word arcade_stage2_bg_chr          ; sd_bg_chr_ptr (tower masonry tiles)
    .word arcade_stage2_spr_chr         ; sd_spr_chr_ptr (tower warden sprite tiles)
    .word 144                           ; sd_enemy1_x
    .word 448                           ; sd_enemy1_y (ground floor)
    .word 112                           ; sd_pot_x
    .word 368                           ; sd_pot_y (lower-mid floor)
    .byte 2                             ; sd_pot_drop (item_dagger)
    .byte 2                             ; sd_item_count
    .word arcade_stage2_items           ; sd_items_ptr
    .byte 1                             ; sd_plat_count
    .word arcade_stage2_plats           ; sd_plats_ptr
    .byte 0                             ; sd_sched_page (vertical page 0 turret)
    .word 216                           ; sd_turret_x
    .word 208                           ; sd_turret_y
    .word 120                           ; sd_zombie_x (patrols mid-left, clear of right ladder)
    .word 208                           ; sd_zombie_y
    .word 0                             ; sd_boss_trig_x (unused in vertical)
    .word 144                           ; sd_boss_trig_y (trigger when player_y <= 144)
    .word 24                            ; sd_boss_arena_l
    .word 208                           ; sd_boss_start_x
    .word 148                           ; sd_boss_min_x
    .word 220                           ; sd_boss_max_x
    .word 144                           ; sd_boss_y
    .byte 4                             ; sd_boss_hp
    .byte 2                             ; sd_boss_shield
    .byte 44                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage2_header), $ff

arcade_stage2_map:
.assert arcade_stage2_map = $8100, lderror, "Stage 2 map must be 256-byte aligned"
    ; Vertical page 0 (rows 0..15): columns 0..15
    .repeat stage2_width, col_idx
        tower_upper_col col_idx
    .endrepeat
    ; Vertical page 1 (rows 16..29): columns 16..31
    .repeat stage2_width, col_idx
        tower_lower_col col_idx
    .endrepeat
.assert * - arcade_stage2_map = stage2_width * 2 * map_col_stride, error, "Unexpected stage 2 map size"

; Stage 2 data tables
arcade_stage2_items:
    ; Slot 0: armor recovery on checkpoint floor (row 13, Y=208)
    .byte 1                             ; item_armor
    .word 88, 208
    ; Slot 1: optional torch on left alcove of upper-mid floor (row 18, Y=288)
    .byte 3                             ; item_torch
    .word 32, 288

arcade_stage2_plats:
    ; Vertical lift between ground and mid floor
    .byte 3                             ; plat_moving_v
    .word 128, 432
    .byte 48

; Distinct Watchtower palettes (slate blue, crimson brick, warm gold, obsidian)
arcade_stage2_bg_pal:
    .byte $0f, $2c, $00, $10,  $0f, $37, $06, $16
    .byte $0f, $38, $18, $28,  $0f, $24, $03, $13

arcade_stage2_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $04, $14, $34
    .byte $0f, $16, $27, $30,  $0f, $1c, $2c, $30

; Stage 2 custom BG masonry tiles (4 tiles = 64 bytes: gravel, ledge, rock_light, rock_dark)
arcade_stage2_bg_chr:
    ; Tile 0 (gravel -> tower battlement trim)
    .byte $00, $00, $00, $00, $e7, $e7, $ff, $00
    .byte $00, $00, $00, $00, $00, $e7, $ff, $00
    ; Tile 1 (ledge -> stone brick parapet)
    .byte $ff, $ff, $81, $ff, $ff, $18, $18, $ff
    .byte $ff, $00, $7e, $00, $00, $e7, $e7, $00
    ; Tile 2 (rock_light -> cut masonry courses)
    .byte $ff, $80, $80, $80, $ff, $08, $08, $08
    .byte $00, $7f, $7f, $7f, $00, $f7, $f7, $f7
    ; Tile 3 (rock_dark -> shadowed masonry)
    .byte $ff, $99, $81, $81, $ff, $18, $18, $18
    .byte $ff, $66, $7e, $7e, $ff, $e7, $e7, $e7

; Stage 2 custom OBJ tiles for the boss's upper half (64 bytes).
arcade_stage2_spr_chr:
    ; Tile 5 top-left (horned tower warden helm)
    .byte $20, $30, $3f, $7f, $5b, $7f, $3e, $1c
    .byte $00, $10, $1f, $3f, $24, $3f, $1e, $0c
    ; Tile 5 bottom-left (armored mantle)
    .byte $3f, $7f, $7f, $6f, $3f, $1e, $1c, $3e
    .byte $1f, $3f, $3f, $10, $1f, $0e, $0c, $1e
    ; Tile 7 top-right (horned helm right)
    .byte $04, $0c, $fc, $fe, $da, $fe, $7c, $38
    .byte $00, $08, $f8, $fc, $24, $fc, $78, $30
    ; Tile 7 bottom-right (armored mantle right)
    .byte $fc, $fe, $fe, $f6, $fc, $78, $38, $7c
    .byte $f8, $fc, $fc, $08, $f8, $70, $30, $78
.endif
