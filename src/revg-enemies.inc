; Stage 1 enemies, items and effects (REVG_STAGE1), in Bank 6 at $C000. Bank 3 (the
; player) stays at $A000 and the stage at $8000, so the terrain probes and the player's
; hit and score routines are called directly. The objects are drawn in Bank 7
; (revg-objects.inc) with the arcade frames.
;
; The behaviour follows docs/revision-g-enemy-behaviour.md (difficulty row 0, the
; default DIP): states, speeds, hit boxes, durability and points. The placement is the
; arcade's (revg-section0-spawns.inc): the placed enemies come in when the camera gets
; near them, and each page's group starts its type's pattern.

ok_none = 0
ok_zombie = 1
ok_crow = 2
ok_plant = 3
ok_pig = 4
ok_knight = 5
ok_arremer = 6
ok_unicorn = 7
ok_magician = 8
ok_shot = 9                     ; enemy shot: ob_var 0 straight, 1 the spell
ok_item = 10                    ; ob_var = arcade item kind
ok_boom = 11                    ; explosion; ob_carry = drop a pot item at the end
ok_popup = 12                   ; score picture
ok_disc = 13                    ; support $00 variants 2-3
ok_trigger = 14                 ; support $00 variants 0-1 (hidden)
ok_key = 15
ok_spark = 16                   ; a shot bouncing off the knight's shield
ok_kinds = 17

ofl_shootable = $01
ofl_harmful = $02
ofl_shield = $80

.segment "WRAM"
oz_t0: .res 1
oz_t1: .res 1
oz_t2: .res 1
oz_t3: .res 1
oz_dx: .res 2                   ; player X - object X
oz_dy: .res 2                   ; object Y - player Y (arcade, up)
oz_py: .res 2                   ; the player's arcade Y
oz_rng: .res 1
oz_reg_page: .res 1
oz_fix_used: .res 2
oz_fix_next: .res 1
oz_sup_used: .res 2
oz_sch_pat: .res 4              ; $ff: idle
oz_sch_rec: .res 4
oz_sch_end: .res 4
oz_sch_wait: .res 4
oz_sch_once: .res 4
oz_spawns: .res 1
oz_drop_sel: .res 1
oz_key_timer: .res 1
oz_boss_seen: .res 1
oz_obj: .res 1
oz_mul: .res 2

.segment "BANK6"
; --- Per-kind tables -----------------------------------------------------------------
;                 -   zom crow plnt pig knt arr uni mag shot item boom pop disc trig key spark
ok_hp:     .byte 0, 0,  0,   0,   0,  0,  2,  9,  0,  0,  0,   0,   0,  0,   0,   0,  0
ok_points: .byte 0, 2,  1,   1,   1,  1, 10, 20, 20,  0,  0,   0,   0,  0,   0,   0,  0
; Shot box (docs: "自弾との当たり"): X half width, Y range of object Y - shot Y.
ok_sx:     .byte 0, 12, 12,  12,  12, 12, 10, 16, 12,  0,  0,   0,   0,  0,   0,   0,  0
ok_sylo:   .byte 0,$EE,$F0,$F0, $F0,$F0,$F3,$EB,$F3,  0,  0,   0,   0,  0,   0,   0,  0
ok_syhi:   .byte 0, 10,  8,  16,  16, 16, 13, 21, 13,  0,  0,   0,   0,  0,   0,   0,  0
; Player box, standing: X half width, Y range of object Y - player Y.
ok_px:     .byte 0,  9, 10,  10,  14, 10, 16, 23, 30,  6,  0,   0,   0,  0,   0,   0,  0
ok_pylo:   .byte 0,$ED,$F8,$F2, $F2,$EC,$ED,$E2,$E2,$FA,  0,   0,   0,  0,   0,   0,  0
ok_pyhi:   .byte 0, 19,  8,  14,  14, 20, 19, 30, 30,  6,  0,   0,   0,  0,   0,   0,  0
; Update routines.
ok_update_lo:
    .byte <ob_free_rts, <oz_zombie, <oz_crow, <oz_plant, <oz_pig, <oz_knight, <oz_arremer
    .byte <oz_unicorn, <oz_magician, <oz_shot, <oz_item, <oz_boom, <oz_popup, <oz_disc
    .byte <oz_trigger, <oz_key, <oz_boom
ok_update_hi:
    .byte >ob_free_rts, >oz_zombie, >oz_crow, >oz_plant, >oz_pig, >oz_knight, >oz_arremer
    .byte >oz_unicorn, >oz_magician, >oz_shot, >oz_item, >oz_boom, >oz_popup, >oz_disc
    .byte >oz_trigger, >oz_key, >oz_boom

.include "revg-section0-spawns.inc"

