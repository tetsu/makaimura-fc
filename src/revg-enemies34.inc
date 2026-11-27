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
ok_pylo:   .byte  0,$F4,$F8,$F6,$F8,$ED,$ED,$D8,$DE,$F5,$F0,  0,  0,  0,  0,  0,  0
ok_pyhi:   .byte  0, 12,  8, 10,  8, 19, 19, 40, 34, 11, 16,  0,  0,  0,  0,  0,  0

ok_upd_lo:
    .byte <ret_rts, <u_pig, <u_bat, <u_devil, <u_flyer, <u_arremer, <u_zombie
    .byte <u_pillar, <u_fire, <u_dragon, <u_shot, <u_item, <u_boom, <u_popup
    .byte <u_disc, <u_trigger, <u_key
ok_upd_hi:
    .byte >ret_rts, >u_pig, >u_bat, >u_devil, >u_flyer, >u_arremer, >u_zombie
    .byte >u_pillar, >u_fire, >u_dragon, >u_shot, >u_item, >u_boom, >u_popup
    .byte >u_disc, >u_trigger, >u_key

; Scheduler simultaneous limits (balanced to arcade pacing):
; Stage 3: pig=1, flyer=1, arremer=1, zombie=2, dragon=1
; Stage 4: devil=2, dragon=1
sch_limit_s3: .byte 1, 1, 1, 2, 1
sch_limit_s4: .byte 2, 1

.include "revg-section2-spawns.inc"
.include "revg-section3-spawns.inc"

; Compact 16-direction velocity tables (0=R, 4=D, 8=L, 12=U; vy[d] == vx[(d+4)&15]):
;   0: 2.26 px/F (Arremer aimed), 16: 2.50 px/F (Devil fast), 32: 0.38 px/F (Devil slow),
;  48: 2.00 px/F (Dragon $5370),  64: 1.55 px/F (Woody Pig $A0D0).
vdir_xi:
    .byte $02,$02,$01,$00,$00,$FF,$FE,$FD,$FD,$FD,$FE,$FF,$00,$00,$01,$02
    .byte $02,$02,$01,$00,$00,$FF,$FE,$FD,$FD,$FD,$FE,$FF,$00,$00,$01,$02
    .byte $00,$00,$00,$00,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$00,$00,$00
    .byte $02,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
    .byte $01,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
vdir_xf:
    .byte $42,$17,$99,$DC,$00,$24,$67,$E9,$BE,$E9,$67,$24,$00,$DC,$99,$17
    .byte $80,$4E,$C4,$F4,$00,$0C,$3C,$B2,$80,$B2,$3C,$0C,$00,$F4,$C4,$4E
    .byte $61,$5A,$45,$25,$00,$DB,$BB,$A6,$9F,$A6,$BB,$DB,$00,$25,$45,$5A
    .byte $00,$DA,$6A,$C3,$00,$3D,$96,$26,$00,$26,$96,$3D,$00,$C3,$6A,$DA
    .byte $8D,$6F,$19,$98,$00,$68,$E7,$91,$73,$91,$E7,$68,$00,$98,$19,$6F
; 16-entry quarter-sine (0..126) for Bat wave & Flyer sine leap
s34_qsine:
    .byte   0, 12, 25, 37, 49, 60, 71, 81, 90, 98,106,112,117,122,125,126
; Arcade Dragon segment lag table (frames behind head): 10,17,24,31,38,45,52,58
drg_lags:
    .byte 10, 17, 24, 31, 38, 45, 52, 58

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
    sta s2_plat_zone
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

; Probe solid/oneway floor at object X's (ob_x, ob_y). Returns C=1 if on ground.
; Stage 3: Y_top = $03BF (959). Stage 4: Y_top = $00FF (255).
on_ground:
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    lda campaign_stage
    cmp #3
    beq @g_s3
    lda #$ff
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda #0
    sbc ob_yh,x
    sta ap_probe_y_hi
    jmp @g_chk
@g_s3:
    lda #$bf
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda #$03
    sbc ob_yh,x
    sta ap_probe_y_hi
@g_chk:
    bmi @g_no
    txa
    pha
    jsr ap_tile_type_at
    sta oz_t0
    pla
    tax
    lda oz_t0
    cmp #tile_type_solid
    beq @g_yes
    cmp #tile_type_oneway
    beq @g_yes
@g_no:
    clc
    rts
@g_yes:
    sec
    rts

; Snap object X onto the top of the ground metatile just probed by on_ground.
snap_ground:
    lda ap_probe_y
    and #$f0
    sta oz_t0
    lda campaign_stage
    cmp #3
    beq @sn_s3
    lda #$fe
    sec
    sbc oz_t0
    sta ob_yl,x
    lda #0
    sbc ap_probe_y_hi
    sta ob_yh,x
    jmp @sn_clr
@sn_s3:
    lda #$be
    sec
    sbc oz_t0
    sta ob_yl,x
    lda #$03
    sbc ap_probe_y_hi
    sta ob_yh,x
@sn_clr:
    lda #0
    sta ob_yf,x
    sta ob_vyf,x
    sta ob_vyi,x
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

; Load 16-dir velocity: A = dir (0..15), Y = table base (0/16/32/48/64).
load_vdir:
    sty oz_t0
    and #15
    sta oz_t1
    clc
    adc oz_t0
    tay
    lda vdir_xi,y
    sta ob_vxi,x
    lda vdir_xf,y
    sta ob_vxf,x
    lda oz_t1
    clc
    adc #4
    and #15
    clc
    adc oz_t0
    tay
    lda vdir_xi,y
    sta ob_vyi,x
    lda vdir_xf,y
    sta ob_vyf,x
    rts

; Set ob_vx = +-(Y.A) by ob_dir,x and zero ob_vy.
set_vx_dir:
    sta oz_t0
    sty oz_t1
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    lda ob_dir,x
    bne @neg
    lda oz_t0
    sta ob_vxf,x
    lda oz_t1
    sta ob_vxi,x
    rts
@neg:
    lda #0
    sec
    sbc oz_t0
    sta ob_vxf,x
    lda #0
    sbc oz_t1
    sta ob_vxi,x
    rts

; Exact 16-dir index (0=R, 4=D, 8=L, 12=U) from object X toward player -> A.
; Note: calc_dx_player gives ob_x - player_x, calc_dy_player gives ob_y - oz_py.
dir_to_player16:
    jsr calc_dx_player
    jsr calc_dy_player
    ; Target dx = player_x - ob_x = -oz_dx; sign bit (left) = !oz_dx+1.7
    lda oz_dx+1
    eor #$80
    sta oz_t2                   ; bit 7 = player is left of object
    lda oz_dx+1
    bpl @dx_pos
    lda #0
    sec
    sbc oz_dx
    sta oz_t0
    lda #0
    sbc oz_dx+1
    beq @dx_ok
    lda #255
    sta oz_t0
    bne @dx_ok
@dx_pos:
    lda oz_dx
    sta oz_t0
    lda oz_dx+1
    beq @dx_ok
    lda #255
    sta oz_t0
@dx_ok:
    ; oz_dy = ob_y - player_y (positive = player is BELOW object = down)
    lda oz_dy+1
    sta oz_t3                   ; bit 7 = player is ABOVE object (up)
    bpl @dy_pos
    lda #0
    sec
    sbc oz_dy
    sta oz_t1
    lda #0
    sbc oz_dy+1
    beq @dy_ok
    lda #255
    sta oz_t1
    bne @dy_ok
@dy_pos:
    lda oz_dy
    sta oz_t1
    lda oz_dy+1
    beq @dy_ok
    lda #255
    sta oz_t1
@dy_ok:
    lda oz_t0
    lsr a
    lsr a
    cmp oz_t1
    bcs @s0
    lda oz_t1
    lsr a
    lsr a
    cmp oz_t0
    bcs @s4
    lda oz_t0
    lsr a
    cmp oz_t1
    bcs @s1
    lda oz_t1
    lsr a
    cmp oz_t0
    bcs @s3
    lda #2
    bne @mq
