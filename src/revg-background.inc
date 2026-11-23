; Stage 1 background art (REVG_STAGE1): the arcade section 0 tiles, streamed into CHR-RAM.
; tools/convert-revg-bg.py writes revg-section0-bg.inc (banks 19 and up, read through the
; $C000 window). docs/stage-design.md, "背景の絵", explains the layout.
;
; - Vertical mirroring: a 64-column ring over $2000 and $2400. A new column and the
;   attribute bytes of its 32 px pair never touch a visible part of the screen.
; - Each 8x8 tile has a fixed slot ($25-$FF). A tile is resident while its column is
;   within the camera's 17 columns plus two on each side. When the camera enters a
;   16 px column, the tiles that come into that range are queued, and the NMI uploads
;   up to three a frame (one when a nametable column also goes up).
; - Palettes 2 and 3 change to the forest's when the camera reaches the swap column. The
;   columns visible at that moment use only palettes 0 and 1.

revg_chr_per_frame = 3
revg_chr_with_column = 1

.segment "ZEROPAGE"
revg_ptr: .res 2
revg_ptr2: .res 2

.segment "WRAM"
revg_bg_active: .res 1          ; Non-zero while the real Stage 1 is drawn this way
revg_cam_col: .res 1            ; Camera column (16 px) whose tiles are resident or queued
revg_zone: .res 1               ; 1 once the forest palettes are loaded
revg_mcol: .res 1
revg_count: .res 1
revg_target: .res 1             ; Camera column the stream is heading for
revg_tile_lo: .res 1
revg_tile_hi: .res 1
revg_slot: .res 1
revg_q_head: .res 1
revg_q_tail: .res 1
revg_q_lo: .res 256
revg_q_hi: .res 256
; Read by the NMI.
revg_chr_count: .res 1
revg_chr_slot: .res revg_chr_per_frame
revg_chr_data: .res 16 * revg_chr_per_frame
revg_attr_hi: .res 1
revg_attr_lo: .res 1
revg_col_attr: .res 7
revg_pal_pending: .res 1
revg_pal_buf: .res 8

.segment "BANK3"
; Draw the screen around the camera during forced blank (draw_pan): the HUD attribute
; rows and every column that is resident (camera column - 2 to + 18).
revg_bg_draw_screen:
    lda pan_x+1
    asl a
    asl a
    asl a
    asl a
    sta revg_cam_col
    ; Attribute row 0 is the HUD's: palette 0 in both nametables.
    lda #0
    sta $2000                   ; Increment by 1; NMI stays off
    ldx #$23
    jsr revg_clear_hud_attributes
    ldx #$27
    jsr revg_clear_hud_attributes
    ; First tile column = 2 * (camera column - margin), at least 0.
    lda revg_cam_col
    sec
    sbc #revg_bg_margin
    bcs :+
    lda #0
:
    asl a
    sta pan_data_x
    lda #0
    rol a
    sta pan_data_x_hi
    lda #2 * (16 + 1 + 2 * revg_bg_margin)
    sta revg_count
@column:
    lda pan_data_x_hi
    cmp #>(revg_bg_camera_columns * 2 + 30)
    bne :+
    lda pan_data_x
    cmp #<(revg_bg_camera_columns * 2 + 30)
:
    bcs @done                   ; Past the last tile column of the section
    jsr revg_prepare_column
    jsr revg_upload_column
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    dec revg_count
    bne @column
@done:
    lda #0
    sta $2000                   ; Back to increment by 1 for the CHR uploads that follow
    rts

; X = $23 or $27.
revg_clear_hud_attributes:
    stx $2006
    ldy #$c0
    sty $2006
    ldy #8
:
    sta $2007
    dey
    bne :-
    rts

; Load the palettes and every tile resident at the camera column (forced blank, from
; ap_load_stage_visuals). Uploading each camera column's list from 0 leaves each slot
; with the last tile that entered it, which is the one resident now.
revg_bg_load_all:
    lda prg_bank2
    pha
    lda #revg_bg_map_bank
    jsr select_bank2
    lda #0
    sta $2000                   ; Increment by 1 (the columns left it at 32); NMI stays off
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    ldx #0
:
    lda revg_bg_palette,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda #0
    sta revg_zone
    sta revg_pal_pending
    sta revg_chr_count
    sta revg_q_head
    sta revg_q_tail
    lda revg_cam_col
    cmp #revg_bg_swap_column
    bcc :+
    lda #1
    jsr revg_prepare_palette
    jsr revg_write_palette
:
    lda #$ff
    sta revg_mcol               ; Camera column being loaded, from 0
@list:
    inc revg_mcol
    lda revg_mcol
    ldx #0
    jsr revg_enqueue_list
