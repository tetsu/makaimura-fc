; Stage 6 with the real Revision G section 5 layout (城内最上層〜2体サタン).
; revg-section5-map.inc: 32 columns (2 pages) x 75 rows (5 vertical pages, 4-bit nibble packed).
; Local X = arcade X - 7424 (0..511), FC feet = 1200 - (arcade Y - 1536).
revg6_start_x = 48              ; Arcade start (7472, 1616)
revg6_start_y = 1120            ; 1200 - 80 (bottom page 4)
revg6_cp_x = 48                 ; Arcade checkpoint (7472, 1616)
revg6_cp_y = 1120
revg6_cp_page = 0
revg6_goal_x = 496              ; Right walk limit. Stage ends on key pickup after 2x Satan.
revg6_boss_trigger_x = 256
revg6_boss_arena_left = 256
revg6_boss_start_x = 384
revg6_boss_min_x = 272
revg6_boss_max_x = 480
revg6_boss_hp = 4
revg6_boss_fire_period = 56

revg6_header:
    stage_header revg6_width, revg6_height, revg6_width / 16 - 1, 4, revg6_start_x, revg6_cp_x, <revg6_cp_y, revg6_cp_page, 0, revg6_goal_x, <revg6_cp_y, revg6_map, revg6_palettes
.assert revg6_header = stage_header_base, lderror, "Stage 6 header offset mismatch"
.res $7f - (* - revg6_header), $ff

revg6_desc:
.assert revg6_desc = sd_base, lderror, "Stage 6 descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_boss_follow | sd_flag_start_page1
    .word revg6_start_y, revg6_cp_y     ; Start and checkpoint Y (16-bit FC feet)
    .byte 4                             ; Checkpoint camera Y page (bottom page 4 of 0..4)
    .word revg6_cp_y                    ; Goal Y
    .byte snd_song_stage5, snd_song_boss
    .word revg6_bg_pal, 0
    .word 0, 0
    .word 0, 0
    .word 0, 0
    .byte 0
    .byte 0
    .word 0
    .byte 0                             ; No dynamic moving platforms in Stage 6
    .word 0
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg6_boss_trigger_x, 0
    .word revg6_boss_arena_left
    .word revg6_boss_start_x, revg6_boss_min_x, revg6_boss_max_x
    .word revg6_cp_y
    .byte revg6_boss_hp, 0
    .byte revg6_boss_fire_period

revg6_bg_pal:
    .byte $0f, $10, $00, $08,  $0f, $2d, $00, $10
    .byte $0f, $2d, $10, $3d,  $0f, $2d, $00, $3d

.res $100 - 1 - (* - revg6_header), $ff

    .include "revg-section5-map.inc"
