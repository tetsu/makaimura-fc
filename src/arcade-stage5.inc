; Bank 16: Arcade Stage 5 (Vertical-climbing 16x30 Outer Castle Multi-Tier Tower).
; Mapped into the $8000 window (bank 16).
; Uses shifted ladder columns (col 11 and col 4) distinct from Stage 2.

arcade_stage5_bank = 16
stage5_width = 16
stage5_height = 30

.macro castle5_upper_col col_idx
    .repeat map_col_stride, row
        .if col_idx = 0 .or col_idx = 15
            .byte mt_rock
        .elseif col_idx = 4 .and row >= 9 .and row <= 12
            .byte mt_ladder
        .elseif col_idx = 11 .and row >= 13 .and row <= 15
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

.macro castle5_lower_col col_idx
    .repeat map_col_stride, lrow
        .if col_idx = 0 .or col_idx = 15
            .byte mt_rock
        .elseif lrow >= 13
            .byte mt_rock
        .elseif lrow = 12
            .byte mt_ledge
        .elseif col_idx = 11 .and (lrow <= 1 .or (lrow >= 7 .and lrow <= 11))
            .byte mt_ladder
        .elseif col_idx = 4 .and lrow >= 2 .and lrow <= 6
            .byte mt_ladder
        .elseif lrow = 2 .or lrow = 7
            .byte mt_oneway
        .else
            .byte mt_sky
        .endif
    .endrepeat
.endmacro

.segment "BANK16"
    .byte bank_tag_base + arcade_stage5_bank
arcade_stage5_header:
    stage_header stage5_width, stage5_height, 0, 1, 48, 184, 208, 0, 0, 208, 144, arcade_stage5_map, arcade_stage5_palettes
.assert arcade_stage5_header = stage_header_base, lderror, "Stage 5 header offset mismatch"

arcade_stage5_palettes:
    .repeat stage5_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage5_palettes = stage5_width, error, "Unexpected stage 5 palette width"
.res 127 - (* - arcade_stage5_header), $ff

arcade_stage5_desc:
.assert arcade_stage5_desc = sd_base, lderror, "Stage 5 descriptor offset mismatch"
    .byte sd_flag_vertical | sd_flag_has_boss | sd_flag_start_page1 ; sd_flags
    .word 448                           ; sd_start_y
    .word 208                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 144                           ; sd_goal_y
    .byte snd_song_stage                ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage5_bg_pal          ; sd_bg_pal_ptr (castle purple/iron/gold)
    .word arcade_stage5_spr_pal         ; sd_spr_pal_ptr (dark knight silver/violet)
    .word arcade_stage5_bg_chr          ; sd_bg_chr_ptr (castle ironwork tiles)
    .word arcade_stage5_spr_chr         ; sd_spr_chr_ptr (twin-gate knight tiles)
    .word 128                           ; sd_enemy1_x
    .word 448                           ; sd_enemy1_y
    .word 112                           ; sd_pot_x
    .word 368                           ; sd_pot_y
    .byte 2                             ; sd_pot_drop (item_dagger)
    .byte 1                             ; sd_item_count
    .word arcade_stage5_items           ; sd_items_ptr
    .byte 1                             ; sd_plat_count
    .word arcade_stage5_plats           ; sd_plats_ptr
    .byte 0                             ; sd_sched_page (vertical page 0)
    .word 216                           ; sd_turret_x
    .word 208                           ; sd_turret_y
    .word 120                           ; sd_zombie_x (patrols mid-left, clear of right ladder)
    .word 208                           ; sd_zombie_y
    .word 0                             ; sd_boss_trig_x
    .word 144                           ; sd_boss_trig_y
    .word 24                            ; sd_boss_arena_l
    .word 208                           ; sd_boss_start_x
    .word 148                           ; sd_boss_min_x
    .word 220                           ; sd_boss_max_x
    .word 144                           ; sd_boss_y
    .byte 6                             ; sd_boss_hp
    .byte 3                             ; sd_boss_shield
    .byte 38                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage5_header), $ff

arcade_stage5_map:
.assert arcade_stage5_map = $8100, lderror, "Stage 5 map must be 256-byte aligned"
    .repeat stage5_width, col_idx
        castle5_upper_col col_idx
    .endrepeat
    .repeat stage5_width, col_idx
        castle5_lower_col col_idx
    .endrepeat
.assert * - arcade_stage5_map = stage5_width * 2 * map_col_stride, error, "Unexpected stage 5 map size"

arcade_stage5_items:
    ; Slot 0: armor recovery on checkpoint floor (Y=208)
    .byte 1                             ; item_armor
    .word 128, 208

arcade_stage5_plats:
    ; Vertical lift on lower castle shaft
    .byte 3                             ; plat_moving_v
    .word 136, 432
    .byte 48

; Outer Castle palettes (royal purple, iron grey, burnished gold, indigo)
arcade_stage5_bg_pal:
    .byte $0f, $14, $04, $24,  $0f, $10, $00, $30
    .byte $0f, $28, $18, $38,  $0f, $13, $03, $23

arcade_stage5_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $14, $24, $30
    .byte $0f, $16, $27, $30,  $0f, $10, $20, $30

; Stage 5 custom BG castle ironwork tiles (64 bytes)
arcade_stage5_bg_chr:
    .byte $00, $00, $00, $55, $aa, $ff, $ff, $00
    .byte $00, $00, $00, $00, $55, $aa, $ff, $00
    .byte $ff, $ff, $aa, $ff, $81, $81, $ff, $00
    .byte $ff, $55, $ff, $00, $7e, $7e, $00, $00
    .byte $ff, $91, $91, $ff, $ff, $89, $89, $ff
    .byte $00, $6e, $6e, $00, $00, $76, $76, $00
    .byte $ff, $aa, $d5, $aa, $ff, $81, $81, $ff
    .byte $00, $55, $2a, $55, $00, $7e, $7e, $00

; Stage 5 custom OBJ Twin-Gate Knight tiles (64 bytes: sprite tiles 5..8)
arcade_stage5_spr_chr:
    .byte $40, $60, $7c, $7f, $5f, $7f, $3e, $1c
    .byte $00, $20, $3c, $3f, $20, $3f, $1e, $0c
    .byte $3f, $7f, $7f, $5f, $3f, $3e, $1c, $3e
    .byte $1f, $3f, $3f, $20, $1f, $1e, $0c, $1e
    .byte $02, $06, $3e, $fe, $fa, $fe, $7c, $38
    .byte $00, $04, $3c, $fc, $04, $fc, $78, $30
    .byte $fc, $fe, $fe, $fa, $fc, $7c, $38, $7c
    .byte $f8, $fc, $fc, $04, $f8, $78, $30, $78
