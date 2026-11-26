; Stage 3 (Revision G Section 2: 洞窟〜ドラゴン) and Stage 4 (Section 3: 吊り橋〜ドラゴン)
; enemies, items, triggers, and multi-segment Dragon boss, placed in Bank 51 ($C000 window).
; Shares WRAM object pool (ob_*) and ZP temporaries (oz_*) with Bank 6 and Bank 49.

.segment "WRAM"
s34_sch_pat: .res 5             ; Up to 5 schedulers (S3: 5, S4: 2)
s34_sch_rec: .res 5
s34_sch_end: .res 5
s34_sch_wait: .res 5
s34_sch_once: .res 5
s34_fix_used: .res 3            ; Up to 18 fixed enemies -> 3 bytes
s34_sup_used: .res 2            ; Up to 13 support triggers -> 2 bytes
s34_drg_head_slot: .res 1       ; Object slot of Dragon head ($ff = none)
s34_drg_len: .res 1             ; Remaining body+tail segments (8 down to 0)
s34_drg_idx: .res 1             ; Head ring buffer write index (0..63)
s34_drg_seg_slot: .res 8        ; Object slots of segments 1..8 (7 body + 1 tail)
s34_drg_hx: .res 64             ; Head X low history
s34_drg_hxh: .res 64            ; Head X high history
s34_drg_hy: .res 64             ; Head Y low history
s34_drg_hyh: .res 64            ; Head Y high history
s34_camy: .res 2                ; Camera arcade Y (lo/hi)

.segment "BANK51"
.scope s34
; Fixed jump table at $C001 matching BANK6 & BANK49.
.assert * = $c001, lderror, "BANK51 jump table must start at $C001"
    jmp init
    jmp update
    jmp shot_check

oz_ptr = oz_mul
oz_i = oz_obj
oz_si = oz_t3
oz_camy = s34_camy
ofl_shoot = ofl_shootable
ofl_touch = ofl_harmful
ofl_alive = 0
ofl_shield = $80                ; Shielded (Stone Pillar idle: reflects player shots)
snd_se_shield = snd_se_clank
snd_se_item = snd_se_pickup
snd_se_boss_die = snd_se_kill
rf_BA04 = rf_4BAC
rf_B60B = rf_4BAC
rf_D275 = rf_5245
rf_D29C = rf_5245
rf_5AD1 = rf_7430
rf_E7A3 = rf_5C54

; Object kinds (match revg-section2-spawns.inc & revg-section3-spawns.inc).
ok_none = 0
ok_pig = 1                      ; Woody Pig ($00 var 1)
ok_crow = 2                     ; Cave Bat ($01 var 2)
ok_devil = 3                    ; Petit Devil ($02 var 1)
ok_flyer = 4                    ; Purple Flyer ($04 var 2)
ok_arremer = 5                  ; Red Arremer ($05)
ok_zombie = 6                   ; Zombie ($0B)
ok_pillar = 7                   ; Grey Stone Pillar ($12)
ok_fire = 8                     ; Fire Pillar ($0E)
ok_dragon = 9                   ; Dragon Boss ($0C: head/body/tail)
ok_shot = 10                    ; Enemy projectile ($05 pillar, $09 dragon fire, aimed)
ok_item = 11                    ; Drop item
ok_boom = 12                    ; Explosion
ok_popup = 13                   ; Score popup
ok_disc = 14                    ; 200 / 500 pt disc
ok_trigger = 15                 ; Hidden armor / 10,000 pt trigger
ok_key = 16                     ; Stage Boss Key + 181F clear sequence
ok_kinds = 17

; Per-kind tables (docs/revision-g-late-enemy-behaviour.md).
;                 -  pig bat dev fly arr zom pil fir drg sht itm bom pop dsc trg key
ok_hp:     .byte  0,  0,  0,  0,  0,  2,  0,  3, $ff, 5,  0,  0,  0,  0,  0,  0,  0
ok_points: .byte  0,  1,  1,  1,  1, 10,  2,  1,  0,  2,  0,  0,  0,  0,  0,  0,  0
ok_sx:     .byte  0, 12, 12, 12, 12, 10, 12, 12, 12, 12,  0,  0,  0,  0,  0,  0,  0
ok_sylo:   .byte  0,$F0,$F0,$F6,$F8,$F3,$EE,$E8,$F8,$F6,  0,  0,  0,  0,  0,  0,  0
ok_syhi:   .byte  0, 12,  8, 10,  8, 13, 10, 24,  8, 10,  0,  0,  0,  0,  0,  0,  0
ok_px:     .byte  0, 10, 10,  9, 10, 16,  9, 18,  8, 17,  6,  0,  0,  0,  0,  0,  0
ok_pylo:   .byte  0,$F4,$F8,$F6,$F8,$ED,$ED,$D8,$DE,$F5,$FA,  0,  0,  0,  0,  0,  0
ok_pyhi:   .byte  0, 12,  8, 10,  8, 19, 19, 40, 34, 11,  6,  0,  0,  0,  0,  0,  0

ok_upd_lo:
    .byte <ret_rts, <u_pig, <u_bat, <u_devil, <u_flyer, <u_arremer, <u_zombie
    .byte <u_pillar, <u_fire, <u_dragon, <u_shot, <u_item, <u_boom, <u_popup
    .byte <u_disc, <u_trigger, <u_key
ok_upd_hi:
    .byte >ret_rts, >u_pig, >u_bat, >u_devil, >u_flyer, >u_arremer, >u_zombie
    .byte >u_pillar, >u_fire, >u_dragon, >u_shot, >u_item, >u_boom, >u_popup
    .byte >u_disc, >u_trigger, >u_key

; Scheduler simultaneous limits:
; Stage 3: pig=2, flyer=2, arremer=1, zombie=3, dragon=1
; Stage 4: devil=3, dragon=1
sch_limit_s3: .byte 2, 2, 1, 3, 1
sch_limit_s4: .byte 3, 1

.include "revg-section2-spawns.inc"
.include "revg-section3-spawns.inc"

; 16-direction velocity tables (2.0 px/frame, 8.8 signed) for Dragon ($5370).
; Dir: 0=R, 4=D (Y- in arcade), 8=L, 12=U (Y+ in arcade).
drg_vxi: .byte $02, $01, $01, $00, $00, $FF, $FE, $FE, $FE, $FE, $FE, $FF, $00, $00, $01, $01
drg_vxf: .byte $00, $DA, $6A, $C3, $00, $3D, $96, $26, $00, $26, $96, $3D, $00, $C3, $6A, $DA
drg_vyi: .byte $00, $FF, $FE, $FE, $FE, $FE, $FE, $FF, $00, $00, $01, $01, $02, $01, $01, $00
drg_vyf: .byte $00, $3D, $96, $26, $00, $26, $96, $3D, $00, $C3, $6A, $DA, $00, $DA, $6A, $C3

; --- Initialization -------------------------------------------------------------------
init:
    ldx #ob_max - 1
    lda #0
:
    sta ob_kind,x
    sta ob_flags,x
    dex
    bpl :-
    ldx #ok_kinds - 1
:
    sta oz_kind_cnt,x
    dex
    bpl :-
    lda #$ff
    sta oz_reg_page
    sta s34_drg_head_slot
    ldx #4
:
    sta s34_sch_pat,x
    dex
    bpl :-
    lda #0
    sta s34_fix_used
    sta s34_fix_used+1
    sta s34_fix_used+2
    sta s34_sup_used
    sta s34_sup_used+1
    sta s34_drg_len
    sta s34_drg_idx
    sta oz_spawns
    sta oz_drop_sel
    sta oz_key_timer
    sta oz_boss_seen
    ldx #4
