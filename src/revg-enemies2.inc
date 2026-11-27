; Stage 2 (Revision G Section 1: 魔物の村〜氷の館〜一角獣×2) enemies, items,
; triggers, and 2x Unicorn boss, placed in Bank 49 ($C000 window).
; Shares WRAM object pool (ob_*) and ZP temporaries (oz_*) with Bank 6 (Stage 1),
; since only one stage runs at a time. Drawn by Bank 7 (revg-objects.inc).

.segment "WRAM"
s2_boss_rem: .res 1             ; Remaining Unicorns (starts at 2)
s2_win_flip: .res 1             ; Alternates top-down / bottom-up window scan ($1577)
s2_win_last: .res 1             ; Last window index used (0..25, or $ff)
s2_sch_pat: .res 5              ; 5 schedulers: $02 devil, $04 flyer, $01 crow, $08 unicorn, $0B zombie
s2_sch_rec: .res 5
s2_sch_end: .res 5
s2_sch_wait: .res 5
s2_sch_once: .res 5
s2_fix_used: .res 3             ; 17 fixed enemies -> 3 bytes (bits 0..16)
s2_sup_used: .res 2             ; 16 support triggers -> 2 bytes

.segment "BANK49"
.scope s2
; Fixed jump table at $C001 matching BANK6 so BANK3 dispatches via revg_enemy_*_vec.
.assert * = $c001, lderror, "BANK49 jump table must start at $C001"
    jmp init
    jmp update
    jmp shot_check

; Object kinds in Stage 2 (match revg-section1-spawns.inc names).
ok_none = 0
ok_devil = 1                    ; Petit Devil? ($02)
ok_flyer = 2                    ; Purple Flying Enemy ($04)
ok_bigman = 3                   ; Big Man ($0A)
ok_crow = 4                     ; Red Bird ($01 var 3)
ok_plant = 5                    ; Plant ($11)
ok_zombie = 6                   ; Zombie ($0B)
ok_arremer = 7                  ; Red Arremer ($05, from $04 transform)
ok_unicorn = 8                  ; Unicorn ($08, 2x Boss)
ok_shot = 9                     ; Enemy shot ($03 Big Man / aimed)
ok_item = 10                    ; Armor / weapon / bonus drop
ok_boom = 11                    ; Explosion
ok_popup = 12                   ; Score popup
ok_disc = 13                    ; 200 / 500 pt disc
ok_trigger = 14                 ; Hidden armor / 10,000 pt trigger
ok_key = 15                     ; Stage 2 Boss Key + 181F clear sequence
ok_kinds = 16

; Per-kind tables (docs/revision-g-late-enemy-behaviour.md).
;                 -   dev fly big crw plt zom arr uni sht itm bom pop dsc trg key
ok_hp:     .byte 0, 0,  0,  9,  0,  0,  0,  2,  9,  0,  0,  0,  0,  0,  0,  0
ok_points: .byte 0, 1,  1,  5,  1,  1,  2, 10, 20,  0,  0,  0,  0,  0,  0,  0
ok_sx:     .byte 0,12, 12, 14, 12, 12, 12, 10, 16,  0,  0,  0,  0,  0,  0,  0
ok_sylo:   .byte 0,$F6,$F8,$F2,$F0,$F0,$EE,$F3,$EB, 0,  0,  0,  0,  0,  0,  0
ok_syhi:   .byte 0,10,  8, 14,  8, 16, 10, 13, 21,  0,  0,  0,  0,  0,  0,  0
ok_px:     .byte 0, 9, 10, 14, 10, 10,  9, 16, 23,  6,  0,  0,  0,  0,  0,  0
ok_pylo:   .byte 0,$F6,$F8,$EF,$F8,$F2,$ED,$ED,$E2,$F6, 0,  0,  0,  0,  0,  0
ok_pyhi:   .byte 0,10,  8, 17,  8, 14, 19, 19, 30, 10,  0,  0,  0,  0,  0,  0

ok_upd_lo:
    .byte <ret_rts, <u_devil, <u_flyer, <u_bigman, <u_crow, <u_plant, <u_zombie
    .byte <u_arremer, <u_unicorn, <u_shot, <u_item, <u_boom, <u_popup, <u_disc
    .byte <u_trigger, <u_key
ok_upd_hi:
    .byte >ret_rts, >u_devil, >u_flyer, >u_bigman, >u_crow, >u_plant, >u_zombie
    .byte >u_arremer, >u_unicorn, >u_shot, >u_item, >u_boom, >u_popup, >u_disc
    .byte >u_trigger, >u_key

; 5 scheduler simultaneous limits: devil=3, flyer=3, crow=3, unicorn=2, zombie=2.
sch_limit: .byte 3, 3, 3, 2, 2

.include "revg-section1-spawns.inc"

; 26 Petit Devil tower windows from Revision G ROM $ADF3 (local X = arcade X - 3584).
win_count = 26
win_xl: .byte 216,216,216,104,104, 88,104,  8,  8, 24,216,216,216,120,120,120,120, 72, 72,248,248,248,248,152, 72, 72
win_xh: .byte   2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0
win_yl: .byte 180,244,104, 88,184,  8,104,100,168,168,244, 72,132, 84,196, 24, 84,148,196,136,232, 72,136,100,152,244
win_yh: .byte   0,  0,  1,  0,  0,  1,  1,  0,  0,  1,  0,  1,  1,  0,  0,  1,  1,  1,  1,  0,  0,  1,  1,  0,  0,  0

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
    sta s2_win_last
    sta s2_plat_zone
    ldx #4
:
    sta s2_sch_pat,x
    dex
    bpl :-
    lda #0
    sta s2_win_flip
    sta s2_fix_used
    sta s2_fix_used+1
    sta s2_fix_used+2
    sta s2_sup_used
    sta s2_sup_used+1
    sta oz_spawns
    sta oz_drop_sel
    sta oz_key_timer
    sta oz_boss_seen
    ldx #4
:
    sta s2_sch_once,x
    dex
    bpl :-
    lda #2                      ; Section 1 has 2 Unicorn bosses ($6ED1 = 2)
    sta s2_boss_rem
    lda #$a7
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
    sta ob_flags,x
    sta ob_dir,x
    sta ob_aux,x
    sta ob_aux2,x
    sta ob_carry,x
    sta ob_cur_l,x
    sta ob_cur_h,x
    sta ob_nxt_l,x
    sta ob_nxt_h,x
    lda #$ff
    sta ob_src,x
    clc
    rts

free:
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

set_anim:
    sta ob_cur_l,x
    sta ob_nxt_l,x
    tya
    sta ob_cur_h,x
    sta ob_nxt_h,x
    lda #0
    sta ob_cnt,x
    rts

; Compute player's arcade Y = (stage_cam_y_max_page * 256 + 254) - player_feet.
player_y:
    lda #254
    sec
    sbc player_feet
    sta oz_py
    lda stage_cam_y_max_page
    sbc ap_player_y_hi
    sta oz_py+1
    rts

; oz_dx = player_x - ob_x, oz_dy = ob_y - oz_py.
rel_player:
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

face_player:
    jsr rel_player
    lda #0
    ldy oz_dx+1
    bpl :+
    lda #1
:
    sta ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    rts

; Move object by signed 8.8 (ob_vxi:ob_vxf, ob_vyi:ob_vyf).
move:
    lda ob_xf,x
    clc
    adc ob_vxf,x
    sta ob_xf,x
    lda ob_xl,x
    adc ob_vxi,x
    sta ob_xl,x
    lda ob_vxi,x
    bpl @xp
    lda ob_xh,x
    adc #$ff
    jmp @xs
@xp:
    lda ob_xh,x
    adc #0
@xs:
    sta ob_xh,x
    lda ob_yf,x
    clc
    adc ob_vyf,x
    sta ob_yf,x
    lda ob_yl,x
    adc ob_vyi,x
    sta ob_yl,x
    lda ob_vyi,x
    bpl @yp
    lda ob_yh,x
    adc #$ff
    jmp @ys
@yp:
    lda ob_yh,x
    adc #0
@ys:
    sta ob_yh,x
    rts

; Compute screen-bottom arcade Y into oz_mul: cam_y_arc = (max_page*256 + 254) - (ap_cam_y + 240).
; For Stage 2 (max_page=1): cam_y_arc = 270 - ap_cam_y (visible arcade Y = [cam_y_arc, cam_y_arc+240]).
calc_cam_y_arc:
    lda ap_cam_y_lo
    clc
    adc #240
    sta oz_t2
    lda ap_cam_y_hi
    adc #0
    sta oz_t3
    lda #254
    sec
    sbc oz_t2
    sta oz_t2
    lda stage_cam_y_max_page
    sbc oz_t3
    sta oz_t3
    rts

; C set if object is visible on screen (X in [cam_x - 16, cam_x + 271] AND Y in [cam_y_arc - 16, cam_y_arc + 255]).
on_screen_xy:
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    lda oz_t0
    clc
    adc #16
    sta oz_t0
    lda oz_t1
    adc #0
    beq @chk_y
    cmp #1
    bne @no_xy
    lda oz_t0
    cmp #32                     ; 256 + 32 = 288
    bcs @no_xy
@chk_y:
    jsr calc_cam_y_arc
    lda ob_yl,x
    sec
    sbc oz_t2
    sta oz_t0
    lda ob_yh,x
    sbc oz_t3
    sta oz_t1
    lda oz_t0
    clc
    adc #16
    sta oz_t0
    lda oz_t1
    adc #0
    beq @ok_xy
    cmp #1
    bne @no_xy
    lda oz_t0
    cmp #32
    bcc @ok_xy
@no_xy:
    clc
    rts
@ok_xy:
    sec
    rts

; C set if object X is within [cam_x - 32, cam_x + 288].
on_screen_x:
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
    sta oz_t0
    lda oz_t1
    adc #0
    beq @ok
    cmp #1
    bne @no
    lda oz_t0
    cmp #64                     ; 256 + 64 = 320
    bcc @ok
@no:
    clc
    rts
@ok:
    sec
    rts

; Arcade $8DC5: C set if object is outside X [cam_x-128, cam_x+383] OR Y [cam_y_arc-64, cam_y_arc+319].
offscreen:
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
    sta oz_t0
    lda oz_t1
    adc #0
    cmp #2                      ; [0..511] = cam_x-128 .. cam_x+383
    bcs @out
    jsr calc_cam_y_arc
    lda ob_yl,x
    sec
    sbc oz_t2
    sta oz_t0
    lda ob_yh,x
    sbc oz_t3
    sta oz_t1
    lda oz_t0
    clc
    adc #64
    sta oz_t0
    lda oz_t1
    adc #0
    beq @in
    cmp #1
    bne @out
    lda oz_t0
    cmp #128                    ; 256 + 128 = 384 (cam_y_arc-64 .. cam_y_arc+319)
    bcc @in
@out:
    sec
    rts
@in:
    clc
    rts

