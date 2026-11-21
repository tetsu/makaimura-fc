; Optional area-manifest viewer. Code stays in the $A000 window while
; the directory and each payload take turns in the $8000 window.
area_bank = 0
area_chr_vram = 1
area_chr_size = 3
area_chr_address = 5
area_palette = 7
area_columns = 9
area_rows = 11
area_regions = 13
area_poses = 15
area_pose_count = 17
area_record_size = 18
area_entry_size = 2 * area_record_size

.segment "ZEROPAGE"
area_data: .res 2
area_row: .res 2
area_nt: .res 2

.segment "BSS"
area_entry: .res area_entry_size
area_index: .res 1
area_count: .res 1
area_pose: .res 1
area_stride: .res 2
area_width: .res 1
area_height: .res 1
area_x: .res 1
area_y: .res 1
area_source_x: .res 1
area_source_y: .res 1
area_attributes: .res 64
area_attr_index: .res 1
area_parts_left: .res 1

.segment "BANK51"
    .byte bank_tag_base + nes_area_viewer_bank

show_area_view:
    jsr silence_music
    lda #area_view_scene
    sta scene
    lda #0
    sta area_index
load_area_view:
    jsr blank_display
    bit $2002
    lda prg_bank0
    pha
    jsr read_area_entry
    lda #0
    sta area_pose
    ldx #area_record_size
    jsr upload_area_art     ; OBJ first, since the four slot-zero colors alias BG.
    ldx #0
    jsr upload_area_art
    jsr draw_area_background
    jsr draw_area_pose
    pla
    jsr select_bank0
    jsr upload_sprites
    jsr enable_static_screen
    lda #$a0
    sta $2000
    lda #$1e
    sta $2001
    rts

update_area_view:
    lda pressed
    and #$03
    beq @pose
    ldx area_index
    ldy area_count
    jsr step_area_selection
    cpx area_index
    beq @idle
    stx area_index
    jmp load_area_view
@pose:
    lda pressed
    and #$0c
    beq @idle
    and #$08
    beq :+
    lda #1                 ; Up selects the next pose.
    bne @step_pose
:
    lda #2
@step_pose:
    ldx area_pose
    ldy area_entry+area_record_size+area_pose_count
    jsr step_area_selection
    cpx area_pose
    beq @idle
    stx area_pose
    lda prg_bank0
    pha
    jsr draw_area_pose
    pla
    jsr select_bank0
    jmp submit_frame
@idle:
    jmp wait_terrain_frame

; X = current index, Y = number of entries. A = 1 forward, 2 backward.
step_area_selection:
    cmp #2
    beq @back
    inx
    stx area_x
    cpy area_x
    bne @done
    ldx #0
    rts
@back:
    dex
    cpx #$ff
    bne @done
    tya
    tax
    dex
@done:
    rts

read_area_entry:
    lda #nes_area_directory_bank
    jsr select_bank0
    lda nes_area_count
    sta area_count
    lda #<nes_area_table
    sta area_data
    lda #>nes_area_table
    sta area_data+1
    ldx area_index
    beq @copy
@seek:
    clc
    lda area_data
    adc #area_entry_size
    sta area_data
    bcc :+
    inc area_data+1
:
    dex
    bne @seek
@copy:
    ldy #area_entry_size - 1
:
    lda (area_data),y
    sta area_entry,y
    dey
    bpl :-
    rts

; X selects the cached BG or OBJ descriptor. Rendering must be off.
; The selected data bank remains mapped for the caller.
upload_area_art:
    lda area_entry+area_bank,x
    jsr select_bank0
    txa
    pha
    lda area_entry+area_chr_vram+1,x
    sta $2006
    lda area_entry+area_chr_vram,x
    sta $2006
    lda area_entry+area_chr_address,x
    sta chr_source
    lda area_entry+area_chr_address+1,x
    sta chr_source+1
    lda area_entry+area_chr_size,x
    pha
    lda area_entry+area_chr_size+1,x
    tax
    pla
    jsr upload_chr_patterns
    pla
    tax
    lda area_entry+area_palette,x
    sta area_data
    lda area_entry+area_palette+1,x
    sta area_data+1
    lda #$3f
    sta $2006
    lda #0
    cpx #0
    beq :+
    lda #$10
:
    sta $2006
    ldy #0
:
    lda (area_data),y
    sta $2007
    iny
    cpy #16
    bne :-
    rts

; Draw one nametable. Large sheets are cropped, small sheets repeat.
; Each region is a 16x16 group of four tiles and a palette index.
draw_area_background:
    lda area_entry+area_columns
    asl
    sta area_stride
    lda area_entry+area_columns+1
    rol
    asl area_stride
    rol
    sta area_stride+1
    clc
    lda area_stride
    adc area_entry+area_columns
    sta area_stride
    lda area_stride+1
    adc area_entry+area_columns+1
    sta area_stride+1
    lda area_entry+area_columns+1
    bne @wide
    lda area_entry+area_columns
    cmp #16
    bcc @width
