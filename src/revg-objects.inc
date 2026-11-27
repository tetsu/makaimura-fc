; Stage 1 sprites from the arcade frames (REVG_STAGE1): Arthur, his shots, the enemies,
; items and effects are drawn with the Revision G animation frames that
; tools/convert-revg-sprites.py converts (revg-sprite-data.inc, docs/revision-g-sprites.md).
;
; - A frame is a list of 8x16 pieces placed around the object point as the arcade
;   places its 16x16 sprites. rf_XXXX is the frame at arcade address $XXXX.
; - The 732 tiles do not fit the 128 sprite slots of CHR-RAM, so the slots are a cache:
;   a piece whose tile is not resident takes the slot used longest ago (not in the last
;   two frames), and the NMI uploads up to rs_max_req tiles a frame. A frame whose tiles
;   cannot all be loaded this frame is drawn as the object's previous frame instead.
; - Animations step as the arcade's $8ED7 does: a frame shows for its duration + 1
;   frames, then the next one.

rs_slots = 128
rs_max_req = 4
rs_max_pieces = 32

.segment "ZEROPAGE"
rs_ptr: .res 2
rs_lptr: .res 2
rs_lptr2: .res 2
rs_clock: .res 1
rs_frame: .res 1
rs_budget: .res 1
rs_req_n: .res 1
rs_fid: .res 2
rs_ox: .res 2
rs_oy: .res 2
rs_mirror: .res 1               ; 0, or $40 to mirror the frame around the object X
rs_pal: .res 1                  ; $ff: the frame's palettes; 0-3: one palette for all
rs_oam: .res 1
rs_full: .res 1
rs_count: .res 1
rs_missing: .res 1
rs_tid: .res 2
rs_scan: .res 1
rs_save_y: .res 1
rs_pi: .res 1
rs_py: .res 1
rs_pyl: .res 1
rs_px: .res 1
rs_ptile: .res 1
rs_tmp: .res 4
rs_nmi_half: .res 1
rs_shot_i: .res 1
rs_y_max_page: .res 1
rs_obj_i: .res 1
rs_obj_n: .res 1
rs_a_cur: .res 2
rs_a_next: .res 2
rs_a_cnt: .res 1
rs_last: .res 2                 ; the fallback frame of the object being drawn
rs_stage_bank: .res 1

.segment "WRAM"
rs_tile_slot:
rs_tile_slot_0: .res 256        ; tile id 0..1023 -> slot + 1, 0 when not resident
rs_tile_slot_1: .res 256
rs_tile_slot_2: .res 256
rs_tile_slot_3: .res 256
rs_slot_lo: .res rs_slots       ; the tile in each slot ($ff in rs_slot_hi: free)
rs_slot_hi: .res rs_slots
rs_slot_stamp: .res rs_slots    ; rs_frame when the slot was last drawn
rs_req_pad_old: .res 9          ; Preserves WRAM layout for symbols before fe_map_pin_late
rs_up_n: .res 1                 ; read by the NMI
rs_up_pad_old: .res 122         ; Expanded rs_req_*/rs_up_*/rs_piece_slot live after fe_map_pin_late
; Arthur
rp_kind: .res 1
rp_cur: .res 2
rp_next: .res 2
rp_cnt: .res 1
rp_prev_x: .res 2
rp_prev_feet: .res 1
rp_prev_lock: .res 1
rp_prev_armor: .res 1           ; Cached ap_armor so armor transitions immediately reload anim
rp_flip_timer: .res 1

; --- Objects (enemies, enemy shots, items, effects), updated in Bank 6 -----------------
ob_max = 20
ob_kind: .res ob_max            ; 0 = free (kinds in revg-enemies.inc)
ob_state: .res ob_max
ob_step: .res ob_max
ob_t0: .res ob_max
ob_t1: .res ob_max
ob_t2: .res ob_max
ob_xf: .res ob_max              ; X, 16.8 (arcade X)
ob_xl: .res ob_max
ob_xh: .res ob_max
ob_yf: .res ob_max              ; Y, 16.8 (arcade Y: up is positive)
ob_yl: .res ob_max
ob_yh: .res ob_max
ob_vxf: .res ob_max             ; speeds, 8.8 signed
ob_vxi: .res ob_max
ob_vyf: .res ob_max
ob_vyi: .res ob_max
ob_cur_l: .res ob_max           ; the frame on screen and the animation
ob_cur_h: .res ob_max
ob_nxt_l: .res ob_max
ob_nxt_h: .res ob_max
ob_cnt: .res ob_max
ob_mirror: .res ob_max
ob_flash: .res ob_max           ; frames left of a hit flash
ob_hp: .res ob_max
ob_var: .res ob_max
ob_flags: .res ob_max           ; ofl_* bits
ob_dir: .res ob_max             ; 0 right, 1 left
ob_by_l: .res ob_max            ; base Y (swing centre) or other per-kind values
ob_by_h: .res ob_max
ob_aux: .res ob_max
ob_aux2: .res ob_max
ob_carry: .res ob_max           ; non-zero: carries a pot (an item when it dies)
ob_src: .res ob_max             ; the placed enemy it came from, or $ff

.segment "CODE"
; NMI: upload the sprite tiles that the last draw requested (up to rs_max_req) and any
; Stage 7 hybrid BG Astaroth update. Called from revg_upload with the PPU in +1 mode.
revg_spr_upload:
    lda rs_up_n
    ora s7_bg_dirty
    beq @ret
    lda prg_bank2
    pha
    lda #7
    jsr select_bank2
    jsr revg_spr_upload_b7
    pla
    jmp select_bank2
@ret:
    rts

call_bank6_objects:
    lda #6
    jsr select_bank2
    jsr revg_update_objects
    lda #4
    jmp select_bank2

call_bank6_clear_seq:
    lda #7
    jsr select_bank2
    jsr revg_step_clear_seq
    bcs @clear_done
    jsr ap_follow_camera
    jmp submit_frame
@clear_done:
    jmp show_course_clear