; Arcade $8E47 approximation: return 16-direction index (0=R, 4=D, 8=L, 12=U, CW) toward player in A.
; Uses oz_dx = player_x - ob_x, oz_dy = ob_y - oz_py (down is positive).
dir_to_player16:
    jsr rel_player
    ; |dx| in oz_t0 (clamped to 255), sign bit in oz_t2 (bit 7 = left)
    lda oz_dx+1
    sta oz_t2
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
    ; |dy| in oz_t1 (clamped to 255), sign bit in oz_t3 (bit 7 = up, since oz_dy = ob_y - py)
    lda oz_dy+1
    sta oz_t3
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
    ; Octant sector 0..4 (0=horiz, 1=22.5, 2=45, 3=67.5, 4=vert) from ratio of |dy| to |dx|
    lda oz_t0
    lsr a
    lsr a                       ; |dx|/4
    cmp oz_t1
    bcs @sec0                   ; |dy| <= |dx|/4 -> 0 (horiz)
    lda oz_t1
    lsr a
    lsr a                       ; |dy|/4
    cmp oz_t0
    bcs @sec4                   ; |dx| <= |dy|/4 -> 4 (vert)
    lda oz_t0
    lsr a                       ; |dx|/2
    cmp oz_t1
    bcs @sec1                   ; |dy| <= |dx|/2 -> 1 (22.5 deg)
    lda oz_t1
    lsr a                       ; |dy|/2
    cmp oz_t0
    bcs @sec3                   ; |dx| <= |dy|/2 -> 3 (67.5 deg)
    lda #2                      ; 45 deg
    bne @map_q
@sec0:
    lda #0
    beq @map_q
@sec1:
    lda #1
    bne @map_q
@sec3:
    lda #3
    bne @map_q
@sec4:
    lda #4
@map_q:
    ; Map (sector 0..4, left=oz_t2.7, up=oz_t3.7) to CW 16-dir (0=R, 4=D, 8=L, 12=U)
    sta oz_t0
    bit oz_t2
    bmi @q_left
    bit oz_t3
    bmi @q_ur
    lda oz_t0                   ; Down-Right: 0..4
    rts
@q_ur:
    lda #16                     ; Up-Right: (16 - sec) & 15
    sec
    sbc oz_t0
    and #15
    rts
@q_left:
    bit oz_t3
    bmi @q_ul
    lda #8                      ; Down-Left: 8 - sec (4..8)
    sec
    sbc oz_t0
    rts
@q_ul:
    lda #8                      ; Up-Left: 8 + sec (8..12)
    clc
    adc oz_t0
    rts

; Arcade 16-direction velocity tables (0=R, 4=D, 8=L, 12=U).
; Mathematical identity: vy[d] == vx[(d + 4) & 15], saving 96 bytes!
; 0: 2.26 px/F (bank4:$5F79), 16: 1.65 px/F ($A905), 32: 0.38 px/F ($A985).
vdir_xi:
    .byte $02,$02,$01,$00,$00,$FF,$FE,$FD,$FD,$FD,$FE,$FF,$00,$00,$01,$02
    .byte $01,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
    .byte $00,$00,$00,$00,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$00,$00,$00
vdir_xf:
    .byte $42,$17,$99,$DC,$00,$24,$67,$E9,$BE,$E9,$67,$24,$00,$DC,$99,$17
    .byte $A6,$86,$2B,$A2,$00,$5E,$D5,$7A,$5A,$7A,$D5,$5E,$00,$A2,$2B,$86
    .byte $61,$5A,$45,$25,$00,$DB,$BB,$A6,$9F,$A6,$BB,$DB,$00,$25,$45,$5A
; 32-entry half-sine table (0..127 -> 0) for Flyer leap & Crow bobbing
s2_half_sine:
    .byte   0,  12,  25,  37,  49,  60,  71,  81,  90,  98, 106, 112, 117, 122, 125, 126
    .byte 127, 126, 125, 122, 117, 112, 106,  98,  90,  81,  71,  60,  49,  37,  25,  12

; Load 16-dir velocity into ob_vx/ob_vy: A = dir (0..15), Y = table base (0=2.26, 16=1.65, 32=0.38).
load_vdir:
    sty oz_t0
    and #15
    sta oz_t1
    ora oz_t0
    tay
    lda vdir_xi,y
    sta ob_vxi,x
    lda vdir_xf,y
    sta ob_vxf,x
    lda oz_t1
    clc
    adc #4
    and #15
    ora oz_t0
    tay
    lda vdir_xi,y
    sta ob_vyi,x
    lda vdir_xf,y
    sta ob_vyf,x
    rts

; Set ob_vx = +-(Y.A) according to ob_dir,x (0=right, 1=left) and zero ob_vy.
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

; Check |oz_dx| <= A (C=1 if true). Preserves Y.
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
    cmp oz_t1                   ; 2*A >= oz_dx + A -> C=1 if true
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

; Check rel_player within |dx|<=A and |dy|<=Y (C=1 if inside).
rel_box_le:
    pha
    tya
    pha
    jsr rel_player
    pla
    jsr abs_dy_le
    pla
    bcc @rb_no
    jmp abs_dx_le
@rb_no:
    clc
    rts

; Check if any active player shot is within +-A horizontally of ob_x (C=1 if found).
shot_near_x:
    sta oz_t2
    ldy #ap_max_shots - 1
@snx_lp:
    lda ps_active,y
    beq @snx_nx
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
    bne @snx_nx
    lda oz_t2
    asl a
    cmp oz_t0
    bcs @snx_yes
@snx_nx:
    dey
    bpl @snx_lp
    clc
    rts
@snx_yes:
    sec
    rts

; Ground probe at (ob_x, ob_y): C set on solid/oneway tile.
on_ground:
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    lda #255
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda stage_cam_y_max_page
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
    cmp #tile_type_solid
    beq @yes
    cmp #tile_type_oneway
    beq @yes
@no:
    clc
    rts
@yes:
    sec
    rts

snap_ground:
    lda ap_probe_y
    and #$f0
    sta oz_t0
    lda #254
    sec
    sbc oz_t0
    sta ob_yl,x
    lda stage_cam_y_max_page
    sbc ap_probe_y_hi
    sta ob_yh,x
    lda #0
    sta ob_yf,x
    sta ob_vyf,x
    sta ob_vyi,x
    rts

; Spawn an aimed or straight enemy shot from object X (with arcade speeds & offsets).
; A = mode (0 = 16-dir aimed 2.26 px/F, 1 = horizontal toward ob_dir, 2 = straight down 2.0 px/F).
fire_shot:
    sta oz_t2
    lda oz_kind_cnt + ok_shot
    cmp #6
    bcc :+
    rts
:
    stx oz_t3
    jsr alloc
    bcc :+
    ldx oz_t3
    rts
:
    lda #ok_shot
    sta ob_kind,x
    inc oz_kind_cnt + ok_shot
    lda #ofl_harmful
    sta ob_flags,x
    ldy oz_t3
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    sta ob_yl,x
    lda ob_yh,y
    sta ob_yh,x
    ; Select authentic arcade Revision G projectile sprite, spawn offset, and speed:
    ;   Devil=$FCE7 (horiz 1.56 / down 2.0, Y-10), Plant=$FD7C (2.26, X-1 Y+5),
    ;   Arremer=$FDD7 (2.26, Y+16), Unicorn=$FDAB/$FDB3 (3.0, Y+8),
    ;   BigMan=$FCF2/$FCFA (2.0, X+-16) / $FD02 (2.0 down, Y-8).
    lda ob_kind,y
    cmp #ok_devil
    beq @an_devil
    cmp #ok_plant
    beq @an_plant
    cmp #ok_arremer
    beq @an_arremer
    cmp #ok_unicorn
    bne :+
    jmp @an_unicorn
:
    ; Big Man ($03): Down ($FD02 at Y-8), Right ($FCF2 at X+16), Left ($FCFA at X-16)
    lda oz_t2
    cmp #2
    beq @an_bm_dn
    lda ob_dir,y
    bne @an_bm_l
    lda ob_xl,x
    clc
    adc #16
    sta ob_xl,x
    bcc :+
    inc ob_xh,x
:
    lda #<rf_FCF2
    ldy #>rf_FCF2
    jmp @an_set
@an_bm_l:
    lda ob_xl,x
    sec
    sbc #16
    sta ob_xl,x
    bcs :+
    dec ob_xh,x
:
    lda #<rf_FCFA
    ldy #>rf_FCFA
    jmp @an_set
@an_bm_dn:
    lda ob_yl,x
    sec
    sbc #8
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda #<rf_FD02
    ldy #>rf_FD02
    jmp @an_set
@an_devil:
    lda ob_yl,x
    sec
    sbc #10
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda #<rf_FCE7
    ldy #>rf_FCE7
    jmp @an_set
@an_plant:
    lda ob_yl,x
    clc
    adc #5
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    lda #<rf_FD7C
    ldy #>rf_FD7C
    jmp @an_set
@an_arremer:
    lda ob_yl,x
    clc
    adc #16
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    lda #<rf_FDD7
    ldy #>rf_FDD7
    jmp @an_set
@an_unicorn:
    lda ob_yl,x
    clc
    adc #8
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    lda ob_dir,y
    bne @an_uni_l
    lda #<rf_FDAB
    ldy #>rf_FDAB
    jmp @an_set
@an_uni_l:
    lda #<rf_FDB3
    ldy #>rf_FDB3
@an_set:
    jsr set_anim
    lda oz_t2
    cmp #1
    beq @horiz
    cmp #2
    beq @down
    ; Mode 0: 16-direction aimed shot at 2.26 px/F (arcade bank4:$5F79)
    jsr dir_to_player16
    ldy #0
    jsr load_vdir
    jmp @done_se
@horiz:
    ldy oz_t3
    lda ob_dir,y
    sta ob_dir,x
    lda ob_kind,y
    cmp #ok_unicorn
    beq @h_uni
    cmp #ok_devil
    beq @h_dev
    lda #0                      ; Big Man horizontal: +-2.0 px/F
    ldy #2
    bne @h_set
@h_uni:
    lda #0                      ; Unicorn horizontal: +-3.0 px/F
    ldy #3
    bne @h_set
@h_dev:
    lda #$90                    ; Petit Devil horizontal: +-1.56 px/F
    ldy #1
@h_set:
    jsr set_vx_dir
    jmp @done_se
@down:
    lda #$fe                    ; -2.0 px/F straight down
    sta ob_vyi,x
@done_se:
    lda #snd_se_efire
    jsr call_bank8_play_se
    ldx oz_t3
    rts

; Spawn object of kind A, variant Y at (oz_t0:oz_t1, oz_t2:oz_t3).
spawn_kind:
    sta oz_mul
    sty oz_mul+1
    jsr alloc
    bcc :+
    rts
:
    lda oz_mul
    sta ob_kind,x
    tay
    lda oz_kind_cnt,y
    clc
    adc #1
    sta oz_kind_cnt,y
    lda ok_hp,y
    sta ob_hp,x
    lda oz_mul+1
    sta ob_var,x
    lda oz_t0
    sta ob_xl,x
    lda oz_t1
    sta ob_xh,x
    lda oz_t2
    sta ob_yl,x
    lda oz_t3
    sta ob_yh,x
    clc
    rts

; --- Spawning: fixed enemies, support triggers, and 2D page schedulers ----------------
place_fixed:
    ; Spawn nearby fixed enemies (17 total) and support triggers (16 total) on demand.
    ldy #revg2_fix_count - 1
