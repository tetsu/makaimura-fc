; Stage 1 & Stage 2 background art (REVG_STAGE1): arcade tiles streamed into CHR-RAM.
; tools/convert-revg-bg.py writes revg-section0-bg.inc (banks 19-23) and
; revg-section1-bg.inc (banks 31-37), read through the $C000 window from BANK50 ($A000).
;
; - revg_bg_active = 1 (Stage 1): vertical mirroring ($80), 64-column ring over $2000/$2400.
; - revg_bg_active = 2 (Stage 2): horizontal mirroring ($81), 32x60 tile ring over $2000/$2800.
; - Each 8x8 tile has a fixed slot ($25-$FF), resident while its column is within
;   [cam_col - margin, cam_col + 17 + margin]. NMI uploads up to 3 tiles/frame (1 with column).

revg_chr_per_frame = 3
revg_chr_with_column = 1

.segment "ZEROPAGE"
revg_ptr: .res 2
revg_ptr2: .res 2

.segment "WRAM"
revg_bg_active: .res 1          ; 0 = legacy, 1 = Stage 1 (1-axis), 2 = Stage 2 (2-axis)
revg_cam_col: .res 1            ; Camera column (16 px) whose tiles are resident or queued
revg_zone: .res 1               ; 1 once zone 2 palettes are loaded
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
revg_col_attr: .res 8
revg_pal_pending: .res 1
revg_pal_buf: .res 8
revg_fade_pending: .res 1       ; 1: the NMI writes revg_fade_buf to $3F00-$3F0F (the opening)
revg_fade_buf: .res 16
revg2_row_attr: .res 8
revg2_attr_split: .res 1
revg2_attr_hi2: .res 1
revg2_attr_lo2: .res 1
revg2_tmp0: .res 1
revg2_tmp1: .res 1
revg2_tmp2: .res 1
revg2_tmp3: .res 1
revg_nmi_cnt: .res 1

.segment "BANK50"
; Per-mode lookup tables indexed by revg_bg_active:
;   1=S1, 2=S4, 3=S7 (1-axis); 4=S2, 5=S3 (2-axis multi-bank); 6=S5, 7=S6 (2-axis 5p packed).
revg_bg_map_bank_tbl:
    .byte 0, revg_bg_map_bank, revg4_bg_map_bank, revg7_bg_map_bank, revg2_bg_map_bank, revg3_bg_map_bank, revg5_bg_map_bank, revg6_bg_map_bank
revg_bg_list_bank_tbl:
    .byte 0, revg_bg_list_bank, revg4_bg_list_bank, revg7_bg_list_bank, revg2_bg_list_bank, revg3_bg_list_bank, revg5_bg_list_bank, revg6_bg_list_bank
revg_bg_pat_bank_tbl:
    .byte 0, revg_bg_pattern_bank, revg4_bg_pattern_bank, revg7_bg_pattern_bank, revg2_bg_pattern_bank, revg3_bg_pattern_bank, revg5_bg_pattern_bank, revg6_bg_pattern_bank
revg_bg_swap_col_tbl:
    .byte 0, revg_bg_swap_column, revg4_bg_swap_column, revg7_bg_swap_column, revg2_bg_swap_column, revg3_bg_swap_column, revg5_bg_swap_column, revg6_bg_swap_column
revg_bg_pal_lo_tbl:
    .byte 0, <revg_bg_palette, <revg4_bg_palette, <revg7_bg_palette, <revg2_bg_palette, <revg3_bg_palette, <revg5_bg_palette, <revg6_bg_palette
revg_bg_pal_hi_tbl:
    .byte 0, >revg_bg_palette, >revg4_bg_palette, >revg7_bg_palette, >revg2_bg_palette, >revg3_bg_palette, >revg5_bg_palette, >revg6_bg_palette
revg_bg_pal2_lo_tbl:
    .byte 0, <revg_bg_palette_forest, <revg4_bg_palette_forest, <revg7_bg_palette_forest, <revg2_bg_palette_zone2, <revg3_bg_palette_zone2, <revg5_bg_palette_zone2, <revg6_bg_palette_zone2
revg_bg_pal2_hi_tbl:
    .byte 0, >revg_bg_palette_forest, >revg4_bg_palette_forest, >revg7_bg_palette_forest, >revg2_bg_palette_zone2, >revg3_bg_palette_zone2, >revg5_bg_palette_zone2, >revg6_bg_palette_zone2
revg_bg_r_slo_lo:
    .byte 0, <revg_bg_right_start_lo, <revg4_bg_right_start_lo, <revg7_bg_right_start_lo, <revg2_bg_right_start_lo, <revg3_bg_right_start_lo, <revg5_bg_right_start_lo, <revg6_bg_right_start_lo