:
    sta s34_sch_once,x
    dex
    bpl :-
    lda #$b3
    sta oz_rng
    jsr place_fixed
    rts

; --- Helpers --------------------------------------------------------------------------
ret_rts:
    rts

rng:
    lda oz_rng
    asl a
    bcc :+
    eor #$1d
:
    sta oz_rng
    rts

alloc:
    ldx #0
@lp:
    lda ob_kind,x
    beq @found
    inx
    cpx #ob_max
    bne @lp
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
    sta ob_vxf,x
    sta ob_vxi,x
    sta ob_vyf,x
    sta ob_vyi,x
    sta ob_cnt,x
    sta ob_mirror,x
    sta ob_flash,x
    sta ob_hp,x
    sta ob_var,x
    sta ob_flags,x
    sta ob_dir,x
    sta ob_by_l,x
    sta ob_by_h,x
    sta ob_aux,x
    sta ob_aux2,x
    sta ob_carry,x
    lda #$ff
    sta ob_src,x
    lda #0
    sta ob_cur_l,x
    sta ob_cur_h,x
    sta ob_nxt_l,x
    sta ob_nxt_h,x
    clc
    rts

free_slot:
    ldy ob_kind,x
    beq @done
    lda oz_kind_cnt,y
    beq :+
    sec
    sbc #1
    sta oz_kind_cnt,y
:
    lda #0
    sta ob_kind,x
    sta ob_flags,x
@done:
    rts

inc_kind_y:
    lda oz_kind_cnt,y
    clc
    adc #1
    sta oz_kind_cnt,y
    rts

set_anim:
    sta ob_nxt_l,x
    sta ob_cur_l,x
    tya
    sta ob_nxt_h,x
    sta ob_cur_h,x
    lda #0
    sta ob_cnt,x
    rts

move_x:
    lda ob_xf,x
    clc
    adc ob_vxf,x
    sta ob_xf,x
    lda ob_xl,x
    adc ob_vxi,x
    sta ob_xl,x
    lda ob_vxi,x
    bmi @neg
    lda ob_xh,x
    adc #0
    sta ob_xh,x
    rts
@neg:
    lda ob_xh,x
    adc #$ff
    sta ob_xh,x
    rts

move_y:
    lda ob_yf,x
    clc
    adc ob_vyf,x
    sta ob_yf,x
    lda ob_yl,x
    adc ob_vyi,x
    sta ob_yl,x
    lda ob_vyi,x
    bmi @neg
    lda ob_yh,x
    adc #0
    sta ob_yh,x
    rts
@neg:
    lda ob_yh,x
    adc #$ff
    sta ob_yh,x
    rts

; Compute player arcade Y in oz_py (lo/hi) and camera arcade Y in oz_camy (lo/hi).
; Stage 3 (4-page): Y_top = 959 ($03BF). Stage 4 (1-page): Y_top = 255 ($00FF).
compute_coords:
    lda campaign_stage
    cmp #3
    beq @s3
    lda #$ff
    sec
    sbc player_feet
    sta oz_py
    lda #0
    sbc ap_player_y_hi
    sta oz_py+1
    lda #0
    sta oz_camy
    sta oz_camy+1
    rts
@s3:
    lda #$bf
    sec
    sbc player_feet
    sta oz_py
    lda #$03
    sbc ap_player_y_hi
    sta oz_py+1
    ; Arcade camera Y (bottom of visible 240px screen) = 959 - (ap_cam_y + 240) = 719 - ap_cam_y ($02CF).
    lda #$cf
    sec
    sbc ap_cam_y_lo
    sta oz_camy
    lda #$02
    sbc ap_cam_y_hi
    sta oz_camy+1
    rts

; Signed 16-bit dx = ob_x - player_x -> oz_dx (lo/hi); C=0 if ob >= player.
calc_dx_player:
    lda ob_xl,x
    sec
    sbc player_x
    sta oz_dx
    lda ob_xh,x
    sbc player_x+1
    sta oz_dx+1
    rts

; Signed 16-bit dy = ob_y - oz_py -> oz_dy (lo/hi); C=0 if ob >= player.
calc_dy_player:
    lda ob_yl,x
    sec
    sbc oz_py
    sta oz_dy
    lda ob_yh,x
    sbc oz_py+1
    sta oz_dy+1
    rts

; Set ob_dir (0=right, 1=left) toward player.
face_player:
    jsr calc_dx_player
    lda oz_dx+1
    bpl @left
    lda #0
    sta ob_dir,x
    rts
@left:
    lda #1
    sta ob_dir,x
    rts

; Check if object X is within [camera_x - 32, camera_x + 288] and Y within [camy - 32, camy + 272].
; Returns C=0 if on-screen, C=1 if off-screen.
in_view:
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    lda oz_t0
    clc
    adc #32
    lda oz_t1
    adc #0
    beq @xv_ok
    cmp #1
    bne @off
    lda oz_t0
    clc
    adc #32
    cmp #64
    bcs @off
@xv_ok:
    lda ob_yl,x
    sec
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    sta oz_t1
    lda oz_t0
    clc
    adc #32
    lda oz_t1
    adc #0
    beq @in
    cmp #1
    bne @off
    lda oz_t0
    clc
    adc #32
    cmp #48
    bcs @off
@in:
    clc
    rts
@off:
    sec
    rts

; Despawn if far off-screen (X outside [cam - 96, cam + 352] or Y outside [camy - 96, camy + 336]).
offscreen_cull:
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    lda oz_t0
    clc
    adc #96
    lda oz_t1
    adc #0
    beq @xc_ok
    cmp #1
    bne @cull
    lda oz_t0
    clc
    adc #96
    cmp #192
    bcs @cull
@xc_ok:
    lda ob_yl,x
    sec
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    sta oz_t1
    lda oz_t0
    clc
    adc #96
    lda oz_t1
    adc #0
    beq @keep
    cmp #1
    bne @cull
    lda oz_t0
    clc
    adc #96
    cmp #176
    bcs @cull
@keep:
    clc
    rts
@cull:
    jsr free_slot
    sec
    rts

; --- Fixed enemy & support trigger placement ------------------------------------------
place_fixed:
    lda campaign_stage
    cmp #4
    bne :+
    jmp @s4_fix
:
    ; Stage 3 fixed enemies (18) + triggers (13).
    ldy #0
@s3_f:
    sty oz_i
    lda revg3_fix_x_lo,y
    sec
    sbc camera_low
    sta oz_t0
    lda revg3_fix_x_hi,y
    sbc camera_high
    sta oz_t1
    lda oz_t0
    clc
    adc #48
    lda oz_t1
    adc #0
    beq @s3_fx
    cmp #1
    bne @s3_fn
    lda oz_t0
    clc
    adc #48
    cmp #96
    bcs @s3_fn
@s3_fx:
    ; Check bit in s34_fix_used
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    tay
    lda bit_mask,y
    and s34_fix_used,x
    bne @s3_fn
    lda bit_mask,y
    ora s34_fix_used,x
    sta s34_fix_used,x
    jsr alloc
    bcs @s3_fn
    ldy oz_i
    lda revg3_fix_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_i
    lda revg3_fix_var,y
    sta ob_var,x
    lda revg3_fix_x_lo,y
    sta ob_xl,x
    lda revg3_fix_x_hi,y
    sta ob_xh,x
    lda revg3_fix_y_lo,y
    sta ob_yl,x
    lda revg3_fix_y_hi,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
@s3_fn:
    ldy oz_i
    iny
    cpy #revg3_fix_count
    beq :+
    jmp @s3_f
:
    ; Stage 3 triggers
    ldy #0
