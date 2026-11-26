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
ok_pylo:   .byte 0,$F6,$F8,$EF,$F8,$F2,$ED,$ED,$E2,$FA, 0,  0,  0,  0,  0,  0
ok_pyhi:   .byte 0,10,  8, 17,  8, 14, 19, 19, 30,  6,  0,  0,  0,  0,  0,  0

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

; C set if object is offscreen (far outside camera X).
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
    adc #80
    lda oz_t1
    adc #0
    beq @in
    cmp #1
    bne @out
    lda oz_t0
    clc
    adc #80
    cmp #160                    ; total window 416 px
    bcc @in
@out:
    sec
    rts
@in:
    clc
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

; Spawn an aimed or straight enemy shot from object X.
; A = mode (0 = aimed toward player, 1 = horizontal toward ob_dir, 2 = straight down).
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
    lda #<rf_FD02
    ldy #>rf_FD02
    jsr set_anim
    lda oz_t2
    cmp #1
    beq @horiz
    cmp #2
    beq @down
    ; Aimed toward player (~1.75 px/F)
    jsr rel_player
    lda #$c0
    sta ob_vxf,x
    lda #1
    ldy oz_dx+1
    bpl :+
    lda #$40
    sta ob_vxf,x
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vyf,x
    lda oz_dy+1
    bmi @aim_up
    lda #$ff
    sta ob_vyi,x
    jmp @done
@aim_up:
    lda #1
    sta ob_vyi,x
    jmp @done
@horiz:
    ldy oz_t3
    lda ob_dir,y
    bne @h_left
    lda #2
    sta ob_vxi,x
    jmp @done
@h_left:
    lda #$fe
    sta ob_vxi,x
    jmp @done
@down:
    lda #$fe
    sta ob_vyi,x
@done:
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

test_fix_bit:
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    sta oz_t0
    txa
    pha
    ldx oz_t0
    lda bit_mask,x
    sta oz_t0
    pla
    tax
    lda s2_fix_used,x
    and oz_t0
    rts

set_fix_bit:
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    sta oz_t0
    txa
    pha
    ldx oz_t0
    lda bit_mask,x
    sta oz_t0
    pla
    tax
    lda s2_fix_used,x
    ora oz_t0
    sta s2_fix_used,x
    rts

test_sup_bit:
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    sta oz_t0
    txa
    pha
    ldx oz_t0
    lda bit_mask,x
    sta oz_t0
    pla
    tax
    lda s2_sup_used,x
    and oz_t0
    rts

set_sup_bit:
    tya
    lsr a
    lsr a
    lsr a
    tax
    tya
    and #7
    sta oz_t0
    txa
    pha
    ldx oz_t0
    lda bit_mask,x
    sta oz_t0
    pla
    tax
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
    ; Offset = page_idx * 9
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

    ; Check 2x Unicorn boss defeated -> key spawn trigger at player_x >= 3280 ($0CD0)
    lda s2_boss_rem
    bne @no_key
    lda oz_boss_seen
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
    ; ok_pylo[y] <= oz_dy <= ok_pyhi[y]
    lda oz_dy
    sec
    sbc ok_pylo,y
    sta oz_t0
    lda oz_dy+1
    sbc #$ff
    bne @no
    lda ok_pyhi,y
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
    ; Big Man enters 6F hit-stun ($0103) with rf_51B0
    lda ob_kind,x
    cmp #ok_bigman
    bne @hit_se
    lda #4
    sta ob_state,x
    lda #6
    sta ob_t0,x
    lda #<rf_51B0
    ldy #>rf_51B0
    jsr set_anim
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
    lda ok_points,y
    beq @no_pts
    txa
    pha
    jsr ap_add_score
    pla
    tax
@no_pts:
    ; Check if this was one of the 2 Unicorn bosses
    lda ob_kind,x
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
    ; Turn into explosion
    ldy ob_kind,x
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
    lda #18
    sta ob_t0,x
    lda #<rf_E878
    ldy #>rf_E878
    jsr set_anim
    txa
    pha
    lda #snd_se_hit
    jsr call_bank8_play_se
    pla
    tax
    rts

