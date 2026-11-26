; Stage 5 (Revision G Section 4: 城外〜城内〜サタン), Stage 6 (Section 5: 城内最上層〜2体サタン),
; and Stage 7 (Section 6: 大魔王アスタロトの玉座) enemies, items, triggers, and bosses.
; Placed in Bank 56 ($C000 window).
; Shares WRAM object pool (ob_*), ZP temporaries (oz_*), and s34_* scratch WRAM with Bank 51.

.segment "WRAM"
s567_boss_rem: .res 1           ; Remaining bosses in stage (S5: 1, S6: 2, S7: 1)
s567_sy: .res 2                 ; Player shot arcade Y (lo/hi)
s567_rng_hi: .res 1             ; High byte of 16-bit RNG (kept separate from oz_reg_page)
rs_tile_slot_4: .res 256        ; Sprite tile id 1024..1279 -> slot + 1 (kept at end of WRAM)
; High Score table, Name Entry state, and Title Attract Demo (kept at very end of WRAM)
hs_magic: .res 1                ; $A5 once default high-score table is initialized
hs_scores: .res 20              ; 5 entries x 4 digits (x100 pts)
hs_names: .res 15               ; 5 entries x 3 letter indices (0..25 = A..Z)
ne_rank: .res 1                 ; 0..4 rank slot being entered
ne_pos: .res 1                  ; 0..2 active letter cursor
ne_chars: .res 3                ; 3 active letter indices (0..25)
attract_idle_lo: .res 1         ; Title screen idle frame counter (low)
attract_idle_hi: .res 1         ; Title screen idle frame counter (high)
attract_active: .res 1          ; 1 while Title Attract Demo is running
attract_timer_lo: .res 1        ; Remaining attract demo frames (low)
attract_timer_hi: .res 1        ; Remaining attract demo frames (high)
attract_prev_pad: .res 1        ; Previous scripted pad byte for rising-edge pressed
attract_saved_opts: .res 4      ; Saved opt_invincible/loop/weapon/no_timer during Attract Demo
pan_y_steps: .res 1             ; Extra vertical camera pixels (0..4) for fast fall tracking

.segment "BANK56"
.scope s567
; Fixed jump table at $C001 matching BANK6, BANK49 & BANK51.
.assert * = $c001, lderror, "BANK56 jump table must start at $C001"
    jmp init
    jmp update
    jmp shot_check

oz_ptr = oz_mul
oz_i = oz_obj
oz_si = oz_t3
oz_camy = s34_camy
oz_sy = s567_sy
ofl_shoot = ofl_shootable
ofl_touch = ofl_harmful
ofl_alive = 0
ofl_shield = $80                ; Shielded (Satan guard pose: reflects player shots)
snd_se_shield = snd_se_clank
snd_se_item = snd_se_pickup
snd_se_boss_die = snd_se_kill
rf_BA04 = rf_4BAC
rf_4BD1 = rf_4BAC
rf_4651 = rf_4974
rf_E7A3 = rf_5C54

; Object kinds (match revg-section4/5/6-spawns.inc).
ok_none = 0
ok_pig = 1                      ; Woody Pig ($00)
ok_crow = 2                     ; Cave Bat ($01 var 2)
ok_devil = 3                    ; Petit Devil ($02 var 2, throws fireball)
ok_arremer = 4                  ; Red Arremer ($05)
ok_satan = 5                    ; Satan Boss ($06)
ok_unicorn = 6                  ; Unicorn ($08)
ok_skull = 7                    ; Floor Skull ($09)
ok_bigman = 8                   ; Big Man ($0A)
ok_dragon = 9                   ; Dragon ($0C, non-boss in Stage 6)
ok_astaroth = 10                ; Great Demon King Astaroth ($0D)
ok_shot = 11                    ; Enemy projectile
ok_item = 12                    ; Drop item (0=armor, 1=bonus, 2=cross)
ok_boom = 13                    ; Explosion
ok_popup = 14                   ; Score popup
ok_disc = 15                    ; 200 / 500 pt disc
ok_trigger = 16                 ; Hidden armor / 10,000 pt trigger
ok_key = 17                     ; Stage Boss Key + clear sequence
ok_kinds = 18

; Per-kind tables (docs/revision-g-late-enemy-behaviour.md).
;                 -  pig bat dev arr sat uni skl big drg ast sht itm bom pop dsc trg key
ok_hp:     .byte  0,  0,  0,  0,  2,  3,  4,  0,  9,  5,  9,  0,  0,  0,  0,  0,  0,  0
ok_points: .byte  0,  1,  1,  1, 10, 30, 20,  1,  5,  2,100,  0,  0,  0,  0,  0,  0,  0
ok_sx:     .byte  0, 12, 12, 12, 10, 16, 14, 12, 14, 12, 14,  0,  0,  0,  0,  0,  0,  0
ok_sylo:   .byte  0,$F0,$F0,$F6,$F3,$F0,$F0,$F4,$F2,$F6,$F7,  0,  0,  0,  0,  0,  0,  0
ok_syhi:   .byte  0, 12,  8, 10, 13, 16, 16, 12, 14, 10,  9,  0,  0,  0,  0,  0,  0,  0
ok_px:     .byte  0, 10, 10,  9, 16, 23, 16, 18, 14, 17, 30,  6,  0,  0,  0,  0,  0,  0
ok_pylo:   .byte  0,$F4,$F8,$F6,$ED,$E5,$EE,$EF,$EF,$F5,$DA, $FA,  0,  0,  0,  0,  0,  0
ok_pyhi:   .byte  0, 12,  8, 10, 19, 27, 18, 17, 17, 11, 38,  6,  0,  0,  0,  0,  0,  0

ok_upd_lo:
    .byte <ret_rts, <u_pig, <u_bat, <u_devil, <u_arremer, <u_satan, <u_unicorn
    .byte <u_skull, <u_bigman, <u_dragon, <u_astaroth, <u_shot, <u_item, <u_boom
    .byte <u_popup, <u_disc, <u_trigger, <u_key
ok_upd_hi:
    .byte >ret_rts, >u_pig, >u_bat, >u_devil, >u_arremer, >u_satan, >u_unicorn
    .byte >u_skull, >u_bigman, >u_dragon, >u_astaroth, >u_shot, >u_item, >u_boom
    .byte >u_popup, >u_disc, >u_trigger, >u_key

; Scheduler simultaneous limits:
; Stage 5: pig=1, devil=2
; Stage 6: pig=1, arremer=2, unicorn=1, bigman=2, dragon=1
sch_limit_s5: .byte 1, 2
sch_limit_s6: .byte 1, 2, 1, 2, 1

.include "revg-section4-spawns.inc"
.include "revg-section5-spawns.inc"
.include "revg-section6-spawns.inc"

; 16-direction velocity tables (2.0 px/frame, 8.8 signed) for Dragon & Satan aimed shots.
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
    ldx #16                     ; oz_kind_cnt has 17 bytes (0..16); ok_key (17) is not counted
:
    sta oz_kind_cnt,x
    dex
    bpl :-
    lda #$ff
    sta oz_reg_page
    sta s34_drg_head_slot
    lda #0
    sta oz_spawns
    sta s34_drg_len
    sta s34_drg_idx
    ldx #4
:
    sta s34_sch_pat,x
    sta s34_sch_rec,x
    sta s34_sch_end,x
    sta s34_sch_wait,x
    sta s34_sch_once,x
    dex
    bpl :-
    sta s34_fix_used
    sta s34_fix_used+1
    sta s34_fix_used+2
    sta s34_sup_used
    sta s34_sup_used+1
    sta oz_boss_seen
    lda #$5d
    sta oz_rng
    lda #$b9
    sta s567_rng_hi
    ; Boss count: Stage 5 = 1 (Satan), Stage 6 = 2 (2x Satan), Stage 7 = 1 (Astaroth).
    lda #1
    ldx campaign_stage
    cpx #6
    bne :+
    lda #2
:
    sta s567_boss_rem
    rts