@s3_s:
    sty oz_i
    lda revg3_sup_x_lo,y
    sec
    sbc camera_low
    lda revg3_sup_x_hi,y
    sbc camera_high
    bne @s3_sn
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    tay
    lda bit_mask,y
    and s34_sup_used,x
    bne @s3_sn
    lda bit_mask,y
    ora s34_sup_used,x
    sta s34_sup_used,x
    jsr alloc
    bcs @s3_sn
    ldy oz_i
    lda #ok_trigger
    sta ob_kind,x
    inc oz_kind_cnt+ok_trigger
    lda revg3_sup_var,y
    sta ob_var,x
    lda revg3_sup_x_lo,y
    sta ob_xl,x
    lda revg3_sup_x_hi,y
    sta ob_xh,x
    lda revg3_sup_y_lo,y
    sta ob_yl,x
    lda revg3_sup_y_hi,y
    sta ob_yh,x
    lda #ofl_alive
    sta ob_flags,x
@s3_sn:
    ldy oz_i
    iny
    cpy #revg3_sup_count
    bne @s3_s
    rts

@s4_fix:
    ; Stage 4 fixed enemies (16) + triggers (2).
    ldy #0
@s4_f:
    sty oz_i
    lda revg4_fix_x_lo,y
    sec
    sbc camera_low
    sta oz_t0
    lda revg4_fix_x_hi,y
    sbc camera_high
    sta oz_t1
    lda oz_t0
    clc
    adc #48
    lda oz_t1
    adc #0
    beq @s4_fx
    cmp #1
    bne @s4_fn
    lda oz_t0
    clc
    adc #48
    cmp #96
    bcs @s4_fn
@s4_fx:
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    tay
    lda bit_mask,y
    and s34_fix_used,x
    bne @s4_fn
    lda bit_mask,y
    ora s34_fix_used,x
    sta s34_fix_used,x
    jsr alloc
    bcs @s4_fn
    ldy oz_i
    lda revg4_fix_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_i
    lda revg4_fix_var,y
    sta ob_var,x
    lda revg4_fix_x_lo,y
    sta ob_xl,x
    lda revg4_fix_x_hi,y
    sta ob_xh,x
    lda revg4_fix_y_lo,y
    sta ob_yl,x
    lda revg4_fix_y_hi,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    ; Stagger fire pillar initial timer using fixed index
    tya
    and #3
    asl a
    asl a
    asl a
    asl a
    clc
    adc #32
    sta ob_t0,x
@s4_fn:
    ldy oz_i
    iny
    cpy #revg4_fix_count
    beq :+
    jmp @s4_f
:
    ; Stage 4 triggers (2)
    ldy #0
@s4_s:
    sty oz_i
    lda revg4_sup_x_lo,y
    sec
    sbc camera_low
    lda revg4_sup_x_hi,y
    sbc camera_high
    bne @s4_sn
    lda bit_mask,y
    and s34_sup_used
    bne @s4_sn
    lda bit_mask,y
    ora s34_sup_used
    sta s34_sup_used
    jsr alloc
    bcs @s4_sn
    ldy oz_i
    lda #ok_trigger
    sta ob_kind,x
    inc oz_kind_cnt+ok_trigger
    lda revg4_sup_var,y
    sta ob_var,x
    lda revg4_sup_x_lo,y
    sta ob_xl,x
    lda revg4_sup_x_hi,y
    sta ob_xh,x
    lda revg4_sup_y_lo,y
    sta ob_yl,x
    lda revg4_sup_y_hi,y
    sta ob_yh,x
    lda #ofl_alive
    sta ob_flags,x
@s4_sn:
    ldy oz_i
    iny
    cpy #revg4_sup_count
    bne @s4_s
    rts

bit_mask: .byte $01, $02, $04, $08, $10, $20, $40, $80

; --- Page Group Scheduler -------------------------------------------------------------
register_page:
    lda campaign_stage
    cmp #4
    bne :+
    jmp @s4_pg
:
    ; Stage 3: 4 vertical pages (row = 3 - pan_y_page), 14 cols -> index = row * 14 + col.
    lda #3
    sec
    sbc pan_y_page
    bpl :+
    lda #0
:
    tay
    lda #0
@mul14:
    dey
    bmi @mul_done
    clc
    adc #revg3_page_cols
    bne @mul14
@mul_done:
    clc
    adc camera_high
    cmp oz_reg_page
    bne :+
    rts
:
    sta oz_reg_page
    ; Multiply page index (0..55) by 9 -> 16-bit offset into revg3_page_groups
    sta oz_t0
    lda #0
    sta oz_t1
    asl oz_t0
    rol oz_t1
    asl oz_t0
    rol oz_t1
    asl oz_t0
    rol oz_t1                   ; * 8
    lda oz_t0
    clc
    adc oz_reg_page
    sta oz_ptr
    lda oz_t1
    adc #0
    sta oz_ptr+1
    lda oz_ptr
    clc
    adc #<revg3_page_groups
    sta oz_ptr
    lda oz_ptr+1
    adc #>revg3_page_groups
    sta oz_ptr+1
    ldy #0
@s3_ent:
    lda (oz_ptr),y
    beq @s3_done
    sec
    sbc #1
    tax                         ; scheduler 0..4
    iny
    lda (oz_ptr),y              ; pattern index
    sta oz_t0
    iny
    lda (oz_ptr),y              ; flags
    sta oz_t1
    iny
    sty oz_i
    lda oz_t0
    cmp s34_sch_pat,x
    beq @s3_next
    ; If one-shot (flags bit 1) and already triggered, skip.
    lda oz_t1
    and #2
    beq @s3_load
    lda s34_sch_once,x
    bne @s3_next
    lda #1
    sta s34_sch_once,x
@s3_load:
    lda oz_t0
    sta s34_sch_pat,x
    tay
    lda revg3_pat_start,y
    sta s34_sch_rec,x
    clc
    adc revg3_pat_count,y
    sta s34_sch_end,x
    lda #10
    sta s34_sch_wait,x
@s3_next:
    ldy oz_i
    cpy #9
    bne @s3_ent
@s3_done:
    rts

@s4_pg:
    lda camera_high
    cmp #revg4_page_cols
    bcc :+
    lda #revg4_page_cols - 1
:
    cmp oz_reg_page
    bne :+
    rts
:
    sta oz_reg_page
    ; Multiply by 9 (max 11 * 9 = 99 < 256)
    asl a
    asl a
    asl a
    clc
    adc oz_reg_page
    tay
    lda #3
    sta oz_t2
@s4_ent:
    lda revg4_page_groups,y
    beq @s4_done
    sec
    sbc #1
    tax                         ; scheduler 0..1
    iny
    lda revg4_page_groups,y     ; pattern
    sta oz_t0
    iny
    lda revg4_page_groups,y     ; flags
    sta oz_t1
    iny
    sty oz_i
    lda oz_t0
    cmp s34_sch_pat,x
    beq @s4_next
    lda oz_t1
    and #2
    beq @s4_load
    lda s34_sch_once,x
    bne @s4_next
    lda #1
    sta s34_sch_once,x
@s4_load:
    lda oz_t0
    sta s34_sch_pat,x
    tay
    lda revg4_pat_start,y
    sta s34_sch_rec,x
    clc
    adc revg4_pat_count,y
    sta s34_sch_end,x
    lda #10
    sta s34_sch_wait,x
@s4_next:
    ldy oz_i
    dec oz_t2
    bne @s4_ent
@s4_done:
    rts

step_schedulers:
    lda oz_boss_seen
    beq :+
    rts                         ; Stop normal spawns during Dragon boss fight
