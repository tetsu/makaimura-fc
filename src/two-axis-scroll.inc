; Horizontal mirroring gives this camera a 32x60 tile ring. The PPU
; masks the leftmost eight pixels so a replaced column is offscreen.
pan_page_rows = 30
pan_no_update = $ff

.segment "ZEROPAGE"
pan_x: .res 2
pan_y_page: .res 1
pan_tile_x: .res 1
pan_tile_y: .res 1
pan_need_row: .res 1
pan_need_col: .res 1
pan_row_addr: .res 2
pan_attr_addr: .res 2
pan_attr_value: .res 1
pan_col_addr: .res 2
pan_col_split_hi: .res 1
pan_col_split_count: .res 1
pan_data_x: .res 1
pan_data_y: .res 1
pan_local_y: .res 1
pan_addr_hi: .res 1
pan_addr_lo: .res 1
pan_attrib: .res 1
pan_ctrl: .res 1
pan_directions: .res 1
pan_top_rows: .res 1
pan_col_length: .res 1
goal_tile_x: .res 1
goal_flag_row: .res 1

.segment "BSS"
pan_row_tiles: .res 32
pan_col_tiles: .res 31

.segment "CODE"
show_pan:
    jsr blank_display
    lda #3
    sta scene
    lda #0
    sta pan_y_page
    ldy #0
    jsr draw_pan
enable_pan_preview:
    jsr enable_screen
    lda #$08            ; BG on, left eight pixels masked.
    sta $2001
    lda #0
    sta terrain_frame
    lda #$80
    sta pan_ctrl
    jmp apply_pan

.if PRG_ROM_UNITS = 32
; A = stage data bank. Open at the top left with the gameplay background assets.
show_stage_view:
    pha
    jsr blank_display
    pla
    jsr select_bank0
    lda #stage_view_scene
    sta scene
    lda #1
    sta arcade_mode
    lda #0
    sta pan_y_page
    sta campaign_stage
    tay
    jsr draw_pan
    jsr call_bank3_load_stage_visuals
    jmp enable_pan_preview

update_stage_view:
    lda pressed
    bmi @next_stage     ; A
    and #$40
    bne @prev_stage    ; B
    lda stage_height_rows
    cmp #16
    bcc @horizontal_only
    jmp advance_pan
@horizontal_only:
    lda pad
    and #3             ; A single-screen-high map has no vertical extent to pan.
    jmp advance_pan_with_directions
@next_stage:
    ldx prg_bank0
    inx
    cpx #arcade_stage_bank + arcade_stage_count
    bne @open_stage
    ldx #arcade_stage_bank
    jmp @open_stage
@prev_stage:
    ldx prg_bank0
    dex
    cpx #arcade_stage_bank - 1
    bne @open_stage
    ldx #arcade_stage_bank + arcade_stage_count - 1
@open_stage:
    txa
    jmp show_stage_view
.endif

; A = horizontal page, Y = tile-aligned vertical scroll within page zero.
; Populate the HUD table and any rows of the lower table already visible.
draw_pan:
    sta pan_x+1
    sty pan_y
    jsr pan_set_tile_x
    tya
    lsr a
    lsr a
    lsr a
    clc
    adc #pan_page_rows
    sta pan_tile_y
    ; Tile columns are 9-bit: a 14-page stage reaches tile 447.
    lda stage_cp_x_hi
    jsr pan_page_tile
    sta z:checkpoint_tile_x
    stx checkpoint_tile_x_hi
    lda stage_cp_x_lo
    lsr a
    lsr a
    lsr a
    ora z:checkpoint_tile_x
    sec
    sbc #1
    sta z:checkpoint_tile_x
    bcs :+
    dec checkpoint_tile_x_hi
:
    lda stage_goal_x_hi
    jsr pan_page_tile
    sta goal_tile_x
    stx goal_tile_x_hi
    lda stage_goal_x_lo
    lsr a
    lsr a
    lsr a
    ora goal_tile_x
    sta goal_tile_x
    lda stage_goal_feet
    lsr a
    lsr a
    lsr a
    sec
    sbc #2
    sta goal_flag_row
    lda #$81
    sta $e000           ; Horizontal mirroring: upper/lower nametables.
    lda #0
    sta pan_x
    sta pan_ctrl
    ldx pan_y_page
    lda pan_page_tiles,x
    sta pan_need_row
    clc
    adc pan_tile_y
    sta pan_tile_y
    ldx #0
    lda scene
    cmp #4
    bne :+
    ldx #hud_rows
:
    stx pan_top_rows
    lda #31
    sec
    sbc pan_top_rows
    sta pan_col_length
    jsr load_terrain_palette