; Types the schedulers count, with their limits (bank4:$4240 row 0; the zombie at 2,
; the FC's sprite budget, docs/revision-g-sprites.md).
oz_sch_limit: .byte 2, 3, 3, 1

; round(127 * sin(i * 90 / 64)), i = 0-64.
oz_sine:
    .byte 0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49, 51, 54, 57, 60, 63
    .byte 65, 68, 71, 73, 76, 78, 81, 83, 85, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 107
    .byte 109, 111, 112, 113, 115, 116, 117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126
    .byte 126, 126, 127, 127, 127, 127

; --- Set-up ----------------------------------------------------------------------------
revg_objects_init:
    ldx #ob_max - 1
    lda #0
:
    sta ob_kind,x
    dex
    bpl :-
    sta oz_fix_used
    sta oz_fix_used+1
    sta oz_sup_used
    sta oz_sup_used+1
    sta oz_fix_next
    sta oz_spawns
    sta oz_drop_sel
    sta oz_key_timer
    sta oz_boss_seen
    lda #$ff
    sta oz_reg_page
    ldx #3
:
    sta oz_sch_pat,x
    dex
    bpl :-
    lda oz_rng
    bne :+
    lda #$5a
    sta oz_rng
:
    rts

; --- The frame -----------------------------------------------------------------------
revg_update_objects:
    jsr oz_player_y
    jsr oz_register_pages
    jsr oz_place_fixed
    jsr oz_run_schedulers
    ldx #0
@obj:
    stx oz_obj
    lda ob_kind,x
    beq @next
    lda ob_flash,x
    beq :+
    dec ob_flash,x
:
    ldy ob_kind,x
    lda ok_update_lo,y
    sta oz_mul
    lda ok_update_hi,y
    sta oz_mul+1
    jsr @call
    ldx oz_obj
    lda ob_kind,x
    beq @next
    jsr oz_range_check
@next:
    ldx oz_obj
    inx
    cpx #ob_max
    bne @obj
    jsr oz_player_contact
    jmp oz_key_wait
@call:
    jmp (oz_mul)

ob_free_rts:
    rts

; The player's arcade Y (254 - feet).
oz_player_y:
    lda #254
    sec
    sbc player_feet
    sta oz_py
    lda #0
    sbc ap_player_y_hi
    sta oz_py+1
    rts

; oz_dx = player X - X, oz_dy = Y - player Y; A = oz_dx low.
oz_rel_player:
    lda ob_yl,x
    sec
    sbc oz_py
    sta oz_dy
    lda ob_yh,x
    sbc oz_py+1
    sta oz_dy+1
    lda player_x
    sec
    sbc ob_xl,x
    sta oz_dx
    lda player_x+1
    sbc ob_xh,x
    sta oz_dx+1
    lda oz_dx
    rts

; A = |oz_dx| (255 when farther), Y = |oz_dy| likewise.
oz_abs_d:
    lda oz_dy+1
    bmi @dyn
    bne @dyfar
    ldy oz_dy
    jmp @dx
@dyn:
    cmp #$ff
    bne @dyfar
    lda oz_dy
    eor #$ff
    clc
    adc #1
    tay
    bne @dx
    ldy #255
    bne @dx
@dyfar:
    ldy #255
@dx:
    lda oz_dx+1
    bmi @dxn
    bne @dxfar
    lda oz_dx
    rts
@dxn:
    cmp #$ff
    bne @dxfar
    lda oz_dx
    eor #$ff
    clc
    adc #1
    bne :+
    lda #255
:
    rts
@dxfar:
    lda #255
    rts

; Face the player: ob_dir 0 (right) when the player is to the right.
oz_face_player:
    jsr oz_rel_player
    lda #0
    ldy oz_dx+1
    bpl :+
    lda #1
:
    sta ob_dir,x
    rts

oz_random:
    lda oz_rng
    asl a
    bcc :+
    eor #$1d
:
    sta oz_rng
    rts

; A x Y -> oz_mul (16 bits).
oz_mul8:
    sta oz_t3
    sty oz_t2
    lda #0
    sta oz_mul+1
    ldy #8
:
    asl a
    rol oz_mul+1
    asl oz_t3
    bcc :+
    clc
    adc oz_t2
    bcc :+
    inc oz_mul+1
:
    dey
    bne :--
    sta oz_mul
    rts

; A = phase (0-255) -> A = sin * 127, signed.
oz_sin:
    tay
    and #$7f
    cmp #65
    bcc :+
    eor #$7f
    adc #0                      ; 128 - phase (C is set by the compare)
:
    tay
    lda oz_sine,y
    pha
    tya
    pla
    rts

; A = phase, Y = amplitude (px) -> oz_mul = amplitude * sin / 128, signed 16 bits.
oz_swing:
    sta oz_t0
    sty oz_t1
    and #$7f
    cmp #65
    bcc :+
    eor #$7f
    adc #0
:
    tay
    lda oz_sine,y
    ldy oz_t1
    jsr oz_mul8
    asl oz_mul
    rol oz_mul+1                ; amplitude * sin * 2 / 256
    lda oz_mul+1
    sta oz_mul
    lda #0
    sta oz_mul+1
    lda oz_t0
    bpl :+
    lda #0
    sec
    sbc oz_mul
    sta oz_mul
    lda #0
    sbc #0
    sta oz_mul+1
:
    rts

; --- Movement --------------------------------------------------------------------------
; Add the speeds to the position.
ob_move:
    lda ob_xf,x
    clc
    adc ob_vxf,x
    sta ob_xf,x
    lda ob_vxi,x
    tay
    adc ob_xl,x
    sta ob_xl,x
    tya
    and #$80
    beq :+
    lda #$ff
:
    adc ob_xh,x
    sta ob_xh,x
    lda ob_yf,x
    clc
    adc ob_vyf,x
    sta ob_yf,x
    lda ob_vyi,x
    tay
    adc ob_yl,x
    sta ob_yl,x
    tya
    and #$80
    beq :+
    lda #$ff
:
    adc ob_yh,x
    sta ob_yh,x
    rts

; A = speed fraction, Y = whole px: set ob_vx toward ob_dir.
ob_set_vx_dir:
    sta oz_t0
    sty oz_t1
    lda ob_dir,x
    beq @right
    lda #0
    sec
    sbc oz_t0
    sta ob_vxf,x
    lda #0
    sbc oz_t1
    sta ob_vxi,x
    rts
@right:
    lda oz_t0
    sta ob_vxf,x
    lda oz_t1
    sta ob_vxi,x
    rts

ob_stop:
    lda #0
    sta ob_vxf,x
    sta ob_vxi,x
    sta ob_vyf,x
    sta ob_vyi,x
    rts

; A/Y = animation start (frame id): shown from the next draw.
ob_anim:
    sta ob_nxt_l,x
    tya
    sta ob_nxt_h,x
    lda #0
    sta ob_cnt,x
    rts

; A/Y = the right-facing animation, oz_t2/oz_t3 = the left one: by ob_dir.
ob_anim_dir:
    pha
    lda ob_dir,x
    beq :+
    pla
    lda oz_t2
    ldy oz_t3
    jmp ob_anim
:
    pla
    jmp ob_anim

; C set when the ground (or a floor, a ladder top) is just under the object point.
; The ground's surface puts the object point at 254 - surface, like the player.
oz_on_ground:
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    lda #255
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda #0
    sbc ob_yh,x
    sta ap_probe_y_hi
    bmi @no
    txa
    pha
    jsr ap_tile_type_at
    sta oz_t0
    pla
    tax
    lda oz_t0
    beq @no
    cmp #tile_type_hazard
    beq @no
    sec
    rts
@no:
    clc
    rts

; Put the object on the surface of the tile oz_on_ground found.
oz_snap_ground:
    lda ap_probe_y
    and #$f0
    sta oz_t0
    lda #254
    sec
    sbc oz_t0
    sta ob_yl,x
    lda #0
    sbc ap_probe_y_hi
    sta ob_yh,x
    lda #0
    sta ob_yf,x
    sta ob_vyf,x
    sta ob_vyi,x
    rts

; Fall at up to 4 px a frame; C set on landing.
oz_fall:
    lda ob_vyi,x
    bpl @accel
    cmp #$fc
    bcc @move
@accel:
    lda ob_vyf,x
    sec
    sbc #$40
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
@move:
    jsr ob_move
    lda ob_vyi,x
    bpl @air
    jsr oz_on_ground
    bcc @air
    jsr oz_snap_ground
    sec
    rts
@air:
    clc
    rts

; --- Slots -----------------------------------------------------------------------------
; X = a free object, C set when there is none.
ob_alloc:
    ldx #0
:
    lda ob_kind,x
    beq @found
    inx
    cpx #ob_max
    bne :-
    sec
    rts
@found:
    lda #0
    sta ob_state,x
    sta ob_step,x
    sta ob_t0,x
    sta ob_t1,x
    sta ob_t2,x
    sta ob_xf,x
    sta ob_yf,x
    sta ob_flash,x
    sta ob_hp,x
    sta ob_var,x
    sta ob_flags,x
    sta ob_dir,x
    sta ob_aux,x
    sta ob_aux2,x
    sta ob_carry,x
    sta ob_mirror,x
    sta ob_cur_l,x
    sta ob_cur_h,x
    sta ob_nxt_l,x
    sta ob_nxt_h,x
    sta ob_cnt,x
    jsr ob_stop
    lda #$ff
    sta ob_src,x
    clc
    rts

; Count the objects of kind A: Y = count.
oz_count_kind:
    sta oz_t0
    ldy #0
    ldx #ob_max - 1
:
    lda ob_kind,x
    cmp oz_t0
    bne :+
    iny
:
    dex
    bpl :--
    rts

; Spawn kind A with variant Y at (oz_t0/oz_t1 = X, oz_t2/oz_t3 = Y). X = the object or
; C set. Every sixth enemy carries a pot ($EB02).
oz_spawn:
    pha
    tya
    pha
    jsr ob_alloc
    pla
    tay
    pla
    bcs @none
    sta ob_kind,x
    tya
    sta ob_var,x
    lda oz_t0
    sta ob_xl,x
    lda oz_t1
    sta ob_xh,x
    lda oz_t2
    sta ob_yl,x
    lda oz_t3
    sta ob_yh,x
    ldy ob_kind,x
    lda ok_hp,y
    sta ob_hp,x
    cpy #ok_magician + 1
    bcs @done
    cpy #ok_unicorn
    beq @done
    cpy #ok_arremer
    beq @done
    cpy #ok_plant
    beq @done
    inc oz_spawns
    lda oz_spawns
    cmp #6
    bcc @done
    lda #0
    sta oz_spawns
    lda #1
    sta ob_carry,x
@done:
    clc
@none:
    rts

; Remove objects far from the camera ($8DC5: X outside camera X - 128 to + 383).
oz_range_check:
    lda ob_kind,x
    cmp #ok_unicorn
    beq @keep
    cmp #ok_arremer
    beq @keep
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    ; In range when -128 <= X - camera < 384: add 128, then 0 to 511.
    lda oz_t0
    clc
    adc #128
    lda oz_t1
    adc #0
    cmp #2
    bcc @keep
    lda #0
    sta ob_kind,x
@keep:
    rts

; --- Placement -------------------------------------------------------------------------
; The group of the page right of the camera starts its patterns (the arcade reads the
; next page's group as the camera reaches it); one page a frame.
oz_register_pages:
    lda camera_high
    cmp #8
    bcs @use_cur_page           ; pages 9-12 use world X; register when camera enters them
    clc
    adc #1
@use_cur_page:
    cmp #revg_page_count
    bcc :+
    lda #revg_page_count - 1
:
    cmp oz_reg_page
    beq @done
    bmi @done
    inc oz_reg_page
    lda oz_reg_page
    asl a
    asl a
    asl a
    clc
    adc oz_reg_page             ; 9 bytes a page
    tay
    lda #3
    sta oz_t3
@entry:
    lda revg_page_groups,y
    beq @done
    sec
    sbc #1
    tax                         ; scheduler
    lda revg_page_groups+1,y
    sta oz_sch_pat,x
    sty oz_t2
    tay
    lda revg_pat_start,y
    sta oz_sch_rec,x
    clc
    adc revg_pat_count,y
    sta oz_sch_end,x
    ldy oz_t2
    lda revg_page_groups+2,y
    sta oz_sch_once,x
    lda #1
    sta oz_sch_wait,x
    iny
    iny
    iny
    dec oz_t3
    bne @entry
@done:
    rts

; One placed enemy a frame: it comes in when the camera is within 64 px of it.
oz_place_fixed:
    ldy oz_fix_next
    iny
    cpy #revg_fix_count
    bcc :+
    ldy #0
:
    sty oz_fix_next
    ; Used already?
    tya
    and #7
    tax
    lda oz_bits,x
    sta oz_t0
    tya
    lsr a
    lsr a
    lsr a
    tax
    lda oz_fix_used,x
    and oz_t0
    bne @done
    ; X - camera X in -64 .. 319?
    lda revg_fix_x_lo,y
    sec
    sbc camera_low
    sta oz_t1
    lda revg_fix_x_hi,y
    sbc camera_high
    sta oz_t2
    jsr oz_in_spawn_window
    bcs @done
    stx oz_t3                   ; mark it used once it is placed
    lda oz_t0
    sta oz_t2
    lda revg_fix_x_lo,y
    sta oz_t0
    lda revg_fix_x_hi,y
    sta oz_t1
    lda oz_t2
    pha
    lda oz_t3
    pha
    lda revg_fix_y_lo,y
    sta oz_t2
    lda revg_fix_y_hi,y
    sta oz_t3
    lda revg_fix_kind,y
    pha
    lda revg_fix_var,y
    tay
    pla
    jsr oz_spawn
    pla
    tay
    pla
    bcs @done
    ora oz_fix_used,y
    sta oz_fix_used,y
@done:
    jsr oz_place_supports
    rts

oz_bits: .byte 1, 2, 4, 8, 16, 32, 64, 128

; oz_t1/oz_t2 = X - camera X: C clear when it is -64 to 319 (inside the range the
; objects are kept in, -128 to 383).
oz_in_spawn_window:
    lda oz_t1
    clc
    adc #64
    sta oz_t1
    lda oz_t2
    adc #0
    beq @in
    cmp #1
    bne @out
    lda oz_t1
    cmp #128
    rts
@in:
    clc
    rts
@out:
    sec
    rts

; The touch triggers come in the same way.
oz_place_supports:
    ldy #revg_sup_count - 1
@each:
    tya
    and #7
    tax
    lda oz_bits,x
    sta oz_t0
    tya
    lsr a
    lsr a
    lsr a
    tax
    lda oz_sup_used,x
    and oz_t0
    bne @next
    lda revg_sup_x_lo,y
    sec
    sbc camera_low
    sta oz_t1
    lda revg_sup_x_hi,y
    sbc camera_high
    sta oz_t2
    jsr oz_in_spawn_window
    bcs @next
    lda oz_t0
    ora oz_sup_used,x
    sta oz_sup_used,x
    sty oz_obj
    lda revg_sup_x_lo,y
    sta oz_t0
    lda revg_sup_x_hi,y
    sta oz_t1
    lda revg_sup_y_lo,y
    sta oz_t2
    lda #0
    sta oz_t3
    lda revg_sup_var,y
    tay
    lda #ok_trigger
    cpy #2
    bcc :+
    lda #ok_disc
:
    jsr oz_spawn
    bcs :+
    lda #0
    sta ob_carry,x
:
    ldy oz_obj
@next:
    dey
    bpl @each
    rts

; Each scheduler counts down its wait while its type has room, then puts the next record.
oz_run_schedulers:
    ldx #3
@sched:
    stx oz_obj
    lda oz_sch_pat,x
    bmi @next
    lda revg_sched_kind,x
    jsr oz_count_kind
    ldx oz_obj
    tya
    cmp oz_sch_limit,x
    bcs @next
    dec oz_sch_wait,x
    bne @next
    jsr oz_sched_spawn
@next:
    ldx oz_obj
    dex
    bpl @sched
    rts

; Put scheduler X's record: the zombie at camera X + 128 + X ($EBF1), the flags' bit 0
; clear adds the camera to the others.
oz_sched_spawn:
    ldy oz_sch_rec,x
    lda revg_rec_wait,y
    sta oz_sch_wait,x
    lda revg_rec_x_lo,y
    sta oz_t0
    lda revg_rec_x_hi,y
    sta oz_t1
    lda revg_rec_y_lo,y
    sta oz_t2
    lda revg_rec_y_hi,y
    sta oz_t3
    lda revg_sched_kind,x
    cmp #ok_zombie
    bne @not_zombie
    lda oz_t0
    clc
    adc #128
    sta oz_t0
    lda oz_t1
    adc #0
    sta oz_t1
    jsr @add_camera_x
    lda oz_py                   ; from the player's height, then down to the ground
    sta oz_t2
    lda oz_py+1
    sta oz_t3
    jmp @put
@not_zombie:
    lda oz_sch_once,x
    and #1
    beq @add_cam
    ; World-coordinate spawns (e.g. Stage 1 second-half forest ghosts on pages 9-11):
    ; hold the record until the camera is close enough that oz_range_check keeps it alive
    ; (X - camera X < 320). The unicorn boss is exempt from oz_range_check.
    lda revg_sched_kind,x
    cmp #ok_unicorn
    beq @put
    lda oz_t0
    sec
    sbc camera_low
    tay
    lda oz_t1
    sbc camera_high
    bmi @put                    ; behind or at camera
    beq @put                    ; 0..255 px ahead
    cmp #1
    bne @hold_world
    cpy #112                    ; < 368 px ahead (inside oz_range_check's 384 limit)?
    bcc @put
@hold_world:
    lda #8
    sta oz_sch_wait,x
    rts
@add_cam:
    jsr @add_camera_x
@put:
    ; The record's variant, then the next record; the pattern ends after its last.
    ldy oz_sch_rec,x
    lda revg_rec_var,y
    pha
    inc oz_sch_rec,x
    lda oz_sch_rec,x
    cmp oz_sch_end,x
    bcc :+
    lda #$ff
    sta oz_sch_pat,x
:
    pla
    tay
    lda revg_sched_kind,x
    jmp oz_spawn
@add_camera_x:
    lda oz_t0
    clc
    adc camera_low
    sta oz_t0
    lda oz_t1
    adc camera_high
    sta oz_t1
    rts

; --- Hits --------------------------------------------------------------------------------
; Shot ap_loop_idx against the objects (called from ap_update_shots). C set when the shot
; is spent. The axe passes through (the arcade's weapon 2).
ap_shot_check_enemies:
    ldx ap_loop_idx
    lda ps_x_lo,x
    sta oz_dx
    lda ps_x_hi,x
    sta oz_dx+1
    lda #254
    sec
    sbc ps_y,x
    sta oz_dy
    lda #0
    sbc ps_y_hi,x
    sta oz_dy+1
    ldx #0
@obj:
    lda ob_flags,x
    and #ofl_shootable
    beq @next
    lda ob_flash,x
    bne @next
    ; dX = object X - shot X
    ldy ob_kind,x
    lda ob_xl,x
    sec
    sbc oz_dx
    sta oz_t0
    lda ob_xh,x
    sbc oz_dx+1
    sta oz_t1
    lda oz_t0
    clc
    adc ok_sx,y
    sta oz_t0
    lda oz_t1
    adc #0
    bne @next
    lda ok_sx,y
    asl a
    cmp oz_t0
    bcc @next
    ; dY = object Y - shot Y within ok_sylo..ok_syhi
    lda ob_yl,x
    sec
    sbc oz_dy
    sta oz_t0
    lda ob_yh,x
    sbc oz_dy+1
    sta oz_t1
    lda oz_t0
    sec
    sbc ok_sylo,y
    sta oz_t0
    lda oz_t1
    sbc #$ff
    bne @next
    lda ok_syhi,y
    sec
    sbc ok_sylo,y
    cmp oz_t0
    bcc @next
    jmp oz_shot_hit
@next:
    inx
    cpx #ob_max
    bne @obj
    clc
    rts

; Shot ap_loop_idx hit object X.
oz_shot_hit:
    stx oz_obj
    lda ob_flags,x
    bpl @not_shield
    ; The knight's shield: the shot bounces off as a spark.
    ldx ap_loop_idx
    lda ps_x_lo,x
    sta oz_t0
    lda ps_x_hi,x
    sta oz_t1
    lda oz_dy
    sta oz_t2
    lda oz_dy+1
    sta oz_t3
    lda #ok_spark
    ldy #0
    jsr oz_spawn
    bcs :+
    lda #<rf_9C4B
    ldy #>rf_9C4B
    jsr ob_anim
    lda #18
    sta ob_t0,x
    lda #0
    sta ob_carry,x
:
    lda #snd_se_clank
    jsr call_bank8_play_se
    sec
    rts
@not_shield:
    ldx oz_obj
    lda #6
    sta ob_flash,x
    lda ob_kind,x
    cmp #ok_arremer
    bne :+
    lda ob_state,x              ; a sleeping Red Arremer wakes, the hit not counted
    bne :+
    lda #1
    sta ob_state,x
    lda #16
    sta ob_t0,x
    jmp @spent
:
    lda ob_hp,x
    beq @dies
    dec ob_hp,x
    lda #snd_se_hit
    jsr call_bank8_play_se
    jmp @spent
@dies:
    jsr oz_kill
@spent:
    ldx ap_loop_idx
    lda ps_kind,x
    cmp #ap_weapon_axe
    beq :+
    sec
    rts
:
    clc
    rts

; Object X is destroyed: points, then the explosion (and a pot's item at its end).
oz_kill:
    lda #snd_se_kill
    jsr call_bank8_play_se
    ldy ob_kind,x
    lda ok_points,y
    beq :+
    stx oz_obj
    jsr ap_add_score
    ldx oz_obj
:
    ldy ob_kind,x
    lda #<rf_5C54               ; small explosion, 18 frames ($E7A3)
    sta oz_t2
    lda #>rf_5C54
    sta oz_t3
    lda #18
    cpy #ok_zombie
    beq @set
    cpy #ok_crow
    beq @set
    lda #<rf_5CDE               ; the unicorn's
    sta oz_t2
    lda #>rf_5CDE
    sta oz_t3
    lda #21
    cpy #ok_unicorn
    beq @set
    lda #<rf_5CA0               ; large, 36 frames ($E7CE)
    sta oz_t2
    lda #>rf_5CA0
    sta oz_t3
    lda #36
@set:
    sta ob_t0,x
    lda #0
    sta ob_aux,x                ; the score picture to show after (0 none)
    cpy #ok_arremer
    bne :+
    lda #1
    sta ob_aux,x
:
    cpy #ok_unicorn
    bne :+
    lda #2
    sta ob_aux,x
    lda #1
    sta oz_key_timer            ; the key comes after the boss
:
    lda #ok_boom
    sta ob_kind,x
    lda #0
    sta ob_flags,x
    sta ob_flash,x
    sta ob_mirror,x
    jsr ob_stop
    lda oz_t2
    ldy oz_t3
    jmp ob_anim

; Enemies and their shots against the player.
oz_player_contact:
    lda ap_invuln
    beq :+
    rts
:
    lda ap_state
    cmp #ap_state_knockback
    bcc :+
    rts
:
    ldx #0
@obj:
    stx oz_obj
    lda ob_flags,x
    and #ofl_harmful
    beq @next
    jsr oz_rel_player
    ldy ob_kind,x
    ; |dX| <= px
    lda oz_dx
    clc
    adc ok_px,y
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @next
    lda ok_px,y
    asl a
    cmp oz_t0
    bcc @next
    ; pylo <= dY <= pyhi (6 px less above a crouching player)
    lda ok_pyhi,y
    sta oz_t1
    lda ap_crouch
    beq :+
    lda oz_t1
    sec
    sbc #6
    sta oz_t1
:
    lda oz_dy
    sec
    sbc ok_pylo,y
    sta oz_t0
    lda oz_dy+1
    sbc #$ff
    bne @next
    lda oz_t1
    sec
    sbc ok_pylo,y
    cmp oz_t0
    bcc @next
    ; Hit.
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    lda ob_kind,x
    cmp #ok_shot
    bne :+
    lda #0
    sta ob_kind,x               ; a shot is used up
:
    jmp ap_apply_player_damage
@next:
    ldx oz_obj
    inx
    cpx #ob_max
    bne @obj
@done:
    rts

; --- Items -------------------------------------------------------------------------------
; Item frames by arcade item kind (bank4:$5AD1): 0 armour, 1 torch, 2 lance, 3 axe,
; 4 cross, 5 dagger, 6 the section 0 bonus (200), 19 the 10,000 point bonus.
oz_item_anim_lo: .byte <rf_5B0C, <rf_5B12, <rf_5B20, <rf_5B32, <rf_5B40, <rf_5B4E, <rf_5B5C
oz_item_anim_hi: .byte >rf_5B0C, >rf_5B12, >rf_5B20, >rf_5B32, >rf_5B40, >rf_5B4E, >rf_5B5C
; The FC weapon for arcade kinds 1-5.
oz_item_weapon: .byte 0, ap_weapon_torch, ap_weapon_lance, ap_weapon_axe, ap_weapon_shield, ap_weapon_dagger

; The pot's item ($EB34): selections 2 and 6 of 0-6 are a weapon other than the one held,
; the rest the bonus.
oz_pick_drop:
    ldy oz_drop_sel
    iny
    cpy #7
    bcc :+
    ldy #0
:
    sty oz_drop_sel
    cpy #2
    beq @weapon
    cpy #6
    beq @weapon
    lda #6
    rts
@weapon:
    jsr oz_random
    and #3
    clc
    adc #1                      ; 1-4, then skip the weapon held
    sta oz_t0
    tay
    lda oz_item_weapon,y
    cmp ap_weapon
    bne :+
    lda #5
    sta oz_t0
:
    lda oz_t0
    rts

; Spawn item kind A at object X's place, falling.
oz_drop_item:
    pha
    lda ob_xl,x
    sta oz_t0
    lda ob_xh,x
    sta oz_t1
    lda ob_yl,x
    sta oz_t2
    lda ob_yh,x
    sta oz_t3
    pla
    tay
    lda #ok_item
    jsr oz_spawn
    bcs :+
    lda #0
    sta ob_carry,x
    jsr oz_item_set_anim
    clc
:
    rts

oz_item_set_anim:
    ldy ob_var,x
    cpy #19
    bne :+
    lda #<rf_5C12
    ldy #>rf_5C12
    jmp ob_anim
:
    cpy #7
    bcc :+
    ldy #6
:
    lda oz_item_anim_lo,y
    pha
    lda oz_item_anim_hi,y
    tay
    pla
    jmp ob_anim

oz_item:
    lda ob_state,x
    bne @landed
    jsr oz_fall
    bcc @out
    inc ob_state,x
@out:
    rts
@landed:
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #13
    bcs @out
    cpy #17
    bcs @out
    ; Taken.
    lda #0
    sta ob_kind,x
    ldy ob_var,x
    bne @not_armour
    lda #ap_armor_full
    sta ap_armor
    lda #snd_se_pickup
    jmp call_bank8_play_se
@not_armour:
    cpy #6
    bcs @bonus
    lda oz_item_weapon,y
    sta ap_weapon
    inc campaign_hud_dirty
    lda #snd_se_pickup
    jmp call_bank8_play_se
@bonus:
    lda #snd_se_pickup
    jsr call_bank8_play_se
    lda #2                      ; 200
    cpy #19
    bne :+
    lda #100                    ; 10,000
:
    jmp ap_add_score

; An explosion; then a score picture or a pot's item.
oz_boom:
    dec ob_t0,x
    bne @out
    lda ob_carry,x
    beq :+
    jsr oz_pick_drop
    pha
    lda #0
    sta ob_kind,x
    pla
    jmp oz_drop_item
:
    lda ob_aux,x
    beq @free
    lda #ok_popup
    sta ob_kind,x
    lda #63
    sta ob_t0,x
    lda ob_aux,x
    cmp #2
    beq :+
    lda #<rf_E878               ; 1000
    ldy #>rf_E878
    jmp ob_anim
:
    lda #<rf_E87C               ; 2000
    ldy #>rf_E87C
    jmp ob_anim
@free:
    lda #0
    sta ob_kind,x
@out:
    rts

oz_popup:
    dec ob_t0,x
    bne :+
    lda #0
    sta ob_kind,x
:
    rts

; Discs (support $00 variants 2 and 3): 200 or 500 points on touch (X +-12, Y +-8).
oz_disc:
    lda ob_state,x
    bne oz_popup
    lda ob_t1,x
    bne :+
    inc ob_t1,x
    lda ob_var,x
    cmp #3
    beq @five
    lda #<rf_5840
    ldy #>rf_5840
    jsr ob_anim
    jmp :+
@five:
    lda #<rf_584E
    ldy #>rf_584E
    jsr ob_anim
:
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #13
    bcs @out
    cpy #9
    bcs @out
    inc ob_state,x
    lda #31
    sta ob_t0,x
    lda #snd_se_pickup
    jsr call_bank8_play_se
    lda ob_var,x
    cmp #3
    beq :+
    lda #<rf_E864
    ldy #>rf_E864
    jsr ob_anim
    lda #2
    jmp ap_add_score
:
    lda #<rf_E870
    ldy #>rf_E870
    jsr ob_anim
    lda #5
    jmp ap_add_score
@out:
    rts

; Hidden triggers: variant 0 puts the armour 100 px right when Arthur has lost it,
; variant 1 the 10,000 point bonus 100 px right and 128 px up.
oz_trigger:
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #17
    bcs @out
    cpy #17
    bcs @out
    lda #0
    sta ob_kind,x
    lda ob_var,x
    bne @bonus
    lda ap_armor
    cmp #ap_armor_full
    beq @out
    lda #0
    jsr oz_drop_item
    bcs @out
    jsr oz_push_right
    lda #snd_se_rise
    jmp call_bank8_play_se
@bonus:
    lda #19
    jsr oz_drop_item
    bcs @out
    lda ob_yl,x
    clc
    adc #128
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    jsr oz_push_right
    lda #snd_se_rise
    jmp call_bank8_play_se
@out:
    rts

oz_push_right:
    lda ob_xl,x
    clc
    adc #100
    sta ob_xl,x
    bcc :+
    inc ob_xh,x
:
    rts

; The key falls after the unicorn, in the middle of the last screen, and ends the stage.
oz_key_wait:
    lda oz_key_timer
    beq @done
    inc oz_key_timer
    lda oz_key_timer
    cmp #100
    bne @done
    lda #0
    sta oz_key_timer
    lda #<(3328 + 128)
    sta oz_t0
    lda #>(3328 + 128)
    sta oz_t1
    lda #200
    sta oz_t2
    lda #0
    sta oz_t3
    tay
    lda #ok_key
    jsr oz_spawn
    bcs @done
    lda #0
    sta ob_carry,x
    lda #<rf_7430
    ldy #>rf_7430
    jmp ob_anim
@done:
    rts

oz_key:
    lda ob_state,x
    beq @falling
    cmp #1
    beq @landed
    rts                         ; state 2: held above Arthur during clear celebration
@falling:
    jsr oz_fall
    bcc @out
    inc ob_state,x
@out:
    rts
@landed:
    lda ap_state
    cmp #ap_state_air
    beq @out
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #13
    bcs @out
    cpy #17
    bcs @out
    ; Clear all other objects and shots, then raise the key above Arthur for the clear
    ; celebration and time-bonus tally.
    stx ap_clear_key_slot
    ldy #ob_max - 1
    lda #0
@clear_others:
    cpy ap_clear_key_slot
    beq :+
    sta ob_kind,y
:
    dey
    bpl @clear_others
    ldy #ap_max_shots - 1
@clear_shots:
    sta ps_active,y
    dey
    bpl @clear_shots
    ldx ap_clear_key_slot
    lda #2
    sta ob_state,x
    lda player_x
    sta ob_xl,x
    lda player_x+1
    sta ob_xh,x
    lda oz_py
    clc
    adc #28
    sta ob_yl,x
    sta ap_clear_key_y
    lda oz_py+1
    adc #0
    sta ob_yh,x
    lda #0
    sta player_left
    sta ap_crouch
    sta ap_shoot_lock
    lda #ap_state_ground
    sta ap_state
    lda #1
    sta ap_key_collected
    sta ap_clear_seq
    lda #90
    sta ap_clear_timer
    lda #20                     ; 2,000 pts for grabbing the key
    jsr ap_add_score
    lda #snd_song_clear
    jsr call_bank8_play_song
    lda #snd_se_key
    jmp call_bank8_play_se

; Stage 1 key-grab celebration animation and rapid time-bonus tally (runs in Bank 6).
revg_step_clear_seq:
    ; Keep Arthur in place facing right and bob the raised key above his head.
    ldx ap_clear_key_slot
    lda ap_clear_timer
    and #$08
    beq :+
    lda ap_clear_key_y
    clc
    adc #2
    bne @set_key_y
:
    lda ap_clear_key_y
@set_key_y:
    sta ob_yl,x
    lda ap_clear_seq
    cmp #1
    bne @clear_not_hold
    dec ap_clear_timer
    bne @clear_draw
    lda #2
    sta ap_clear_seq
    lda #2
    sta ap_clear_timer
    bne @clear_draw
@clear_not_hold:
    cmp #2
    bne @clear_final_hold
    dec ap_clear_timer
    bne @clear_draw
    lda #2
    sta ap_clear_timer
    ; Check if campaign_clock == 000.
    lda campaign_clock
    cmp #campaign_zero
    bne @tally_tick
    lda campaign_clock+1
    cmp #campaign_zero
    bne @tally_tick
    lda campaign_clock+2
    cmp #campaign_zero
    bne @tally_tick
    lda #3
    sta ap_clear_seq
    lda #45
    sta ap_clear_timer
    bne @clear_draw
@tally_tick:
    ; Decrement 3-digit HUD clock by 1 second and add 100 points.
    dec campaign_clock+2
    lda campaign_clock+2
    cmp #campaign_zero
    bcs @tally_score
    lda #campaign_zero + 9
    sta campaign_clock+2
    dec campaign_clock+1
    lda campaign_clock+1
    cmp #campaign_zero
    bcs @tally_score
    lda #campaign_zero + 9
    sta campaign_clock+1
    dec campaign_clock
@tally_score:
    lda #1
    jsr ap_add_score
    lda campaign_clock+2
    sec
    sbc #campaign_zero
    and #3
    bne @clear_draw
    lda #snd_se_pickup
    jsr call_bank8_play_se
    jmp @clear_draw
@clear_final_hold:
    dec ap_clear_timer
    bne @clear_draw
    lda #0
    sta ap_clear_seq
    jmp show_course_clear
@clear_draw:
    jsr draw_sprites
    jmp submit_frame

; --- Enemy shots -------------------------------------------------------------------------
; Spawn a shot at (oz_t0..oz_t3) aimed at the player at about 2.26 px (16 directions),
; animation A/Y. X = the shot.
oz_aimed_shot:
    pha
    tya
    pha
    lda #ok_shot
    ldy #0
    jsr oz_spawn
    pla
    tay
    pla
    bcs @none
    jsr ob_anim
    lda #ofl_harmful
    sta ob_flags,x
    lda #0
    sta ob_carry,x
    jsr oz_aim
    lda #snd_se_efire
    jsr call_bank8_play_se
    clc
@none:
    rts

; Aim object X at the player: 2.26 px split between X and Y by the larger distance.
oz_aim:
    jsr oz_rel_player
    jsr oz_abs_d
    sta oz_t0                   ; |dx|
    sty oz_t1                   ; |dy|
    ; Speeds: the larger axis 2.26, the other 2.26 * small / large (rounded to 16ths).
    lda oz_t0
    cmp oz_t1
    bcs @x_major
    ; Y major
    lda #$02
    sta ob_vyi,x
    lda #$42
    sta ob_vyf,x
    lda oz_t0
    ldy oz_t1
    jsr oz_ratio16
    sta ob_vxf,x
    lda #0
    sta ob_vxi,x
    jmp @signs
@x_major:
    lda #$02
    sta ob_vxi,x
    lda #$42
    sta ob_vxf,x
    lda oz_t1
    ldy oz_t0
    jsr oz_ratio16
    sta ob_vyf,x
    lda #0
    sta ob_vyi,x
@signs:
    ; Toward the player: X sign of oz_dx, Y opposite to oz_dy.
    lda oz_dx+1
    bpl :+
    jsr ob_neg_vx
:
    lda oz_dy+1
    bmi :+
    jsr ob_neg_vy
:
    rts

; A = small, Y = large -> A = 2.26 * small / large as a fraction (x256, up to 255).
oz_ratio16:
    sta oz_t2
    sty oz_t3
    ldy #0
    lda oz_t2
    asl a
    asl a                       ; small * 4 (/ large) -> quarters, roughly *2.26/2.26
:
    cmp oz_t3
    bcc :+
    sbc oz_t3
    iny
    cpy #4
    bne :-
:
    tya
    asl a
    asl a
    asl a
    asl a
    asl a
    asl a                       ; quarters -> 64ths of 256 (0, 64, 128, 192, 255)
    bcc :+
    lda #$ff
:
    ora #$20
    rts

ob_neg_vx:
    lda #0
    sec
    sbc ob_vxf,x
    sta ob_vxf,x
    lda #0
    sbc ob_vxi,x
    sta ob_vxi,x
    rts

ob_neg_vy:
    lda #0
    sec
    sbc ob_vyf,x
    sta ob_vyf,x
    lda #0
    sbc ob_vyi,x
    sta ob_vyi,x
    rts

; Shots fly straight and go when they leave the screen.
oz_shot:
    jsr ob_move
    lda ob_xl,x
    sec
    sbc camera_low
    lda ob_xh,x
    sbc camera_high
    bne @gone
    lda ob_yh,x
    bne @gone
    lda ob_yl,x
    cmp #240
    bcs @gone
    rts
@gone:
    lda #0
    sta ob_kind,x
    rts

; --- Zombie (type $0B) ---------------------------------------------------------------------
; States: 0 coming down to the ground, 1 rising (87 frames), 2 walking, 3 sinking (36).
oz_zombie:
    lda ob_state,x
    bne @not_down
    ; From the player's height down to the ground, 2 px a frame.
    jsr oz_on_ground
    bcs @grounded
    lda ob_yl,x
    sec
    sbc #2
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda ob_yh,x
    bpl :+
    lda #0                      ; no ground: gone
    sta ob_kind,x
:
    rts
@grounded:
    jsr oz_snap_ground
    jsr oz_face_player
    lda ob_var,x
    beq @rise
    jmp oz_zombie_walk_start
@rise:
    lda #snd_se_rise
    jsr call_bank8_play_se
    lda #1
    sta ob_state,x
    lda #87
    sta ob_t0,x
    lda #<rf_5272
    ldy #>rf_5272
    sta oz_t2
    sty oz_t3
    lda #<rf_523C               ; the $523C series faces right ($526D goes on to the right walk $52FC)
    ldy #>rf_523C
    jmp ob_anim_dir
@not_down:
    cmp #1
    bne @not_rising
    dec ob_t0,x
    lda ob_t0,x
    cmp #57
    bne :+
    lda #<rf_527B
    ldy #>rf_527B
    sta oz_t2
    sty oz_t3
    lda #<rf_5245
    ldy #>rf_5245
    jsr ob_anim_dir
:
    lda ob_t0,x
    cmp #32
    bne :+
    lda #<rf_5283
    ldy #>rf_5283
    sta oz_t2
    sty oz_t3
    lda #<rf_524D
    ldy #>rf_524D
    jsr ob_anim_dir
:
    lda ob_t0,x
    bne :+
    jmp oz_zombie_walk_start
:
    rts
@not_rising:
    cmp #2
    bne @sinking
    ; Walk at 0.81 px; after passing the player walk on for 40-110 frames, then sink.
    jsr ob_move
    lda ob_aux,x
    bne @past
    jsr oz_rel_player
    lda ob_dir,x
    beq :+
    lda oz_dx+1                 ; walking left: past when the player is right of it
    bmi @walk_on
    bpl @passed
:
    lda oz_dx+1
    bpl @walk_on
@passed:
    inc ob_aux,x
    jsr oz_random
    and #63
    clc
    adc #40
    sta ob_t0,x
    rts
@walk_on:
    lda rs_frame
    and #1
    bne :+
    dec ob_t1,x                 ; the 180-step walk time ($0A)
    beq @sink
:
    ; The ground ends: sink.
    jsr oz_on_ground
    bcc @sink
    rts
@past:
    dec ob_t0,x
    beq @sink
    jsr oz_on_ground
    bcc @sink
    rts
@sink:
    lda #3
    sta ob_state,x
    lda #36
    sta ob_t0,x
    lda #ofl_harmful
    sta ob_flags,x
    lda #0
    sta ob_flags,x
    jsr ob_stop
    lda #<rf_5295
    ldy #>rf_5295
    sta oz_t2
    sty oz_t3
    lda #<rf_525F
    ldy #>rf_525F
    jmp ob_anim_dir
@sinking:
    dec ob_t0,x
    lda ob_t0,x
    cmp #18
    bne :+
    lda #<rf_529D
    ldy #>rf_529D
    sta oz_t2
    sty oz_t3
    lda #<rf_5267
    ldy #>rf_5267
    jsr ob_anim_dir
:
    lda ob_t0,x
    bne :+
    sta ob_kind,x
:
    rts

oz_zombie_walk_start:
    lda #2
    sta ob_state,x
    lda #0
    sta ob_aux,x
    lda #180
    sta ob_t1,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #$cf                    ; 0.81 px
    ldy #0
    jsr ob_set_vx_dir
    lda #<rf_5316
    ldy #>rf_5316
    sta oz_t2
    sty oz_t3
    lda #<rf_52FC
    ldy #>rf_52FC
    jmp ob_anim_dir

; A/Y = the left animation, oz_t2/oz_t3 = the right one: by ob_dir.
oz_anim_lr:
    pha
    lda ob_dir,x
    bne :+
    pla
    lda oz_t2
    ldy oz_t3
    jmp ob_anim
:
    pla
    jmp ob_anim

; --- Crow (type $01, variant 5) --------------------------------------------------------
; 0 waiting for the screen, 1 sitting (looks at the player, flies off after its time),
; 2 flying 1.25 px with the swing.
oz_crow_sit_time: .byte 80, 180, 125, 160, 60, 10, 110, 165
oz_crow_amp: .byte 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 16, 12, 8, 14, 6, 10

oz_crow:
    lda ob_state,x
    bne @not_wait
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr oz_random
    and #7
    tay
    lda oz_crow_sit_time,y
    sta ob_t0,x
    lda #1
    sta ob_state,x
    jsr oz_face_player
    jmp @pose
@not_wait:
    cmp #1
    beq @far1794
    jmp @flying
@far1794:
    ; Turn to the player; a new pose when turning or every 55-75 frames.
    lda ob_dir,x
    pha
    jsr oz_face_player
    pla
    cmp ob_dir,x
    bne @pose
    dec ob_t1,x
    bne :+
@pose:
    jsr oz_random
    and #15
    clc
    adc #55
    sta ob_t1,x
    jsr oz_random
    and #1
    beq @pose_a
    lda #<rf_4A0C               ; right (the arcade draws $4A0C mirrored)
    ldy #>rf_4A0C
    sta oz_t2
    sty oz_t3
    lda #<rf_4A4A               ; left
    ldy #>rf_4A4A
    jsr oz_anim_lr
    jmp :+
@pose_a:
    lda #<rf_4A04
    ldy #>rf_4A04
    sta oz_t2
    sty oz_t3
    lda #<rf_4A42
    ldy #>rf_4A42
    jsr oz_anim_lr
:
    dec ob_t0,x
    bne @out
    ; Fly off the way it faces.
    lda #2
    sta ob_state,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #16
    sta ob_aux2,x
    lda #0
    sta ob_aux,x
    lda #$40
    ldy #1
    jsr ob_set_vx_dir
    lda #<rf_4A2E
    ldy #>rf_4A2E
    sta oz_t2
    sty oz_t3
    lda #<rf_4A6C
    ldy #>rf_4A6C
    jmp oz_anim_lr
@out:
    rts
@flying:
    jsr ob_move
    ; Y = base + amplitude * sin, half a period in about 21 frames.
    lda ob_aux,x
    clc
    adc #6
    sta ob_aux,x
    bcc :+
    jsr oz_random
    and #15
    tay
    lda oz_crow_amp,y
    sta ob_aux2,x
:
    lda ob_aux,x
    ldy ob_aux2,x
    jsr oz_swing
    jmp oz_set_y_from_base

; ob_y = base + oz_mul.
oz_set_y_from_base:
    lda ob_by_l,x
    clc
    adc oz_mul
    sta ob_yl,x
    lda ob_by_h,x
    adc oz_mul+1
    sta ob_yh,x
    rts

; --- Man-eating plant (type $11) ------------------------------------------------------
oz_plant_waits: .byte 65, 70, 8, 80, 8, 55, 60, 85, 80, 90, 33, 73, 75, 90, 45, 9, 79, 29, 80, 45, 53

oz_plant:
    lda ob_state,x
    bne @not_init
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #1
    sta ob_state,x
    lda oz_plant_waits
    sta ob_t0,x
    lda #0
    sta ob_aux,x
@idle_anim:
    jsr oz_face_player
    lda #<rf_56E5
    ldy #>rf_56E5
    sta oz_t2
    sty oz_t3
    lda #<rf_56D7
    ldy #>rf_56D7
    jmp oz_anim_lr
@not_init:
    cmp #1
    bne @opening
    ; Count while on screen.
    lda ob_xl,x
    sec
    sbc camera_low
    lda ob_xh,x
    sbc camera_high
    bne @out
    lda ob_dir,x
    pha
    jsr oz_face_player
    pla
    cmp ob_dir,x
    beq :+
    jsr @idle_anim
:
    dec ob_t0,x
    bne @out
    lda #2
    sta ob_state,x
    lda #24
    sta ob_t0,x
    lda #<rf_5709
    ldy #>rf_5709
    sta oz_t2
    sty oz_t3
    lda #<rf_56F7
    ldy #>rf_56F7
    jmp oz_anim_lr
@out:
    rts
@opening:
    dec ob_t0,x
    bne @out
    ; Shoot (type 6) at (X - 1, Y + 5) and read the next wait.
    lda ob_xl,x
    sec
    sbc #1
    sta oz_t0
    lda ob_xh,x
    sbc #0
    sta oz_t1
    lda ob_yl,x
    clc
    adc #5
    sta oz_t2
    lda ob_yh,x
    adc #0
    sta oz_t3
    stx oz_obj
    lda #<rf_FD7C
    ldy #>rf_FD7C
    jsr oz_aimed_shot
    ldx oz_obj
    inc ob_aux,x
    ldy ob_aux,x
    cpy #21
    bcc :+
    ldy #0
    tya
    sta ob_aux,x
:
    lda oz_plant_waits,y
    sta ob_t0,x
    lda #1
    sta ob_state,x
    jmp @idle_anim

; --- Woody Pig? (type $00) --------------------------------------------------------------
; Flies across at 1.55 px; every 16 frames may loop toward the player or, when above
; the player and more than 16 px away, stop and shoot (26 frames, then 10 + 10).
oz_pig:
    lda ob_state,x
    bne @not_init
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr oz_face_player
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
@fly_set:
    lda #$8d                    ; 1.55 px
    ldy #1
    jsr ob_set_vx_dir
    lda #<rf_4974
    ldy #>rf_4974
    sta oz_t2
    sty oz_t3
    lda #<rf_4966
    ldy #>rf_4966
    jmp oz_anim_lr
@not_init:
    cmp #1
    beq @flying
    jmp @shooting
@flying:
    jsr ob_move
    ; The loop (a dive toward the player's height): a swing of the base Y.
    lda ob_aux,x
    beq :+
    clc
    adc #8
    sta ob_aux,x
    ldy #24
    jsr oz_swing
    jsr oz_set_y_from_base
:
    inc ob_t1,x
    lda ob_t1,x
    and #15
    bne @out
    jsr oz_rel_player
    jsr oz_abs_d
    sta ob_aux2,x
    ; Shoot when above the player and more than 16 px away, one shot at a time.
    cmp #17
    bcc @no_shot
    lda oz_dy+1
    bmi @no_shot
    lda #ok_shot
    jsr oz_count_kind
    ldx oz_obj
    cpy #1
    bcs @no_shot
    jsr oz_random
    and #3
    bne @no_shot
    lda #2
    sta ob_state,x
    lda #26
    sta ob_t0,x
    jsr ob_stop
    jsr oz_face_player
    lda #<rf_49A0
    ldy #>rf_49A0
    sta oz_t2
    sty oz_t3
    lda #<rf_4992
    ldy #>rf_4992
    jmp oz_anim_lr
@no_shot:
    ; Turn back toward the player when far past it.
    lda ob_aux2,x
    cmp #128
    bcc :+
    lda ob_dir,x
    pha
    jsr oz_face_player
    pla
    cmp ob_dir,x
    beq :+
    jsr @fly_set
:
    lda ob_aux2,x
    cmp #96
    bcs @out
    lda ob_aux,x
    bne @out
    jsr oz_random
    and #1
    beq @out
    lda #8
    sta ob_aux,x
@out:
    rts
@shooting:
    dec ob_t0,x
    bne @out
    inc ob_state,x
    lda ob_state,x
    cmp #3
    bne :+
    lda #10
    sta ob_t0,x
    ; The shot (type 4): along the longer axis at about 2.27 px.
    lda ob_xl,x
    sta oz_t0
    lda ob_xh,x
    sta oz_t1
    lda ob_yl,x
    sta oz_t2
    lda ob_yh,x
    sta oz_t3
    stx oz_obj
    lda #<rf_FD19
    ldy #>rf_FD19
    jsr oz_aimed_shot
    bcs @shot_done
    jsr oz_rel_player
    jsr oz_abs_d
    sty oz_t1
    cmp oz_t1
    bcs @shot_done
    lda #<rf_FD20               ; vertical
    ldy #>rf_FD20
    jsr ob_anim
@shot_done:
    ldx oz_obj
    rts
:
    lda #1
    sta ob_state,x
    jmp @fly_set

; --- Flying knight (type $03) --------------------------------------------------------------
; Left at 1.06 px on a swing of 50 px (86 frames a period); the shield holds while it is
; right of the player.
oz_knight:
    lda ob_state,x
    bne @flying
    lda #1
    sta ob_state,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #1
    sta ob_dir,x
    lda #$10                    ; 1.06 px
    ldy #1
    jsr ob_set_vx_dir
    lda #<rf_AEB5
    ldy #>rf_AEB5
    jsr ob_anim
@flying:
    jsr ob_move
    lda ob_aux,x
    clc
    adc #3
    sta ob_aux,x
    ldy #50
    jsr oz_swing
    jsr oz_set_y_from_base
    ; Shield while X >= the player's X.
    lda #ofl_shootable | ofl_harmful | ofl_shield
    tay
    lda ob_xl,x
    cmp player_x
    lda ob_xh,x
    sbc player_x+1
    bcs :+
    ldy #ofl_shootable | ofl_harmful
:
    tya
    sta ob_flags,x
    ; The two frames of the swing: rising and falling.
    lda ob_aux,x
    cmp #3
    bne :+
    lda #<rf_AEB5
    ldy #>rf_AEB5
    jmp ob_anim
:
    cmp #129
    bne :+
    lda #<rf_AEBF
    ldy #>rf_AEBF
    jmp ob_anim
:
    rts

; --- Red Arremer (type $05) -------------------------------------------------------------
; docs/revision-g-enemy-behaviour.md. States (ob_state): 0 sleeping until the player is
; within 96 px (X) and 48 px (Y), 1 waking (16 frames, $0001), 2 walking ($0100), 3
; shooting ($0102), 4 flying ($0101), 5 coming down to the ground, 6 keeping its distance
; ($0103), 7 dodging a shot ($0105), 8 the level rush and 9 the swoop ($0106), 10 powering
; up ($0104). Three hits.
; Fields: ob_t0 fire timer (+$06), ob_t1 rush timer (+$07), ob_t2 state timer, ob_aux fly
; count (+$08, every 8 frames), ob_aux2 its table index, ob_by_l the ground Y, ob_by_h the
; power-up count (+$09, every 32 frames), ob_var the power stage (+$0D), ob_step the phase.
oz_arremer_fire: .byte 90, 100, 220, 130, 160, 180, 200, 120
oz_arremer_rush: .byte 72, 136, 85, 92, 108, 144, 70, 49
oz_arremer_flyt: .byte 10, 20, 30, 40, 50, 60
; By the power stage: fly 1.75 / 2.0, distance 2.13 / 2.25, dodge 1.13 / 1.38, rush
; 2.13 / 2.38 px (8.8).
oz_arremer_fly_v:   .byte $c0, $00
oz_arremer_fly_vi:  .byte 1, 2
oz_arremer_dist_v:  .byte $22, $40
oz_arremer_dodge_v: .byte $21, $61
oz_arremer_rush_v:  .byte $22, $61

oz_arremer:
    lda ob_state,x
    bne @not_sleeping
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda ob_t2,x
    bne :+
    inc ob_t2,x
    lda #<rf_4BAC
    ldy #>rf_4BAC
    jsr ob_anim
:
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #97
    bcs @out
    cpy #49
    bcs @out
    lda #1
    sta ob_state,x
    lda #16
    sta ob_t0,x
@out:
    rts
@not_sleeping:
    cmp #1
    bne @awake
    dec ob_t0,x
    bne @out
    ; $0003: three hits, the timers from the table of difficulty row 0.
    lda #2
    sta ob_hp,x
    lda oz_arremer_fire
    sta ob_t0,x
    lda oz_arremer_rush
    sta ob_t1,x
    lda oz_arremer_flyt
    sta ob_aux,x
    lda #1
    sta ob_aux2,x
    lda #50                     ; 50 x 32 = 1,600 frames
    sta ob_by_h,x
    lda #0
    sta ob_var,x
    lda ob_yl,x
    sta ob_by_l,x
    jmp oz_arremer_walk
@awake:
    asl a
    tay
    lda @states-3,y
    pha
    lda @states-4,y
    pha
    rts
@states:
    .word oz_arremer_walking - 1, oz_arremer_shoot - 1, oz_arremer_fly - 1
    .word oz_arremer_down - 1, oz_arremer_distance - 1, oz_arremer_dodge - 1
    .word oz_arremer_rush_level - 1, oz_arremer_swoop - 1, oz_arremer_power - 1

; --- walking ($0100) and its checks ---
oz_arremer_walking:
    lda ob_flash,x
    bne :+                      ; hit: no move for the hit pose
    jsr ob_move
:
    jsr oz_face_player_keep
    jsr oz_arremer_speed
    ; Fire (+$06).
    dec ob_t0,x
    bne :+
    jmp oz_arremer_start_shoot
:
    ; Rush (+$07).
    dec ob_t1,x
    bne :+
    jsr oz_random
    and #7
    tay
    lda oz_arremer_rush,y
    sta ob_t1,x
    jmp oz_arremer_start_rush
:
    ; Fly (+$08, every 8 frames).
    lda rs_frame
    and #7
    bne :+
    dec ob_aux,x
    bne :+
    ldy ob_aux2,x
    lda oz_arremer_flyt,y
    sta ob_aux,x
    iny
    cpy #6
    bcc @idx
    ldy #0
@idx:
    tya
    sta ob_aux2,x
    jmp oz_arremer_takeoff
:
    ; Power up (+$09, every 32 frames, stage 0 only).
    lda ob_var,x
    bne :+
    lda rs_frame
    and #31
    bne :+
    dec ob_by_h,x
    bne :+
    jmp oz_arremer_start_power
:
    ; Dodge: a shot coming at it on the player's side.
    jsr oz_arremer_threat
    bcc :+
    jmp oz_arremer_start_dodge
:
    ; Distance: outside 48-112 px.
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #48
    bcc :+
    cmp #113
    bcc @done
:
    jmp oz_arremer_start_distance
@done:
    rts

; Walk toward ob_dir at 0.80 px.
oz_arremer_speed:
    lda #$cd
    ldy #0
    jmp ob_set_vx_dir

oz_face_player_keep:
    lda ob_dir,x
    pha
    jsr oz_face_player
    pla
    cmp ob_dir,x
    beq :+
    jmp oz_arremer_walk_anim
:
    rts

oz_arremer_walk:
    lda #2
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #0
    sta ob_vyf,x
    sta ob_vyi,x
    jsr oz_face_player
oz_arremer_walk_anim:
    lda #<rf_4DBB
    ldy #>rf_4DBB
    sta oz_t2
    sty oz_t3
    lda #<rf_4DA7
    ldy #>rf_4DA7
    jmp oz_anim_lr

; C set when a player shot is within 64 px on the player's side of it and near its
; height (the shot's FC Y within 24 px of 12 px above its feet).
oz_arremer_threat:
    lda ob_yl,x
; A = the Y to measure the shot's height against (the ground while floating, so that it
; stays up until the shot has gone by).
oz_arremer_threat_at:
    pha
    jsr oz_rel_player
    jsr oz_abs_d
    sta oz_t3
    pla
    sta oz_t2
    lda oz_t3
    cmp #32
    bcc @no                     ; close to the player: no dodging
    lda #254 - 12
    sec
    sbc oz_t2
    sta oz_t2                   ; FC Y of its middle (feet 254 - Y, 12 up)
    ldy #ap_max_shots - 1
@shot:
    lda ps_active,y
    beq @next
    cmp #ap_shot_spent
    beq @next
    lda ps_y_hi,y
    bne @next
    lda ps_y,y
    sec
    sbc oz_t2
    clc
    adc #24
    cmp #48
    bcs @next
    ; sdx = shot X - its X: on the player's side and within 64 px.
    lda ps_x_lo,y
    sec
    sbc ob_xl,x
    sta oz_t0
    lda ps_x_hi,y
    sbc ob_xh,x
    sta oz_t1
    eor oz_dx+1
    bmi @next                   ; the other side
    lda oz_t1
    bpl :+
    lda oz_t0
    eor #$ff
    clc
    adc #1
    sta oz_t0
    lda oz_t1
    eor #$ff
    adc #0
    sta oz_t1
:
    lda oz_t1
    bne @next
    lda oz_t0
    cmp #64
    bcs @next
    sec
    rts
@next:
    dey
    bpl @shot
@no:
    clc
    rts

; --- shooting ($0102): 6 frames, the shot, 6 frames ---
oz_arremer_start_shoot:
    lda #3
    sta ob_state,x
    lda #12
    sta ob_t2,x
    jsr ob_stop
    lda #<rf_4C58
    ldy #>rf_4C58
    sta oz_t2
    sty oz_t3
    lda #<rf_4C53
    ldy #>rf_4C53
    jmp oz_anim_lr

oz_arremer_shoot:
    dec ob_t2,x
    lda ob_t2,x
    cmp #6
    bne :+
    lda ob_xl,x
    sta oz_t0
    lda ob_xh,x
    sta oz_t1
    lda ob_yl,x
    clc
    adc #16
    sta oz_t2
    lda ob_yh,x
    adc #0
    sta oz_t3
    stx oz_obj
    lda #<rf_FDD7
    ldy #>rf_FDD7
    jsr oz_aimed_shot
    ldx oz_obj
    rts
:
    lda ob_t2,x
    bne :+
    jsr oz_random
    and #7
    tay
    lda oz_arremer_fire,y
    sta ob_t0,x
    jmp oz_arremer_walk
:
    rts

; --- flying ($0101): up 56-88 px in 21 frames, at the player, 80 px past, 5 frames ---
oz_arremer_takeoff:
    lda #4
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    jsr oz_face_player
    jsr ob_stop
    jsr oz_random
    and #$7f
    clc
    adc #$ab
    sta ob_vyf,x
    lda #2
    adc #0
    sta ob_vyi,x                ; 2.67-3.17 px: 56-67 px
    jsr oz_random
    bpl :+
    inc ob_vyi,x                ; or 3.67-4.17 px: 77-88 px
:
oz_arremer_fly_anim:
    lda #<rf_4E09
    ldy #>rf_4E09
    sta oz_t2
    sty oz_t3
    lda #<rf_4DFF
    ldy #>rf_4DFF
    jmp oz_anim_lr

oz_arremer_fly:
    jsr ob_move
    lda ob_step,x
    bne @glide
    dec ob_t2,x
    bne @out
    ; At the player at 1.75 px (2.0 powered up).
    inc ob_step,x
    lda #0
    sta ob_vyf,x
    sta ob_vyi,x
    jsr oz_face_player
    ldy ob_var,x
    lda oz_arremer_fly_v,y
    pha
    lda oz_arremer_fly_vi,y
    tay
    pla
    jsr ob_set_vx_dir
    lda #160                    ; a limit, in case the player keeps ahead
    sta ob_t2,x
    jmp oz_arremer_fly_anim
@out:
    rts
@glide:
    cmp #2
    beq @wait
    dec ob_t2,x
    beq @stop
    ; 80 px past the player: stop.
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #80
    bcc @out
    lda ob_dir,x
    eor #1                      ; flying right (0): past when the player is left
    beq :+
    lda oz_dx+1
    bpl @out
    bmi @stop
:
    lda oz_dx+1
    bmi @out
@stop:
    lda #2
    sta ob_step,x
    lda #5
    sta ob_t2,x
    jmp ob_stop
@wait:
    dec ob_t2,x
    bne @out
    jmp oz_arremer_start_down

; --- down to the ground in 16 frames ---
oz_arremer_start_down:
    lda #5
    sta ob_state,x
    lda #16
    sta ob_t2,x
    jsr ob_stop
    ; vy = -(Y - ground) / 16 (8.8)
    lda ob_yl,x
    sec
    sbc ob_by_l,x
    bcs :+
    lda #0
:
    sta oz_t0
    lda #0
    sta oz_t1
    ldy #4
:
    asl oz_t0
    rol oz_t1
    dey
    bne :-
    lda #0
    sec
    sbc oz_t0
    sta ob_vyf,x
    lda #0
    sbc oz_t1
    sta ob_vyi,x
    jmp oz_arremer_fly_anim

oz_arremer_down:
    jsr ob_move
    dec ob_t2,x
    bne :+
    lda ob_by_l,x
    sta ob_yl,x
    lda #0
    sta ob_yh,x
    sta ob_yf,x
    jmp oz_arremer_walk
:
    rts

; --- keeping its distance ($0103): away within 80 px, closer beyond, 7-27 frames ---
oz_arremer_start_distance:
    lda #6
    sta ob_state,x
    jsr oz_random
    and #15
    sta oz_t0
    jsr oz_random
    and #3
    clc
    adc oz_t0
    adc #7
    sta ob_t2,x
    jsr oz_face_player
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #81
    bcs :+
    lda ob_dir,x
    eor #1
    sta ob_dir,x
:
    ldy ob_var,x
    lda oz_arremer_dist_v,y
    ldy #2
    jsr ob_set_vx_dir
    jmp oz_arremer_walk_anim

oz_arremer_distance:
    jsr ob_move
    dec ob_t2,x
    bne :+
    jmp oz_arremer_walk
:
    rts

; --- dodging ($0105): up for 21 frames, up to 64 floating, then down ---
oz_arremer_start_dodge:
    lda #7
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    jsr ob_stop
    ldy ob_var,x
    lda oz_arremer_dodge_v,y
    sta ob_vyf,x
    lda #1
    sta ob_vyi,x
    jmp oz_arremer_fly_anim

oz_arremer_dodge:
    jsr ob_move
    dec ob_t2,x
    bne @out
    lda ob_step,x
    bne @floating
    inc ob_step,x
    lda #64
    sta ob_t2,x
    jmp ob_stop
@floating:
    jmp oz_arremer_start_down
@out:
    lda ob_step,x
    beq :+
    lda ob_by_l,x
    jsr oz_arremer_threat_at    ; floating: down once the shot has gone by
    bcs :+
    jmp oz_arremer_start_down
:
    rts

; --- the rush ($0106) ---
oz_arremer_start_rush:
    jsr oz_face_player
    jsr oz_rel_player
    jsr oz_abs_d
    lda oz_dy+1
    bmi @level                  ; below the player
    cpy #17
    bcs @swoop
@level:
    ; Within 16 px of the player's height: across at 2.13 px to 64 px past.
    lda #8
    sta ob_state,x
    lda #120
    sta ob_t2,x
    ldy ob_var,x
    lda oz_arremer_rush_v,y
    ldy #2
    jsr ob_set_vx_dir
    jmp oz_arremer_fly_anim
@swoop:
    ; Higher: down to where the player is in about 43 frames, then up as far on the
    ; other side (v = d x 6 / 256).
    lda #9
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #43
    sta ob_t2,x
    lda oz_dx
    ldy oz_dx+1
    jsr oz_mul6
    sta ob_vxi,x
    lda oz_t0
    sta ob_vxf,x
    lda #0
    sec
    sbc oz_dy
    pha
    lda #0
    sbc oz_dy+1
    tay
    pla
    jsr oz_mul6
    sta ob_vyi,x
    lda oz_t0
    sta ob_vyf,x
    jmp oz_arremer_fly_anim

; A/Y = signed 16-bit d: oz_t0 = low and A = high byte of d x 6 (8.8 speed of d/42.7).
oz_mul6:
    sta oz_t0
    sty oz_t1
    asl oz_t0
    rol oz_t1                   ; 2d
    lda oz_t0
    sta oz_t2
    lda oz_t1
    sta oz_t3
    asl oz_t0
    rol oz_t1                   ; 4d
    lda oz_t0
    clc
    adc oz_t2
    sta oz_t0
    lda oz_t1
    adc oz_t3
    rts

oz_arremer_rush_level:
    jsr ob_move
    dec ob_t2,x
    beq @back
    jsr oz_rel_player
    jsr oz_abs_d
    cmp #64
    bcc @out
    lda ob_dir,x
    beq :+
    lda oz_dx+1                 ; rushing left: past when the player is right of it
    bpl @back
    rts
:
    lda oz_dx+1
    bmi @back
@out:
    rts
@back:
    jmp oz_arremer_walk

oz_arremer_swoop:
    jsr ob_move
    dec ob_t2,x
    bne @out
    lda ob_step,x
    bne @up_done
    inc ob_step,x
    lda #43
    sta ob_t2,x
    lda #0
    sec
    sbc ob_vyf,x
    sta ob_vyf,x
    lda #0
    sbc ob_vyi,x
    sta ob_vyi,x
@out:
    rts
@up_done:
    jmp oz_arremer_start_down

; --- powering up ($0104): 80 frames blinking, not hit ---
oz_arremer_start_power:
    lda #10
    sta ob_state,x
    lda #80
    sta ob_t2,x
    lda #ofl_harmful
    sta ob_flags,x
    lda #1
    sta ob_var,x
    jmp ob_stop

oz_arremer_power:
    lda #2
    sta ob_flash,x
    dec ob_t2,x
    bne :+
    jmp oz_arremer_walk
:
    rts

; --- Unicorn (type $08) -------------------------------------------------------------------
; 0 waiting for the screen, 1 landing and waking (2-4 x 16 frames), 2 walking (0.53 px),
; 3 jumping, 4 charging (1.80 px to 16 px past the player), 5 aiming (25 frames), then
; a shot at 3.0 px. Ten hits.
oz_unicorn:
    lda ob_state,x
    bne @not_wait
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda ob_xl,x
    sec
    sbc camera_low
    lda ob_xh,x
    sbc camera_high
    bne @out
    lda #1
    sta ob_state,x
    jsr oz_random
    and #$30
    clc
    adc #32
    sta ob_t0,x
    lda oz_boss_seen
    bne :+
    inc oz_boss_seen
    lda #snd_song_boss
    jsr call_bank8_play_song
:
    lda #<rf_4FAB
    ldy #>rf_4FAB
    jsr ob_anim
@out:
    rts
@not_wait:
    cmp #1
    bne @active
    jsr oz_fall
    dec ob_t0,x
    bne @out
    jmp oz_unicorn_walk
@active:
    cmp #2
    bne :+
    jmp oz_unicorn_walking
:
    cmp #3
    bne :+
    jmp oz_unicorn_jump
:
    cmp #4
    bne :+
    jmp oz_unicorn_charge
:
    jmp oz_unicorn_aim

oz_unicorn_walk:
    lda #2
    sta ob_state,x
    jsr oz_face_player
    jsr oz_random
    and #63
    clc
    adc #51
    sta ob_t0,x                 ; the next jump or charge
    jsr oz_random
    and #63
    clc
    adc #22
    sta ob_t1,x                 ; the next shot
    lda #$88                    ; 0.53 px
    ldy #0
    jsr ob_set_vx_dir
    lda #<rf_4FAB
    ldy #>rf_4FAB
    sta oz_t2
    sty oz_t3
    lda #<rf_4F7D
    ldy #>rf_4F7D
    jmp oz_anim_lr

oz_unicorn_walking:
    jsr oz_fall
    ; A shot coming within 56 px in front: jump.
    jsr oz_shot_near
    bcs oz_unicorn_leap
    dec ob_t1,x
    bne :+
    lda #5
    sta ob_state,x
    lda #25
    sta ob_t0,x
    jsr ob_stop
    jsr oz_face_player
    lda #<rf_508E
    ldy #>rf_508E
    sta oz_t2
    sty oz_t3
    lda #<rf_5081
    ldy #>rf_5081
    jmp oz_anim_lr
:
    dec ob_t0,x
    bne @out
    jsr oz_random
    and #1
    beq oz_unicorn_leap
    ; Charge.
    lda #4
    sta ob_state,x
    jsr oz_face_player
    lda #$cd                    ; 1.80 px
    ldy #1
    jsr ob_set_vx_dir
    lda #<rf_502D
    ldy #>rf_502D
    sta oz_t2
    sty oz_t3
    lda #<rf_4FFF
    ldy #>rf_4FFF
    jmp oz_anim_lr
@out:
    rts

oz_unicorn_leap:
    lda #3
    sta ob_state,x
    jsr oz_face_player
    lda #6
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    sta ob_t2,x
    jsr oz_random
    and #$7f
    clc
    adc #$70
    ldy #0
    jsr ob_set_vx_dir
    lda #<rf_4FEC
    ldy #>rf_4FEC
    sta oz_t2
    sty oz_t3
    lda #<rf_4FDF
    ldy #>rf_4FDF
    jmp oz_anim_lr

; C set when a player shot is within 56 px in front of object X, near its height.
oz_shot_near:
    ldy #0
@shot:
    lda ps_active,y
    beq @next
    lda ps_x_lo,y
    sec
    sbc ob_xl,x
    sta oz_t0
    lda ps_x_hi,y
    sbc ob_xh,x
    sta oz_t1
    lda ob_dir,x
    beq @right
    ; Facing left: the shot X - X in -56 .. 0.
    lda oz_t1
    cmp #$ff
    bne @next
    lda oz_t0
    cmp #256 - 56
    bcs @found
    bcc @next
@right:
    lda oz_t1
    bne @next
    lda oz_t0
    cmp #57
    bcc @found
@next:
    iny
    cpy #ap_max_shots
    bne @shot
    clc
    rts
@found:
    sec
    rts

; Up from 6.0 px with gravity 0.3125 for 16 frames and 0.098 for 10, then down.
oz_unicorn_jump:
    inc ob_t2,x
    lda ob_t2,x
    cmp #17
    bcc @strong
    cmp #27
    bcc @weak
@strong:
    lda ob_vyf,x
    sec
    sbc #$50
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
    jmp @move
@weak:
    lda ob_vyf,x
    sec
    sbc #$19
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
@move:
    jsr ob_move
    lda ob_vyi,x
    bpl @out
    jsr oz_on_ground
    bcc @out
    jsr oz_snap_ground
    jmp oz_unicorn_walk
@out:
    rts

oz_unicorn_charge:
    jsr oz_fall
    jsr oz_rel_player
    lda ob_dir,x
    beq :+
    ; Charging left: until 16 px left of the player.
    lda oz_dx
    clc
    adc #16
    lda oz_dx+1
    adc #0
    bmi @stop
    rts
:
    lda oz_dx
    sec
    sbc #16
    lda oz_dx+1
    sbc #0
    bmi @stop
    rts
@stop:
    jmp oz_unicorn_walk

oz_unicorn_aim:
    jsr oz_fall
    dec ob_t0,x
    bne @out
    ; The shot (type 1): level, 3.0 px the way it faces.
    lda ob_xl,x
    sta oz_t0
    lda ob_xh,x
    sta oz_t1
    lda ob_yl,x
    clc
    adc #8
    sta oz_t2
    lda ob_yh,x
    adc #0
    sta oz_t3
    lda ob_dir,x
    pha
    stx oz_obj
    lda #ok_shot
    ldy #0
    jsr oz_spawn
    pla
    bcs @back
    sta ob_dir,x
    lda #ofl_harmful
    sta ob_flags,x
    lda #0
    sta ob_carry,x
    tay
    lda #3
    tay
    lda #0
    jsr ob_set_vx_dir
    lda #<rf_FDB3
    ldy #>rf_FDB3
    sta oz_t2
    sty oz_t3
    lda #<rf_FDAB
    ldy #>rf_FDAB
    jsr oz_anim_lr
@back:
    ldx oz_obj
    jmp oz_unicorn_walk
@out:
    rts

; --- Magician (type $07): not placed yet (it needs the tombstone hit count) -----------
oz_magician:
    lda #0
    sta ob_kind,x
    rts