@s0:
    lda #0
    beq @mq
@s1:
    lda #1
    bne @mq
@s3:
    lda #3
    bne @mq
@s4:
    lda #4
@mq:
    sta oz_t0
    bit oz_t2
    bmi @ql
    bit oz_t3
    bmi @qur
    lda oz_t0
    rts
@qur:
    lda #16
    sec
    sbc oz_t0
    and #15
    rts
@ql:
    bit oz_t3
    bmi @qul
    lda #8
    sec
    sbc oz_t0
    rts
@qul:
    lda #8
    clc
    adc oz_t0
    rts

; Check |oz_dx| <= A (C=1 if true).
abs_dx_le:
    sta oz_t0
    lda oz_dx
    clc
    adc oz_t0
    sta oz_t1
    lda oz_dx+1
    adc #0
    bne @no
    lda oz_t0
    asl a
    cmp oz_t1
    rts
@no:
    clc
    rts

; Check |oz_dy| <= A (C=1 if true).
abs_dy_le:
    sta oz_t0
    lda oz_dy
    clc
    adc oz_t0
    sta oz_t1
    lda oz_dy+1
    adc #0
    bne @no
    lda oz_t0
    asl a
    cmp oz_t1
    rts
@no:
    clc
    rts

; Check player within |dx|<=A and |dy|<=Y (C=1 if inside).
rel_box_le:
    pha
    tya
    pha
    jsr calc_dx_player
    jsr calc_dy_player
    pla
    jsr abs_dy_le
    pla
    bcc @rbn
    jmp abs_dx_le
@rbn:
    clc
    rts

; Check if any active player shot is within +-A px horizontally of ob_x (C=1 if found).
shot_near_x:
    sta oz_t2
    ldy #ap_max_shots - 1
@sn_lp:
    lda ps_active,y
    beq @sn_nx
    lda ps_x_lo,y
    sec
    sbc ob_xl,x
    sta oz_t0
    lda ps_x_hi,y
    sbc ob_xh,x
    sta oz_t1
    lda oz_t0
    clc
    adc oz_t2
    sta oz_t0
    lda oz_t1
    adc #0
    bne @sn_nx
    lda oz_t2
    asl a
    cmp oz_t0
    bcs @sn_y
@sn_nx:
    dey
    bpl @sn_lp
    clc
    rts
@sn_y:
    sec
    rts

; Arcade $8DC5: Despawn if X outside [cam-128, cam+383] OR Y outside [camy-64, camy+319].
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
    adc #128
    lda oz_t1
    adc #0
    cmp #2
    bcs offscreen_do_cull
offscreen_y_cull:
    lda ob_yl,x
    sec
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    sta oz_t1
    lda oz_t0
    clc
    adc #64
    sta oz_t0
    lda oz_t1
    adc #0
    beq offscreen_keep
    cmp #1
    bne offscreen_do_cull
    lda oz_t0
    cmp #128
    bcc offscreen_keep
offscreen_do_cull:
    jsr free_slot
    sec
    rts
offscreen_keep:
    clc
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
    clc
    adc #32                     ; arcade pacing: avoid overlapping enemy floods
    bcc :+
    lda #$ff
:
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
    jsr s4_sync_platforms
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
    jsr call_spawn_dragon_boss
    jmp @upd_loop
@chk_s4_boss:
    lda camera_high
    cmp #10                     ; page 10 (X >= 2560)
    bcc @upd_loop
    jsr call_spawn_dragon_boss
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
    lda oz_ptr+1
    cmp #$c0
    bcs @call_local
    lda prg_bank0
    pha
    lda #15
    jsr select_bank0
    ldx oz_i
    jsr @call_local
    pla
    jmp select_bank0
@call_local:
    jmp (oz_ptr)

call_spawn_dragon_boss:
    lda prg_bank0
    pha
    lda #15
    jsr select_bank0
    jsr spawn_dragon_boss
    pla
    jmp select_bank0

; --- Enemy AI Routines (non-ground AIs in BANK15 $8000, ground AIs in BANK51 $C000) ---
.segment "BANK15"
; 1. Woody Pig ($00 var 1, Arcade $9ED0-$A3B0):
;    State 1: 48F invuln glowing eyes (rf_4764/rf_475A).
;    State 2: Flat flight at 1.55 px/F (1/4 fast 2.25 px/F), no spurious Y bob.
;    State 3 ($0101): 8-step somersault loop (4F or 6F/step, 1.55 px/F 16-dir table).
;    State 4 ($0102): Stop-and-fire (26F pause + 2.27 px/F horiz/down spear + 20F recovery).
u_pig:
    lda ob_state,x
    beq @pg_init
    cmp #1
    beq @pg_eyes
    cmp #2
    bne :+
    jmp @pg_fly
:
    cmp #3
    bne :+
    jmp @pg_loop
:
    jmp @pg_shoot
@pg_init:
    lda #1
    sta ob_state,x
    lda #0
    sta ob_flags,x              ; invuln during 48F eyes intro
    jsr face_player
    jsr rng
    and #7
    sta ob_by_h,x               ; loops remaining (0..7)
    jsr rng
    and #3
    bne :+
    lda #1                      ; bit 0: 1/4 fast 2.25 px/F
    bne @pg_sbit
:
    lda #0
@pg_sbit:
    sta oz_t0
    jsr rng
    and #2                      ; bit 1: 6F vs 4F loop step
    ora oz_t0
    sta ob_by_l,x
    lda #48
    sta ob_t0,x
    lda #0
    sta ob_mirror,x
    lda ob_dir,x
    beq :+
    lda #<rf_4764
    ldy #>rf_4764
    jmp set_anim
:
    lda #<rf_475A
    ldy #>rf_475A
    jmp set_anim
@pg_eyes:
    dec ob_t0,x
    bne @pg_ret
    jsr face_player
@pg_enter_fly:
    lda #2
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #0
    sta ob_mirror,x
    ldy ob_dir,x
    beq :+
    lda #8
:
    sta ob_aux,x
    jsr @pg_set_fly_v
    jsr rng
    and #31
    clc
    adc #40
    sta ob_t0,x                 ; spear cooldown
    lda ob_dir,x
    beq :+
    lda #<rf_4974
    ldy #>rf_4974
    jmp set_anim
:
    lda #<rf_4966
    ldy #>rf_4966
    jmp set_anim
@pg_ret:
    rts
@pg_fly:
    jsr move_x
    dec ob_t0,x
    bne @pg_chk_lp
    ; Enter $0102 stop-and-fire (26F stop + spear + 20F hold)
    lda #4
    sta ob_state,x
    lda #46
    sta ob_t0,x
    jmp offscreen_cull
@pg_chk_lp:
    lda ob_by_h,x
    beq @pg_cull
    lda frames
    and #15
    bne @pg_cull
    jsr calc_dx_player
    lda #112
    jsr abs_dx_le
    bcc @pg_cull
    jsr rng
    and #3
    bne @pg_cull
    dec ob_by_h,x
    lda #3
    sta ob_state,x
    lda #8
    sta ob_step,x               ; 8 steps around 16-dir circle (+-2 each)
    jsr @pg_lstep_t
    sta ob_t1,x
@pg_cull:
    jmp offscreen_cull
@pg_loop:
    jsr move_x
    jsr move_y
    dec ob_t1,x
    bne @pg_cull
    jsr @pg_lstep_t
    sta ob_t1,x
    lda ob_dir,x
    beq @pg_lcw
    lda ob_aux,x
    clc
    adc #2
    bne @pg_lset
@pg_lcw:
    lda ob_aux,x
    sec
    sbc #2
@pg_lset:
    and #15
    sta ob_aux,x
    ldy #64
    jsr load_vdir
    dec ob_step,x
    bne @pg_cull
    jmp @pg_enter_fly
