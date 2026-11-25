; The opening demo of the game build: after START on the title the arcade shows Arthur and
; the princess in the graveyard, fades the background out, and a demon carries her off
; (docs/revision-g-sprites.md, "タイトルとオープニング"). The FC plays the frames that
; tools/convert-revg-frontend.py recorded from MAME (revg_opening_track, in the frontend
; bank) over Stage 1 at camera X 0, then starts Stage 1.
; demo_active and demo_finish are in vrc7a-test.s (cleared at reset).

revg_frontend_bank = 19 + revg_bg_bank_count + revg_spr_bank_count
op_objects = 4                  ; Arthur, the princess, the demon, the armour

.segment "WRAM"
op_ptr: .res 2                  ; the next record in revg_opening_track
op_count: .res 1                ; frames left of the current record
op_fade: .res 1                 ; the fade level on screen (0 full - 3 black)
op_fid_lo: .res op_objects      ; frame id, 0 when hidden
op_fid_hi: .res op_objects
op_x: .res op_objects
op_y: .res op_objects
op_last_lo: .res op_objects     ; the last frame drawn whole (rs_draw_or_last)
op_last_hi: .res op_objects
op_time: .res 2                 ; frames since op_start
op_stage_song: .res 1           ; 1 once the opening started the stage song (cleared at reset)
op_blackout: .res 1             ; post-cutscene arcade blackout frames before Stage 1 begins

.segment "BANK3"
; Start the track and upload the ':' HUD glyph (trampoline to BANK7).
op_start:
    lda prg_bank2
    pha
    lda #7
    jsr select_bank2
    jsr op_start_b7
    pla
    jmp select_bank2

; Once a frame instead of the play update: trampoline to BANK7.
op_tick:
    lda prg_bank2
    pha
    lda #7
    jsr select_bank2
    jsr op_tick_b7
    pla
    jmp select_bank2

.segment "CODE"
; Trampoline into BANK7 so FIXED keeps headroom.
op_finish_to_play:
    lda prg_bank2
    pha
    lda #7
    jsr select_bank2
    jsr op_finish_to_play_b7
    pla
    jmp select_bank2

.segment "BANK7"
op_start_b7:
    lda #<revg_opening_track
    sta op_ptr
    lda #>revg_opening_track
    sta op_ptr+1
    lda #0
    sta op_count
    sta op_fade
    sta op_time
    sta op_time+1
    sta op_stage_song
    sta op_blackout
    ldx #op_objects - 1
:
    sta op_fid_lo,x
    sta op_fid_hi,x
    sta op_last_lo,x
    sta op_last_hi,x
    dex
    bpl :-
    ; Replace tile $17 ('+') in CHR-RAM with ':' for the arcade M:SS HUD timer.
    lda #$01
    sta $2006
    lda #$70
    sta $2006
    ldx #0
:
    lda op_colon_tile,x
    sta $2007
    inx
    cpx #16
    bne :-
    rts
op_colon_tile:
    .byte $00, $00, $18, $18, $00, $18, $18, $00
    .res 8, 0
op_tick_b7:
    lda op_blackout
    beq @not_blackout
    dec op_blackout
    bne @blackout_wait
    lda #1
    sta demo_finish
@blackout_wait:
    rts
@not_blackout:
    ; End the cutscene at frame 515 (right after Arthur finishes walking forward to X=129
    ; and pauses), before revg_opening_track's final 39F tail snaps him back to X=96.
    lda op_time+1
    cmp #>515
    bcc @run_track
    bne @enter_blackout
    lda op_time
    cmp #<515
    bcs @enter_blackout
@run_track:
    jsr op_sound_b7
    lda op_count
    beq :+
    dec op_count
    rts
:
    lda prg_bank0
    pha
    lda #revg_frontend_bank
    jsr select_bank0
    lda op_ptr
    sta rs_lptr                 ; (a zero-page pointer the sprite drawing uses later)
    lda op_ptr+1
    sta rs_lptr+1
    ldy #0
    lda (rs_lptr),y
    bne :+
    pla
    jsr select_bank0
@enter_blackout:
    ; Track finished: hide sprites, hold arcade blackout briefly before Stage 1 begins.
    ldx #op_objects - 1
    lda #0
@hide_op:
    sta op_fid_lo,x
    sta op_fid_hi,x
    dex
    bpl @hide_op
    lda op_fade
    cmp #3
    beq @already_dark
    lda #3
    sta op_fade
    jsr op_fade_palette
@already_dark:
    lda #45                     ; ~0.75s arcade blackout pause before Stage 1 starts
    sta op_blackout
    rts
:
    sta op_count
    dec op_count                ; this frame is the first of them
    iny
    lda (rs_lptr),y
    pha                         ; the fade level
    ldx #0
