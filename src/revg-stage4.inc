; Stage 4 with the real Revision G section 3 layout (浮島〜吊り橋〜ドラゴン).
; revg-section3-map.inc: 192 columns (12 pages) x 15 rows (1 vertical page).
; Local X = arcade X - 3584 (0..3071), FC feet = 256 - (arcade Y - 512).
revg4_start_x = 48              ; Arcade start (3632, 576)
revg4_start_y = 176             ; Ground row 12 -> FC row 11 (11 * 16)
revg4_cp_x = 1408               ; Arcade checkpoint (4992, 576), page 5
revg4_cp_y = 192                ; Ground row 13 -> FC row 12 (12 * 16)
revg4_cp_page = 5
revg4_goal_x = 3056             ; Right walk limit. Stage ends on key pickup after Dragon.
revg4_boss_trigger_x = 2560     ; Camera X 2560 (page 10) triggers Dragon
revg4_boss_arena_left = 2816
revg4_boss_start_x = 2944
revg4_boss_min_x = 2848
revg4_boss_max_x = 3048
revg4_boss_hp = 6
revg4_boss_fire_period = 64

revg4_header:
    stage_header revg4_width, revg4_height, revg4_width / 16 - 1, 0, revg4_start_x, revg4_cp_x, <revg4_cp_y, revg4_cp_page, 0, revg4_goal_x, <revg4_start_y, revg4_map, revg4_palettes
.assert revg4_header = stage_header_base, lderror, "Stage 4 header offset mismatch"
.res $7f - (* - revg4_header), $ff

revg4_desc:
.assert revg4_desc = sd_base, lderror, "Stage 4 descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_boss_follow
    .word revg4_start_y, revg4_cp_y
    .byte 0                             ; 1 vertical page
    .word revg4_start_y
    .byte snd_song_stage3, snd_song_boss
    .word revg4_bg_pal, 0
    .word 0, 0
    .word 0, 0
    .word 0, 0
    .byte 0
    .byte 0
    .word 0
    .byte 4                             ; 4 floating platforms across the Stage 4 chasm
    .word revg4_plats
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg4_boss_trigger_x, 0
    .word revg4_boss_arena_left
    .word revg4_boss_start_x, revg4_boss_min_x, revg4_boss_max_x
    .word revg4_start_y
    .byte revg4_boss_hp, 0
    .byte revg4_boss_fire_period

; Stage 4 initial eyeball moving platforms (Zone 0, Chasm 0: X=128..303).
; Zones 1..3 (Chasms 1..3: X=400..1135) are synced dynamically by BANK51 s34::s4_sync_platforms.
revg4_plats:
    .byte 2                             ; plat_moving_h (support $05 var 3, Chasm 0)
    .word 136, 184
    .byte 80
    .byte 2                             ; plat_moving_h (support $05 var 0, Chasm 0)
    .word 160, 152
    .byte 80
    .byte 2                             ; plat_moving_h (support $05 var 2, Chasm 0)
    .word 184, 136
    .byte 88
    .byte 2                             ; plat_moving_h (support $05 var 1, Chasm 0)
    .word 208, 104
    .byte 88

revg4_bg_pal:
    .byte $0f, $10, $00, $17,  $0f, $07, $06, $17
    .byte $0f, $07, $08, $17,  $0f, $07, $17, $37

.res $100 - 1 - (* - revg4_header), $ff

    .include "revg-section3-map.inc"