revg_bg_r_slo_hi:
    .byte 0, >revg_bg_right_start_lo, >revg4_bg_right_start_lo, >revg7_bg_right_start_lo, >revg2_bg_right_start_lo, >revg3_bg_right_start_lo, >revg5_bg_right_start_lo, >revg6_bg_right_start_lo
revg_bg_r_shi_lo:
    .byte 0, <revg_bg_right_start_hi, <revg4_bg_right_start_hi, <revg7_bg_right_start_hi, <revg2_bg_right_start_hi, <revg3_bg_right_start_hi, <revg5_bg_right_start_hi, <revg6_bg_right_start_hi
revg_bg_r_shi_hi:
    .byte 0, >revg_bg_right_start_hi, >revg4_bg_right_start_hi, >revg7_bg_right_start_hi, >revg2_bg_right_start_hi, >revg3_bg_right_start_hi, >revg5_bg_right_start_hi, >revg6_bg_right_start_hi
revg_bg_r_ilo_lo:
    .byte 0, <revg_bg_right_id_lo, <revg4_bg_right_id_lo, <revg7_bg_right_id_lo, <revg2_bg_right_id_lo, <revg3_bg_right_id_lo, <revg5_bg_right_id_lo, <revg6_bg_right_id_lo
revg_bg_r_ilo_hi:
    .byte 0, >revg_bg_right_id_lo, >revg4_bg_right_id_lo, >revg7_bg_right_id_lo, >revg2_bg_right_id_lo, >revg3_bg_right_id_lo, >revg5_bg_right_id_lo, >revg6_bg_right_id_lo
revg_bg_r_ihi_lo:
    .byte 0, <revg_bg_right_id_hi, <revg4_bg_right_id_hi, <revg7_bg_right_id_hi, <revg2_bg_right_id_hi, <revg3_bg_right_id_hi, <revg5_bg_right_id_hi, <revg6_bg_right_id_hi
revg_bg_r_ihi_hi:
    .byte 0, >revg_bg_right_id_hi, >revg4_bg_right_id_hi, >revg7_bg_right_id_hi, >revg2_bg_right_id_hi, >revg3_bg_right_id_hi, >revg5_bg_right_id_hi, >revg6_bg_right_id_hi
revg_bg_l_slo_lo:
    .byte 0, <revg_bg_left_start_lo, <revg4_bg_left_start_lo, <revg7_bg_left_start_lo, <revg2_bg_left_start_lo, <revg3_bg_left_start_lo, <revg5_bg_left_start_lo, <revg6_bg_left_start_lo
revg_bg_l_slo_hi:
    .byte 0, >revg_bg_left_start_lo, >revg4_bg_left_start_lo, >revg7_bg_left_start_lo, >revg2_bg_left_start_lo, >revg3_bg_left_start_lo, >revg5_bg_left_start_lo, >revg6_bg_left_start_lo
revg_bg_l_shi_lo:
    .byte 0, <revg_bg_left_start_hi, <revg4_bg_left_start_hi, <revg7_bg_left_start_hi, <revg2_bg_left_start_hi, <revg3_bg_left_start_hi, <revg5_bg_left_start_hi, <revg6_bg_left_start_hi
revg_bg_l_shi_hi:
    .byte 0, >revg_bg_left_start_hi, >revg4_bg_left_start_hi, >revg7_bg_left_start_hi, >revg2_bg_left_start_hi, >revg3_bg_left_start_hi, >revg5_bg_left_start_hi, >revg6_bg_left_start_hi
revg_bg_l_ilo_lo:
    .byte 0, <revg_bg_left_id_lo, <revg4_bg_left_id_lo, <revg7_bg_left_id_lo, <revg2_bg_left_id_lo, <revg3_bg_left_id_lo, <revg5_bg_left_id_lo, <revg6_bg_left_id_lo
revg_bg_l_ilo_hi:
    .byte 0, >revg_bg_left_id_lo, >revg4_bg_left_id_lo, >revg7_bg_left_id_lo, >revg2_bg_left_id_lo, >revg3_bg_left_id_lo, >revg5_bg_left_id_lo, >revg6_bg_left_id_lo
revg_bg_l_ihi_lo:
    .byte 0, <revg_bg_left_id_hi, <revg4_bg_left_id_hi, <revg7_bg_left_id_hi, <revg2_bg_left_id_hi, <revg3_bg_left_id_hi, <revg5_bg_left_id_hi, <revg6_bg_left_id_hi
