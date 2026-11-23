; Bank 8 ($C000-$DFFF): VRC7a FM 6-voice music + APU/FM sound effects.
; Plays the Revision G songs that tools/convert-revg-music.py converts from the Z80 sound
; ROM (src/arcade-music-data.inc): six FM channels, like the arcade's two YM2203s. The
; tables are in this bank; each song's streams are in a data bank (arc_song_bank) that
; is mapped at $8000 while the driver reads them.
; Uses WRAM_SND ($7800+) and calls fixed-bank fm_write so the hardware
; $9010 (>=6 cycles) and $9030 (>=42 cycles) wait rules are always respected.

snd_song_none     = 0
snd_song_title    = 1   ; $3C, $3D, $3A: the opening demo
snd_song_stage    = 2   ; $30 then $2B: sections 0-1 (graveyard and forest)
snd_song_boss     = 3   ; $2D: the section 0-1 boss pages
snd_song_clear    = 4   ; $3B: after the key is taken
snd_song_gameover = 5   ; $2F
snd_song_stage3   = 6   ; $30 then $33: sections 2-3
snd_song_stage5   = 7   ; $30 then $29: sections 4-5
snd_song_max      = 7

snd_se_none       = 0
snd_se_shot       = 1
snd_se_damage     = 2
snd_se_key        = 3
snd_se_max        = snd_se_key

snd_fm_channels   = 6

; Sequence stream commands (one stream per FM channel):
;   $00 dd       key off (the pitch stays for the release), then wait dd frames
;   $01-$5F dd   key on note n (MIDI n - 1), then wait dd frames
;   $60 dd       wait dd frames
;   $61 nn dd    change the pitch to note nn without a new attack, then wait dd frames
;   $62 ii       set the instrument and volume ($30+ch), then read the next command
;   $63 pp       load custom patch pp (arc_patches) into $00-$07, then read on
;   $FE          end of this channel (a held note keeps sounding, as in the arcade)
;   $FF lo hi    jump

.segment "WRAM_SND"
; Preserve $7800..$7807 exact offsets for Mesen audio smoke tests.
snd_song_id:        .res 1                  ; $7800
snd_playing:        .res 1                  ; $7801
snd_custom_patch:   .res 1                  ; $7802 ($ff = unloaded)
snd_se_id:          .res 1                  ; $7803
snd_se_timer:       .res 1                  ; $7804
snd_se_fm_mask:     .res 1                  ; $7805 (bit 5 = FM ch5 preempted by SE)
snd_se_apu_mask:    .res 1                  ; $7806 (bit 1 = Pulse2, bit 3 = Noise)
snd_tick_count:     .res 1                  ; $7807

snd_fm_ptr_lo:      .res snd_fm_channels    ; 16-bit stream pointers for FM ch0..5
snd_fm_ptr_hi:      .res snd_fm_channels
snd_fm_wait:        .res snd_fm_channels
snd_fm_inst:        .res snd_fm_channels    ; $30+ch shadow
snd_fm_fnum_lo:     .res snd_fm_channels    ; $10+ch shadow
snd_fm_fnum_hi:     .res snd_fm_channels    ; $20+ch shadow (bit 4 = key on)
snd_song_bank:      .res 1                  ; the data bank of the playing song

snd_tmp0:           .res 1
snd_tmp1:           .res 1
snd_ch:             .res 1
snd_saved_bank0:    .res 1
snd_call_arg:       .res 1                  ; the song or effect call_bank8_play_* passes on

.segment "BANK8"
    .byte bank_tag_base + 8

; Full sound driver reset (silence all FM + APU voices and clear work RAM).
snd_init:
    lda #$ff
    sta snd_custom_patch
    lda #0
    sta snd_song_id
    sta snd_playing
    sta snd_se_id
    sta snd_se_timer
    sta snd_se_fm_mask
    sta snd_se_apu_mask
    sta snd_tick_count
    jmp snd_silence_all

; Silence all 6 FM voices and all APU channels cleanly.
snd_silence_all:
    lda #0
    jsr set_apu_channels
    sta $4000
    sta $4004
    sta $4008
    sta $400c
    ldx #snd_fm_channels - 1
@sil_fm:
    lda #0
    sta snd_fm_fnum_hi,x
    txa
    ora #$20
    ldy #0
    jsr fm_write
    dex
    bpl @sil_fm
    rts

