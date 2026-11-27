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
gate_open_pending: .res 1       ; Non-zero: NMI writes 4 blank ($00) tiles at gate_open_addr
gate_open_hi: .res 1            ; PPU nametable address high byte for gate opening row
gate_open_lo: .res 1            ; PPU nametable address low byte for gate opening row
s2_plat_zone: .res 1            ; Stage 2 active moving-platform camera zone ($ff = none)
fe_map_scroll_x: .res 1         ; Makai Map horizontal scroll X (0..143 across $2000->$2400)
fe_map_mode: .res 1             ; 0=stage clear -> next stage, 1=opening/miss -> start/restart stage
fe_idle_lo: .res 1              ; Auto-advance timer (low) for Makai Map & Game Over
fe_idle_hi: .res 1              ; Auto-advance timer (high) for Makai Map & Game Over
fe_map_active: .res 1           ; 1 while arcade Makai Map VRC7 IRQ split + pin sprite are active
fe_map_pin_x: .res 1            ; Arcade Arthur pin base X ($6485 start / $6493 checkpoint)
fe_map_pin_y: .res 1            ; Arcade Arthur pin OAM Y (arcade_y - 17)
fe_map_pin_late: .res 1         ; 0=Stages 1-4 (show before scroll), 1=Stages 5-7 (show at scroll=143)
rs_req_slot: .res rs_max_req
rs_req_lo: .res rs_max_req
rs_req_hi: .res rs_max_req
rs_up_hi: .res rs_max_req
rs_up_lo: .res rs_max_req
rs_up_data: .res 32 * rs_max_req
rs_piece_slot: .res rs_max_pieces
ob_last_l: .res ob_max          ; Last fully-loaded frame shown for each object (rs_draw_or_last)
ob_last_h: .res ob_max
ob_last_kind: .res ob_max       ; Last ob_kind seen in slot (resets ob_last on kind change)
rs_obj_phase: .res 1            ; Coprime OAM/cache rotation phase across ob_max slots
fe_end_active: .res 1           ; 1 while Loop 2 true ending scene is active
fe_end_dirty: .res 1            ; Non-zero: VBlank writes 32-byte row to fe_end_row_hi:lo
fe_end_row_hi: .res 1
fe_end_row_lo: .res 1
fe_end_fslot: .res 6            ; Starting CHR-RAM sprite slot for ending frames 0..5
fe_end_row_buf: .res 32         ; 32-byte nametable row buffer for glitch-free page swaps
rs_no_bg_prio: .res 1           ; 1 while drawing platforms (never push behind opaque BG)
s2_boss_bgm: .res 1             ; Stage 2 Unicorn boss BGM one-shot guard (replaces ap_boss_lock)
s2_plat_clk_lo: .res 1          ; Stage 2 continuous 448-frame water-channel platform clock
s2_plat_clk_hi: .res 1

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
ok_hp:     .byte  0,  0,  0,  0,  2,  3,  9,  0,  9,  5,  9,  0,  0,  0,  0,  0,  0,  0
ok_points: .byte  0,  1,  1,  1, 10, 30, 20,  1,  5,  2,100,  0,  0,  0,  0,  0,  0,  0
ok_sx:     .byte  0, 12, 12, 12, 10, 16, 16, 12, 14, 12, 10,  0,  0,  0,  0,  0,  0,  0
ok_sylo:   .byte  0,$F0,$F0,$F6,$F3,$E8,$EB,$F4,$F2,$F6,$F7,  0,  0,  0,  0,  0,  0,  0
ok_syhi:   .byte  0, 12,  8, 10, 13, 24, 21, 12, 14, 10,  9,  0,  0,  0,  0,  0,  0,  0
ok_px:     .byte  0, 10, 10,  9, 16, 23, 23, 18, 14, 17, 30,  6,  0,  0,  0,  0,  0,  0
ok_pylo:   .byte  0,$F4,$F8,$F6,$ED,$E5,$E2,$EF,$EF,$F5,$DA, $F0,  0,  0,  0,  0,  0,  0
ok_pyhi:   .byte  0, 12,  8, 10, 19, 27, 30, 17, 17, 11, 38, 16,  0,  0,  0,  0,  0,  0

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

; Compact 16-direction velocity tables (0=R, 4=D, 8=L, 12=U; vy[d] == vx[(d+4)&15]):
;   0: 2.26 px/F (Arremer/Satan aimed), 16: 1.65 px/F (Devil var 2 fast),
;  32: 0.38 px/F (Devil slow), 48: 2.00 px/F (Dragon $5370), 64: 1.55 px/F (Pig $A0D0).
vdir_xi:
    .byte $02,$02,$01,$00,$00,$FF,$FE,$FD,$FD,$FD,$FE,$FF,$00,$00,$01,$02
    .byte $01,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
    .byte $00,$00,$00,$00,$00,$FF,$FF,$FF,$FF,$FF,$FF,$FF,$00,$00,$00,$00
    .byte $02,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
    .byte $01,$01,$01,$00,$00,$FF,$FE,$FE,$FE,$FE,$FE,$FF,$00,$00,$01,$01
vdir_xf:
    .byte $42,$17,$99,$DC,$00,$24,$67,$E9,$BE,$E9,$67,$24,$00,$DC,$99,$17
    .byte $A6,$86,$2B,$A2,$00,$5E,$D5,$7A,$5A,$7A,$D5,$5E,$00,$A2,$2B,$86
    .byte $61,$5A,$45,$25,$00,$DB,$BB,$A6,$9F,$A6,$BB,$DB,$00,$25,$45,$5A
    .byte $00,$DA,$6A,$C3,$00,$3D,$96,$26,$00,$26,$96,$3D,$00,$C3,$6A,$DA
    .byte $8D,$6F,$19,$98,$00,$68,$E7,$91,$73,$91,$E7,$68,$00,$98,$19,$6F
; 16-entry quarter-sine (0..126) for Bat wave & Floor Skull 80-px leap
s567_qsine:
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
    lda oz_ptr+1
    cmp #$c0
    bcs @disp_local
    lda prg_bank0
    pha
    lda #18                     ; BANK18 ($8000): non-ground Stage 5-7 enemy & boss AIs
    jsr select_bank0
    ldx oz_i
    jsr @disp_local
    pla
    jmp select_bank0
@disp_local:
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
    sta ob_cnt,x
    sta ob_cur_l,x
    sta ob_cur_h,x
    sta ob_nxt_l,x
    sta ob_nxt_h,x
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

; Arcade $8DC5: C=1 if X outside [cam-128, cam+383] OR Y outside [camy-64, camy+319].
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
    adc #128
    lda oz_t1
    adc #0
    cmp #2
    bcs check_off_yes
check_offscreen_y:
    sec
    lda ob_yl,x
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    sta oz_t1
    clc
    lda oz_t0
    adc #64
    sta oz_t0
    lda oz_t1
    adc #0
    beq check_off_no
    cmp #1
    bne check_off_yes
    lda oz_t0
    cmp #128
    bcc check_off_no
check_off_yes:
    sec
    rts
check_off_no:
    clc
    rts

; Probe solid/oneway floor at (ob_x, ob_y). Stage 5/6: Y_top = 1200 ($04B0), Stage 7: 256 ($0100).
on_ground:
    lda ob_xl,x
    sta ap_probe_x
    lda ob_xh,x
    sta ap_probe_x+1
    lda campaign_stage
    cmp #7
    beq @og_s7
    lda #$b0
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda #$04
    sbc ob_yh,x
    sta ap_probe_y_hi
    jmp @og_chk
@og_s7:
    lda #0
    sec
    sbc ob_yl,x
    sta ap_probe_y
    lda #1
    sbc ob_yh,x
    sta ap_probe_y_hi
@og_chk:
    bmi @og_no
    txa
    pha
    jsr ap_tile_type_at
    sta oz_t0
    pla
    tax
    lda oz_t0
    cmp #tile_type_solid
    beq @og_yes
    cmp #tile_type_oneway
    beq @og_yes
    cmp #tile_type_ladder
    bne @og_no
    lda ob_kind,x
    cmp #ok_bigman
    beq @og_yes
@og_no:
    clc
    rts
@og_yes:
    sec
    rts

snap_ground:
    lda ap_probe_y
    and #$f0
    sta oz_t0
    lda campaign_stage
    cmp #7
    beq @sg_s7
    lda #$af
    sec
    sbc oz_t0
    sta ob_yl,x
    lda #$04
    sbc ap_probe_y_hi
    sta ob_yh,x
    jmp @sg_clr
@sg_s7:
    lda #$ff
    sec
    sbc oz_t0
    sta ob_yl,x
    lda #0
    sbc ap_probe_y_hi
    sta ob_yh,x
@sg_clr:
    lda #0
    sta ob_yf,x
    sta ob_vyf,x
    sta ob_vyi,x
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

; oz_dx = player_x - ob_x, oz_dy = ob_y - oz_py (down is positive).
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

; Check rel_player within |dx|<=A and |dy|<=Y (C=1 if inside).
rel_box_le:
    pha
    tya
    pha
    jsr rel_player
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
    beq :+
    rts
:
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
    ldy ap_crouch               ; crouching lowers Arthur's top edge by 6 px
    beq :+
    sec
    sbc #6
    bcs :+
    lda #0
:
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