; --- Main Per-Frame Update ------------------------------------------------------------
update:
    ; Convert player FC feet to arcade local Y.
    ; Stage 5 & 6 (5-page): local_y = 1200 ($04B0) - FC_y
    ; Stage 7     (1-page): local_y =  256 ($0100) - FC_y
    lda campaign_stage
    cmp #7
    beq @s7_coord
    sec
    lda #$b0
    sbc player_feet
    sta oz_py
    lda #$04
    sbc ap_player_y_hi
    sta oz_py+1
    sec
    lda #$b0
    sbc ap_cam_y_lo
    sta oz_camy
    lda #$04
    sbc ap_cam_y_hi
    sta oz_camy+1
    sec
    lda oz_camy
    sbc #240
    sta oz_camy
    bcs @coord_ok
    dec oz_camy+1
    jmp @coord_ok
@s7_coord:
    sec
    lda #0
    sbc player_feet
    sta oz_py
    lda #1
    sbc ap_player_y_hi
    sta oz_py+1
    lda #0
    sta oz_camy
    sta oz_camy+1
@coord_ok:
    jsr tick_rng
    jsr spawn_fixed
    jsr tick_schedulers

    ldx #0
@loop:
    stx oz_i
    lda ob_kind,x
    bne @has_obj
    jmp @next
@has_obj:
    lda ob_flash,x
    beq :+
    dec ob_flash,x
:
    ldy ob_kind,x
    lda ok_upd_lo,y
    sta oz_ptr
    lda ok_upd_hi,y
    sta oz_ptr+1
    jsr @dispatch
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
    beq :+
    jmp @loop
:
    rts

@dispatch:
    jmp (oz_ptr)

; --- Helpers --------------------------------------------------------------------------
ret_rts:
    rts

tick_rng:
    lda oz_rng
    asl a
    rol s567_rng_hi
    bcc :+
    eor #$2d
:
    sta oz_rng
    rts

alloc:
    ldx #0
@al:
    lda ob_kind,x
    beq @found
    inx
    cpx #ob_max
    bne @al
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
    sta ob_mirror,x
    sta ob_flash,x
    sta ob_hp,x
    sta ob_var,x
    sta ob_flags,x
    sta ob_dir,x
    sta ob_aux,x
    sta ob_aux2,x
    sta ob_carry,x
    clc
    rts

inc_kind_y:
    cpy #17
    bcs @iky_ret
    lda oz_kind_cnt,y
    clc
    adc #1
    sta oz_kind_cnt,y
@iky_ret:
    rts

free_obj:
    ldy ob_kind,x
    beq @fr_done
    cpy #17
    bcs :+
    lda oz_kind_cnt,y
    beq :+
    sec
    sbc #1
    sta oz_kind_cnt,y
:
    lda #0
    sta ob_kind,x
    sta ob_flags,x
@fr_done:
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

step_anim:
    lda ob_cur_l,x
    ora ob_cur_h,x
    beq @sa_done
    lda ob_cnt,x
    beq @sa_adv
    dec ob_cnt,x
    rts
@sa_adv:
    lda ob_nxt_l,x
    sta ob_cur_l,x
    sta rs_fid
    lda ob_nxt_h,x
    sta ob_cur_h,x
    sta rs_fid+1
    lda prg_bank0
    pha
    lda rs_fid+1
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    adc #revg_spr_meta_bank
    jsr select_bank0
    lda rs_fid
    sta rs_ptr
    lda rs_fid+1
    and #$1f
    ora #$80
    sta rs_ptr+1
    ldy #0
    lda (rs_ptr),y
    sta ob_cnt,x
    iny
    lda (rs_ptr),y
    sta ob_nxt_l,x
    iny
    lda (rs_ptr),y
    sta ob_nxt_h,x
    pla
    jsr select_bank0
@sa_done:
    rts

add_vel:
    clc
    lda ob_xf,x
    adc ob_vxf,x
    sta ob_xf,x
    lda ob_xl,x
    adc ob_vxi,x
    sta ob_xl,x
    lda ob_vxi,x
    bpl @vx_p
    lda ob_xh,x
    adc #$ff
    jmp @vx_s
@vx_p:
    lda ob_xh,x
    adc #0
@vx_s:
    sta ob_xh,x
    clc
    lda ob_yf,x
    adc ob_vyf,x
    sta ob_yf,x
    lda ob_yl,x
    adc ob_vyi,x
    sta ob_yl,x
    lda ob_vyi,x
    bpl @vy_p
    lda ob_yh,x
    adc #$ff
    jmp @vy_s
@vy_p:
    lda ob_yh,x
    adc #0
@vy_s:
    sta ob_yh,x
    rts

face_player:
    sec
    lda player_x
    sbc ob_xl,x
    lda player_x+1
    sbc ob_xh,x
    bmi @fp_left
    lda #0
    sta ob_dir,x
    sta ob_mirror,x
    rts
@fp_left:
    lda #1
    sta ob_dir,x
    lda #$40
    sta ob_mirror,x
    rts

check_offscreen:
    sec
    lda ob_xl,x
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    clc
    lda oz_t0
    adc #64
    lda oz_t1
    adc #0
    beq @cx_ok
    cmp #1
    bne @cx_out
    lda oz_t0
    clc
    adc #64
    cmp #128
    bcs @cx_out
@cx_ok:
    clc
    rts
@cx_out:
    sec
    rts

check_onscreen_xy:
    ; C=0 if object is within [cam_x - 16, cam_x + 272] and [cam_y - 16, cam_y + 256]
    sec
    lda ob_xl,x
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    sta oz_t1
    clc
    lda oz_t0
    adc #16
    lda oz_t1
    adc #0
    beq @ox_ok
    cmp #1
    bne @oxy_out
    lda oz_t0
    clc
    adc #16
    cmp #32
    bcs @oxy_out
@ox_ok:
    sec
    lda ob_yl,x
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    sta oz_t1
    clc
    lda oz_t0
    adc #16
    lda oz_t1
    adc #0
    beq @oy_ok
    cmp #1
    bne @oxy_out
    lda oz_t0
    clc
    adc #16
    cmp #32
    bcs @oxy_out
@oy_ok:
    clc
    rts
@oxy_out:
    sec
    rts

check_player_hit:
    lda ap_invuln
    ora opt_invincible
    bne @ph_no
    lda ap_state
    cmp #ap_state_dead
    beq @ph_no
    cmp #ap_state_knockback
    beq @ph_no
    ldy ob_kind,x
    sec
    lda ob_xl,x
    sbc player_x
    sta oz_t0
    lda ob_xh,x
    sbc player_x+1
    sta oz_t1
    lda ok_px,y
    sta oz_t2
    clc
    lda oz_t0
    adc oz_t2
    sta oz_t0
    lda oz_t1
    adc #0
    bne @ph_no
    lda oz_t2
    asl a
    cmp oz_t0
    bcc @ph_no
    sec
    lda ob_yl,x
    sbc oz_py
    sta oz_t0
    lda ob_yh,x
    sbc oz_py+1
    sta oz_t1
    lda oz_t0
    sec
    sbc ok_pylo,y
    sta oz_t0
    lda oz_t1
    sbc #$ff
    bne @ph_no
    lda ok_pyhi,y
    sec
    sbc ok_pylo,y
    cmp oz_t0
    bcc @ph_no
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    jsr ap_apply_player_damage
    ldx oz_i
    lda ob_kind,x
    cmp #ok_shot
    bne @ph_no
    jmp free_obj
@ph_no:
    rts

; Compute 16-dir index (0..15) from object X toward player -> A
dir_to_player:
    sec
    lda player_x
    sbc ob_xl,x
    sta oz_t0
    lda player_x+1
    sbc ob_xh,x
    sta oz_t1
    sec
    lda oz_py
    sbc ob_yl,x
    sta oz_t2
    lda oz_py+1
    sbc ob_yh,x
    bpl @dy_pos
    ; player is below (arcade Y smaller)
    lda oz_t1
    bmi @dl
    lda #2                      ; down-right
    rts
@dl:
    lda #6                      ; down-left
    rts
@dy_pos:
    bne @dy_hi
    lda oz_t2
    cmp #24
    bcs @dy_hi
    lda oz_t1
    bmi @hl
    lda #0                      ; right
    rts
@hl:
    lda #8                      ; left
    rts
@dy_hi:
    lda oz_t1
    bmi @ul
    lda #14                     ; up-right
    rts
@ul:
    lda #10                     ; up-left
    rts

bit_mask: .byte $01, $02, $04, $08, $10, $20, $40, $80

