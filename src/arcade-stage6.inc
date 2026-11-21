; Bank 17: Arcade Stage 6 (Vertical-climbing 16x30 Inner Castle / Throne Approach).
; Mapped into the $8000 window (bank 17).
; Pot enemy on Tier 1 (Y=368) drops the required 4th weapon: Holy Shield (item_shield = 6).
; Clearing Stage 6 without Holy Shield bounces the player back to Stage 5.

arcade_stage6_bank = 17
stage6_width = 16
stage6_height = 30

.segment "BANK17"
    .byte bank_tag_base + arcade_stage6_bank
arcade_stage6_header:
    stage_header stage6_width, stage6_height, 0, 1, 48, 200, 208, 0, 0, 208, 144, arcade_stage6_map, arcade_stage6_palettes
.assert arcade_stage6_header = stage_header_base, lderror, "Stage 6 header offset mismatch"

arcade_stage6_palettes:
    .repeat stage6_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage6_palettes = stage6_width, error, "Unexpected stage 6 palette width"
.res 127 - (* - arcade_stage6_header), $ff

arcade_stage6_desc:
.assert arcade_stage6_desc = sd_base, lderror, "Stage 6 descriptor offset mismatch"
    .byte sd_flag_vertical | sd_flag_has_boss | sd_flag_start_page1 ; sd_flags
    .word 448                           ; sd_start_y
    .word 208                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 144                           ; sd_goal_y
    .byte snd_song_stage                ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage6_bg_pal          ; sd_bg_pal_ptr (inner castle crimson/bone/black)
    .word arcade_stage6_spr_pal         ; sd_spr_pal_ptr (throne guard crimson/bone/gold)
    .word arcade_stage6_bg_chr          ; sd_bg_chr_ptr (bone-arch tiles)
    .word arcade_stage6_spr_chr         ; sd_spr_chr_ptr (throne guard captain tiles)
    .word 136                           ; sd_enemy1_x
    .word 448                           ; sd_enemy1_y
    .word 112                           ; sd_pot_x
    .word 368                           ; sd_pot_y
    .byte 6                             ; sd_pot_drop (item_shield = required 4th weapon!)
    .byte 2                             ; sd_item_count
    .word arcade_stage6_items           ; sd_items_ptr
    .byte 0                             ; sd_plat_count
    .word 0                             ; sd_plats_ptr
    .byte 0                             ; sd_sched_page (vertical page 0)
    .word 0                             ; sd_turret_x (zombies patrol Tier 3, clear of ladders)
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
    .byte 4                             ; sd_boss_shield
    .byte 36                            ; sd_boss_fire_per

.res 255 - (* - arcade_stage6_header), $ff

arcade_stage6_map:
.assert arcade_stage6_map = $8100, lderror, "Stage 6 map must be 256-byte aligned"
    .repeat stage6_width, col_idx
        tower_upper_col col_idx
    .endrepeat
    .repeat stage6_width, col_idx
        tower_lower_col col_idx
    .endrepeat
.assert * - arcade_stage6_map = stage6_width * 2 * map_col_stride, error, "Unexpected stage 6 map size"

arcade_stage6_items:
    ; Slot 0: armor recovery on checkpoint floor (Y=208)
    .byte 1                             ; item_armor
    .word 96, 208
    ; Slot 1: optional Dagger on left alcove of Tier 2 (Y=288)
    .byte 2                             ; item_dagger
    .word 32, 288

; Inner Castle palettes (deep crimson, obsidian, ivory bone, tarnished gold)
arcade_stage6_bg_pal:
    .byte $0f, $06, $16, $30,  $0f, $05, $15, $25
    .byte $0f, $28, $18, $30,  $0f, $00, $10, $30

arcade_stage6_spr_pal:
    .byte $0f, $12, $21, $30,  $0f, $06, $16, $30
    .byte $0f, $17, $28, $30,  $0f, $15, $25, $30

; Stage 6 custom BG inner castle bone-arch tiles (64 bytes)
arcade_stage6_bg_chr:
    .byte $00, $00, $3c, $42, $81, $ff, $00, $00
    .byte $00, $00, $00, $3c, $7e, $ff, $00, $00
    .byte $ff, $db, $ff, $81, $c3, $e7, $ff, $00
    .byte $ff, $24, $ff, $7e, $3c, $18, $00, $00
    .byte $c3, $e7, $ff, $db, $db, $ff, $e7, $c3
    .byte $3c, $18, $00, $24, $24, $00, $18, $3c
    .byte $ff, $c3, $a5, $99, $99, $a5, $c3, $ff
    .byte $00, $3c, $5a, $66, $66, $5a, $3c, $00

; Stage 6 custom OBJ Throne Guard Captain tiles (64 bytes: sprite tiles 5..8)
arcade_stage6_spr_chr:
    .byte $14, $3c, $7f, $7f, $5b, $7f, $3e, $1f
    .byte $00, $1c, $3f, $3f, $24, $3f, $1e, $0f
    .byte $3f, $7f, $ff, $df, $7f, $3f, $1e, $3f
    .byte $1f, $3f, $7f, $20, $3f, $1f, $0e, $1f
    .byte $28, $3c, $fe, $fe, $da, $fe, $7c, $f8
    .byte $00, $38, $fc, $fc, $24, $fc, $78, $f0
    .byte $fc, $fe, $ff, $fb, $fe, $fc, $78, $fc
    .byte $f8, $fc, $fe, $04, $fc, $f8, $70, $f8
