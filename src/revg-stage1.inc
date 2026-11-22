; Stage 1 with the real Revision G section 0 layout (512 KiB game mode, -RevgStage1).
; tools/convert-revg-section.py writes revg-section0-map.inc from the extracted map:
; 224 columns (14 pages), one-way floors, ladders, pits, the lake and tombstones.
; The art is a placeholder. Coordinates are FC world pixels: X as in the arcade,
; feet = 256 - arcade Y (docs/stage-design.md, "Revision G の実マップ").
revg_start_x = 96               ; Arcade start (96, 64)
revg_cp_x = 1920                ; Arcade checkpoint (1920, 64), camera X 1792
revg_cp_page = 7
revg_floor = 192
revg_goal_x = 3440              ; Before the gate wall; the unicorn and key come later
revg_raft_x = 1664              ; Arcade path platform (1664, 36) over the lake
revg_raft_feet = 218
revg_raft_span = 112            ; Lake columns 103-111 (X 1648-1791)

revg_header:
    stage_header revg_width, revg_height, revg_width / 16 - 1, 0, revg_start_x, revg_cp_x, revg_floor, revg_cp_page, 16, revg_goal_x, revg_floor, revg_map, revg_palettes
.assert revg_header = stage_header_base, lderror, "Real stage header offset mismatch"
.res $7f - (* - revg_header), $ff

revg_desc:
.assert revg_desc = sd_base, lderror, "Real stage descriptor offset mismatch"
    .byte 0                             ; Horizontal, no boss, no scheduler table
    .word revg_floor, revg_floor        ; Start and checkpoint Y
    .byte 0                             ; Checkpoint camera Y page
    .word revg_floor                    ; Goal Y
    .byte snd_song_stage, snd_song_boss
    .word 0, 0                          ; Default BG and OBJ palettes
    .word 0, 0                          ; Default BG and OBJ CHR
    .word 0, 0                          ; No fixed enemy
    .word 0, 0                          ; No pot enemy
    .byte 0
    .byte 0                             ; No items
    .word 0
    .byte 1
    .word revg_plats
    .byte $ff                           ; No scheduler page
    .word 0, 0                          ; No turret
    .word 0, 0                          ; No zombie spawn
    .res ::sd_sched_table_ptr - ::sd_boss_trig_x, 0
    .word 0
revg_plats:
    .byte 2                             ; plat_moving_h
    .word revg_raft_x, revg_raft_feet
    .byte revg_raft_span
.res $100 - 1 - (* - revg_header), $ff

    .include "revg-section0-map.inc"