; Exact 16-dir index (0=R, 4=D, 8=L, 12=U) from object X toward player -> A.
dir_to_player:
    jsr rel_player
    lda oz_dx+1
    sta oz_t2
    bpl @dp_xp
    lda #0
    sec
    sbc oz_dx
    sta oz_t0
    lda #0
    sbc oz_dx+1
    beq @dp_xok
    lda #255
    sta oz_t0
    bne @dp_xok
@dp_xp:
    lda oz_dx
    sta oz_t0
    lda oz_dx+1
    beq @dp_xok
    lda #255
    sta oz_t0
@dp_xok:
    lda oz_dy+1
    sta oz_t3
    bpl @dp_yp
    lda #0
    sec
    sbc oz_dy
    sta oz_t1
    lda #0
    sbc oz_dy+1
    beq @dp_yok
    lda #255
    sta oz_t1
    bne @dp_yok
@dp_yp:
    lda oz_dy
    sta oz_t1
    lda oz_dy+1
    beq @dp_yok
    lda #255
    sta oz_t1
@dp_yok:
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

; === Non-ground Stage 5-7 Enemies & Bosses in BANK18 ($8000) ==========================
.segment "BANK18"

; --- Woody Pig ($00, Stages 5 & 6, Arcade $9ED0-$A3B0) --------------------------------
; 48F invuln eyes intro -> 1.55/2.25 px/F flat flight -> $0101 somersault -> $0102 spear!
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
    sta ob_flags,x
    jsr face_player
    lda oz_rng
    and #7
    sta ob_by_h,x
    lda s567_rng_hi
    and #3
    bne :+
    lda #1
    bne @pg_sb
:
    lda #0
@pg_sb:
    sta oz_t0
    lda oz_rng
    and #2
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
    lda ob_by_l,x
    and #1
    beq @pg_nspd
    lda #$40
    ldy #2
    bne @pg_svx
@pg_nspd:
    lda #$8d
    ldy #1
@pg_svx:
    jsr set_vx_dir
    lda oz_rng
    and #31
    clc
    adc #40
    sta ob_t0,x
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
    jsr add_vel
    dec ob_t0,x
    bne @pg_cl
    lda #4
    sta ob_state,x
    lda #46
    sta ob_t0,x
    jmp @pg_off
@pg_cl:
    lda ob_by_h,x
    beq @pg_off
    lda frames
    and #15
    bne @pg_off
    jsr rel_player
    lda #112
    jsr abs_dx_le
    bcc @pg_off
    lda oz_rng
    and #3
    bne @pg_off
    dec ob_by_h,x
    lda #3
    sta ob_state,x
    lda #8
    sta ob_step,x
    lda #4
    ldy ob_by_l,x
    tya
    and #2
    beq :+
    lda #6
    bne @pg_lt
:
    lda #4
@pg_lt:
    sta ob_t1,x
@pg_off:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts
@pg_loop:
    jsr add_vel
    dec ob_t1,x
    bne @pg_off
    lda #4
    ldy ob_by_l,x
    tya
    and #2
    beq :+
    lda #6
    bne @pg_lt2
:
    lda #4
@pg_lt2:
    sta ob_t1,x
    lda ob_dir,x
    beq @pg_cw
    lda ob_aux,x
    clc
    adc #2
    bne @pg_ls
@pg_cw:
    lda ob_aux,x
    sec
    sbc #2
@pg_ls:
    and #15
    sta ob_aux,x
    ldy #64
    jsr load_vdir
    dec ob_step,x
    bne @pg_off
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
    bne @pg_off
    jmp @pg_enter_fly

; --- Cave Bat ($01 var 2, Arcade $A450-$A720) -----------------------------------------
; Hangs until onscreen & |dx|<=127 -> 43F swoop at +-1.25 px/F -> 42F +-16px sine wave!
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
    lda #1
    sta ob_state,x
    jsr face_player
    jmp @bt_anim
@bt_hang:
    jsr check_onscreen_xy
    bcs @bt_ret
    jsr rel_player
    lda #127
    jsr abs_dx_le
    bcc @bt_ret
    jsr face_player
    lda #2
    sta ob_state,x
    lda #43
    sta ob_t0,x
    lda #$40                    ; +-1.25 px/F
    ldy #1
    jsr set_vx_dir
    lda oz_dy+1
    bmi @bt_gd
    bne @bt_sd
    lda oz_dy
    cmp #56
    bcc @bt_md
@bt_sd:
    lda #$fe
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
    bne @bt_anim
@bt_md:
    lda #$ff
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    beq @bt_anim
@bt_gd:
    lda #$ff
    sta ob_vyi,x
    lda #$c0
    sta ob_vyf,x
@bt_anim:
    lda #0
    sta ob_mirror,x
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
    jsr add_vel
    dec ob_t0,x
    bne @bt_off
    lda #3
    sta ob_state,x
    lda #0
    sta ob_t1,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
@bt_off:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts
@bt_wave:
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jsr add_vel
    inc ob_t1,x
    lda ob_t1,x
    cmp #42
    bcc :+
    lda #0
    sta ob_t1,x
:
    cmp #21
    bcs @bt_ng
    sta oz_t0
    lda #0
    sta oz_t1
    beq @bt_q
@bt_ng:
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
    sta oz_t0
    lsr a
    clc
    adc oz_t0
    cmp #16
    bcc :+
    lda #15
:
    tay
    lda s567_qsine,y
    lsr a
    lsr a
    lsr a
    sta oz_t0
    lda oz_t1
    bne @bt_sb
    lda ob_by_l,x
    clc
    adc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    jmp @bt_off
@bt_sb:
    lda ob_by_l,x
    sec
    sbc oz_t0
    sta ob_yl,x
    lda ob_by_h,x
    sbc #0
    sta ob_yh,x
    jmp @bt_off

; --- Petit Devil ($02 var 2, Stage 5, Arcade $A7D7-$AD10) -----------------------------
; 2-stage emerge (31F invuln -> 49..80F vuln) -> 3 alternating fast (1.65) / slow (0.38)
; 16-dir curving cycles, Y>=54 clamp, horiz 1.56 / down 2.0 throw (rf_FCE7 at Y-10) -> leave!
u_devil:
    lda ob_state,x
    beq @dv_init
    cmp #1
    beq @dv_emerge
    cmp #2
    beq @dv_swoop
    jmp @dv_leave
@dv_init:
    lda #1
    sta ob_state,x
    lda #0
    sta ob_flags,x              ; 31F invuln emerge
    sta ob_by_l,x
    sta ob_aux2,x               ; hasn't fired this swoop
    lda #31
    sta ob_t0,x
    lda #3
    sta ob_aux,x                ; 3 fast/slow cycles
    jsr face_player
    lda #<rf_A893
    ldy #>rf_A893
    jmp set_anim
@dv_emerge:
    lda #$10
    ldy #0
    jsr set_vx_dir
    jsr add_vel
    dec ob_t0,x
    bne @dv_ret0
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #2
    sta ob_state,x
    lda #0
    sta ob_by_l,x
    lda oz_rng
    and #31
    clc
    adc #49
    sta ob_t0,x
    jsr dir_to_player
    sta ob_step,x
    jmp @dv_load
@dv_ret0:
    rts
@dv_swoop:
    jsr add_vel
    ; Floor clamp Y >= 54 (no velocity bounce)
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
    ; Curve 16-dir heading 1 step toward Arthur every 8F
    lda frames
    and #7
    bne @dv_ns
    jsr dir_to_player
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
    jsr @dv_load
@dv_ns:
    ; Fire rf_FCE7 once per swoop when aligned horizontally (|dy|<=16, |dx|>=50) or above (dy>=60, |dx|<=5)
    lda ob_aux2,x
    bne @dv_nf
    jsr rel_player
    lda #16
    jsr abs_dy_le
    bcc @dv_cdn
    lda #49
    jsr abs_dx_le
    bcs @dv_cdn
    jsr face_player
    lda #1                      ; horizontal 1.56 px/F
    jsr fire_devil_shot
    ldx oz_i
    inc ob_aux2,x
    jmp @dv_nf
@dv_cdn:
    lda oz_dy+1
    bmi @dv_nf
    bne @dv_cd5
    lda oz_dy
    cmp #60
    bcc @dv_nf
@dv_cd5:
    lda #5
    jsr abs_dx_le
    bcc @dv_nf
    lda #2                      ; straight down 2.0 px/F
    jsr fire_devil_shot
    ldx oz_i
    inc ob_aux2,x
@dv_nf:
    dec ob_t0,x
    bne @dv_off
    lda ob_by_l,x
    bne @dv_sdn
    lda #1
    sta ob_by_l,x
    lda oz_rng
    and #31
    clc
    adc #48
    sta ob_t0,x
    jsr @dv_load
    jmp @dv_off
@dv_sdn:
    dec ob_aux,x
    beq @dv_lv
    lda #0
    sta ob_by_l,x
    sta ob_aux2,x
    lda oz_rng
    and #31
    clc
    adc #80
    sta ob_t0,x
    jsr dir_to_player
    sta ob_step,x
    jsr @dv_load
    jmp @dv_off
@dv_lv:
    lda #3
    sta ob_state,x
    lda #$a6                    ; 1.65 px/F leave
    ldy #1
    jsr set_vx_dir
@dv_leave:
    jsr add_vel
@dv_off:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts
@dv_load:
    ldy #16                     ; 1.65 px/F
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