.ifdef NES_AREA_TRIAL
    jsr check_trial_bank
    bcs :+
    jsr load_trial_background_banked
:
.endif
.ifdef REVG_STAGE1
    ; revg_bg_active: 1=S1, 2=S4, 3=S7 (1-axis); 4=S2, 5=S3, 6=S5, 7=S6 (2-axis).
    lda #0
    sta revg_bg_active
    lda arcade_mode
    beq pan_initial_rows
    ldx campaign_stage
    cpx #8
    bcs pan_initial_rows
    lda @revg_stage_bg_mode,x
    beq pan_initial_rows
    sta revg_bg_active
    cmp #4
    bcs pan_initial_rows
    lda #$80
    sta $e000           ; Vertical mirroring: a 64-column ring (Stages 1, 4 & 7).
    lda #pan_no_update
    sta pan_need_row
    sta pan_need_col
    ldx #0
    jmp call_bank50_bg
@revg_stage_bg_mode:
    .byte 0, 1, 4, 5, 2, 6, 7, 3
.endif
pan_initial_rows:
    jsr prepare_pan_row
    jsr upload_pan_row
    inc pan_need_row
    lda pan_need_row
    cmp pan_tile_y
    bne pan_initial_rows
    lda #pan_no_update
    sta pan_need_row
    sta pan_need_col
    rts

advance_pan:
    lda pad
advance_pan_with_directions:
    sta pan_directions
    jsr prepare_pan
    jmp submit_frame

prepare_pan:
    lda #pan_no_update
    sta pan_need_row
    sta pan_need_col
    jsr pan_move_x
prepare_pan_moved:
    jsr pan_move_y
prepare_pan_after_y:
    lda pan_x+1
    jsr pan_set_tile_x
    lda pan_x
    lsr a
    lsr a
    lsr a
    ora pan_tile_x
    sta pan_tile_x
    lda pan_y
    lsr a
    lsr a
    lsr a
    ldx pan_y_page
    clc
    adc pan_page_tiles,x
    sta pan_tile_y

    ldx pan_need_row
    bmi :+
    clc
    adc pan_row_offset,x
    cpx #1
    bne pan_row_ready
    clc
    adc pan_top_rows     ; The playable viewport starts below the HUD.
pan_row_ready:
    sta pan_need_row
    jsr prepare_pan_row
:
    ldx pan_need_col
    bmi :+
    lda pan_tile_x
    clc
    adc pan_col_offset,x
    sta pan_data_x
    lda pan_tile_x_hi
    adc #0
    sta pan_data_x_hi
    lda pan_data_x
    and #31              ; Only the ring column; bit 7 would read as "no update".
    sta pan_need_col
    jsr prepare_pan_column
:
    rts