; --- Fixed Enemy & Trigger Spawner ----------------------------------------------------
spawn_fixed:
    lda campaign_stage
    cmp #5
    beq @s5_fix
    cmp #6
    bne :+
    jmp @s6_fix
:
    jmp @s7_fix

@s5_fix:
    ldy #0
@s5_f:
    sty oz_i
    ; Check X and Y near camera (5-page vertical tower!)
    sec
    lda revg5_fix_x_lo,y
    sbc camera_low
    sta oz_t0
    lda revg5_fix_x_hi,y
    sbc camera_high
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    lda oz_t1
    adc #0
    beq @s5_fx
    cmp #1
    beq :+
    jmp @s5_fn
:
    lda oz_t0
    clc
    adc #32
    cmp #64
    bcc @s5_fx
    jmp @s5_fn
@s5_fx:
    sec
    lda revg5_fix_y_lo,y
    sbc oz_camy
    sta oz_t0
    lda revg5_fix_y_hi,y
    sbc oz_camy+1
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    lda oz_t1
    adc #0
    beq @s5_fy
    cmp #1
    beq :+
    jmp @s5_fn
:
    lda oz_t0
    clc
    adc #32
    cmp #64
    bcc @s5_fy
    jmp @s5_fn
@s5_fy:
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
    bne @s5_fn
    stx oz_t0
    lda bit_mask,y
    sta oz_t1
    jsr alloc
    bcs @s5_fn
    ldy oz_t0
    lda oz_t1
    ora s34_fix_used,y
    sta s34_fix_used,y
    ldy oz_i
    lda revg5_fix_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_i
    lda revg5_fix_var,y
    sta ob_var,x
    lda revg5_fix_x_lo,y
    sta ob_xl,x
    lda revg5_fix_x_hi,y
    sta ob_xh,x
    lda revg5_fix_y_lo,y
    sta ob_yl,x
    lda revg5_fix_y_hi,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
@s5_fn:
    ldy oz_i
    iny
    cpy #revg5_fix_count
    beq :+
    jmp @s5_f
:
    ; Stage 5 triggers
    ldy #0
@s5_s:
    sty oz_i
    sec
    lda revg5_sup_x_lo,y
    sbc camera_low
    lda revg5_sup_x_hi,y
    sbc camera_high
    bne @s5_sn
    sec
    lda revg5_sup_y_lo,y
    sbc oz_camy
    lda revg5_sup_y_hi,y
    sbc oz_camy+1
    bne @s5_sn
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
    bne @s5_sn
    lda bit_mask,y
    ora s34_sup_used,x
    sta s34_sup_used,x
    jsr alloc
    bcs @s5_sn
    ldy oz_i
    lda #ok_trigger
    sta ob_kind,x
    inc oz_kind_cnt+ok_trigger
    lda revg5_sup_var,y
    sta ob_var,x
    lda revg5_sup_x_lo,y
    sta ob_xl,x
    lda revg5_sup_x_hi,y
    sta ob_xh,x
    lda revg5_sup_y_lo,y
    sta ob_yl,x
    lda revg5_sup_y_hi,y
    sta ob_yh,x
    lda #ofl_alive
    sta ob_flags,x
@s5_sn:
    ldy oz_i
    iny
    cpy #revg5_sup_count
    bne @s5_s
    rts

@s6_fix:
    ldy #0
@s6_f:
    sty oz_i
    sec
    lda revg6_fix_x_lo,y
    sbc camera_low
    sta oz_t0
    lda revg6_fix_x_hi,y
    sbc camera_high
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    lda oz_t1
    adc #0
    beq @s6_fx
    cmp #1
    beq :+
    jmp @s6_fn
:
    lda oz_t0
    clc
    adc #32
    cmp #64
    bcc @s6_fx
    jmp @s6_fn
@s6_fx:
    sec
    lda revg6_fix_y_lo,y
    sbc oz_camy
    sta oz_t0
    lda revg6_fix_y_hi,y
    sbc oz_camy+1
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    lda oz_t1
    adc #0
    beq @s6_fy
    cmp #1
    beq :+
    jmp @s6_fn
:
    lda oz_t0
    clc
    adc #32
    cmp #64
    bcc @s6_fy
    jmp @s6_fn
@s6_fy:
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
    bne @s6_fn
    stx oz_t0
    lda bit_mask,y
    sta oz_t1
    jsr alloc
    bcs @s6_fn
    ldy oz_t0
    lda oz_t1
    ora s34_fix_used,y
    sta s34_fix_used,y
    ldy oz_i
    lda revg6_fix_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_i
    lda revg6_fix_var,y
    sta ob_var,x
    lda revg6_fix_x_lo,y
    sta ob_xl,x
    lda revg6_fix_x_hi,y
    sta ob_xh,x
    lda revg6_fix_y_lo,y
    sta ob_yl,x
    lda revg6_fix_y_hi,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
@s6_fn:
    ldy oz_i
    iny
    cpy #revg6_fix_count
    beq :+
    jmp @s6_f
:
    ; Stage 6 triggers
    ldy #0
@s6_s:
    sty oz_i
    sec
    lda revg6_sup_x_lo,y
    sbc camera_low
    lda revg6_sup_x_hi,y
    sbc camera_high
    bne @s6_sn
    sec
    lda revg6_sup_y_lo,y
    sbc oz_camy
    lda revg6_sup_y_hi,y
    sbc oz_camy+1
    bne @s6_sn
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
    bne @s6_sn
    lda bit_mask,y
    ora s34_sup_used,x
    sta s34_sup_used,x
    jsr alloc
    bcs @s6_sn
    ldy oz_i
    lda #ok_trigger
    sta ob_kind,x
    inc oz_kind_cnt+ok_trigger
    lda revg6_sup_var,y
    sta ob_var,x
    lda revg6_sup_x_lo,y
    sta ob_xl,x
    lda revg6_sup_x_hi,y
    sta ob_xh,x
    lda revg6_sup_y_lo,y
    sta ob_yl,x
    lda revg6_sup_y_hi,y
    sta ob_yh,x
    lda #ofl_alive
    sta ob_flags,x
@s6_sn:
    ldy oz_i
    iny
    cpy #revg6_sup_count
    bne @s6_s
    rts

@s7_fix:
    ; Stage 7: single fixed Astaroth ($0D) spawned immediately on entry.
    lda s34_fix_used
    bne @s7_done
    lda #1
    sta s34_fix_used
    jsr alloc
    bcs @s7_done
    lda #ok_astaroth
    sta ob_kind,x
    inc oz_kind_cnt+ok_astaroth
    lda ok_hp+ok_astaroth
    sta ob_hp,x
    lda revg7_fix_x_lo
    sta ob_xl,x
    lda revg7_fix_x_hi
    sta ob_xh,x
    lda revg7_fix_y_lo
    sta ob_yl,x
    lda revg7_fix_y_hi
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
@s7_done:
    rts

; --- Page-Group Enemy Schedulers ------------------------------------------------------
tick_schedulers:
    lda campaign_stage
    cmp #7
    bne :+
    rts                         ; Stage 7 has no scheduled enemies
:
    cmp #6
    beq @ts_s6
    ; Stage 5: 3 cols x 5 rows; arcade row = 4 - pan_y_page
    lda #4
    sec
    sbc pan_y_page
    bpl :+
    lda #0
:
    sta oz_t0                   ; row 0..4
    asl a
    clc
    adc oz_t0                   ; row * 3
    clc
    adc camera_high
    cmp #revg5_page_count
    bcc @s5_pg
    lda #revg5_page_count - 1
@s5_pg:
    cmp oz_reg_page
    beq @s5_tick
    sta oz_reg_page
    jsr load_page_s5
@s5_tick:
    ldx #0
@s5_sl:
    stx oz_i
    lda s34_sch_pat,x
    beq @s5_sn
    lda s34_sch_wait,x
    beq @s5_sp
    dec s34_sch_wait,x
    jmp @s5_sn
@s5_sp:
    ldy revg5_sched_kind,x
    lda oz_kind_cnt,y
    cmp sch_limit_s5,x
    bcs @s5_sn
    jsr spawn_rec_s5
@s5_sn:
    ldx oz_i
    inx
    cpx #2
    bne @s5_sl
    rts

@ts_s6:
    ; Stage 6: 2 cols x 5 rows; arcade row = 4 - pan_y_page
    lda #4
    sec
    sbc pan_y_page
    bpl :+
    lda #0