; --- Floor Skull ($09, Stages 5 & 6, Arcade $CB60-$CD90) ------------------------------
; FIXED CRITICAL INFINITE-FREEZE BUG: separates ground-pause (ob_step==0) from airborne
; gravity (ob_step==1) & 80-px sine-arc leap (ob_step==2)!
; Dormant (rf_CB80) -> 4F rise (rf_CC10) -> 2 small bounces (vy=3.0, g=0.164, vx=+-0.80)
; with 6F ground pauses -> 40F 80-px sine-arc aimed leap toward Arthur -> repeat!
u_skull:
    lda ob_state,x
    beq @sk_init
    cmp #1
    beq @sk_dormant
    cmp #2
    beq @sk_rise
    jmp @sk_active
@sk_init:
    lda #1
    sta ob_state,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    lda #<rf_CB80
    ldy #>rf_CB80
    jmp set_anim
@sk_dormant:
    jsr check_onscreen_xy       ; arcade $CB5A: C=0 if on-screen
    bcs @sk_ret
    lda #32                     ; arcade $CB5E-$CB76: |dx|<=32 and |dy|<=32
    ldy #32
    jsr rel_box_le
    bcc @sk_ret
    lda #2
    sta ob_state,x
    lda #4                      ; arcade 4F rise
    sta ob_t0,x
    lda #2
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
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
    sta ob_aux,x                ; 2 small bounces before 80-px sine-arc leap
    lda #0
    sta ob_step,x               ; step 0 = ground pause / launch ready
    lda #1
    sta ob_t0,x
    lda #<rf_CD04
    ldy #>rf_CD04
    jmp set_anim
@sk_active:
    lda ob_step,x
    beq @sk_ground_wait
    cmp #1
    bne :+
    jmp @sk_air_hop
:
    ; step 2: 40F 80-px sine-arc aimed leap toward Arthur!
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jsr add_vel
    inc ob_t0,x
    lda ob_t0,x
    cmp #40
    bcc :+
    jmp @sk_land
:
    ; Map 0..39 -> symmetric 0..19 -> quarter-sine index 0..15
    cmp #20
    bcc :+
    lda #39
    sec
    sbc ob_t0,x
:
    lsr a
    sta oz_t0
    asl a
    clc
    adc oz_t0                   ; * 1.5 -> 0..15
    lsr a
    cmp #16
    bcc :+
    lda #15
:
    tay
    lda s567_qsine,y            ; 0..126 -> scale to 0..80 px (v*5/8 = v/2 + v/8)
    lsr a
    sta oz_t0
    lsr a
    lsr a
    clc
    adc oz_t0                   ; 0..78 (~80 px peak!)
    clc
    adc ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    adc #0
    sta ob_yh,x
    jmp @sk_off
@sk_ground_wait:
    ; Ground pause (NO gravity while waiting on floor!)
    dec ob_t0,x
    beq :+
    rts
:
    jsr face_player
    lda ob_aux,x
    beq @sk_start_leap
    dec ob_aux,x
    lda #1
    sta ob_step,x               ; airborne small hop
    lda #$cc                    ; +-0.80 px/F
    ldy #0
    jsr set_vx_dir
    lda #3                      ; vy = +3.0 px/F
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    rts
@sk_start_leap:
    lda #2
    sta ob_aux,x                ; reset for next 2 hops after leap
    lda #2
    sta ob_step,x               ; sine-arc leap
    lda #0
    sta ob_t0,x
    lda #$c0                    ; +-1.75 px/F horizontal toward Arthur
    ldy #1
    jmp set_vx_dir
@sk_air_hop:
    ; Small hop gravity: g = 0.164 px/F^2 ($00.$2A)
    sec
    lda ob_vyf,x
    sbc #$2a
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
    bpl @sk_off
@sk_land:
    lda ob_by_l,x
    sta ob_yl,x
    lda ob_by_h,x
    sta ob_yh,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    sta ob_vxi,x
    sta ob_vxf,x
    sta ob_step,x               ; return to ground-pause substate!
    lda #6                      ; 6F pause on ground
    sta ob_t0,x
    rts
@sk_off:
    jsr check_offscreen
    bcc :+
    jmp free_obj
:
    rts

; --- 9-Segment Dragon ($0C, Stage 6 regular enemy, Arcade $D622-$D9E0) ----------------
; FIXED: updates ob_dir & head sprite (rf_5344/rf_5352) on turn, DISABLES player-aiming
; in Stage 6 (pure sinusoidal patrol + curved 48F U-turns), exact lag table (10..58F),
; 60F coiling intro, fires 3.125 px/F rf_FD9C at (X+-10,Y-5), Stage 6 offscreen despawn!
u_dragon:
    lda ob_var,x
    beq @drg_head
    lda s34_drg_head_slot
    bpl @drg_seg_ok
    jmp free_obj
@drg_seg_ok:
    ldy ob_aux,x                ; segment index 0..7 -> exact arcade lag drg_lags[y]
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
@drg_head:
    lda ob_state,x
    beq :+
    jmp @dh_run
:
    lda #1                      ; State 1: 60F coiling intro ($D6D1)
    sta ob_state,x
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
    lda #0
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
    sta ob_aux,x                ; 0..7
    cmp #7
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
    txa
    sta s34_drg_seg_slot,y
    inc oz_t2
    lda oz_t2
    cmp #8
    bne @dh_sp
@dh_sp_done:
    ldx oz_i
    lda #8                      ; initial dir = left
    sta ob_aux,x
    lda #1                      ; facing left
    sta ob_dir,x
    lda #60                     ; 60F coiling intro
    sta ob_t0,x
    lda #0
    sta ob_t2,x
    lda #<rf_5352
    ldy #>rf_5352
    jmp set_anim
@dh_run:
    ; Stage 6 ($D622): despawn head & segments if far offscreen
    jsr check_offscreen
    bcc @dh_alive
    lda #$ff
    sta s34_drg_head_slot
    lda #0
    sta s34_drg_len
    jmp free_obj
@dh_alive:
    lda ob_aux,x
    ldy #48                     ; 2.0 px/F
    jsr load_vdir
    jsr add_vel
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
    ; State 1: 60F coiling intro
    lda ob_state,x
    cmp #1
    bne @dh_act
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
    bne @dh_ret
    lda #2
    sta ob_state,x
    lda #8
    sta ob_aux,x
    lda #1
    sta ob_dir,x
    lda #16
    sta ob_t0,x
    jmp @dh_set_an
@dh_ret:
    rts
@dh_act:
    ; Restore closed mouth after 12F
    lda ob_t2,x
    beq @dh_nm
    dec ob_t2,x
    bne @dh_nm
    jsr @dh_set_an
@dh_nm:
    ; Every 32F: 1/4 chance to breathe 3.125 px/F flame in facing direction!
    lda frames
    and #31
    bne @dh_nf
    lda oz_rng
    and #3
    bne @dh_nf
    jsr rel_player
    lda #72
    jsr abs_dy_le
    bcc @dh_nf
    jsr fire_horiz_shot
    ldx oz_i
    lda #12
    sta ob_t2,x
    lda ob_dir,x
    beq :+
    lda #<rf_53C2
    ldy #>rf_53C2
    jsr set_anim
    jmp @dh_nf
:
    lda #<rf_53B4
    ldy #>rf_53B4
    jsr set_anim
@dh_nf:
    ldx oz_i
    ; State 3: curved 48F U-turn (8 steps x 6F)
    lda ob_state,x
    cmp #3
    bne @dh_patrol
    dec ob_t0,x
    bne @dh_ret
    lda #6
    sta ob_t0,x
    lda ob_aux,x
    clc
    adc ob_by_l,x
    and #15
    sta ob_aux,x
    dec ob_step,x
    lda ob_step,x
    cmp #4
    bne :+
    lda ob_dir,x
    eor #1
    sta ob_dir,x
    jsr @dh_set_an
:
    lda ob_step,x
    bne @dh_ret
    lda #2
    sta ob_state,x
    lda #16
    sta ob_t0,x
    rts
@dh_patrol:
    ; Curved U-turn at arena edges [cam+28, cam+228]; Stage 6 disables player-aiming!
    lda ob_xl,x
    sec
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    bmi @dh_utr
    bne @dh_utl
    lda oz_t0
    cmp #28
    bcc @dh_utr
    cmp #228
    bcs @dh_utl
    dec ob_t0,x
    bne @dh_ret2
    lda #16
    sta ob_t0,x
    lda oz_rng
    and #3
    tay
    lda ob_dir,x
    beq @dh_wr
    lda @dh_ld,y
    sta ob_aux,x
    rts
@dh_wr:
    lda @dh_rd,y
    sta ob_aux,x
    rts
@dh_utr:
    lda ob_dir,x
    beq @dh_ret2
    lda #3
    sta ob_state,x
    lda #8
    sta ob_step,x
    lda #6
    sta ob_t0,x
    lda #1
    sta ob_by_l,x
    rts
@dh_utl:
    lda ob_dir,x
    bne @dh_ret2
    lda #3
    sta ob_state,x
    lda #8
    sta ob_step,x
    lda #6
    sta ob_t0,x
    lda #$ff
    sta ob_by_l,x
@dh_ret2:
    rts
@dh_set_an:
    lda #0
    sta ob_mirror,x
    lda ob_dir,x
    beq :+
    lda #<rf_5352
    ldy #>rf_5352
    jmp set_anim
:
    lda #<rf_5344
    ldy #>rf_5344
    jmp set_anim
@dh_ld: .byte 7, 8, 9, 8
@dh_rd: .byte 15, 0, 1, 0