@pg_shoot:
    dec ob_t0,x
    lda ob_t0,x
    cmp #20
    bne :+
    jsr face_player
    jsr fire_spear_shot
    ldx oz_i
    rts
:
    cmp #0
    bne @pg_cull
    jmp @pg_enter_fly
@pg_set_fly_v:
    lda ob_by_l,x
    and #1
    beq @pg_norm
    lda #$40                    ; 2.25 px/F
    ldy #2
    jmp set_vx_dir
@pg_norm:
    lda #$8d                    ; 1.55 px/F
    ldy #1
    jmp set_vx_dir
@pg_lstep_t:
    lda ob_by_l,x
    and #2
    beq :+
    lda #6
    rts
:
    lda #4
    rts

; 2. Cave Bat ($01 var 2, Arcade $A450-$A720):
;    Hangs on ceiling until on-screen & within 128 px; swoops 43F toward player's height
;    at +-1.25 px/F ($01.$40 / $FE.$C0), then flies horizontally with a 42F +-16px sine wave.
u_bat:
    lda ob_state,x
    beq @bt_init
    cmp #1
    beq @bt_hang
    cmp #2
    bne :+
    jmp @bt_swoop
:
    jmp @bt_wave
@bt_init:
    inc ob_state,x
    jsr face_player
    jmp @bt_anim
@bt_hang:
    jsr in_view
    bcs @bt_ret
    jsr calc_dx_player
    lda #127
    jsr abs_dx_le
    bcc @bt_ret
    jsr face_player
    lda #2
    sta ob_state,x
    lda #43                     ; arcade 43-frame swoop
    sta ob_t0,x
    lda #$40                    ; +-1.25 px/F ($01.$40 / $FE.$C0)
    ldy #1
    jsr set_vx_dir
    jsr calc_dy_player          ; oz_dy = bat_y - player_y
    lda oz_dy+1
    bmi @bt_gdrop
    bne @bt_sdrop
    lda oz_dy
    cmp #56
    bcc @bt_mdrop
@bt_sdrop:
    lda #$fe
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
    bne @bt_anim
@bt_mdrop:
    lda #$ff
    sta ob_vyi,x
    lda #$00
    sta ob_vyf,x
    beq @bt_anim
@bt_gdrop:
    lda #$ff
    sta ob_vyi,x
    lda #$c0
    sta ob_vyf,x
@bt_anim:
    lda ob_dir,x
    beq :+
    lda #<rf_49D0
    ldy #>rf_49D0
    jmp set_anim
:
    lda #<rf_49B2
    ldy #>rf_49B2
    jmp set_anim
@bt_ret:
    rts
@bt_swoop:
    jsr move_x
    jsr move_y
    dec ob_t0,x
    bne :+
    lda #3
    sta ob_state,x
    lda #0
    sta ob_t1,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
:
    jmp offscreen_cull
@bt_wave:
    jsr move_x
    ; Authentic ~42F period, +-16 px sine wave around ob_by
    inc ob_t1,x
    lda ob_t1,x
    cmp #42
    bcc :+
    lda #0
    sta ob_t1,x
:
    ; Map 0..41 -> half-wave 0..20 (+sign) or 21..41 (-sign), quarter index 0..10 -> 0..15
    cmp #21
    bcs @bt_neg
    sta oz_t0
    lda #0
    sta oz_t1
    beq @bt_q
@bt_neg:
    sec
    sbc #21
    sta oz_t0
    lda #1
    sta oz_t1
@bt_q:
    lda oz_t0
    cmp #11
    bcc :+
    lda #20
    sec
    sbc oz_t0
:
    ; Scale 0..10 -> 0..15 (idx + idx/2)
    sta oz_t0
    lsr a
    clc
    adc oz_t0
    cmp #16
    bcc :+
    lda #15
:
    tay
    lda s34_qsine,y             ; 0..126 -> >>3 = 0..15 (+-16 px)
    lsr a
    lsr a
    lsr a
    sta oz_t0
    lda oz_t1
    bne @bt_sub
    lda ob_by_l,x
    clc
    adc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    jmp offscreen_cull
@bt_sub:
    lda ob_by_l,x
    sec
    sbc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    sbc #0
    sta ob_yh,x
    jmp offscreen_cull

; 3. Petit Devil ($02 var 1, Stage 4, Arcade $A7D7-$AD10):
;    State 1: 31F invuln rise (+1.0 px/F) -> 49..80F vuln rise (+1.40 px/F) = 80..111F emerge.
;    State 2: 3 alternating fast (2.50 px/F, 80..111F) / slow (0.38 px/F, 48..95F) 16-dir curving,
;             Y >= 54 floor clamp (no bounce).
;    State 3 ($0103): Fly off horizontally at 2.50 px/F.
u_devil:
    lda ob_state,x
    beq @dv_init
    cmp #1
    beq @dv_rise
    cmp #2
    beq @dv_swoop
    jmp @dv_leave
@dv_init:
    lda #1
    sta ob_state,x
    lda #0
    sta ob_flags,x              ; invuln for first 31F
    lda #31
    sta ob_t0,x
    lda #0
    sta ob_by_l,x               ; sub-stage 0=invuln 31F, 1=vuln 49..80F
    lda #3
    sta ob_aux,x                ; 3 fast/slow cycles
    lda #1
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    lda #<rf_A893
    ldy #>rf_A893
    jmp set_anim
@dv_rise:
    jsr move_y
    dec ob_t0,x
    bne @dv_cull
    lda ob_by_l,x
    bne @dv_start_sw
    ; End 31F invuln -> 49..80F vulnerable fast rise (+1.40 px/F)
    lda #1
    sta ob_by_l,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #$65
    sta ob_vyf,x
    jsr rng
    and #31
    clc
    adc #49
    sta ob_t0,x
    jmp offscreen_cull
@dv_start_sw:
    lda #2
    sta ob_state,x
    lda #0
    sta ob_by_l,x               ; phase 0 = fast 2.50 px/F
    jsr rng
    and #31
    clc
    adc #80
    sta ob_t0,x
    jsr dir_to_player16
    sta ob_step,x
    jsr @dv_load_v
@dv_cull:
    jmp offscreen_cull
@dv_swoop:
    jsr move_x
    jsr move_y
    ; Arcade $A7FF: floor clamp Y >= 54
    lda ob_yh,x
    bne :+
    lda ob_yl,x
    cmp #54
    bcs :+
    lda #54
    sta ob_yl,x
    lda #0
    sta ob_yf,x
:
    ; Curve 16-dir heading 1 step toward player every 8F
    lda frames
    and #7
    bne @dv_ns
    jsr dir_to_player16
    sec
    sbc ob_step,x
    and #15
    beq @dv_ns
    cmp #8
    bcc @dv_cw
    lda ob_step,x
    sec
    sbc #1
    and #15
    sta ob_step,x
    jmp @dv_uv
@dv_cw:
    lda ob_step,x
    clc
    adc #1
    and #15
    sta ob_step,x
@dv_uv:
    jsr @dv_load_v
@dv_ns:
    dec ob_t0,x
    bne @dv_cull
    lda ob_by_l,x
    bne @dv_sl_dn
    lda #1
    sta ob_by_l,x
    jsr rng
    and #31
    clc
    adc #48
    sta ob_t0,x
    jsr @dv_load_v
    jmp offscreen_cull
@dv_sl_dn:
    dec ob_aux,x
    beq @dv_go_lv
    lda #0
    sta ob_by_l,x
    jsr rng
    and #31
    clc
    adc #80
    sta ob_t0,x
    jsr dir_to_player16
    sta ob_step,x
    jsr @dv_load_v
    jmp offscreen_cull
@dv_go_lv:
    lda #3
    sta ob_state,x
    lda #$80                    ; 2.50 px/F leave
    ldy #2
    jsr set_vx_dir