:
    asl a                       ; row * 2
    clc
    adc camera_high
    cmp #revg6_page_count
    bcc @s6_pg
    lda #revg6_page_count - 1
@s6_pg:
    cmp oz_reg_page
    beq @s6_tick
    sta oz_reg_page
    jsr load_page_s6
@s6_tick:
    ldx #0
@s6_sl:
    stx oz_i
    lda s34_sch_pat,x
    beq @s6_sn
    lda s34_sch_wait,x
    beq @s6_sp
    dec s34_sch_wait,x
    jmp @s6_sn
@s6_sp:
    ldy revg6_sched_kind,x
    lda oz_kind_cnt,y
    cmp sch_limit_s6,x
    bcs @s6_sn
    jsr spawn_rec_s6
@s6_sn:
    ldx oz_i
    inx
    cpx #5
    bne @s6_sl
    rts

load_page_s5:
    ; Page index in A; each entry is 9 bytes (A * 9 = A * 8 + A)
    sta oz_t0
    asl a
    asl a
    asl a
    clc
    adc oz_t0
    tay
    ldx #0
    lda #0
    sta s34_sch_pat
    sta s34_sch_pat+1
    lda #3
    sta oz_t2
@lp5:
    lda revg5_page_groups,y
    beq @lp5_done
    sec
    sbc #1
    tax
    lda revg5_page_groups+1,y
    clc
    adc #1
    sta s34_sch_pat,x
    sec
    sbc #1
    sty oz_t1
    tay
    lda revg5_pat_start,y
    sta s34_sch_rec,x
    clc
    adc revg5_pat_count,y
    sta s34_sch_end,x
    lda #30
    sta s34_sch_wait,x
    ldy oz_t1
    lda revg5_page_groups+2,y
    sta s34_sch_once,x
    iny
    iny
    iny
    dec oz_t2
    bne @lp5
@lp5_done:
    rts

load_page_s6:
    sta oz_t0
    asl a
    asl a
    asl a
    clc
    adc oz_t0
    tay
    ldx #4
    lda #0
:
    sta s34_sch_pat,x
    dex
    bpl :-
    lda #3
    sta oz_t2
@lp6:
    lda revg6_page_groups,y
    beq @lp6_done
    sec
    sbc #1
    tax
    lda revg6_page_groups+1,y
    clc
    adc #1
    sta s34_sch_pat,x
    sec
    sbc #1
    sty oz_t1
    tay
    lda revg6_pat_start,y
    sta s34_sch_rec,x
    clc
    adc revg6_pat_count,y
    sta s34_sch_end,x
    lda #30
    sta s34_sch_wait,x
    ldy oz_t1
    lda revg6_page_groups+2,y
    sta s34_sch_once,x
    iny
    iny
    iny
    dec oz_t2
    bne @lp6
@lp6_done:
    rts

spawn_rec_s5:
    ldx oz_i
    ldy s34_sch_rec,x
    lda revg5_rec_wait,y
    sta s34_sch_wait,x
    inc s34_sch_rec,x
    lda s34_sch_rec,x
    cmp s34_sch_end,x
    bcc :+
    lda s34_sch_once,x
    and #2
    beq @s5_wrap
    lda #0
    sta s34_sch_pat,x
    jmp :+
@s5_wrap:
    lda s34_sch_pat,x
    sec
    sbc #1
    tax
    lda revg5_pat_start,x
    ldx oz_i
    sta s34_sch_rec,x
:
    sty oz_t2
    jsr alloc
    bcc :+
    rts
:
    ldy oz_i
    lda revg5_sched_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_t2
    lda revg5_rec_var,y
    sta ob_var,x
    ldy oz_i
    lda s34_sch_once,y
    and #1
    bne @s5_world
    ldy oz_t2
    clc
    lda revg5_rec_x_lo,y
    adc camera_low
    sta ob_xl,x
    lda revg5_rec_x_hi,y
    adc camera_high
    sta ob_xh,x
    clc
    lda revg5_rec_y_lo,y
    adc oz_camy
    sta ob_yl,x
    lda revg5_rec_y_hi,y
    adc oz_camy+1
    sta ob_yh,x
    jmp @s5_done
@s5_world:
    ldy oz_t2
    lda revg5_rec_x_lo,y
    sta ob_xl,x
    lda revg5_rec_x_hi,y
    sta ob_xh,x
    sec
    lda revg5_rec_y_lo,y
    sbc #<512
    sta ob_yl,x
    lda revg5_rec_y_hi,y
    sbc #>512
    sta ob_yh,x
@s5_done:
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    rts

spawn_rec_s6:
    ldx oz_i
    ldy s34_sch_rec,x
    lda revg6_rec_wait,y
    sta s34_sch_wait,x
    inc s34_sch_rec,x
    lda s34_sch_rec,x
    cmp s34_sch_end,x
    bcc :+
    lda s34_sch_once,x
    and #2
    beq @s6_wrap
    lda #0
    sta s34_sch_pat,x
    jmp :+
@s6_wrap:
    lda s34_sch_pat,x
    sec
    sbc #1
    tax
    lda revg6_pat_start,x
    ldx oz_i
    sta s34_sch_rec,x
:
    sty oz_t2
    jsr alloc
    bcc :+
    rts
:
    ldy oz_i
    lda revg6_sched_kind,y
    sta ob_kind,x
    tay
    jsr inc_kind_y
    lda ok_hp,y
    sta ob_hp,x
    ldy oz_t2
    lda revg6_rec_var,y
    sta ob_var,x
    ldy oz_i
    lda s34_sch_once,y
    and #1
    bne @s6_world
    ldy oz_t2
    clc
    lda revg6_rec_x_lo,y
    adc camera_low
    sta ob_xl,x
    lda revg6_rec_x_hi,y
    adc camera_high
    sta ob_xh,x
    clc
    lda revg6_rec_y_lo,y
    adc oz_camy
    sta ob_yl,x
    lda revg6_rec_y_hi,y
    adc oz_camy+1
    sta ob_yh,x
    jmp @s6_done
@s6_world:
    ldy oz_t2
    lda revg6_rec_x_lo,y
    sta ob_xl,x
    lda revg6_rec_x_hi,y
    sta ob_xh,x
    sec
    lda revg6_rec_y_lo,y
    sbc #<1536
    sta ob_yl,x
    lda revg6_rec_y_hi,y
    sbc #>1536
    sta ob_yh,x
@s6_done:
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    rts

; --- Woody Pig ($00) ------------------------------------------------------------------
u_pig:
    lda ob_state,x
    bne @pg_run
    inc ob_state,x
    jsr face_player
    lda ob_dir,x
    bne @pg_l
    lda #1
    sta ob_vxi,x
    lda #$60
    sta ob_vxf,x
    jmp @pg_an
@pg_l:
    lda #$fe
    sta ob_vxi,x
    lda #$a0
    sta ob_vxf,x
@pg_an:
    lda #80
    sta ob_t0,x
    lda #<rf_4651
    ldy #>rf_4651
    jmp set_anim
@pg_run:
    jsr add_vel
    dec ob_t0,x
    bne @pg_chk
    lda #90
    sta ob_t0,x
    jsr fire_aimed_shot
@pg_chk:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- Cave Bat ($01 var 2) -------------------------------------------------------------
u_bat:
    lda ob_state,x
    bne @bt_fly
    jsr check_onscreen_xy
    bcs @bt_wait
    inc ob_state,x
    jsr face_player
    lda ob_dir,x
    bne @bt_l
    lda #1
    sta ob_vxi,x
    lda #$80
    sta ob_vxf,x
    jmp @bt_an
@bt_l:
    lda #$fe
    sta ob_vxi,x
    lda #$80
    sta ob_vxf,x
@bt_an:
    lda #<rf_49D0
    ldy #>rf_49D0
    jmp set_anim
@bt_wait:
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_49B2
    ldy #>rf_49B2
    jmp set_anim
:
    rts
@bt_fly:
    inc ob_t1,x
    lda ob_t1,x
    and #$10
    beq @bt_up
    lda #$ff
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
    jmp @bt_mv