; A = song ID (0..snd_song_max). Resets all voices, loads the song's first custom
; patch into $00..$07 and points the six FM channels at their streams.
snd_play_song:
    sta snd_tmp0
    jsr snd_silence_all
    lda #0
    sta snd_se_id
    sta snd_se_timer
    sta snd_se_fm_mask
    sta snd_se_apu_mask
    sta shot_sound_pending
    sta shot_sound_frames

    lda snd_tmp0
    sta snd_song_id
    beq @stop_song
    cmp #snd_song_max + 1
    bcs @stop_song

    tax
    lda arc_song_bank - 1,x
    sta snd_song_bank
    lda arc_song_patch - 1,x
    jsr snd_load_custom_patch
    ; The six streams; each starts by setting its instrument ($62).
    ldx snd_song_id
    lda #0
:
    dex
    beq :+
    clc
    adc #snd_fm_channels
    bne :-
:
    tay
    ldx #0
@init_fm_ch:
    lda #0
    sta snd_fm_fnum_lo,x
    sta snd_fm_fnum_hi,x
    sta snd_fm_inst,x
    lda #1
    sta snd_fm_wait,x
    lda arc_trk_lo,y
    sta snd_fm_ptr_lo,x
    lda arc_trk_hi,y
    sta snd_fm_ptr_hi,x
    iny
    inx
    cpx #snd_fm_channels
    bne @init_fm_ch
    lda #1
    sta snd_playing
    rts

@stop_song:
    lda #0
    sta snd_playing
    rts

; A = custom patch index (arc_patches, 8 bytes each). Preserves X.
snd_load_custom_patch:
    sta snd_custom_patch
    stx snd_tmp1
    asl a
    asl a
    asl a
    tax
    lda #0
    sta snd_tmp0
@patch_loop:
    ldy arc_patches,x
    lda snd_tmp0
    jsr fm_write
    inx
    inc snd_tmp0
    lda snd_tmp0
    cmp #8
    bne @patch_loop
    ldx snd_tmp1
    rts

; A = SE ID (1=shot, 2=damage, 3=key).
snd_play_se:
    cmp #snd_se_none
    beq @se_done
    ; Damage and Key SEs have higher priority than shot SE.
    cmp #snd_se_shot
    bne @accept_se
    lda snd_se_id
    cmp #snd_se_damage
    beq @se_done
    cmp #snd_se_key
    beq @se_done
    lda #snd_se_shot
@accept_se:
    sta snd_tmp0
    ; Clean up any APU channels owned by a previously active SE before starting the new one.
    lda snd_se_apu_mask
    and #$02
    beq @accept_chk_noi
    lda apu_channels
    and #$fd
    jsr set_apu_channels
@accept_chk_noi:
    lda snd_se_apu_mask
    and #$08
    beq @accept_dispatch
    lda #$30
    sta $400c
@accept_dispatch:
    lda snd_tmp0
    sta snd_se_id
    cmp #snd_se_shot
    beq @start_se_shot
    cmp #snd_se_damage
    beq @start_se_damage
    cmp #snd_se_key
    beq @start_se_key
@se_done:
    rts

@start_se_shot:
    lda #6
    sta snd_se_timer
    lda #$02                    ; Pulse 2
    sta snd_se_apu_mask
    lda #0
    sta $4005
    lda apu_channels
    ora #$02
    jsr set_apu_channels
    lda #0
    sta $4007
    rts

@start_se_damage:
    lda #14
    sta snd_se_timer
    lda #$20                    ; Preempt FM ch5
    sta snd_se_fm_mask
    lda #$08                    ; Preempt APU Noise
    sta snd_se_apu_mask
    ; Key-off FM ch5 then program metallic FM crash instrument ($35 = inst 15, vol 2).
    lda #$25
    ldy #0
    jsr fm_write
    lda #$35
    ldy #$f2
    jsr fm_write
    lda #$15
    ldy #$80
    jsr fm_write
    lda #$25
    ldy #$14                    ; Key-on, block 2
    jsr fm_write
    ; APU noise burst
    lda apu_channels
    ora #$08
    jsr set_apu_channels
    lda #$3c
    sta $400c
    lda #$0a
    sta $400e
    lda #$08
    sta $400f
    rts

@start_se_key:
    lda #18
    sta snd_se_timer
    lda #$20                    ; Preempt FM ch5
    sta snd_se_fm_mask
    lda #0
    sta snd_se_apu_mask
    lda #$25
    ldy #0
    jsr fm_write
    lda #$35
    ldy #$31                    ; Bell/piano preset, loud
    jsr fm_write
    rts