:
    lda campaign_stage
    cmp #4
    beq @s4_step
    ldx #4
@s3_lp:
    stx oz_i
    lda s34_sch_pat,x
    bmi @s3_nx
    dec s34_sch_wait,x
    bne @s3_nx
    ; Check simultaneous limit
    ldy revg3_sched_kind,x
    lda oz_kind_cnt,y
    cmp sch_limit_s3,x
    bcc @s3_do
    lda #20
    sta s34_sch_wait,x
    jmp @s3_nx
@s3_do:
    ldy s34_sch_rec,x
    lda revg3_rec_wait,y
    sta s34_sch_wait,x
    ; Spawn from Stage 3 record Y
    jsr spawn_s3_rec
    ldx oz_i
    inc s34_sch_rec,x
    lda s34_sch_rec,x
    cmp s34_sch_end,x
    bcc @s3_nx
    lda s34_sch_once,x
    beq @s3_wrap
    lda #$ff
    sta s34_sch_pat,x
    jmp @s3_nx
@s3_wrap:
    ldy s34_sch_pat,x
    lda revg3_pat_start,y
    sta s34_sch_rec,x
@s3_nx:
    ldx oz_i
    dex
    bpl @s3_lp
    rts

@s4_step:
    ldx #1
@s4_lp:
    stx oz_i
    lda s34_sch_pat,x
    bmi @s4_nx
    dec s34_sch_wait,x
    bne @s4_nx
    ldy revg4_sched_kind,x
    lda oz_kind_cnt,y
    cmp sch_limit_s4,x
    bcc @s4_do
    lda #20
    sta s34_sch_wait,x
    jmp @s4_nx
@s4_do:
    ldy s34_sch_rec,x
    lda revg4_rec_wait,y
    sta s34_sch_wait,x
    jsr spawn_s4_rec
    ldx oz_i
    inc s34_sch_rec,x
    lda s34_sch_rec,x
    cmp s34_sch_end,x
    bcc @s4_nx
    lda s34_sch_once,x
    beq @s4_wrap
    lda #$ff
    sta s34_sch_pat,x
    jmp @s4_nx
@s4_wrap:
    ldy s34_sch_pat,x
    lda revg4_pat_start,y
    sta s34_sch_rec,x
@s4_nx:
    ldx oz_i
    dex
    bpl @s4_lp
    rts

spawn_s3_rec:
    sty oz_t2                   ; record index
    jsr alloc
    bcc :+
    rts
:
    ldy oz_i
    lda revg3_sched_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_t2
    lda revg3_rec_var,y
    sta ob_var,x
    ; Camera-relative X & Y
    lda revg3_rec_x_lo,y
    clc
    adc camera_low
    sta ob_xl,x
    lda revg3_rec_x_hi,y
    adc camera_high
    sta ob_xh,x
    lda revg3_rec_y_lo,y
    clc
    adc oz_camy
    sta ob_yl,x
    lda revg3_rec_y_hi,y
    adc oz_camy+1
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    ; Every 5th normal spawn carries a pot
    lda ob_kind,x
    cmp #ok_dragon
    beq @no_pot
    inc oz_spawns
    lda oz_spawns
    cmp #5
    bcc @no_pot
    lda #0
    sta oz_spawns
    lda #1
    sta ob_carry,x
@no_pot:
    rts

spawn_s4_rec:
    sty oz_t2
    jsr alloc
    bcc :+
    rts
:
    ldy oz_i
    lda revg4_sched_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_t2
    lda revg4_rec_var,y
    sta ob_var,x
    lda revg4_rec_x_lo,y
    clc
    adc camera_low
    sta ob_xl,x
    lda revg4_rec_x_hi,y
    adc camera_high
    sta ob_xh,x
    lda revg4_rec_y_lo,y
    sta ob_yl,x
    lda revg4_rec_y_hi,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    rts

; --- Main Frame Update ----------------------------------------------------------------
update:
    jsr compute_coords
    jsr place_fixed
    jsr register_page
    jsr step_schedulers
    ; Also trigger Dragon if player reaches boss camera column (Stage 3: cam >= 3072, Stage 4: cam >= 2560)
    lda oz_boss_seen
    bne @upd_loop
    lda campaign_stage
    cmp #4
    beq @chk_s4_boss
    lda camera_high
    cmp #12                     ; page 12 (X >= 3072)
    bcc @upd_loop
    lda pan_y_page
    cmp #3                      ; bottom page
    bne @upd_loop
    jsr spawn_dragon_boss
    jmp @upd_loop
@chk_s4_boss:
    lda camera_high
    cmp #10                     ; page 10 (X >= 2560)
    bcc @upd_loop
    jsr spawn_dragon_boss
@upd_loop:
    ldx #0
@obj:
    stx oz_i
    lda ob_kind,x
    beq @next
    lda ob_flash,x
    beq :+
    dec ob_flash,x
:
    ldy ob_kind,x
    lda ok_upd_lo,y
    sta oz_ptr
    lda ok_upd_hi,y
    sta oz_ptr+1
    jsr @call
    ldx oz_i
    lda ob_kind,x
    beq @next
    lda ob_flags,x
    and #ofl_touch
    beq @next
    jsr check_player_hit
@next:
    ldx oz_i
    inx
    cpx #ob_max
    bne @obj
    rts
@call:
    jmp (oz_ptr)

; --- Enemy AI Routines ----------------------------------------------------------------
; 1. Woody Pig ($00 var 1): horizontal swoop + spear throw.
u_pig:
    lda ob_state,x
    bne @fly
    inc ob_state,x
    jsr face_player
    lda #0
    sta ob_vxi,x
    lda #$e0
    ldy ob_dir,x
    beq :+
    lda #$ff
    sta ob_vxi,x
    lda #$20
:
    sta ob_vxf,x
    lda #50
    sta ob_t0,x
    lda ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    lda #<rf_4974
    ldy #>rf_4974
    jmp set_anim
@fly:
    jsr move_x
    ; Slight vertical sine bob
    inc ob_t1,x
    lda ob_t1,x
    and #$10
    beq :+
    lda #$ff
    sta ob_vyi,x
    lda #$c0
    bne :++
:
    lda #0
    sta ob_vyi,x
    lda #$40
:
    sta ob_vyf,x
    jsr move_y
    dec ob_t0,x
    bne :+
    lda #75
    sta ob_t0,x
    jsr fire_aimed_shot
:
    jmp offscreen_cull

; 2. Cave Bat ($01 var 2): hangs on ceiling, swoops down when player approaches, waves.
u_bat:
    lda ob_state,x
    beq @init
    cmp #1
    beq @hang
    cmp #2
    beq @swoop
    jmp @wave
@init:
    inc ob_state,x
    jsr face_player
    lda #<rf_49B2
    ldy #>rf_49B2
    ldx oz_i
    lda ob_dir,x
    beq :+
    lda #<rf_49D0
    ldy #>rf_49D0
    bne :++
:
    lda #<rf_49B2
    ldy #>rf_49B2
:
    jmp set_anim
@hang:
    jsr in_view
    bcs @bat_ret
    jsr calc_dx_player
    lda oz_dx+1
    beq @chk_d
    cmp #$ff
    bne @bat_ret
    lda oz_dx
    cmp #$a0                    ; within 96 px
    bcc @bat_ret
    bcs @start_swoop
@chk_d:
    lda oz_dx
    cmp #96
    bcs @bat_ret
@start_swoop:
    jsr face_player
    lda #2
    sta ob_state,x
    lda #40
    sta ob_t0,x
    lda #$fe
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #$40
    ldy ob_dir,x
    beq :+
    lda #$c0
:
    sta ob_vxf,x
@bat_ret:
    rts