@flp:
    jsr test_fix_bit
    bne @fnext
    lda revg2_fix_x_lo,y
    sec
    sbc camera_low
    sta oz_t0
    lda revg2_fix_x_hi,y
    sbc camera_high
    bne @fnext
    ; Within [cam_x, cam_x + 255]: spawn!
    jsr set_fix_bit
    sty oz_obj
    lda revg2_fix_x_lo,y
    sta oz_t0
    lda revg2_fix_x_hi,y
    sta oz_t1
    lda revg2_fix_y_lo,y
    sta oz_t2
    lda revg2_fix_y_hi,y
    sta oz_t3
    lda revg2_fix_kind,y
    ldx revg2_fix_var,y
    pha
    txa
    tay
    pla
    jsr spawn_kind
    ldy oz_obj
@fnext:
    dey
    bpl @flp

    ; Support triggers (discs & hidden armor)
    ldy #revg2_sup_count - 1
@slp:
    jsr test_sup_bit
    bne @snext
    lda revg2_sup_x_lo,y
    sec
    sbc camera_low
    lda revg2_sup_x_hi,y
    sbc camera_high
    bne @snext
    jsr set_sup_bit
    sty oz_obj
    lda revg2_sup_x_lo,y
    sta oz_t0
    lda revg2_sup_x_hi,y
    sta oz_t1
    lda revg2_sup_y_lo,y
    sta oz_t2
    lda revg2_sup_y_hi,y
    sta oz_t3
    lda revg2_sup_var,y
    cmp #2
    bcs @is_disc
    pha
    tay
    lda #ok_trigger
    jsr spawn_kind
    pla
    jmp @sres
@is_disc:
    tay
    lda #ok_disc
    jsr spawn_kind
@sres:
    ldy oz_obj
@snext:
    dey
    bpl @slp
    rts

bit_mask: .byte 1, 2, 4, 8, 16, 32, 64, 128

; Compute X = Y >> 3, oz_t0 = bit_mask[Y & 7] (preserves Y).
bit_prep:
    tya
    and #7
    tax
    lda bit_mask,x
    sta oz_t0
    tya
    lsr a
    lsr a
    lsr a
    tax
    rts

test_fix_bit:
    jsr bit_prep
    lda s2_fix_used,x
    and oz_t0
    rts

set_fix_bit:
    jsr bit_prep
    lda s2_fix_used,x
    ora oz_t0
    sta s2_fix_used,x
    rts

test_sup_bit:
    jsr bit_prep
    lda s2_sup_used,x
    and oz_t0
    rts

set_sup_bit:
    jsr bit_prep
    lda s2_sup_used,x
    ora oz_t0
    sta s2_sup_used,x
    rts

; Register current 2D page group (col 0..13, arcade row = 1 - pan_y_page).
check_page_group:
    lda camera_high
    cmp #revg2_page_cols
    bcc :+
    lda #revg2_page_cols - 1
:
    ldy pan_y_page
    bne :+                      ; pan_y_page = 1 -> bottom row (arcade row 0)
    clc
    adc #revg2_page_cols        ; pan_y_page = 0 -> top row (arcade row 1)
:
    cmp oz_reg_page
    bne :+
    rts
:
    sta oz_reg_page
    ; Clear non-boss (non-once) schedulers before loading the new page's groups.
    ldx #4
    lda #$ff
@clr_sch:
    ldy s2_sch_once,x
    bne @clr_sk
    sta s2_sch_pat,x
@clr_sk:
    dex
    bpl @clr_sch
    ; Offset = page_idx * 9
    lda oz_reg_page
    sta oz_t0
    asl a
    asl a
    asl a
    clc
    adc oz_t0
    tay
    ldx #0
@entry:
    lda revg2_page_groups,y
    beq @done
    sec
    sbc #1                      ; scheduler 0..4
    tax
    lda revg2_page_groups+1,y   ; pattern index
    sta oz_t1
    lda revg2_page_groups+2,y   ; flags
    and #2                      ; bit 1 = once (Unicorn boss)
    beq @load_pat
    lda s2_sch_once,x
    bne @next_e
    lda #1
    sta s2_sch_once,x
@load_pat:
    lda oz_t1
    cmp s2_sch_pat,x
    beq @next_e
    sta s2_sch_pat,x
    sty oz_t2
    tay
    lda revg2_pat_start,y
    sta s2_sch_rec,x
    clc
    adc revg2_pat_count,y
    sta s2_sch_end,x
    lda #20
    sta s2_sch_wait,x
    ldy oz_t2
@next_e:
    iny
    iny
    iny
    cpy #255
    bcc @entry
@done:
    rts

tick_schedulers:
    ldx #4
@lp:
    lda s2_sch_pat,x
    bpl :+
    jmp @next
:
    lda s2_sch_wait,x
    beq @try
    dec s2_sch_wait,x
    jmp @next
@try:
    ldy revg2_sched_kind,x
    lda oz_kind_cnt,y
    cmp sch_limit,x
    bcs @next
    ; Spawn record s2_sch_rec[x]
    stx oz_obj
    ldy s2_sch_rec,x
    lda revg2_rec_x_lo,y
    sta oz_t0
    lda revg2_rec_x_hi,y
    sta oz_t1
    ; If relative (small X < 256 for crow/zombie), add camera_x
    lda revg2_sched_kind,x
    cmp #ok_crow
    beq @rel_x
    cmp #ok_zombie
    bne @abs_x
@rel_x:
    lda oz_t0
    clc
    adc camera_low
    sta oz_t0
    lda oz_t1
    adc camera_high
    sta oz_t1
@abs_x:
    lda revg2_rec_y_lo,y
    sta oz_t2
    lda revg2_rec_y_hi,y
    sta oz_t3
    lda revg2_rec_wait,y
    sta s2_sch_wait,x
    lda revg2_rec_var,y
    pha
    lda revg2_sched_kind,x
    tay
    pla
    stx oz_obj
    tax
    tya                         ; A = kind, X = var
    stx oz_mul+1
    ldy oz_mul+1
    jsr spawn_kind
    ldx oz_obj
    inc s2_sch_rec,x
    lda s2_sch_rec,x
    cmp s2_sch_end,x
    bcc @next
    ; Loop or stop if one-shot (Unicorn boss)
    lda s2_sch_once,x
    beq @loop_pat
    lda #$ff
    sta s2_sch_pat,x
    jmp @next
@loop_pat:
    ldy s2_sch_pat,x
    lda revg2_pat_start,y
    sta s2_sch_rec,x
    lda #90
    sta s2_sch_wait,x
@next:
    dex
    bmi :+
    jmp @lp
:
    rts

; --- Main frame update ----------------------------------------------------------------
update:
    jsr rng
    jsr sync_platforms
    jsr player_y
    jsr place_fixed
    jsr check_page_group
    jsr tick_schedulers

    ; Check 2x Unicorn boss defeated -> wait for boss explosion/popup to finish, then spawn key.
    lda s2_boss_rem
    bne @no_key
    lda oz_boss_seen
    bne @no_key
    lda oz_kind_cnt + ok_boom
    ora oz_kind_cnt + ok_popup
    bne @no_key
    lda player_x+1
    cmp #$0c
    bcc @no_key
    bne @spawn_key
    lda player_x
    cmp #$d0
    bcc @no_key
@spawn_key:
    ; Clear all remaining objects so the boss key always gets a slot (arcade clears room).
    ldx #ob_max - 1
:
    jsr free
    dex
    bpl :-
    lda #1
    sta oz_boss_seen
    lda #<3365
    sta oz_t0
    lda #>3365
    sta oz_t1
    lda #192
    sta oz_t2
    lda #0
    sta oz_t3
    lda #ok_key
    ldy #0
    jsr spawn_kind
    lda #snd_song_key
    jsr call_bank8_play_song
@no_key:

    ldx #0
@loop:
    stx oz_obj
    lda ob_kind,x
    beq @next
    lda ob_flash,x
    beq :+
    dec ob_flash,x
:
    ldy ob_kind,x
    lda ok_upd_lo,y
    sta oz_mul
    lda ok_upd_hi,y
    sta oz_mul+1
    jsr @dispatch
    ldx oz_obj
    lda ob_kind,x
    beq @next
    lda ob_flags,x
    and #ofl_harmful
    beq @next
    jsr check_player_hit
    ldx oz_obj
@next:
    inx
    cpx #ob_max
    bne @loop
    rts
@dispatch:
    jmp (oz_mul)

; --- Player contact check -------------------------------------------------------------
check_player_hit:
    lda ap_invuln
    ora opt_invincible
    bne @no
    lda ap_state
    cmp #ap_state_knockback
    bcs @no
    jsr rel_player
    ldy ob_kind,x
    ; |oz_dx| <= ok_px[y]
    lda oz_dx
    clc
    adc ok_px,y
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @no
    lda ok_px,y
    asl a
    cmp oz_t0
    bcc @no
    ; ok_pylo[y] <= oz_dy <= ok_pyhi[y] (crouching lowers top edge by 6 px, arcade $F0CA+$98)
    lda oz_dy
    sec
    sbc ok_pylo,y
    sta oz_t0
    lda oz_dy+1
    sbc #$ff
    bne @no
    lda ok_pyhi,y
    ldy ap_crouch
    beq :+
    sec
    sbc #6
:
    ldy ob_kind,x
    sec
    sbc ok_pylo,y
    cmp oz_t0
    bcc @no
    ; Hit! Knock Arthur away from object X.
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    stx oz_t3
    jsr ap_apply_player_damage
    ldx oz_t3
    lda ob_kind,x
    cmp #ok_shot
    bne @no
    jsr free
@no:
    rts

; --- Player shot collision check (called from BANK3 ap_update_shots) ------------------
shot_check:
    ldy ap_loop_idx
    ; Convert shot FC Y to arcade Y = (stage_cam_y_max_page * 256 + 254) - ps_y
    lda #254
    sec
    sbc ps_y,y
    sta oz_t2
    lda stage_cam_y_max_page
    sbc ps_y_hi,y
    sta oz_t3
    ldx #0
@slp:
    lda ob_kind,x
    beq @snext
    lda ob_flags,x
    and #ofl_shootable
    beq @snext
    ; |ob_x - ps_x| <= ok_sx[kind]
    lda ob_xl,x
    sec
    sbc ps_x_lo,y
    sta oz_t0
    lda ob_xh,x
    sbc ps_x_hi,y
    sta oz_t1
    stx oz_obj
    lda ob_kind,x
    tax
    lda oz_t0
    clc
    adc ok_sx,x
    sta oz_t0
    lda oz_t1
    adc #0
    bne @smiss
    lda ok_sx,x
    asl a
    cmp oz_t0
    bcc @smiss
    ; ob_y - shot_y in [ok_sylo, ok_syhi]
    ldx oz_obj
    lda ob_yl,x
    sec
    sbc oz_t2
    sta oz_t0
    lda ob_yh,x
    sbc oz_t3
    sta oz_t1
    lda ob_kind,x
    tax
    lda oz_t0
    sec
    sbc ok_sylo,x
    sta oz_t0
    lda oz_t1
    sbc #$ff
    bne @smiss
    lda ok_syhi,x
    sec
    sbc ok_sylo,x
    cmp oz_t0
    bcc @smiss
    ; Hit!
    ldx oz_obj
    jsr hit_enemy
    sec
    rts
@smiss:
    ldx oz_obj
@snext:
    inx
    cpx #ob_max
    bne @slp
    clc
    rts

