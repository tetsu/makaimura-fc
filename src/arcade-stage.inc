; Bank 12: 2D metatile test stage for arcade player physics, terrain, and armor.
; Mapped into the $8000 window (bank 12).

arcade_stage_bank = 12
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
arcade_stage_header:
    stage_header default_stage_width, default_stage_height, 2, 2, 48, 480, 176, 1, 16, 760, 176, arcade_stage_map, arcade_stage_palettes
.assert arcade_stage_header = stage_header_base, lderror, "Arcade stage header offset mismatch"

arcade_stage_palettes:
    .repeat default_stage_width / 2, idx
        pal_pair (idx .mod 4)
    .endrepeat
.assert * - arcade_stage_palettes = default_stage_width, error, "Unexpected arcade palette width"
.res 255 - (* - arcade_stage_header), $ff

arcade_stage_map:
    ; Columns 0-9 (X=0..159): flat start zone
    .repeat 10
        arcade_flat_col
    .endrepeat
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
    ; Columns 24-30 (X=384..495): tombstone (col 26), pot enemy (col 28), checkpoint (col 30)
    .repeat 7
        arcade_flat_col
    .endrepeat
    ; Columns 31-33 (X=496..543, across X=512 page 1->2 boundary): pit with moving platform
    arcade_pit_col
    arcade_pit_col
    arcade_pit_col
    ; Columns 34-47 (X=544..767): vertical platform (col 36), dagger item (col 40), goal (X=760)
    .repeat 14
        arcade_flat_col
    .endrepeat
.assert * - arcade_stage_map = default_stage_width * map_col_stride, error, "Unexpected arcade map size"
