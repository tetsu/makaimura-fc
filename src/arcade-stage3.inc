; Bank 14: Arcade Stage 3 (Horizontal 48x15 Underground Cavern & Waterway).
; Mapped into the $8000 window (bank 14).

arcade_stage3_bank = 14

.macro cavern_upper_col
    .repeat map_col_stride, row
        .if row = 9
            .byte mt_oneway
        .elseif row < 10
            .byte mt_sky
        .elseif row = 10
            .byte mt_gravel
        .elseif row = 11
            .byte mt_ledge
        .else
            .byte mt_rock
        .endif
    .endrepeat
.endmacro

.segment "BANK14"
    .byte bank_tag_base + arcade_stage3_bank
arcade_stage3_header:
    stage_header default_stage_width, default_stage_height, 2, 2, 48, 480, 176, 1, 16, 760, 176, arcade_stage3_map, arcade_stage3_palettes
.assert arcade_stage3_header = stage_header_base, lderror, "Stage 3 header offset mismatch"

arcade_stage3_palettes:
    .repeat default_stage_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage3_palettes = default_stage_width, error, "Unexpected stage 3 palette width"
.res 127 - (* - arcade_stage3_header), $ff

arcade_stage3_desc:
.assert arcade_stage3_desc = sd_base, lderror, "Stage 3 descriptor offset mismatch"
    .byte sd_flag_has_boss              ; sd_flags (horizontal + boss)
    .word 176                           ; sd_start_y
    .word 176                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 176                           ; sd_goal_y
    .byte snd_song_stage3               ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage3_bg_pal          ; sd_bg_pal_ptr (cavern navy/teal/white)
    .word arcade_stage3_spr_pal         ; sd_spr_pal_ptr (aqua/ice guardian)
    .word arcade_stage3_bg_chr          ; sd_bg_chr_ptr (stalactite/cavern tiles)
    .word arcade_stage3_spr_chr         ; sd_spr_chr_ptr (deep guardian tiles)
    .word 200                           ; sd_enemy1_x
    .word 176                           ; sd_enemy1_y
    .word 384                           ; sd_pot_x
    .word 176                           ; sd_pot_y
    .byte 2                             ; sd_pot_drop (item_dagger)
    .byte 1                             ; sd_item_count
    .word arcade_stage3_items           ; sd_items_ptr
    .byte 1                             ; sd_plat_count
    .word arcade_stage3_plats           ; sd_plats_ptr
    .byte 2                             ; sd_sched_page (camera_high == 2)
    .word 712                           ; sd_turret_x
    .word 176                           ; sd_turret_y
    .word 752                           ; sd_zombie_x
    .word 176                           ; sd_zombie_y
    .word 720                           ; sd_boss_trig_x
    .word 0                             ; sd_boss_trig_y
    .word 520                           ; sd_boss_arena_l
    .word 736                           ; sd_boss_start_x
    .word 684                           ; sd_boss_min_x
    .word 744                           ; sd_boss_max_x
    .word 176                           ; sd_boss_y
    .byte 5                             ; sd_boss_hp
    .byte 3                             ; sd_boss_shield
    .byte 42                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage3_header), $ff

arcade_stage3_map:
.assert arcade_stage3_map = $8100, lderror, "Stage 3 map must be 256-byte aligned"
    ; Columns 0-7 (X=0..127): cavern entrance
    .repeat 8
        arcade_flat_col
    .endrepeat
    ; Columns 8-9 (X=128..159): water hazard pit (32 px)
    arcade_pit_col
    arcade_pit_col
    ; Columns 10-13 (X=160..223): stepping ledge with fixed enemy 1
    .repeat 4
        arcade_flat_col
    .endrepeat
    ; Columns 14-15 (X=224..255): upper cavern shelf
    cavern_upper_col
    cavern_upper_col
    ; Column 16 (X=256..271): ladder up to upper shelf
    arcade_ladder_col 9, 10
    ; Columns 17-21 (X=272..351): 2-tier cavern corridor
    .repeat 5
        cavern_upper_col
    .endrepeat
    ; Columns 22-25 (X=352..415): lower flat with pot enemy (Dagger)
    .repeat 4
        arcade_flat_col
    .endrepeat
    ; Columns 26-27 (X=416..447): second water hazard pit (32 px)
    arcade_pit_col
    arcade_pit_col
    ; Columns 28-30 (X=448..495): checkpoint bank (X=480)
    .repeat 3
        arcade_flat_col
    .endrepeat
    ; Columns 31-35 (X=496..575): wide waterway bridged by horizontal moving platform
    .repeat 5
        arcade_pit_col
    .endrepeat
    ; Columns 36-47 (X=576..767): cavern boss chamber
    .repeat 12
        arcade_flat_col
    .endrepeat
.assert * - arcade_stage3_map = default_stage_width * map_col_stride, error, "Unexpected stage 3 map size"

arcade_stage3_items:
    ; Slot 0: armor recovery on upper cavern shelf (X=304, Y=144)
    .byte 1                             ; item_armor
    .word 304, 144

arcade_stage3_plats:
    ; Slot 0: horizontal ferry across wide waterway (X=504..552, Y=160)
    .byte 2                             ; plat_moving_h
    .word 504, 160
    .byte 48

; Cavern palettes (deep navy, aqua teal, frost white, slate)
arcade_stage3_bg_pal:
    .byte $0f, $21, $11, $31,  $0f, $2c, $1c, $3c
    .byte $0f, $12, $02, $22,  $0f, $10, $00, $20

arcade_stage3_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $1c, $2c, $3c
    .byte $0f, $16, $27, $30,  $0f, $11, $21, $31

; Stage 3 custom BG cavern tiles (64 bytes: stalagmite gravel, wet ledge, limestone, deep rock)
arcade_stage3_bg_chr:
    .byte $00, $00, $08, $1c, $3e, $7f, $ff, $00
    .byte $00, $00, $00, $08, $1c, $3e, $7f, $00
    .byte $ff, $ff, $db, $99, $18, $00, $00, $00
    .byte $ff, $e7, $c3, $81, $00, $00, $00, $00
    .byte $e7, $c3, $81, $c3, $e7, $ff, $7e, $3c
    .byte $18, $3c, $7e, $3c, $18, $00, $81, $c3
    .byte $ff, $7e, $3c, $7e, $ff, $db, $c3, $db
    .byte $00, $81, $c3, $81, $00, $24, $3c, $24

; Stage 3 custom OBJ tiles for the boss's upper half (64 bytes).
arcade_stage3_spr_chr:
    .byte $0e, $1f, $3f, $7b, $7f, $3f, $1f, $0f
    .byte $06, $0f, $1f, $3c, $3f, $1f, $0f, $07
    .byte $1f, $3f, $7f, $7f, $3e, $1c, $3e, $7f
    .byte $0f, $1f, $3f, $3f, $1e, $0c, $1e, $3f
    .byte $70, $f8, $fc, $de, $fe, $fc, $f8, $f0
    .byte $60, $f0, $f8, $3c, $fc, $f8, $f0, $e0
    .byte $f8, $fc, $fe, $fe, $7c, $38, $7c, $fe
    .byte $f0, $f8, $fc, $fc, $78, $30, $78, $fc