revg_bg_l_ihi_hi:
    .byte 0, >revg_bg_left_id_hi, >revg4_bg_left_id_hi, >revg7_bg_left_id_hi, >revg2_bg_left_id_hi, >revg3_bg_left_id_hi, >revg5_bg_left_id_hi, >revg6_bg_left_id_hi

.assert revg3_bg_mt_tl = revg2_bg_mt_tl, lderror, "revg3_bg_mt_tl offset mismatch"
.assert revg3_bg_map_lo = revg2_bg_map_lo, lderror, "revg3_bg_map_lo offset mismatch"
.assert revg3_bg_attr_lo = revg2_bg_attr_lo, lderror, "revg3_bg_attr_lo offset mismatch"
.assert revg5_bg_mt_tl = revg2_bg_mt_tl, lderror, "revg5_bg_mt_tl offset mismatch"
.assert revg6_bg_mt_tl = revg2_bg_mt_tl, lderror, "revg6_bg_mt_tl offset mismatch"
.assert revg6_bg_map_lo = revg5_bg_map_lo, lderror, "revg6_bg_map_lo offset mismatch"
.assert revg6_bg_attr_lo = revg5_bg_attr_lo, lderror, "revg6_bg_attr_lo offset mismatch"

; X = dispatch index:
;   0 = revg_bg_draw_screen (Stages 1 & 4)
;   1 = revg_bg_load_all    (Stages 1..4)
;   2 = revg_bg_stream      (Stages 1..4)
;   3 = revg_prepare_column (Stages 1..4)
;   4 = revg2_prepare_row   (Stages 2 & 3)
revg_bg_dispatch:
    lda revg_bg_disp_hi,x
    pha
    lda revg_bg_disp_lo,x
    pha
    rts
revg_bg_disp_lo:
    .byte <(revg_bg_draw_screen - 1), <(revg_bg_load_all - 1)
    .byte <(revg_bg_stream - 1), <(revg_prepare_column - 1)
    .byte <(revg2_prepare_row - 1)
revg_bg_disp_hi:
    .byte >(revg_bg_draw_screen - 1), >(revg_bg_load_all - 1)
    .byte >(revg_bg_stream - 1), >(revg_prepare_column - 1)
    .byte >(revg2_prepare_row - 1)

; Draw the Stage 1 screen around the camera during forced blank (draw_pan): the HUD attribute
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
    lda revg_bg_active
    cmp #2
    beq @s4_bound
    cmp #3
    beq @s7_bound
    lda pan_data_x_hi
    cmp #>(revg_bg_camera_columns * 2 + 30)
    bne :+
    lda pan_data_x
    cmp #<(revg_bg_camera_columns * 2 + 30)
:
    bcs @done
    bcc @do_col
@s7_bound:
    lda pan_data_x_hi
    bne @done
    lda pan_data_x
    cmp #32
    bcs @done
    bcc @do_col
@s4_bound:
    lda pan_data_x_hi
    cmp #>(revg4_bg_camera_columns * 2 + 30)
    bne :+
    lda pan_data_x
    cmp #<(revg4_bg_camera_columns * 2 + 30)
:
    bcs @done                   ; Past the last tile column of the section
@do_col:
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
    ; Stage 7 throne room: snapshot rows 16..23 ($2200..$22FF) and attr ($23E0..$23EF)
    ; so hybrid BG Astaroth can restore the throne cleanly when stepping or defeated.
    lda revg_bg_active
    cmp #3
    bne @done_ret
    lda #$22
    sta $2006
    lda #$00
    sta $2006
    lda $2007                   ; PPU read-buffer prime
    ldx #0
:
    lda $2007
    sta s7_bg_orig_nt,x
    inx
    bne :-
    lda #$23
    sta $2006
    lda #$e0
    sta $2006
    lda $2007                   ; PPU read-buffer prime
    ldx #0
:
    lda $2007
    sta s7_bg_orig_attr,x
    inx
    cpx #16
    bne :-
