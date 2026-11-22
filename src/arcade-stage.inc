; Bank 12: Arcade Stage 1 (Horizontal-focused 48x15 cemetery/forest stage).
; Mapped into the $8000 window (bank 12).
; Also defines the shared $8080 arcade_stage_desc layout used by Bank 12+.

arcade_stage_bank = 12

; Arcade stage descriptor flags ($8080)
sd_flag_vertical    = $01
sd_flag_has_boss    = $02
sd_flag_start_page1 = $04
sd_flag_sched_table = $08

; Fixed descriptor addresses in the $8000 stage window (59 bytes: $8080..$80BA)
sd_base          = $8080
sd_flags         = sd_base + 0
sd_start_y_lo    = sd_base + 1
sd_start_y_hi    = sd_base + 2
sd_cp_y_lo       = sd_base + 3
sd_cp_y_hi       = sd_base + 4
sd_cp_cam_y_page = sd_base + 5
sd_goal_y_lo     = sd_base + 6
sd_goal_y_hi     = sd_base + 7
sd_bgm_stage     = sd_base + 8
sd_bgm_boss      = sd_base + 9
sd_bg_pal_ptr    = sd_base + 10
sd_spr_pal_ptr   = sd_base + 12
sd_bg_chr_ptr    = sd_base + 14
sd_spr_chr_ptr   = sd_base + 16
sd_enemy1_x      = sd_base + 18
sd_enemy1_y      = sd_base + 20
sd_pot_x         = sd_base + 22
sd_pot_y         = sd_base + 24
sd_pot_drop      = sd_base + 26
sd_item_count    = sd_base + 27
sd_items_ptr     = sd_base + 28
sd_plat_count    = sd_base + 30
sd_plats_ptr     = sd_base + 31
sd_sched_page    = sd_base + 33
sd_turret_x      = sd_base + 34
sd_turret_y      = sd_base + 36
sd_zombie_x      = sd_base + 38
sd_zombie_y      = sd_base + 40
sd_boss_trig_x   = sd_base + 42
sd_boss_trig_y   = sd_base + 44
sd_boss_arena_l  = sd_base + 46
sd_boss_start_x  = sd_base + 48
sd_boss_min_x    = sd_base + 50
sd_boss_max_x    = sd_base + 52
sd_boss_y        = sd_base + 54
sd_boss_hp       = sd_base + 56
sd_boss_shield   = sd_base + 57
sd_boss_fire_per = sd_base + 58
; Optional pointer to 9-byte records (page, turret X/Y, zombie X/Y).
sd_sched_table_ptr = sd_base + 59
sd_sched_stride  = 9
sd_sched_pages   = 3
.assert (sd_sched_pages - 1) * sd_sched_stride < sd_sched_page - sd_base, error, "Scheduler record offsets must distinguish table and legacy records"

; Stage 1 constants (kept for clarity and regression compatibility)
arcade_enemy_x = 312
arcade_enemy_feet = 176
arcade_pot_enemy_x = 448
arcade_pot_enemy_feet = 176
arcade_armor_item_x = 320
arcade_armor_item_feet = 112
arcade_dagger_item_x = 640
arcade_dagger_item_feet = 176
arcade_tombstone_x = 416
arcade_tombstone_y = 160
arcade_hplat_x = 500
arcade_hplat_y = 160
arcade_hplat_span = 44
arcade_vplat_x = 576
arcade_vplat_y = 176
arcade_vplat_span = 40
arcade_turret_x = 712
arcade_turret_feet = 176
arcade_zombie_spawn_x = 752
arcade_zombie_feet = 176
arcade_boss_trigger_x = 720
arcade_boss_arena_left = 520
arcade_boss_start_x = 736
arcade_boss_min_x = 684
arcade_boss_max_x = 744
arcade_boss_feet = 176
arcade_boss_hp_init = 4
arcade_boss_shield_init = 2
arcade_boss_fire_period = 48

; Authentic Makaimura Stage 1 graveyard/mountain background column helpers
; All background deco metatiles (mt_cross..mt_castle) have tile_type_air so
; collision and gameplay physics remain 100% identical to arcade_flat_col.
.macro arcade_flat_deco_col r4, r5, r9, r10
    .repeat map_col_stride, row
        .if row = 4
            .byte r4
        .elseif row = 5
            .byte r5
        .elseif row = 9
            .byte r9
        .elseif row = 10
            .byte r10
        .elseif row < 11
            .byte mt_sky
        .elseif row = 11
            .byte mt_ledge
        .else
            .byte mt_rock
        .endif
    .endrepeat
.endmacro

.macro arcade_flat_col
    cliff_col_2d 11
.endmacro