.ifdef REVG_STAGE1
; prepare_pan with extra horizontal pixels in bits 6-4 of pan_directions (the arcade
; camera, arcade-player.inc) and extra vertical pixels in pan_y_steps (up to 4 extra,
; so 5 px/frame total matches Arthur's max fall speed). Under 8 px in each axis, so at
; most one new row and one new column per frame.
prepare_pan_steps:
    lda #pan_no_update
    sta pan_need_row
    sta pan_need_col
:
    jsr pan_move_x
    lda pan_directions
    and #$70
    beq :+
    lda pan_directions
    sec
    sbc #$10
    sta pan_directions
    jmp :-
:
    jsr pan_move_y
    lda pan_y_steps
    beq @y_done
:
    jsr pan_move_y
    dec pan_y_steps
    bne :-
@y_done:
    jmp prepare_pan_after_y
.endif

; A = camera X high byte (page). Set pan_tile_x/pan_tile_x_hi to its first tile column.
; Preserves Y.
pan_set_tile_x:
    jsr pan_page_tile
    sta pan_tile_x
    stx pan_tile_x_hi
    rts

; A = page. Return its first tile column: low byte in A, bit 8 in X. Preserves Y.
pan_page_tile:
    pha
    lsr a
    lsr a
    lsr a
    tax
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    rts

upload_pan:
.ifdef REVG_STAGE1
    lda revg_bg_active
    beq :+
    jmp revg_upload
:
.endif
    lda pan_need_row
    bmi :+
    jsr upload_pan_row
:
    lda pan_need_col
    bmi :+
    jsr upload_pan_column
:
    rts

pan_move_x:
    lda pan_directions
    and #3
    cmp #2
    beq pan_left
    cmp #1
    bne pan_x_done
    lda pan_x+1
    cmp stage_cam_x_max_hi
    beq pan_x_done
    inc pan_x
    bne :+
    inc pan_x+1
:
    lda pan_x
    and #7
    cmp #1
    bne pan_x_done
    ldx #0
    stx pan_need_col
    rts
pan_left:
    lda pan_x
    bne :+
    lda pan_x+1
    beq pan_x_done
    dec pan_x+1
:
    dec pan_x
    lda pan_x
    and #7
    cmp #7
    bne pan_x_done
    ldx #1
    stx pan_need_col
pan_x_done:
    rts

pan_move_y:
    lda pan_directions
    and #$0c
    cmp #$08
    beq pan_up
    cmp #$04
    bne pan_y_done
    lda pan_y_page
    cmp stage_cam_y_max_page
    beq pan_y_done
    inc pan_y
    lda pan_y
    cmp #240
    bne :+
    lda #0
    sta pan_y
    inc pan_y_page
:
    lda pan_y
    and #7
    cmp #1
    bne pan_y_done
    ldx #0
    stx pan_need_row
    rts
pan_up:
    lda pan_y
    bne :+
    lda pan_y_page
    beq pan_y_done
    dec pan_y_page
    lda #240
    sta pan_y
:
    dec pan_y
    lda pan_y
    and #7
    cmp #7
    bne pan_y_done
    ldx #1
    stx pan_need_row
pan_y_done:
    rts

; A = world tile row. Return its start address and row within its
; physical nametable. Nametable 0 is $2000 and nametable 1 is $2800.
pan_address_for_row:
@wrap60:
    cmp #pan_page_rows*2
    bcc :+
    sbc #pan_page_rows*2
    bcs @wrap60
:
    ldy #$20
    cmp #pan_page_rows
    bcc :+
    sbc #pan_page_rows
    ldy #$28
:
    sty pan_addr_hi
    sta pan_local_y
    asl a
    asl a
    asl a
    asl a
    asl a
    sta pan_addr_lo
    lda pan_local_y
    lsr a
    lsr a
    lsr a
    ora pan_addr_hi
    sta pan_addr_hi
    rts

prepare_pan_row:
.ifdef REVG_STAGE1
    lda revg_bg_active
    cmp #4
    bcc :+
    ldx #4
    jmp call_bank50_bg
:
.endif
    lda pan_need_row
    jsr pan_address_for_row
    lda pan_addr_hi
    sta pan_row_addr+1
    ora #3
    sta pan_attr_addr+1
    lda pan_addr_lo
    sta pan_row_addr
    lda pan_local_y
    and #$1c
    asl a
    ora #$c0
    sta pan_attr_addr
.ifdef NES_AREA_TRIAL
    jsr check_trial_bank
    bcs :+
    lda #$55          ; All trial terrain uses palette 1. HUD resets its own row.
    bne pan_store_attr
:
.endif
    lda pan_local_y
    and #2
    beq :+
    lda pan_need_row
    sec
    sbc #2
    jmp pan_attr_palette
:
    lda pan_need_row
pan_attr_palette:
    lsr a
    and #3
    tax
    lda terrain_palette_half,x
    sta pan_attrib
    inx
    txa
    and #3
    tax
    lda terrain_palette_half,x
    asl a
    asl a
    asl a
    asl a
    ora pan_attrib
.ifdef NES_AREA_TRIAL
pan_store_attr:
.endif
    sta pan_attr_value

    lda pan_need_row
    sta pan_data_y
    lda pan_tile_x
    sta pan_data_x
    lda pan_tile_x_hi
    sta pan_data_x_hi
    lda pan_x
    and #7
    beq :+
    jsr pan_next_data_x  ; At fine X > 0 only the left clipped tile is hidden.
:
    ; Fill the 32-column ring in two runs, from the first column to 31 and then
    ; from 0, so X is the buffer index without masking. pan_tile preserves X.
    ; Restarts redraw the whole screen this way during forced blank, and must
    ; still reach enable_terrain inside the same vertical blank.
    lda pan_data_x
    and #31
    sta pan_row_start
    tax
pan_row_to_end:
    jsr pan_tile
    sta pan_row_tiles,x
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    inx
    cpx #32
    bne pan_row_to_end
    ldx #0
pan_row_from_start:
    cpx pan_row_start
    beq pan_row_done
    jsr pan_tile
    sta pan_row_tiles,x
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    inx
    bne pan_row_from_start
pan_row_done:
    rts

pan_next_data_x:
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    rts

prepare_pan_column:
.ifdef REVG_STAGE1
    lda revg_bg_active
    beq :+
    ldx #3
    jmp call_bank50_bg
:
.endif
    lda pan_tile_y
    clc
    adc pan_top_rows
    sta pan_data_y
    jsr pan_address_for_row
    lda pan_addr_hi
    sta pan_col_addr+1
    and #$28
    eor #8
    sta pan_col_split_hi
    lda pan_need_col
    and #31
    ora pan_addr_lo
    sta pan_col_addr
    sec
    lda #pan_page_rows
    sbc pan_local_y
    cmp pan_col_length
    bcc :+
    lda pan_col_length
:
    sta pan_col_split_count
    ldx #0               ; prepare_pan set pan_data_x/pan_data_x_hi.
:
    jsr pan_tile
    sta pan_col_tiles,x
    inc pan_data_y
    inx
    cpx pan_col_length
    bne :-
    rts

.ifdef NES_AREA_TRIAL
pan_trial_tile:
    lda pan_data_y
    lsr a
    tay
    lda pan_data_x
    lsr a
    jsr read_map_cell
    asl a
    asl a
    sta pan_attrib
    lda pan_data_x
    lsr a
    lda pan_data_y
    rol a
    and #3
    ora pan_attrib
    tay
    lda trial_metatiles,y
    rts
.endif

; X is the buffer index; Y is scratch.
pan_tile:
    ; Compare the low byte first: it settles almost every tile in one test,
    ; and the first screen must still be drawn within the same frames.
    lda pan_data_x
    cmp goal_tile_x
    bne pan_not_goal
    lda pan_data_x_hi
    cmp goal_tile_x_hi
    bne pan_not_goal_reload
    lda pan_data_y
    sec
    sbc goal_flag_row
    beq pan_goal_flag
    cmp #1
    bne pan_not_goal_reload
    lda #(goal_pattern-glyphs)/16 + 1
    rts
pan_goal_flag:
    lda #(goal_pattern-glyphs)/16
    rts
pan_not_goal_reload:
    lda pan_data_x
pan_not_goal:
    cmp z:checkpoint_tile_x
    bne pan_map_tile
    lda pan_data_x_hi
    cmp checkpoint_tile_x_hi
    bne pan_map_tile_reload
.ifdef NES_AREA_TRIAL
    cmp_trial_bank
    bcs @legacy_checkpoint
    lda stage_cp_feet
    lsr a
    lsr a
    lsr a
    sec
    sbc #1
    cmp pan_data_y
    beq @checkpoint
    bne pan_map_tile_reload
@legacy_checkpoint:
.endif
    ldy pan_data_y
    cpy #23
    bne pan_map_tile_reload
.ifdef NES_AREA_TRIAL
@checkpoint:
.endif
    lda #(checkpoint_pattern-glyphs)/16
    rts
pan_map_tile_reload:
    lda pan_data_x
pan_map_tile:
.ifdef NES_AREA_TRIAL
    cmp_trial_bank
    bcc pan_trial_tile
    lda pan_data_x
.endif
    lsr a
    tay
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    bne @skip_scenery
.endif
    lda pan_data_y
    jsr scenery_tile
    bcc @skip_scenery
    rts
@skip_scenery:
    lda pan_data_y
    lsr a
    tay
    lda pan_data_x_hi    ; Metatile column = 9-bit tile column / 2.
    lsr a
    lda pan_data_x
    ror a
    jsr read_map_cell
    bne :+
    jmp pan_sky
:
.if PRG_ROM_UNITS = 32
    cmp #mt_cross
    bcs @pan_arc_deco
    tay
    lda arcade_mode
    beq @pan_legacy_restore
    lda sd_bg_chr_ptr
    ora sd_bg_chr_ptr+1
    bne @pan_legacy_restore
    tya
    cmp #mt_gravel
    beq @pan_arc_fence
    cmp #mt_ledge
    beq @pan_arc_grass
    cmp #mt_rock
    beq @pan_arc_dirt
    jmp @pan_legacy_mt
@pan_legacy_restore:
    tya
@pan_legacy_mt:
.endif
    cmp #mt_gravel
    beq pan_gravel
    cmp #mt_ledge
    beq @check_ledge_half
.if PRG_ROM_UNITS = 32
    cmp #mt_oneway
    beq @pan_oneway
    cmp #mt_ladder
    beq @pan_ladder
    cmp #mt_hazard
    beq @pan_hazard
.endif
    jmp pan_rock
.if PRG_ROM_UNITS = 32
@pan_arc_grass:
    lda #$40
    bne @pan_arc_quad
@pan_arc_dirt:
    lda #$44
    bne @pan_arc_quad
@pan_arc_fence:
    lda #$48
    bne @pan_arc_quad
@pan_arc_deco:
    ; Real-map metatiles reuse the grass, dirt and tombstone art.
    cmp #mt_floor
    bcc @pan_deco_range
    beq @pan_arc_grass
    cmp #mt_dirt
    beq @pan_arc_dirt
    cmp #mt_floor_top
    bcs @pan_arc_grass
    lda #mt_tomb_bg
@pan_deco_range:
    ; mt_cross(7)..mt_castle(14) -> base tile $4C, $50, ..., $68
    sec
    sbc #4
    asl a
    asl a
    clc
    adc #$40
@pan_arc_quad:
    sta pan_attrib
    lda pan_data_x
    lsr a
    lda pan_data_y
    rol a
    and #3
    ora pan_attrib
    rts
.endif
@check_ledge_half:
    lda pan_data_y
    and #1
    bne pan_rock
    ldy #metatile_ledge
    jmp pan_metatile_tile
.if PRG_ROM_UNITS = 32
@pan_oneway:
    lda pan_data_y
    and #1
    bne pan_sky
    lda #(oneway_pattern-glyphs)/16
    rts
@pan_ladder:
    lda #(ladder_pattern-glyphs)/16
    rts
@pan_hazard:
    lda #(hazard_pattern-glyphs)/16
    rts
.endif
pan_sky:
    ldy #metatile_sky
    beq pan_metatile_tile
pan_gravel:
    ldy #metatile_gravel
    jmp pan_metatile_tile
pan_scenery:
    rts
pan_rock:
    lda pan_data_y
    eor pan_data_x
    ; Diagonal bands make a row error or an attribute seam easy to see.
    and #4
    beq :+
    ldy #metatile_rock_dark
    bne pan_metatile_tile
:
    ldy #metatile_rock_light
pan_metatile_tile:
    sty pan_attrib
    lda pan_data_x
    lsr a               ; Carry holds the left/right bit.
    lda pan_data_y
    rol a               ; Combine the upper/lower and left/right bits.
    and #3
    ora pan_attrib
    tay
    lda terrain_metatiles,y
    rts

upload_pan_row:
    lda pan_ctrl
    sta $2000
    lda pan_row_addr+1
    sta $2006
    lda pan_row_addr
    sta $2006
    ldx #0
:
    lda pan_row_tiles,x
    sta $2007
    lda pan_row_tiles+1,x
    sta $2007
    lda pan_row_tiles+2,x
    sta $2007
    lda pan_row_tiles+3,x
    sta $2007
    inx
    inx
    inx
    inx
    cpx #32
    bne :-
    lda pan_attr_addr+1
    sta $2006
    lda pan_attr_addr
    sta $2006
.ifdef REVG_STAGE1
    lda revg_bg_active
    cmp #4
    bcc @legacy_row_attr
    ldx #0
:
    lda revg2_row_attr,x
    sta $2007
    inx
    cpx #8
    bne :-
    rts
@legacy_row_attr:
.endif
    lda pan_attr_value
    ldx #8
:
    sta $2007
    dex
    bne :-
    rts

upload_pan_column:
    lda pan_ctrl
    ora #4
    sta $2000
    lda pan_col_addr+1
    sta $2006
    lda pan_col_addr
    sta $2006
    ldx #0
:
    lda pan_col_tiles,x
    sta $2007
    inx
    cpx pan_col_split_count
    bne :-
    cpx pan_col_length
    beq pan_column_done
    ldy pan_col_split_hi
    sty $2006
    lda pan_col_addr
    and #31
    cpy #$20
    bne @col_wrap_lo
    ldy pan_ctrl
    beq @col_wrap_lo
    ; Never overwrite fixed HUD rows 0..3 ($2000..$207F) when column wraps into nametable $2000.
    ora #$80                    ; row 4 ($2080 | col)
    sta $2006
    txa
    adc #(hud_rows - 1)         ; C=1 guaranteed by cpy #$20 falling through
    tax
    cpx pan_col_length
    bcc @col_wrap_loop
    rts
@col_wrap_lo:
    sta $2006
@col_wrap_loop:
    lda pan_col_tiles,x
    sta $2007
    inx
    cpx pan_col_length
    bne @col_wrap_loop
pan_column_done:
    rts

apply_pan:
    lda pan_y_page
    and #1
    asl a
    ora pan_ctrl
    sta $2000
    lda pan_x
    sta $2005
    lda pan_y
    sta $2005
    rts

.segment "RODATA"
pan_page_tiles: .byte 0, 30, 60, 90, 120, 150
pan_row_offset: .byte 30, 0
pan_col_offset: .byte 32, 1