hit_enemy:
    lda ob_hp,x
    beq @kill
    dec ob_hp,x
    lda #6
    sta ob_flash,x
    ; Big Man enters 6F hit-stun ($0103) with rf_51B0; Red Arremer enters 5F hit-stun ($4DFF).
    lda ob_kind,x
    cmp #ok_bigman
    bne @chk_arr_hit
    lda #4
    sta ob_state,x
    lda #6
    sta ob_t0,x
    lda #<rf_51B0
    ldy #>rf_51B0
    jsr set_anim
    jmp @hit_se
@chk_arr_hit:
    cmp #ok_arremer
    bne @chk_uni_wake
    lda ob_state,x
    bne @arr_stun
    ; Hit while sleeping: wake immediately with HP reset to 3 (arcade $0003)!
    lda #2
    sta ob_hp,x
    lda #1
    sta ob_state,x
    lda #16
    sta ob_t0,x
    jmp @hit_se
@arr_stun:
    lda #5
    sta ob_flash,x
    jmp @hit_se
@chk_uni_wake:
    cmp #ok_unicorn
    bne @hit_se
    ; Unicorn sleeping/ground wake: 4 hits (HP <= 5) wakes immediately (arcade $C470)
    lda ob_state,x
    bne @hit_se
    lda ob_hp,x
    cmp #6
    bcs @hit_se
    lda #1
    sta ob_state,x
    lda #30
    sta ob_t0,x
@hit_se:
    txa
    pha
    lda #snd_se_hit
    jsr call_bank8_play_se
    pla
    tax
    rts
@kill:
    ldy ob_kind,x
    sty oz_t2                   ; remember original kind for explosion & popup
    lda ok_points,y
    beq @no_pts
    txa
    pha
    jsr ap_add_score
    pla
    tax
@no_pts:
    ; Check if this was one of the 2 Unicorn bosses
    lda oz_t2
    cmp #ok_unicorn
    bne @not_boss
    lda s2_boss_rem
    beq @not_boss
    dec s2_boss_rem
    bne @not_boss               ; 1st Unicorn dead: keep boss BGM playing!
    ; 2nd Unicorn dead: stop BGM immediately (arcade $E796)!
    txa
    pha
    lda #snd_song_none
    jsr call_bank8_play_song
    pla
    tax
@not_boss:
    ; Decrement kind counter and turn into authentic explosion (+ popup for Big Man/Arremer/Unicorn)
    ldy oz_t2
    lda oz_kind_cnt,y
    beq :+
    sec
    sbc #1
    sta oz_kind_cnt,y
:
    lda #ok_boom
    sta ob_kind,x
    inc oz_kind_cnt + ok_boom
    lda #0
    sta ob_flags,x
    sta ob_state,x
    sta ob_var,x                ; popup kind after boom: 0=none, 1=500, 2=1000, 3=2000
    lda oz_t2
    cmp #ok_unicorn
    beq @kill_uni
    cmp #ok_bigman
    beq @kill_big
    cmp #ok_arremer
    beq @kill_arr
    cmp #ok_plant
    beq @kill_big36
    ; Small 18F explosion (rf_5C54)
    lda #18
    sta ob_t0,x
    lda #<rf_5C54
    ldy #>rf_5C54
    jmp @kill_set
@kill_uni:
    lda #3                      ; 2000 popup (63F)
    sta ob_var,x
    lda #21
    sta ob_t0,x
    lda #<rf_5C7A
    ldy #>rf_5C7A
    jmp @kill_set
@kill_big:
    lda #1                      ; 500 popup (112F)
    sta ob_var,x
    bne @kill_big36
@kill_arr:
    lda #2                      ; 1000 popup (63F)
    sta ob_var,x
@kill_big36:
    lda #36
    sta ob_t0,x
    lda #<rf_5CA0
    ldy #>rf_5CA0
@kill_set:
    jsr set_anim
    txa
    pha
    lda #snd_se_kill
    jsr call_bank8_play_se
    pla
    tax
    rts

; --- Enemy AI 1: Petit Devil ($02, var 0) ---------------------------------------------
; Arcade $A7D7-$AD10:
;   - Picks a visible tower window (2D X & Y) from 26-window table ($ADF3).
;   - State 1: 31F invuln emerge at 0.06 px/F ($00.$10).
;   - State 2: 3 alternating fast (1.65 px/F, 80..111F) / slow (0.38 px/F, 48..95F)
;     16-direction curving cycles ($ABCD), Y >= 54 floor clamp (no velocity bounce),
;     fires 1 shot (rf_FCE7) per swoop when aligned horizontally (|dy|<=16, |dx|>=50 -> 1.56 px/F)
;     or directly above (|dx|<=5, dy>=60 -> 2.0 px/F straight down).
;   - State 3 ($0103): Fly off horizontally at 1.65 px/F until offscreen.
; Fields: ob_aux = cycles left (3..1), ob_aux2 = shot fired this cycle (0/1),
;         ob_step = current 16-dir (0..15), ob_by_l = phase (0=fast, 1=slow).
u_devil:
    lda ob_state,x
    beq :+
    jmp @dv_active
:
    ; State 0: pick a visible window (both X and Y on screen!)
    jsr calc_cam_y_arc          ; oz_t2:oz_t3 = cam_y_arc
    lda s2_win_flip
    eor #1
    sta s2_win_flip
    bne @scan_up
    ldy #0
    jmp @scan
@scan_up:
    ldy #win_count - 1
@scan:
    cpy s2_win_last
    beq @snext
    lda win_xl,y
    sec
    sbc camera_low
    lda win_xh,y
    sbc camera_high
    bne @snext
    ; Check window Y within [cam_y_arc + 16, cam_y_arc + 228]
    lda win_yl,y
    sec
    sbc oz_t2
    sta oz_t0
    lda win_yh,y
    sbc oz_t3
    bne @snext
    lda oz_t0
    cmp #16
    bcc @snext
    cmp #228
    bcs @snext
    ; Found visible window!
    sty s2_win_last
    lda win_xl,y
    sta ob_xl,x
    lda win_xh,y
    sta ob_xh,x
    lda win_yl,y
    sta ob_yl,x
    lda win_yh,y
    sta ob_yh,x
    lda #<rf_A893
    ldy #>rf_A893
    jsr set_anim
    jsr face_player
    lda #1
    sta ob_state,x
    lda #31                     ; 31F emerge invulnerable
    sta ob_t0,x
    lda #3                      ; 3 fast/slow swoop cycles
    sta ob_aux,x
    lda #0
    sta ob_aux2,x               ; hasn't fired shot yet
    sta ob_by_l,x               ; phase 0 = fast glide
    rts
@snext:
    lda s2_win_flip
    bne @sdec
    iny
    cpy #win_count
    bcc @scan
    jmp free
@sdec:
    dey
    bpl @scan
    jmp free

@dv_active:
    cmp #1
    bne @dv_swoop
    ; State 1: 31F invulnerable emerge at 0.06 px/F ($00.$10)
    dec ob_t0,x
    bne @emerge_move
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #2
    sta ob_state,x
    jsr rng
    and #31
    clc
    adc #49                     ; 49..80F initial fast emerge glide
    sta ob_t0,x
    lda #0
    sta ob_by_l,x
    jsr dir_to_player16
    sta ob_step,x
    jmp @dv_load_vel
@emerge_move:
    lda #$10
    ldy #0
    jsr set_vx_dir
    jmp move

@dv_swoop:
    cmp #2
    beq :+
    jmp @dv_leave
:
    jsr move
    ; Arcade $A7FF: Floor clamp Y >= 54 (no velocity bounce!)
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
    ; Arcade $ABCD: steer 16-direction heading 1 step toward Arthur every 8F
    lda frames
    and #7
    bne @dv_no_steer
    jsr dir_to_player16
    sec
    sbc ob_step,x
    and #15
    beq @dv_no_steer
    cmp #8
    bcc @dv_cw
    lda ob_step,x
    sec
    sbc #1
    and #15
    sta ob_step,x
    jmp @dv_upd_vel
@dv_cw:
    lda ob_step,x
    clc
    adc #1
    and #15
    sta ob_step,x
@dv_upd_vel:
    jsr @dv_load_vel
@dv_no_steer:
    ; Arcade $AC88-$AD10: Fire 1 shot (type $08: rf_FCE7) per swoop when aligned:
    ;   horizontal: |dy| <= 16 & |dx| >= 50 -> 1.56 px/F
    ;   straight down: dy >= 60 & |dx| <= 5 -> 2.0 px/F
    lda ob_aux2,x
    bne @dv_no_fire
    jsr rel_player
    lda #16
    jsr abs_dy_le
    bcc @dv_chk_dn
    lda #49
    jsr abs_dx_le
    bcs @dv_chk_dn
    jsr face_player
    lda #1                      ; horizontal shot (rf_FCE7, 1.56 px/F)
    jsr fire_shot
    inc ob_aux2,x
    jmp @dv_no_fire
@dv_chk_dn:
    lda oz_dy+1
    bmi @dv_no_fire
    bne @dv_chk_dx5
    lda oz_dy
    cmp #60
    bcc @dv_no_fire
@dv_chk_dx5:
    lda #5
    jsr abs_dx_le
    bcc @dv_no_fire
    lda #2                      ; straight down shot (rf_FCE7, 2.0 px/F)
    jsr fire_shot
    inc ob_aux2,x
@dv_no_fire:
    dec ob_t0,x
    bne @dv_chk_off
    ; Alternate fast (phase 0) <-> slow (phase 1)
    lda ob_by_l,x
    bne @dv_slow_done
    ; Fast finished -> enter slow curving (48..95F at 0.38 px/F)
    lda #1
    sta ob_by_l,x
    jsr rng
    and #31
    clc
    adc #48
    sta ob_t0,x
    jsr @dv_load_vel
    jmp @dv_chk_off
@dv_slow_done:
    dec ob_aux,x
    beq @dv_start_leave
    ; Start next fast swoop (80..111F at 1.65 px/F) and allow another throw
    lda #0
    sta ob_by_l,x
    sta ob_aux2,x
    jsr rng
    and #31
    clc
    adc #80
    sta ob_t0,x
    jsr dir_to_player16
    sta ob_step,x
    jsr @dv_load_vel
    jmp @dv_chk_off
@dv_start_leave:
    lda #3
    sta ob_state,x
    lda #$a6
    ldy #1
    jsr set_vx_dir
@dv_leave:
    jsr move
@dv_chk_off:
    jsr offscreen
    bcc :+
    jmp free
:
    rts

; Load velocity from 16-dir table (16=v165 if ob_by_l==0, 32=v038 if ob_by_l==1) + update facing
@dv_load_vel:
    ldy #16
    lda ob_by_l,x
    beq :+
    ldy #32
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