@done_ret:
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
; ap_load_stage_visuals). Works for Stages 1..4 (revg_bg_active = 1..4).
revg_bg_load_all:
    lda pan_x+1
    asl a
    asl a
    asl a
    asl a
    sta revg_cam_col
    lda prg_bank2
    pha
    ldx revg_bg_active
    lda revg_bg_map_bank_tbl,x
    jsr select_bank2
    lda revg_bg_pal_lo_tbl,x
    sta revg_ptr
    lda revg_bg_pal_hi_tbl,x
    sta revg_ptr+1
    lda #0
    sta $2000                   ; Increment by 1; NMI stays off
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    ldy #0
:
    lda (revg_ptr),y
    sta $2007
    iny
    cpy #16
    bne :-
    lda #0
    sta revg_zone
    sta revg_pal_pending
    sta revg_fade_pending
    sta gate_open_pending
    sta revg_chr_count
    sta revg_q_head
    sta revg_q_tail
    ldx revg_bg_active
    lda revg_cam_col
    cmp revg_bg_swap_col_tbl,x
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
; column entered, swap the palettes at the zone boundary, and copy the next tiles for NMI.
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
    ldx revg_bg_active
    lda revg_cam_col
    cmp revg_bg_swap_col_tbl,x
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
    bmi @no_col_cap
    ldx #revg_chr_with_column
    lda pan_need_row
    bpl @defer_chr              ; diagonal scroll: defer BG CHR to keep VBlank safe
    lda campaign_hud_dirty
    beq @cap_done
@defer_chr:
    ldx #0                      ; scroll + HUD dirty: defer BG CHR by 1 frame
    beq @cap_done
@no_col_cap:
    lda pan_need_row
    bmi @cap_done
    ldx #revg_chr_with_column
    lda campaign_hud_dirty
    bne @defer_chr
@cap_done:
    stx revg_count
    cpx #0
    beq @stream_done
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

; A = 0 for zone 1 or 1 for zone 2 palettes 2 and 3; the NMI writes them.
revg_prepare_palette:
    sta revg_zone
    ldx revg_bg_active
    lda revg_bg_map_bank_tbl,x
    jsr select_bank2
    lda revg_zone
    bne @z2
    lda revg_bg_pal_lo_tbl,x
    clc
    adc #8
    sta revg_ptr
    lda revg_bg_pal_hi_tbl,x
    adc #0
    sta revg_ptr+1
    jmp @cp_pal
@z2:
    lda revg_bg_pal2_lo_tbl,x
    sta revg_ptr
    lda revg_bg_pal2_hi_tbl,x
    sta revg_ptr+1
@cp_pal:
    ldy #7
:
    lda (revg_ptr),y
    sta revg_pal_buf,y
    dey
    bpl :-
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
    sta revg2_tmp0              ; camera column
    stx revg2_tmp1              ; direction (0=right, 1=left)
    ldx revg_bg_active
    lda revg_bg_list_bank_tbl,x
    jsr select_bank2
    lda revg2_tmp1
    bne @use_left
    lda revg_bg_r_slo_lo,x
    sta revg_ptr
    lda revg_bg_r_slo_hi,x
    sta revg_ptr+1
    lda revg_bg_r_shi_lo,x
    sta revg_ptr2
    lda revg_bg_r_shi_hi,x
    sta revg_ptr2+1
    ldy revg2_tmp0
    iny
    lda (revg_ptr),y
    dey
    sec
    sbc (revg_ptr),y
    sta revg_count
    lda (revg_ptr),y
    sta revg2_tmp2              ; start lo
    lda (revg_ptr2),y
    sta revg2_tmp3              ; start hi
    lda revg2_tmp2
    clc
    adc revg_bg_r_ilo_lo,x
    sta revg_ptr
    lda revg2_tmp3
    adc revg_bg_r_ilo_hi,x
    sta revg_ptr+1
    lda revg2_tmp2
    clc
    adc revg_bg_r_ihi_lo,x
    sta revg_ptr2
    lda revg2_tmp3
    adc revg_bg_r_ihi_hi,x
    sta revg_ptr2+1
    jmp @copy
@use_left:
    lda revg_bg_l_slo_lo,x
    sta revg_ptr
    lda revg_bg_l_slo_hi,x
    sta revg_ptr+1
    lda revg_bg_l_shi_lo,x
    sta revg_ptr2
    lda revg_bg_l_shi_hi,x
    sta revg_ptr2+1
    ldy revg2_tmp0
    iny
    lda (revg_ptr),y
    dey
    sec
    sbc (revg_ptr),y
    sta revg_count
    lda (revg_ptr),y
    sta revg2_tmp2
    lda (revg_ptr2),y
    sta revg2_tmp3
    lda revg2_tmp2
    clc
    adc revg_bg_l_ilo_lo,x
    sta revg_ptr
    lda revg2_tmp3
    adc revg_bg_l_ilo_hi,x
    sta revg_ptr+1
    lda revg2_tmp2
    clc
    adc revg_bg_l_ihi_lo,x
    sta revg_ptr2
    lda revg2_tmp3
    adc revg_bg_l_ihi_hi,x
    sta revg_ptr2+1
