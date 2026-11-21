game_over_scene = 6
campaign_initial_lives = 3
campaign_frames_per_second = 60
campaign_zero = (campaign_digit_patterns - glyphs) / 16

.segment "CODE"
reset_campaign_clock:
    lda #campaign_frames_per_second
    sta campaign_subsecond
    lda #campaign_zero + 1
    sta campaign_clock
    lda #campaign_zero + 2
    sta campaign_clock+1
    lda #campaign_zero
    sta campaign_clock+2
    rts

tick_campaign_clock:
    dec campaign_subsecond
    bne clock_done
    lda #campaign_frames_per_second
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
    lda #campaign_zero + 9
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

; Labels only change on entry. Both this cold draw and VBlank use +1 VRAM stride.
draw_campaign_hud:
    lda #$20
    sta $2006
    lda #$23
    sta $2006
    ldx #0
:
    lda campaign_hud_labels,x
    sta $2007
    inx
    cpx #campaign_hud_length
    bne :-
    lda #$20
    sta $2006
    lda #$43
    sta $2006
    ldx #0
:
    lda campaign_controls,x
    sta $2007
    inx
    cpx #campaign_controls_length
    bne :-
upload_campaign_hud:
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
    lda arcade_mode
    beq @skip_weapon_hud
    lda #$20
    sta $2006
    lda #$3e
    sta $2006
    ldx ap_weapon
    lda campaign_weapon_glyphs,x
    sta $2007
@skip_weapon_hud:
    lda #0
    sta campaign_hud_dirty
    rts

.segment "RODATA"
campaign_hud_labels: ; STG # LIFE # TIME ### CP #
    .byte $12, $13, $08, 0, 0, 0, $0c, $0a, $07, $06, 0, 0, 0
    .byte $13, $0a, $0d, $06, 0, 0, 0, 0, 0, $04, $10, 0, 0
campaign_hud_length = * - campaign_hud_labels
campaign_controls: ; A JUMP  B FIRE  START RETRY
    .byte $02, 0, $18, $14, $0d, $10, 0, 0, $03, 0, $07, $0a, $11, $06, 0, 0
    .byte $12, $13, $02, $11, $13, 0, $11, $06, $13, $11, $17
campaign_controls_length = * - campaign_controls
campaign_checkpoint_glyphs: .byte $0e, $17 ; N, Y
campaign_weapon_glyphs: .byte $0c, $05, $13 ; L (Lance), D (Dagger), T (Torch)