; --- Enemy AI 2: Purple Flying Enemy ($04, var 1) -------------------------------------
; Arcade $B0BF-$B510:
;   - 3,600F (14*256+16) active timer -> transforms into Red Arremer ($05).
;   - State 1: 31F invuln emerge from building window.
;   - State 2: 6-stage accelerating descent (0.19..1.97 px/F X, -1.71 px/F Y) until floor.
;   - State 3: Walk on floor at 1.43 px/F for {120,160,60,16}F; dodges nearby player shots ($B4AA).
;   - State 4: 32F sine-arc leap (+32 or +48 px) at 1.43 px/F -> State 5.
;   - State 5: Level flight at 1.43 px/F for {60,90,120,180}F; dives on nearby shot -> State 6.
;   - State 6: Sine descent (16F gentle -1.0 px/F -> -2.0 px/F fall) until floor -> State 3.
u_flyer:
    lda ob_state,x
    bne @fl_not0
    lda #<rf_B19E
    ldy #>rf_B19E
    jsr set_anim
    jsr face_player
    lda #1
    sta ob_state,x
    lda #31                     ; 31F emerge invuln
    sta ob_t0,x
    lda #14                     ; 3,600F transform countdown (14*256 + 16)
    sta ob_aux,x
    lda #16
    sta ob_aux2,x
    rts
@fl_not0:
    ; Transform countdown while active ($B0BF)
    dec ob_aux2,x
    bne @fl_no_xform
    dec ob_aux,x
    bne @fl_no_xform
    ; Transform into Red Arremer!
    dec oz_kind_cnt + ok_flyer
    lda #ok_arremer
    sta ob_kind,x
    inc oz_kind_cnt + ok_arremer
    lda #2
    sta ob_hp,x
    lda #0
    sta ob_state,x
    rts
@fl_no_xform:
    lda ob_state,x
    cmp #1
    bne @fl_not1
    ; State 1: 31F invuln emerge
    dec ob_t0,x
    bne @fl_ret
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #2
    sta ob_state,x
    jsr face_player
    lda #0
    sta ob_step,x               ; descent stage 0..5
    lda #6
    sta ob_t0,x
    lda #$49
    sta ob_vyf,x
    lda #$fe                    ; -1.71 px/F descent
    sta ob_vyi,x
    jsr @fl_set_desc_vx
@fl_ret:
    rts

@fl_not1:
    cmp #2
    bne @fl_not2
    ; State 2: 6-stage accelerating descent until ground
    dec ob_t0,x
    bne @fl_desc_mv
    lda #6
    sta ob_t0,x
    lda ob_step,x
    cmp #5
    bcs @fl_desc_mv
    inc ob_step,x
    jsr @fl_set_desc_vx
@fl_desc_mv:
    jsr move
    jsr on_ground
    bcs :+
    jmp @fl_chk_off
:
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
    lda @fl_walk_tbl,y          ; {120, 160, 60, 16}
    sta ob_t0,x
    rts

@fl_not2:
    cmp #3
    bne @fl_not3
    ; State 3: Walk on floor at 1.43 px/F + shot-dodge ($B4AA)
    lda #48
    jsr shot_near_x
    bcs @fl_enter_leap
    jsr @fl_set_143_vx
    jsr move
    jsr on_ground
    bcc @fl_enter_leap
    dec ob_t0,x
    beq :+
    jmp @fl_chk_off
:
@fl_enter_leap:
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
    sta ob_step,x               ; 0..31 sine step
    jsr rng
    and #1                      ; 0 = 32 px leap, 1 = 48 px leap
    sta ob_t0,x
    rts

@fl_not3:
    cmp #4
    bne @fl_not4
    ; State 4: 32F sine-arc leap (+32 or +48 px) at 1.43 px/F
    jsr @fl_set_143_vx
    jsr move
    lda ob_step,x
    lsr a                       ; 0..15 rising quarter-wave
    tay
    lda s2_half_sine,y          ; 0..126
    lsr a
    lsr a                       ; 0..31 (32 px)
    sta oz_t0
    lda ob_t0,x
    beq :+
    lda oz_t0
    lsr a
    clc
    adc oz_t0                   ; 0..46 (48 px)
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
    bcc @fl_chk_off
    ; Enter State 5: Level flight for {60, 90, 120, 180}F
    lda #5
    sta ob_state,x
    jsr face_player
    jsr rng
    and #3
    tay
    lda @fl_fly_tbl,y
    sta ob_t0,x
    rts

@fl_not4:
    cmp #5
    bne @fl_desc2
    ; State 5: Level flight at 1.43 px/F + shot-dodge dive
    lda #48
    jsr shot_near_x
    bcs @fl_enter_desc2
    jsr @fl_set_143_vx
    jsr move
    dec ob_t0,x
    bne @fl_chk_off
@fl_enter_desc2:
    lda #6
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #$ff                    ; gentle -1.0 px/F start
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    rts

@fl_desc2:
    ; State 6: Sine descent (16F at -1.0 px/F -> -2.0 px/F fall) until floor
    lda ob_step,x
    cmp #16
    bcs @fl_d2_fast
    inc ob_step,x
    bne @fl_d2_mv
@fl_d2_fast:
    lda #$fe                    ; -2.0 px/F
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
@fl_d2_mv:
    jsr move
    jsr on_ground
    bcc @fl_chk_off
    jsr snap_ground
    jmp @fl_enter_walk

@fl_chk_off:
    jsr offscreen
    bcc :+
    jmp free
:
    rts

@fl_set_143_vx:
    lda #$6e
    ldy #1
    jmp set_vx_dir

@fl_set_desc_vx:
    ldy ob_step,x
    lda @fl_desc_xf,y
    pha
    lda @fl_desc_xi,y
    tay
    pla
    jsr set_vx_dir
    lda #$49
    sta ob_vyf,x
    lda #$fe                    ; preserve -1.71 px/F descent
    sta ob_vyi,x
    rts

@fl_desc_xi: .byte $00, $00, $00, $01, $01, $01
@fl_desc_xf: .byte $30, $80, $E0, $40, $A0, $F8
@fl_walk_tbl: .byte 120, 160, 60, 16
@fl_fly_tbl:  .byte 60, 90, 120, 180

; --- Enemy AI 3: Big Man ($0A, var 0, HP 10) ------------------------------------------
; Arcade $CE3A-$D20F (100% Revision G faithful):
;   - NEVER despawns offscreen; pauses all AI when offscreen (on_screen_xy == 0).
;   - State 1 ($0100.1): Walk at 0.53 px/F using 9-entry table ($51C4: 190,100,163,230,98,156,120,160,120).
;     Every frame checks Y-band (player_y - ob_y in -10..+45 -> State 2 Charge) & directly-below
;     (|dx| <= 10 & ob_y >= player_y -> State 3 Downward Shoot). Every 16F checks ledge edge.
;   - State 5 ($0100.2): Idle pause 100F (rf_5148) or 70F (rf_511C), then random dir -> State 1.
;   - State 2 ($0101): Charge & Horiz Shoot using 9-entry table ($51FC: 45,35,20,20,42,60,35,50,28 x2F).
;     Alternates .1 (check Y-band; only turn if player >=40px behind) & .2 (step 2.0 px/F -> 1.0 avg;
;     at ledge stop if player past edge else turn; dec charge timer) -> .3 (10F stop + rf_5170) ->
;     .4 (8F wait + fire horiz rf_FCF2/FA) -> loop back to .1!
;   - State 3 ($0102): Downward Shoot (rf_5184, 30F firing at frame 11 + 20F recovery = 50F cycle).
;   - State 4 ($0103): 6F hit-stun (rf_51B0), returns to State 1.
; Fields: ob_aux lo nibble = walk table idx (0..8), hi nibble = charge table idx (0..8);
;         ob_aux2 = remaining charge steps (+$06); ob_by_l = edge flag/dir (+$14).
u_bigman:
    lda ob_state,x
    bne @bm_active
    ; State 0 ($0001): wait until visible on screen (X & Y)
    jsr on_screen_xy
    bcs :+
    rts
:
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
    jsr face_player
    lda #0
    sta ob_aux,x                ; walk idx=0, charge idx=0
    sta ob_step,x
    lda #$ff
    sta ob_by_l,x               ; no edge remembered
    lda @bm_walk_tbl            ; 190F
    sta ob_t0,x
    lda @bm_rush_tbl            ; 45 (x2 = 90F)
    sta ob_aux2,x
    rts

@bm_active:
    ; Arcade $CE9D: pause everything while offscreen (-$0F == 0); NEVER despawn!
    jsr on_screen_xy
    bcs :+
    rts
:
    lda ob_state,x
    cmp #4
    bne @bm_not_stun
    ; State 4 ($0103): 6F hit-stun -> back to $0100.0 (walk)
    dec ob_t0,x
    bne @bm_ret0
    jmp @bm_enter_walk
@bm_ret0:
    rts

@bm_not_stun:
    cmp #5
    bne @bm_not_idle
    ; State 5 ($0100.2): 100F (rf_5148) or 70F (rf_511C) idle pause -> random dir -> State 1
    dec ob_t0,x
    bne @bm_ret0
    jsr rng
    and #1
    sta ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    jmp @bm_enter_walk

@bm_not_idle:
    cmp #1
    beq @bm_patrol
    cmp #2
    bne :+
    jmp @bm_charge
:
    jmp @bm_down

@bm_patrol:
    ; State 1 ($0100.1): check Y-band (-10..+45) then directly below (|dx|<=10, ob_y>=py)
    jsr @bm_in_yband
    bcc @bm_chk_below
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
@bm_chk_below:
    jsr @bm_is_below
    bcc @bm_walk_step
    lda #3
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
@bm_walk_step:
    ; Every 16F check floor edge ($CFC8): remember edge dir in ob_by_l, reverse if heading into edge
    lda frames
    and #15
    bne @bm_do_walk
    jsr on_ground
    bcs @bm_on_floor
    lda ob_by_l,x
    bpl :+
    lda ob_dir,x
    sta ob_by_l,x
:
    lda ob_dir,x
    cmp ob_by_l,x
    bne @bm_do_walk
    eor #1
    sta ob_dir,x
    lda ob_mirror,x
    eor #$40
    sta ob_mirror,x
    jmp @bm_do_walk
@bm_on_floor:
    lda #$ff
    sta ob_by_l,x
@bm_do_walk:
    lda #$88                    ; 0.53 px/F ($00.$88)
    ldy #0
    jsr set_vx_dir
    jsr move
    ; Count walk timer ($51C4); on 0 advance index & enter State 5 idle pause (70F or 100F)
    dec ob_t0,x
    bne @bm_ret1
    lda ob_aux,x
    and #$0f
    clc
    adc #1
    cmp #9
    bcc :+
    lda #0
:
    sta oz_t0
    lda ob_aux,x
    and #$f0
    ora oz_t0
    sta ob_aux,x
    lda #5
    sta ob_state,x
    jsr rng
    and #1
    bne @bm_idle70
    lda #100                    ; 100F rf_5148
    sta ob_t0,x
    lda #<rf_5148
    ldy #>rf_5148
    jmp set_anim
@bm_idle70:
    lda #70                     ; 70F rf_511C
    sta ob_t0,x
    lda #<rf_511C
    ldy #>rf_511C
    jmp set_anim
@bm_ret1:
    rts

@bm_enter_walk:
    lda #1
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda ob_aux,x
    and #$0f
    tay
    lda @bm_walk_tbl,y
    sta ob_t0,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim

@bm_charge:
    ; State 2 ($0101): .0=init, .1=check Y-band & 40px behind turn, .2=move 2.0 px/F, .3=10F stop, .4=8F shoot
    lda ob_step,x
    bne @bmc_not0
    jsr face_player
    lda #1
    sta ob_step,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim
@bmc_not0:
    cmp #1
    bne @bmc_not1
    jsr @bm_in_yband
    bcs :+
    jmp @bm_enter_walk
