; Bank 15: Arcade Stage 4 (Horizontal 48x15 Collapsing Bridge & Volcanic Caldera).
; Mapped into the $8000 window (bank 15).

arcade_stage4_bank = 15

.segment "BANK15"
    .byte bank_tag_base + arcade_stage4_bank
arcade_stage4_header:
    stage_header default_stage_width, default_stage_height, 2, 2, 48, 480, 176, 1, 16, 760, 176, arcade_stage4_map, arcade_stage4_palettes
.assert arcade_stage4_header = stage_header_base, lderror, "Stage 4 header offset mismatch"

arcade_stage4_palettes:
    .repeat default_stage_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage4_palettes = default_stage_width, error, "Unexpected stage 4 palette width"
.res 127 - (* - arcade_stage4_header), $ff

arcade_stage4_desc:
.assert arcade_stage4_desc = sd_base, lderror, "Stage 4 descriptor offset mismatch"
    .byte sd_flag_has_boss              ; sd_flags (horizontal + boss)
    .word 176                           ; sd_start_y
    .word 176                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 176                           ; sd_goal_y
    .byte snd_song_stage3               ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage4_bg_pal          ; sd_bg_pal_ptr (volcanic crimson/orange/ash)
    .word arcade_stage4_spr_pal         ; sd_spr_pal_ptr (magma/amber colossus)
    .word arcade_stage4_bg_chr          ; sd_bg_chr_ptr (basalt/ember tiles)
    .word arcade_stage4_spr_chr         ; sd_spr_chr_ptr (flame shell boss tiles)
    .word 272                           ; sd_enemy1_x
    .word 176                           ; sd_enemy1_y
    .word 320                           ; sd_pot_x
    .word 176                           ; sd_pot_y
    .byte 2                             ; sd_pot_drop (item_dagger)
    .byte 1                             ; sd_item_count
    .word arcade_stage4_items           ; sd_items_ptr
    .byte 3                             ; sd_plat_count
    .word arcade_stage4_plats           ; sd_plats_ptr
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
    .byte 6                             ; sd_boss_hp
    .byte 3                             ; sd_boss_shield
    .byte 40                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage4_header), $ff

arcade_stage4_map:
.assert arcade_stage4_map = $8100, lderror, "Stage 4 map must be 256-byte aligned"
    ; Columns 0-6 (X=0..111): caldera rim
    .repeat 7
        arcade_flat_col
    .endrepeat
    ; Columns 7-11 (X=112..191): first lava chasm bridged by moving platform 0
    .repeat 5
        arcade_pit_col
    .endrepeat
    ; Columns 12-21 (X=192..351): mid volcanic bridge (tombstone, enemy1, pot enemy)
    .repeat 10
        arcade_flat_col
    .endrepeat
    ; Columns 22-23 (X=352..383): narrow lava fissure (32 px, jumpable)
    arcade_pit_col
    arcade_pit_col
    ; Columns 24-30 (X=384..495): checkpoint causeway (armor + CP at X=480)
    .repeat 7
        arcade_flat_col
    .endrepeat
    ; Columns 31-35 (X=496..575): second lava chasm bridged by moving platform 2
    .repeat 5
        arcade_pit_col
    .endrepeat
    ; Columns 36-47 (X=576..767): volcanic boss caldera
    .repeat 12
        arcade_flat_col
    .endrepeat
.assert * - arcade_stage4_map = default_stage_width * map_col_stride, error, "Unexpected stage 4 map size"

arcade_stage4_items:
    ; Slot 0: armor recovery before checkpoint (X=424, Y=176)
    .byte 1                             ; item_armor
    .word 424, 176

arcade_stage4_plats:
    ; Slot 0: horizontal bridge over first lava chasm (X=120..168, Y=160)
    .byte 2                             ; plat_moving_h
    .word 120, 160
    .byte 48
    ; Slot 1: volcanic pillar (tombstone) on mid bridge (X=224, Y=160)
    .byte 1                             ; plat_tombstone
    .word 224, 160
    .byte 0
    ; Slot 2: horizontal bridge over second lava chasm (X=504..552, Y=160)
    .byte 2                             ; plat_moving_h
    .word 504, 160
    .byte 48

; Volcanic palettes (crimson magma, burnt orange, ash grey, ember gold)
arcade_stage4_bg_pal:
    .byte $0f, $16, $06, $27,  $0f, $27, $17, $37
    .byte $0f, $10, $00, $2d,  $0f, $15, $05, $26

arcade_stage4_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $16, $27, $38
    .byte $0f, $17, $28, $30,  $0f, $06, $16, $27

; Stage 4 custom BG volcanic basalt tiles (64 bytes)
arcade_stage4_bg_chr:
    .byte $00, $00, $18, $3c, $7e, $ff, $db, $00
    .byte $00, $00, $00, $18, $3c, $66, $c3, $00
    .byte $ff, $e7, $ff, $bd, $81, $c3, $ff, $00
    .byte $ff, $ff, $c3, $81, $00, $00, $00, $00
    .byte $ff, $81, $bd, $a5, $a5, $bd, $81, $ff
    .byte $00, $7e, $42, $5a, $5a, $42, $7e, $00
    .byte $c3, $99, $3c, $66, $66, $3c, $99, $c3
    .byte $3c, $66, $c3, $99, $99, $c3, $66, $3c

; Stage 4 custom OBJ tiles for the boss's upper half (64 bytes).
arcade_stage4_spr_chr:
    .byte $18, $3c, $7e, $ff, $db, $ff, $7e, $3c
    .byte $00, $18, $3c, $7e, $24, $7e, $3c, $18
    .byte $7f, $ff, $df, $ff, $7f, $3f, $1f, $3f
    .byte $3f, $7f, $20, $7f, $3f, $1f, $0f, $1f
    .byte $18, $3c, $7e, $ff, $db, $ff, $7e, $3c
    .byte $00, $18, $3c, $7e, $24, $7e, $3c, $18
    .byte $fe, $ff, $fb, $ff, $fe, $fc, $f8, $fc
    .byte $fc, $fe, $04, $fe, $fc, $f8, $f0, $f8