; --- Enemy AI 1: Petit Devil? ($02, var 0) --------------------------------------------
; Picks a visible tower window from the 26-window table ($ADF3), emerges invuln for 31F,
; then executes 3 swoop/arc passes toward Arthur before flying off.
u_devil:
    lda ob_state,x
    bne @active
    ; State 0: pick a visible window
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
    lda #3                      ; 3 swoop passes
    sta ob_aux,x
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

@active:
    cmp #1
    bne @swoop
    ; State 1: emerging slowly from window (invuln)
    dec ob_t0,x
    bne @emerge_move
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #2
    sta ob_state,x
    lda #70
    sta ob_t0,x
    jsr @aim_vel
    rts
@emerge_move:
    lda #$10
    sta ob_vxf,x
    lda #0
    ldy ob_dir,x
    beq :+
    lda #$f0
    sta ob_vxf,x
    lda #$ff
:
    sta ob_vxi,x
    jmp move

@swoop:
    cmp #2
    bne @leave
    jsr move
    ; Floor clamp Y >= 54 ($A7FF)
    lda ob_yh,x
    bne :+
    lda ob_yl,x
    cmp #54
    bcs :+
    lda #54
    sta ob_yl,x
    lda #1
    sta ob_vyi,x
:
    dec ob_t0,x
    bne @chk_off
    dec ob_aux,x
    beq @start_leave
    lda #75
    sta ob_t0,x
    jsr @aim_vel
    jmp @chk_off
@start_leave:
    lda #3
    sta ob_state,x
    lda #$a0
    sta ob_vxf,x
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$60
    sta ob_vxf,x
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
@leave:
    jsr move
@chk_off:
    jsr offscreen
    bcc :+
    jmp free
:
    rts

@aim_vel:
    jsr face_player
    lda #$a0
    sta ob_vxf,x
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$60
    sta ob_vxf,x
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vyf,x
    lda oz_dy+1
    bmi @go_up
    lda #$ff
    sta ob_vyi,x
    rts
@go_up:
    lda #1
    sta ob_vyi,x
    rts

; --- Enemy AI 2: Purple Flying Enemy ($04, var 1) -------------------------------------
; Emerges from building window (30F invuln), swoops down to floor, walks/flies,
; dodges shots, and transforms into Red Arremer ($05) if on-screen long enough.
u_flyer:
    lda ob_state,x
    bne @not0
    lda #<rf_B19E
    ldy #>rf_B19E
    jsr set_anim
    jsr face_player
    lda #1
    sta ob_state,x
    lda #30                     ; 30F emerge invuln
    sta ob_t0,x
    lda #14                     ; ~3,584F transform countdown (14 * 256)
    sta ob_aux,x
    lda #0
    sta ob_aux2,x
    rts
@not0:
    ; Transform countdown while active ($B0BF)
    dec ob_aux2,x
    bne @no_xform
    dec ob_aux,x
    bne @no_xform
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
@no_xform:
    lda ob_state,x
    cmp #1
    bne @not1
    ; Emerging
    dec ob_t0,x
    bne @ret
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #2
    sta ob_state,x
    jsr face_player
    lda #$49
    sta ob_vyf,x
    lda #$fe                    ; -1.71 px/F descent
    sta ob_vyi,x
    lda #$80
    sta ob_vxf,x
    lda #0
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
@ret:
    rts
@not1:
    cmp #2
    bne @walk
    ; Swooping down until ground
    jsr move
    jsr on_ground
    bcc @chk_f_off
    jsr snap_ground
    lda #3
    sta ob_state,x
    lda #<rf_B10B
    ldy #>rf_B10B
    jsr set_anim
    lda #80
    sta ob_t0,x
    jsr face_player
    rts
@walk:
    cmp #3
    bne @fly
    ; Walking on floor at 1.43 px/F
    lda #$6e
    sta ob_vxf,x
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$92
    sta ob_vxf,x
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jsr move
    jsr on_ground
    bcc @takeoff
    dec ob_t0,x
    bne @chk_f_off
@takeoff:
    lda #4
    sta ob_state,x
    lda #<rf_B19E
    ldy #>rf_B19E
    jsr set_anim
    lda #50
    sta ob_t0,x
    lda #1
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    rts
@fly:
    jsr move
    dec ob_t0,x
    bne @chk_f_off
    lda #2
    sta ob_state,x
    lda #$49
    sta ob_vyf,x
    lda #$fe
    sta ob_vyi,x