:
    lda ob_dir,x
    bne @bmc_chk_bl
    lda oz_dx
    clc
    adc #40
    lda oz_dx+1
    adc #0
    bpl @bmc_to2
    lda #1
    sta ob_dir,x
    lda #$40
    sta ob_mirror,x
    bne @bmc_to2
@bmc_chk_bl:
    lda oz_dx+1
    bmi @bmc_to2
    bne @bmc_turn_r
    lda oz_dx
    cmp #40
    bcc @bmc_to2
@bmc_turn_r:
    lda #0
    sta ob_dir,x
    sta ob_mirror,x
@bmc_to2:
    lda #2
    sta ob_step,x
    rts

@bmc_not1:
    cmp #2
    beq :+
    jmp @bmc_not2
:
    ; .2 ($D088): step 2.0 px/F (avg 1.0 across .1/.2), check ledge, dec rush timer (+$06)
    lda #0
    ldy #2
    jsr set_vx_dir
    jsr move
    jsr on_ground
    bcs @bmc_step_ok
    ; At ledge edge: undo step; if player is behind us reverse, else hold at edge!
    lda ob_vxi,x
    eor #$ff
    clc
    adc #1
    sta ob_vxi,x
    jsr move
    jsr rel_player
    lda ob_dir,x
    bne @bmc_edge_l
    lda oz_dx+1
    bpl @bmc_step_ok
    lda #1
    sta ob_dir,x
    lda #$40
    sta ob_mirror,x
    bne @bmc_step_ok
@bmc_edge_l:
    lda oz_dx+1
    bmi @bmc_step_ok
    lda #0
    sta ob_dir,x
    sta ob_mirror,x
@bmc_step_ok:
    lda ob_cnt,x
    cmp #2
    bcc :+
    sec
    sbc #2
    sta ob_cnt,x
:
    dec ob_aux2,x
    beq @bmc_to3
    lda #1
    sta ob_step,x
    rts
@bmc_to3:
    lda ob_aux,x
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    adc #1
    cmp #9
    bcc :+
    lda #0
:
    tay
    lda @bm_rush_tbl,y
    sta ob_aux2,x
    tya
    asl a
    asl a
    asl a
    asl a
    sta oz_t0
    lda ob_aux,x
    and #$0f
    ora oz_t0
    sta ob_aux,x
    lda #3
    sta ob_step,x
    lda #10                     ; .3: 10F stop before shoot pose
    sta ob_t0,x
    rts

@bmc_not2:
    cmp #3
    bne @bmc_s4
    dec ob_t0,x
    bne @bmc_ret
    lda #4
    sta ob_step,x
    lda #8
    sta ob_t0,x
    lda #<rf_5170
    ldy #>rf_5170
    jmp set_anim
@bmc_s4:
    dec ob_t0,x
    bne @bmc_ret
    lda #1                      ; horizontal shot (rf_FCF2 / rf_FCFA, 2.0 px/F)
    jsr fire_shot
    lda #1
    sta ob_step,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim
@bmc_ret:
    rts

@bm_down:
    ; State 3 ($0102): .0=face+rf_5184, .1=recheck Y-band/below, .2=30F (fire at 20), .3=20F wait
    lda ob_step,x
    bne @bmd_not0
    jsr face_player
    lda #1
    sta ob_step,x
    lda #<rf_5184
    ldy #>rf_5184
    jmp set_anim
@bmd_not0:
    cmp #1
    bne @bmd_not1
    jsr @bm_in_yband
    bcc :+
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
:
    jsr @bm_is_below
    bcs :+
    jmp @bm_enter_walk
:
    lda #2
    sta ob_step,x
    lda #30                     ; .2: 30F (fire on frame 11 when t0==20)
    sta ob_t0,x
    rts
@bmd_not1:
    cmp #2
    bne @bmd_s3
    dec ob_t0,x
    lda ob_t0,x
    cmp #20
    bne :+
    lda #2                      ; straight down shot (rf_FD02, 2.0 px/F, Y-8)
    jsr fire_shot
    rts
:
    cmp #0
    bne @bmd_ret
    lda #3
    sta ob_step,x
    lda #20                     ; .3: 20F recovery -> total 50-51F cycle
    sta ob_t0,x
    rts
@bmd_s3:
    dec ob_t0,x
    bne @bmd_ret
    lda #1
    sta ob_step,x
@bmd_ret:
    rts

@bm_in_yband:
    jsr rel_player
    lda oz_dy
    clc
    adc #45
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @bmy_no
    lda oz_t0
    cmp #56
    bcc @bmy_yes
@bmy_no:
    clc
    rts
@bmy_yes:
    sec
    rts

@bm_is_below:
    jsr rel_player
    lda oz_dy+1
    bmi @bmb_no
    lda #10
    jmp abs_dx_le
@bmb_no:
    clc
    rts

@bm_walk_tbl: .byte 190, 100, 163, 230, 98, 156, 120, 160, 120
@bm_rush_tbl: .byte 45, 35, 20, 20, 42, 60, 35, 50, 28

; --- Shared Stage 2 Enemies: Red Bird ($01 var 3/4), Plant ($11), Zombie ($0B) --------
u_crow:
    lda ob_state,x
    bne @fly
    lda #<rf_4A04
    ldy #>rf_4A04
    jsr set_anim
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr on_screen_xy
    bcc @ret
    lda #1
    sta ob_state,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #0
    sta ob_t0,x
    jsr face_player
    lda #<rf_4A2E
    ldy #>rf_4A2E
    jsr set_anim
    lda ob_var,x
    cmp #3
    bne @cr_var4
    ; Variant 3 (window Red Bird): straight toward Arthur at 1.70 px/F (16-dir)
    jsr dir_to_player16
    ldy #16
    jmp load_vdir
@ret:
    rts
@cr_var4:
    ; Variant 4 (bobbing crow): horizontal 1.25 px/F + sinusoidal vertical wave (+-16 px)
    lda #$40
    ldy #1
    jmp set_vx_dir
@fly:
    lda ob_var,x
    cmp #3
    beq @cr_mv
    inc ob_t0,x
    lda ob_t0,x
    and #$1f
    tay
    lda s2_half_sine,y
    lsr a
    lsr a
    lsr a
    sta oz_t0
    lda ob_t0,x
    and #$20
    bne @cr_down
    lda ob_by_l,x
    clc
    adc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    jmp @cr_mv
@cr_down:
    lda ob_by_l,x
    sec
    sbc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    sbc #0
    sta ob_yh,x
@cr_mv:
    jsr move
    jsr offscreen
    bcc :+
    jmp free
:
    rts

; Plant ($11): NEVER despawns offscreen; 2D on_screen_xy; 21-entry Row-0 wait table;
; 24F mouth-open telegraph (rf_5709/rf_56F7) -> 16-dir 2.26 px/F rf_FD7C from (X-1, Y+5).
u_plant:
    lda ob_state,x
    bne @pl_act
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #0
    sta ob_aux,x
    lda @pl_waits
    sta ob_t0,x
    jsr @pl_face_idle
    rts
@pl_act:
    jsr on_screen_xy
    bcc @pl_ret
    lda ob_state,x
    cmp #2
    beq @pl_open
    jsr @pl_face_idle
    dec ob_t0,x
    bne @pl_ret
    lda #2
    sta ob_state,x
    lda #24
    sta ob_t0,x
    lda ob_dir,x
    beq :+
    lda #<rf_56F7
    ldy #>rf_56F7
    jmp set_anim
:
    lda #<rf_5709
    ldy #>rf_5709
    jmp set_anim
@pl_open:
    dec ob_t0,x
    bne @pl_ret
    lda #0
    jsr fire_shot
    lda #1
    sta ob_state,x
    inc ob_aux,x
    lda ob_aux,x
    cmp #21
    bcc :+
    lda #0
    sta ob_aux,x
:
    tay
    lda @pl_waits,y
    sta ob_t0,x
    jsr @pl_face_idle
@pl_ret:
    rts
@pl_face_idle:
    jsr rel_player
    lda #0
    sta ob_mirror,x
    ldy oz_dx+1
    bpl :+
    lda #1
:
    cmp ob_dir,x
    bne @pl_set_dir
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne @pl_f_ret
@pl_set_dir:
    sta ob_dir,x
    beq :+
    lda #<rf_56D7
    ldy #>rf_56D7
    jmp set_anim
:
    lda #<rf_56E5
    ldy #>rf_56E5
    jmp set_anim
@pl_f_ret:
    rts
@pl_waits:
    .byte 65, 70, 8, 80, 8, 55, 60, 85, 80, 90, 33, 73, 75, 90, 45, 9, 79, 29, 80, 45, 53

; Zombie ($0B): ground snap -> 87F invuln rise -> 0.81 px/F walk (360F or 40..110F post-pass
; or ledge edge) -> 36F invuln sink -> free!
u_zombie:
    lda ob_state,x
    bne @z_not0
    lda oz_py
    sta ob_yl,x
    lda oz_py+1
    sta ob_yh,x
    ldy #16
@z_scan:
    jsr on_ground
    bcs @z_found
    lda ob_yl,x
    sec
    sbc #8
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    dey
    bne @z_scan
    jmp free
@z_found:
    jsr snap_ground
    lda #0
    sta ob_flags,x
    jsr face_player
    lda ob_var,x
    bne @z_start_walk
    lda #1
    sta ob_state,x
    lda #87
    sta ob_t0,x
    lda #<rf_5245
    ldy #>rf_5245
    jmp set_anim
@z_not0:
    cmp #1
    bne @z_not1
    dec ob_t0,x
    bne @z_ret
@z_start_walk:
    lda #2
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr face_player
    lda #180
    sta ob_t0,x
    jsr rng
    and #$3f
    clc
    adc #40
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
@z_not1:
    cmp #2
    bne @z_sink
    lda #$cf                    ; 0.81 px/F
    ldy #0
    jsr set_vx_dir
    jsr move
    jsr offscreen
    bcc :+
    jmp free
:
    jsr on_ground
    bcc @z_enter_sink
    lda ob_step,x
    bne @z_past
    jsr rel_player
    lda ob_dir,x
    beq @z_chk_pr
    lda oz_dx+1
    bpl @z_go_past
    bmi @z_tick_walk
@z_chk_pr:
    lda oz_dx+1
    bmi @z_go_past
@z_tick_walk:
    lda frames
    and #1
    bne @z_ret
    dec ob_t0,x
    beq @z_enter_sink
    rts
@z_go_past:
    lda #1
    sta ob_step,x
    rts
@z_past:
    dec ob_t1,x
    beq @z_enter_sink
    rts
@z_enter_sink:
    lda #3
    sta ob_state,x
    lda #0
    sta ob_flags,x
    lda #36
    sta ob_t0,x
    lda #<rf_5245
    ldy #>rf_5245
    jmp set_anim
@z_sink:
    dec ob_t0,x
    bne @z_ret
    jmp free

; --- Stage 2 Red Arremer ($05): Full Arcade 7-State Machine ($B4E6-$BC40) -------------
; States: 0=sleep(X+-96,Y+-48 or hit), 1=walk($0100, 0.80 px/F), 2=fly-over($0101, 1.75 px/F),
;   3=shoot($0102, 6F+shot+6F), 4=spacing($0103, 2.13 px/F 7..27F), 5=dodge($0105, 21F rise+hover),
;   6=swoop/rush($0106, 2.13 level or 43F dive+climb).
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
    lda #<rf_4BAC
    ldy #>rf_4BAC
    jsr set_anim
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
:
    lda ob_var,x
    cmp #2
    beq @ar_wake
    lda #96
    ldy #48
    jsr rel_box_le
    bcc @ar_ret0