@copy:
    ldy #0
    ldx revg_q_tail
@entry:
    cpy revg_count
    beq @copied
    txa
    clc
    adc #1
    cmp revg_q_head
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
    ldx revg_bg_active
    lda revg_tile_hi
    clc
    adc revg_bg_pat_bank_tbl,x
    jsr select_bank2
    ; Pattern at $C010 + 16 * id, slot at $D010 + id (identical in Stage 1 & Stage 2).
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

; Fill pan_col_tiles and attribute bytes for tile column pan_data_x (9 bits).
; Dispatches to revg2_prepare_column when revg_bg_active >= 4 (2-axis: Stages 2, 3, 5, 6).
revg_prepare_column:
    lda revg_bg_active
    cmp #4
    bcc :+
    jmp revg2_prepare_column
:
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
    lda revg_bg_active
    cmp #2
    bne :+
    jmp @s4_col
:
    cmp #3
    bne :+
    jmp @s7_col
:
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
@s4_col:
    lda #revg4_bg_map_bank
    jsr select_bank2
    ldy revg_mcol
    lda pan_data_x
    lsr a
    bcc :+
    jmp @s4_right
:
.repeat revg4_bg_view_rows, row
    ldx .ident(.sprintf("revg4_bg_map_row%d", row)),y
    lda revg4_bg_mt_tl,x
    sta pan_col_tiles + 2 * row
    lda revg4_bg_mt_bl,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
    jmp @s4_attributes
@s4_right:
.repeat revg4_bg_view_rows, row
    ldx .ident(.sprintf("revg4_bg_map_row%d", row)),y
    lda revg4_bg_mt_tr,x
    sta pan_col_tiles + 2 * row
    lda revg4_bg_mt_br,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
@s4_attributes:
    tya
    lsr a
    tay
.repeat 7, row
    lda .ident(.sprintf("revg4_bg_attr_row%d", row)),y
    sta revg_col_attr + row
.endrepeat
    pla
    jmp select_bank2

@s7_col:
    lda #revg7_bg_map_bank
    jsr select_bank2
    ldy revg_mcol
    lda pan_data_x
    lsr a
    bcc :+
    jmp @s7_right
:
.repeat revg_bg_view_rows, row
    ldx .ident(.sprintf("revg7_bg_map_row%d", row)),y
    lda revg7_bg_mt_tl,x
    sta pan_col_tiles + 2 * row
    lda revg7_bg_mt_bl,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
    jmp @s7_attributes
@s7_right:
.repeat revg_bg_view_rows, row
    ldx .ident(.sprintf("revg7_bg_map_row%d", row)),y
    lda revg7_bg_mt_tr,x
    sta pan_col_tiles + 2 * row
    lda revg7_bg_mt_br,x
    sta pan_col_tiles + 2 * row + 1
.endrepeat
@s7_attributes:
    tya
    lsr a
    tay
.repeat 7, row
    lda .ident(.sprintf("revg7_bg_attr_row%d", row)),y
    sta revg_col_attr + row
.endrepeat
    pla
    jmp select_bank2

; --- Stage 2 (2-axis) row & column preparation in BANK50 --------------------------------
; Prepare pan_row_tiles[0..31] and revg2_row_attr[0..7] for pan_need_row (0..59).
revg2_prepare_row:
    lda pan_need_row
    jsr pan_address_for_row
    ; During live gameplay (pan_ctrl != 0), never overwrite the fixed HUD rows 0..2 ($2000..$205F)
    ; when vertical scrolling wraps through even pages 2 or 4 (Stages 3, 5, 6).
    lda pan_ctrl
    beq @row_hud_ok
    lda pan_addr_hi
    cmp #$20
    bne @row_hud_ok
    lda pan_addr_lo
    cmp #$60
    bcs @row_hud_ok
    lda #pan_no_update
    sta pan_need_row
    rts
@row_hud_ok:
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

    lda prg_bank2
    pha
    ; Determine vertical page (0..3) from pan_need_row / 30 and select its map bank.
    ldy #0
    lda pan_need_row
@row_vp:
    cmp #pan_page_rows
    bcc @row_vp_ok
    sbc #pan_page_rows
    iny
    bne @row_vp
@row_vp_ok:
    sty revg2_tmp0              ; page (0..4)
    ldx revg_bg_active
    cpx #6
    bcs @row_packed5
    tya
    clc
    adc revg_bg_map_bank_tbl,x
    jsr select_bank2
    ; Map row pointer for local metatile row = pan_local_y >> 1 (0..14).
    lda pan_local_y
    lsr a
    tay
    lda revg2_bg_map_lo,y
    sta revg_ptr
    lda revg2_bg_map_hi,y
    sta revg_ptr+1
    jmp @row_ptr_ok
