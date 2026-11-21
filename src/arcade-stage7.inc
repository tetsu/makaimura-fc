; Bank 18: Arcade Stage 7 (Fixed 16x15 Demon King's Throne Boss Arena).
; Mapped into the $8000 window (bank 18).
; Loop 1 clear -> loops to Stage 1 (Loop 2, rank retained).
; Loop 2 clear -> True Ending (FINAL CLEAR -> Title).

arcade_stage7_bank = 18
stage7_width = 16
stage7_height = 15

.macro throne_arena_col col_idx
    .repeat map_col_stride, row
        .if col_idx = 0 .or col_idx = 15
            .byte mt_rock
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

.segment "BANK18"
    .byte bank_tag_base + arcade_stage7_bank
arcade_stage7_header:
    stage_header stage7_width, stage7_height, 0, 0, 40, 40, 176, 0, 0, 224, 176, arcade_stage7_map, arcade_stage7_palettes
.assert arcade_stage7_header = stage_header_base, lderror, "Stage 7 header offset mismatch"

arcade_stage7_palettes:
    .repeat stage7_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage7_palettes = stage7_width, error, "Unexpected stage 7 palette width"
.res 127 - (* - arcade_stage7_header), $ff

arcade_stage7_desc:
.assert arcade_stage7_desc = sd_base, lderror, "Stage 7 descriptor offset mismatch"
    .byte sd_flag_has_boss              ; sd_flags (fixed horizontal arena + final boss)
    .word 176                           ; sd_start_y
    .word 176                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 176                           ; sd_goal_y
    .byte snd_song_boss                 ; sd_bgm_stage (throne starts straight in boss mood)
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage7_bg_pal          ; sd_bg_pal_ptr (throne obsidian/gold/royal purple)
    .word arcade_stage7_spr_pal         ; sd_spr_pal_ptr (demon king purple/gold/crimson)
    .word arcade_stage7_bg_chr          ; sd_bg_chr_ptr (throne pillar/mosaic tiles)
    .word arcade_stage7_spr_chr         ; sd_spr_chr_ptr (demon king sprite tiles)
    .word 0                             ; sd_enemy1_x (none)
    .word 0                             ; sd_enemy1_y
    .word 0                             ; sd_pot_x (none)
    .word 0                             ; sd_pot_y
    .byte 0                             ; sd_pot_drop
    .byte 0                             ; sd_item_count
    .word 0                             ; sd_items_ptr
    .byte 0                             ; sd_plat_count
    .word 0                             ; sd_plats_ptr
    .byte $ff                           ; sd_sched_page (disabled in throne arena)
    .word 0                             ; sd_turret_x
    .word 0                             ; sd_turret_y
    .word 0                             ; sd_zombie_x
    .word 0                             ; sd_zombie_y
    .word 52                            ; sd_boss_trig_x (triggers immediately on approach)
    .word 0                             ; sd_boss_trig_y
    .word 24                            ; sd_boss_arena_l
    .word 204                           ; sd_boss_start_x
    .word 148                           ; sd_boss_min_x
    .word 220                           ; sd_boss_max_x
    .word 176                           ; sd_boss_y
    .byte 8                             ; sd_boss_hp (Demon King core HP)
    .byte 4                             ; sd_boss_shield (Demon King lower armor)
    .byte 34                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage7_header), $ff

arcade_stage7_map:
.assert arcade_stage7_map = $8100, lderror, "Stage 7 map must be 256-byte aligned"
    .repeat stage7_width, col_idx
        throne_arena_col col_idx
    .endrepeat
.assert * - arcade_stage7_map = stage7_width * map_col_stride, error, "Unexpected stage 7 map size"

; Demon King's Throne palettes (obsidian black, imperial gold, royal violet, white)
arcade_stage7_bg_pal:
    .byte $0f, $28, $18, $38,  $0f, $14, $04, $24
    .byte $0f, $16, $06, $27,  $0f, $10, $00, $30

arcade_stage7_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $14, $24, $38
    .byte $0f, $16, $28, $30,  $0f, $04, $14, $28

; Stage 7 custom BG throne mosaic tiles (64 bytes)
arcade_stage7_bg_chr:
    .byte $00, $00, $00, $81, $c3, $e7, $ff, $00
    .byte $00, $00, $00, $00, $81, $c3, $ff, $00
    .byte $ff, $ff, $db, $ff, $ff, $81, $ff, $00
    .byte $ff, $24, $ff, $00, $7e, $ff, $00, $00
    .byte $ff, $81, $99, $bd, $bd, $99, $81, $ff
    .byte $00, $7e, $66, $42, $42, $66, $7e, $00
    .byte $db, $ff, $81, $bd, $bd, $81, $ff, $db
    .byte $24, $00, $7e, $42, $42, $7e, $00, $24

; Stage 7 custom OBJ Demon King tiles (64 bytes: sprite tiles 5..8)
arcade_stage7_spr_chr:
    .byte $81, $c3, $e7, $ff, $db, $ff, $7e, $3f
    .byte $80, $c0, $67, $7f, $24, $7f, $3e, $1f
    .byte $7f, $ff, $ff, $ef, $7f, $3f, $3e, $7f
    .byte $3f, $7f, $7f, $10, $3f, $1f, $1e, $3f
    .byte $81, $c3, $e7, $ff, $db, $ff, $7e, $fc
    .byte $01, $03, $e6, $fe, $24, $fe, $7c, $f8
    .byte $fe, $ff, $ff, $f7, $fe, $fc, $7c, $fe
    .byte $fc, $fe, $fe, $08, $fc, $f8, $78, $fc