@ar_wake:
    lda #2
    sta ob_hp,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
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
    lda #$cc                    ; 0.80 px/F
    ldy #0
    jsr set_vx_dir
    jsr move
    dec ob_t0,x
    bne @ar_chk_rush
    lda ob_aux,x
    and #7
    tay
    lda @ar_fire_t,y
    sta ob_t0,x
    inc ob_aux,x
    lda #3
    sta ob_state,x
    lda #12
    sta ob_t2,x
    lda #<rf_4DA7
    ldy #>rf_4DA7
    jmp set_anim
@ar_chk_rush:
    dec ob_t1,x
    bne @ar_chk_dist
    lda ob_aux,x
    and #7
    tay
    lda @ar_rush_t,y
    sta ob_t1,x
    lda #6
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #43
    sta ob_t2,x
    jsr face_player
    lda #<rf_4D0F
    ldy #>rf_4D0F
    jmp set_anim
@ar_chk_dist:
    lda #64
    jsr shot_near_x
    bcc @ar_no_dg
    lda #5
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    lda #<rf_4C9B
    ldy #>rf_4C9B
    jmp set_anim
@ar_no_dg:
    jsr rel_player
    lda oz_dx+1
    bpl :+
    lda #0
    sec
    sbc oz_dx
    tay
    lda #0
    sbc oz_dx+1
    jmp :++
:
    ldy oz_dx
    lda oz_dx+1
:
    bne @ar_go_fly
    cpy #48
    bcc @ar_go_space
    cpy #113
    bcc @ar_w_ret
@ar_go_space:
    lda #4
    sta ob_state,x
    jsr rng
    and #15
    clc
    adc #10
    sta ob_t2,x
    cpy #80
    bcs @ar_sp_in
    lda ob_dir,x
    eor #1
    sta ob_step,x
    rts
@ar_sp_in:
    lda ob_dir,x
    sta ob_step,x
@ar_w_ret:
    rts
@ar_go_fly:
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda #21
    sta ob_t2,x
    lda #<rf_4C9B
    ldy #>rf_4C9B
    jmp set_anim

@ar_fly:
    lda ob_step,x
    bne @arf_not0
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #3
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr move
    dec ob_t2,x
    bne @arf_ret
    lda #1
    sta ob_step,x
    lda #64
    sta ob_t2,x
    jsr face_player
    rts
@arf_not0:
    cmp #1
    bne @arf_desc
    lda #$c0                    ; 1.75 px/F
    ldy #1
    jsr set_vx_dir
    jsr move
    dec ob_t2,x
    bne @arf_ret
    lda #2
    sta ob_step,x
    lda #16
    sta ob_t2,x
    rts
@arf_desc:
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #$fd
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr move
    jsr on_ground
    bcs @arf_land
    dec ob_t2,x
    bne @arf_ret
@arf_land:
    jsr snap_ground
    jmp @ar_to_walk
@arf_ret:
    rts

@ar_shoot:
    dec ob_t2,x
    lda ob_t2,x
    cmp #6
    bne :+
    lda #0
    jmp fire_shot
:
    cmp #0
    bne @arf_ret
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
    jsr move
    dec ob_t2,x
    bne @arf_ret
    jmp @ar_to_walk

@ar_dodge:
    lda ob_step,x
    bne @ard_not0
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #1
    sta ob_vyi,x
    lda #$20                    ; 1.13 px/F
    sta ob_vyf,x
    jsr move
    dec ob_t2,x
    bne @arf_ret
    lda #1
    sta ob_step,x
    lda #40
    sta ob_t2,x
    rts
@ard_not0:
    cmp #1
    beq :+
    jmp @arf_desc
:
    dec ob_t2,x
    lda ob_t2,x
    cmp #20
    bne :+
    lda #0
    jsr fire_shot
    rts
:
    cmp #0
    beq :+
    rts
:
    lda #2
    sta ob_step,x
    lda #24
    sta ob_t2,x
    rts

@ar_swoop:
    lda #$20                    ; 2.13 px/F
    ldy #2
    jsr set_vx_dir
    lda ob_step,x
    bne @ars_up
    jsr rel_player
    lda #0
    ldy oz_dy+1
    bmi :+
    ldy oz_dy
    cpy #16
    bcc :+
    lda #$ff
:
    sta ob_vyi,x
    jsr move
    jsr on_ground
    bcc :+
    jsr snap_ground
:
    dec ob_t2,x
    bne @ars_ret
    jmp @ar_to_walk
@ars_up:
    lda #1
    sta ob_vyi,x
    jsr move
    dec ob_t2,x
    bne @ars_ret
@ar_to_walk:
    lda #1
    sta ob_state,x
    lda #<rf_4C53
    ldy #>rf_4C53
    jmp set_anim
@ars_ret:
    rts
@ar_fire_t: .byte 90, 100, 220, 130, 160, 180, 200, 120
@ar_rush_t: .byte 72, 136, 85, 92, 108, 144, 70, 49

; --- Boss: 2x Unicorn ($08 var 1 & 4, HP 10 each, Arcade $C3B7-$CB0D) ------------------
u_unicorn:
    lda ob_state,x
    bne @u_not0
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne @u_waking
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    jsr face_player
    jsr @u_set_walk_anim
    lda #32
    ldy ob_var,x
    cpy #4
    bne :+
    lda #64
:
    sta ob_t0,x
    lda ap_boss_lock
    bne @u_ret0
    lda #1
    sta ap_boss_lock
    txa
    pha
    lda #snd_song_boss
    jsr call_bank8_play_song
    pla
    tax
    rts
@u_waking:
    jsr on_screen_x
    bcc @u_ret0
    dec ob_t0,x
    bne @u_ret0
    lda #1
    sta ob_state,x
    lda #24
    sta ob_t0,x
    jsr rng
    and #$3f
    clc
    adc #51
    sta ob_t1,x
    jsr rng
    and #$3f
    clc
    adc #22
    sta ob_t2,x
    lda #1
    sta ob_step,x
@u_ret0:
    rts

@u_not0:
    cmp #1
    beq @u_walk
    cmp #2
    bne :+
    jmp @u_aim_walk
:
    cmp #3
    bne :+
    jmp @u_fire_stop
:
    cmp #4
    bne :+
    jmp @u_charge
:
    jmp @u_jump

@u_walk:
    ; Arcade $CA24: shot-dodge jump only during walk after a brief post-landing window
    lda ob_t1,x
    cmp #36
    bcs @u_no_dg
    lda #56
    jsr shot_near_x
    bcs @u_enter_jump_rand
@u_no_dg:
    dec ob_t2,x
    bne @u_chk_jc
    lda #2
    sta ob_state,x
    lda #40
    sta ob_t0,x
    jsr face_player
    jmp @u_set_aim_anim
@u_chk_jc:
    dec ob_t1,x
    bne @u_walk_seg
    jsr rng
    and #1
    beq @u_enter_charge
    jmp @u_enter_jump_aimed
@u_enter_charge:
    lda #4
    sta ob_state,x
    jsr face_player
    jmp @u_set_chg_anim
@u_enter_jump_rand:
    jsr face_player
    lda #0
    jmp @u_launch_jump

@u_walk_seg:
    dec ob_t0,x
    bne @u_do_walk
    jsr rng
    and #$1f
    clc
    adc #10
    sta ob_t0,x
    jsr face_player
    jsr @u_set_walk_anim
    lda oz_dx+1
    bpl :+
    lda #0
    sec
    sbc oz_dx
    jmp :++
:
    lda oz_dx
:
    cmp #48
    bcs @u_tog_wp
    jsr rng
    and #1
    beq @u_tog_wp
    lda #2
    jmp @u_launch_jump
@u_tog_wp:
    jsr rng
    and #3
    sta ob_step,x
@u_do_walk:
    lda ob_step,x
    beq @u_w_ret
    lda #$88                    ; 0.53 px/F
    ldy #0
    jsr set_vx_dir
    jmp @u_clamp_move
@u_w_ret:
    rts

@u_aim_walk:
    jsr face_player
    lda #$88
    ldy #0
    jsr set_vx_dir
    jsr @u_clamp_move
    dec ob_t0,x
    bne @u_w_ret
    lda #3
    sta ob_state,x
    lda #25
    sta ob_t0,x
    rts

@u_fire_stop:
    dec ob_t0,x
    bne @u_w_ret
    lda #1
    jsr fire_shot
    jmp @u_back_to_walk

@u_charge:
    lda #$cc                    ; 1.80 px/F
    ldy #1
    jsr set_vx_dir
    jsr @u_clamp_move
    jsr rel_player
    lda ob_dir,x
    bne @uc_chk_l
    lda oz_dx
    clc
    adc #16
    lda oz_dx+1
    adc #0
    bpl :+
    jmp @u_back_to_walk
:
    rts
@uc_chk_l:
    lda oz_dx+1
    bmi @uc_ret
    beq :+
    jmp @u_back_to_walk
:
    lda oz_dx
    cmp #16
    bcc @uc_ret
    jmp @u_back_to_walk
@uc_ret:
    rts

@u_enter_jump_aimed:
    jsr face_player
    lda #1
@u_launch_jump:
    sta oz_t0
    lda #5
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda oz_t0
    cmp #2
    beq @uj_back
    cmp #1
    beq @uj_aim
    jsr rng
    and #3
    beq @uj_zero
    cmp #1
    beq @uj_43
    lda #$f3
    ldy #0
    beq @uj_set_vx
@uj_zero:
    lda #0
    tay
    beq @uj_set_vx
@uj_43:
    lda #$6e
    ldy #0
    beq @uj_set_vx
@uj_back:
    lda #0
    ldy #1
    jsr set_vx_dir
    lda #0
    sec
    sbc ob_vxi,x
    sta ob_vxi,x
    jmp @uj_vy6
@uj_aim:
    jsr rel_player
    lda #63
    jsr abs_dx_le
    bcs @uj_050
    lda #127
    jsr abs_dx_le
    bcs @uj_125
    lda #0
    ldy #2
    bne @uj_set_vx
@uj_125:
    lda #$40
    ldy #1
    bne @uj_set_vx
@uj_050:
    lda #$80
    ldy #0
@uj_set_vx:
    jsr set_vx_dir
@uj_vy6:
    lda #6
    sta ob_vyi,x
    jmp @u_set_leap_anim

@u_jump:
    jsr @u_clamp_move
    lda ob_step,x
    cmp #16
    bcc @ug_hi
    cmp #26
    bcc @ug_lo
@ug_hi:
    lda #$50
    bne @ug_sub
@ug_lo:
    lda #$19
@ug_sub:
    sta oz_t0
    lda ob_vyf,x
    sec
    sbc oz_t0
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
    inc ob_step,x
    lda ob_yh,x
    cmp ob_by_h,x
    bcc @u_land
    beq :+
    rts
:
    lda ob_yl,x
    cmp ob_by_l,x
    bcc @u_land
    rts
@u_land:
    lda ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    sta ob_yh,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
