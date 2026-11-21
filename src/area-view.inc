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

.ifdef NES_AREA_TRIAL
trial_metatiles: .res 7 * 4
trial_offsets: .res 7 * 2
.if nes_area_trial_player
trial_pose_size = 1 + 2 * 4
trial_idle = 0
trial_walk_a = trial_pose_size
trial_walk_b = 2 * trial_pose_size
trial_jump = 3 * trial_pose_size
trial_player_addresses: .res 4 * 2
trial_player_poses: .res 4 * trial_pose_size
trial_player_last_x: .res 1
trial_player_steps: .res 1
.endif
emit_area_trial
.endif

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
    jsr upload_area_chr
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

; Preserve X and return the palette pointer in area_data.
upload_area_chr:
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
    rts

.ifdef NES_AREA_TRIAL
; Called with the course bank mapped and rendering off. Keep the HUD and
; goal/checkpoint patterns from the normal background table.
load_trial_background:
    ldx #13
:
    lda trial_region_offsets,x
    sta trial_offsets,x
    dex
    bpl :-
    lda trial_area_index
    sta area_index
    jsr read_area_entry
    jsr load_menu_patterns
    ldx #0
    jsr upload_area_chr
    ldx #0
    ldy #0
@region:
    clc
    lda area_entry+area_regions
    adc trial_offsets,x
    sta area_row
    lda area_entry+area_regions+1
    adc trial_offsets+1,x
    sta area_row+1
    ; Copy the four 8x8 quadrants, preserving the map's collision IDs.
    txa
    asl a
    tax
    .repeat 4, quadrant
        lda (area_row),y
        sta trial_metatiles+quadrant,x
        iny
    .endrepeat
    ldy #0
    txa
    lsr a
    tax
    inx
    inx
    cpx #14
    bne @region
    ldy #4              ; The build verifies that every region uses this palette.
    lda (area_row),y
    asl a
    asl a
    tay
    lda #$3f
    sta $2006
    lda #$05            ; Palette 1 colors 1-3. Keep HUD palette 0 intact.
    sta $2006
    .repeat 3
        iny
        lda (area_data),y
        sta $2007
    .endrepeat
    lda #nes_area_trial_bank
    jmp select_bank0

.if nes_area_trial_player
; BG loading has already cached both area descriptors. Preserve the
; encounter's patterns and palettes, and cache the four player poses.
load_trial_player:
    lda player_x
    sta trial_player_last_x
    lda #0
    sta trial_player_steps
    ldx #7
:
    lda trial_player_pose_addresses,x
    sta trial_player_addresses,x
    dex
    bpl :-
    ldx #area_record_size
    jsr upload_area_chr
    ldx #0
    ldy #0
@pose:
    lda trial_player_addresses,y
    sta area_row
    iny
    lda trial_player_addresses,y
    sta area_row+1
    iny
    sty area_parts_left
    ldy #0
    lda (area_row),y       ; One or two parts, verified by the build.
    asl a
    asl a
    tay
    stx area_x
    ; Advance to the last byte of this record before copying backwards.
    tya
    clc
    adc area_x
    tax
@copy:
    lda (area_row),y
    sta trial_player_poses,x
    dex
    dey
    bpl @copy
    txa
    clc
    adc #trial_pose_size+1
    tax
    ldy area_parts_left
    cpy #8
    bne @pose

    lda trial_player_poses+4
    and #3
    asl a
    asl a
    tay
    lda #$3f
    sta $2006
    lda #$11              ; Player palette 0 colors 1–3.
    sta $2006
    .repeat 3
        iny
        lda (area_data),y
        sta $2007
    .endrepeat
    lda #nes_area_trial_bank
    jmp select_bank0

.segment "BANK52"
; Draw from RAM with the course map still mapped. Poses are anchored at
; (player_x, player_feet). Left-facing poses reflect around player_x.
draw_trial_player:
    ldy #trial_idle
    lda player_hit
    bne @rest
    lda jump_step
    bmi @ground
    ldy #trial_jump
    bne @rest
@ground:
    lda player_x
    cmp trial_player_last_x
    beq @rest
    inc trial_player_steps
    ldy #trial_walk_a
    lda trial_player_steps
    and #8
    beq @draw
    ldy #trial_walk_b
    bne @draw
@rest:
    lda #0
    sta trial_player_steps
@draw:
    lda player_x
    sta trial_player_last_x
    lda trial_player_poses,y
    sta area_parts_left
    iny
@part:
    lda player_x
    ldx player_x+1
    jsr set_sprite_x
    lda trial_player_poses,y
    ldx player_left
    beq :+
    eor #$ff
    sec
    sbc #7                ; Mirrored left edge is -dx-8.
:
    ldx #0
    cmp #$80
    bcc :+
    dex
:
    clc
    adc draw_x
    sta draw_x
    txa
    adc draw_x+1
    sta draw_x+1
    iny
    sec
    lda player_feet
    sbc pan_y
    bcc @skip
    beq @skip
    ; OAM stores screen Y minus one. Carry is set after subtracting pan_y.
    sbc #1
    clc
    adc trial_player_poses,y
    bcc @skip
    sta draw_y
    lda trial_player_poses+1,y
    sta draw_tile
    lda trial_player_poses+2,y
    and #$c0              ; Preserve flips, select player palette 0.
    ldx player_left
    beq :+
    eor #$40
:
    ldx player_hit
    beq :+
    ora #2                ; Keep the trial course's hit tint.
:
    sta draw_palette
    jsr emit_sprite
    jmp @next
@skip:
    ldx draw_slot
    jsr next_sprite
@next:
    iny
    iny
    iny
    dec area_parts_left
    bne @part
    rts

.segment "BANK51"
.endif
.endif

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
.ifdef NES_AREA_TRIAL
area_trial_menu:
    .byte $0c, $06, $07, $13, $17, $03, $00, $02, $11, $06, $02, $00, $13, $11, $0a, $02, $0c ; LEFT+B AREA TRIAL
area_trial_menu_length = * - area_trial_menu
.endif
