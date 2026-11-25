; Bank 8 audio diagnostics. Keep the selected IDs independent of playback.
; The screen runs from Bank 3 ($A000) with Bank 8 at $C000; callers select Bank 3.
; Only commit_sound_test, which the NMI calls, stays in the fixed bank.
.ifdef REVG_STAGE1
st_song_max = snd_song_max
st_se_max   = 9                 ; single-digit display (1..9)
.else
st_song_max = 7                 ; prototype sound test (1..7 / 1..3)
st_se_max   = 3
.endif

.segment "BSS"
sound_test_song: .res 1
sound_test_se: .res 1

.segment "BANK3"
show_sound_test:
    jsr blank_display
    jsr call_bank8_audio_init
    lda #sound_test_scene
    sta scene
    lda #snd_song_title
    sta sound_test_song
    lda #snd_se_shot
    sta sound_test_se
    jsr load_menu_patterns
    jsr clear_screen
    ldx #0
    ldy #0
@row:
    lda sound_test_row_hi,y
    sta $2006
    lda sound_test_row_lo,y
    sta $2006
@tile:
    lda sound_test_text,x
    inx
    cmp #$ff
    beq @next_row
    sta $2007
    jmp @tile
@next_row:
    iny
    cpy #sound_test_row_count
    bne @row
    jsr commit_sound_test
    jmp enable_static_screen

update_sound_test:
    lda pressed
    and #$0f
    beq @wait
    jsr select_sound_test_ids
    jsr submit_frame
    jmp @buttons
@wait:
    jsr wait_terrain_frame
@buttons:
    lda pressed
    and #$10           ; START stops both music and SE.
    beq @play_song
    lda #snd_song_none
    jmp call_bank8_play_song
@play_song:
    lda pressed
    and #$80
    beq @play_se
    lda sound_test_song
    jsr call_bank8_play_song
@play_se:
    lda pressed
    and #$40
    beq @done
    lda sound_test_se
    jmp call_bank8_play_se
@done:
    rts

select_sound_test_ids:
    lda pressed
    and #$03
    cmp #$01
    beq @next_song
    cmp #$02
    bne @select_se
    dec sound_test_song
    bne @select_se
    lda #st_song_max
    bne @save_song
@next_song:
    lda sound_test_song
    clc
    adc #1
    cmp #st_song_max + 1
    bcc @save_song
    lda #1
@save_song:
    sta sound_test_song
@select_se:
    lda pressed
    and #$0c
    cmp #$08
    beq @next_se
    cmp #$04
    bne @done
    dec sound_test_se
    bne @done
    lda #st_se_max
    bne @save_se
@next_se:
    lda sound_test_se
    clc
    adc #1
    cmp #st_se_max + 1
    bcc @save_se
    lda #1
@save_se:
    sta sound_test_se
@done:
    rts

sound_test_row_hi: .byte $21, $21, $21, $21, $21, $22, $22, $22, $22
sound_test_row_count = * - sound_test_row_hi
sound_test_row_lo: .byte $08, $68, $88, $a8, $c8, $08, $28, $48, $88
sound_test_text:
    .byte $12, $0f, $14, $0e, $05, $00, $13, $06, $12, $13, $ff ; SOUND TEST
    .byte $03, $08, $0d, $00, $ff ; BGM
    .byte $0c, $06, $07, $13, $00, $11, $0a, $08, $09, $13, $ff ; LEFT RIGHT
    .byte $12, $06, $00, $00, $ff ; SE
    .byte $14, $10, $00, $05, $0f, $16, $0e, $ff ; UP DOWN
    .byte $02, $00, $10, $0c, $02, $18, $00, $03, $08, $0d, $ff ; A PLAY BGM
    .byte $03, $00, $10, $0c, $02, $18, $00, $12, $06, $ff ; B PLAY SE
    .byte $12, $13, $02, $11, $13, $00, $12, $13, $0f, $10, $ff ; START STOP
    .byte $12, $06, $0c, $06, $04, $13, $00, $0d, $06, $0e, $14, $ff ; SELECT MENU

.segment "CODE"
; Called during forced blank on entry, and by NMI for subsequent changes.
commit_sound_test:
    lda #$21
    sta $2006
    lda #$6c
    sta $2006
    lda sound_test_song
    clc
    adc #campaign_zero
    sta $2007
    lda #$21
    sta $2006
    lda #$ac
    sta $2006
    lda sound_test_se
    clc
    adc #campaign_zero
    sta $2007
    lda #0
    sta $2005
    sta $2005
    rts

