; Horizontal mirroring gives this camera a 32x60 tile ring. The PPU
; masks the leftmost eight pixels so a replaced column is offscreen.
pan_page_rows = 30
pan_x_limit_high = 2           ; 512 px of travel across a 768 px map
pan_y_limit_page = 2           ; 480 px of travel across a 720 px map
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

.segment "BSS"
pan_row_tiles: .res 32
pan_col_tiles: .res 31

.segment "CODE"
show_pan:
    jsr blank_display
    lda #3
    sta scene
    lda #0
    ldy #0
    jsr draw_pan
    jsr enable_screen
    lda #$08            ; BG on, left eight pixels masked.
    sta $2001
    lda #0
    sta terrain_frame
    lda #$80
    sta pan_ctrl
    jmp apply_pan

; A = horizontal page, Y = tile-aligned vertical scroll within page zero.
; Populate the HUD table and any rows of the lower table already visible.
draw_pan:
    sta pan_x+1
    sty pan_y
    asl a
    asl a
    asl a
    asl a
    asl a
    sta pan_tile_x
    tya
    lsr a
    lsr a
    lsr a
    clc
    adc #pan_page_rows
    sta pan_tile_y
    lda #$81
    sta $e000           ; Horizontal mirroring: upper/lower nametables.
    lda #0
    sta pan_x
    sta pan_y_page
    sta pan_need_row
    sta pan_ctrl
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
pan_initial_rows:
    jsr prepare_pan_row
    jsr upload_pan_row
    inc pan_need_row
    lda pan_need_row
    cmp pan_tile_y
    bne pan_initial_rows
    rts

advance_pan:
    lda pad
    sta pan_directions
    jsr prepare_pan
    jmp submit_frame

prepare_pan:
    lda #pan_no_update
    sta pan_need_row
    sta pan_need_col
    jsr pan_move_x
    jsr pan_move_y
    lda pan_x+1
    asl a
    asl a
    asl a
    asl a
    asl a
    sta pan_tile_x
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
    sta pan_need_col
    jsr prepare_pan_column
:
    rts

upload_pan:
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
    cmp #pan_x_limit_high
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
    cmp #pan_y_limit_page
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
    cmp #pan_page_rows*2
    bcc :+
    sbc #pan_page_rows*2
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
    sta pan_attr_value

    lda pan_need_row
    sta pan_data_y
    lda pan_tile_x
    sta pan_data_x
    lda pan_x
    and #7
    beq :+
    inc pan_data_x       ; At fine X > 0 only the left clipped tile is hidden.
:
    ldx #32
pan_row_data:
    jsr pan_tile
    pha
    lda pan_data_x
    and #31
    tay
    pla
    sta pan_row_tiles,y
    inc pan_data_x
    dex
    bne pan_row_data
    rts

prepare_pan_column:
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
    lda pan_need_col
    sta pan_data_x
    ldx #0
:
    jsr pan_tile
    sta pan_col_tiles,x
    inc pan_data_y
    inx
    cpx pan_col_length
    bne :-
    rts

; Use the same cliff profile and pattern tiles as the horizontal terrain.
; X is the buffer index; Y is scratch.
pan_tile:
    lda pan_data_x
    cmp #goal_x / 8
    bne :+
    ldy pan_data_y
    cpy #(goal_feet / 8) - 2
    beq pan_goal_flag
    cpy #(goal_feet / 8) - 1
    bne :+
    lda #(goal_pattern-glyphs)/16 + 1
    rts
pan_goal_flag:
    lda #(goal_pattern-glyphs)/16
    rts
:
    cmp #(checkpoint_x / 8) - 1
    bne :+
    ldy pan_data_y
    cpy #23
    bne :+
    lda #(checkpoint_pattern-glyphs)/16
    rts
:
    lsr a
    tay
    lda pan_data_y
    jsr scenery_tile
    bcs pan_scenery
    lda terrain_columns,y
    and #cliff_height_mask
    asl a
    cmp pan_data_y
    beq pan_ledge
    bcc pan_rock
    sbc #1              ; The sky tile immediately above the ledge has gravel.
    cmp pan_data_y
    beq pan_gravel
    ldy #metatile_sky
    beq pan_metatile_tile
pan_gravel:
    ldy #metatile_gravel
    jmp pan_metatile_tile
pan_scenery:
    rts
pan_ledge:
    ldy #metatile_ledge
    jmp pan_metatile_tile
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
    inx
    cpx #32
    bne :-
    lda pan_attr_addr+1
    sta $2006
    lda pan_attr_addr
    sta $2006
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
    lda pan_col_split_hi
    sta $2006
    lda pan_col_addr
    and #31
    sta $2006
:
    lda pan_col_tiles,x
    sta $2007
    inx
    cpx pan_col_length
    bne :-
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
pan_page_tiles: .byte 0, 30, 60
pan_row_offset: .byte 30, 0
pan_col_offset: .byte 32, 1