@dv_leave:
    jsr move_x
    jmp offscreen_cull
@dv_load_v:
    ldy #16                     ; 2.50 px/F
    lda ob_by_l,x
    beq :+
    ldy #32                     ; 0.38 px/F
:
    lda ob_step,x
    jsr load_vdir
    lda #0
    ldy ob_vxi,x
    bpl :+
    lda #1
:
    sta ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    rts

.segment "BANK51"
; 4. Purple Flyer ($04 var 2, Stage 3, Arcade $B0BF-$B510):
;    10F invuln wait -> 6-stage accelerating descent (0.19..1.97 X, -1.71 Y) ->
;    {120,160,60,16}F walk at 1.43 px/F + shot-dodge -> 32F sine leap (+32/+48 px) ->
;    {60,90,120,180}F level flight + shot-dodge dive -> gentle+fast fall -> 3,600F Arremer transform!
u_flyer:
    lda ob_state,x
    bne @fl_not0
    lda #1
    sta ob_state,x
    lda #0
    sta ob_flags,x              ; 10F invuln wait
    lda #10
    sta ob_t0,x
    lda #14                     ; 3,600F transform countdown
    sta ob_aux,x
    lda #16
    sta ob_aux2,x
    jsr face_player
    lda #<rf_B19E
    ldy #>rf_B19E
    jmp set_anim
@fl_not0:
    dec ob_aux2,x
    bne @fl_nx
    dec ob_aux,x
    bne @fl_nx
    ; Transform into Red Arremer!
    dec oz_kind_cnt+ok_flyer
    lda #ok_arremer
    sta ob_kind,x
    inc oz_kind_cnt+ok_arremer
    lda #2
    sta ob_hp,x
    lda #0
    sta ob_state,x
    rts
@fl_nx:
    lda ob_state,x
    cmp #1
    bne @fl_not1
    dec ob_t0,x
    bne @fl_ret
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #2
    sta ob_state,x
    jsr face_player
    lda #0
    sta ob_step,x
    lda #6
    sta ob_t0,x
    jsr @fl_desc_vx
@fl_ret:
    rts
@fl_not1:
    cmp #2
    bne @fl_not2
    dec ob_t0,x
    bne @fl_dmv
    lda #6
    sta ob_t0,x
    lda ob_step,x
    cmp #5
    bcs @fl_dmv
    inc ob_step,x
    jsr @fl_desc_vx
@fl_dmv:
    jsr move_x
    jsr move_y
    jsr on_ground
    bcc @fl_cull_j
    jsr snap_ground
@fl_enter_walk:
    lda #3
    sta ob_state,x
    lda #<rf_B10B
    ldy #>rf_B10B
    jsr set_anim
    jsr face_player
    jsr rng
    and #3
    tay
    lda @fl_wtbl,y
    sta ob_t0,x
    rts
@fl_cull_j:
    jmp offscreen_cull
@fl_not2:
    cmp #3
    bne @fl_not3
    lda #48
    jsr shot_near_x
    bcs @fl_leap
    lda #$6e                    ; 1.43 px/F
    ldy #1
    jsr set_vx_dir
    jsr move_x
    jsr on_ground
    bcc @fl_leap
    dec ob_t0,x
    bne @fl_cull_j
@fl_leap:
    lda #4
    sta ob_state,x
    lda #<rf_B19E
    ldy #>rf_B19E
    jsr set_anim
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #0
    sta ob_step,x
    jsr rng
    and #1
    sta ob_t0,x
    rts
@fl_not3:
    cmp #4
    bne @fl_not4
    lda #$6e
    ldy #1
    jsr set_vx_dir
    jsr move_x
    lda ob_step,x
    lsr a
    tay
    lda s34_qsine,y
    lsr a
    lsr a                       ; 0..31
    sta oz_t0
    lda ob_t0,x
    beq :+
    lda oz_t0
    lsr a
    clc
    adc oz_t0                   ; 0..46
    sta oz_t0
:
    lda ob_by_l,x
    clc
    adc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    inc ob_step,x
    lda ob_step,x
    cmp #32
    bcc @fl_cull2
    lda #5
    sta ob_state,x
    jsr face_player
    jsr rng
    and #3
    tay
    lda @fl_ftbl,y
    sta ob_t0,x
    rts
@fl_not4:
    cmp #5
    bne @fl_desc2
    lda #48
    jsr shot_near_x
    bcs @fl_go_d2
    lda #$6e
    ldy #1
    jsr set_vx_dir
    jsr move_x
    dec ob_t0,x
    bne @fl_cull2
@fl_go_d2:
    lda #6
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #$ff
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    rts
@fl_desc2:
    lda ob_step,x
    cmp #16
    bcs :+
    inc ob_step,x
    bne @fl_d2m
:
    lda #$fe
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
@fl_d2m:
    jsr move_x
    jsr move_y
    jsr on_ground
    bcc @fl_cull2
    jsr snap_ground
    jmp @fl_enter_walk
@fl_cull2:
    jmp offscreen_cull
@fl_desc_vx:
    ldy ob_step,x
    lda @fl_dxf,y
    pha
    lda @fl_dxi,y
    tay
    pla
    jsr set_vx_dir
    lda #$49
    sta ob_vyf,x
    lda #$fe                    ; -1.71 px/F
    sta ob_vyi,x
    rts
@fl_dxi: .byte $00,$00,$00,$01,$01,$01
@fl_dxf: .byte $30,$80,$E0,$40,$A0,$F8
@fl_wtbl: .byte 120,160,60,16
@fl_ftbl: .byte 60,90,120,180

; 5. Red Arremer ($05, Stages 3 & 4, Arcade $B4E6-$BC40):
;    Full 7-state machine (sleep X+-96 Y+-48 or free-wake on hit, ground walk 0.80,
;    fly-over 1.75, shoot 6F+shot+6F, spacing 2.13, shot-dodge $0105, swoop 2.13),
;    5F hit-stun, Y-only culling.
u_arremer:
    lda ob_flash,x
    beq :+
    rts
:
    lda ob_state,x
    bne @ar_not0
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_BA04
    ldy #>rf_BA04
    jsr set_anim
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
:
    lda #96
    ldy #48
    jsr rel_box_le
    bcc @ar_ret0
@ar_wake:
    lda #2
    sta ob_hp,x
    lda #0
    sta ob_aux,x
    lda #90
    sta ob_t0,x
    lda #72
    sta ob_t1,x
    lda #1
    sta ob_state,x
    lda #<rf_4C53
    ldy #>rf_4C53
    jmp set_anim
@ar_ret0:
    rts
@ar_not0:
    cmp #1
    beq @ar_walk
    cmp #2
    bne :+
    jmp @ar_fly
:
    cmp #3
    bne :+
    jmp @ar_shoot
:
    cmp #4
    bne :+
    jmp @ar_space
:
    cmp #5
    bne :+
    jmp @ar_dodge
:
    jmp @ar_swoop
@ar_walk:
    jsr face_player
    lda ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    lda #$cc                    ; 0.80 px/F
    ldy #0
    jsr set_vx_dir
    jsr move_x
    dec ob_t0,x
    bne @ar_cr
    lda ob_aux,x
    and #7
    tay
    lda @ar_ft,y
    sta ob_t0,x
    inc ob_aux,x
    lda #3
    sta ob_state,x
    lda #12
    sta ob_t2,x
    lda #<rf_4DA7
    ldy #>rf_4DA7
    jmp set_anim
@ar_cr:
    dec ob_t1,x
    bne @ar_cd
    lda ob_aux,x
    and #7
    tay
    lda @ar_rt,y
    sta ob_t1,x
    lda #6
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #43
    sta ob_t2,x
    lda #<rf_4D0F
    ldy #>rf_4D0F
    jmp set_anim
@ar_cd:
    lda #64
    jsr shot_near_x
    bcc @ar_sp
    lda #5
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    lda #<rf_4C9B
    ldy #>rf_4C9B
    jmp set_anim
