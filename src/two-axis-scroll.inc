; Horizontal mirroring gives this camera a 32x60 tile ring. The PPU
; masks the leftmost eight pixels so a replaced column is offscreen.
pan_page_rows = 30
pan_x_limit_high = 2           ; 512 px of travel across a 768 px map
pan_y_limit_page = 2           ; 480 px of travel across a 720 px map
pan_no_update = $ff

.segment "ZEROPAGE"
pan_x: .res 2
pan_y: .res 1                 ; PPU Y within a 240 px nametable
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

.segment "BSS"
pan_row_tiles: .res 32
pan_col_tiles: .res 31

.segment "CODE"
show_pan:
    jsr blank_display
    lda #3
    sta scene
    lda #$81
    sta $e000           ; Horizontal mirroring: upper/lower nametables.
    lda #0
    sta pan_x
    sta pan_x+1
    sta pan_y
    sta pan_y_page
    sta pan_tile_x
    sta pan_need_row
    sta pan_ctrl
    jsr load_terrain_palette
pan_initial_rows:
    jsr prepare_pan_row
    jsr upload_pan_row
    inc pan_need_row
    lda pan_need_row
    cmp #pan_page_rows
    bne pan_initial_rows
    jsr enable_screen
    lda #$08            ; BG on, left eight pixels masked.
    sta $2001
    lda #0
    sta terrain_frame
    lda #$80
    sta pan_ctrl
    jmp apply_pan

advance_pan:
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
    jsr wait_terrain_frame
    lda pan_need_row
    bmi :+
    jsr upload_pan_row
:
    lda pan_need_col
    bmi :+
    jsr upload_pan_column
:
    jmp apply_pan

pan_move_x:
    lda pad
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
    lda pad
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
    lda pan_palette_pairs,x
    sta pan_attrib
    inx
    txa
    and #3
    tax
    lda pan_palette_pairs,x
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
    sta pan_col_split_count
    lda pan_need_col
    sta pan_data_x
    lda pan_tile_y
    sta pan_data_y
    ldx #0
:
    jsr pan_tile
    sta pan_col_tiles,x
    inc pan_data_y
    inx
    cpx #31
    bne :-
    rts

; Use the same cliff profile and pattern tiles as the horizontal terrain.
; X is the buffer index; Y is scratch.
pan_tile:
    lda pan_data_x
    lsr a
    tay
    lda terrain_heights,y
    asl a
    cmp pan_data_y
    beq pan_ledge
    bcc pan_rock
    sbc #1              ; The sky tile immediately above the ledge has gravel.
    cmp pan_data_y
    beq pan_gravel
    lda #0
    rts
pan_gravel:
    lda #(terrain_tiles-glyphs)/16
    rts
pan_ledge:
    lda #(terrain_tiles-glyphs)/16+1
    rts
pan_rock:
    lda pan_data_y
    eor pan_data_x
    ; Diagonal bands make a row error or an attribute seam easy to see.
    and #4
    beq :+
    lda #(terrain_tiles-glyphs)/16+3
    rts
:
    lda #(terrain_tiles-glyphs)/16+2
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
    lda pan_col_split_hi
    sta $2006
    lda pan_col_addr
    and #31
    sta $2006
:
    lda pan_col_tiles,x
    sta $2007
    inx
    cpx #31
    bne :-
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
pan_palette_pairs: .byte $00, $05, $0a, $0f