.macro arcade_pit_col
    .repeat map_col_stride, row
        .if row < 11
            .byte mt_sky
        .else
            .byte mt_hazard
        .endif
    .endrepeat
.endmacro

.macro arcade_oneway_col row_oneway, wall_row
    .repeat map_col_stride, row
        .if row = row_oneway
            .byte mt_oneway
        .elseif wall_row > 0 .and row = wall_row
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

.macro arcade_ladder_col top_row, bottom_row
    .repeat map_col_stride, row
        .if row >= top_row .and row <= bottom_row
            .byte mt_ladder
        .elseif row < 11
            .byte mt_sky
        .elseif row = 11
            .byte mt_ledge
        .else
            .byte mt_rock
        .endif
    .endrepeat
.endmacro

.segment "BANK12"
    .byte bank_tag_base + arcade_stage_bank
.ifdef PLATFORM_COURSE
    .include "platform-course.inc"
.else
arcade_stage_header:
    stage_header default_stage_width, default_stage_height, 2, 2, 48, 480, 176, 1, 16, 760, 176, arcade_stage_map, arcade_stage_palettes
.assert arcade_stage_header = stage_header_base, lderror, "Arcade stage header offset mismatch"

arcade_stage_palettes:
    ; Use BG pal 0 (grass/earth/dead tree) and BG pal 1 (graveyard fence/cross/mountain)
    .repeat default_stage_width / 2, idx
        pal_pair (idx .mod 2)
    .endrepeat
.assert * - arcade_stage_palettes = default_stage_width, error, "Unexpected arcade palette width"
.res 127 - (* - arcade_stage_header), $ff

arcade_stage1_desc:
.assert arcade_stage1_desc = sd_base, lderror, "Stage 1 descriptor offset mismatch"
    .byte sd_flag_has_boss              ; sd_flags
    .word 176                           ; sd_start_y
    .word 176                           ; sd_cp_y
    .byte 0                             ; sd_cp_cam_y_page
    .word 176                           ; sd_goal_y
    .byte snd_song_stage                ; sd_bgm_stage
    .byte snd_song_boss                 ; sd_bgm_boss
    .word arcade_stage1_bg_pal          ; sd_bg_pal_ptr
    .word arcade_stage1_spr_pal         ; sd_spr_pal_ptr
    .word 0                             ; sd_bg_chr_ptr (use default terrain tiles)
    .word 0                             ; sd_spr_chr_ptr (use default sprite tiles)
    .word arcade_enemy_x                ; sd_enemy1_x
    .word arcade_enemy_feet             ; sd_enemy1_y
    .word arcade_pot_enemy_x            ; sd_pot_x
    .word arcade_pot_enemy_feet         ; sd_pot_y
    .byte 3                             ; sd_pot_drop (item_torch)
    .byte 2                             ; sd_item_count
    .word arcade_stage1_items           ; sd_items_ptr
    .byte 3                             ; sd_plat_count
    .word arcade_stage1_plats           ; sd_plats_ptr
    .byte 2                             ; sd_sched_page (camera_high == 2)
    .word arcade_turret_x               ; sd_turret_x
    .word arcade_turret_feet            ; sd_turret_y
    .word arcade_zombie_spawn_x         ; sd_zombie_x
    .word arcade_zombie_feet            ; sd_zombie_y
    .word arcade_boss_trigger_x         ; sd_boss_trig_x
    .word 0                             ; sd_boss_trig_y (unused in horizontal)
    .word arcade_boss_arena_left        ; sd_boss_arena_l
    .word arcade_boss_start_x           ; sd_boss_start_x
    .word arcade_boss_min_x             ; sd_boss_min_x
    .word arcade_boss_max_x             ; sd_boss_max_x
    .word arcade_boss_feet              ; sd_boss_y
    .byte arcade_boss_hp_init           ; sd_boss_hp
    .byte arcade_boss_shield_init       ; sd_boss_shield
    .byte arcade_boss_fire_period       ; sd_boss_fire_per

.res 255 - (* - arcade_stage_header), $ff

