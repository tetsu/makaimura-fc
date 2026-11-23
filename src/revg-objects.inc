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
rs_max_req = 3
rs_max_pieces = 20

.segment "ZEROPAGE"
rs_ptr: .res 2
rs_lptr: .res 2
rs_lptr2: .res 2

.segment "WRAM"
rs_tile_slot: .res 1024         ; tile id -> slot + 1, 0 when not resident
rs_slot_lo: .res rs_slots       ; the tile in each slot ($ff in rs_slot_hi: free)
rs_slot_hi: .res rs_slots
rs_slot_stamp: .res rs_slots    ; rs_frame when the slot was last drawn
rs_clock: .res 1
rs_frame: .res 1
rs_budget: .res 1
rs_req_n: .res 1
rs_req_slot: .res rs_max_req
rs_req_lo: .res rs_max_req
rs_req_hi: .res rs_max_req
rs_up_n: .res 1                 ; read by the NMI
rs_up_hi: .res rs_max_req
rs_up_lo: .res rs_max_req
rs_up_data: .res 32 * rs_max_req
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
rs_obj_i: .res 1
rs_obj_n: .res 1
rs_piece_slot: .res rs_max_pieces
rs_a_cur: .res 2
rs_a_next: .res 2
rs_a_cnt: .res 1
rs_last: .res 2                 ; the fallback frame of the object being drawn
rs_stage_bank: .res 1
; Arthur
rp_kind: .res 1
rp_cur: .res 2
rp_next: .res 2
rp_cnt: .res 1
rp_prev_x: .res 2
rp_prev_feet: .res 1
rp_prev_lock: .res 1
rp_flip: .res 1
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
; NMI: upload the sprite tiles that the last draw requested (up to rs_max_req). Called
; from revg_upload with the PPU in +1 mode.
revg_spr_upload:
    ldy rs_up_n
    bne :+
    rts
:
    ldy #0
    ldx #0
@tile:
    lda rs_up_hi,y
    sta $2006
    lda rs_up_lo,y
    sta $2006
    lda #2
    sta rs_nmi_half
@half:
.repeat 16
    lda rs_up_data,x
    sta $2007
    inx
.endrepeat
    dec rs_nmi_half
    beq :+
    jmp @half
:
    iny
    cpy rs_up_n
    beq :+
    jmp @tile
:
    lda #0
    sta rs_up_n
    rts

call_bank6_objects:
    lda #6
    jsr select_bank2
    jsr revg_update_objects
    lda #4
    jmp select_bank2

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
    lda #$ff
    sta rp_kind
    lda #<rf_9596
    sta rp_cur
    lda #>rf_9596
    sta rp_cur+1
    rts

revg_spr_palettes:
    revg_spr_palette_bytes

.segment "BANK7"
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

; rs_tid -> A = its slot with C clear, loading it if the budget allows; C set if not.
rs_get_slot:
    ldy rs_tid
    lda #<rs_tile_slot
    sta rs_lptr
    lda rs_tid+1
    clc
    adc #>rs_tile_slot
    sta rs_lptr+1
    lda (rs_lptr),y
    beq @miss
    tax
    dex
    lda rs_frame
    sta rs_slot_stamp,x
    txa
    clc
    rts
@miss:
    lda rs_req_n
    cmp rs_budget
    bcs @fail
    jsr rs_evict
    bcs @fail
    txa
    clc
    adc #1
    sta (rs_lptr),y
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
; old tile is forgotten. C set when every slot is in use. Keeps Y and rs_lptr.
rs_evict:
    sty rs_save_y
    lda #rs_slots
    sta rs_scan
@scan:
    lda rs_clock
    clc
    adc #1
    and #rs_slots - 1
    sta rs_clock
    tax
    lda rs_slot_hi,x
    bmi @take
    lda rs_frame
    sec
    sbc rs_slot_stamp,x
    cmp #2
    bcs @drop
    dec rs_scan
    bne @scan
    ldy rs_save_y
    sec
    rts