; --- Satan Boss ($06, Stage 5 x1, Stage 6 x2, Arcade $BB40-$C340) ---------------------
; FIXED Variant 1 wake: waits 6x64=384F (or early-wakes at HP=1 in shot_check with HP reset=4!).
; Active Floating (180F): random guard/shot tables, 30F guard (ofl_shield) + 5F shot,
; <48px horizontal back-off at 1.31 px/F ($01.$50), smooth return-to-screen flight,
; 3 arcade swoop trajectories (level+96px climb, sine-arc 2.0/2.25, below-player 3.5 rush)!
u_satan:
    lda ob_state,x
    beq @st_wait
    cmp #1
    bne :+
    jmp @st_float
:
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
    lda #6                      ; 6 x 64 = 384F wake delay for Variant 1
    sta ob_t0,x
    lda #64
    sta ob_t1,x
:
    jsr face_player
    jsr check_onscreen_xy
    bcs @st_wret
    lda ob_var,x
    beq @st_wake
    ; Stage 6 Variant 1: unshielded while dormant; hitting down to HP=1 wakes early (shot_check)
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    dec ob_t1,x
    bne @st_wret
    lda #64
    sta ob_t1,x
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
    lda #180                    ; Arcade 180F float duration before swoop
    sta ob_t0,x
    lda oz_rng
    and #3
    tay
    lda @st_gtbl,y
    sta ob_t1,x                 ; Guard timer
    lda #0
    sta ob_aux,x                ; 0 = floating, 1 = 30F guard
    sta ob_t2,x                 ; Shot fires 5F after guard ends (arcade $BD00)
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jmp set_anim
@st_wret:
    rts
@st_float:
    jsr face_player
    lda ob_aux,x
    bne @st_guarding
    dec ob_t1,x
    bne @st_f_shoot
    lda #1
    sta ob_aux,x
    lda #30                     ; 30F guard pose (shielded)
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
    lda #0
    sta ob_aux,x
    lda oz_rng
    and #3
    tay
    lda @st_gtbl,y
    sta ob_t1,x
    lda #5                      ; Fire 16-dir 2.26 px/F shot 5F after guard ends
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
    ldx oz_i
    lda oz_rng
    and #3
    tay
    lda @st_stbl,y
    sta ob_t2,x
@st_drift:
    ; Smooth return-to-screen steering if outside [cam_x+24..cam_x+232, cam_y+40..cam_y+200]
    sec
    lda ob_xl,x
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    bmi @st_ret_r
    bne @st_ret_l
    lda oz_t0
    cmp #24
    bcc @st_ret_r
    cmp #232
    bcs @st_ret_l
    ; Arcade back-off: if |dx| < 48 px, retreat horizontally at 1.31 px/F ($01.$50)
    jsr rel_player
    lda #47
    jsr abs_dx_le
    bcc @st_wander
    lda ob_dir,x
    beq @st_ret_l               ; facing right -> back off left
@st_ret_r:
    lda #1
    sta ob_vxi,x
    lda #$50
    sta ob_vxf,x
    jmp @st_chky
@st_ret_l:
    lda #$fe
    sta ob_vxi,x
    lda #$b0
    sta ob_vxf,x
    jmp @st_chky
@st_wander:
    ; If we were just returning from edge/back-off (|vxi| >= 1), immediately pick gentle wander!
    lda ob_vxi,x
    cmp #1
    beq @st_pick_w
    cmp #$fe
    beq @st_pick_w
    lda frames
    and #$3f
    bne @st_chky
@st_pick_w:
    lda oz_rng
    and #3
    tay
    lda @st_wxi,y
    sta ob_vxi,x
    lda @st_wxf,y
    sta ob_vxf,x
    lda @st_wyi,y
    sta ob_vyi,x
    lda @st_wyf,y
    sta ob_vyf,x
@st_chky:
    sec
    lda ob_yl,x
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    bmi @st_go_u
    bne @st_go_d
    lda oz_t0
    cmp #44
    bcc @st_go_u
    cmp #196
    bcc @st_dmv
@st_go_d:
    lda #$ff
    sta ob_vyi,x
    lda #$60
    sta ob_vyf,x
    bne @st_dmv
@st_go_u:
    lda #0
    sta ob_vyi,x
    lda #$a0
    sta ob_vyf,x
@st_dmv:
    jsr add_vel
    dec ob_t0,x
    bne @st_fret
    ; Start one of 3 arcade swoop trajectories!
    lda #2
    sta ob_state,x
    lda #0
    sta ob_aux,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jsr set_anim
    jsr rel_player
    ; Trajectory 2: if Satan is below/level with Arthur -> fast 3.5 px/F rush!
    lda oz_dy+1
    bmi @st_sw_fast
    ; Else randomly pick 0 (level 2.25 + 96px climb) or 1 (sine-arc 2.0/2.25)
    lda oz_rng
    and #1
    sta ob_step,x
    bne @st_sw_arc
    ; Trajectory 0: level 2.25 px/F for 32F, then +3.0 px/F climb for 32F (= 96 px!)
    lda #$40
    ldy #2
    jsr set_vx_dir
    lda #64
    sta ob_t0,x
    rts
@st_sw_arc:
    ; Trajectory 1: 16-dir aimed sine-arc swoop at 2.0 px/F, pulling up halfway
    jsr dir_to_player
    ldy #48
    jsr load_vdir
    lda #56
    sta ob_t0,x
    rts
@st_sw_fast:
    ; Trajectory 2: fast 3.5 px/F ($03.$80) below-player rush!
    lda #2
    sta ob_step,x
    lda #$80
    ldy #3
    jsr set_vx_dir
    lda #48
    sta ob_t0,x
@st_fret:
    rts
@st_swoop:
    lda ob_step,x
    bne @st_sw12
    ; Trajectory 0: at t0==32 switch to +3.0 px/F steep climb (96 px!)
    lda ob_t0,x
    cmp #32
    bne @st_sw_mv
    lda #3
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    beq @st_sw_mv
@st_sw12:
    lda ob_t0,x
    cmp #28
    bne @st_sw_mv
    lda #1
    sta ob_vyi,x
    lda #$80
    sta ob_vyf,x
@st_sw_mv:
    jsr add_vel
    ; Keep swoop inside boss room ceiling [cam_y+40 .. cam_y+192] & side walls [cam_x+24 .. cam_x+232]
    sec
    lda ob_yl,x
    sbc oz_camy
    sta oz_t0
    lda ob_yh,x
    sbc oz_camy+1
    bmi @st_cl_by
    bne @st_cl_ty
    lda oz_t0
    cmp #40
    bcc @st_cl_by
    cmp #192
    bcc @st_cl_x
@st_cl_ty:
    clc
    lda oz_camy
    adc #190
    sta ob_yl,x
    lda oz_camy+1
    adc #0
    sta ob_yh,x
    jmp @st_cl_x
@st_cl_by:
    clc
    lda oz_camy
    adc #42
    sta ob_yl,x
    lda oz_camy+1
    adc #0
    sta ob_yh,x
@st_cl_x:
    sec
    lda ob_xl,x
    sbc camera_low
    sta oz_t0
    lda ob_xh,x
    sbc camera_high
    bmi @st_cl_lx
    bne @st_cl_rx
    lda oz_t0
    cmp #24
    bcc @st_cl_lx
    cmp #232
    bcc @st_sw_dec
@st_cl_rx:
    clc
    lda camera_low
    adc #230
    sta ob_xl,x
    lda camera_high
    adc #0
    sta ob_xh,x
    jmp @st_sw_dec
@st_cl_lx:
    clc
    lda camera_low
    adc #26
    sta ob_xl,x
    lda camera_high
    adc #0
    sta ob_xh,x
@st_sw_dec:
    dec ob_t0,x
    beq :+
    rts
:
    lda #1
    sta ob_state,x
    lda #180
    sta ob_t0,x
    lda oz_rng
    and #3
    tay
    lda @st_gtbl,y
    sta ob_t1,x
    lda #20
    sta ob_t2,x
    rts
@st_gtbl: .byte 60, 80, 100, 120
@st_stbl: .byte 50, 70, 95, 120
@st_wxi:  .byte $00, $FF, $00, $00
@st_wxf:  .byte $70, $90, $00, $00
@st_wyi:  .byte $00, $00, $00, $FF
@st_wyf:  .byte $00, $00, $70, $90

; --- Great Demon King Astaroth ($0D, Stage 7, Arcade $DB50-$DF60) ---------------------
; Walk speed 0.18 px/F (9/16) or 0.41 px/F (7/16), bounds X=96..224, 32F stomp SE,
; random walk/pause/fire/rapid tables, distance high/low flame rule (|dx|>=80 -> 75% high,
; <80 -> 100% low), Cross-only face (+-10, Y-9..+9) vs body shield (+-32, Y+-32).
u_astaroth:
    lda ob_state,x
    beq @as_init
    cmp #1
    beq @as_walk
    cmp #2
    bne :+
    jmp @as_pause
:
    jmp @as_rapid
@as_init:
    lda #1
    sta ob_state,x
    lda #1
    sta ob_aux,x                ; walk dir: 1 = left, 0 = right
    jsr @as_face
    jsr @as_pick_spd
    lda #140
    sta ob_t0,x
    lda #85
    sta ob_t1,x
    lda #220
    sta ob_t2,x
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim
; Astaroth ROM frames (rf_54C8..rf_5512) face LEFT natively, so invert ob_mirror vs face_player.
@as_face:
    jsr face_player
    lda ob_mirror,x
    eor #$40
    sta ob_mirror,x
    rts