@bt_up:
    lda #0
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
@bt_mv:
    jsr add_vel
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- Petit Devil ($02 var 2: swoops and throws fireball) ------------------------------
u_devil:
    lda ob_state,x
    bne @dv_act
    inc ob_state,x
    jsr face_player
    lda #48
    sta ob_t0,x
    lda #0
    sta ob_aux,x                ; 0 = still holds fireball
    lda #<rf_A893
    ldy #>rf_A893
    jmp set_anim
@dv_act:
    dec ob_t0,x
    bne @dv_mv
    lda #40
    sta ob_t0,x
    jsr dir_to_player
    tay
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    lda ob_aux,x
    bne @dv_mv
    inc ob_aux,x
    jsr fire_aimed_shot
@dv_mv:
    jsr add_vel
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- Red Arremer ($05) ----------------------------------------------------------------
u_arremer:
    lda ob_state,x
    bne @ar_act
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_BA04
    ldy #>rf_BA04
    jsr set_anim
:
    jsr check_onscreen_xy
    bcs @ar_ret
    inc ob_state,x
    lda #40
    sta ob_t0,x
    lda #1
    sta ob_vyi,x
    lda #<rf_4BD1
    ldy #>rf_4BD1
    jmp set_anim
@ar_act:
    dec ob_t0,x
    bne @ar_mv
    lda #36
    sta ob_t0,x
    jsr dir_to_player
    tay
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    jsr face_player
    lda oz_rng
    and #1
    beq @ar_mv
    jsr fire_aimed_shot
@ar_mv:
    jsr add_vel
    jsr check_offscreen
    bcc @ar_ret
    jmp free_obj
@ar_ret:
    rts

; --- Unicorn ($08, Stage 6) -----------------------------------------------------------
u_unicorn:
    lda ob_state,x
    bne @un_act
    inc ob_state,x
    jsr face_player
    lda #45
    sta ob_t0,x
    lda #<rf_4FAB
    ldy #>rf_4FAB
    jmp set_anim
@un_act:
    dec ob_t0,x
    bne @un_mv
    lda #45
    sta ob_t0,x
    jsr face_player
    lda ob_dir,x
    bne @un_l
    lda #1
    sta ob_vxi,x
    lda #$80
    sta ob_vxf,x
    jmp @un_sh
@un_l:
    lda #$fe
    sta ob_vxi,x
    lda #$80
    sta ob_vxf,x
@un_sh:
    jsr fire_aimed_shot
@un_mv:
    jsr add_vel
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- Floor Skull ($09, Stage 5 & 6) ---------------------------------------------------
; Lies dormant (rf_CB80) until player enters +-32px, rises (rf_CC10), bounces 2x then leaps!
u_skull:
    lda ob_state,x
    beq @sk_init
    cmp #1
    beq @sk_dormant
    cmp #2
    beq @sk_rise
    jmp @sk_bounce
@sk_init:
    lda #1
    sta ob_state,x
    lda ob_yl,x
    sta ob_by_l,x               ; Remember floor Y
    lda ob_yh,x
    sta ob_by_h,x
    lda #<rf_CB80
    ldy #>rf_CB80
    jmp set_anim
@sk_dormant:
    ; Wait until player within X +-40, Y +-40
    sec
    lda ob_xl,x
    sbc player_x
    sta oz_t0
    lda ob_xh,x
    sbc player_x+1
    sta oz_t1
    clc
    lda oz_t0
    adc #40
    lda oz_t1
    adc #0
    bne @sk_ret
    lda oz_t0
    clc
    adc #40
    cmp #80
    bcs @sk_ret
    sec
    lda ob_yl,x
    sbc oz_py
    sta oz_t0
    lda ob_yh,x
    sbc oz_py+1
    sta oz_t1
    clc
    lda oz_t0
    adc #40
    lda oz_t1
    adc #0
    bne @sk_ret
    lda oz_t0
    clc
    adc #40
    cmp #80
    bcs @sk_ret
    lda #2
    sta ob_state,x
    lda #6
    sta ob_t0,x
    lda #2
    sta ob_vyi,x
    lda #<rf_CC10
    ldy #>rf_CC10
    jmp set_anim
@sk_ret:
    rts
@sk_rise:
    jsr add_vel
    dec ob_t0,x
    bne @sk_ret
    lda #3
    sta ob_state,x
    lda #2
    sta ob_aux,x                ; 2 small bounces before leap
    lda #0
    sta ob_t0,x                 ; start bounce immediately
    lda #<rf_CD04
    ldy #>rf_CD04
    jmp set_anim
@sk_bounce:
    lda ob_t0,x
    beq @sk_launch
    dec ob_t0,x
    ; Apply gravity (arcade Y+: vy -= 0.1875)
    sec
    lda ob_vyf,x
    sbc #$30
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
    jsr add_vel
    ; Land when Y <= floor Y (ob_by)
    sec
    lda ob_yl,x
    sbc ob_by_l,x
    lda ob_yh,x
    sbc ob_by_h,x
    bpl @sk_air
    lda ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    sta ob_yh,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    sta ob_vxi,x
    sta ob_vxf,x
    lda #6
    sta ob_t0,x                 ; 6F pause on ground
    rts
@sk_air:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts
@sk_launch:
    jsr face_player
    lda ob_aux,x
    beq @sk_leap
    dec ob_aux,x
    ; Small hop: vy = 3.0, vx = +-0.8
    lda #3
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    lda ob_dir,x
    bne @sk_hl
    lda #0
    sta ob_vxi,x
    lda #$cc
    sta ob_vxf,x
    jmp @sk_go
@sk_hl:
    lda #$ff
    sta ob_vxi,x
    lda #$34
    sta ob_vxf,x
    jmp @sk_go
@sk_leap:
    ; Big leap toward player: vy = 4.5, vx = +-1.75
    lda #2
    sta ob_aux,x
    lda #4
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
    lda ob_dir,x
    bne @sk_ll
    lda #1
    sta ob_vxi,x
    lda #$c0
    sta ob_vxf,x
    jmp @sk_go
@sk_ll:
    lda #$fe
    sta ob_vxi,x
    lda #$40
    sta ob_vxf,x
@sk_go:
    lda #60
    sta ob_t0,x
    rts

; --- Big Man ($0A, Stage 5 & 6) -------------------------------------------------------
u_bigman:
    lda ob_state,x
    bne @bm_act
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
:
    jsr check_onscreen_xy
    bcs @bm_ret
    inc ob_state,x
    jsr face_player
    lda #60
    sta ob_t0,x
@bm_ret:
    rts
@bm_act:
    dec ob_t0,x
    bne @bm_mv
    lda #70
    sta ob_t0,x
    jsr face_player
    lda ob_dir,x
    bne @bm_l
    lda #0
    sta ob_vxi,x
    lda #$90
    sta ob_vxf,x
    jmp @bm_sh
@bm_l:
    lda #$ff
    sta ob_vxi,x
    lda #$70
    sta ob_vxf,x
@bm_sh:
    jsr fire_horiz_shot
@bm_mv:
    jsr add_vel
    rts

; --- 9-Segment Dragon ($0C, Stage 6 regular enemy) ------------------------------------
u_dragon:
    lda ob_var,x
    beq @drg_head
    ; Body/tail segment follows head ring buffer
    lda s34_drg_head_slot
    bpl @drg_seg_ok
    jmp free_obj
@drg_seg_ok:
    lda ob_aux,x                ; segment index 1..8 -> lag = index * 7
    asl a
    asl a
    asl a
    sec
    sbc ob_aux,x
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

@drg_head:
    lda ob_state,x
    beq :+
    jmp @dh_run
:
    inc ob_state,x
    stx s34_drg_head_slot
    lda #8
    sta s34_drg_len
    lda #0
    sta s34_drg_idx
    ldy #63
@dh_fill:
    lda ob_xl,x
    sta s34_drg_hx,y
    lda ob_xh,x
    sta s34_drg_hxh,y
    lda ob_yl,x
    sta s34_drg_hy,y
    lda ob_yh,x
    sta s34_drg_hyh,y
    dey
    bpl @dh_fill
    ; Spawn 7 body + 1 tail
    lda #1
    sta oz_t2
@dh_sp:
    jsr alloc
    bcs @dh_sp_done
    lda #ok_dragon
    sta ob_kind,x
    inc oz_kind_cnt+ok_dragon
    lda #0
    sta ob_hp,x
    lda oz_t2
    sta ob_aux,x
    cmp #8
    beq @dh_tail
    lda #1
    sta ob_var,x
    lda #<rf_D5CE
    ldy #>rf_D5CE
    jmp @dh_sa
