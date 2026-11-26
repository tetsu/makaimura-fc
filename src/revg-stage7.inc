; Stage 7 with the real Revision G section 6 layout (大魔王アスタロトの玉座).
; revg-section6-map.inc: 16 columns (1 page) x 15 rows (1 vertical page).
; Local X = arcade X - 7936 (0..255), FC feet = 256 - (arcade Y - 2560).
revg7_start_x = 88              ; Arcade start (8024, 2624)
revg7_start_y = 176             ; Ground row 12 -> FC row 11 (11 * 16)
revg7_cp_x = 48                 ; Arcade checkpoint (7984, 2624)
revg7_cp_y = 176
revg7_cp_page = 0
revg7_goal_x = 240
revg7_boss_trigger_x = 0
revg7_boss_arena_left = 0
revg7_boss_start_x = 160
revg7_boss_min_x = 96
revg7_boss_max_x = 224
revg7_boss_hp = 10
revg7_boss_fire_period = 60

revg7_header:
    stage_header revg7_width, revg7_height, 0, 0, revg7_start_x, revg7_cp_x, <revg7_cp_y, revg7_cp_page, 0, revg7_goal_x, <revg7_cp_y, revg7_map, revg7_palettes
.assert revg7_header = stage_header_base, lderror, "Stage 7 header offset mismatch"
.res $7f - (* - revg7_header), $ff

revg7_desc:
.assert revg7_desc = sd_base, lderror, "Stage 7 descriptor offset mismatch"
    .byte sd_flag_has_boss
    .word revg7_start_y, revg7_cp_y
    .byte 0                             ; 1 vertical page
    .word revg7_start_y
    .byte snd_song_boss, snd_song_boss
    .word revg7_bg_pal, 0
    .word 0, 0
    .word 0, 0
    .word 0, 0
    .byte 0
    .byte 0
    .word 0
    .byte 0
    .word 0
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg7_boss_trigger_x, 0
    .word revg7_boss_arena_left
    .word revg7_boss_start_x, revg7_boss_min_x, revg7_boss_max_x
    .word revg7_start_y
    .byte revg7_boss_hp, 0
    .byte revg7_boss_fire_period

revg7_bg_pal:
    .byte $0f, $10, $00, $2d,  $0f, $06, $2d, $00
    .byte $0f, $09, $00, $10,  $0f, $06, $2d, $16

.res $100 - 1 - (* - revg7_header), $ff

    .include "revg-section6-map.inc"