@swoop:
    jsr move_x
    jsr move_y
    dec ob_t0,x
    bne :+
    lda #3
    sta ob_state,x
    lda #0
    sta ob_t1,x
:
    jmp offscreen_cull
@wave:
    jsr move_x
    inc ob_t1,x
    lda ob_t1,x
    and #$10
    beq :+
    lda #$ff
    sta ob_vyi,x
    lda #$80
    bne :++
:
    lda #0
    sta ob_vyi,x
    lda #$80
:
    sta ob_vyf,x
    jsr move_y
    jmp offscreen_cull

; 3. Petit Devil ($02 var 1, Stage 4): rises from bottom, swoops toward Arthur.
u_devil:
    lda ob_state,x
    beq @init
    cmp #1
    beq @rise
    jmp @swoop
@init:
    inc ob_state,x
    lda #48
    sta ob_t0,x
    lda #1
    sta ob_vyi,x
    lda #$40
    sta ob_vyf,x
    lda #<rf_A893
    ldy #>rf_A893
    jmp set_anim
@rise:
    jsr move_y
    dec ob_t0,x
    bne @dev_cull
    lda #2
    sta ob_state,x
    lda #64
    sta ob_t0,x
    jsr face_player
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #$a0
    ldy ob_dir,x
    beq :+
    lda #$60
:
    sta ob_vxf,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
@dev_cull:
    jmp offscreen_cull
@swoop:
    jsr move_x
    inc ob_t1,x
    lda ob_t1,x
    and #$10
    beq :+
    lda #$ff
    sta ob_vyi,x
    lda #$a0
    bne :++
:
    lda #0
    sta ob_vyi,x
    lda #$60
:
    sta ob_vyf,x
    jsr move_y
    jmp offscreen_cull

; 4. Purple Flyer ($04 var 2, Stage 3): dives down then flies/hops toward player.
u_flyer:
    lda ob_state,x
    beq @init
    cmp #1
    beq @dive
    jmp @fly
@init:
    inc ob_state,x
    jsr face_player
    lda #32
    sta ob_t0,x
    lda #$fe
    sta ob_vyi,x
    lda #$60
    sta ob_vyf,x
    lda #0
    sta ob_vxi,x
    lda #$80
    ldy ob_dir,x
    beq :+
    lda #$ff
    sta ob_vxi,x
    lda #$80
:
    sta ob_vxf,x
    lda #<rf_B19E
    ldy #>rf_B19E
    jmp set_anim
@dive:
    jsr move_x
    jsr move_y
    dec ob_t0,x
    bne @fl_cull
    lda #2
    sta ob_state,x
    jsr face_player
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #$60
    ldy ob_dir,x
    beq :+
    lda #$a0
:
    sta ob_vxf,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
@fl_cull:
    jmp offscreen_cull
@fly:
    jsr move_x
    inc ob_t1,x
    lda ob_t1,x
    and #$18
    cmp #$0c
    bcc :+
    lda #$ff
    sta ob_vyi,x
    lda #$c0
    bne :++
:
    lda #0
    sta ob_vyi,x
    lda #$40
:
    sta ob_vyf,x
    jsr move_y
    jmp offscreen_cull

; 5. Red Arremer ($05, Stages 3 & 4): sits, wakes up, hovers, swoops, fires aimed shot.
u_arremer:
    lda ob_state,x
    beq @init
    cmp #1
    beq @sit
    cmp #2
    beq @hover
    jmp @swoop
@init:
    inc ob_state,x
    jsr face_player
    lda ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    lda #<rf_BA04
    ldy #>rf_BA04
    jmp set_anim
@sit:
    jsr in_view
    bcs @ar_ret
    jsr calc_dx_player
    lda oz_dx+1
    beq @ar_c
    cmp #$ff
    bne @ar_ret
    lda oz_dx
    cmp #$90
    bcc @ar_ret
    bcs @ar_wake
@ar_c:
    lda oz_dx
    cmp #112
    bcs @ar_ret
@ar_wake:
    lda #2
    sta ob_state,x
    lda #40
    sta ob_t0,x
    lda #<rf_B60B
    ldy #>rf_B60B
    jsr set_anim
    ldx oz_i
    lda #1
    sta ob_vyi,x
    lda #$00
    sta ob_vyf,x
@ar_ret:
    rts
@hover:
    jsr face_player
    lda ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    ; Rise gently while timer > 20, then hover
    lda ob_t0,x
    cmp #20
    bcc :+
    jsr move_y
:
    dec ob_t0,x
    bne @ar_ret
    ; Fire aimed shot and swoop!
    jsr fire_aimed_shot
    ldx oz_i
    lda #3
    sta ob_state,x
    lda #36
    sta ob_t0,x
    lda #2
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    lda #$ff
    sta ob_vyi,x
    lda #$40
    sta ob_vyf,x
    rts
@swoop:
    jsr move_x
    jsr move_y
    dec ob_t0,x
    bne :+
    lda #2
    sta ob_state,x
    lda #48
    sta ob_t0,x
    lda #1
    sta ob_vyi,x
    lda #$20
    sta ob_vyf,x
:
    jmp offscreen_cull

; 6. Zombie ($0B, Stage 3): rises, walks toward player, sinks.
u_zombie:
    lda ob_state,x
    beq @init
    cmp #1
    beq @rise
    jmp @walk
@init:
    inc ob_state,x
    jsr face_player
    lda ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    lda #20
    sta ob_t0,x
    lda #<rf_D275
    ldy #>rf_D275
    jmp set_anim
@rise:
    dec ob_t0,x
    bne @z_ret
    lda #2
    sta ob_state,x
    lda #120
    sta ob_t0,x
    lda #0
    sta ob_vxi,x
    lda #$d0
    ldy ob_dir,x
    beq :+
    lda #$ff
    sta ob_vxi,x
    lda #$30
:
    sta ob_vxf,x
    lda #<rf_D29C
    ldy #>rf_D29C
    jmp set_anim
@z_ret:
    rts
@walk:
    jsr move_x
    dec ob_t0,x
    bne :+
    jmp free_slot
:
    jmp offscreen_cull

; 7. Grey Stone Pillar ($12, Stage 3):
;    Shielded (ofl_shield) while waiting; when player within Y +-48 and timer expires,
;    drops shield for 72 frames and fires horizontal shot ($05, 2.0 px/f).
u_pillar:
    lda ob_state,x
    beq @init
    cmp #1
    beq @wait
    jmp @fire_st
@init:
    inc ob_state,x
    jsr face_player
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    lda #64
    sta ob_t0,x
    jsr @set_frame
    rts
@set_frame:
    lda ob_dir,x
    beq @fr_r
    lda #<rf_57A1
    ldy #>rf_57A1
    jmp set_anim
@fr_r:
    lda #<rf_5795
    ldy #>rf_5795
    jmp set_anim
@wait:
    jsr in_view
    bcs @p_ret
    jsr face_player
    ; Check player Y within +-48
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #48
    tay
    lda oz_dy+1
    adc #0
    bne @p_ret
    cpy #96
    bcs @p_ret
    dec ob_t0,x
    bne @p_ret
    ; Drop shield and enter firing state (72 frames)
    lda #2
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #72
    sta ob_t0,x
    jsr @set_frame
@p_ret:
    rts
@fire_st:
    lda ob_t0,x
    cmp #42
    bne @no_shot
    ; Fire horizontal stone shot (2.0 px/f) from Y + 7 (or Y - 16 alternating)
    inc ob_aux,x
    lda ob_aux,x
    and #1
    beq @hi_shot
    lda #$f0                    ; -16
    bne @do_sh
@hi_shot:
    lda #7