@ar_sp:
    jsr calc_dx_player
    lda #47
    jsr abs_dx_le
    bcs @ar_go_sp
    lda #112
    jsr abs_dx_le
    bcs @ar_ycull
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    lda #<rf_4C9B
    ldy #>rf_4C9B
    jmp set_anim
@ar_go_sp:
    lda #4
    sta ob_state,x
    jsr rng
    and #15
    clc
    adc #10
    sta ob_t2,x
    lda ob_dir,x
    eor #1
    sta ob_step,x
@ar_ycull:
    jmp offscreen_y_cull
@ar_fly:
    lda ob_step,x
    bne @arf1
    lda #3
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr move_y
    dec ob_t2,x
    bne @ar_ycull
    lda #1
    sta ob_step,x
    lda #60
    sta ob_t2,x
    jsr face_player
    rts
@arf1:
    cmp #1
    bne @arf_dn
    lda #$c0                    ; 1.75 px/F
    ldy #1
    jsr set_vx_dir
    jsr move_x
    dec ob_t2,x
    bne @ar_ycull
    lda #2
    sta ob_step,x
    lda #20
    sta ob_t2,x
    rts
@arf_dn:
    lda #$fd
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr move_y
    jsr on_ground
    bcs @arf_ld
    dec ob_t2,x
    bne @ar_ycull
@arf_ld:
    jsr snap_ground
@ar_to_walk:
    lda #1
    sta ob_state,x
    lda #<rf_4C53
    ldy #>rf_4C53
    jmp set_anim
@ar_shoot:
    dec ob_t2,x
    lda ob_t2,x
    cmp #6
    bne :+
    jsr fire_arremer_shot
    ldx oz_i
    rts
:
    cmp #0
    bne @ar_ycull
    jmp @ar_to_walk
@ar_space:
    lda ob_dir,x
    pha
    lda ob_step,x
    sta ob_dir,x
    lda #$20                    ; 2.13 px/F
    ldy #2
    jsr set_vx_dir
    pla
    sta ob_dir,x
    jsr move_x
    dec ob_t2,x
    bne @ar_yc2
    jmp @ar_to_walk
@ar_dodge:
    lda ob_step,x
    bne @ard1
    lda #1
    sta ob_vyi,x
    lda #$20
    sta ob_vyf,x
    jsr move_y
    dec ob_t2,x
    bne @ar_yc2
    lda #1
    sta ob_step,x
    lda #40
    sta ob_t2,x
    rts
@ard1:
    cmp #1
    beq :+
    jmp @arf_dn
:
    dec ob_t2,x
    lda ob_t2,x
    cmp #20
    bne :+
    jsr fire_arremer_shot
    ldx oz_i
    rts
:
    cmp #0
    bne @ar_yc2
    lda #2
    sta ob_step,x
    lda #24
    sta ob_t2,x
    rts
@ar_swoop:
    lda #$20                    ; 2.13 px/F
    ldy #2
    jsr set_vx_dir
    jsr calc_dy_player
    lda #0
    ldy oz_dy+1
    bmi :+
    ldy oz_dy
    cpy #16
    bcc :+
    lda #$ff
:
    sta ob_vyi,x
    jsr move_x
    jsr move_y
    jsr on_ground
    bcc :+
    jsr snap_ground
:
    dec ob_t2,x
    bne @ar_yc2
    jmp @ar_to_walk
@ar_yc2:
    jmp offscreen_y_cull
@ar_ft: .byte 90,100,220,130,160,180,200,120
@ar_rt: .byte 72,136,85,92,108,144,70,49

; 6. Zombie ($0B, Stage 3, Arcade $D25A):
;    Ground scan -> var 0: 87F invuln rise (rf_5245) / var 1: immediate walk ->
;    0.81 px/F walk (360F limit, 40..103F post-pass, ledge check) -> 36F invuln sink -> free!
u_zombie:
    lda ob_state,x
    beq @z_init
    cmp #1
    beq @z_rise
    cmp #2
    bne :+
    jmp @z_walk
:
    jmp @z_sink
@z_init:
    jsr on_ground
    bcc @z_scan
    lda ob_yl,x
    clc
    adc #16
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    jsr on_ground
    bcc @z_scan
    jmp free_slot
@z_scan:
    jsr on_ground
    bcs @z_grd
    lda ob_yl,x
    sec
    sbc #8
    sta ob_yl,x
    bcs @z_scan
    dec ob_yh,x
    lda ob_yh,x
    bpl @z_scan
    jmp free_slot
@z_grd:
    jsr snap_ground
    jsr face_player
    lda #0
    sta ob_flags,x              ; invuln while rising
    sta ob_mirror,x
    lda #snd_se_rise
    jsr call_bank8_play_se
    ldx oz_i
    lda ob_var,x
    bne @z_go_walk
    lda #1
    sta ob_state,x
    lda #87                     ; arcade 87F rise
    sta ob_t0,x
    lda #<rf_5245
    ldy #>rf_5245
    jmp set_anim
@z_rise:
    dec ob_t0,x
    bne @z_ret
@z_go_walk:
    lda #2
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    jsr face_player
    lda #180                    ; 360F (ticked every 2F)
    sta ob_t0,x
    jsr rng
    and #$3f
    clc
    adc #40                     ; 40..103F post-pass
    sta ob_t1,x
    lda #0
    sta ob_step,x
    sta ob_mirror,x
    lda ob_dir,x
    beq :+
    lda #<rf_52FC
    ldy #>rf_52FC
    jmp set_anim
:
    lda #<rf_5316
    ldy #>rf_5316
    jmp set_anim
@z_ret:
    rts
@z_walk:
    lda #$cf                    ; 0.81 px/F
    ldy #0
    jsr set_vx_dir
    lda ob_xl,x
    sta oz_t3
    jsr move_x
    lda ob_xl,x
    eor oz_t3
    and #$f0
    beq @z_chk_p
    jsr on_ground
    bcc @z_go_sink
@z_chk_p:
    lda ob_step,x
    bne @z_past
    jsr calc_dx_player          ; oz_dx = ob_x - player_x
    lda ob_dir,x
    beq @z_pr
    lda oz_dx+1
    bmi @z_set_past             ; moving left & ob_x < player_x -> passed!
    bpl @z_tk
@z_pr:
    lda oz_dx+1
    bpl @z_set_past             ; moving right & ob_x >= player_x -> passed!
@z_tk:
    lda frames
    and #1
    bne @z_cull
    dec ob_t0,x
    beq @z_go_sink
    bne @z_cull
@z_set_past:
    lda #1
    sta ob_step,x
    bne @z_cull
@z_past:
    dec ob_t1,x
    beq @z_go_sink
@z_cull:
    jmp offscreen_cull
@z_go_sink:
    lda #3
    sta ob_state,x
    lda #0
    sta ob_flags,x              ; invuln while sinking
    lda #36                     ; 36F sink
    sta ob_t0,x
    lda #<rf_5245
    ldy #>rf_5245
    jmp set_anim
@z_sink:
    dec ob_t0,x
    bne @z_ret
    jmp free_slot

.segment "BANK15"
; 7. Grey Stone Pillar ($12, Stage 3, Arcade $D3CC-$D5A0):
;    25-entry x 4F wait table ($57B5) with dual independent upper (Y+7) / lower (Y-16) timers;
;    shielded while waiting; unshielded 72F firing window (supports single & $0104 double-shot);
;    never flips ob_mirror!
u_pillar:
    lda ob_state,x
    beq @pl_init
    cmp #1
    beq @pl_wait
    jmp @pl_fire
@pl_init:
    lda #1
    sta ob_state,x
    jsr face_player
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    lda #0
    sta ob_mirror,x
    sta ob_aux,x                ; table index 0..24
    jsr @pl_next_t
    sta ob_t0,x                 ; upper mouth timer (+7)
    jsr @pl_next_t
    sta ob_t1,x                 ; lower mouth timer (-16)
