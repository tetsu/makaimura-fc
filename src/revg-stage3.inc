; Stage 3 with the real Revision G section 2 layout (洞窟〜城門〜ドラゴン).
; revg-section2-map.inc: 224 columns (14 pages) x 60 rows (4 vertical pages, 4-bit nibble packed).
; Local X = arcade X (0..3583), FC feet = 960 - (arcade Y - 512).
revg3_start_x = 80              ; Arcade start (80, 576)
revg3_start_y = 896             ; 960 - 64
revg3_cp_x = 1152               ; Arcade checkpoint (1152, 576), page 4, bottom y-page 3
revg3_cp_y = 880                ; Ground row 60 -> FC row 55 (55 * 16)
revg3_cp_page = 4
revg3_goal_x = 3568             ; Right walk limit. Stage ends on key pickup after Dragon.
revg3_boss_trigger_x = 3072     ; Camera X 3072 (page 12) triggers Dragon
revg3_boss_arena_left = 3328
revg3_boss_start_x = 3456
revg3_boss_min_x = 3360
revg3_boss_max_x = 3560
revg3_boss_hp = 6
revg3_boss_fire_period = 64

revg3_header:
    stage_header revg3_width, revg3_height, revg3_width / 16 - 1, 3, revg3_start_x, revg3_cp_x, <revg3_cp_y, revg3_cp_page, 0, revg3_goal_x, <revg3_cp_y, revg3_map, revg3_palettes
.assert revg3_header = stage_header_base, lderror, "Stage 3 header offset mismatch"
.res $7f - (* - revg3_header), $ff

revg3_desc:
.assert revg3_desc = sd_base, lderror, "Stage 3 descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_boss_follow | sd_flag_start_page1
    .word revg3_start_y, revg3_cp_y     ; Start and checkpoint Y (16-bit FC feet)
    .byte 3                             ; Checkpoint camera Y page (bottom page 3 of 0..3)
    .word revg3_cp_y                    ; Goal Y
    .byte snd_song_stage3, snd_song_boss
    .word revg3_bg_pal, 0
    .word 0, 0
    .word 0, 0
    .word 0, 0
    .byte 0
    .byte 0
    .word 0
    .byte 0                             ; No dynamic moving platforms in Stage 3
    .word 0
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg3_boss_trigger_x, 0
    .word revg3_boss_arena_left
    .word revg3_boss_start_x, revg3_boss_min_x, revg3_boss_max_x
    .word revg3_cp_y
    .byte revg3_boss_hp, 0
    .byte revg3_boss_fire_period

revg3_bg_pal:
    .byte $0f, $10, $00, $08,  $0f, $08, $1c, $2c
    .byte $0f, $07, $17, $37,  $0f, $2d, $00, $10

.res $100 - 1 - (* - revg3_header), $ff

    .include "revg-section2-map.inc"