@as_walk:
    jsr @as_face
    ; Stomp SE every 32F while walking
    lda frames
    and #31
    bne :+
    lda #snd_se_stomp
    jsr call_bank8_play_se
    ldx oz_i
:
    ; Authentic walk speed: 0.18 ($00.$2E) or 0.41 ($00.$69) px/F
    lda #$2e
    ldy ob_step,x
    beq :+
    lda #$69
:
    sta oz_t0
    lda ob_aux,x
    bne @as_wl
    lda #0
    sta ob_vxi,x
    lda oz_t0
    sta ob_vxf,x
    jsr add_vel
    lda ob_xl,x
    cmp #224                    ; arcade right bound X=224
    bcc @as_wchk
    lda #1
    sta ob_aux,x
    jmp @as_wchk
@as_wl:
    lda #0
    sec
    sbc oz_t0
    sta ob_vxf,x
    lda #$ff
    sta ob_vxi,x
    jsr add_vel
    lda ob_xl,x
    cmp #96                     ; arcade left bound X=96
    bcs @as_wchk
    lda #0
    sta ob_aux,x
@as_wchk:
    jsr @as_fire_tick
    dec ob_t2,x
    beq @as_enter_rapid
    dec ob_t0,x
    beq :+
    rts
:
    lda #2
    sta ob_state,x
    lda oz_rng
    and #3
    tay
    lda @as_ptbl,y
    sta ob_t0,x
    lda #<rf_5512
    ldy #>rf_5512
    jmp set_anim
@as_pause:
    jsr @as_face
    jsr @as_fire_tick
    dec ob_t2,x
    beq @as_enter_rapid
    dec ob_t0,x
    beq :+
    rts
:
    lda ob_aux,x
    eor #1
    sta ob_aux,x
    jsr @as_pick_spd
    lda #1
    sta ob_state,x
    lda oz_rng
    and #3
    tay
    lda @as_wtbl,y
    sta ob_t0,x
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim
@as_enter_rapid:
    lda #3
    sta ob_state,x
    lda oz_rng
    and #3
    tay
    lda @as_rtbl,y
    sta ob_t0,x
    lda #12
    sta ob_t1,x
    lda #<rf_5512
    ldy #>rf_5512
    jmp set_anim
@as_rapid:
    jsr @as_face
    dec ob_t1,x
    bne @as_rt
    lda #1                      ; rapid mode (random 50/50 high/low)
    jsr fire_astaroth_shot
    ldx oz_i
    lda #26
    sta ob_t1,x
@as_rt:
    dec ob_t0,x
    bne @as_ret
    jsr @as_pick_spd
    lda #1
    sta ob_state,x
    lda oz_rng
    and #3
    tay
    lda @as_wtbl,y
    sta ob_t0,x
    lda @as_ftbl,y
    sta ob_t1,x
    lda @as_ttbl,y
    sta ob_t2,x
    lda #<rf_54C8
    ldy #>rf_54C8
    jmp set_anim
@as_ret:
    rts
@as_fire_tick:
    dec ob_t1,x
    bne :+
    lda #0                      ; normal mode (distance high/low rule)
    jsr fire_astaroth_shot
    ldx oz_i
    lda oz_rng
    and #3
    tay
    lda @as_ftbl,y
    sta ob_t1,x
:
    rts
@as_pick_spd:
    ; 9/16 slow 0.18 px/F (ob_step=0), 7/16 fast 0.41 px/F (ob_step=1)
    lda oz_rng
    and #15
    cmp #9
    bcc :+
    lda #1
    bne @as_ps
:
    lda #0
@as_ps:
    sta ob_step,x
    rts
@as_wtbl: .byte 100, 140, 180, 220
@as_ptbl: .byte 60, 90, 120, 150
@as_ftbl: .byte 70, 95, 120, 145
@as_ttbl: .byte 180, 200, 220, 250
@as_rtbl: .byte 80, 96, 112, 128

; === Ground-Probing Stage 5-6 Enemies back in BANK56 ($C000) ==========================
.segment "BANK56"

; --- Red Arremer ($05, Stages 5 & 6, Arcade $B4E6-$BC40) -------------------------------
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
    lda #$cc                    ; 0.80 px/F
    ldy #0
    jsr set_vx_dir
    jsr add_vel
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
    jsr rel_player
    lda #47
    jsr abs_dx_le
    bcs @ar_go_sp
    lda #112
    jsr abs_dx_le
    bcs @ar_yc
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
    lda oz_rng
    and #15
    clc
    adc #10
    sta ob_t2,x
    lda ob_dir,x
    eor #1
    sta ob_step,x
@ar_yc:
    jsr check_offscreen_y
    bcc :+
    jmp free_obj
:
    rts
@ar_fly:
    lda ob_step,x
    bne @arf1
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #3
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr add_vel
    dec ob_t2,x
    bne @ar_yc
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
    jsr add_vel
    dec ob_t2,x
    bne @ar_yc
    lda #2
    sta ob_step,x
    lda #20
    sta ob_t2,x
    rts
@arf_dn:
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #$fd
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr add_vel
    jsr on_ground
    bcs @arf_ld
    dec ob_t2,x
    bne @ar_yc
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
    jsr fire_aimed_shot
    ldx oz_i
    rts
:
    cmp #0
    beq :+
    jmp @ar_yc2
:
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
    jsr add_vel
    dec ob_t2,x
    beq :+
    jmp @ar_yc2
:
    jmp @ar_to_walk
@ar_dodge:
    lda ob_step,x
    bne @ard1
    lda #0
    sta ob_vxi,x
    sta ob_vxf,x
    lda #1
    sta ob_vyi,x
    lda #$20
    sta ob_vyf,x
    jsr add_vel
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
    jsr fire_aimed_shot
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
    jsr add_vel
    jsr on_ground
    bcc :+
    jsr snap_ground
:
    dec ob_t2,x
    bne @ar_yc2
    jmp @ar_to_walk
@ar_yc2:
    jsr check_offscreen_y
    bcc :+
    jmp free_obj
:
    rts
@ar_ft: .byte 90,100,220,130,160,180,200,120
@ar_rt: .byte 72,136,85,92,108,144,70,49

.segment "BANK18"
; --- Unicorn ($08 var 2, Stage 6, HP 10, Arcade $C3B7-$CB0D) --------------------------
; Full arcade AI: 10 HP, 4-hit early wake, 0.53 px/F walk/pause, 40F aim-walk + 25F stop
; + 3.0 px/F horiz shot (Y+8), 1.80 px/F charge to 16px past Arthur, 2-stage gravity jump,
; Stage 6 local bounds X in [152..328] (arcade 7576..7752).
u_unicorn:
    lda ob_state,x
    bne @u_not0
    lda ob_cur_l,x
    ora ob_cur_h,x
    bne @u_waking
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda ob_yl,x
    sta ob_by_l,x
    lda ob_yh,x
    sta ob_by_h,x
    jsr face_player
    jsr @u_an_walk
    lda #48                     ; 48F wake-up (or 4-hit early wake in shot_check)
    sta ob_t0,x
    rts
@u_waking:
    jsr check_onscreen_xy
    bcs @u_ret0
    dec ob_t0,x
    bne @u_ret0
    jmp @u_back_walk
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
    lda ob_t1,x
    cmp #36
    bcs @u_ndg
    lda #56
    jsr shot_near_x
    bcs @u_jrand
@u_ndg:
    dec ob_t2,x
    bne @u_cjc
    lda #2
    sta ob_state,x
    lda #40
    sta ob_t0,x
    jsr face_player
    jmp @u_an_aim
@u_cjc:
    dec ob_t1,x
    bne @u_wseg
    lda oz_rng
    and #1
    beq @u_go_chg
    jmp @u_jaim
@u_go_chg:
    lda #4
    sta ob_state,x
    jsr face_player
    jmp @u_an_chg
@u_jrand:
    jsr face_player
    lda #0
    jmp @u_launch_j
@u_wseg:
    dec ob_t0,x
    bne @u_dw
    lda oz_rng
    and #$1f
    clc
    adc #10
    sta ob_t0,x
    jsr face_player
    jsr @u_an_walk
    jsr rel_player
    lda #47
    jsr abs_dx_le
    bcc @u_twp
    lda s567_rng_hi
    and #1
    beq @u_twp
    lda #2
    jmp @u_launch_j
@u_twp:
    lda oz_rng
    and #3
    sta ob_step,x
@u_dw:
    lda ob_step,x
    beq @u_wret
    lda #$88                    ; 0.53 px/F
    ldy #0
    jsr set_vx_dir
    jmp @u_cmove
@u_wret:
    rts
@u_aim_walk:
    jsr face_player
    lda #$88
    ldy #0
    jsr set_vx_dir
    jsr @u_cmove
    dec ob_t0,x
    bne @u_wret
    lda #3
    sta ob_state,x
    lda #25
    sta ob_t0,x
    rts
@u_fire_stop:
    dec ob_t0,x
    bne @u_wret
    jsr fire_horiz_shot         ; Unicorn 3.0 px/F horiz (rf_FDAB/B3 at Y+8)
    ldx oz_i
    jmp @u_back_walk
@u_charge:
    lda #$cc                    ; 1.80 px/F
    ldy #1
    jsr set_vx_dir
    jsr @u_cmove
    jsr rel_player
    lda ob_dir,x
    bne @uc_l
    lda oz_dx
    clc
    adc #16
    lda oz_dx+1
    adc #0
    bpl @u_wret
    jmp @u_back_walk
