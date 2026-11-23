; Stage 1 with the real Revision G section 0 layout (the 512 KiB game, ./build.ps1 -Hardware).
; revg-section0-map.inc (committed; ./build.ps1 -Hardware -RefreshRevgMap rebuilds it from the arcade ROM):
; 224 columns (14 pages), one-way floors, ladders, pits, the lake and tombstones.
; The art is a placeholder. Coordinates are FC world pixels: X as in the arcade,
; feet = 256 - arcade Y (docs/stage-design.md, "Revision G の実マップ").
revg_start_x = 96               ; Arcade start (96, 64)
revg_cp_x = 1920                ; Arcade checkpoint (1920, 64), camera X 1792
revg_cp_page = 7
revg_floor = 192
revg_goal_x = 3568              ; Right walk limit. The stage ends when the player takes the key
revg_raft_x = 1664              ; Arcade path platform (1664, 36) over the lake
revg_raft_feet = 194            ; MAME: the player stands at Y=60 on the raft at Y=36
revg_raft_span = 112            ; Lake columns 103-111 (X 1648-1791)
; Placeholder enemies until the arcade enemy AI (TODO C-2). Zombies use the prototype
; zombie and appear where the arcade puts the first one: camera X + 128 + 144.
revg_zombie_ahead = 272
; The prototype boss stands in for the unicorn at the gate. The arcade unicorn takes 10 hits.
revg_boss_trigger_x = 3392
revg_boss_arena_left = 3336     ; Camera X 3328 is the right end of section 0
revg_boss_start_x = 3560
revg_boss_min_x = 3448
revg_boss_max_x = 3560
revg_boss_hp = 10
revg_boss_fire_period = 48

revg_header:
    stage_header revg_width, revg_height, revg_width / 16 - 1, 0, revg_start_x, revg_cp_x, revg_floor, revg_cp_page, 16, revg_goal_x, revg_floor, revg_map, revg_palettes
.assert revg_header = stage_header_base, lderror, "Real stage header offset mismatch"
.res $7f - (* - revg_header), $ff

revg_desc:
.assert revg_desc = sd_base, lderror, "Real stage descriptor offset mismatch"
    .byte sd_flag_has_boss | sd_flag_sched_table | sd_flag_boss_follow
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
    .byte $ff                           ; The scheduler table replaces the single page
    .word 0, 0                          ; No turret
    .word 0, 0                          ; No zombie spawn
    .word revg_boss_trigger_x, 0
    .word revg_boss_arena_left
    .word revg_boss_start_x, revg_boss_min_x, revg_boss_max_x
    .word revg_floor                    ; Boss feet
    .byte revg_boss_hp, 0               ; No lower armour
    .byte revg_boss_fire_period
.assert * = sd_sched_table_ptr, error, "Real stage boss fields out of place"
    .word revg_sched
revg_plats:
    .byte 2                             ; plat_moving_h
    .word revg_raft_x, revg_raft_feet
    .byte revg_raft_span
.res $100 - 1 - (* - revg_header), $ff

    .include "revg-section0-map.inc"

; Zombies on the open ground of the graveyard (pages 0, 1, 4) and the forest (8, 11).
revg_sched:
    .repeat sd_sched_pages, page
        .if page = 0 .or page = 1 .or page = 4 .or page = 8 .or page = 11
            .byte page
            .word 0, 0                  ; No turret
            .word $8000 | revg_zombie_ahead, revg_floor
        .else
            .byte $ff
            .res sd_sched_stride - 1, 0
        .endif
    .endrepeat