@dh_tail:
    lda #2
    sta ob_var,x
    lda #<rf_D615
    ldy #>rf_D615
@dh_sa:
    jsr set_anim
    ldy s34_drg_head_slot
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    lda ob_yl,y
    sta ob_yl,x
    lda ob_yh,y
    sta ob_yh,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    ldy oz_t2
    dey
    txa
    sta s34_drg_seg_slot,y
    inc oz_t2
    lda oz_t2
    cmp #9
    bne @dh_sp
@dh_sp_done:
    ldx oz_i
    lda #8
    sta ob_aux,x                ; initial dir = left
    lda #16
    sta ob_t0,x
    lda #48
    sta ob_t1,x
    lda #<rf_5344
    ldy #>rf_5344
    jmp set_anim

@dh_run:
    ldy s34_drg_idx
    lda ob_xl,x
    sta s34_drg_hx,y
    lda ob_xh,x
    sta s34_drg_hxh,y
    lda ob_yl,x
    sta s34_drg_hy,y
    lda ob_yh,x
    sta s34_drg_hyh,y
    iny
    tya
    and #63
    sta s34_drg_idx
    dec ob_t0,x
    bne @dh_mv
    lda #20
    sta ob_t0,x
    jsr dir_to_player
    sta ob_aux,x
@dh_mv:
    ldy ob_aux,x
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    jsr add_vel
    dec ob_t1,x
    bne @dh_done
    lda #64
    sta ob_t1,x
    jsr fire_horiz_shot
@dh_done:
    rts

; --- Satan Boss ($06, Stage 5 x1, Stage 6 x2) -----------------------------------------
; state 0: dormant/shielded until on screen (var 1 waits 180F before activating)
; state 1: floating (drifts, keeps distance, guards 30F with ofl_shield, fires aimed $0B)
; state 2: swoop dive toward player, then returns to state 1
u_satan:
    lda ob_state,x
    beq @st_wait
    cmp #1
    beq @st_float
    jmp @st_swoop

@st_wait:
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_4EA9
    ldy #>rf_4EA9
    jsr set_anim
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    lda #180
    sta ob_t0,x
:
    jsr face_player
    jsr check_onscreen_xy
    bcs @st_wret
    ; Stage 6 variant 1 waits before activating (can be shot to wake early)
    lda ob_var,x
    beq @st_wake
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    dec ob_t0,x
    bne @st_wret
@st_wake:
    lda oz_boss_seen
    bne :+
    lda #1
    sta oz_boss_seen
    lda #snd_song_boss
    jsr call_bank8_play_song
    ldx oz_i
:
    lda #1
    sta ob_state,x
    lda #3                      ; Reset HP to 4 (3+1) on activation
    sta ob_hp,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #150                    ; Float duration before swoop
    sta ob_t0,x
    lda #45                     ; Guard cycle timer
    sta ob_t1,x
    lda #0
    sta ob_aux,x                ; 0 = floating, 1 = guarding (30F)
    sta ob_t2,x                 ; Shot timer
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jmp set_anim
@st_wret:
    rts

@st_float:
    jsr face_player
    ; Guard cycle
    lda ob_aux,x
    bne @st_guarding
    ; Normal floating
    dec ob_t1,x
    bne @st_f_shoot
    ; Enter 30F guard pose (shielded!)
    lda #1
    sta ob_aux,x
    lda #30
    sta ob_t1,x
    lda #ofl_alive | ofl_shoot | ofl_touch | ofl_shield
    sta ob_flags,x
    lda #<rf_4EA9
    ldy #>rf_4EA9
    jsr set_anim
    jmp @st_drift
@st_guarding:
    dec ob_t1,x
    bne @st_drift
    ; Leave guard pose, fire aimed shot 5F later
    lda #0
    sta ob_aux,x
    lda #60
    sta ob_t1,x
    lda #5
    sta ob_t2,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jsr set_anim
    jmp @st_drift

@st_f_shoot:
    lda ob_t2,x
    beq @st_drift
    dec ob_t2,x
    bne @st_drift
    jsr fire_satan_shot
    lda #55
    sta ob_t2,x

@st_drift:
    ; Drift gently; keep within camera bounds
    lda frames
    and #$3f
    bne @st_dmv
    lda oz_rng
    and #3
    beq @sd_r
    cmp #1
    beq @sd_l
    cmp #2
    beq @sd_u
    ; down
    lda #$ff
    sta ob_vyi,x
    lda #$90
    sta ob_vyf,x
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    jmp @st_dmv
@sd_r:
    lda #0
    sta ob_vxi,x
    lda #$70
    sta ob_vxf,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jmp @st_dmv
@sd_l:
    lda #$ff
    sta ob_vxi,x
    lda #$90
    sta ob_vxf,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jmp @st_dmv
@sd_u:
    lda #0
    sta ob_vyi,x
    lda #$70
    sta ob_vyf,x
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
@st_dmv:
    jsr add_vel
    jsr clamp_satan_bounds
    dec ob_t0,x
    bne @st_fret
    ; Start swoop dive toward player!
    lda #2
    sta ob_state,x
    lda #0
    sta ob_aux,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jsr set_anim
    jsr dir_to_player
    tay
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    lda #56
    sta ob_t0,x
@st_fret:
    rts

@st_swoop:
    jsr add_vel
    ; Halfway through swoop, pull up (vy = +1.5)
    lda ob_t0,x
    cmp #28
    bne :+
    lda #1
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
:
    jsr clamp_satan_bounds
    dec ob_t0,x
    bne @st_sret
    lda #1
    sta ob_state,x
    lda #130
    sta ob_t0,x
    lda #50
    sta ob_t1,x
    lda #20
    sta ob_t2,x
@st_sret:
    rts

clamp_satan_bounds:
    ; Keep Satan within [cam_x + 24, cam_x + 232] and [cam_y + 40, cam_y + 200]
    sec
    lda ob_xl,x
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    bmi @cs_lx
    bne @cs_rx
    lda oz_t0
    cmp #24
    bcc @cs_lx
    cmp #232
    bcc @cs_y
@cs_rx:
    clc
    lda camera_low
    adc #230
    sta ob_xl,x
    lda camera_high
    adc #0
    sta ob_xh,x
    jmp @cs_y
@cs_lx:
    clc
    lda camera_low
    adc #26
    sta ob_xl,x
    lda camera_high
    adc #0
    sta ob_xh,x
@cs_y:
    sec
    lda ob_yl,x
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    bmi @cs_by
    bne @cs_ty
    lda oz_t0
    cmp #40
    bcc @cs_by
    cmp #200
    bcc @cs_ok
@cs_ty:
    clc
    lda oz_camy
    adc #198
    sta ob_yl,x
    lda oz_camy+1
    adc #0
    sta ob_yh,x
    rts
@cs_by:
    clc
    lda oz_camy
    adc #42
    sta ob_yl,x
    lda oz_camy+1
    adc #0
    sta ob_yh,x
@cs_ok:
    rts

; --- Great Demon King Astaroth ($0D, Stage 7) -----------------------------------------
; Walks between X=96..224, pauses, fires diagonal flame ($0A), enters rapid-fire mode.
; Vulnerable ONLY to Cross (ap_weapon == ap_weapon_shield) at face height (handled in shot_check).
u_astaroth:
    lda ob_state,x
    beq @as_init
    cmp #1
    beq @as_walk
    cmp #2
    beq @as_pause
    jmp @as_rapid

@as_init:
    lda #1
    sta ob_state,x
    lda #1
    sta ob_aux,x                ; walk dir: 1 = left, 0 = right
    lda #140
    sta ob_t0,x                 ; walk timer
    lda #90
    sta ob_t1,x                 ; shot timer
    lda #240
    sta ob_t2,x                 ; timer until rapid fire
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim

@as_walk:
    jsr face_player
    ; Slow walk: +-0.375 px/frame
    lda ob_aux,x
    bne @as_wl
    lda #0
    sta ob_vxi,x
    lda #$60
    sta ob_vxf,x
    jsr add_vel
    lda ob_xl,x
    cmp #216
    bcc @as_wchk
    lda #1
    sta ob_aux,x
    jmp @as_wchk