@row_packed5:
    lda revg_bg_map_bank_tbl,x
    jsr select_bank2
    lda pan_need_row
    lsr a                       ; global metatile row (0..74)
    tay
    lda revg5_bg_map_lo,y
    sta revg_ptr
    lda revg5_bg_map_hi,y
    sta revg_ptr+1
@row_ptr_ok:
    lda pan_local_y
    and #1
    sta revg2_tmp1              ; 0 = top half (tl/tr), 1 = bottom half (bl/br)

    ; Starting world tile X = (pan_tile_x_hi:pan_tile_x) + (1 if fine X > 0).
    lda pan_tile_x
    sta pan_data_x
    lda pan_tile_x_hi
    sta pan_data_x_hi
    lda pan_x
    and #7
    beq :+
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    ; Save starting X for attribute pass.
    lda pan_data_x
    sta revg2_tmp2
    lda pan_data_x_hi
    sta revg2_tmp3

    lda #32
    sta revg_count
@tile_loop:
    lda pan_data_x_hi
    lsr a
    lda pan_data_x
    ror a
    tay                         ; metatile col (0..223)
    lda (revg_ptr),y
    tax                         ; metatile id
    lda pan_data_x
    and #1
    ora revg2_tmp1
    beq @q_tl
    lda revg2_tmp1
    bne @q_bot
    lda revg2_bg_mt_tr,x
    jmp @store_t
@q_tl:
    lda revg2_bg_mt_tl,x
    jmp @store_t
@q_bot:
    lda pan_data_x
    and #1
    bne @q_br
    lda revg2_bg_mt_bl,x
    jmp @store_t
@q_br:
    lda revg2_bg_mt_br,x
@store_t:
    pha
    lda pan_data_x
    and #31
    tax
    pla
    sta pan_row_tiles,x
    inc pan_data_x
    bne :+
    inc pan_data_x_hi
:
    dec revg_count
    bne @tile_loop

    ; Fill revg2_row_attr[0..7] for attribute row = pan_local_y >> 2 (0..7).
    lda pan_local_y
    lsr a
    lsr a
    tay
    ; Keep palette 0 on HUD attribute row ($23C0..$23C7 on even pages 0, 2, 4).
    bne @do_row_attr
    lda revg2_tmp0
    and #1
    bne @do_row_attr
    lda #0
    ldx #7
:
    sta revg2_row_attr,x
    dex
    bpl :-
    jmp @row_done
@do_row_attr:
    lda revg_bg_active
    cmp #6
    bcs @do_row_attr5
    lda revg2_bg_attr_lo,y
    sta revg_ptr
    lda revg2_bg_attr_hi,y
    sta revg_ptr+1
    jmp @ra_ptr_ok
@do_row_attr5:
    tya
    sta revg2_tmp1              ; ar (0..7)
    lda revg2_tmp0              ; page (0..4)
    asl a
    asl a
    asl a
    ora revg2_tmp1              ; page * 8 + ar (0..39)
    tay
    lda revg5_bg_attr_lo,y
    sta revg_ptr
    lda revg5_bg_attr_hi,y
    sta revg_ptr+1
@ra_ptr_ok:
    ; First attribute pair = (start_x + 3) >> 2 (covers all 8 visible attribute columns).
    lda revg2_tmp2
    clc
    adc #3
    sta revg2_tmp2
    lda revg2_tmp3
    adc #0
    lsr a
    ror revg2_tmp2
    lsr a
    ror revg2_tmp2
    ldy revg2_tmp2              ; attr pair (0..111)
    lda #8
    sta revg_count
@attr_loop:
    tya
    and #7
    tax
    cpy #112
    bcc :+
    lda #0
    beq @st_attr
:
    lda (revg_ptr),y
@st_attr:
    sta revg2_row_attr,x
    iny
    dec revg_count
    bne @attr_loop
@row_done:
    pla
    jmp select_bank2

; Prepare pan_col_tiles[0..pan_col_length-1] and split 8-byte revg_col_attr[0..7]
; for Stage 2 horizontal-mirroring 2-axis scroll.
revg2_prepare_column:
    lda #31
    sec
    sbc pan_top_rows
    tay
    lda pan_y
    and #7
    bne :+
    dey                         ; tile-aligned: only 30 - pan_top_rows rows visible (avoid HUD row 0 & OOB)