.segment "BANK3"
; Forget every sprite tile (the slots' contents are not known any more) and Arthur's
; animation. Called with the stage visuals.
rs_init:
    lda #<rs_tile_slot
    sta rs_lptr
    lda #>rs_tile_slot
    sta rs_lptr+1
    ldx #4
    lda #0
    tay
:
    sta (rs_lptr),y
    iny
    bne :-
    inc rs_lptr+1
    dex
    bne :-
:
    sta rs_tile_slot_4,y
    iny
    bne :-
    ldx #rs_slots - 1
    lda #$ff
:
    sta rs_slot_hi,x
    dex
    bpl :-
    lda #0
    sta rs_up_n
    sta rs_req_n
    sta rs_clock
    sta rs_obj_phase
    ldx #ob_max - 1
:
    sta ob_last_kind,x
    dex
    bpl :-
    lda #$ff
    sta rp_kind
    sta rp_prev_armor
    lda #<rf_9596
    sta rp_cur
    sta rp_last
    lda #>rf_9596
    sta rp_cur+1
    sta rp_last+1
    rts

revg_spr_palettes:
    revg_spr_palette_bytes

.segment "BANK7"
.assert rs_max_req = 4, error, "revg_spr_upload_b7 expects rs_max_req = 4"
.macro rs_upload_one_tile idx
    lda rs_up_hi + idx
    sta $2006
    lda rs_up_lo + idx
    sta $2006
    .repeat 32, i
        lda rs_up_data + (idx * 32) + i
        sta $2007
    .endrepeat
.endmacro

; Fast unrolled NMI sprite tile uploader (~278 cycles/tile, zero indexing overhead)
; plus Stage 7 hybrid BG Astaroth nametable/attribute uploader.
revg_spr_upload_b7:
    lda rs_up_n
    bne :+
    jmp @chk_s7_bg
:
    rs_upload_one_tile 0
    dec rs_up_n
    bne :+
    jmp @chk_s7_bg
:
    rs_upload_one_tile 1
    dec rs_up_n
    bne :+
    jmp @chk_s7_bg
:
    rs_upload_one_tile 2
    dec rs_up_n
    bne :+
    jmp @chk_s7_bg
:
    rs_upload_one_tile 3
    lda #0
    sta rs_up_n
@chk_s7_bg:
    lda s7_bg_dirty
    bne :+
    rts
:
    jmp s7_bg_upload_b7

; rs_fid -> its record bank at $8000 and rs_ptr.
rs_map_fid:
    lda rs_fid+1
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    adc #revg_spr_meta_bank
    cmp prg_bank0
    beq :+
    jsr select_bank0
:
    lda rs_fid
    sta rs_ptr
    lda rs_fid+1
    and #$1f
    ora #$80
    sta rs_ptr+1
    rts

; Step the animation in rs_a_*: a frame shows for its duration + 1 frames.
rs_anim_step:
    lda rs_a_cnt
    beq @advance
    dec rs_a_cnt
    rts
@advance:
    lda rs_a_next
    sta rs_a_cur
    sta rs_fid
    lda rs_a_next+1
    sta rs_a_cur+1
    sta rs_fid+1
    jsr rs_map_fid
    ldy #0
    lda (rs_ptr),y
    sta rs_a_cnt
    iny
    lda (rs_ptr),y
    sta rs_a_next
    iny
    lda (rs_ptr),y
    sta rs_a_next+1
    rts

; Cache miss path: allocate/evict a slot for rs_tid (with Y = rs_tid low) if budget allows.
; Returns A = slot with C clear, or C set on failure.
rs_get_slot_miss:
    lda rs_req_n
    cmp rs_budget
    bcs @fail
    jsr rs_evict
    bcs @fail
    txa
    clc
    adc #1
    ldy rs_tid+1
    beq @set0
    cpy #1
    beq @set1
    cpy #2
    beq @set2
    cpy #3
    beq @set3
    ldy rs_tid
    sta rs_tile_slot_4,y
    jmp @queued
@set3:
    ldy rs_tid
    sta rs_tile_slot_3,y
    jmp @queued
@set0:
    ldy rs_tid
    sta rs_tile_slot_0,y
    jmp @queued
@set1:
    ldy rs_tid
    sta rs_tile_slot_1,y
    jmp @queued
@set2:
    ldy rs_tid
    sta rs_tile_slot_2,y
@queued:
    lda rs_tid
    sta rs_slot_lo,x
    lda rs_tid+1
    sta rs_slot_hi,x
    lda rs_frame
    sta rs_slot_stamp,x
    ldy rs_req_n
    txa
    sta rs_req_slot,y
    lda rs_tid
    sta rs_req_lo,y
    lda rs_tid+1
    sta rs_req_hi,y
    inc rs_req_n
    txa
    clc
    rts
@fail:
    sec
    rts

; X = a free slot, or the next one (clock order) not drawn in the last two frames; its
; old tile is forgotten. C set when every slot is in use.
rs_evict:
    lda #rs_slots
    sta rs_scan
@scan:
    ldx rs_clock
    inx
    txa
    and #rs_slots - 1
    sta rs_clock
    tax
    ; In Stage 7, pin slots 0..77 (all 6 pre-loaded Astaroth frames) so Astaroth never re-streams.
    lda campaign_stage
    cmp #7
    bne :+
    cpx #78
    bcc @scan_next
:
    lda rs_slot_hi,x
    bmi @take
    lda rs_frame
    sec
    sbc rs_slot_stamp,x
    cmp #2
    bcs @drop
@scan_next:
    dec rs_scan
    bne @scan
    sec
    rts
@drop:
    ldy rs_slot_lo,x
    lda rs_slot_hi,x
    beq @clr0
    cmp #1
    beq @clr1
    cmp #2
    beq @clr2
    cmp #3
    beq @clr3
    lda #0
    sta rs_tile_slot_4,y
    clc
    rts
@clr3:
    lda #0
    sta rs_tile_slot_3,y
    clc
    rts
@clr0:
    lda #0
    sta rs_tile_slot_0,y
    clc
    rts
@clr1:
    lda #0
    sta rs_tile_slot_1,y
    clc
    rts
@clr2:
    lda #0
    sta rs_tile_slot_2,y
@take:
    clc
    rts

; Find (or load) the slot of every piece of rs_fid; rs_missing counts the pieces that
; could not get one. Fast-paths resident tiles directly through rs_tile_slot_0..3.
rs_resolve:
    jsr rs_map_fid
    lda #0
    sta rs_missing
    ldy #3
    lda (rs_ptr),y
    cmp #rs_max_pieces + 1
    bcc :+
    lda #rs_max_pieces
:
    sta rs_count
    tax
    beq @done
    ldx #0
    ldy #6
@piece:
    lda (rs_ptr),y
    sta rs_tid
    iny
    sty rs_py
    lda (rs_ptr),y
    and #$1c
    beq @p0
    cmp #$04
    beq @p1
    cmp #$08
    beq @p2
    cmp #$0c
    beq @p3
    ldy rs_tid
    lda rs_tile_slot_4,y
    bne @hit
    lda #4
    bne @miss
@p3:
    ldy rs_tid
    lda rs_tile_slot_3,y
    bne @hit
    lda #3
    bne @miss
@p0:
    ldy rs_tid
    lda rs_tile_slot_0,y
    bne @hit
    beq @miss_a
@p1:
    ldy rs_tid
    lda rs_tile_slot_1,y
    bne @hit
    lda #1
    bne @miss
@p2:
    ldy rs_tid
    lda rs_tile_slot_2,y
    beq @miss2
@hit:
    tay
    dey
    lda rs_frame
    sta rs_slot_stamp,y
    tya
    sta rs_piece_slot,x
@advance:
    lda rs_py
    clc
    adc #3
    tay
    inx
    cpx rs_count
    bne @piece
@done:
    rts
@miss2:
    lda #2
@miss:
@miss_a:
    sta rs_tid+1
    stx rs_pi
    jsr rs_get_slot_miss
    ldx rs_pi
    bcc :+
    inc rs_missing
    lda #$ff
:
    sta rs_piece_slot,x
    jmp @advance

; Write the pieces of rs_fid (after rs_resolve) to OAM around rs_ox, rs_oy (the OAM Y
; of the object point's row: 255 - Y + camera).
rs_emit:
    jsr rs_map_fid
rs_emit_mapped:
    lda rs_full
    bne @ret
    lda rs_count
    bne @emit_go
@ret:
    rts
@emit_go:
    ; Alternate piece emit order every frame so wide bosses (>8 sprites/scanline with
    ; Arthur or shots) flicker across both sides instead of permanently losing one half.
    lda rs_frame
    and #1
    bne @emit_rev_init
    ldx #0
    ldy #4
    bne @piece
@emit_rev_init:
    ldx rs_count
    txa
    asl a
    asl a
    tay
    dex
@piece:
    stx rs_pi
    sty rs_py
    lda rs_piece_slot,x
    cmp #$ff
    bne :+
    jmp @next
:
    asl a
    ora #1
    sta rs_ptile
    lda (rs_ptr),y              ; top
    ldx #0
    cmp #$80
    bcc :+
    dex
:
    clc
    adc rs_oy
    sta rs_pyl
    txa
    adc rs_oy+1
    bne @next
    lda rs_pyl
    cmp #239
    bcs @next
    cmp #hud_rows * 8 - 1
    bcc @next
    iny
    lda (rs_ptr),y              ; left
    ldx rs_skip_center
    beq @left_keep
    clc
    adc #16
    cmp #32                     ; skip center 4 cols (-16, -8, 0, +8) drawn on BG
    bcc @next
    lda (rs_ptr),y
@left_keep:
    bit rs_mirror
    bvc :+
    eor #$ff
    sec
    sbc #7
:
    ldx #0
    cmp #$80
    bcc :+
    dex
:
    clc
    adc rs_ox
    sta rs_px
    txa
    adc rs_ox+1
    bne @next
    iny
    iny
    lda (rs_ptr),y              ; attribute
    and #$c3
    eor rs_mirror
    ldx rs_pal
    bmi :+
    and #$fc
    ora rs_pal
:
    ldx rs_oam
    ldy rs_pyl
    cpy #208                    ; arcade BG attr bit 3: rows 13-14 (grass/water foreground)
    bcc :+
    ora #$20                    ; OAM behind-BG priority for underground/underwater pieces
:
    sta sprites+2,x
    tya
    sta sprites,x
    lda rs_ptile
    sta sprites+1,x
    lda rs_px
    sta sprites+3,x
    txa
    clc
    adc #4
    sta rs_oam
    bne @next
    lda #1
    sta rs_full
    rts
@next:
    ldx rs_pi
    lda rs_frame
    and #1
    bne @next_rev
    lda rs_py
    clc
    adc #4
    tay
    inx
    cpx rs_count
    beq @done
    jmp @piece
@next_rev:
    dex
    bmi @done
    lda rs_py
    sec
    sbc #4
    tay
    jmp @piece
@done:
    rts

; Draw rs_fid, or rs_last when rs_fid's tiles cannot all be loaded this frame. Returns
; the frame drawn in rs_fid.
rs_draw_or_last:
    jsr rs_resolve
    lda rs_missing
    beq @emit_now
    lda rs_last
    ora rs_last+1
    beq @emit_now
    lda rs_last
    cmp rs_fid
    bne @try_last
    lda rs_last+1
    cmp rs_fid+1
    bne @try_last
@emit_now:
    jmp rs_emit_mapped
@try_last:
    lda rs_fid
    pha
    lda rs_fid+1
    pha
    lda rs_last
    sta rs_fid
    lda rs_last+1
    sta rs_fid+1
    jsr rs_resolve
    lda rs_missing
    bne @partial
    pla
    pla
    jmp rs_emit_mapped
@partial:
    pla
    sta rs_fid+1
    pla
    sta rs_fid
    jsr rs_resolve
    jmp rs_emit_mapped

; A = world X low, X = high -> rs_ox (minus the camera).
rs_set_x:
    sec
    sbc camera_low
    sta rs_ox
    txa
    sbc camera_high
    sta rs_ox+1
    rts

; A = FC world Y low (254 - arcade Y), X = high -> rs_oy = Y + 1 - camera.
rs_set_fc_y:
    sec
    adc #0
    bcc :+
    inx
:
    sec
    sbc ap_cam_y_lo
    sta rs_oy
    txa
    sbc ap_cam_y_hi
    sta rs_oy+1
    rts

; A = arcade Y low, X = high -> rs_oy = Y_top - Y - camera.
; Y_top: $00FF (1p), $01FF (2p), $03BF (4p Stage 3), $04AF (5p Stage 5 & 6).
rs_set_arcade_y:
    sta rs_tmp
    stx rs_tmp+1
    lda #$ff
    ldy rs_y_max_page
    cpy #3
    bne :+
    lda #$bf
    bne @ytop_ok
:
    cpy #4
    bne @ytop_ok
    lda #$af
@ytop_ok:
    sec
    sbc rs_tmp
    sta rs_tmp
    lda rs_y_max_page
    sbc rs_tmp+1
    tax
    lda rs_tmp
    sec
    sbc ap_cam_y_lo
    sta rs_oy
    txa
    sbc ap_cam_y_hi
    sta rs_oy+1
    rts

; --- The frame ---------------------------------------------------------------------
revg_draw_sprites:
    lda prg_bank0
    sta rs_stage_bank
    lda stage_cam_y_max_page
    sta rs_y_max_page
    jsr rs_drop_unsent
    inc rs_frame
    lda #0
    sta rs_oam
    sta rs_full
    sta rs_req_n
    sta s7_saw_ast
    sta rs_skip_center
    ; Cap sprite CHR uploads to stay within NTSC VBlank (2273 cycles) alongside
    ; BG scroll nametable, BG CHR streaming, HUD updates, palette fades, and OAM DMA.
    ; Unrolled revg_spr_upload_b7 takes ~278 cyc/tile, fitting 4 tiles on clean frames.
    lda demo_active
    beq @normal_budget
    ldx #2
    lda campaign_hud_dirty
    ora revg_fade_pending
    bne @set_budget             ; 2 sprite tiles when HUD or palette fade uploads in opening.
    ldx #4
    bne @set_budget
@normal_budget:
    ldx #0
    lda pan_need_col
    bpl @set_budget             ; Defer sprite CHR on any BG column upload frame.
    lda pan_need_row
    bpl @set_budget             ; Defer sprite CHR on any BG row upload frame.
    lda revg_chr_count
    cmp #2
    bcs @set_budget             ; Defer sprite CHR when 2+ BG CHR tiles upload.
    cmp #1
    bne @no_bg_chr
    ldx #1
    lda campaign_hud_dirty
    ora revg_fade_pending
    ora revg_pal_pending
    ora gate_open_pending
    bne @set_budget             ; 1 sprite tile when 1 BG CHR tile + HUD/fade/pal/gate upload.
    ldx #2                      ; 2 sprite tiles when only 1 BG CHR tile uploads.
    bne @set_budget
@no_bg_chr:
    ldx #2
    lda campaign_hud_dirty
    ora revg_fade_pending
    bne @set_budget             ; 2 sprite tiles when HUD or palette fade uploads.
    ldx #4                      ; 4 sprite tiles on clean non-scroll frames (boss fights).
@set_budget:
    stx rs_budget
    lda demo_active
    beq :+
    jsr rs_draw_opening
    jmp @hide
:
    ; OAM scanline priority: Arthur (1st, never flickers) -> Platforms -> Enemies -> Weapons.
    ; Weapons sit last on 3 of 4 frames so they flicker first instead of characters,
    ; and swap ahead of enemies on 1 of 4 frames so shots never vanish in dense crowds.
    jsr rp_draw_player
    jsr rs_draw_platforms
    lda rs_frame
    and #3
    beq @shots_first
    jsr rs_draw_objects
    jsr rs_draw_shots
    jmp @hide
@shots_first:
    jsr rs_draw_shots
    jsr rs_draw_objects
@hide:
    ; If Stage 7 Astaroth left BG hybrid mode (died, flipped, or rapid-fire), restore throne BG.
    lda s7_bg_active
    beq :+
    lda s7_saw_ast
    bne :+
    jsr s7_bg_clear_b7
:
    ; Hide the rest.
    lda rs_full
    bne @flush
    ldx rs_oam
    lda #$f8
:
    sta sprites,x
    inx
    inx
    inx
    inx
    bne :-
@flush:
    jsr rs_flush
    lda rs_stage_bank
    jmp select_bank0

; Tiles the last draw requested that no NMI has sent (the draw ran twice without one, as
; while a stage loads) are not in their slots: forget them, so this draw loads them
; again instead of drawing what the slots held before.
rs_drop_unsent:
    ldx rs_up_n
    beq @done
@tile:
    dex
    lda rs_up_hi,x              ; the slot: (address - chr_sprite_address) / 32
    and #$0f
    asl a
    asl a
    asl a
    sta rs_tmp
    lda rs_up_lo,x
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    ora rs_tmp
    tay
    lda rs_slot_hi,y
    bmi @next
    cmp #4
    beq @slot4
    clc
    adc #>rs_tile_slot
    sta rs_lptr2+1
    lda #<rs_tile_slot
    sta rs_lptr2
    jmp @clear_slot
@slot4:
    lda #<rs_tile_slot_4
    sta rs_lptr2
    lda #>rs_tile_slot_4
    sta rs_lptr2+1
@clear_slot:
    lda rs_slot_lo,y
    pha
    lda #$ff
    sta rs_slot_hi,y
    pla
    tay
    lda #0
    sta (rs_lptr2),y
@next:
    txa
    bne @tile
    stx rs_up_n
@done:
    rts

; Copy the requested tiles for the NMI.
rs_flush:
    ldx #0
    stx rs_tmp
@tile:
    cpx rs_req_n
    beq @done
    stx rs_tmp+1
    lda rs_req_hi,x
    clc
    adc #revg_spr_tile_bank
    jsr select_bank0
    ldx rs_tmp+1
    lda rs_req_lo,x
    pha
    lsr a
    lsr a
    lsr a
    ora #$80
    sta rs_lptr2+1
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    sta rs_lptr2
    lda rs_req_slot,x
    pha
    lsr a
    lsr a
    lsr a
    ora #>chr_sprite_address
    sta rs_up_hi,x
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    sta rs_up_lo,x
    ldx rs_tmp
    ldy #0
:
    lda (rs_lptr2),y
    sta rs_up_data,x
    inx
    iny
    cpy #32
    bne :-
    stx rs_tmp
    ldx rs_tmp+1
    inx
    jmp @tile
@done:
    stx rs_up_n
    rts

; --- Arthur --------------------------------------------------------------------------
; The arcade frames face right; facing left mirrors them, as the arcade's left frames
; are the right ones mirrored.
rp_kind_stand = 0
rp_kind_walk = 1
rp_kind_throw = 2
rp_kind_crouch = 3
rp_kind_crouch_throw = 4
rp_kind_jump = 5
rp_kind_jump_up = 6
rp_kind_ladder = 7
rp_kind_ladder_still = 8
rp_kind_hit = 9
rp_kind_fly_dead = 10
rp_kind_fall_apart = 11
rp_kind_bones = 12
rp_kind_frog = 13
rp_kind_clear = 14

; Animation starts by kind, armoured then naked. The hurt flies as $9913 (one frame, naked);
; the death's flight $9927 turns to bones every other frame ($992D).
rp_start_lo:
    .byte <rf_9596, <rf_9596, <rf_965A, <rf_9732, <rf_966E, <rf_96DE, <rf_970A, <rf_9213, <rf_9213
    .byte <rf_9913, <rf_9927, <rf_9947, <rf_9ABE, <rf_9779, <rf_7374
    .byte <rf_95F2, <rf_95F2, <rf_969A, <rf_9746, <rf_96A8, <rf_96F2, <rf_96F2, <rf_9221, <rf_9221
    .byte <rf_9913, <rf_9927, <rf_9947, <rf_9ABE, <rf_9779, <rf_7374
rp_start_hi:
    .byte >rf_9596, >rf_9596, >rf_965A, >rf_9732, >rf_966E, >rf_96DE, >rf_970A, >rf_9213, >rf_9213
    .byte >rf_9913, >rf_9927, >rf_9947, >rf_9ABE, >rf_9779, >rf_7374
    .byte >rf_95F2, >rf_95F2, >rf_969A, >rf_9746, >rf_96A8, >rf_96F2, >rf_96F2, >rf_9221, >rf_9221
    .byte >rf_9913, >rf_9927, >rf_9947, >rf_9ABE, >rf_9779, >rf_7374
; Kinds that show their first frame only.
rp_still:
    .byte 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1

; A = Arthur's kind this frame.
rp_choose:
    lda ap_clear_seq
    beq @not_clear
    cmp #2
    beq @clear_pose
    ; Steps 1 & 3 ($7350 / $738B): walk right toward and through the castle door.
    lda #rp_kind_walk
    rts
@clear_pose:
    ; Step 2 ($7369): wait 60 frames in the arcade $7374 pose while the door opens.
    lda #rp_kind_clear
    rts
@not_clear:
    lda ap_state
    cmp #ap_state_dead
    bne :+
    lda #rp_kind_bones
    rts
:
    cmp #ap_state_knockback
    bne @not_hit
    lda ap_kb_dying
    bne :+
    lda #rp_kind_hit
    rts
:
    lda ap_kb_step
    cmp #3
    bcs :+
    lda #rp_kind_fly_dead
    rts
:
    cmp #5
    bcs :+
    lda #rp_kind_fall_apart
    rts
:
    lda #rp_kind_bones
    rts
@not_hit:
    lda ap_frog_timer
    ora ap_frog_timer+1
    beq @not_frog
    lda #rp_kind_frog
    rts
@not_frog:
    lda ap_state
    cmp #ap_state_ladder
    bne @not_ladder
    lda player_feet
    cmp rp_prev_feet
    beq :+
    lda #rp_kind_ladder
    rts
:
    lda #rp_kind_ladder_still
    rts
@not_ladder:
    cmp #ap_state_air
    bne @ground
    lda ap_air_dir
    bne :+
    lda #rp_kind_jump_up
    rts
:
    lda #rp_kind_jump
    rts
@ground:
    lda ap_crouch
    beq @standing
    lda ap_shoot_lock
    beq :+
    lda #rp_kind_crouch_throw
    rts
:
    lda #rp_kind_crouch
    rts
@standing:
    lda ap_shoot_lock
    beq :+
    lda #rp_kind_throw
    rts
:
    lda player_x
    cmp rp_prev_x
    bne @walk
    lda player_x+1
    cmp rp_prev_x+1
    bne @walk
    lda #rp_kind_stand
    rts
@walk:
    lda #rp_kind_walk
    rts

rp_draw_player:
    jsr rp_choose
    sta rs_tmp+2
    ; A new throw restarts the throw pose.
    lda ap_shoot_lock
    cmp rp_prev_lock
    beq :+
    bcc :+
    lda #$ff
    sta rp_kind
    sta rp_prev_armor
:
    ; Reload animation whenever kind or armor state changes.
    lda ap_armor
    cmp rp_prev_armor
    beq :+
    sta rp_prev_armor
    lda #$ff
    sta rp_kind
:
    lda rs_tmp+2
    cmp rp_kind
    beq @same
    sta rp_kind
    ldx #0
    lda ap_armor
    cmp #ap_armor_full
    beq :+
    ldx #15                     ; naked table offset (15 kinds)
:
    txa
    clc
    adc rp_kind
    tax
    lda rp_start_lo,x
    sta rp_next
    lda rp_start_hi,x
    sta rp_next+1
    lda #0
    sta rp_cnt
    beq @step
@same:
    ldx rp_kind
    lda rp_still,x
    bne @shown
@step:
    lda rp_next
    sta rs_a_next
    lda rp_next+1
    sta rs_a_next+1
    lda rp_cnt
    sta rs_a_cnt
    lda rp_cur
    sta rs_a_cur
    lda rp_cur+1
    sta rs_a_cur+1
    jsr rs_anim_step
    lda rs_a_cur
    sta rp_cur
    lda rs_a_cur+1
    sta rp_cur+1
    lda rs_a_next
    sta rp_next
    lda rs_a_next+1
    sta rp_next+1
    lda rs_a_cnt
    sta rp_cnt
@shown:
    lda player_x
    sta rp_prev_x
    lda player_x+1
    sta rp_prev_x+1
    lda player_feet
    sta rp_prev_feet
    lda ap_shoot_lock
    sta rp_prev_lock
    ; The blink after a hit: every other frame shows nothing ($8EFD).
    lda ap_state
    cmp #ap_state_knockback
    beq :+
    lda ap_invuln
    and #1
    beq :+
    rts
:
    lda rp_cur
    sta rs_fid
    lda rp_cur+1
    sta rs_fid+1
    lda player_left
    beq :+
    lda #$40
:
    ; Ladder animations ($9213/$9219 armored, $9221/$9227 naked) already alternate both sides.
    ldx rp_kind
    cpx #rp_kind_ladder
    bcc :+
    cpx #rp_kind_ladder_still + 1
    bcs :+
    lda #0
:
    sta rs_mirror
    lda #$ff
    sta rs_pal
    lda player_x
    ldx player_x+1
    jsr rs_set_x
    lda player_feet
    ldx ap_player_y_hi
    jsr rs_set_fc_y
    lda rp_last
    sta rs_last
    lda rp_last+1
    sta rs_last+1
    jsr rs_draw_or_last
    lda rs_missing
    bne :+
    lda rs_fid
    sta rp_last
    lda rs_fid+1
    sta rp_last+1
:
    rts

.segment "WRAM"
rp_last: .res 2
.segment "BANK7"

; --- Shots ---------------------------------------------------------------------------
; Frames by FC weapon (lance, dagger, torch, cross, axe), facing right.
rs_shot_frames_lo:
    .byte <rf_9D7B, <rf_9DB1, <rf_9D45, <rf_9D87, <rf_9D91
rs_shot_frames_hi:
    .byte >rf_9D7B, >rf_9DB1, >rf_9D45, >rf_9D87, >rf_9D91
; The spinning torch and axe: four frames each, 3 frames apiece (duration 2).
rs_torch_lo: .byte <rf_9D45, <rf_9D48, <rf_9D4B, <rf_9D4E
rs_torch_hi: .byte >rf_9D45, >rf_9D48, >rf_9D4B, >rf_9D4E
rs_axe_lo: .byte <rf_9D91, <rf_9D94, <rf_9D97, <rf_9D9A
rs_axe_hi: .byte >rf_9D91, >rf_9D94, >rf_9D97, >rf_9D9A

rs_draw_shots:
    ldx #0
@shot:
    stx rs_shot_i
    lda ps_active,x
    bne @far895
    jmp @next
@far895:
    cmp #ap_shot_spent
    bne @far897
    jmp @next
@far897:
    cmp #2
    bne @flying
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne @flying
    ; The torch's fire on the ground (shape 3): two frames, 4 frames apiece.
    lda rs_frame
    and #4
    beq :+
    lda #<rf_9D67
    ldy #>rf_9D67
    bne @set
:
    lda #<rf_9D61
    ldy #>rf_9D61
    bne @set
@flying:
    lda ps_kind,x
    cmp #ap_weapon_torch
    beq @spin_torch
    cmp #ap_weapon_axe
    beq @spin_axe
    tay
    lda rs_shot_frames_lo,y
    pha
    lda rs_shot_frames_hi,y
    tay
    pla
    jmp @set
@spin_torch:
    jsr rs_spin_frame4
    lda rs_torch_lo,y
    pha
    lda rs_torch_hi,y
    tay
    pla
    jmp @set
@spin_axe:
    jsr rs_spin_frame4
    lda rs_axe_lo,y
    pha
    lda rs_axe_hi,y
    tay
    pla
@set:
    sta rs_fid
    sty rs_fid+1
    lda rs_fid
    sta rs_last
    lda rs_fid+1
    sta rs_last+1
    ldx rs_shot_i
    lda #0
    ldy ps_vx,x
    bpl :+
    lda #$40
:
    sta rs_mirror
    lda #$ff
    sta rs_pal
    lda ps_x_lo,x
    pha
    lda ps_x_hi,x
    tax
    pla
    jsr rs_set_x
    ldx rs_shot_i
    lda ps_y,x
    pha
    lda ps_y_hi,x
    tax
    pla
    jsr rs_set_fc_y
    jsr rs_draw_or_last
@next:
    ldx rs_shot_i
    inx
    cpx #ap_max_shots
    beq @far978
    jmp @shot
@far978:
    rts

; Y = (rs_frame / 3) & 3 in O(1) via mod-12 binary reduction (12 * 16 = 192, 96, 48, 24, 12).
rs_spin_frame4:
    lda rs_frame
    cmp #192
    bcc :+
    sbc #192
:
    cmp #96
    bcc :+
    sbc #96
:
    cmp #48
    bcc :+
    sbc #48
:
    cmp #24
    bcc :+
    sbc #24
:
    cmp #12
    bcc :+
    sbc #12
:
    ldy #0
    cmp #3
    bcc @done
    iny
    cmp #6
    bcc @done
    iny
    cmp #9
    bcc @done
    iny
@done:
    rts

; --- Moving & falling platforms: authentic Revision G frames per stage/support type ---
;   Stage 1 ($02):            rf_5871 (32x16 grass raft)
;   Stage 2 Tower ($03):      rf_5878 (64x16 blue/white ice elevator & falling ledge)
;   Stage 2 Water ($04):      rf_5885 (32x16 grey stone brick platform)
;   Stage 4 & 5 ($05):        rf_5896 (48x16 eyeball cluster platform)
rs_draw_platforms:
    ldx #0
@plat:
    stx rs_shot_i
    lda plat_type,x
    cmp #plat_moving_h
    bcc @next
    lda campaign_stage
    cmp #2
    beq @plat_s2
    cmp #4
    beq @plat_orb
    cmp #5
    beq @plat_orb
    lda #<rf_5871
    ldy #>rf_5871
    jmp @plat_set_fid
@plat_s2:
    lda plat_x_hi,x
    cmp #4                      ; Tower ($03) is local X < 1024; Water channels ($04) >= 2432
    bcs @plat_s2_brick
    lda #<rf_5878
    ldy #>rf_5878
    jmp @plat_set_fid
@plat_s2_brick:
    lda #<rf_5885
    ldy #>rf_5885
    jmp @plat_set_fid
@plat_orb:
    lda #<rf_5896
    ldy #>rf_5896
@plat_set_fid:
    sta rs_fid
    sta rs_last
    sty rs_fid+1
    sty rs_last+1
    lda #0
    sta rs_mirror
    lda #$ff                    ; Use each arcade platform frame's own fitted palette
    sta rs_pal
    lda plat_x_lo,x
    pha
    lda plat_x_hi,x
    tax
    pla
    jsr rs_set_x
    ldx rs_shot_i
    ; Arcade support object center is 24 px under where Arthur stands ($9EB8: y_off=24/23/21).
    lda plat_y,x
    clc
    adc #24
    pha
    lda plat_y_hi,x
    adc #0
    tax
    pla
    jsr rs_set_fc_y
    jsr rs_draw_or_last
@next:
    ldx rs_shot_i
    inx
    cpx #ap_max_plats
    bne @plat
    rts

; --- Objects -------------------------------------------------------------------------
; Coprime stride 7 across all ob_max (20) slots gives every enemy/boss fair OAM
; scanline priority and CHR-RAM streaming budget. Each object retains ob_last_* so
; large boss frames keep showing their last fully-loaded frame until the new frame's
; tiles finish streaming into CHR-RAM.
rs_draw_objects:
    lda rs_obj_phase
    clc
    adc #7
    cmp #ob_max
    bcc :+
    sbc #ob_max
:
    sta rs_obj_phase
    sta rs_obj_i
    lda #ob_max
    sta rs_obj_n
@obj:
    ldx rs_obj_i
    lda ob_kind,x
    bne @active
    sta ob_last_kind,x
    sta ob_last_l,x
    sta ob_last_h,x
    jmp @next
@active:
    cmp ob_last_kind,x
    beq @same_kind
    sta ob_last_kind,x
    lda #0
    sta ob_last_l,x
    sta ob_last_h,x
@same_kind:
    ; In-place animation countdown when the current frame still has ticks left.
    lda ob_cnt,x
    beq @check_adv
    dec ob_cnt,x
    jmp @use_cur
@check_adv:
    lda ob_nxt_l,x              ; no animation set yet: nothing to draw
    ora ob_nxt_h,x
    ora ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    jmp @next
:
    ; Hold advance if ob_cur is still streaming into CHR-RAM (ob_cur != ob_last)
    ; and this is not a terminating frame (ob_nxt == 0).
    lda ob_nxt_l,x
    ora ob_nxt_h,x
    beq @anim_adv
    lda ob_cur_l,x
    ora ob_cur_h,x
    beq @anim_adv
    lda ob_cur_l,x
    cmp ob_last_l,x
    bne @use_cur
    lda ob_cur_h,x
    cmp ob_last_h,x
    bne @use_cur
@anim_adv:
    lda ob_nxt_l,x
    sta ob_cur_l,x
    sta rs_fid
    lda ob_nxt_h,x
    sta ob_cur_h,x
    sta rs_fid+1
    ora rs_fid
    beq @set_last
    jsr rs_map_fid
    ldx rs_obj_i
    ldy #0
    lda (rs_ptr),y
    sta ob_cnt,x
    iny
    lda (rs_ptr),y
    sta ob_nxt_l,x
    iny
    lda (rs_ptr),y
    sta ob_nxt_h,x
    jmp @set_last
@use_cur:
    lda ob_cur_l,x
    sta rs_fid
    lda ob_cur_h,x
    sta rs_fid+1
@set_last:
    lda ob_last_l,x
    sta rs_last
    lda ob_last_h,x
    sta rs_last+1
@anim_ready:
    ; An empty frame (id 0) draws nothing.
    lda rs_fid
    ora rs_fid+1
    bne :+
    jmp @next
:
    ; Pre-cull off-screen objects before CHR cache resolution:
    ; rs_ox = ob_x - camera_x; skip when outside [-32, +287] -> (rs_ox + 32) >= 320.
    lda ob_xl,x
    sec
    sbc camera_low
    sta rs_ox
    lda ob_xh,x
    sbc camera_high
    sta rs_ox+1
    lda rs_ox
    clc
    adc #32
    tay
    lda rs_ox+1
    adc #0
    beq @x_vis
    cmp #1
    bne @skip_cull
    cpy #64
    bcs @skip_cull
@x_vis:
    lda ob_yl,x
    pha
    lda ob_yh,x
    tax
    pla
    jsr rs_set_arcade_y
    ; Skip when rs_oy is outside [-48, +255] -> (rs_oy + 48) >= 304.
    lda rs_oy
    clc
    adc #48
    tay
    lda rs_oy+1
    adc #0
    beq @y_vis
    cmp #1
    bne @skip_cull
    cpy #48
    bcc @y_vis
@skip_cull:
    jmp @next
@y_vis:
    ldx rs_obj_i
    lda ob_mirror,x
    sta rs_mirror
    lda #$ff
    ldy ob_flash,x
    beq :+
    lda rs_frame
    and #1
    beq :+
    lda #1                      ; the hit flash: every other frame in the red palette
:
    sta rs_pal
    ; Stage 7 Astaroth hybrid BG+sprite path: center 4 cols on BG ($A0..$F7) in BOTH
    ; left (ob_mirror=0) and right (ob_mirror=$40) directions -> always 4 sprites/scanline!
    lda campaign_stage
    cmp #7
    bne @normal_draw
    lda ob_kind,x
    cmp #10                     ; ok_astaroth
    bne @normal_draw
@ast_bg_go:
    lda #1
    sta s7_saw_ast
    sta rs_skip_center
    lda ob_xl,x
    clc
    adc #4
    and #$f8                    ; Snap to 8-px tile grid so BG center & sprite edges align seamlessly
    sta rs_ox
    lda #0
    sta rs_ox+1
    jsr rs_draw_or_last
    lda #0
    sta rs_skip_center
    ldx rs_obj_i
    lda rs_missing
    bne @after_last_upd
    lda rs_fid
    sta ob_last_l,x
    lda rs_fid+1
    sta ob_last_h,x
    lda rs_ox
    lsr a
    lsr a
    lsr a                       ; A = draw_col (0..31)
    jsr s7_bg_prepare_b7
    jmp @after_last_upd
@normal_draw:
    jsr rs_draw_or_last
    ldx rs_obj_i
    lda rs_missing
    bne @after_last_upd
    lda rs_fid
    sta ob_last_l,x
    lda rs_fid+1
    sta ob_last_h,x
@after_last_upd:
    ; A pot carrier shows its pot above it.
    lda ob_carry,x
    beq @next
    lda #<rf_E5B3
    sta rs_fid
    sta rs_last
    lda #>rf_E5B3
    sta rs_fid+1
    sta rs_last+1
    lda #0
    sta rs_mirror
    lda rs_oy
    sec
    sbc #20
    sta rs_oy
    bcs :+
    dec rs_oy+1
:
    jsr rs_draw_or_last
@next:
    inc rs_obj_i
    lda rs_obj_i
    cmp #ob_max
    bcc :+
    lda #0
    sta rs_obj_i
:
    dec rs_obj_n
    beq @far1142
    jmp @obj
@far1142:
    rts

; ============================================================================
; Boss Pre-loading (all stages, forced blank) & Stage 7 Hybrid BG Astaroth (BANK7)
; ============================================================================
rs_preload_boss_b7:
    lda #0
    sta s7_bg_active
    sta s7_bg_dirty
    sta s7_bg_rest_n
    lda #$ff
    sta s7_bg_col
    lda demo_active
    beq :+
    rts
:
    lda campaign_stage
    bne :+
    rts
:
    cmp #8
    bcc :+
    rts
:
    lda prg_bank0
    pha
    lda campaign_stage
    cmp #7
    bne @pl_not_s7
    ; Stage 7: clear s7_spr_to_bg[0..127] and set next free BG CHR tile = $A0
    ldx #127
    lda #0
:
    sta s7_spr_to_bg,x
    dex
    bpl :-
    lda #$a0
    sta s7_next_bg
@pl_not_s7:
    ldx campaign_stage
    lda rs_pl_start_tbl,x
    sta rs_tmp+2                ; current index in rs_pl_frames (word index)
    lda rs_pl_end_tbl,x
    sta rs_tmp+3                ; end index (exclusive)
@pl_frame:
    lda rs_tmp+2
    cmp rs_tmp+3
    bne :+
    jmp @pl_frames_done
:
    asl a
    tax
    lda rs_pl_frames,x
    sta rs_fid
    lda rs_pl_frames+1,x
    sta rs_fid+1
    jsr rs_map_fid
    ldy #3
    lda (rs_ptr),y
    sta rs_count
    lda #0
    sta rs_pi
@pl_piece:
    jsr rs_map_fid
    lda rs_pi
    asl a
    asl a
    clc
    adc #5                      ; piece+1 = left
    tay
    lda (rs_ptr),y
    sta rs_px                   ; save piece left
    iny
    lda (rs_ptr),y              ; tid_lo
    sta rs_tid
    iny
    lda (rs_ptr),y              ; attr
    sta rs_py                   ; save piece attr
    lsr a
    lsr a
    and #$07
    sta rs_tid+1
    ; Check if tid is already resident in rs_tile_slot_0..4
    ldy rs_tid
    lda rs_tid+1
    beq @pl_chk0
    cmp #1
    beq @pl_chk1
    cmp #2
    beq @pl_chk2
    cmp #3
    beq @pl_chk3
    lda rs_tile_slot_4,y
    jmp @pl_chk_done
@pl_chk0:
    lda rs_tile_slot_0,y
    jmp @pl_chk_done
@pl_chk1:
    lda rs_tile_slot_1,y
    jmp @pl_chk_done
@pl_chk2:
    lda rs_tile_slot_2,y
    jmp @pl_chk_done
@pl_chk3:
    lda rs_tile_slot_3,y
@pl_chk_done:
    beq @pl_alloc
    sec
    sbc #1
    sta rs_ptile                ; existing sprite slot (0..127)
    jmp @pl_chk_bg
@pl_alloc:
    ; Allocate next sequential sprite slot from rs_clock and upload 32B to $1000+slot*32
    ldx rs_clock
    stx rs_ptile
    inc rs_clock
    txa
    clc
    adc #1
    ldy rs_tid
    ldx rs_tid+1
    beq @pl_s0
    cpx #1
    beq @pl_s1
    cpx #2
    beq @pl_s2
    cpx #3
    beq @pl_s3
    sta rs_tile_slot_4,y
    jmp @pl_reg
@pl_s0:
    sta rs_tile_slot_0,y
    jmp @pl_reg
@pl_s1:
    sta rs_tile_slot_1,y
    jmp @pl_reg
@pl_s2:
    sta rs_tile_slot_2,y
    jmp @pl_reg
@pl_s3:
    sta rs_tile_slot_3,y
@pl_reg:
    ldx rs_ptile
    lda rs_tid
    sta rs_slot_lo,x
    lda rs_tid+1
    sta rs_slot_hi,x
    lda #0
    sta rs_slot_stamp,x
    ; Compute PPU sprite dest $1000 + slot*32 and upload 32 bytes
    lda rs_ptile
    pha
    lsr a
    lsr a
    lsr a
    ora #>chr_sprite_address
    sta $2006
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    sta $2006
    jsr rs_pl_copy_32
@pl_chk_bg:
    ; Stage 7 only: upload unflipped left-facing center tiles for rf_54C8 (idx 7) & rf_54DA (idx 8)
    ; into BG CHR $A0..$D7 (28 x 2 = 56 tiles), and save rf_54C8's 16 tiles in s7_bg_base_l.
    lda campaign_stage
    cmp #7
    bne @pl_next_piece
    lda rs_tmp+2
    cmp #9                      ; Only indices 7 (rf_54C8) & 8 (rf_54DA) -> 28 tiles ($A0..$D7)
    bcs @pl_next_piece
    lda rs_px                   ; left
    clc
    adc #16
    cmp #32
    bcs @pl_next_piece
    ldx rs_ptile
    lda s7_spr_to_bg,x
    bne @pl_have_lbg            ; already uploaded to BG CHR
    lda s7_next_bg
    sta s7_spr_to_bg,x
    pha
    lsr a
    lsr a
    lsr a
    lsr a                       ; high byte of $0000 + bg_tile * 16
    sta $2006
    pla
    asl a
    asl a
    asl a
    asl a
    sta $2006
    inc s7_next_bg
    inc s7_next_bg
    jsr rs_pl_copy_32           ; Unflipped: rf_54C8/DA natively face LEFT!
@pl_have_lbg:
    ; For rf_54C8 (idx 7), also record s7_bg_base_l[col*4 + row/2] and upload right-facing copy!
    lda rs_tmp+2
    cmp #7
    bne @pl_next_piece
    jsr rs_map_fid
    lda rs_pi
    asl a
    asl a
    clc
    adc #4                      ; piece+0 = top (-32, -16, 0, +16)
    tay
    lda (rs_ptr),y
    clc
    adc #32
    lsr a
    lsr a
    lsr a
    lsr a                       ; row_pair = 0..3
    sta rs_tmp+0
    lda rs_px                   ; left (-16, -8, 0, +8)
    clc
    adc #16
    lsr a                       ; (left+16)/2 = 0, 4, 8, 12
    ora rs_tmp+0                ; idx = col*4 + row_pair (0..15)
    tay
    ldx rs_ptile
    lda s7_spr_to_bg,x
    sta s7_bg_base_l,y
@pl_next_piece:
    inc rs_pi
    lda rs_pi
    cmp rs_count
    beq :+
    jmp @pl_piece
:
    inc rs_tmp+2
    jmp @pl_frame
@pl_frames_done:
    lda campaign_stage
    cmp #7
    bne @pl_done
    ; Also upload right-facing (H-flipped) 16 center tiles of rf_54C8 into BG CHR $D8..$F7 (32 tiles).
    lda #<rf_54C8
    sta rs_fid
    lda #>rf_54C8
    sta rs_fid+1
    lda #0
    sta rs_pi
@pl_rflip:
    jsr rs_map_fid
    lda rs_pi
    asl a
    asl a
    clc
    adc #4
    tay
    lda (rs_ptr),y              ; top
    clc
    adc #32
    lsr a
    lsr a
    lsr a
    lsr a                       ; row_pair 0..3
    sta rs_tmp+0
    iny
    lda (rs_ptr),y              ; left
    clc
    adc #16
    cmp #32
    bcs @pl_rf_next
    lsr a                       ; col*4 (0, 4, 8, 12)
    ora rs_tmp+0                ; idx = col*4 + row_pair (0..15)
    sta rs_tmp+1
    iny
    lda (rs_ptr),y              ; tid_lo
    sta rs_tid
    iny
    lda (rs_ptr),y              ; attr
    lsr a
    lsr a
    and #$07
    sta rs_tid+1
    lda s7_next_bg
    ldy rs_tmp+1
    sta s7_bg_base_r,y
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
    inc s7_next_bg
    inc s7_next_bg
    jsr rs_pl_copy_32_hflip
@pl_rf_next:
    inc rs_pi
    lda rs_pi
    cmp #32
    bne @pl_rflip
    ; Set Stage 7 BG Palette 3 ($3F0C..$3F0F) to Astaroth crimson/gold/skin ($0F/$16/$27/$37).
    lda #$3f
    sta $2006
    lda #$0c
    sta $2006
    ldx #0
:
    lda s7_ast_bg_pal3,x
    sta $2007
    inx
    cpx #4
    bne :-
@pl_done:
    pla
    jmp select_bank0
@pl_ret:
    rts

; Copy 32 bytes of sprite tile (rs_tid) from revg_spr_tile_bank to PPU $2007.
rs_pl_copy_32:
    lda rs_tid+1
    clc
    adc #revg_spr_tile_bank
    jsr select_bank0
    lda rs_tid
    pha
    lsr a
    lsr a
    lsr a
    ora #$80
    sta rs_lptr2+1
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    sta rs_lptr2
    ldy #0
:
    lda (rs_lptr2),y
    sta $2007
    iny
    cpy #32
    bne :-
    rts

; Copy 32 bytes of sprite tile (rs_tid) horizontally bit-reversed to PPU $2007 (for BG ob_mirror=$40).
rs_pl_copy_32_hflip:
    lda rs_tid+1
    clc
    adc #revg_spr_tile_bank
    jsr select_bank0
    lda rs_tid
    pha
    lsr a
    lsr a
    lsr a
    ora #$80
    sta rs_lptr2+1
    pla
    asl a
    asl a
    asl a
    asl a
    asl a
    sta rs_lptr2
    ldy #0
@hf_byte:
    lda (rs_lptr2),y
    sta rs_tmp+0
    ldx #8
    lda #0
@hf_bit:
    lsr rs_tmp+0
    rol a
    dex
    bne @hf_bit
    sta $2007
    iny
    cpy #32
    bne @hf_byte
    rts

s7_ast_bg_pal3:
    .byte $0f, $16, $27, $37
rs_pl_start_tbl:
    .byte 0, 0, 0, 2, 2, 4, 4, 7
rs_pl_end_tbl:
    .byte 0, 2, 2, 4, 4, 7, 7, 13
rs_pl_frames:
    .word rf_4FAB, rf_4F7D                                      ; 0..1: S1/S2 Unicorn
    .word rf_5344, rf_53B4                                      ; 2..3: S3/S4 Dragon
    .word rf_4E2B, rf_4E36, rf_4EA9                             ; 4..6: S5/S6 Satan
    .word rf_54C8, rf_54DA, rf_54EC, rf_54FE, rf_5512, rf_553A  ; 7..12: S7 Astaroth (all 6)

; Prepare Stage 7 hybrid BG update for Astaroth at tile column A (center of 4 BG cols col-2..col+1).
s7_bg_prepare_b7:
    ldx s7_bg_dirty
    beq :+
    rts                         ; Keep pending VBlank upload intact until consumed
:
    sta rs_tmp+0                ; draw_col
    lda s7_bg_active
    beq @prep_do
    lda rs_tmp+0
    cmp s7_bg_col
    bne @prep_moved
    lda rs_mirror
    cmp s7_bg_mirror
    bne @prep_do
    lda rs_fid
    cmp s7_bg_fid_lo
    bne @prep_do
    lda rs_fid+1
    cmp s7_bg_fid_hi
    bne @prep_do
    rts                         ; Same column, facing, & frame -> zero VBlank work!
@prep_moved:
    ; Compute which throne column(s) were exposed by moving from s7_bg_col -> draw_col.
    lda rs_tmp+0
    sec
    sbc s7_bg_col
    cmp #1
    beq @rest_left1
    cmp #$ff
    beq @rest_right1
    ; Jumped >1 col: restore all 4 old columns
    lda s7_bg_col
    sec
    sbc #2
    sta s7_bg_rest_col
    lda #4
    sta s7_bg_rest_n
    bne @fill_rest
@rest_left1:
    lda s7_bg_col
    sec
    sbc #2
    sta s7_bg_rest_col
    lda #1
    sta s7_bg_rest_n
    bne @fill_rest
@rest_right1:
    lda s7_bg_col
    clc
    adc #1
    sta s7_bg_rest_col
    lda #1
    sta s7_bg_rest_n
@fill_rest:
    jsr s7_bg_fill_rest_buf
@prep_do:
    ; Fill s7_bg_buf[0..31] (4 cols x 8 rows) for BOTH left (rs_mirror=0) and right (rs_mirror=$40).
    lda rs_mirror
    bne @pb_right
    ; Left-facing (native): use s7_spr_to_bg[slot], falling back to s7_bg_base_l for rf_54FE.
    jsr rs_map_fid
    ldx #0
    ldy #4
@pb_loop:
    stx rs_pi
    sty rs_py
    iny
    lda (rs_ptr),y              ; left (-16, -8, 0, +8)
    clc
    adc #16
    cmp #32
    bcs @pb_next                ; outer sprite column
    and #$18                    ; col_idx * 8 (0, 8, 16, 24)
    sta rs_tmp+1
    dey
    lda (rs_ptr),y              ; top (-32, -16, 0, +16)
    clc
    adc #32
    lsr a
    lsr a
    lsr a                       ; row_idx (0, 2, 4, 6)
    ora rs_tmp+1                ; buf_idx (0..30, even)
    tay
    lda rs_piece_slot,x
    tax
    lda s7_spr_to_bg,x
    bne @pb_put
    ; Fallback to rf_54C8 left-facing center tile at same (col, row_pair)
    tya
    lsr a                       ; (col*8 + row_idx) / 2 = col*4 + row_pair (0..15)
    tax
    lda s7_bg_base_l,x
@pb_put:
    sta s7_bg_buf,y
    clc
    adc #1
    sta s7_bg_buf+1,y
@pb_next:
    ldx rs_pi
    lda rs_py
    clc
    adc #4
    tay
    inx
    cpx rs_count
    bne @pb_loop
    jmp @pb_attrs
@pb_right:
    ; Right-facing (ob_mirror=$40): place mirrored cols (3-col) from s7_bg_base_r ($D8..$F7).
    ldx #0                      ; src col*4 + row_pair (0..15)
@pbr_loop:
    txa
    and #$0c                    ; src col*4 (0, 4, 8, 12)
    eor #$0c                    ; mirrored col*4 (12, 8, 4, 0)
    asl a                       ; mirrored col*8 (24, 16, 8, 0)
    sta rs_tmp+1
    txa
    and #3                      ; row_pair (0..3)
    asl a                       ; row_idx (0, 2, 4, 6)
    ora rs_tmp+1                ; dest buf_idx (0..30)
    tay
    lda s7_bg_base_r,x
    sta s7_bg_buf,y
    clc
    adc #1
    sta s7_bg_buf+1,y
    inx
    cpx #16
    bne @pbr_loop
@pb_attrs:

    ; Build attribute rows 4 & 5 ($23E0..$23EF): base throne attrs + Palette 3 (%11) on Astaroth.
    ldx #15
:
    lda s7_bg_orig_attr,x
    sta s7_bg_attr_r4,x
    dex
    bpl :-
    lda rs_tmp+0
    sec
    sbc #2                      ; first BG tile col (draw_col - 2)
    sta rs_tmp+1
    ldy #4
@attr_col:
    lda rs_tmp+1
    lsr a                       ; metatile col (0..15)
    tax
    lsr a                       ; attr byte index (0..7)
    sta rs_tmp+2
    txa
    and #1
    beq @attr_left
    lda #$cc                    ; right half of 32x32 attr cell -> pal 3 (%11)
    bne @attr_or
@attr_left:
    lda #$33                    ; left half of 32x32 attr cell -> pal 3 (%11)
@attr_or:
    ldx rs_tmp+2
    pha
    ora s7_bg_attr_r4,x
    sta s7_bg_attr_r4,x
    pla
    ora s7_bg_attr_r5,x
    sta s7_bg_attr_r5,x
    inc rs_tmp+1
    dey
    bne @attr_col

    lda rs_tmp+0
    sta s7_bg_col
    lda rs_mirror
    sta s7_bg_mirror
    lda rs_fid
    sta s7_bg_fid_lo
    lda rs_fid+1
    sta s7_bg_fid_hi
    lda #1
    sta s7_bg_active
    sta s7_bg_dirty
    rts

; Restore all 4 BG columns occupied by Astaroth back to the original throne room snapshot.
s7_bg_clear_b7:
    lda s7_bg_col
    sec
    sbc #2
    sta s7_bg_rest_col
    lda #4
    sta s7_bg_rest_n
    jsr s7_bg_fill_rest_buf
    ldx #15
:
    lda s7_bg_orig_attr,x
    sta s7_bg_attr_r4,x
    dex
    bpl :-
    lda #0
    sta s7_bg_active
    lda #2
    sta s7_bg_dirty
    rts

; Copy s7_bg_rest_n (1 or 4) columns starting at s7_bg_rest_col from s7_bg_orig_nt into s7_bg_rest_buf.
s7_bg_fill_rest_buf:
    lda s7_bg_rest_col
    sta rs_tmp+1                ; current col (0..31)
    lda s7_bg_rest_n
    sta rs_tmp+2                ; cols left
    ldy #0                      ; dest offset in s7_bg_rest_buf
@fr_col:
    ldx rs_tmp+1                ; row 0 offset = col
    lda #8
    sta rs_tmp+3
@fr_row:
    lda s7_bg_orig_nt,x
    sta s7_bg_rest_buf,y
    iny
    txa
    clc
    adc #32
    tax
    dec rs_tmp+3
    bne @fr_row
    inc rs_tmp+1
    dec rs_tmp+2
    bne @fr_col
    rts

; NMI VBlank uploader for Stage 7 hybrid BG Astaroth (~450-620 cycles total).
s7_bg_upload_b7:
    lda pan_ctrl
    ora #4                      ; +32 vertical increment for 8-tile columns
    sta $2000
    lda s7_bg_rest_n
    beq @up_ast
    ldx #0                      ; index into s7_bg_rest_buf
    ldy s7_bg_rest_col
@up_rc:
    lda #$22
    sta $2006
    sty $2006
    .repeat 8
        lda s7_bg_rest_buf,x
        sta $2007
        inx
    .endrepeat
    iny
    dec s7_bg_rest_n
    bne @up_rc
@up_ast:
    lda s7_bg_dirty
    cmp #1
    bne @up_attr
    lda s7_bg_col
    sec
    sbc #2
    tay
    ldx #0
    lda #4
    sta rs_nmi_half
@up_ac:
    lda #$22
    sta $2006
    sty $2006
    .repeat 8
        lda s7_bg_buf,x
        sta $2007
        inx
    .endrepeat
    iny
    dec rs_nmi_half
    bne @up_ac
@up_attr:
    lda pan_ctrl                ; back to +1 horizontal increment
    sta $2000
    lda #$23
    sta $2006
    lda #$e0
    sta $2006
    ldx #0
:
    lda s7_bg_attr_r4,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda #0
    sta s7_bg_dirty
    rts