@as_wl:
    lda #$ff
    sta ob_vxi,x
    lda #$a0
    sta ob_vxf,x
    jsr add_vel
    lda ob_xl,x
    cmp #104
    bcs @as_wchk
    lda #0
    sta ob_aux,x
@as_wchk:
    jsr astaroth_fire_tick
    dec ob_t2,x
    beq @as_enter_rapid
    dec ob_t0,x
    beq :+
    rts
:
    ; Pause
    lda #2
    sta ob_state,x
    lda #90
    sta ob_t0,x
    lda #<rf_5512
    ldy #>rf_5512
    jmp set_anim

@as_pause:
    jsr face_player
    jsr astaroth_fire_tick
    dec ob_t2,x
    beq @as_enter_rapid
    dec ob_t0,x
    beq :+
    rts
:
    ; Resume walking, flip walk dir
    lda ob_aux,x
    eor #1
    sta ob_aux,x
    lda #1
    sta ob_state,x
    lda #150
    sta ob_t0,x
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim

@as_enter_rapid:
    lda #3
    sta ob_state,x
    lda #96                     ; Rapid fire duration
    sta ob_t0,x
    lda #12
    sta ob_t1,x
    lda #<rf_553A
    ldy #>rf_553A
    jmp set_anim

@as_rapid:
    jsr face_player
    dec ob_t1,x
    bne @as_rt
    jsr fire_astaroth_shot
    lda #26
    sta ob_t1,x
@as_rt:
    dec ob_t0,x
    beq :+
    rts
:
    ; Return to walking
    lda #1
    sta ob_state,x
    lda #160
    sta ob_t0,x
    lda #80
    sta ob_t1,x
    lda #220
    sta ob_t2,x
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim
@as_ret:
    rts

astaroth_fire_tick:
    dec ob_t1,x
    bne :+
    jsr fire_astaroth_shot
    lda #95
    sta ob_t1,x
:
    rts

; --- Enemy Projectiles ----------------------------------------------------------------
fire_aimed_shot:
    lda oz_kind_cnt+ok_shot
    cmp #4
    bcs @fa_no
    jsr dir_to_player
    sta oz_t2
    jsr alloc
    bcs @fa_no
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
    ldy oz_t2
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda #<rf_FDAB
    ldy #>rf_FDAB
    jsr set_anim
    ldx oz_i
@fa_no:
    rts

fire_horiz_shot:
    lda oz_kind_cnt+ok_shot
    cmp #4
    bcs @fh_no
    jsr alloc
    bcs @fh_no
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
    lda ob_dir,y
    bne @fh_l
    lda #2
    sta ob_vxi,x
    jmp @fh_an
@fh_l:
    lda #$fe
    sta ob_vxi,x
@fh_an:
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda #<rf_FD02
    ldy #>rf_FD02
    jsr set_anim
    ldx oz_i
@fh_no:
    rts

fire_satan_shot:
    lda oz_kind_cnt+ok_shot
    cmp #4
    bcs @fs_no
    jsr dir_to_player
    sta oz_t2
    jsr alloc
    bcs @fs_no
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
    ldy oz_t2
    lda drg_vxi,y
    sta ob_vxi,x
    lda drg_vxf,y
    sta ob_vxf,x
    lda drg_vyi,y
    sta ob_vyi,x
    lda drg_vyf,y
    sta ob_vyf,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda #<rf_FDCF
    ldy #>rf_FDCF
    jsr set_anim
    ldx oz_i
@fs_no:
    rts

fire_astaroth_shot:
    lda oz_kind_cnt+ok_shot
    cmp #4
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
    lda #1                      ; var 1 = Astaroth flame (disappears at floor Y <= 64)
    sta ob_var,x
    ldy oz_i
    lda ob_xl,y
    sta ob_xl,x
    lda ob_xh,y
    sta ob_xh,x
    ; High or low shot randomly
    lda oz_rng
    and #1
    beq @fas_lo
    clc
    lda ob_yl,y
    adc #12
    sta ob_yl,x
    lda ob_yh,y
    adc #0
    sta ob_yh,x
    jmp @fas_vel
@fas_lo:
    sec
    lda ob_yl,y
    sbc #5
    sta ob_yl,x
    lda ob_yh,y
    sbc #0
    sta ob_yh,x
@fas_vel:
    lda ob_dir,y
    bne @fas_l
    lda #1
    sta ob_vxi,x
    lda #$70
    sta ob_vxf,x
    jmp @fas_vy
@fas_l:
    lda #$fe
    sta ob_vxi,x
    lda #$90
    sta ob_vxf,x
@fas_vy:
    lda #$ff
    sta ob_vyi,x
    lda #$68                    ; vy = -0.59 px/F
    sta ob_vyf,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    lda #<rf_FDBF
    ldy #>rf_FDBF
    jsr set_anim
    ldx oz_i
@fas_no:
    rts

u_shot:
    jsr add_vel
    lda ob_var,x
    beq @us_off
    ; Astaroth flame disappears on hitting the floor (arcade Y <= 62)
    lda ob_yh,x
    bne @us_off
    lda ob_yl,x
    cmp #62
    bcs @us_off
    jmp free_obj
@us_off:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- Items, Effects, Triggers & Boss Key ----------------------------------------------
u_item:
    dec ob_t0,x
    bne :+
    jmp free_obj
:
    sec
    lda ob_xl,x
    sbc player_x
    sta oz_t0
    lda ob_xh,x
    sbc player_x+1
    sta oz_t1
    clc
    lda oz_t0
    adc #16
    sta oz_t0
    lda oz_t1
    adc #0
    bne @it_ret
    lda oz_t0
    cmp #32
    bcs @it_ret
    sec
    lda ob_yl,x
    sbc oz_py
    sta oz_t0
    lda ob_yh,x
    sbc oz_py+1
    sta oz_t1
    clc
    lda oz_t0
    adc #20
    sta oz_t0
    lda oz_t1
    adc #0
    bne @it_ret
    lda oz_t0
    cmp #40
    bcs @it_ret
    lda ob_var,x
    beq @it_armor
    cmp #2
    beq @it_cross
    ; Bonus points
    lda #2
    jsr ap_add_score
    jmp @it_pick
@it_armor:
    lda #ap_armor_full
    sta ap_armor
    jmp @it_pick
@it_cross:
    lda #ap_weapon_shield       ; Cross (weapon 3)
    sta ap_weapon
    inc campaign_hud_dirty
@it_pick:
    lda #snd_se_item
    jsr call_bank8_play_se
    ldx oz_i
    jmp free_obj
@it_ret:
    rts

u_boom:
    dec ob_t0,x
    bne :+
    jmp free_obj
:
    rts

u_popup:
    dec ob_t0,x
    bne :+
    jmp free_obj
:
    rts

u_disc:
    jmp free_obj

u_trigger:
    sec
    lda ob_xl,x
    sbc player_x
    sta oz_t0
    lda ob_xh,x
    sbc player_x+1
    sta oz_t1
    clc
    lda oz_t0
    adc #16
    sta oz_t0
    lda oz_t1
    adc #0
    bne @tr_ret
    lda oz_t0
    cmp #32
    bcs @tr_ret
    sec
    lda ob_yl,x
    sbc oz_py
    sta oz_t0
    lda ob_yh,x
    sbc oz_py+1
    sta oz_t1
    clc
    lda oz_t0
    adc #24
    sta oz_t0
    lda oz_t1
    adc #0
    bne @tr_ret
    lda oz_t0
    cmp #48
    bcs @tr_ret
    lda ob_var,x
    beq @tr_armor
    cmp #2
    beq @tr_200
    lda #5                      ; 500 pts
    jsr ap_add_score
    jmp @tr_done
@tr_200:
    lda #2
    jsr ap_add_score
    jmp @tr_done
@tr_armor:
    lda ap_armor
    cmp #ap_armor_full
    beq @tr_done
    lda #ap_armor_full
    sta ap_armor
@tr_done:
    lda #snd_se_item
    jsr call_bank8_play_se
    ldx oz_i
    jmp free_obj
@tr_ret:
    rts

u_key:
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne :+
    lda #<rf_7430
    ldy #>rf_7430
    jsr set_anim