@chk_f_off:
    jsr offscreen
    bcc :+
    jmp free
:
    rts

; --- Enemy AI 3: Big Man ($0A, var 0, HP 10) ------------------------------------------
; Patrols floor (0.53 px/F), charges & shoots horizontally when player is at same Y,
; shoots downward when player is directly below, 6F hit-stun on non-lethal hit.
u_bigman:
    lda ob_state,x
    bne @bm_active
    ; State 0: wait until on-screen
    jsr on_screen_x
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
    lda #90
    sta ob_t0,x
    rts
@bm_active:
    cmp #4
    bne @bm_not_stun
    ; State 4: 6F hit-stun ($0103)
    dec ob_t0,x
    bne @bm_ret
    lda #1
    sta ob_state,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
@bm_ret:
    rts
@bm_not_stun:
    jsr offscreen
    bcc :+
    jmp free
:
    jsr on_screen_x
    bcc @bm_ret                 ; Off-screen: pause
    lda ob_state,x
    cmp #1
    beq :+
    jmp @bm_not_patrol
:
    ; State 1: patrol at 0.53 px/F
    jsr rel_player
    ; Check same height: player_y - ob_y in [-10..+45] => ob_y - player_y in [-45..+10]
    lda oz_dy
    clc
    adc #45
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @bm_chk_below
    lda oz_t0
    cmp #56
    bcs @bm_chk_below
    ; Enter State 2 (charge & horizontal shot)
    lda #2
    sta ob_state,x
    lda #40
    sta ob_t0,x
    jsr face_player
    rts
@bm_chk_below:
    ; Check player directly below: |oz_dx| <= 12 and oz_dy > 0
    lda oz_dy+1
    bmi @bm_walk
    lda oz_dx
    clc
    adc #12
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @bm_walk
    lda oz_t0
    cmp #25
    bcs @bm_walk
    ; Enter State 3 (downward shot)
    lda #3
    sta ob_state,x
    lda #30
    sta ob_t0,x
    lda #<rf_5184
    ldy #>rf_5184
    jsr set_anim
    rts
@bm_walk:
    lda #$88                    ; 0.53 px/F
    sta ob_vxf,x
    lda #0
    ldy ob_dir,x
    beq :+
    lda #$78
    sta ob_vxf,x
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    jsr move
    ; Turn at ledge edge (check every 8 frames)
    lda frames
    and #7
    bne @bm_ret2
    jsr on_ground
    bcs @bm_ret2
    lda ob_dir,x
    eor #1
    sta ob_dir,x
    lda ob_mirror,x
    eor #$40
    sta ob_mirror,x
@bm_ret2:
    rts

@bm_not_patrol:
    cmp #2
    bne @bm_down
    ; State 2: charge (1.0 px/F) then pause & shoot horizontally
    dec ob_t0,x
    beq @bm_fire_h
    lda ob_t0,x
    cmp #18
    bcc @bm_ret3                ; Pausing before shot
    bne :+
    lda #<rf_5170
    ldy #>rf_5170
    jsr set_anim
    rts
:
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    jsr move
    jsr on_ground
    bcs @bm_ret3
    ; Undo step at ledge edge
    lda ob_vxi,x
    eor #$ff
    clc
    adc #1
    sta ob_vxi,x
    jsr move
@bm_ret3:
    rts
@bm_fire_h:
    lda #1                      ; horizontal shot
    jsr fire_shot
    lda #1
    sta ob_state,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
    rts

@bm_down:
    ; State 3: downward shot every ~50F
    dec ob_t0,x
    lda ob_t0,x
    cmp #20
    bne :+
    lda #2                      ; straight down shot
    jsr fire_shot
    rts
:
    cmp #0
    bne @bm_ret4
    lda #1
    sta ob_state,x
    lda #<rf_50E4
    ldy #>rf_50E4
    jsr set_anim
@bm_ret4:
    rts

; --- Shared Stage 2 Enemies: Red Bird, Plant, Zombie, Red Arremer ---------------------
u_crow:
    lda ob_state,x
    bne @fly
    lda #<rf_4A04
    ldy #>rf_4A04
    jsr set_anim
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr rel_player
    lda oz_dx+1
    bpl :+
    eor #$ff