@obj:
    iny
    lda (rs_lptr),y
    sta op_fid_lo,x
    iny
    lda (rs_lptr),y
    sta op_fid_hi,x
    iny
    lda (rs_lptr),y
    sta op_x,x
    iny
    lda (rs_lptr),y
    sta op_y,x
    inx
    cpx #op_objects
    bne @obj
    iny
    tya
    clc
    adc op_ptr
    sta op_ptr
    bcc :+
    inc op_ptr+1
:
    pla
    cmp op_fade
    beq @fade_done
    sta op_fade
    jsr op_fade_palette
@fade_done:
    pla
    jmp select_bank0
; The opening's sounds at the frames the arcade sends them (MAME 0.289, counted from the
; first frame of $792D): arcade $35 coin/start trill + opening music $36 at 0 (carrying
; $3A kidnapping at 322, ending at 506), thunder rumble at 125 ($3C) and 189 ($3D), and
; the armour $10 at 407. After the post-cutscene blackout finishes, op_finish_to_play_b7
; restores the Stage 1 palette and starts $30/$2B.
op_sound_time_lo: .byte <0, <125, <189, <407
op_sound_time_hi: .byte >0, >125, >189, >407
op_sound_b7:
    ldx #3
@check:
    lda op_time
    cmp op_sound_time_lo,x
    bne @next
    lda op_time+1
    cmp op_sound_time_hi,x
    bne @next
    cpx #0
    beq @start
    cpx #3
    beq @armour
    lda #snd_se_thunder
    jsr call_bank8_play_se
    jmp @count
@start:
    lda #snd_song_title
    jsr call_bank8_play_song
    lda #snd_se_start
    jsr call_bank8_play_se
    jmp @count
@armour:
    lda #snd_se_pickup
    jsr call_bank8_play_se
    jmp @count
@next:
    dex
    bpl @check
@count:
    inc op_time
    bne :+
    inc op_time+1
:
    rts

; Seamless transition from the opening cutscene into playable Stage 1 (in BANK7).
op_finish_to_play_b7:
    lda op_fade
    beq :+
    lda #0
    sta op_fade
    jsr op_fade_palette
:
    lda #0
    sta ap_pad
    sta ap_pad_next
    sta ap_pressed
    lda op_stage_song
    bne @song_started
    lda #snd_song_stage
    jsr call_bank8_play_song
@song_started:
    lda #0
    sta op_stage_song
    inc campaign_hud_dirty
    rts

.segment "CODE"

; The Stage 1 background colours, op_fade steps darker (the arcade fades in three steps
; of 64 frames), for the NMI.
op_fade_palette:
    lda prg_bank2
    pha
    lda #revg_bg_map_bank
    jsr select_bank2
    ldx #15
@colour:
    lda revg_bg_palette,x
    ldy op_fade
    cpy #3
    bcs @black
:
    dey
    bmi @store
    sec
    sbc #$10
    bcs :-
@black:
    lda #$0f
@store:
    and #$3f
    sta ap_tmp2
    and #$0f
    cmp #$0d
    bcc :+
    lda #$0f                    ; the column $xD-$xF is black (and $0D is not safe)
    sta ap_tmp2
:
    lda ap_tmp2
    sta revg_fade_buf,x
    dex
    bpl @colour
    lda #1
    sta revg_fade_pending
    pla
    jmp select_bank2

.segment "BANK7"
; Draw the opening's objects (from revg_draw_sprites).
rs_draw_opening:
    ldx #0
@obj:
    stx rs_shot_i
    lda op_fid_lo,x
    ora op_fid_hi,x
    beq @next
    lda op_fid_lo,x
    sta rs_fid
    lda op_fid_hi,x
    sta rs_fid+1
    lda op_last_lo,x
    ora op_last_hi,x
    bne :+
    lda op_fid_lo,x             ; nothing drawn yet: no older frame to fall back on
    sta op_last_lo,x
    lda op_fid_hi,x
    sta op_last_hi,x
:
    lda op_last_lo,x
    sta rs_last
    lda op_last_hi,x
    sta rs_last+1
    lda #0
    sta rs_mirror
    lda #$ff
    sta rs_pal
    lda op_x,x
    ldx #0
    jsr rs_set_x
    ldx rs_shot_i
    lda op_y,x
    ldx #0
    jsr rs_set_arcade_y
    jsr rs_draw_or_last
    ldx rs_shot_i
    lda rs_missing
    bne @next
    lda rs_fid
    sta op_last_lo,x
    lda rs_fid+1
    sta op_last_hi,x
@next:
    ldx rs_shot_i
    inx
    cpx #op_objects
    bne @obj
    rts