@drop:
    lda #<rs_tile_slot
    sta rs_lptr2
    lda rs_slot_hi,x
    clc
    adc #>rs_tile_slot
    sta rs_lptr2+1
    ldy rs_slot_lo,x
    lda #0
    sta (rs_lptr2),y
@take:
    ldy rs_save_y
    clc
    rts

; Find (or load) the slot of every piece of rs_fid; rs_missing counts the pieces that
; could not get one.
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
    stx rs_pi
    lda (rs_ptr),y
    sta rs_tid
    iny
    lda (rs_ptr),y
    lsr a
    lsr a
    and #3
    sta rs_tid+1
    sty rs_py
    jsr rs_get_slot
    ldx rs_pi
    bcc :+
    inc rs_missing
    lda #$ff
:
    sta rs_piece_slot,x
    lda rs_py
    clc
    adc #3
    tay
    inx
    cpx rs_count
    bne @piece
@done:
    rts

; Write the pieces of rs_fid (after rs_resolve) to OAM around rs_ox, rs_oy (the OAM Y
; of the object point's row: 255 - Y + camera).
rs_emit:
    lda rs_full
    beq @far377
    jmp @done
@far377:
    jsr rs_map_fid
    lda rs_count
    bne @far380
    jmp @done
@far380:
    ldx #0
    ldy #4
@piece:
    stx rs_pi
    sty rs_py
    lda rs_piece_slot,x
    cmp #$ff
    bne @far388
    jmp @next
@far388:
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
    sta sprites+2,x
    lda rs_pyl
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
    lda rs_py
    clc
    adc #4
    tay
    inx
    cpx rs_count
    beq @far462
    jmp @piece
@far462:
@done:
    rts

; Draw rs_fid, or rs_last when rs_fid's tiles cannot all be loaded this frame. Returns
; the frame drawn in rs_fid.
rs_draw_or_last:
    jsr rs_resolve
    lda rs_missing
    beq @emit
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
    jmp rs_emit
@partial:
    pla
    sta rs_fid+1
    pla
    sta rs_fid
    jsr rs_resolve
@emit:
    jmp rs_emit

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

; A = arcade Y low, X = high -> rs_oy = 255 - Y - camera.
rs_set_arcade_y:
    sta rs_tmp
    stx rs_tmp+1
    lda #$ff
    sec
    sbc rs_tmp
    sta rs_tmp
    lda #0
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
    inc rs_frame
    lda #0
    sta rs_oam
    sta rs_full
    sta rs_req_n
    ; Two new tiles a frame, one when a background column also goes up this frame.
    ldx #rs_max_req
    lda pan_need_col
    bmi :+
    ldx #1
:
    stx rs_budget
    jsr rp_draw_player
    jsr rs_draw_shots
    jsr rs_draw_platforms
    jsr rs_draw_objects
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

; Animation starts by kind, armoured then naked.
rp_start_lo:
    .byte <rf_9596, <rf_9596, <rf_965A, <rf_9732, <rf_966E, <rf_96DE, <rf_970A, <rf_9213, <rf_9213
    .byte <rf_9927, <rf_9913, <rf_9947, <rf_9ABE
    .byte <rf_95F2, <rf_95F2, <rf_969A, <rf_9746, <rf_96A8, <rf_96F2, <rf_96F2, <rf_91FF, <rf_91FF
    .byte <rf_9927, <rf_9913, <rf_9947, <rf_9ABE
rp_start_hi:
    .byte >rf_9596, >rf_9596, >rf_965A, >rf_9732, >rf_966E, >rf_96DE, >rf_970A, >rf_9213, >rf_9213
    .byte >rf_9927, >rf_9913, >rf_9947, >rf_9ABE
    .byte >rf_95F2, >rf_95F2, >rf_969A, >rf_9746, >rf_96A8, >rf_96F2, >rf_96F2, >rf_91FF, >rf_91FF
    .byte >rf_9927, >rf_9913, >rf_9947, >rf_9ABE
; Kinds that show their first frame only.
rp_still:
    .byte 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0

; A = Arthur's kind this frame.
rp_choose:
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
:
    lda rs_tmp+2
    cmp rp_kind
    beq @same
    sta rp_kind
    ldx #0
    lda ap_armor
    cmp #ap_armor_full
    beq :+
    ldx #13
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
    ; The naked climb has one frame, mirrored every 6 frames (the armoured one has both).
    lda rp_kind
    cmp #rp_kind_ladder
    bne @shown
    dec rp_flip_timer
    bpl @shown
    lda #5
    sta rp_flip_timer
    lda rp_flip
    eor #$40
    sta rp_flip
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
    sta rs_mirror
    ; The naked climb takes its mirror from rp_flip.
    lda rp_kind
    cmp #rp_kind_ladder
    bcc :+
    cmp #rp_kind_ladder_still + 1
    bcs :+
    lda ap_armor
    cmp #ap_armor_full
    beq :+
    lda rp_flip
    sta rs_mirror
:
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
    lda rs_frame
    jsr rs_div3
    and #3
    tay
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne :+
    lda rs_torch_lo,y
    pha
    lda rs_torch_hi,y
    tay
    pla
    jmp @set
:
    cmp #ap_weapon_axe
    bne :+
    lda rs_axe_lo,y
    pha
    lda rs_axe_hi,y
    tay
    pla
    jmp @set
:
    tay
    lda rs_shot_frames_lo,y
    pha
    lda rs_shot_frames_hi,y
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

; A = A / 3 (0-85).
rs_div3:
    ldy #0
:
    cmp #3
    bcc :+
    sbc #3
    iny
    bne :-
:
    tya
    rts

; --- The raft over the lake: the arcade's grass island (support $02, shape 2) ----------
rs_draw_platforms:
    ldx #0
@plat:
    stx rs_shot_i
    lda plat_type,x
    cmp #plat_moving_h
    bcc @next
    lda #<rf_5871
    sta rs_fid
    sta rs_last
    lda #>rf_5871
    sta rs_fid+1
    sta rs_last+1
    lda #0
    sta rs_mirror
    lda #$ff
    sta rs_pal
    lda plat_x_lo,x
    pha
    lda plat_x_hi,x
    tax
    pla
    jsr rs_set_x
    ldx rs_shot_i
    lda plat_y,x
    pha
    lda plat_y_hi,x
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
; The starting object moves on each frame, so that past eight on a line every one blinks.
rs_draw_objects:
    lda rs_frame
    and #7
    cmp #ob_max
    bcc :+
    lda #0
:
    sta rs_obj_i
    lda #ob_max
    sta rs_obj_n
@obj:
    ldx rs_obj_i
    lda ob_kind,x
    bne @far1048
    jmp @next
@far1048:
    lda ob_nxt_l,x              ; no animation set yet: nothing to draw
    ora ob_nxt_h,x
    ora ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    jmp @next
:
    lda ob_cur_l,x
    sta rs_a_cur
    lda ob_cur_h,x
    sta rs_a_cur+1
    lda ob_nxt_l,x
    sta rs_a_next
    lda ob_nxt_h,x
    sta rs_a_next+1
    lda ob_cnt,x
    sta rs_a_cnt
    lda rs_a_cur
    sta rs_last
    lda rs_a_cur+1
    sta rs_last+1
    jsr rs_anim_step
    ldx rs_obj_i
    lda rs_a_cur
    sta ob_cur_l,x
    sta rs_fid
    lda rs_a_cur+1
    sta ob_cur_h,x
    sta rs_fid+1
    lda rs_a_next
    sta ob_nxt_l,x
    lda rs_a_next+1
    sta ob_nxt_h,x
    lda rs_a_cnt
    sta ob_cnt,x
    ; An empty frame (id 0) draws nothing.
    lda rs_fid
    ora rs_fid+1
    beq @next
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
    lda ob_xl,x
    pha
    lda ob_xh,x
    tax
    pla
    jsr rs_set_x
    ldx rs_obj_i
    lda ob_yl,x
    pha
    lda ob_yh,x
    tax
    pla
    jsr rs_set_arcade_y
    jsr rs_draw_or_last
    ; A pot carrier shows its pot above it.
    ldx rs_obj_i
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