:
    ; Wait for player pickup
    sec
    lda ob_xl,x
    sbc player_x
    sta oz_t0
    lda ob_xh,x
    sbc player_x+1
    sta oz_t1
    clc
    lda oz_t0
    adc #20
    sta oz_t0
    lda oz_t1
    adc #0
    bne @ky_ret
    lda oz_t0
    cmp #40
    bcs @ky_ret
    sec
    lda ob_yl,x
    sbc oz_py
    sta oz_t0
    lda ob_yh,x
    sbc oz_py+1
    sta oz_t1
    clc
    lda oz_t0
    adc #24
    sta oz_t0
    lda oz_t1
    adc #0
    bne @ky_ret
    lda oz_t0
    cmp #48
    bcs @ky_ret
    jmp start_clear_seq
@ky_ret:
    rts

start_clear_seq:
    ; Clear all objects & shots and start the 181F arcade clear walk!
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
    lda #5
    jsr ap_add_score
    lda #snd_song_clear
    jmp call_bank8_play_song

; --- Player Shot Hit Detection --------------------------------------------------------
; X = player shot slot (0..2). Returns C=1 if shot hit/absorbed, C=0 otherwise.
shot_check:
    stx oz_si
    ; Convert shot FC Y to arcade local Y
    lda campaign_stage
    cmp #7
    beq @sc_s7
    sec
    lda #$b0
    sbc ps_y,x
    sta oz_sy
    lda #$04
    sbc ps_y_hi,x
    sta oz_sy+1
    jmp @sc_go
@sc_s7:
    sec
    lda #0
    sbc ps_y,x
    sta oz_sy
    lda #1
    sbc ps_y_hi,x
    sta oz_sy+1
@sc_go:
    ldx #0
@sh_lp:
    stx oz_i
    lda ob_kind,x
    beq @sh_nx_j
    lda ob_flags,x
    and #ofl_shoot
    beq @sh_nx_j
    ldy ob_kind,x
    ; Special Astaroth body vs face check ($DD01)
    cpy #ok_astaroth
    beq @sh_astaroth
    ; Standard X box check
    ldx oz_si
    lda ps_x_lo,x
    ldy oz_i
    sec
    sbc ob_xl,y
    sta oz_t0
    lda ps_x_hi,x
    sbc ob_xh,y
    sta oz_t1
    ldx ob_kind,y
    lda ok_sx,x
    sta oz_t2
    clc
    lda oz_t0
    adc oz_t2
    sta oz_t0
    lda oz_t1
    adc #0
    bne @sh_nx_j
    lda oz_t2
    asl a
    cmp oz_t0
    bcc @sh_nx_j
    ; Standard Y box check
    ldy oz_i
    sec
    lda oz_sy
    sbc ob_yl,y
    sta oz_t0
    lda oz_sy+1
    sbc ob_yh,y
    sta oz_t1
    ldx ob_kind,y
    lda oz_t0
    sec
    sbc ok_sylo,x
    sta oz_t0
    lda oz_t1
    sbc #$ff
    bne @sh_nx_j
    lda ok_syhi,x
    sec
    sbc ok_sylo,x
    cmp oz_t0
    bcc @sh_nx_j
    jmp @sh_hit

@sh_nx_j:
    jmp @sh_nx

@sh_astaroth:
    ; Astaroth: body box X +-32, Y +-32 reflects shots; face box X +-14, Y -9..+9 takes Cross only!
    ldx oz_si
    lda ps_x_lo,x
    ldy oz_i
    sec
    sbc ob_xl,y
    sta oz_t0
    lda ps_x_hi,x
    sbc ob_xh,y
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    sta oz_t0
    lda oz_t1
    adc #0
    bne @sh_nx_j
    lda oz_t0
    cmp #64
    bcs @sh_nx_j
    sec
    lda oz_sy
    sbc ob_yl,y
    sta oz_t0
    lda oz_sy+1
    sbc ob_yh,y
    sta oz_t1
    clc
    lda oz_t0
    adc #32
    sta oz_t0
    lda oz_t1
    adc #0
    bne @sh_nx_j
    lda oz_t0
    cmp #64
    bcs @sh_nx_j
    ; Inside body! If Y is outside face (-9..+9 -> offset 23..41), body reflects the shot.
    cmp #23
    bcc @as_reflect
    cmp #42
    bcs @as_reflect
    ; At face height: shots outside face X (+-14) pass through unreflected until reaching the face!
    ldx oz_si
    lda ps_x_lo,x
    ldy oz_i
    sec
    sbc ob_xl,y
    clc
    adc #14
    cmp #29
    bcs @sh_nx_j
    ; Face hit! Only Cross (ap_weapon == ap_weapon_shield) damages Astaroth!
    lda ap_weapon
    cmp #ap_weapon_shield
    beq @sh_hit
@as_reflect:
    lda #snd_se_shield
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts

@sh_hit:
    ldx oz_i
    lda ob_flags,x
    and #ofl_shield
    beq @sh_dmg
    lda #snd_se_shield
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts

@sh_dmg:
    ; Dragon body/tail special handling
    lda ob_kind,x
    cmp #ok_dragon
    bne @sh_norm
    lda ob_var,x
    beq @sh_norm
    jmp @dragon_body_hit

@sh_norm:
    lda #6
    sta ob_flash,x
    lda ob_hp,x
    beq @sh_kill
    dec ob_hp,x
    lda #snd_se_clank
    jsr call_bank8_play_se
    ldx oz_si
    sec
    rts

@sh_kill:
    ldy ob_kind,x
    lda ok_points,y
    beq :+
    jsr ap_add_score
:
    ldx oz_i
    lda ob_kind,x
    cmp #ok_satan
    beq @kill_satan
    cmp #ok_astaroth
    beq @kill_astaroth
    cmp #ok_dragon
    beq @kill_dragon_head
    ; Normal enemy death
    jsr make_boom
    lda #snd_se_kill
    jsr call_bank8_play_se
    inc oz_spawns
    lda oz_spawns
    cmp #4                      ; Drop item every 4 kills (Cross if not held!)
    bcc @sh_done
    lda #0
    sta oz_spawns
    jsr drop_item
@sh_done:
    ldx oz_si
    sec
    rts

@kill_satan:
    jsr make_boom
    lda #snd_se_boss_die
    jsr call_bank8_play_se
    dec s567_boss_rem
    bne @sh_done
    ; All Satans defeated -> drop Boss Key at player's floor Y!
    ldx oz_i
    lda #ok_key
    sta ob_kind,x
    lda #0
    sta ob_state,x
    sta ob_cur_l,x
    sta ob_cur_h,x
    lda #ofl_alive
    sta ob_flags,x
    lda oz_py
    sta ob_yl,x
    lda oz_py+1
    sta ob_yh,x
    jmp @sh_done

@kill_astaroth:
    ; Defeating Astaroth immediately starts the Stage 7 clear sequence!
    lda #0
    sta s567_boss_rem
    lda #snd_se_boss_die
    jsr call_bank8_play_se
    jsr start_clear_seq
    jmp @sh_done

@dragon_body_hit:
    lda #2
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
    lda #10
    jsr ap_add_score
    ldy s34_drg_len
    beq @dh_end
@dh_lp:
    dey
    sty oz_t2
    ldx s34_drg_seg_slot,y
    jsr make_boom
    ldy oz_t2
    bne @dh_lp
    lda #0
    sta s34_drg_len
@dh_end:
    lda #$ff
    sta s34_drg_head_slot
    ldx oz_i
    jsr make_boom
    lda #snd_se_kill
    jsr call_bank8_play_se
    jmp @sh_done

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

make_boom:
    ldy ob_kind,x
    beq @mb_set
    cpy #17
    bcs @mb_set
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
    lda #220
    sta ob_t0,x
    lda #ofl_alive
    sta ob_flags,x
    ; Prioritize Cross weapon in Stages 5 & 6 if player does not hold it yet!
    lda ap_weapon
    cmp #ap_weapon_shield
    beq @di_other
    lda #2                      ; Cross weapon item
    sta ob_var,x
    lda #<rf_5B40
    ldy #>rf_5B40
    jmp set_anim
@di_other:
    lda ap_armor
    cmp #ap_armor_full
    beq @di_bonus
    lda #0                      ; Armor
    sta ob_var,x
    lda #<rf_5B0C
    ldy #>rf_5B0C
    jmp set_anim
@di_bonus:
    lda #1
    sta ob_var,x
    lda #<rf_5B32
    ldy #>rf_5B32
    jmp set_anim

.endscope