; Per-frame sound driver update (called once per frame from main loop).
snd_tick:
    inc snd_tick_count
    ; Bridge legacy shot_sound_pending flag into SE driver.
    lda shot_sound_pending
    beq @tick_se_now
    lda #0
    sta shot_sound_pending
    lda #snd_se_shot
    jsr snd_play_se

@tick_se_now:
    jsr snd_update_se
    lda snd_playing
    beq @tick_done
    lda prg_bank0
    sta snd_saved_bank0
    lda snd_song_bank
    jsr select_bank0
    jsr snd_update_fm_bgm
    lda snd_saved_bank0
    jsr select_bank0
@tick_done:
    rts

; Update active SE and restore preempted FM/APU channels upon completion.
snd_update_se:
    lda snd_se_id
    bne :+
    rts
:
    ldx snd_se_timer
    bne :+
    jmp @se_finish
:
    cmp #snd_se_shot
    beq @se_u_shot
    cmp #snd_se_damage
    beq @se_u_damage
    cmp #snd_se_key
    beq @se_u_key
    jmp @se_finish

@se_u_shot:
    lda snd_shot_vol-1,x
    sta $4004
    lda snd_shot_per-1,x
    sta $4006
    dec snd_se_timer
    beq :+
    rts
:
    jmp @se_finish

@se_u_damage:
    lda snd_dmg_fnum-1,x
    tay
    lda #$15
    jsr fm_write
    lda snd_dmg_noise-1,x
    sta $400c
    dec snd_se_timer
    beq :+
    rts
:
    jmp @se_finish

@se_u_key:
    txa
    cmp #18
    beq @key_note0
    cmp #12
    beq @key_note1
    cmp #6
    beq @key_note2
    bne @key_dec
@key_note0:
    ldy #$ac                    ; C5
    lda #$15
    jsr fm_write
    ldy #$18                    ; Key-on, block 4
    lda #$25
    jsr fm_write
    jmp @key_dec
@key_note1:
    ldy #$d9                    ; E5
    lda #$15
    jsr fm_write
    ldy #$18
    lda #$25
    jsr fm_write
    jmp @key_dec
@key_note2:
    ldy #$02                    ; G5
    lda #$15
    jsr fm_write
    ldy #$19
    lda #$25
    jsr fm_write
@key_dec:
    dec snd_se_timer
    bne @se_u_done

@se_finish:
    ; Mute Pulse 2 if shot SE owned it.
    lda snd_se_apu_mask
    and #$02
    beq @check_restore_fm
    lda apu_channels
    and #$fd
    jsr set_apu_channels
@check_restore_fm:
    ; Restore preempted FM ch5 instrument and pitch from BGM shadow.
    lda snd_se_fm_mask
    and #$20
    beq @clear_se_state
    lda #$25
    ldy #0
    jsr fm_write
    lda #$35
    ldy snd_fm_inst+5
    jsr fm_write
    lda snd_fm_fnum_hi+5
    and #$10
    beq @clear_se_state         ; the music's note there is not held
    lda #$15
    ldy snd_fm_fnum_lo+5
    jsr fm_write
    lda #$25
    ldy snd_fm_fnum_hi+5
    jsr fm_write
@clear_se_state:
    lda #0
    sta snd_se_id
    sta snd_se_timer
    sta snd_se_fm_mask
    sta snd_se_apu_mask
@se_u_done:
    rts

; Update the six FM channels. Channel 5 only updates its shadow while an effect owns it.
snd_update_fm_bgm:
    ldx #0
    beq @fm_ch_loop
@fm_skip:
    jmp @fm_next_ch
@fm_ch_loop:
    stx snd_ch
    lda snd_fm_ptr_hi,x
    beq @fm_skip             ; this channel has ended
    dec snd_fm_wait,x
    bne @fm_skip
@fm_read:
    lda snd_fm_ptr_lo,x
    sta snd_ptr
    lda snd_fm_ptr_hi,x
    sta snd_ptr+1
    ldy #0
@fm_command:
    lda (snd_ptr),y
    cmp #$ff
    bne @fm_not_jump
    iny
    lda (snd_ptr),y
    pha
    iny
    lda (snd_ptr),y
    sta snd_ptr+1
    pla
    sta snd_ptr
    ldy #0
    beq @fm_command
@fm_not_jump:
    iny
    cmp #$fe
    bne :+
    jmp @fm_channel_end