:
    bne @ret
    lda #1
    sta ob_state,x
    lda #<rf_4A42
    ldy #>rf_4A42
    jsr set_anim
    jsr face_player
@ret:
    rts
@fly:
    lda #$80
    sta ob_vxf,x
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    inc ob_t0,x
    lda ob_t0,x
    and #$10
    beq :+
    lda #$ff
    bne :++
:
    lda #0
:
    sta ob_vyi,x
    jsr move
    jsr offscreen
    bcc :+
    jmp free
:
    rts

u_plant:
    lda ob_state,x
    bne @act
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    jsr face_player
    lda ob_dir,x
    beq :+
    lda #<rf_56E5               ; Plant facing left (arcade $56E5)
    ldy #>rf_56E5
    bne :++
:
    lda #<rf_56D7               ; Plant facing right (arcade $56D7)
    ldy #>rf_56D7
:
    jsr set_anim
    lda #80
    sta ob_t0,x
    rts
@act:
    jsr offscreen
    bcc :+
    jmp free
:
    jsr on_screen_x
    bcc @ret
    dec ob_t0,x
    bne @ret
    lda #90
    sta ob_t0,x
    lda #0                      ; aimed shot
    jsr fire_shot
@ret:
    rts

u_zombie:
    lda ob_state,x
    bne @walk
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #<rf_5245
    ldy #>rf_5245
    jsr set_anim
    jsr face_player
    rts
@walk:
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    sta ob_vyi,x
    sta ob_vyf,x
    jsr move
    jsr offscreen
    bcc :+
    jmp free
:
    rts

; --- Stage 2 Red Arremer ($05): Arcade perch/climb/hover/dodge/swoop AI ---------------
; States: 0=init/wake, 1=hover above Arthur (+64..80px) & dodge shots, 2=swoop dive & fire, 3=pull up.
u_arremer:
    lda ob_state,x
    bne @ar_not0
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda #<rf_4BAC
    ldy #>rf_4BAC
    jsr set_anim
    lda #40
    sta ob_t0,x
    rts
@ar_not0:
    cmp #1
    beq @ar_hover
    cmp #2
    bne :+
    jmp @ar_swoop
:
    ; State 3: pull up back to hover altitude (+72 px above player)
    jsr face_player
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    lda #2
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    jsr move
    dec ob_t0,x
    beq :+
    rts
:
    lda #1
    sta ob_state,x
    lda #36
    sta ob_t0,x
    rts
@ar_hover:
    jsr face_player
    ; Maintain ~72 px above Arthur, pace horizontally
    jsr rel_player
    lda #0
    sta ob_vyi,x
    sta ob_vyf,x
    lda oz_dy+1
    bmi @ar_up
    bne @ar_dn
    lda oz_dy
    cmp #60
    bcc @ar_up
    cmp #84
    bcs @ar_dn
    jmp @ar_hx
@ar_up:
    lda #1
    sta ob_vyi,x
    bne @ar_hx
@ar_dn:
    lda #$ff
    sta ob_vyi,x
@ar_hx:
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    ; Dodge incoming player shots by hopping up
    ldy #ap_max_shots - 1
@ar_chk_sh:
    lda ps_active,y
    bne @ar_dodge
    dey
    bpl @ar_chk_sh
    bmi @ar_no_dodge
@ar_dodge:
    lda #2
    sta ob_vyi,x
@ar_no_dodge:
    jsr move
    dec ob_t0,x
    bne @ar_ret
    ; Enter State 2: steep swoop dive toward Arthur!
    lda #2
    sta ob_state,x
    lda #26
    sta ob_t0,x
    jsr face_player
    lda #2
    ldy ob_dir,x
    beq :+
    lda #$fe
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    lda #$fe                    ; -2 px/F dive
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    lda #0                      ; aimed fireball at dive start
    jsr fire_shot
    rts
@ar_swoop:
    jsr move
    dec ob_t0,x
    bne @ar_ret
    ; Pull up (State 3)
    lda #3
    sta ob_state,x
    lda #24
    sta ob_t0,x
@ar_ret:
    rts