@do_sh:
    jsr fire_pillar_shot
    ldx oz_i
@no_shot:
    dec ob_t0,x
    bne @p_ret
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    lda #96
    sta ob_t0,x
    rts

; 8. Fire Pillar ($0E, Stage 4):
;    Invulnerable (absorbs shots); cycles idle (rf_55EC) -> mid (rf_5602) -> tall (rf_5631, lethal!) -> mid -> idle.
u_fire:
    lda ob_state,x
    beq @init
    jsr in_view
    bcc :+
    rts
:
    ; Flicker mirror every 4 frames for lively flame animation
    lda oz_rng
    and #$40
    sta ob_mirror,x
    dec ob_t0,x
    beq :+
    rts
:
    lda ob_state,x
    cmp #1
    beq @to_mid_up
    cmp #2
    beq @to_tall
    cmp #3
    bne :+
    jmp @to_mid_dn
:
    ; Back to idle
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot  ; harmless when idle
    sta ob_flags,x
    lda #96
    sta ob_t0,x
    lda ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    sta ob_yh,x
    lda #<rf_55EC
    ldy #>rf_55EC
    jmp set_anim
@init:
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot
    sta ob_flags,x
    lda ob_t0,x
    bne :+
    lda #64
    sta ob_t0,x
:
    lda #<rf_55EC
    ldy #>rf_55EC
    jmp set_anim
@to_mid_up:
    lda #2
    sta ob_state,x
    lda #28
    sta ob_t0,x
    lda ob_by_l,x
    clc
    adc #8
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    lda #<rf_5602
    ldy #>rf_5602
    jmp set_anim
@to_tall:
    lda #3
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch  ; LETHAL at peak height!
    sta ob_flags,x
    lda #36
    sta ob_t0,x
    lda ob_by_l,x
    clc
    adc #16
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    lda #<rf_5631
    ldy #>rf_5631
    jmp set_anim
@to_mid_dn:
    lda #4
    sta ob_state,x
    lda #ofl_alive | ofl_shoot
    sta ob_flags,x
    lda #20
    sta ob_t0,x
    lda ob_by_l,x
    clc
    adc #8
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    lda #<rf_5602
    ldy #>rf_5602
    jmp set_anim
@f_ret:
    rts

; 9. Dragon Boss ($0C, Stages 3 & 4):
;    var = 0: Head (6 HP), var = 1: Body (7 segs), var = 2: Tail (1 seg).
;    Head records (X, Y) in 64-entry ring buffer; body/tail follow at 7-frame intervals.
spawn_dragon_boss:
    lda #1
    sta oz_boss_seen
    ; Clear non-boss objects so Fire Pillars / stage enemies cannot starve the 9 Dragon slots.
    ldx #ob_max - 1
    lda #0
:
    sta ob_kind,x
    sta ob_flags,x
    dex
    bpl :-
    ldx #ok_kinds - 1
:
    sta oz_kind_cnt,x
    dex
    bpl :-
    lda #snd_song_boss
    jsr call_bank8_play_song
    ; Allocate head
    jsr alloc
    bcc :+
    rts
:
    stx s34_drg_head_slot
    lda #ok_dragon
    sta ob_kind,x
    inc oz_kind_cnt+ok_dragon
    lda #5                      ; 6 HP (hits 0..5)
    sta ob_hp,x
    lda #0
    sta ob_var,x
    sta ob_state,x
    lda #10                     ; Initial steering countdown (avoid 0->255 wrap)
    sta ob_t0,x
    lda #8                      ; Direction 8 = Left
    sta ob_aux,x
    lda #1                      ; Facing left
    sta ob_dir,x
    ; Position at right side of boss arena
    lda camera_low
    clc
    adc #224
    sta ob_xl,x
    lda camera_high
    adc #0
    sta ob_xh,x
    lda oz_camy
    clc
    adc #112
    sta ob_yl,x
    lda oz_camy+1
    adc #0
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #<rf_5352
    ldy #>rf_5352
    jsr set_anim
    ; Prefill 64-entry ring buffer with initial head position trailing right
    ldx s34_drg_head_slot
    lda ob_xl,x
    sta oz_t0
    lda ob_xh,x
    sta oz_t1
    lda ob_yl,x
    sta oz_t2
    lda ob_yh,x
    sta oz_ptr
    ldy #63
@fill_ring:
    lda oz_t0
    sta s34_drg_hx,y
    lda oz_t1
    sta s34_drg_hxh,y
    lda oz_t2
    sta s34_drg_hy,y
    lda oz_ptr
    sta s34_drg_hyh,y
    dey
    bpl @fill_ring
    lda #0
    sta s34_drg_idx
    ; Allocate 8 trailing segments (0..6 = body, 7 = tail)
    lda #0
    sta s34_drg_len
@seg_lp:
    jsr alloc
    bcs @seg_done
    ldy s34_drg_len
    txa
    sta s34_drg_seg_slot,y
    lda #ok_dragon
    sta ob_kind,x
    inc oz_kind_cnt+ok_dragon
    lda #0                      ; 1 HP (any hit destroys trailing segment)
    sta ob_hp,x
    lda #1                      ; var 1 = body
    cpy #7
    bne :+
    lda #2                      ; var 2 = tail
:
    sta ob_var,x
    tya
    clc
    adc #1
    sta ob_aux2,x               ; segment index 1..8
    lda oz_t0
    sta ob_xl,x
    lda oz_t1
    sta ob_xh,x
    lda oz_t2
    sta ob_yl,x
    lda oz_ptr
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda ob_var,x
    cmp #2
    beq @set_tail
    lda #<rf_D5CE
    ldy #>rf_D5CE
    jsr set_anim
    jmp @seg_nx
@set_tail:
    lda #<rf_D615
    ldy #>rf_D615
    jsr set_anim
@seg_nx:
    inc s34_drg_len
    lda s34_drg_len
    cmp #8
    bne @seg_lp
@seg_done:
    rts

u_dragon:
    lda ob_var,x
    beq @u_head
    ; Body or tail segment: read ring buffer at (s34_drg_idx - seg_idx * 7) & 63
    lda ob_aux2,x               ; 1..8
    asl a
    asl a
    asl a                       ; * 8
    sec
    sbc ob_aux2,x               ; * 7
    sta oz_t0
    lda s34_drg_idx
    sec
    sbc oz_t0
    and #63
    tay
    lda s34_drg_hx,y
    sta ob_xl,x
    lda s34_drg_hxh,y
    sta ob_xh,x
    lda s34_drg_hy,y
    sta ob_yl,x
    lda s34_drg_hyh,y
    sta ob_yh,x
    rts

@u_head:
    ; If spawned by scheduler instead of trigger, init segments via spawn_dragon_boss
    cpx s34_drg_head_slot
    beq @head_go
    ; Already have a dragon? Free duplicate.
    lda s34_drg_head_slot
    bpl @dup_free
    jsr free_slot
    jmp spawn_dragon_boss
@dup_free:
    jmp free_slot

@head_go:
    ; Step 16-direction velocity
    ldy ob_aux,x
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    jsr move_x
    jsr move_y
    ; Record head position into ring buffer
    inc s34_drg_idx
    lda s34_drg_idx
    and #63
    sta s34_drg_idx
    tay
    lda ob_xl,x
    sta s34_drg_hx,y
    lda ob_xh,x
    sta s34_drg_hxh,y
    lda ob_yl,x
    sta s34_drg_hy,y
    lda ob_yh,x
    sta s34_drg_hyh,y

    ; Fire mouth animation restore
    lda ob_t2,x
    beq @no_mouth
    dec ob_t2,x
    bne @no_mouth
    jsr @set_head_anim