@uc_l:
    lda oz_dx+1
    bmi @u_wret
    beq :+
    jmp @u_back_walk
:
    lda oz_dx
    cmp #16
    bcc @u_wret
    jmp @u_back_walk
@u_jaim:
    jsr face_player
    lda #1
@u_launch_j:
    sta oz_t0
    lda #5
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda oz_t0
    cmp #2
    beq @uj_bk
    cmp #1
    beq @uj_am
    lda oz_rng
    and #3
    beq @uj_z
    cmp #1
    beq @uj_43
    lda #$f3
    ldy #0
    beq @uj_sv
@uj_z:
    lda #0
    tay
    beq @uj_sv
@uj_43:
    lda #$6e
    ldy #0
    beq @uj_sv
@uj_bk:
    lda #0
    ldy #1
    jsr set_vx_dir
    lda #0
    sec
    sbc ob_vxi,x
    sta ob_vxi,x
    jmp @uj_vy
@uj_am:
    jsr rel_player
    lda #63
    jsr abs_dx_le
    bcs @uj_50
    lda #127
    jsr abs_dx_le
    bcs @uj_125
    lda #0
    ldy #2
    bne @uj_sv
@uj_125:
    lda #$40
    ldy #1
    bne @uj_sv
@uj_50:
    lda #$80
    ldy #0
@uj_sv:
    jsr set_vx_dir
@uj_vy:
    lda #6
    sta ob_vyi,x
    jmp @u_an_leap
@u_jump:
    jsr @u_cmove
    lda ob_step,x
    cmp #16
    bcc @ug_hi
    cmp #26
    bcc @ug_lo
@ug_hi:
    lda #$50
    bne @ug_sb
@ug_lo:
    lda #$19
@ug_sb:
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
@u_back_walk:
    lda #1
    sta ob_state,x
    sta ob_step,x
    lda oz_rng
    and #$1f
    clc
    adc #10
    sta ob_t0,x
    lda s567_rng_hi
    and #$3f
    clc
    adc #51
    sta ob_t1,x
    lda oz_rng
    and #$3f
    clc
    adc #22
    sta ob_t2,x
    jsr face_player
@u_an_walk:
    lda #0
    beq @u_ai
@u_an_aim:
    lda #4
    bne @u_ai
@u_an_chg:
    lda #8
    bne @u_ai
@u_an_leap:
    lda #12
@u_ai:
    ldy ob_dir,x
    beq :+
    ora #2
:
    tay
    lda #0
    sta ob_mirror,x
    lda @u_atbl,y
    pha
    lda @u_atbl+1,y
    tay
    pla
    jmp set_anim
@u_atbl:
    .word rf_4F7D, rf_4FAB
    .word rf_5081, rf_508E
    .word rf_4FFF, rf_502D
    .word rf_4FDF, rf_4FEC
@u_cmove:
    jsr add_vel
    ; Stage 6 Unicorn local bounds: X in [152..328] ($0098..$0148)
    lda ob_xh,x
    bne @uc_h1
    lda ob_xl,x
    cmp #152
    bcs @uc_ok
    lda #152
    sta ob_xl,x
    rts
@uc_h1:
    cmp #1
    bne @uc_hi
    lda ob_xl,x
    cmp #<328
    bcc @uc_ok
@uc_hi:
    lda #<328
    sta ob_xl,x
    lda #>328
    sta ob_xh,x
@uc_ok:
    rts

.segment "BANK56"
; --- Big Man ($0A var 0 & 1, Stages 5 & 6, HP 10, Arcade $CE3A-$D20F) -----------------
; Full arcade AI: NEVER despawns offscreen, pauses offscreen, var 1 waits for |dy|<=16 +
; staggered 128/192F wake delay so Stage 6 pairs separate!, 0.53 px/F walk + ledge check
; + 70F/100F idle pause, State 2 Charge + horiz shot, State 3 Downward Shoot, State 4 6F stun!
u_bigman:
    lda ob_state,x
    bne @bm_act
    jsr check_onscreen_xy
    bcs @bm_ret0
    ; Variant 1 (Stage 6 pairs): wait until |dy|<=16 AND 128/192F staggered delay!
    lda ob_var,x
    beq @bm_wake
    lda ob_t0,x
    bne @bm_v1_cnt
    jsr rel_player
    lda #16
    jsr abs_dy_le
    bcc @bm_ret0
    lda #128
    ldy oz_rng
    bpl :+
    lda #192
:
    sta ob_t0,x
@bm_ret0:
    rts
@bm_v1_cnt:
    dec ob_t0,x
    bne @bm_ret0
@bm_wake:
    lda #1
    sta ob_state,x
    lda #ofl_alive | ofl_shoot | ofl_touch
    sta ob_flags,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
    jsr face_player
    lda #0
    sta ob_aux,x
    sta ob_step,x
    lda #$ff
    sta ob_by_l,x
    lda @bm_wtbl
    sta ob_t0,x
    lda @bm_rtbl
    sta ob_aux2,x
    rts
@bm_act:
    jsr check_onscreen_xy
    bcs @bm_ret0                ; pause offscreen, NEVER despawn!
    lda ob_state,x
    cmp #4
    bne @bm_ns
    ; State 4 ($0103): 6F hit-stun
    dec ob_t0,x
    bne @bm_ret0
    jmp @bm_ewalk
@bm_ns:
    cmp #5
    bne @bm_ni
    ; State 5 ($0100.2): 100F/70F idle pause -> random dir -> walk
    dec ob_t0,x
    bne @bm_ret0
    lda oz_rng
    and #1
    sta ob_dir,x
    beq :+
    lda #$40
:
    sta ob_mirror,x
    jmp @bm_ewalk
@bm_ni:
    cmp #1
    beq @bm_patrol
    cmp #2
    bne :+
    jmp @bm_charge
:
    jmp @bm_down
@bm_patrol:
    jsr @bm_yband
    bcc @bm_cb
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
@bm_cb:
    jsr @bm_below
    bcc @bm_ws
    lda #3
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
@bm_ws:
    lda frames
    and #15
    bne @bm_dw
    jsr on_ground
    bcs @bm_onf
    lda ob_by_l,x
    bpl :+
    lda ob_dir,x
    sta ob_by_l,x
:
    lda ob_dir,x
    cmp ob_by_l,x
    bne @bm_dw
    eor #1
    sta ob_dir,x
    lda ob_mirror,x
    eor #$40
    sta ob_mirror,x
    jmp @bm_dw
@bm_onf:
    lda #$ff
    sta ob_by_l,x
@bm_dw:
    lda #$88                    ; 0.53 px/F
    ldy #0
    jsr set_vx_dir
    jsr add_vel
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
    lda oz_rng
    and #1
    bne @bm_i70
    lda #100
    sta ob_t0,x
    lda #<rf_5148
    ldy #>rf_5148
    jmp set_anim
@bm_i70:
    lda #70
    sta ob_t0,x
    lda #<rf_511C
    ldy #>rf_511C
    jmp set_anim
@bm_ret1:
    rts
@bm_ewalk:
    lda #1
    sta ob_state,x
    lda #0
    sta ob_step,x
    lda ob_aux,x
    and #$0f
    tay
    lda @bm_wtbl,y
    sta ob_t0,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim
@bm_charge:
    lda ob_step,x
    bne @bmc_n0
    jsr face_player
    lda #1
    sta ob_step,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim
@bmc_n0:
    cmp #1
    bne @bmc_n1
    jsr @bm_yband
    bcs :+
    jmp @bm_ewalk
:
    lda ob_dir,x
    bne @bmc_bl
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
@bmc_bl:
    lda oz_dx+1
    bmi @bmc_to2
    bne @bmc_tr
    lda oz_dx
    cmp #40
    bcc @bmc_to2
@bmc_tr:
    lda #0
    sta ob_dir,x
    sta ob_mirror,x
@bmc_to2:
    lda #2
    sta ob_step,x
    rts
@bmc_n1:
    cmp #2
    beq :+
    jmp @bmc_n2
:
    lda #0
    ldy #2
    jsr set_vx_dir
    jsr add_vel
    jsr on_ground
    bcs @bmc_sok
    lda ob_vxi,x
    eor #$ff
    clc
    adc #1
    sta ob_vxi,x
    jsr add_vel
    jsr rel_player
    lda ob_dir,x
    bne @bmc_el
    lda oz_dx+1
    bpl @bmc_sok
    lda #1
    sta ob_dir,x
    lda #$40
    sta ob_mirror,x
    bne @bmc_sok
@bmc_el:
    lda oz_dx+1
    bmi @bmc_sok
    lda #0
    sta ob_dir,x
    sta ob_mirror,x
@bmc_sok:
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
    lda @bm_rtbl,y
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
    lda #10
    sta ob_t0,x
    rts
@bmc_n2:
    cmp #3
    bne @bmc_s4
    dec ob_t0,x
    bne @bmc_rt
    lda #4
    sta ob_step,x
    lda #8
    sta ob_t0,x
    lda #<rf_5174
    ldy #>rf_5174
    jmp set_anim
@bmc_s4:
    dec ob_t0,x
    bne @bmc_rt
    jsr fire_horiz_shot         ; Big Man horiz rf_FCF2/FCFA (X+-16, 2.0 px/F)
    ldx oz_i
    lda #1
    sta ob_step,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jmp set_anim