:
    sty pan_col_length
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

    ; Metatile column = (pan_data_x_hi:pan_data_x) >> 1 (0..223); right bit in revg2_tmp2.
    lda pan_data_x
    and #1
    sta revg2_tmp2
    lda pan_data_x_hi
    lsr a
    lda pan_data_x
    ror a
    sta revg_mcol

    lda prg_bank2
    pha
    lda #$ff
    sta revg2_tmp3              ; cached map page ($ff = none)

    ldx #0
@col_loop:
    stx revg2_tmp1              ; buffer index
    lda stage_height_rows
    asl a
    cmp pan_data_y
    beq @col_oob
    bcs @col_in_bounds
@col_oob:
    lda #0
    jmp @col_st
@col_in_bounds:
    lda pan_data_y
    ldy #0
@col_vp:
    cmp #pan_page_rows
    bcc @col_vp_ok
    sbc #pan_page_rows
    iny
    bne @col_vp
@col_vp_ok:
    sta revg2_tmp0              ; local tile row (0..29)
    ldx revg_bg_active
    cpx #6
    bcs @col_packed5
    cpy revg2_tmp3
    beq :+
    sty revg2_tmp3
    tya
    clc
    adc revg_bg_map_bank_tbl,x
    jsr select_bank2
:
    lda revg2_tmp0
    lsr a
    tay                         ; local metatile row (0..14)
    lda revg2_bg_map_lo,y
    sta revg_ptr
    lda revg2_bg_map_hi,y
    sta revg_ptr+1
    jmp @col_ptr_ok
@col_packed5:
    lda revg2_tmp3
    beq :+
    lda #0
    sta revg2_tmp3
    lda revg_bg_map_bank_tbl,x
    jsr select_bank2
:
    lda pan_data_y
    lsr a                       ; global metatile row (0..74)
    tay
    lda revg5_bg_map_lo,y
    sta revg_ptr
    lda revg5_bg_map_hi,y
    sta revg_ptr+1
@col_ptr_ok:
    ldy revg_mcol
    lda (revg_ptr),y
    tax                         ; metatile id
    lda revg2_tmp0
    and #1
    bne @col_bot
    lda revg2_tmp2
    bne @col_tr
    lda revg2_bg_mt_tl,x
    jmp @col_st
@col_tr:
    lda revg2_bg_mt_tr,x
    jmp @col_st
@col_bot:
    lda revg2_tmp2
    bne @col_br
    lda revg2_bg_mt_bl,x
    jmp @col_st
@col_br:
    lda revg2_bg_mt_br,x
@col_st:
    ldx revg2_tmp1
    sta pan_col_tiles,x
    inc pan_data_y
    inx
    cpx pan_col_length
    beq :+
    jmp @col_loop
:

    ; Compute 8 column attribute bytes spanning the visible vertical window.
    lda pan_col_addr+1
    and #$28
    ora #$03
    sta revg_attr_hi            ; $23 or $2B
    eor #$08
    sta revg2_attr_hi2          ; other nametable's attribute page
    lda revg_mcol
    lsr a
    sta revg2_tmp2              ; attr pair (0..111)
    and #7
    sta revg2_tmp0              ; attr_col (0..7)
    ora #$c0
    sta revg2_attr_lo2          ; $C0 + attr_col (row 0 of second nametable)
    lda pan_local_y
    lsr a
    lsr a
    sta revg2_tmp1              ; ar_start (0..7)
    asl a
    asl a
    asl a
    ora revg2_tmp0
    ora #$c0
    sta revg_attr_lo            ; $C0 + ar_start*8 + attr_col
    lda #8
    sec
    sbc revg2_tmp1
    sta revg2_attr_split        ; 1..8 bytes in first nametable

    ; First nametable vertical page = (pan_tile_y + pan_top_rows) / 30 (0..3).
    lda pan_tile_y
    clc
    adc pan_top_rows
    ldy #0
@ca_vp:
    cmp #pan_page_rows
    bcc @ca_vp_ok
    sbc #pan_page_rows
    iny
    bne @ca_vp
@ca_vp_ok:
    sty revg2_tmp0              ; current page (0..4)
    ldx revg_bg_active
    cpx #6
    bcs @ca_sel5
    tya
    clc
    adc revg_bg_map_bank_tbl,x
    jsr select_bank2
    jmp @ca_loop_init
@ca_sel5:
    lda revg_bg_map_bank_tbl,x
    jsr select_bank2
@ca_loop_init:

    ldx #0
    lda revg2_tmp1              ; ar = ar_start
    sta revg_count