@no_mouth:
    ; Periodically breathe horizontal fire (every 64 frames if Y within +-72)
    inc ob_t1,x
    lda ob_t1,x
    and #$3f
    bne @no_fire
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #72
    tay
    lda oz_dy+1
    adc #0
    bne @no_fire
    cpy #144
    bcs @no_fire
    jsr fire_dragon_flame
    ldx oz_i
    lda #12
    sta ob_t2,x
    lda ob_dir,x
    beq @fm_r
    lda #<rf_53C2
    ldy #>rf_53C2
    jsr set_anim
    jmp @no_fire
@fm_r:
    lda #<rf_53B4
    ldy #>rf_53B4
    jsr set_anim
@no_fire:
    ldx oz_i
    ; Steering every 10 frames
    dec ob_t0,x
    beq :+
    rts
:
    lda #10
    sta ob_t0,x
    ; Check horizontal turn-around relative to camera bounds [cam + 24, cam + 232]
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    bmi @turn_right
    bne @turn_left
    lda oz_t0
    cmp #28
    bcc @turn_right
    cmp #228
    bcs @turn_left
    ; Normal sinusoidal vertical steering toward player height
    jsr calc_dy_player
    lda oz_dy+1
    bmi @steer_up
    bne @steer_dn
    lda oz_dy
    cmp #96
    bcs @steer_dn
    ; Gentle wave in current facing direction
    jsr rng
    and #3
    tay
    lda ob_dir,x
    beq @wave_r
    lda @drg_l_dirs,y
    sta ob_aux,x
    rts
@wave_r:
    lda @drg_r_dirs,y
    sta ob_aux,x
    rts
@steer_up:
    lda ob_dir,x
    beq :+
    lda #10                     ; up-left
    bne @set_d
:
    lda #14                     ; up-right
    bne @set_d
@steer_dn:
    lda ob_dir,x
    beq :+
    lda #6                      ; down-left
    bne @set_d
:
    lda #2                      ; down-right
@set_d:
    sta ob_aux,x
    rts
@turn_right:
    lda #0
    sta ob_dir,x
    sta ob_aux,x
    jmp @set_head_anim
@turn_left:
    lda #1
    sta ob_dir,x
    lda #8
    sta ob_aux,x
@set_head_anim:
    lda ob_dir,x
    beq :+
    lda #<rf_5352
    ldy #>rf_5352
    jmp set_anim
:
    lda #<rf_5344
    ldy #>rf_5344
    jmp set_anim

@drg_l_dirs: .byte 7, 8, 9, 8
@drg_r_dirs: .byte 15, 0, 1, 0

; --- Projectiles, Items, Effects, Key -------------------------------------------------
fire_aimed_shot:
    jsr alloc
    bcc :+
    rts
:
    lda #ok_shot
    sta ob_kind,x
    inc oz_kind_cnt+ok_shot
    ldy oz_i
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    sta ob_yl,x
    lda ob_yh,y
    sta ob_yh,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    jsr face_player
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #$80
    sta ob_vxf,x
    jsr calc_dy_player
    lda oz_dy+1
    bmi @up
    lda #$ff
    sta ob_vyi,x
    lda #$80
    bne @sy
@up:
    lda #0
    sta ob_vyi,x
    lda #$80
@sy:
    sta ob_vyf,x
    lda #<rf_FD02
    ldy #>rf_FD02
    jmp set_anim

; A = signed Y offset from pillar
fire_pillar_shot:
    sta oz_t2
    jsr alloc
    bcc :+
    rts
:
    lda #ok_shot
    sta ob_kind,x
    inc oz_kind_cnt+ok_shot
    ldy oz_i
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    clc
    adc oz_t2
    sta ob_yl,x
    lda ob_yh,y
    ldy oz_t2
    bpl :+
    adc #$ff
    jmp :++
:
    adc #0
:
    sta ob_yh,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    ldy oz_i
    lda ob_dir,y
    sta ob_dir,x
    lda #2
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    sta ob_vyi,x
    sta ob_vyf,x
    lda #<rf_FD32
    ldy #>rf_FD32
    jmp set_anim

fire_dragon_flame:
    jsr alloc
    bcc :+
    rts
:
    lda #ok_shot
    sta ob_kind,x
    inc oz_kind_cnt+ok_shot
    ldy oz_i
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    sec
    sbc #5
    sta ob_yl,x
    lda ob_yh,y
    sbc #0
    sta ob_yh,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda ob_dir,y
    sta ob_dir,x
    beq @fl_r
    lda #$fc                    ; -3.125 px/f
    sta ob_vxi,x
    lda #$e0
    bne @fl_vx
@fl_r:
    lda #3                      ; +3.125 px/f
    sta ob_vxi,x
    lda #$20
@fl_vx:
    sta ob_vxf,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    lda #<rf_FD9C
    ldy #>rf_FD9C
    jmp set_anim

u_shot:
    jsr move_x
    jsr move_y
    jmp offscreen_cull

u_item:
    dec ob_t0,x
    bne :+
    jmp free_slot
:
    ; Touch player?
    jsr calc_dx_player
    lda oz_dx
    clc
    adc #16
    tay
    lda oz_dx+1
    adc #0
    bne @it_ret
    cpy #32
    bcs @it_ret
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #24
    tay
    lda oz_dy+1
    adc #0
    bne @it_ret
    cpy #48
    bcs @it_ret
    lda #snd_se_item
    jsr call_bank8_play_se
    ldx oz_i
    lda ob_var,x
    beq @armor
    lda #5                      ; 500 pts bonus
    jsr ap_add_score
    ldx oz_i
    jmp free_slot
@armor:
    lda #ap_armor_full
    sta ap_armor
    jmp free_slot
@it_ret:
    rts

u_boom:
    dec ob_t0,x
    bne :+
    jmp free_slot
:
    rts

u_popup:
    dec ob_t0,x
    bne :+
    jmp free_slot
:
    rts

u_disc:
    jmp u_item

u_trigger:
    jsr calc_dx_player
    lda oz_dx
    clc
    adc #14
    tay
    lda oz_dx+1
    adc #0
    bne @tr_ret
    cpy #28
    bcs @tr_ret
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #20
    tay
    lda oz_dy+1
    adc #0
    bne @tr_ret
    cpy #40
    bcs @tr_ret
    lda #snd_se_item
    jsr call_bank8_play_se
    ldx oz_i
    lda ob_var,x
    beq @tr_arm
    lda #5
    jsr ap_add_score
    ldx oz_i
    jmp free_slot
@tr_arm:
    lda #ap_armor_full
    sta ap_armor
    jmp free_slot
@tr_ret:
    rts

; Boss Key + 181-frame clear walk (matches Stage 1 & 2).
u_key:
    lda ob_state,x
    bne @key_wait
    ; Drop key to player ground level
    inc ob_state,x
    lda #snd_se_key
    jsr call_bank8_play_se
    ldx oz_i
    lda #<rf_5AD1
    ldy #>rf_5AD1
    jmp set_anim
@key_wait:
    ; Touch player?
    jsr calc_dx_player
    lda oz_dx
    clc
    adc #18
    tay
    lda oz_dx+1
    adc #0
    bne @k_ret
    cpy #36
    bcs @k_ret
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #28
    tay
    lda oz_dy+1
    adc #0
    bne @k_ret
    cpy #56
    bcs @k_ret
    ; Key collected! Clear all objects & shots and start 181F clear walk.
    ldx #ob_max - 1
    lda #0
:
    sta ob_kind,x
    sta ob_flags,x
    dex
    bpl :-
    ldx #ap_max_shots - 1