@bmc_rt:
    rts
@bm_down:
    lda ob_step,x
    bne @bmd_n0
    jsr face_player
    lda #1
    sta ob_step,x
    lda #<rf_5188
    ldy #>rf_5188
    jmp set_anim
@bmd_n0:
    cmp #1
    bne @bmd_n1
    jsr @bm_yband
    bcc :+
    lda #2
    sta ob_state,x
    lda #0
    sta ob_step,x
    rts
:
    jsr @bm_below
    bcs :+
    jmp @bm_ewalk
:
    lda #2
    sta ob_step,x
    lda #30
    sta ob_t0,x
    rts
@bmd_n1:
    cmp #2
    bne @bmd_s3
    dec ob_t0,x
    lda ob_t0,x
    cmp #20
    bne :+
    jsr fire_bigman_down
    ldx oz_i
    rts
:
    cmp #0
    bne @bmd_rt
    lda #3
    sta ob_step,x
    lda #20
    sta ob_t0,x
    rts
@bmd_s3:
    dec ob_t0,x
    bne @bmd_rt
    lda #1
    sta ob_step,x
@bmd_rt:
    rts
@bm_yband:
    jsr rel_player
    lda oz_dy
    clc
    adc #45
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @bmy_n
    lda oz_t0
    cmp #56
    bcc @bmy_y
@bmy_n:
    clc
    rts
@bmy_y:
    sec
    rts
@bm_below:
    jsr rel_player
    lda oz_dy+1
    bmi @bmy_n
    lda #10
    jmp abs_dx_le
@bm_wtbl: .byte 190,100,163,230, 98,156,120,160,120
@bm_rtbl: .byte  45, 35, 20, 20, 42, 60, 35, 50, 28

; --- Enemy Projectiles (6-shot cap, arcade speeds/offsets, snd_se_efire) --------------
alloc_enemy_shot:
    lda oz_kind_cnt+ok_shot
    cmp #6
    bcc :+
    sec
    rts
:
    jsr alloc
    bcc :+
    sec
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
    lda ob_dir,y
    sta ob_dir,x
    lda #ofl_alive | ofl_touch
    sta ob_flags,x
    clc
    rts

; Arremer 16-dir 2.26 px/F aimed fireball (rf_FDD7 at Y+16)
fire_aimed_shot:
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    lda ob_yl,x
    clc
    adc #16
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    jsr dir_to_player
    ldy #0                      ; 2.26 px/F
    jsr load_vdir
    lda #<rf_FDD7
    ldy #>rf_FDD7
shot_finish_play:
    jsr set_anim
    lda #snd_se_efire
    jsr call_bank8_play_se
    ldx oz_i
    rts

; Woody Pig spear: 2.27 px/F horizontal (rf_FD12/19) OR straight down (rf_FD27)
fire_spear_shot:
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    jsr rel_player
    lda oz_dy+1
    bmi @sp_h
    bne @sp_cd
    lda oz_dy
    cmp #16
    bcc @sp_h
@sp_cd:
    lda #32
    jsr abs_dx_le
    bcc @sp_h
    lda #$fd                    ; -2.27 px/F straight down
    sta ob_vyi,x
    lda #$bb
    sta ob_vyf,x
    lda #<rf_FD27
    ldy #>rf_FD27
    jmp shot_finish_play
@sp_h:
    lda #$45                    ; 2.27 px/F horizontal
    ldy #2
    jsr set_vx_dir
    lda ob_dir,x
    bne @sp_l
    lda #<rf_FD12
    ldy #>rf_FD12
    jmp shot_finish_play
@sp_l:
    lda #<rf_FD19
    ldy #>rf_FD19
    jmp shot_finish_play

; Petit Devil throw (A=1 horiz 1.56 px/F, A=2 down 2.0 px/F; rf_FCE7 at Y-10)
fire_devil_shot:
    sta oz_t2
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    lda ob_yl,x
    sec
    sbc #10
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda oz_t2
    cmp #2
    beq @fd_dn
    lda #$90                    ; 1.56 px/F horiz
    ldy #1
    jsr set_vx_dir
    jmp @fd_an
@fd_dn:
    lda #$fe                    ; -2.0 px/F down
    sta ob_vyi,x
@fd_an:
    lda #<rf_FCE7
    ldy #>rf_FCE7
    jmp shot_finish_play

; Big Man downward shot (rf_FD02 at Y-8, -2.0 px/F)
fire_bigman_down:
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    lda ob_yl,x
    sec
    sbc #8
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda #$fe
    sta ob_vyi,x
    lda #<rf_FD02
    ldy #>rf_FD02
    jmp shot_finish_play

; Horizontal shots: Dragon (3.125 px/F at X+-10,Y-5), Unicorn (3.0 px/F at Y+8), Big Man (2.0 px/F at X+-16)
fire_horiz_shot:
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    ldy oz_i
    lda ob_kind,y
    cmp #ok_dragon
    beq @fh_drg
    cmp #ok_unicorn
    beq @fh_uni
    ; Big Man: X +- 16, 2.0 px/F
    lda ob_dir,x
    bne @fh_bml
    lda ob_xl,x
    clc
    adc #16
    sta ob_xl,x
    bcc :+
    inc ob_xh,x
:
    lda #0
    ldy #2
    jsr set_vx_dir
    lda #<rf_FCF2
    ldy #>rf_FCF2
    jmp shot_finish_play
@fh_bml:
    lda ob_xl,x
    sec
    sbc #16
    sta ob_xl,x
    bcs :+
    dec ob_xh,x
:
    lda #0
    ldy #2
    jsr set_vx_dir
    lda #<rf_FCFA
    ldy #>rf_FCFA
    jmp shot_finish_play
@fh_uni:
    ; Unicorn: Y + 8, 3.0 px/F
    lda ob_yl,x
    clc
    adc #8
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    lda #0
    ldy #3
    jsr set_vx_dir
    lda ob_dir,x
    bne @fh_ul
    lda #<rf_FDAB
    ldy #>rf_FDAB
    jmp shot_finish_play
@fh_ul:
    lda #<rf_FDB3
    ldy #>rf_FDB3
    jmp shot_finish_play
@fh_drg:
    ; Dragon: X +- 10, Y - 5, 3.125 px/F
    lda ob_dir,x
    bne @fh_dl
    lda ob_xl,x
    clc
    adc #10
    sta ob_xl,x
    bcc :+
    inc ob_xh,x
:
    jmp @fh_dy
@fh_dl:
    lda ob_xl,x
    sec
    sbc #10
    sta ob_xl,x
    bcs :+
    dec ob_xh,x
:
@fh_dy:
    lda ob_yl,x
    sec
    sbc #5
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
    lda #$20                    ; 3.125 px/F
    ldy #3
    jsr set_vx_dir
    lda #<rf_FD9C
    ldy #>rf_FD9C
    jmp shot_finish_play

; Satan 16-dir 2.26 px/F aimed fireball (rf_FDCF)
fire_satan_shot:
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    jsr dir_to_player
    ldy #0                      ; 2.26 px/F
    jsr load_vdir
    lda #<rf_FDCF
    ldy #>rf_FDCF
    jmp shot_finish_play

; Astaroth flame (A=0 normal distance rule: |dx|>=80 -> 75% high else 100% low; A=1 rapid 50/50)
; High offset: (X+-10, Y+12). Low offset: (X+-4, Y-5).
fire_astaroth_shot:
    sta oz_t2
    jsr alloc_enemy_shot
    bcc :+
    rts
:
    lda #1                      ; var 1 = Astaroth flame (vanishes at floor Y <= 62)
    sta ob_var,x
    lda oz_t2
    bne @fas_rapid
    ; Normal mode: |dx| < 80 -> 100% low; |dx| >= 80 -> 75% high
    jsr rel_player
    lda #79
    jsr abs_dx_le
    bcs @fas_lo
    lda oz_rng
    and #3
    beq @fas_lo
    bne @fas_hi
@fas_rapid:
    lda oz_rng
    and #1
    beq @fas_lo
@fas_hi:
    lda #10                     ; X +- 10
    jsr @fas_offx
    lda ob_yl,x
    clc
    adc #12
    sta ob_yl,x
    bcc :+
    inc ob_yh,x
:
    jmp @fas_vel
@fas_lo:
    lda #4                      ; X +- 4
    jsr @fas_offx
    lda ob_yl,x
    sec
    sbc #5
    sta ob_yl,x
    bcs :+
    dec ob_yh,x
:
@fas_vel:
    lda #$70                    ; 1.44 px/F horizontal
    ldy #1
    jsr set_vx_dir
    lda #$ff
    sta ob_vyi,x
    lda #$68                    ; vy = -0.59 px/F
    sta ob_vyf,x
    lda ob_dir,x
    bne @fas_la
    lda #<rf_FDBF
    ldy #>rf_FDBF
    jmp shot_finish_play
@fas_la:
    lda #<rf_FDC7
    ldy #>rf_FDC7
    jmp shot_finish_play
@fas_offx:
    sta oz_t0
    lda ob_dir,x
    bne @fax_l
    lda ob_xl,x
    clc
    adc oz_t0
    sta ob_xl,x
    bcc :+
    inc ob_xh,x
:
    rts
@fax_l:
    lda ob_xl,x
    sec
    sbc oz_t0
    sta ob_xl,x
    bcs :+
    dec ob_xh,x
:
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

