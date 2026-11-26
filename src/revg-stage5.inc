; Stage 5 with the real Revision G section 4 layout (城外〜城内タワー〜サタン).
; revg-section4-map.inc: 48 columns (3 pages) x 75 rows (5 vertical pages, 4-bit nibble packed).
; Local X = arcade X - 6656 (0..767), FC feet = 1200 - (arcade Y - 512).
revg5_start_x = 80              ; Arcade start (6736, 592)
revg5_start_y = 1136            ; Ground row 77 -> FC row 71 (71 * 16)
revg5_cp_x = 128                ; Arcade checkpoint (6784, 592)
revg5_cp_y = 1136
revg5_cp_page = 0
revg5_goal_x = 752              ; Right walk limit. Stage ends on key pickup after Satan.
revg5_boss_trigger_x = 512
revg5_boss_arena_left = 512
revg5_boss_start_x = 672
revg5_boss_min_x = 528
revg5_boss_max_x = 736
revg5_boss_hp = 4
revg5_boss_fire_period = 60

revg5_header:
    stage_header revg5_width, revg5_height, revg5_width / 16 - 1, 4, revg5_start_x, revg5_cp_x, <revg5_cp_y, revg5_cp_page, 0, revg5_goal_x, <revg5_cp_y, revg5_map, revg5_palettes
.assert revg5_header = stage_header_base, lderror, "Stage 5 header offset mismatch"
.res $7f - (* - revg5_header), $ff

revg5_desc:
.assert revg5_desc = sd_base, lderror, "Stage 5 descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_boss_follow | sd_flag_start_page1
    .word revg5_start_y, revg5_cp_y     ; Start and checkpoint Y (16-bit FC feet)
    .byte 4                             ; Checkpoint camera Y page (bottom page 4 of 0..4)
    .word revg5_cp_y                    ; Goal Y
    .byte snd_song_stage5, snd_song_boss
    .word revg5_bg_pal, 0
    .word 0, 0
    .word 0, 0
    .word 0, 0
    .byte 0
    .byte 0
    .word 0
    .byte 3                             ; 3 ball platforms in the castle tower
    .word revg5_plats
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg5_boss_trigger_x, 0
    .word revg5_boss_arena_left
    .word revg5_boss_start_x, revg5_boss_min_x, revg5_boss_max_x
    .word revg5_cp_y
    .byte revg5_boss_hp, 0
    .byte revg5_boss_fire_period

revg5_plats:
    .byte 2                             ; plat_moving_h (ball platform 1)
    .word 280, 600
    .byte 48
    .byte 2                             ; plat_moving_h (ball platform 2)
    .word 376, 680
    .byte 48
    .byte 2                             ; plat_moving_h (ball platform 3)
    .word 520, 520
    .byte 48

revg5_bg_pal:
    .byte $0f, $10, $00, $2d,  $0f, $2d, $10, $3d
    .byte $0f, $05, $0c, $10,  $0f, $2d, $00, $3d

.res $100 - 1 - (* - revg5_header), $ff

    .include "revg-section4-map.inc"