:
    sta ps_active,x
    dex
    bpl :-
    sta player_left
    sta ap_crouch
    sta ap_shoot_lock
    sta ap_clear_key_y
    lda #ap_state_ground
    sta ap_state
    lda #1
    sta ap_key_collected
    sta ap_clear_seq
    lda #40
    sta ap_clear_timer
    lda #2                      ; 200 pts
    jsr ap_add_score
    lda #snd_song_clear
    jmp call_bank8_play_song
@k_ret:
    rts

; --- Player Hit & Shot Collision ------------------------------------------------------
check_player_hit:
    lda ap_invuln
    ora ap_clear_seq
    ora opt_invincible
    bne @ph_ret
    lda ap_state
    cmp #ap_state_knockback
    beq @ph_ret
    cmp #ap_state_dead
    beq @ph_ret
    ldy ob_kind,x
    jsr calc_dx_player
    lda oz_dx
    clc
    adc ok_px,y
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @ph_ret
    lda ok_px,y
    asl a
    cmp oz_t0
    bcc @ph_ret
    jsr calc_dy_player
    lda oz_dy
    sec
    sbc ok_pylo,y               ; dy - (-pylo) = dy + |pylo|
    sta oz_t0
    lda oz_dy+1
    sbc #$ff
    bne @ph_ret
    lda ok_pyhi,y
    sec
    sbc ok_pylo,y
    cmp oz_t0
    bcc @ph_ret
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    jsr ap_apply_player_damage
    ldx oz_i
    lda ob_kind,x
    cmp #ok_shot
    bne @ph_ret
    jmp free_slot
@ph_ret:
    rts

; Shot collision called from ap_update_shots (X = shot index 0..3).
; Returns C=1 if shot hit an object (consumes shot), C=0 otherwise.
shot_check:
    stx oz_si
    jsr compute_coords
    ; Convert shot FC Y to arcade Y -> oz_t2 (lo) / oz_ptr (hi)
    lda campaign_stage
    cmp #3
    beq @sh_s3
    lda #$ff
    sec
    sbc ps_y,x
    sta oz_t2
    lda #0
    sbc ps_y_hi,x
    sta oz_ptr
    jmp @sh_scan
@sh_s3:
    lda #$bf
    sec
    sbc ps_y,x
    sta oz_t2
    lda #$03
    sbc ps_y_hi,x
    sta oz_ptr
@sh_scan:
    ldx #0
@sh_lp:
    stx oz_i
    lda ob_kind,x
    bne :+
    jmp @sh_nx
:
    lda ob_flags,x
    and #ofl_shoot
    bne :+
    jmp @sh_nx
:
    ldy ob_kind,x
    ; Check X
    lda ob_xl,x
    ldx oz_si
    sec
    sbc ps_x_lo,x
    sta oz_dx
    ldx oz_i
    lda ob_xh,x
    ldx oz_si
    sbc ps_x_hi,x
    sta oz_dx+1
    lda oz_dx
    clc
    adc ok_sx,y
    sta oz_t0
    lda oz_dx+1
    adc #0
    beq :+
    jmp @sh_nx
:
    lda ok_sx,y
    asl a
    cmp oz_t0
    bcs :+
    jmp @sh_nx
:
    ; Check Y
    ldx oz_i
    lda ob_yl,x
    sec
    sbc oz_t2
    sta oz_dy
    lda ob_yh,x
    sbc oz_ptr
    sta oz_dy+1
    lda oz_dy
    sec
    sbc ok_sylo,y
    sta oz_t0
    lda oz_dy+1
    sbc #$ff
    bne @sh_nx
    lda ok_syhi,y
    sec
    sbc ok_sylo,y
    cmp oz_t0
    bcc @sh_nx
    ; HIT! Check Fire Pillar (invulnerable, absorbs shot)
    cpy #ok_fire
    beq @sh_absorb
    ; Check Shield (Stone Pillar waiting)
    lda ob_flags,x
    and #ofl_shield
    beq @sh_dmg
    lda #snd_se_shield
    jsr call_bank8_play_se
@sh_absorb:
    ldx oz_si
    sec
    rts
@sh_dmg:
    ; Dragon body/tail hit: destroy trailing-most segment!
    cpy #ok_dragon
    bne @normal_dmg
    lda ob_var,x
    beq @normal_dmg
    jmp @dragon_body_hit
@normal_dmg:
    lda ob_hp,x
    beq @kill
    dec ob_hp,x
    lda #6
    sta ob_flash,x
    lda #snd_se_hit
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts
@kill:
    ; Award points
    lda ok_points,y
    beq :+
    jsr ap_add_score
:
    ldx oz_i
    lda ob_kind,x
    cmp #ok_dragon
    beq @kill_dragon_head
    ; Spawn pot drop if carrying
    lda ob_carry,x
    beq @no_drop
    jsr drop_item
@no_drop:
    ldx oz_i
    jsr make_boom
    lda #snd_se_kill
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts
@sh_nx:
    ldx oz_i
    inx
    cpx #ob_max
    beq :+
    jmp @sh_lp
:
    ldx oz_si
    clc
    rts

@dragon_body_hit:
    lda #2                      ; 200 pts per body segment
    jsr ap_add_score
    lda s34_drg_len
    beq @db_done
    dec s34_drg_len
    ldy s34_drg_len
    ldx s34_drg_seg_slot,y
    jsr make_boom
    lda #snd_se_kill
    jsr call_bank8_play_se
@db_done:
    ldx oz_si
    sec
    rts

@kill_dragon_head:
    ; Extra 1,000 pts for Dragon head (total 1,200)
    lda #10
    jsr ap_add_score
    ; Destroy all remaining body/tail segments
    ldy s34_drg_len
    beq @dh_key
@dh_lp:
    dey
    sty oz_t2
    ldx s34_drg_seg_slot,y
    jsr make_boom
    ldy oz_t2
    bne @dh_lp
    lda #0
    sta s34_drg_len
@dh_key:
    lda #$ff
    sta s34_drg_head_slot
    ; Turn head slot into Boss Key!
    ldx oz_i
    lda #ok_key
    sta ob_kind,x
    lda #0
    sta ob_state,x
    sta ob_flags,x
    lda #ofl_alive
    sta ob_flags,x
    ; Drop key at player's ground Y
    lda oz_py
    sta ob_yl,x
    lda oz_py+1
    sta ob_yh,x
    lda #snd_se_boss_die
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts

make_boom:
    ldy ob_kind,x
    beq @mb_set
    lda oz_kind_cnt,y
    beq @mb_set
    sec
    sbc #1
    sta oz_kind_cnt,y
@mb_set:
    lda #ok_boom
    sta ob_kind,x
    inc oz_kind_cnt+ok_boom
    lda #0
    sta ob_flags,x
    lda #18
    sta ob_t0,x
    lda #<rf_E7A3
    ldy #>rf_E7A3
    jmp set_anim

drop_item:
    jsr alloc
    bcc :+
    rts
:
    lda #ok_item
    sta ob_kind,x
    inc oz_kind_cnt+ok_item
    ldy oz_i
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    sta ob_yl,x
    lda ob_yh,y
    sta ob_yh,x
    lda #180
    sta ob_t0,x
    lda #ofl_alive
    sta ob_flags,x
    lda ap_armor
    cmp #ap_armor_full
    beq @bonus_drop
    lda #0                      ; Armor
    sta ob_var,x
    lda #<rf_5B0C
    ldy #>rf_5B0C
    jmp set_anim
@bonus_drop:
    lda #1                      ; Bonus
    sta ob_var,x
    lda #<rf_5B32
    ldy #>rf_5B32
    jmp set_anim

.endscope