:
    cmp #$62
    bcs @fm_untimed
    sta snd_tmp0                ; $00-$61
    cmp #$61
    bne :+
    lda (snd_ptr),y             ; $61: the note
    iny
    sta snd_tmp0
    lda #$80                    ; bit 7 marks the legato
    ora snd_tmp0
    sta snd_tmp0
:
    lda (snd_ptr),y
    iny
    sta snd_fm_wait,x
    jsr @fm_store_pointer
    lda snd_tmp0
    beq @fm_key_off
    cmp #$60
    beq @fm_skip
    jsr @fm_set_pitch           ; $01-$5F or the legato note
    jsr @fm_owned
    bcs @fm_skip
    lda snd_tmp0
    bmi @fm_write_pitch         ; legato: no new attack
    txa                         ; key off first, keeping the pitch, so the note attacks again
    ora #$20
    tay
    lda snd_fm_fnum_hi,x
    and #$0f
    jsr @fm_write_y_reg
@fm_write_pitch:
    txa
    ora #$10
    tay
    lda snd_fm_fnum_lo,x
    jsr @fm_write_y_reg
    txa
    ora #$20
    tay
    lda snd_fm_fnum_hi,x
    jsr @fm_write_y_reg
    jmp @fm_next_ch

@fm_key_off:
    lda snd_fm_fnum_hi,x
    and #$0f
    sta snd_fm_fnum_hi,x
    jsr @fm_owned
    bcs :+
    txa
    ora #$20
    tay
    lda snd_fm_fnum_hi,x
    jsr @fm_write_y_reg
:
    jmp @fm_next_ch

@fm_untimed:                    ; $62 ii or $63 pp, then the next command
    sta snd_tmp0
    lda (snd_ptr),y
    iny
    pha
    jsr @fm_store_pointer
    pla
    ldy snd_tmp0
    cpy #$63
    beq @fm_patch
    sta snd_fm_inst,x
    jsr @fm_owned
    bcc :+
    jmp @fm_read
:
    txa
    ora #$30
    tay
    lda snd_fm_inst,x
    jsr @fm_write_y_reg
    jmp @fm_read
@fm_patch:
    jsr snd_load_custom_patch
    jmp @fm_read

@fm_channel_end:
    lda #0
    sta snd_fm_ptr_hi,x
    ldy #snd_fm_channels - 1    ; the song ends when every channel has
:
    lda snd_fm_ptr_hi,y
    bne @fm_next_ch
    dey
    bpl :-
    sta snd_playing

@fm_next_ch:
    ldx snd_ch
    inx
    cpx #snd_fm_channels
    beq :+
    jmp @fm_ch_loop
:
    rts

; The stream pointer of channel X = snd_ptr + Y.
@fm_store_pointer:
    tya
    clc
    adc snd_ptr
    sta snd_fm_ptr_lo,x
    lda snd_ptr+1
    adc #0
    sta snd_fm_ptr_hi,x
    rts

; The F-number shadow of channel X from note snd_tmp0 (bit 7 ignored), key on.
@fm_set_pitch:
    lda snd_tmp0
    and #$7f
    tay
    lda arc_fm_fnum_lo,y
    sta snd_fm_fnum_lo,x
    lda arc_fm_fnum_hi,y
    sta snd_fm_fnum_hi,x
    rts

; C set when an effect owns channel X (channel 5 under snd_se_fm_mask bit 5).
@fm_owned:
    cpx #5
    bne :+
    lda snd_se_fm_mask
    and #$20
    cmp #$20
    rts
:
    clc
    rts

; Write A to VRC7 register Y.
@fm_write_y_reg:
    sta snd_tmp1
    tya
    ldy snd_tmp1
    jmp fm_write

; ---------------------------------------------------------------------------
; Sound effect tables (in BANK8)
; ---------------------------------------------------------------------------

; Shot SE envelopes (indexed descending 6..1)
snd_shot_vol:
    .byte $32, $34, $36, $38, $3a, $3c
snd_shot_per:
    .byte $78, $66, $56, $48, $3c, $32

; Damage SE envelopes (indexed descending 14..1)
snd_dmg_fnum:
    .byte $30, $34, $38, $3c, $40, $46, $4c, $52, $58, $60, $68, $70, $78, $80
snd_dmg_noise:
    .byte $30, $31, $32, $33, $34, $35, $36, $37, $38, $39, $3a, $3b, $3c, $3d