@pl_frame:
    lda #0
    sta ob_mirror,x
    lda ob_dir,x
    beq :+
    lda #<rf_57A1
    ldy #>rf_57A1
    jmp set_anim
:
    lda #<rf_5795
    ldy #>rf_5795
    jmp set_anim
@pl_wait:
    jsr in_view
    bcs @pl_ret
    jsr face_player
    jsr calc_dy_player
    lda #48
    jsr abs_dy_le
    bcc @pl_ret
    lda #0
    sta ob_step,x               ; bit 0 = upper due, bit 1 = lower due
    dec ob_t0,x
    bne :+
    lda #1
    sta ob_step,x
:
    dec ob_t1,x
    bne :+
    lda ob_step,x
    ora #2
    sta ob_step,x
:
    lda ob_step,x
    beq @pl_ret
    ; Drop shield for 72F firing window (single or simultaneous double-shot!)
    lda #2
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #72
    sta ob_t2,x
    jmp @pl_frame
@pl_ret:
    rts
@pl_fire:
    lda ob_t2,x
    cmp #48
    bcs :+
    cmp #36
    bcc :+
    lda #1
    sta ob_flash,x
:
    lda ob_t2,x
    cmp #42
    bne @pl_ns
    lda ob_step,x
    and #1
    beq @pl_chk_lo
    lda #7                      ; upper shot (Y + 7)
    jsr fire_pillar_shot
    ldx oz_i
    jsr @pl_next_t
    sta ob_t0,x
@pl_chk_lo:
    lda ob_step,x
    and #2
    beq @pl_ns
    lda #$f0                    ; lower shot (Y - 16)
    jsr fire_pillar_shot
    ldx oz_i
    jsr @pl_next_t
    sta ob_t1,x
@pl_ns:
    dec ob_t2,x
    bne @pl_ret
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    jmp @pl_frame
@pl_next_t:
    ldy ob_aux,x
    lda @pl_tbl,y
    asl a
    asl a                       ; * 4 frames
    iny
    cpy #25
    bcc :+
    ldy #0
:
    sty oz_t0
    lda oz_t0
    sta ob_aux,x
    lda @pl_tbl,y
    asl a
    asl a
    rts
@pl_tbl:
    .byte 16, 8,20,12,24,10,18, 6,22,14,16, 8,20,12,26,10,18, 8,22,14,16,10,20,12,18

; 8. Fire Pillar ($0E, Stage 4, Arcade $E4C8):
;    Global 32F clock + 9-entry interval table (4,6,2,6,4,6,8,2,6 x 32F);
;    127F eruption with 3 Form-10 lethal rf_5631 pulses (78-84, 92-101, 106-108F,
;    aborted early when interval == 2).
u_fire:
    lda ob_state,x
    bne @fr_act
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot
    sta ob_flags,x
    ; Stagger table index 0..8 using initial ob_t0
    lda ob_t0,x
    lsr a
    lsr a
    lsr a
    lsr a
    and #7
    sta ob_aux,x
    tay
    lda @fr_int,y
    sta ob_t1,x                 ; 32F-tick countdown
    lda #<rf_55EC
    ldy #>rf_55EC
    jmp set_anim
@fr_act:
    jsr in_view
    bcs @fr_ret
    lda oz_rng
    and #$40
    sta ob_mirror,x
    lda ob_state,x
    cmp #1
    bne @fr_erupt
    ; Wait on global 32F clock
    lda frames
    and #31
    bne @fr_ret
    dec ob_t1,x
    bne @fr_ret
    ; Start 127F eruption
    lda #2
    sta ob_state,x
    lda #0
    sta ob_t0,x
    ldy ob_aux,x
    lda @fr_int,y
    sta ob_step,x               ; remember interval (2 = short abort!)
    iny
    cpy #9
    bcc :+
    ldy #0
:
    tya
    sta ob_aux,x
    lda @fr_int,y
    sta ob_t1,x
@fr_ret:
    rts
@fr_erupt:
    inc ob_t0,x
    lda ob_t0,x
    cmp #127
    bcs @fr_end
    ; Interval 2 aborts early at frame 76 (no tall lethal pulses)
    ldy ob_step,x
    cpy #2
    bne @fr_full
    cmp #76
    bcs @fr_end
@fr_full:
    cmp #32
    bcc @fr_lo
    cmp #78
    bcc @fr_mid
    cmp #85
    bcc @fr_tall                ; pulse 1: 78..84
    cmp #92
    bcc @fr_mid
    cmp #102
    bcc @fr_tall                ; pulse 2: 92..101
    cmp #106
    bcc @fr_mid
    cmp #109
    bcc @fr_tall                ; pulse 3: 106..108
@fr_mid:
    lda #ofl_alive | ofl_shoot
    sta ob_flags,x
    lda #8
    jsr @fr_set_y
    lda #<rf_5602
    ldy #>rf_5602
    jmp set_anim
@fr_tall:
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #16
    jsr @fr_set_y
    lda #<rf_5631
    ldy #>rf_5631
    jmp set_anim
@fr_end:
    lda #1
    sta ob_state,x
@fr_lo:
    lda #ofl_alive | ofl_shoot
    sta ob_flags,x
    lda #0
    jsr @fr_set_y
    lda #<rf_55EC
    ldy #>rf_55EC
    jmp set_anim
@fr_set_y:
    clc
    adc ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    rts
@fr_int: .byte 4, 6, 2, 6, 4, 6, 8, 2, 6

; 9. Dragon Boss ($0C, Stages 3 & 4, Arcade $D691-$D9E0):
;    Exact segment lag table (10,17,24,31,38,45,52,58F), 60F coiling intro,
;    curved 48F 16-dir U-turns at screen edges, 3/16 player-Y aiming,
;    1/4-per-32F fire breath at (X+-10, Y-5) at +-3.125 px/F.
spawn_dragon_boss:
    lda #1
    sta oz_boss_seen
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
    sta ob_state,x              ; State 0: 60F coiling intro ($D6D1)
    lda #60
    sta ob_t0,x
    lda #8                      ; Direction 8 = Left
    sta ob_aux,x
    lda #1                      ; Facing left
    sta ob_dir,x
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
    ; Prefill 64-entry ring buffer
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
    lda #0
    sta ob_hp,x
    lda #1
    cpy #7
    bne :+
    lda #2
:
    sta ob_var,x
    tya
    sta ob_aux2,x               ; segment index 0..7 -> drg_lags[y]
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
    ; Body or tail: exact arcade lag drg_lags[0..7] = 10,17,24,31,38,45,52,58
    ldy ob_aux2,x
    lda s34_drg_idx
    sec
    sbc drg_lags,y
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
    cpx s34_drg_head_slot
    beq @head_go
    lda s34_drg_head_slot
    bpl @dup_free
    jsr free_slot
    jmp spawn_dragon_boss
@dup_free:
    jmp free_slot
@head_go:
    ; Step 2.0 px/F velocity via load_vdir(48)
    lda ob_aux,x
    ldy #48
    jsr load_vdir
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
    ; State 0: 60F coiling intro ($D6D1)
    lda ob_state,x
    bne @dh_active
    lda ob_t0,x
    and #7
    bne :+
    lda ob_aux,x
    clc
    adc #1
    and #15
    sta ob_aux,x
:
    dec ob_t0,x
    bne @dh_ret0
    lda #1
    sta ob_state,x
    lda #8
    sta ob_aux,x
    lda #1
    sta ob_dir,x
    lda #16
    sta ob_t0,x
    jmp @set_head_anim
@dh_ret0:
    rts
@dh_active:
    ; Fire mouth animation restore
    lda ob_t2,x
    beq @no_mouth
    dec ob_t2,x
    bne @no_mouth
    jsr @set_head_anim