; --- Boss: 2x Unicorn ($08, HP 10 each, Arcade walk/pause/aim/jump AI) ----------------
; States: 0=init, 1=walk/pause spacing, 2=aiming walk (36F), 3=aimed leap (gravity 0.25)
u_unicorn:
    lda ob_state,x
    bne @u_not0
    lda #1
    sta ob_state,x
    lda #ofl_shootable | ofl_harmful
    sta ob_flags,x
    lda ob_yl,x
    sta ob_by_l,x               ; Remember ground Y (80)
    lda ob_yh,x
    sta ob_by_h,x
    lda #<rf_4FAB
    ldy #>rf_4FAB
    jsr set_anim
    jsr face_player
    ; Stagger timers between the 2 Unicorns using slot index
    txa
    asl a
    asl a
    clc
    adc #24
    sta ob_t0,x
    lda #32
    sta ob_t1,x
    ; Start boss BGM when first Unicorn spawns
    lda ap_boss_lock
    bne @u_ret
    lda #1
    sta ap_boss_lock
    txa
    pha
    lda #snd_song_boss
    jsr call_bank8_play_song
    pla
    tax
@u_ret:
    rts
@u_not0:
    cmp #3
    bne :+
    jmp @u_jump
:
    cmp #2
    beq @u_aim
    ; State 1: walk & pause spacing
    jsr face_player
    dec ob_t0,x
    bne @u_walk_step
    lda #2
    sta ob_state,x
    lda #36
    sta ob_t0,x
    lda #1                      ; horizontal fireball
    jsr fire_shot
    rts
@u_walk_step:
    lda ob_t0,x
    and #$10
    beq @u_ret                  ; Brief pause half the cycle (arcade pacing)
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    sta ob_vyi,x
    sta ob_vyf,x
    jsr @u_clamp_move
    rts
@u_aim:
    ; State 2: aiming walk toward Arthur then launch distance-scaled leap
    jsr face_player
    lda #1
    ldy ob_dir,x
    beq :+
    lda #$ff
:
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    sta ob_vyi,x
    sta ob_vyf,x
    jsr @u_clamp_move
    dec ob_t0,x
    bne @u_ret
    ; Launch aimed jump: vy = +5.0 px/F, vx = +-1.75 px/F toward Arthur
    lda #3
    sta ob_state,x
    lda #5
    sta ob_vyi,x
    lda #0
    sta ob_vyf,x
    lda #1
    sta ob_vxi,x
    lda #$c0
    sta ob_vxf,x
    lda ob_dir,x
    beq :+
    lda #$fe
    sta ob_vxi,x
    lda #$40
    sta ob_vxf,x
:
    rts
@u_jump:
    ; State 3: airborne arc with gravity -0.25 px/F^2
    jsr @u_clamp_move
    lda ob_vyf,x
    sec
    sbc #$40
    sta ob_vyf,x
    lda ob_vyi,x
    sbc #0
    sta ob_vyi,x
    ; Land when Y <= ob_by (ground Y)
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
    lda #1
    sta ob_state,x
    jsr rng
    and #$1f
    clc
    adc #20
    sta ob_t0,x
    rts
@u_clamp_move:
    ; Keep within arena [3340, 3556]
    lda ob_xh,x
    cmp #>3340
    bcc @push_r
    bne :+
    lda ob_xl,x
    cmp #<3340
    bcc @push_r
:
    lda ob_xh,x
    cmp #>3556
    bcc @do_mv
    bne @push_l
    lda ob_xl,x
    cmp #<3556
    bcc @do_mv
@push_l:
    lda #$ff
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
    jmp @do_mv
@push_r:
    lda #1
    sta ob_vxi,x
    lda #0
    sta ob_vxf,x
@do_mv:
    jmp move

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
    bne :+
    jmp free
:
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
    jsr rel_player
    lda oz_dx
    clc
    adc #14
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #29
    bcs @ret
    lda oz_dy
    clc
    adc #16
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #33
    bcs @ret
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
    jsr rel_player
    lda oz_dx
    clc
    adc #16
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #33
    bcs @ret
    lda oz_dy
    clc
    adc #20
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #41
    bcs @ret
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
    jsr rel_player
    lda oz_dx
    clc
    adc #16
    sta oz_t0
    lda oz_dx+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #33
    bcs @ret
    lda oz_dy
    clc
    adc #24
    sta oz_t0
    lda oz_dy+1
    adc #0
    bne @ret
    lda oz_t0
    cmp #49
    bcs @ret
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