@drain:
    ldx revg_q_head
    cpx revg_q_tail
    beq @next
    jsr revg_dequeue_tile
    sty $2006                   ; Slot address: high byte, then low byte
    stx $2006
    ldy #0
:
    lda (revg_ptr),y
    sta $2007
    iny
    cpy #16
    bne :-
    jmp @drain
@next:
    lda revg_mcol
    cmp revg_cam_col
    bne @list
    pla
    jmp select_bank2

; Each frame after the camera moved (ap_pan_camera): queue the tiles of every camera
; column entered, swap the palettes at the lake, and copy the next tiles for the NMI.
revg_bg_stream:
    lda revg_bg_active
    bne :+
    rts
:
    lda prg_bank2
    pha
    ; Camera column = 16-bit camera X / 16.
    lda pan_x
    lsr a
    lsr a
    lsr a
    lsr a
    sta revg_target
    lda pan_x+1
    asl a
    asl a
    asl a
    asl a
    ora revg_target
    sta revg_target
@step:
    lda revg_cam_col
    cmp revg_target
    beq @steps_done
    bcc @step_right
    ldx #1                      ; Left: the tiles of the column the range now reaches
    jsr revg_enqueue_list
    dec revg_cam_col
    jmp @step
@step_right:
    inc revg_cam_col
    lda revg_cam_col
    ldx #0
    jsr revg_enqueue_list
    jmp @step
@steps_done:
    lda revg_cam_col
    cmp #revg_bg_swap_column
    lda #0
    rol a
    cmp revg_zone
    beq @zone_ready
    jsr revg_prepare_palette
@zone_ready:
    lda #0
    sta revg_chr_count
    ldx #revg_chr_per_frame
    lda pan_need_col
    bmi :+
    ldx #revg_chr_with_column
:
    stx revg_count
@tile:
    ldx revg_q_head
    cpx revg_q_tail
    beq @stream_done
    jsr revg_dequeue_tile
    lda revg_slot
    ldx revg_chr_count
    sta revg_chr_slot,x
    txa
    asl a
    asl a
    asl a
    asl a
    tax
    ldy #0
:
    lda (revg_ptr),y
    sta revg_chr_data,x
    inx
    iny
    cpy #16
    bne :-
    inc revg_chr_count
    dec revg_count
    bne @tile
@stream_done:
    pla
    jmp select_bank2

; A = 0 for the graveyard or 1 for the forest palettes 2 and 3; the NMI writes them.
revg_prepare_palette:
    sta revg_zone
    lda #revg_bg_map_bank
    jsr select_bank2
    ldx #7
    lda revg_zone
    bne @forest
:
    lda revg_bg_palette+8,x
    sta revg_pal_buf,x
    dex
    bpl :-
    bmi @ready
@forest:
:
    lda revg_bg_palette_forest,x
    sta revg_pal_buf,x
    dex
    bpl :-
@ready:
    lda #1
    sta revg_pal_pending
    rts

; Forced blank only: write the prepared palettes 2 and 3.
revg_write_palette:
    lda #$3f
    sta $2006
    lda #8
    sta $2006
    ldx #0
:
    lda revg_pal_buf,x
    sta $2007
    inx
    cpx #8
    bne :-
    lda #0
    sta revg_pal_pending
    rts

; A = camera column, X = 0 for the right list or 1 for the left one. Append its tiles
; to the queue. Leaves the list bank in $C000.
revg_enqueue_list:
    tay
    lda #revg_bg_list_bank
    jsr select_bank2
    cpx #0
    bne @left
    lda revg_bg_right_start_lo+1,y
    sec
    sbc revg_bg_right_start_lo,y
    sta revg_count
    lda revg_bg_right_start_lo,y
    clc
    adc #<revg_bg_right_id_lo
    sta revg_ptr
    lda revg_bg_right_start_hi,y
    adc #>revg_bg_right_id_lo
    sta revg_ptr+1
    lda revg_bg_right_start_lo,y
    clc
    adc #<revg_bg_right_id_hi
    sta revg_ptr2
    lda revg_bg_right_start_hi,y
    adc #>revg_bg_right_id_hi
    sta revg_ptr2+1
    jmp @copy
@left:
    lda revg_bg_left_start_lo+1,y
    sec
    sbc revg_bg_left_start_lo,y
    sta revg_count
    lda revg_bg_left_start_lo,y
    clc
    adc #<revg_bg_left_id_lo
    sta revg_ptr
    lda revg_bg_left_start_hi,y
    adc #>revg_bg_left_id_lo
    sta revg_ptr+1
    lda revg_bg_left_start_lo,y
    clc
    adc #<revg_bg_left_id_hi
    sta revg_ptr2
    lda revg_bg_left_start_hi,y
    adc #>revg_bg_left_id_hi
    sta revg_ptr2+1
@copy:
    ldy #0
    ldx revg_q_tail