@u_back_to_walk:
    lda #1
    sta ob_state,x
    lda #1
    sta ob_step,x
    jsr rng
    and #$1f
    clc
    adc #10
    sta ob_t0,x
    jsr rng
    and #$3f
    clc
    adc #51
    sta ob_t1,x
    jsr rng
    and #$3f
    clc
    adc #22
    sta ob_t2,x
    jsr face_player
@u_set_walk_anim:
    lda #0
    beq @u_anim_idx
@u_set_aim_anim:
    lda #4
    bne @u_anim_idx
@u_set_chg_anim:
    lda #8
    bne @u_anim_idx
@u_set_leap_anim:
    lda #12
@u_anim_idx:
    ldy ob_dir,x
    beq :+
    ora #2
:
    tay
    lda #0
    sta ob_mirror,x
    lda @u_an_tbl,y
    pha
    lda @u_an_tbl+1,y
    tay
    pla
    jmp set_anim
@u_an_tbl:
    .word rf_4F7D, rf_4FAB      ; 0: walk R/L
    .word rf_5081, rf_508E      ; 4: aim R/L
    .word rf_4FFF, rf_502D      ; 8: charge R/L
    .word rf_4FDF, rf_4FEC      ; 12: leap R/L

@u_clamp_move:
    jsr move
    lda ob_xh,x
    cmp #>3328
    bcc @uc_lo
    bne :+
    lda ob_xl,x
    cmp #<3328
    bcc @uc_lo
:
    lda ob_xh,x
    cmp #>3556
    bcc @uc_ok
    bne @uc_hi
    lda ob_xl,x
    cmp #<3556
    bcc @uc_ok
@uc_hi:
    lda #<3556
    sta ob_xl,x
    lda #>3556
    sta ob_xh,x
    rts
@uc_lo:
    lda #<3328
    sta ob_xl,x
    lda #>3328
    sta ob_xh,x
@uc_ok:
    rts

; --- Projectiles, items, discs, triggers, explosion, key ------------------------------
u_shot:
    jsr move
    jsr offscreen
    bcc :+
    jmp free
:
    rts

u_boom:
    dec ob_t0,x
    bne @ub_ret
    lda ob_var,x
    beq @ub_free
    dec oz_kind_cnt + ok_boom
    lda #ok_popup
    sta ob_kind,x
    inc oz_kind_cnt + ok_popup
    lda #0
    sta ob_mirror,x
    lda ob_var,x
    cmp #1
    beq @ub_500
    cmp #2
    beq @ub_1000
    lda #63
    sta ob_t0,x
    lda #<rf_E87C
    ldy #>rf_E87C
    jmp set_anim
@ub_1000:
    lda #63
    sta ob_t0,x
    lda #<rf_E878
    ldy #>rf_E878
    jmp set_anim
@ub_500:
    lda #112
    sta ob_t0,x
    lda #<rf_E870
    ldy #>rf_E870
    jmp set_anim
@ub_free:
    jmp free
@ub_ret:
    rts

u_popup:
    dec ob_t0,x
    bne :+
    jmp free
:
    rts

u_disc:
    lda ob_state,x
    bne @chk
    lda #1
    sta ob_state,x
    lda #<rf_5840
    ldy #>rf_5840
    jsr set_anim
@chk:
    jsr offscreen
    bcc :+
    jmp free
:
    lda #14
    ldy #16
    jsr rel_box_le
    bcc @ret
    stx oz_obj
    lda #2
    ldy ob_var,x
    cpy #3
    bne :+
    lda #5
:
    jsr ap_add_score
    lda #snd_se_pickup
    jsr call_bank8_play_se
    ldx oz_obj
    jmp free
@ret:
    rts

u_trigger:
    ; Hidden armor trigger ($00 var 0): restores armor if naked when touched
    jsr offscreen
    bcc :+
    jmp free
:
    lda #16
    ldy #20
    jsr rel_box_le
    bcc @ret
    lda ap_armor
    cmp #ap_armor_full
    beq @used
    lda #ap_armor_full
    sta ap_armor
    txa
    pha
    lda #snd_se_pickup
    jsr call_bank8_play_se
    pla
    tax
@used:
    jmp free
@ret:
    rts

u_item:
    jmp free

; Stage 2 Boss Key: 30F sparkle -> descent to ground -> pickup -> 181F clear sequence!
u_key:
    lda ob_state,x
    bne @not0
    lda #1
    sta ob_state,x
    lda #30
    sta ob_t0,x
    lda #<rf_7422
    ldy #>rf_7422
    jsr set_anim
    rts
@not0:
    cmp #1
    bne @not1
    dec ob_t0,x
    bne @ret
    lda #2
    sta ob_state,x
    lda #<rf_7430
    ldy #>rf_7430
    jsr set_anim
    lda #$c3
    sta ob_vyf,x
    lda #$fe                    ; -1.24 px/F descent
    sta ob_vyi,x
@ret:
    rts
@not1:
    cmp #2
    bne @wait_pick
    jsr move
    lda ob_yh,x
    bne @chk_pick
    lda ob_yl,x
    cmp #80
    bcs @chk_pick
    lda #80
    sta ob_yl,x
    lda #3
    sta ob_state,x
@wait_pick:
@chk_pick:
    lda #16
    ldy #24
    jsr rel_box_le
    bcc @ret
    ; Picked up! Clear all objects/shots and start 181F clear sequence (arcade $7341).
    ldy #ob_max - 1
    lda #0
@clr_ob:
    sta ob_kind,y
    sta ob_flags,y
    dey
    bpl @clr_ob
    ldy #ap_max_shots - 1
@clr_sh:
    sta ps_active,y
    dey
    bpl @clr_sh
    sta player_left
    sta ap_crouch
    sta ap_shoot_lock
    sta ap_clear_key_y
    lda #ap_state_ground
    sta ap_state
    lda #1
    sta ap_key_collected
    sta ap_clear_seq
    lda #50
    sta ap_clear_timer
    lda #2                      ; 200 pts on key pickup
    jmp ap_add_score

; --- Dynamic Stage 2 moving platform zone loader (all 14 arcade supports $03/$04) ---
; Zone boundaries are placed on solid stone piers so platforms never swap over water:
;   0 (player_x < 2048):      Tower elevator $03 var 0 (X=392, Y=318, 198..438) + falling $03 var 1 (X=720, Y=360)
;   1 (2048 <= x < 2560):     Channel 1 (2448, 2480, 2512) + Channel 2 left (2608)
;   2 (2560 <= x < 2736):     Channel 2 (2608, 2640, 2672) + Channel 3 left (2800)
;   3 (2736 <= x < 2944):     Channel 3 all 4 bricks (2800, 2832, 2864, 2896)
;   4 (player_x >= 2944):     Channel 3 right (2896) + Channel 4 (2992, 3024)
sync_platforms:
    lda ap_riding_plat
    bmi :+
    rts                         ; Never swap platform slots while Arthur is riding one
:
    lda player_x+1
    cmp #>2048                  ; 8
    bcc @z0
    cmp #>2560                  ; 10
    bcc @z1
    bne @chk_z3
    ; High byte == 10 (2560..2815): split between Zone 2 and Zone 3 at pier midpoint X = 2736 ($0AB0).
    lda player_x
    cmp #<2736
    bcc @z2
    bcs @z3
@chk_z3:
    cmp #>2944                  ; 11 (2816..3071): split between Zone 3 and Zone 4 at pier midpoint X = 2944 ($0B80).
    bne @z4
    lda player_x
    cmp #<2944
    bcc @z3
@z4:
    lda #4
    bne @chk_z
@z0:
    lda #0
    beq @chk_z
@z1:
    lda #1
    bne @chk_z
@z2:
    lda #2
    bne @chk_z
@z3:
    lda #3
@chk_z:
    cmp s2_plat_zone
    beq @sp_done
    sta s2_plat_zone
    ; Index = zone * 32 (4 slots x 8 bytes: type, x_lo, x_hi, y_lo, y_hi, span, timer, phase)
    asl a
    asl a
    asl a
    asl a
    asl a
    tay
    ldx #0
@load_slot:
    lda s2_plat_zones,y
    sta plat_type,x
    iny
    lda s2_plat_zones,y
    sta plat_x_lo,x
    iny
    lda s2_plat_zones,y
    sta plat_x_hi,x
    iny
    lda s2_plat_zones,y
    sta plat_y,x
    iny
    lda s2_plat_zones,y
    sta plat_y_hi,x
    iny
    lda s2_plat_zones,y
    sta plat_span,x
    iny
    lda s2_plat_zones,y
    sta plat_timer,x
    iny
    lda s2_plat_zones,y
    sta plat_phase,x
    iny
    lda #0
    sta plat_dx,x
    sta plat_dy,x
    inx
    cpx #ap_max_plats
    bne @load_slot
@sp_done:
    rts

s2_plat_zones:
    ; Zone 0: Tower 64x16 ice platforms ($03 var 0 vertical elevator 198..438, $03 var 1 fast-drop 3 px/F)
    .byte 3, <392,  >392,  <318, >318, 240, 120, 1
    .byte 4, <720,  >720,  <360, >360, 168,   0, 0
    .byte 0, 0,     0,     0,    0,    0,     0, 0
    .byte 0, 0,     0,     0,    0,    0,     0, 0
    ; Zones 1..4: Village water-channel 32x16 stone bricks ($04 var 0..3, sweep FC feet 240..464, pier=432)
    ;   var 0: y=432 phase=0 (UP)   timer=32    var 1: y=432 phase=1 (DOWN) timer=192
    ;   var 2: y=368 phase=0 (UP)   timer=96    var 3: y=368 phase=1 (DOWN) timer=128
    ; Zone 1: Channel 1 (2448 v0, 2480 v1, 2512 v2) + Channel 2 left (2608 v3)
    .byte 3, <2448, >2448, <432, >432, 224,  32, 0
    .byte 3, <2480, >2480, <432, >432, 224, 192, 1
    .byte 3, <2512, >2512, <368, >368, 224,  96, 0
    .byte 3, <2608, >2608, <368, >368, 224, 128, 1
    ; Zone 2: Channel 2 (2608 v3, 2640 v0, 2672 v1) + Channel 3 left (2800 v2)
    .byte 3, <2608, >2608, <368, >368, 224, 128, 1
    .byte 3, <2640, >2640, <432, >432, 224,  32, 0
    .byte 3, <2672, >2672, <432, >432, 224, 192, 1
    .byte 3, <2800, >2800, <368, >368, 224,  96, 0
    ; Zone 3: Channel 3 all 4 bricks (2800 v2, 2832 v3, 2864 v0, 2896 v1)
    .byte 3, <2800, >2800, <368, >368, 224,  96, 0
    .byte 3, <2832, >2832, <368, >368, 224, 128, 1
    .byte 3, <2864, >2864, <432, >432, 224,  32, 0
    .byte 3, <2896, >2896, <432, >432, 224, 192, 1
    ; Zone 4: Channel 3 right (2896 v1) + Channel 4 (2992 v2, 3024 v3)
    .byte 3, <2896, >2896, <432, >432, 224, 192, 1
    .byte 3, <2992, >2992, <368, >368, 224,  96, 0
    .byte 3, <3024, >3024, <368, >368, 224, 128, 1
    .byte 0, 0,     0,     0,    0,    0,     0, 0

.endscope