arcade_stage_map:
.assert arcade_stage_map = $8100, lderror, "Stage 1 map must be 256-byte aligned"
    ; Columns 0-9 (X=0..159): Authentic Makaimura Stage 1 graveyard start with mountains, castle, crosses, tombstones, dead trees
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_cross
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_tomb_bg
    arcade_flat_deco_col mt_sky,    mt_mtn_l,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_mtn_c,  mt_mtn_r,  mt_tree_hi, mt_tree_lo
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_castle, mt_mtn_c,  mt_sky,     mt_cross
    arcade_flat_deco_col mt_sky,    mt_mtn_r,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_tomb_bg
    ; Columns 10-11 (X=160..191): hazard pit (32 px wide)
    arcade_pit_col
    arcade_pit_col
    ; Columns 12-13 (X=192..223): landing flat
    arcade_flat_col
    arcade_flat_col
    ; Column 14 (X=224..239): one-way platform at row 9 (Y=144)
    arcade_oneway_col 9, 0
    ; Column 15 (X=240..255): wall at row 10 (Y=160) + one-way at row 9 (Y=144)
    arcade_oneway_col 9, 10
    ; Column 16 (X=256..271, across nametable seam): one-way at row 9 (Y=144)
    arcade_oneway_col 9, 0
    ; Column 17 (X=272..287): ladder from row 7 (Y=112) to row 10 (Y=160)
    arcade_ladder_col 7, 10
    ; Columns 18-23 (X=288..383): two-tier section (one-way at Y=112, floor at Y=176)
    .repeat 6
        arcade_oneway_col 7, 0
    .endrepeat
    ; Columns 24-30 (X=384..495): graveyard crosses, dead tree, tombstone (col 26), pot enemy (col 28), checkpoint (col 30)
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_mtn_l,  mt_sky,     mt_cross
    arcade_flat_deco_col mt_mtn_c,  mt_mtn_r,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_tree_hi, mt_tree_lo
    arcade_flat_deco_col mt_castle, mt_mtn_c,  mt_sky,     mt_tomb_bg
    arcade_flat_deco_col mt_sky,    mt_mtn_r,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_cross
    ; Columns 31-33 (X=496..543, across X=512 page 1->2 boundary): pit with moving platform
    arcade_pit_col
    arcade_pit_col
    arcade_pit_col
    ; Columns 34-47 (X=544..767): graveyard approach to Unicorn Cyclops arena (X=760)
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_tree_hi, mt_tree_lo
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_mtn_l,  mt_sky,     mt_cross
    arcade_flat_deco_col mt_mtn_c,  mt_mtn_r,  mt_sky,     mt_tomb_bg
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_castle, mt_mtn_c,  mt_sky,     mt_cross
    arcade_flat_deco_col mt_sky,    mt_mtn_r,  mt_tree_hi, mt_tree_lo
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_sky,    mt_sky,     mt_tomb_bg
    arcade_flat_deco_col mt_sky,    mt_mtn_l,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_mtn_c,  mt_mtn_r,  mt_sky,     mt_cross
    arcade_flat_deco_col mt_castle, mt_mtn_c,  mt_sky,     mt_gravel
    arcade_flat_deco_col mt_sky,    mt_mtn_r,  mt_sky,     mt_tomb_bg
.assert * - arcade_stage_map = default_stage_width * map_col_stride, error, "Unexpected arcade map size"

; Stage 1 data tables (referenced by arcade_stage1_desc)
arcade_stage1_items:
    ; Each entry: type (1B), x (2B), y (2B)
    .byte 1                             ; item_armor
    .word arcade_armor_item_x, arcade_armor_item_feet
    .byte 2                             ; item_dagger
    .word arcade_dagger_item_x, arcade_dagger_item_feet

arcade_stage1_plats:
    ; Each entry: type (1B), x (2B), y (2B), span (1B)
    .byte 1                             ; plat_tombstone
    .word arcade_tombstone_x, arcade_tombstone_y
    .byte 0
    .byte 2                             ; plat_moving_h
    .word arcade_hplat_x, arcade_hplat_y
    .byte arcade_hplat_span
    .byte 3                             ; plat_moving_v
    .word arcade_vplat_x, arcade_vplat_y
    .byte arcade_vplat_span

; Authentic Makaimura Stage 1 palettes (matched to revg Stage 1 graveyard screenshot):
;   BG0: Black night sky, dark brown soil, tan dirt, vivid green grass
;   BG1: Black night sky, dark gray stone, silver fence/cross/mtn, green grass highlight
;   BG2: Black night sky, deep navy, castle blue, white
;   BG3: Black night sky, crimson red, gold HUD, white
arcade_stage1_bg_pal:
    .byte $0f, $07, $17, $1a,  $0f, $00, $10, $1a
    .byte $0f, $01, $12, $30,  $0f, $16, $28, $30

;   OBJ0: Armored Arthur / Tombstone / Lance / Dagger (dark steel, silver, white)
;   OBJ1: Naked Arthur / Torch Flame / Key / Fireball (crimson, flesh/orange, yellow-white)
;   OBJ2: Zombie (blue coat, red hair/eyes, pale white face)
;   OBJ3: Unicorn Cyclops Boss / Green Monster / Moving Platform (green, tan/brown, white)
arcade_stage1_spr_pal:
    .byte $0f, $00, $10, $30,  $0f, $16, $27, $38
    .byte $0f, $12, $16, $30,  $0f, $1a, $27, $30
.endif