; u_boom var: 0=free, 1=500(112F), 2=1000(63F), 3=2000(63F), 4=Satan Boss Key, 5=Astaroth Clear!
u_boom:
    dec ob_t0,x
    bne @ub_ret
    lda ob_var,x
    beq @ub_free
    dec oz_kind_cnt+ok_boom
    lda #ok_popup
    sta ob_kind,x
    inc oz_kind_cnt+ok_popup
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
    jmp free_obj
@ub_ret:
    rts

u_popup:
    dec ob_t0,x
    bne @up_ret
    lda ob_var,x
    cmp #4
    beq @up_key
    cmp #5
    beq @up_ast
    jmp free_obj
@up_key:
    ; Satan Boss Key drop (matches Stage 1/2: 30F rf_7422 sparkle + snd_song_key -> -1.24 px/F fall)
    dec oz_kind_cnt+ok_popup
    lda #ok_key
    sta ob_kind,x
    lda #0
    sta ob_state,x
    sta ob_vxi,x
    sta ob_vxf,x
    sta ob_vyi,x
    sta ob_vyf,x
    sta ob_yf,x
    sta ob_mirror,x
    lda #ofl_alive
    sta ob_flags,x
    ; Arcade $73EF-$7421: fixed per-stage key X = Xmax+37 (S5: 549=$0225, S6: 293=$0125),
    ; spawn Y = 1168 ($0490, FC screen Y=32 below HUD), landing floor Y = 1104 ($0450).
    lda #$25
    sta ob_xl,x
    lda #$02                    ; Stage 5: 512 + 37 = 549 ($0225)
    ldy campaign_stage
    cpy #6
    bne :+
    lda #$01                    ; Stage 6: 256 + 37 = 293 ($0125)
:
    sta ob_xh,x
    lda #$50                    ; top boss floor Y = 1104 ($0450)
    sta ob_t1,x
    lda #$04
    sta ob_t2,x
    lda #$90                    ; spawn Y = 1168 ($0490, screen Y=32)
    sta ob_yl,x
    lda #$04
    sta ob_yh,x
    lda #30
    sta ob_t0,x
    lda #<rf_7422
    ldy #>rf_7422
    jsr set_anim
    lda #snd_song_key
    jmp call_bank8_play_song
@up_ast:
    jmp start_clear_seq
@up_ret:
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
    lda ob_state,x
    bne @ky_not0
    ; State 0: 30-frame arcade sparkle (rf_7422) high in the air
    dec ob_t0,x
    beq :+
    rts
:
    lda #1
    sta ob_state,x
    lda #$c3                    ; -1.24 px/F ($FE.$C3) arcade key descent
    sta ob_vyf,x
    lda #$fe
    sta ob_vyi,x
    lda #<rf_7430
    ldy #>rf_7430
    jmp set_anim
@ky_not0:
    cmp #1
    bne @ky_wait
    ; State 1: descending until ob_y <= target floor (ob_t2:ob_t1)
    lda ob_yf,x
    clc
    adc ob_vyf,x
    sta ob_yf,x
    lda ob_yl,x
    adc ob_vyi,x
    sta ob_yl,x
    lda ob_yh,x
    adc #$ff
    sta ob_yh,x
    cmp ob_t2,x
    bcc @ky_land
    bne @ky_ret
    lda ob_yl,x
    cmp ob_t1,x
    beq @ky_land
    bcs @ky_ret
@ky_land:
    lda ob_t1,x
    sta ob_yl,x
    lda ob_t2,x
    sta ob_yh,x
    lda #0
    sta ob_vyf,x
    sta ob_vyi,x
    lda #2
    sta ob_state,x
    rts
@ky_wait:
    ; State 2: Wait for player pickup
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
    lda #50                     ; arcade $7345: $32 frames walk to gate (step 2 plays $3B)
    sta ap_clear_timer
    lda #5
    jmp ap_add_score

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
    ; FIXED Y-sign inversion: compute enemy_Y - shot_Y (matching Stages 1-4 & ok_sylo/ok_syhi)
    ldy oz_i
    sec
    lda ob_yl,y
    sbc oz_sy
    sta oz_t0
    lda ob_yh,y
    sbc oz_sy+1
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
    ; Astaroth ($DD01): body X+-32 Y+-32 reflects with clank; face X+-10 Y-9..+9 takes Cross only!
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
    lda ob_yl,y
    sbc oz_sy
    sta oz_t0
    lda ob_yh,y
    sbc oz_sy+1
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
    ; Inside body! Outside face Y (-9..+9 -> offset 23..41), body reflects with clank.
    cmp #23
    bcc @as_reflect
    cmp #42
    bcs @as_reflect
    ; At face height: outside face X (+-10) passes through until reaching face!
    ldx oz_si
    lda ps_x_lo,x
    ldy oz_i
    sec
    sbc ob_xl,y
    clc
    adc #10
    cmp #21
    bcs @sh_nx_j
    ; Face hit! Only Cross (ap_weapon == ap_weapon_shield) damages Astaroth; non-Cross vanishes ($0203)
    lda ap_weapon
    cmp #ap_weapon_shield
    beq @sh_hit
    ldx oz_si
    sec
    rts
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
    bne @sh_chk_spec
    lda ob_var,x
    beq @sh_chk_spec
    jmp @dragon_body_hit

@sh_chk_spec:
    ; 1) Sleeping Red Arremer wakes freely with HP reset to 2 (3 hits)
    cmp #ok_arremer
    bne @sh_chk_sat
    lda ob_state,x
    bne @sh_norm
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
    jmp @sh_clank
@sh_chk_sat:
    ; 2) Dormant Stage 6 Satan Variant 1: never killed while dormant; at HP=1 wakes early with HP=4!
    cmp #ok_satan
    bne @sh_norm
    lda ob_state,x
    bne @sh_norm
    lda ob_hp,x
    cmp #2
    bcs @sh_norm                ; still above HP=1 -> decrement normally
    ; Hit down to HP=1 -> wake immediately with HP reset to 4 (ob_hp=3)!
    lda #1
    sta ob_state,x
    lda #3
    sta ob_hp,x
    lda #180
    sta ob_t0,x
    lda #60
    sta ob_t1,x
    lda #0
    sta ob_aux,x
    sta ob_t2,x
    lda #<rf_4E2B
    ldy #>rf_4E2B
    jsr set_anim
    jmp @sh_clank

@sh_norm:
    lda ob_hp,x
    beq @sh_kill
    dec ob_hp,x
    lda #5
    sta ob_flash,x
    ; Big Man 6F hit-stun ($0103, rf_51B4)
    lda ob_kind,x
    cmp #ok_bigman
    bne @sh_chk_uni_wake
    lda #4
    sta ob_state,x
    lda #6
    sta ob_t0,x
    lda #<rf_51B4
    ldy #>rf_51B4
    jsr set_anim
    jmp @sh_clank
@sh_chk_uni_wake:
    ; Unicorn 4-hit early wake while waking (ob_state==0, ob_hp <= 5)
    cmp #ok_unicorn
    bne @sh_clank
    lda ob_state,x
    bne @sh_clank
    lda ob_hp,x
    cmp #6
    bcs @sh_clank
    lda #1
    sta ob_state,x
    sta ob_step,x
    lda #20
    sta ob_t0,x
    lda #51
    sta ob_t1,x
    lda #22
    sta ob_t2,x
@sh_clank:
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
    bne :+
    jmp @kill_astaroth
:
    cmp #ok_dragon
    bne :+
    jmp @kill_dragon_head
:
    cmp #ok_bigman
    beq @kill_bigman
    cmp #ok_arremer
    beq @kill_arremer
    cmp #ok_unicorn
    beq @kill_unicorn
    ; Normal enemy death
    jsr make_boom
    jmp @sh_kill_drop
@kill_bigman:
    jsr make_boom
    lda #36
    sta ob_t0,x
    lda #1                      ; 500 popup (112F)
    sta ob_var,x
    lda #<rf_5CA0
    ldy #>rf_5CA0
    jsr set_anim
    jmp @sh_kill_drop
@kill_arremer:
    jsr make_boom
    lda #36
    sta ob_t0,x
    lda #2                      ; 1000 popup (63F)
    sta ob_var,x
    lda #<rf_5CA0
    ldy #>rf_5CA0
    jsr set_anim
    jmp @sh_kill_drop
@kill_unicorn:
    jsr make_boom
    lda #22
    sta ob_t0,x
    lda #3                      ; 2000 popup (63F)
    sta ob_var,x
    lda #<rf_5C7A
    ldy #>rf_5C7A
    jsr set_anim
@sh_kill_drop:
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
    lda #22                     ; 22F boss explosion (rf_5C7A)
    sta ob_t0,x
    lda #3                      ; 2000 popup (or 4 = Boss Key if last Satan!)
    dec s567_boss_rem
    bne :+
    lda #4
:
    sta ob_var,x
    lda #<rf_5C7A
    ldy #>rf_5C7A
    jsr set_anim
    lda #snd_se_boss_die
    jsr call_bank8_play_se
    jmp @sh_done

@kill_astaroth:
    ; 22F boss explosion (rf_5C7A) -> 63F popup -> Stage 7 clear sequence!
    lda #0
    sta s567_boss_rem
    jsr make_boom
    lda #22
    sta ob_t0,x
    lda #5                      ; var 5 = start_clear_seq after 63F popup
    sta ob_var,x
    lda #<rf_5C7A
    ldy #>rf_5C7A
    jsr set_anim
    lda #snd_se_boss_die
    jsr call_bank8_play_se
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
