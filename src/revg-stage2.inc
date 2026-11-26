; Stage 2 with the real Revision G section 1 layout (魔物の村〜氷の館〜一角獣×2).
; revg-section1-map.inc: 224 columns (14 pages) x 30 rows (2 vertical pages).
; Local X = arcade X - 3584 (0..3583), FC feet = 512 - arcade Y.
revg2_start_x = 80              ; Arcade start (3664, 112)
revg2_start_y = 400             ; 512 - 112
revg2_cp_x = 1152               ; Arcade checkpoint (4736, 80), camera X 4608 (page 4)
revg2_cp_y = 432                ; 512 - 80
revg2_cp_page = 4
revg2_goal_x = 3568             ; Right walk limit. Stage ends on key pickup after 2x Unicorn.
revg2_boss_trigger_x = 3328     ; Camera X 3328 (arcade 6912) is the right end of section 1
revg2_boss_arena_left = 3328
revg2_boss_start_x = 3456
revg2_boss_min_x = 3360
revg2_boss_max_x = 3560
revg2_boss_hp = 10
revg2_boss_fire_period = 48

revg2_header:
    stage_header revg2_width, revg2_height, revg2_width / 16 - 1, 1, revg2_start_x, revg2_cp_x, <revg2_cp_y, revg2_cp_page, 0, revg2_goal_x, <revg2_cp_y, revg2_map, revg2_palettes
.assert revg2_header = stage_header_base, lderror, "Stage 2 header offset mismatch"
.res $7f - (* - revg2_header), $ff

revg2_desc:
.assert revg2_desc = sd_base, lderror, "Stage 2 descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_boss_follow | sd_flag_start_page1
    .word revg2_start_y, revg2_cp_y     ; Start and checkpoint Y (16-bit FC feet)
    .byte 1                             ; Checkpoint camera Y page (bottom page)
    .word revg2_cp_y                    ; Goal Y
    .byte snd_song_stage, snd_song_boss
    .word revg2_bg_pal, 0               ; Stage 2 village/ice-palace BG palette, default OBJ
    .word 0, 0                          ; Default BG and OBJ CHR
    .word 0, 0                          ; No legacy fixed enemy
    .word 0, 0                          ; No legacy pot enemy
    .byte 0
    .byte 0                             ; No legacy items
    .word 0
    .byte 4                             ; 4 dynamic moving platforms (tower & water channel)
    .word revg2_plats
    .byte $ff                           ; No legacy scheduler
    .word 0, 0
    .word 0, 0
    .word revg2_boss_trigger_x, 0
    .word revg2_boss_arena_left
    .word revg2_boss_start_x, revg2_boss_min_x, revg2_boss_max_x
    .word revg2_cp_y                    ; Boss feet
    .byte revg2_boss_hp, 0
    .byte revg2_boss_fire_period

; Stage 2 initial platforms (Zone 0: Tower). Ice Palace zones 1..4 are synced by BANK49 s2::sync_platforms.
revg2_plats:
    .byte 4                             ; plat_falling (tower support $03 var 0 at local X=392, Y=342)
    .word 392, 342
    .byte 112
    .byte 4                             ; plat_falling (tower chasm support $03 var 1 at local X=704, Y=384)
    .word 704, 384
    .byte 96
    .byte 0                             ; unused in Zone 0
    .word 0, 0
    .byte 0
    .byte 0                             ; unused in Zone 0
    .word 0, 0
    .byte 0

; Stage 2 BG palettes: slate village stone, crimson roof/brick, ice palace cyan, warm gold.
revg2_bg_pal:
    .byte $0f, $10, $00, $20,  $0f, $06, $16, $27
    .byte $0f, $11, $21, $31,  $0f, $07, $17, $37

.res $100 - 1 - (* - revg2_header), $ff

    .include "revg-section1-map.inc"