@no_mouth:
    ; Every 32F: 1/4 random chance to breathe flame if player Y within +-72
    lda frames
    and #31
    bne @no_fire
    jsr rng
    and #3
    bne @no_fire
    jsr calc_dy_player
    lda #72
    jsr abs_dy_le
    bcc @no_fire
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
    ; State 2: smooth curved 48F U-turn (8 steps x 6F)
    lda ob_state,x
    cmp #2
    bne @dh_flight
    dec ob_t0,x
    bne @dh_ret0
    lda #6
    sta ob_t0,x
    lda ob_aux,x
    clc
    adc ob_by_l,x               ; +1 or -1 ($FF)
    and #15
    sta ob_aux,x
    dec ob_step,x
    lda ob_step,x
    cmp #4
    bne :+
    lda ob_dir,x
    eor #1
    sta ob_dir,x
    jsr @set_head_anim
:
    lda ob_step,x
    bne @dh_ret0
    lda #1
    sta ob_state,x
    lda #16
    sta ob_t0,x
    rts
@dh_flight:
    ; Check horizontal turn-around at camera edges [cam+28, cam+228] -> curved U-turn!
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    bmi @start_uturn_r
    beq :+
    jmp @start_uturn_l
:
    lda oz_t0
    cmp #28
    bcc @start_uturn_r
    cmp #228
    bcc :+
    jmp @start_uturn_l
:
    ; Steering every 16F: 3/16 player-Y aiming, else gentle wave
    dec ob_t0,x
    beq :+
    rts
:
    lda #16
    sta ob_t0,x
    jsr rng
    and #15
    cmp #3
    bcc @dh_aim_y
    ; Clamp Y within [camy+32, camy+200]
    lda ob_yl,x
    sec
    sbc oz_camy
    cmp #40
    bcc @steer_up
    cmp #196
    bcs @steer_dn
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
@dh_aim_y:
    jsr calc_dy_player
    lda oz_dy+1
    bmi @steer_up
    bpl @steer_dn
@steer_up:
    lda ob_dir,x
    beq :+
    lda #10
    bne @set_d
:
    lda #14
    bne @set_d
@steer_dn:
    lda ob_dir,x
    beq :+
    lda #6
    bne @set_d
:
    lda #2
@set_d:
    sta ob_aux,x
    rts
@start_uturn_r:
    lda ob_dir,x
    beq @dh_ret1                ; already heading right
    lda #2
    sta ob_state,x
    lda #8                      ; 8 steps of +-1 -> 180 deg curve
    sta ob_step,x
    lda #6
    sta ob_t0,x
    lda #1                      ; CW if low, CCW if high
    ldy ob_yl,x
    cpy #128
    bcc :+
    lda #$ff
:
    sta ob_by_l,x
@dh_ret1:
    rts
@start_uturn_l:
    lda ob_dir,x
    bne @dh_ret1                ; already heading left
    lda #2
    sta ob_state,x
    lda #8
    sta ob_step,x
    lda #6
    sta ob_t0,x
    lda #$ff
    ldy ob_yl,x
    cpy #128
    bcc :+
    lda #1
:
    sta ob_by_l,x
    rts
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

.segment "BANK51"
; --- Projectiles, Items, Effects, Key -------------------------------------------------
; 16-dir aimed shot at 2.26 px/F (6-shot cap + snd_se_efire).
fire_aimed_shot:
    lda oz_kind_cnt+ok_shot
    cmp #6
    bcc :+
    rts
:
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
    jsr dir_to_player16
    ldy #0                      ; 2.26 px/F
    jsr load_vdir
    lda #<rf_FD02
    ldy #>rf_FD02
    jsr set_anim
    lda #snd_se_efire
    jmp call_bank8_play_se

; Woody Pig spear: horizontal 2.27 px/F (rf_FD12/19) OR straight down 2.27 px/F (rf_FD27).
fire_spear_shot:
    jsr fire_aimed_shot
    lda ob_kind,x
    cmp #ok_shot
    bne @sp_out
    ldy oz_i
    lda ob_dir,y
    sta ob_dir,x
    lda #0
    sta ob_mirror,x
    ; Check if player is directly below (oz_dy > 16 and |oz_dx| <= 32)
    jsr calc_dy_player
    lda oz_dy+1
    bmi @sp_horiz
    bne @sp_chk_dx
    lda oz_dy
    cmp #16
    bcc @sp_horiz
@sp_chk_dx:
    jsr calc_dx_player
    lda #32
    jsr abs_dx_le
    bcc @sp_horiz
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #$fd                    ; -2.27 px/F straight down ($FD.$BB)
    sta ob_vyi,x
    lda #$bb
    sta ob_vyf,x
    lda #<rf_FD27
    ldy #>rf_FD27
    jmp set_anim
@sp_horiz:
    lda #$45                    ; 2.27 px/F ($02.$45)
    ldy #2
    jsr set_vx_dir
    lda ob_dir,x
    bne @sp_l
    lda #<rf_FD12
    ldy #>rf_FD12
    jmp set_anim
@sp_l:
    lda #<rf_FD19
    ldy #>rf_FD19
    jmp set_anim
@sp_out:
    rts

; Red Arremer fireball (rf_FDD7, 16-dir 2.26 px/F from Y+16).
fire_arremer_shot:
    jsr fire_aimed_shot
    lda ob_kind,x
    cmp #ok_shot
    bne @ar_so
    lda ob_yl,x
    clc
    adc #16
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    lda #<rf_FDD7
    ldy #>rf_FDD7
    jmp set_anim
@ar_so:
    rts

; --- Dynamic Stage 4 eyeball moving platforms (all 16 arcade support $05 entries) ---
s4_sync_platforms:
    lda campaign_stage
    cmp #4
    beq :+
    rts
:
    lda ap_riding_plat
    bmi :+
    rts                         ; Never swap while Arthur is riding a platform
:
    ; Switch zones at the midpoints of the 4 solid islands (X=352, 608, 864)
    ; so platforms never swap mid-chasm.
    lda player_x+1
    cmp #1
    bcc @s4_z0
    bne @s4_chk_z2
    lda player_x
    cmp #<352
    bcc @s4_z0
    bcs @s4_z1
@s4_chk_z2:
    cmp #2
    bne @s4_chk_z3
    lda player_x
    cmp #<608
    bcc @s4_z1
    bcs @s4_z2
@s4_chk_z3:
    cmp #3
    bne @s4_z3
    lda player_x
    cmp #<864
    bcc @s4_z2
@s4_z3:
    lda #3
    bne @s4_z
@s4_z2:
    lda #2
    bne @s4_z
@s4_z1:
    lda #1
    bne @s4_z
@s4_z0:
    lda #0
@s4_z:
    cmp s2_plat_zone
    beq @s4p_done
    sta s2_plat_zone
    ; Index = zone * 32 (4 slots x 8 bytes: type, x_lo, x_hi, y_lo, y_hi, span, timer, phase)
    asl a
    asl a
    asl a
    asl a
    asl a
    tay
    ldx #0
@s4_load:
    lda s4_plat_zones,y
    sta plat_type,x
    iny
    lda s4_plat_zones,y
    sta plat_x_lo,x
    iny
    lda s4_plat_zones,y
    sta plat_x_hi,x
    iny
    lda s4_plat_zones,y
    sta plat_y,x
    iny
    lda s4_plat_zones,y
    sta plat_y_hi,x
    iny
    lda s4_plat_zones,y
    sta plat_span,x
    iny
    lda s4_plat_zones,y
    sta plat_timer,x
    iny
    lda s4_plat_zones,y
    sta plat_phase,x
    iny
    lda #0
    sta plat_dx,x
    sta plat_dy,x
    inx
    cpx #ap_max_plats
    bne @s4_load
@s4p_done:
    rts

