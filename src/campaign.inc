game_over_scene = 6
campaign_initial_lives = 3
campaign_frames_per_second = 60
campaign_zero = (campaign_digit_patterns - glyphs) / 16

.segment "CODE"
reset_campaign_clock:
    lda #campaign_frames_per_second
    sta campaign_subsecond
.ifdef REVG_STAGE1
    lda arcade_mode
    bne @arcade_clock
.endif
    lda #campaign_zero + 1
    sta campaign_clock
    lda #campaign_zero + 2
    sta campaign_clock+1
    lda #campaign_zero
    sta campaign_clock+2
    rts
.ifdef REVG_STAGE1
@arcade_clock:
    lda #64                     ; arcade clock ticks 1 second per 64 frames
    sta campaign_subsecond
    ldx campaign_stage
    cpx #8
    bcc :+
    ldx #1
:
    lda @ac_min,x
    ldy checkpoint_reached
    beq :+
    cpx #2
    bne :+
    lda #campaign_zero + 2      ; Stage 2 checkpoint gives 2:30
:
    sta campaign_clock
    lda @ac_sec,x
    sta campaign_clock+1
    lda #campaign_zero
    sta campaign_clock+2
    rts
; Arcade clocks: S1=2:00, S2=1:30(2:30 cp), S3=2:30, S4=2:00, S5=3:40, S6=3:40, S7=2:00
@ac_min: .byte campaign_zero+2, campaign_zero+2, campaign_zero+1, campaign_zero+2, campaign_zero+2, campaign_zero+3, campaign_zero+3, campaign_zero+2
@ac_sec: .byte campaign_zero,   campaign_zero,   campaign_zero+3, campaign_zero+3, campaign_zero,   campaign_zero+4, campaign_zero+4, campaign_zero
.endif

tick_campaign_clock:
    lda arcade_mode
    beq :+
    lda opt_no_timer
    bne clock_done
:
    dec campaign_subsecond
    bne clock_done
    lda #campaign_frames_per_second
.ifdef REVG_STAGE1
    ldx arcade_mode
    beq :+
    lda #64
:
.endif
    sta campaign_subsecond
    inc campaign_hud_dirty
    dec campaign_clock+2
    lda campaign_clock+2
    cmp #campaign_zero
    bcs clock_check_zero
    lda #campaign_zero + 9
    sta campaign_clock+2
    dec campaign_clock+1
    lda campaign_clock+1
    cmp #campaign_zero
    bcs clock_done
.ifdef REVG_STAGE1
    lda arcade_mode
    beq :+
    lda #campaign_zero + 5      ; arcade M:SS wraps tens-of-seconds to 5 (2:00 -> 1:59)
    bne :++
:
.endif
    lda #campaign_zero + 9
.ifdef REVG_STAGE1
:
.endif
    sta campaign_clock+1
    dec campaign_clock
    rts
clock_check_zero:
    cmp #campaign_zero
    bne clock_done
    cmp campaign_clock+1
    bne clock_done
    cmp campaign_clock
    bne clock_done
    lda #1
    sta player_hit
clock_done:
    rts

; Labels only change on entry (display off); delegate cold draw to BANK7 so FIXED keeps headroom.
draw_campaign_hud:
    lda prg_bank2
    pha
    lda #7
    jsr select_bank2
    jsr draw_campaign_hud_b7
    pla
    jsr select_bank2
upload_campaign_hud:
.ifdef REVG_STAGE1
    lda arcade_mode
    bne @upload_arcade_hud
.endif
    lda #$20
    sta $2006
    lda #$27
    sta $2006
    lda campaign_stage
    clc
    adc #campaign_zero
    sta $2007
    lda #$20
    sta $2006
    lda #$2e
    sta $2006
    lda campaign_lives
    clc
    adc #campaign_zero
    sta $2007
    lda #$20
    sta $2006
    lda #$35
    sta $2006
    lda campaign_clock
    sta $2007
    lda campaign_clock+1
    sta $2007
    lda campaign_clock+2
    sta $2007
    lda #$20
    sta $2006
    lda #$3c
    sta $2006
    ldx checkpoint_reached
    lda campaign_checkpoint_glyphs,x
    sta $2007
.ifdef REVG_STAGE1
    jmp @hud_done
@upload_arcade_hud:
    ; Row 2: 1UP score ($2042), TOP SCORE ($204A), TIME M:SS ($2055), L# + weapon ($205B)
    lda #$42
    ldx #0
    jsr @write_score6
    lda #$4a
    ldx #(ap_top_score - ap_score)
    jsr @write_score6
    lda #$20
    sta $2006
    lda #$55
    sta $2006
    lda campaign_clock
    sta $2007
    lda #$17                    ; ':' colon tile uploaded in ap_load_stage_visuals
    sta $2007
    lda campaign_clock+1
    sta $2007
    lda campaign_clock+2
    sta $2007
    lda #$20
    sta $2006
    lda #$5b
    sta $2006
    lda #$0c                    ; L
    sta $2007
    lda campaign_lives
    clc
    adc #campaign_zero
    sta $2007
    lda #0
    sta $2007
    lda ap_frog_timer
    ora ap_frog_timer+1
    beq :+
    lda #0                      ; HUD weapon slot is blank while transformed into a frog
    beq :++
:
    ldx ap_weapon
    lda campaign_weapon_glyphs,x
:
    sta $2007
@hud_done:
    lda #0
    sta campaign_hud_dirty
    rts
@write_score6:
    ldy #$20
    sty $2006
    sta $2006
    ldy #4
:
    lda ap_score,x
    clc
    adc #campaign_zero
    sta $2007
    inx
    dey
    bne :-
    lda #campaign_zero
    sta $2007
    sta $2007
    rts
.else
    lda arcade_mode
    beq @hud_done
    lda #$20
    sta $2006
    lda #$3e
    sta $2006
    ldx ap_weapon
    lda campaign_weapon_glyphs,x
    sta $2007
    lda #$20
    sta $2006
    lda #$49
    sta $2006
    ldx #0
:
    lda ap_score,x
    clc
    adc #campaign_zero
    sta $2007
    inx
    cpx #4
    bne :-
    lda #campaign_zero
    sta $2007
    sta $2007
    lda #$20
    sta $2006
    lda #$56
    sta $2006
    lda ap_loop
    clc
    adc #campaign_zero + 1
    sta $2007
@hud_done:
    lda #0
    sta campaign_hud_dirty
    rts
.endif

.segment "RODATA"
campaign_checkpoint_glyphs: .byte $0e, $17 ; N, Y
.ifdef REVG_STAGE1
campaign_weapon_glyphs: .byte $0c, $05, $13, $04, $02 ; L (Lance), D (Dagger), T (Torch), C (Cross), A (Axe)
.else
campaign_weapon_glyphs: .byte $0c, $05, $13, $12 ; L (Lance), D (Dagger), T (Torch), S (Shield)
.endif