@wide:
    lda #16
@width:
    sta area_width
    lda area_entry+area_rows+1
    bne @tall
    lda area_entry+area_rows
    cmp #15
    bcc @height
@tall:
    lda #15
@height:
    sta area_height
    lda #0
    sta area_y
    sta area_source_y
    sta area_nt
    ldx #63
:
    sta area_attributes,x
    dex
    bpl :-
    lda #$20
    sta area_nt+1
@first_row:
    lda area_entry+area_regions
    sta area_row
    lda area_entry+area_regions+1
    sta area_row+1
@row:
    lda #0
    sta area_x
    sta area_source_x
@first_column:
    lda area_row
    sta area_data
    lda area_row+1
    sta area_data+1
@column:
    jsr draw_area_region
    inc area_x
    lda area_x
    cmp #16
    beq @next_row
    inc area_source_x
    lda area_source_x
    cmp area_width
    bne :+
    lda #0
    sta area_source_x
    jmp @first_column
:
    clc
    lda area_data
    adc #5
    sta area_data
    bcc @column
    inc area_data+1
    jmp @column
@next_row:
    inc area_y
    lda area_y
    cmp #15
    beq @attributes
    clc
    lda area_nt
    adc #64
    sta area_nt
    bcc :+
    inc area_nt+1
:
    inc area_source_y
    lda area_source_y
    cmp area_height
    bne :+
    lda #0
    sta area_source_y
    jmp @first_row
:
    clc
    lda area_row
    adc area_stride
    sta area_row
    lda area_row+1
    adc area_stride+1
    sta area_row+1
    jmp @row
@attributes:
    lda #$23
    sta $2006
    lda #$c0
    sta $2006
    ldx #0
:
    lda area_attributes,x
    sta $2007
    inx
    cpx #64
    bne :-
    rts

draw_area_region:
    lda area_nt+1
    sta $2006
    lda area_x
    asl
    ora area_nt
    tax
    stx $2006
    ldy #0
    lda (area_data),y
    sta $2007
    iny
    lda (area_data),y
    sta $2007
    lda area_y
    cmp #14
    beq @palette           ; Last row only has space for the top two tiles.
    lda area_nt+1
    sta $2006
    txa
    ora #$20
    sta $2006
    iny
    lda (area_data),y
    sta $2007
    iny
    lda (area_data),y
    sta $2007
@palette:
    lda area_x
    lsr
    sta area_attr_index
    lda area_y
    and #$fe
    asl
    asl
    ora area_attr_index
    sta area_attr_index
    lda area_x
    lsr
    lda area_y
    and #1
    rol
    asl
    tax
    ldy #4
    lda (area_data),y
    cpx #0
    beq @store
:
    asl
    dex
    bne :-
@store:
    ldx area_attr_index
    ora area_attributes,x
    sta area_attributes,x
    rts

draw_area_pose:
    lda area_entry+area_record_size+area_bank
    jsr select_bank0
    lda area_pose
    asl
    sta area_data
    lda #0
    rol
    sta area_data+1
    clc
    lda area_data
    adc area_entry+area_record_size+area_poses
    sta area_data
    lda area_data+1
    adc area_entry+area_record_size+area_poses+1
    sta area_data+1
    ldy #0
    lda (area_data),y
    tax
    iny
    lda (area_data),y
    sta area_data+1
    stx area_data
    lda #$f8
    ldx #0
:
    sta sprites,x
    inx
    bne :-
    dey
    lda (area_data),y
    beq @done
    sta area_parts_left
    inc area_data          ; Index 64 parts from byte zero through byte 255.
    bne @part
    inc area_data+1
@part:
    lda (area_data),y      ; Anchor at (128, 128) with signed dx and dy.
    eor #$80
    sta sprites+3,x
    iny
    lda (area_data),y
    eor #$80
    sec
    sbc #1                 ; OAM stores screen Y minus one.
    cmp #239
    bcs @hide
    sta sprites,x
@hide:
    iny
    lda (area_data),y
    sta sprites+1,x
    iny
    lda (area_data),y
    sta sprites+2,x
    iny
    inx
    inx
    inx
    inx
    dec area_parts_left
    bne @part
@done:
    rts

.segment "RODATA"
area_view_menu:
    .byte $0c, $06, $07, $13, $17, $02, $00, $02, $11, $06, $02, $00, $15, $0a, $06, $16 ; LEFT+A AREA VIEW
area_view_menu_length = * - area_view_menu