@entry:
    cpy revg_count
    beq @copied
    lda (revg_ptr),y
    sta revg_q_lo,x
    lda (revg_ptr2),y
    sta revg_q_hi,x
    inx
    iny
    bne @entry
@copied:
    stx revg_q_tail
    rts

; Take the next queued tile: select its pattern bank, point revg_ptr at its 16 bytes,
; and return its slot in revg_slot and the slot's PPU address (Y high, X low).
revg_dequeue_tile:
    lda revg_q_lo,x
    sta revg_tile_lo
    lda revg_q_hi,x
    sta revg_tile_hi
    inx
    stx revg_q_head
    lda revg_tile_hi
    clc
    adc #revg_bg_pattern_bank
    jsr select_bank2
    ; Pattern at $C010 + 16 * id, slot at revg_bg_slots + id.
    lda revg_tile_lo
    lsr a
    lsr a
    lsr a
    lsr a
    sta revg_ptr+1
    lda revg_tile_lo
    asl a
    asl a
    asl a
    asl a
    clc
    adc #<revg_bg_patterns
    sta revg_ptr
    lda revg_ptr+1
    adc #>revg_bg_patterns
    sta revg_ptr+1
    ldx revg_tile_lo
    lda revg_bg_slots,x
    sta revg_slot
    pha
    asl a
    asl a
    asl a
    asl a
    tax                         ; Slot * 16, low byte
    pla
    lsr a
    lsr a
    lsr a
    lsr a
    tay                         ; High byte (pattern table 0)
    rts

; Fill pan_col_tiles and the attribute bytes for tile column pan_data_x (9 bits), and
; point pan_col_addr at its rows 4-29 in the 64-column ring.
revg_prepare_column:
    lda pan_data_x_hi
    lsr a
    lda pan_data_x
    ror a
    sta revg_mcol
    lda pan_data_x
    and #$20
    lsr a
    lsr a
    lsr a
    ora #$20
    sta pan_col_addr+1
    ora #3
    sta revg_attr_hi
    lda pan_data_x
    and #31
    pha
    ora #$80                    ; Row 4
    sta pan_col_addr
    pla
    lsr a
    lsr a
    ora #$c8                    ; Attribute row 1
    sta revg_attr_lo
    lda #revg_bg_view_rows * 2
    sta pan_col_split_count
    sta pan_col_length
    lda prg_bank2
    pha
    lda #revg_bg_map_bank
    jsr select_bank2
    ldy revg_mcol
    lda pan_data_x
    lsr a
    bcc :+
    jmp @right
:
.repeat revg_bg_view_rows, row
    ldx .ident(.sprintf("revg_bg_map_row%d", row)),y
    lda revg_bg_mt_tl,x
    sta pan_col_tiles + 2 * row
    lda revg_bg_mt_bl,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
    jmp @attributes
@right:
.repeat revg_bg_view_rows, row
    ldx .ident(.sprintf("revg_bg_map_row%d", row)),y
    lda revg_bg_mt_tr,x
    sta pan_col_tiles + 2 * row
    lda revg_bg_mt_br,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
@attributes:
    tya
    lsr a
    tay
.repeat 7, row
    lda .ident(.sprintf("revg_bg_attr_row%d", row)),y
    sta revg_col_attr + row
.endrepeat
    pla
    jmp select_bank2

.segment "CODE"
; Fixed bank: the NMI part (from upload_pan), also used by revg_bg_draw_screen.
revg_upload:
    lda pan_ctrl                ; Increment by 1
    sta $2000
    jsr revg_spr_upload
    lda revg_chr_count
    bne :+
    jmp @palette
:
    lda pan_ctrl                ; Increment by 1
    sta $2000
    ldy #0
    ldx #0
@tile:
    lda revg_chr_slot,y
    pha
    lsr a
    lsr a
    lsr a
    lsr a
    sta $2006
    pla
    asl a
    asl a
    asl a
    asl a
    sta $2006
.repeat 16
    lda revg_chr_data,x
    sta $2007
    inx
.endrepeat
    iny
    cpy revg_chr_count
    beq :+
    jmp @tile
:
    lda #0
    sta revg_chr_count
@palette:
    lda revg_pal_pending
    beq @column
    lda #$3f
    sta $2006
    lda #8
    sta $2006
    ldx #0
:
    lda revg_pal_buf,x
    sta $2007
    inx
    cpx #8
    bne :-
    lda #0
    sta revg_pal_pending
@column:
    lda pan_need_col
    bmi revg_upload_done
revg_upload_column:
    jsr upload_pan_column
    lda revg_attr_lo
    ldx #0
:
    ldy revg_attr_hi
    sty $2006
    sta $2006
    ldy revg_col_attr,x
    sty $2007
    clc
    adc #8
    inx
    cpx #7
    bne :-
revg_upload_done:
    rts