s4_plat_zones:
    ; Zone 0 (Chasm 0: X=128..303, between Island 0 [0..127] and Island 1 [304..399])
    .byte 2, <136, >136, <184, >184, 80,  0, 0
    .byte 2, <160, >160, <152, >152, 80, 32, 0
    .byte 2, <184, >184, <136, >136, 88, 16, 1
    .byte 2, <208, >208, <104, >104, 88, 48, 1
    ; Zone 1 (Chasm 1: X=400..527, between Island 1 [304..399] and Island 2 [528..687])
    .byte 2, <384, >384, <120, >120, 80,  0, 0
    .byte 2, <416, >416, <184, >184, 80, 32, 1
    .byte 2, <440, >440, <120, >120, 88, 16, 0
    .byte 2, <448, >448, <168, >168, 88, 48, 1
    ; Zone 2 (Chasm 2: X=688..815, between Island 2 [528..687] and Island 3 [816..911])
    .byte 2, <680, >680, <136, >136, 80,  0, 0
    .byte 2, <704, >704, <168, >168, 80, 32, 1
    .byte 2, <720, >720, <104, >104, 88, 16, 0
    .byte 2, <736, >736, <120, >120, 88, 48, 1
    ; Zone 3 (Chasm 3: X=912..1135, between Island 3 [816..911] and Island 4 [1136..1359])
    .byte 2, <920,  >920,  <152, >152, 96,  0, 0
    .byte 2, <960,  >960,  <120, >120, 96, 32, 1
    .byte 2, <1008, >1008, <168, >168, 96, 16, 0
    .byte 2, <1040, >1040, <136, >136, 96, 48, 1

; A = signed Y offset from pillar (6-shot cap + snd_se_efire)
fire_pillar_shot:
    sta oz_t2
    lda oz_kind_cnt+ok_shot
    cmp #6
    bcc :+
    rts
:
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
    lda #0
    ldy #2
    jsr set_vx_dir
    lda ob_dir,x
    bne @pil_l
    lda #<rf_FD32               ; arcade $FD32: Stone Pillar shot Right
    ldy #>rf_FD32
    jmp @pil_set
@pil_l:
    lda #<rf_FD40               ; arcade $FD40: Stone Pillar shot Left
    ldy #>rf_FD40
@pil_set:
    jsr set_anim
    lda #snd_se_efire
    jmp call_bank8_play_se

fire_dragon_flame:
    lda oz_kind_cnt+ok_shot
    cmp #6
    bcc :+
    rts
:
    jsr alloc
    bcc :+
    rts
:
    lda #ok_shot
    sta ob_kind,x
    inc oz_kind_cnt+ok_shot
    ldy oz_i
    lda ob_dir,y
    sta ob_dir,x
    ; Spawn at (X +- 10, Y - 5)
    lda ob_xl,y
    ldy ob_dir,x
    bne @df_xl
    clc
    adc #10
    sta ob_xl,x
    ldy oz_i
    lda ob_xh,y
    adc #0
    sta ob_xh,x
    jmp @df_y
@df_xl:
    sec
    sbc #10
    sta ob_xl,x
    ldy oz_i
    lda ob_xh,y
    sbc #0
    sta ob_xh,x
@df_y:
    ldy oz_i
    lda ob_yl,y
    sec
    sbc #5
    sta ob_yl,x
    lda ob_yh,y
    sbc #0
    sta ob_yh,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda #$20                    ; +-3.125 px/F
    ldy #3
    jsr set_vx_dir
    lda #<rf_FD9C
    ldy #>rf_FD9C
    jsr set_anim
    lda #snd_se_efire
    jmp call_bank8_play_se

u_shot:
    jsr move_x
    jsr move_y
    jmp offscreen_cull

u_item:
    dec ob_t0,x
    bne :+
    jmp free_slot
:
    ; Fall to ground if airborne so items never float mid-air where Arthur can't avoid them.
    lda ob_state,x
    bne @it_landed
    jsr on_ground
    bcs @it_snap
    lda ob_yl,x
    sec
    sbc #2
    sta ob_yl,x
    bcs @it_ret
    dec ob_yh,x
    lda ob_yh,x
    bpl @it_ret
    jmp free_slot
@it_snap:
    jsr snap_ground
    lda ob_yl,x
    sec
    sbc #8                      ; sit flush on floor for clean jump clearance
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    inc ob_state,x
@it_landed:
    ; Touch player?
    jsr calc_dx_player
    lda oz_dx
    clc
    adc #14
    tay
    lda oz_dx+1
    adc #0
    bne @it_ret
    cpy #28
    bcs @it_ret
    jsr calc_dy_player
    lda oz_dy
    clc
    adc #12
    tay
    lda oz_dy+1
    adc #0
    bne @it_ret
    cpy #24
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
    bne @ub_ret
    lda ob_var,x
    beq @ub_free
    ; Morph into score popup (var 1=Arremer 1000, var 3=Dragon 1000 + Boss Key!)
    dec oz_kind_cnt+ok_boom
    lda #ok_popup
    sta ob_kind,x
    inc oz_kind_cnt+ok_popup
    lda #0
    sta ob_mirror,x
    lda #63
    sta ob_t0,x
    lda #<rf_E878
    ldy #>rf_E878
    jmp set_anim
@ub_free:
    jmp free_slot
@ub_ret:
    rts

u_popup:
    dec ob_t0,x
    bne @up_ret
    lda ob_var,x
    cmp #3
    bne @up_free
    ; Dragon Boss Key drop after 22F boom + 63F popup!
    dec oz_kind_cnt+ok_popup
    lda #ok_key
    sta ob_kind,x
    inc oz_kind_cnt+ok_key
    lda #0
    sta ob_state,x
    lda #ofl_alive
    sta ob_flags,x
    lda oz_py
    sta ob_yl,x
    lda oz_py+1
    sta ob_yh,x
    rts
@up_free:
    jmp free_slot
@up_ret:
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
    lda #50                     ; arcade $7345: $32 frames walk to gate (step 2 plays $3B)
    sta ap_clear_timer
    lda #2                      ; 200 pts
    jmp ap_add_score
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
    ldy ap_crouch               ; crouching lowers Arthur's top edge by 6 px
    beq :+
    sec
    sbc #6
    bcs :+
    lda #0
:
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
    beq :+
    jmp @sh_nx
:
    lda ok_syhi,y
    sec
    sbc ok_sylo,y
    cmp oz_t0
    bcs :+
    jmp @sh_nx
:
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
    ; Sleeping Red Arremer wakes freely with HP reset to 2 (3 hits)
    cpy #ok_arremer
    bne @nd_chk_hp
    lda ob_state,x
    bne @nd_chk_hp
    lda #1
    sta ob_state,x
    lda #2
    sta ob_hp,x
    lda #0
    sta ob_aux,x
    lda #90
    sta ob_t0,x
    lda #72
    sta ob_t1,x
    lda #<rf_4C53
    ldy #>rf_4C53
    jsr set_anim
    lda #snd_se_hit
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts
@nd_chk_hp:
    lda ob_hp,x
    beq @kill
    dec ob_hp,x
    lda #5                      ; 5F flash/stun (Arremer hit-stun)
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
    lda ob_kind,x
    cmp #ok_arremer
    beq @kill_arr
    jsr make_boom
    jmp @kill_se
@kill_arr:
    jsr make_boom
    lda #36                     ; Arremer 36F rf_5CA0 + 1000 popup
    sta ob_t0,x
    lda #1
    sta ob_var,x
    lda #<rf_5CA0
    ldy #>rf_5CA0
    jsr set_anim
@kill_se:
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
    ; Morph Dragon head into 22F boss explosion (rf_5C7A) -> 63F 1000 popup -> Boss Key!
    ldx oz_i
    jsr make_boom
    lda #22
    sta ob_t0,x
    lda #3                      ; var 3 = spawn Boss Key after 1000 popup
    sta ob_var,x
    lda #<rf_5C7A
    ldy #>rf_5C7A
    jsr set_anim
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
    sta ob_var,x
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