@cattr_loop:
    cpx revg2_attr_split
    bne @cattr_same
    ; Switch to next vertical page (mod 2 for S2, mod 4 for S3, mod 5 for S5/S6), ar = 0.
    ldy revg2_tmp0
    iny
    lda revg_bg_active
    cmp #6
    bcs @ca_mod5
    cmp #5
    beq @ca_mod4
    tya
    and #1
    jmp @ca_setp
@ca_mod4:
    tya
    and #3
    jmp @ca_setp
@ca_mod5:
    tya
    cmp #5
    bcc @ca_setp5
    lda #0
@ca_setp5:
    sta revg2_tmp0
    lda #0
    sta revg_count
    jmp @cattr_same
@ca_setp:
    sta revg2_tmp0
    ldy revg_bg_active
    clc
    adc revg_bg_map_bank_tbl,y
    jsr select_bank2
    lda #0
    sta revg_count
@cattr_same:
    ldy revg_count
    bne @cattr_read
    lda revg2_tmp0
    and #1
    bne @cattr_read
    lda #0                      ; preserve HUD palette 0 on $2000 attr row 0
    beq @cattr_st
@cattr_read:
    lda revg_bg_active
    cmp #6
    bcs @cattr_read5
    lda revg2_bg_attr_lo,y
    sta revg_ptr
    lda revg2_bg_attr_hi,y
    sta revg_ptr+1
    jmp @cattr_fetch
@cattr_read5:
    lda revg2_tmp0
    asl a
    asl a
    asl a
    ora revg_count              ; page * 8 + ar (0..39)
    tay
    lda revg5_bg_attr_lo,y
    sta revg_ptr
    lda revg5_bg_attr_hi,y
    sta revg_ptr+1
@cattr_fetch:
    ldy revg2_tmp2
    lda (revg_ptr),y
@cattr_st:
    sta revg_col_attr,x
    inc revg_count
    inx
    cpx #8
    beq :+
    jmp @cattr_loop
:

    pla
    jmp select_bank2

.segment "CODE"
; Fixed bank trampoline to BANK50 ($A000) BG routines (X = dispatch index 0..4).
call_bank50_bg:
    lda prg_bank1
    pha
    lda #50
    jsr select_bank1
    jsr revg_bg_dispatch
    pla
    jmp select_bank1

; Fixed bank: the NMI part (from upload_pan), also used by revg_bg_draw_screen.
revg_upload:
    lda pan_ctrl                ; Increment by 1
    sta $2000
    jsr revg_spr_upload
    lda revg_chr_count
    beq @palette
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
    lda #8
    sta revg_nmi_cnt
@b2:
    lda revg_chr_data,x
    sta $2007
    lda revg_chr_data+1,x
    sta $2007
    inx
    inx
    dec revg_nmi_cnt
    bne @b2
    iny
    cpy revg_chr_count
    bne @tile
    lda #0
    sta revg_chr_count
@palette:
    lda revg_fade_pending
    beq @no_fade
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    tax
:
    lda revg_fade_buf,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda #0
    sta revg_fade_pending
@no_fade:
    lda revg_pal_pending
    beq @nt_updates
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
@nt_updates:
    lda revg_bg_active
    cmp #4
    bcs revg_upload_s2_nt
    lda pan_need_col
    bmi revg_upload_done
revg_upload_column:
    jsr upload_pan_column
    lda #7
    sta revg_nmi_cnt
    lda revg_attr_lo
    ldy revg_attr_hi
    ldx #0
revg_upload_attr_run:
    sty $2006
    sta $2006
    pha
    lda revg_col_attr,x
    sta $2007
    pla
    clc
    adc #8
    inx
    cpx revg_nmi_cnt
    bne revg_upload_attr_run
revg_upload_done:
    lda gate_open_pending
    beq @gate_done
    lda pan_ctrl
    ora #4                      ; +32 vertical increment: clear 1 full 8-tile gate column
    sta $2000
    lda gate_open_hi
    sta $2006
    lda gate_open_lo
    sta $2006
    lda #0
    ldx #8
:
    sta $2007
    dex
    bne :-
    sta gate_open_pending
@gate_done:
    rts
revg_upload_s2_nt:
    lda pan_need_row
    bmi :+
    jsr upload_pan_row
:
    lda pan_need_col
    bmi revg_upload_done
    jsr upload_pan_column
    lda revg2_attr_split
    sta revg_nmi_cnt
    lda revg_attr_lo
    ldy revg_attr_hi
    ldx #0
    jsr revg_upload_attr_run
    cpx #8
    beq revg_upload_done
    lda #8
    sta revg_nmi_cnt
    lda revg2_attr_lo2
    ldy revg2_attr_hi2
    jmp revg_upload_attr_run
