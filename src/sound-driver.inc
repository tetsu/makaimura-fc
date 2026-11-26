; Bank 8 ($C000-$DFFF): VRC7a FM 6-voice music + APU/FM sound effects.
; Plays the Revision G songs that tools/convert-revg-music.py converts from the Z80 sound
; ROM (src/arcade-music-data.inc): six FM channels, like the arcade's two YM2203s. The
; tables are in this bank; each song's streams are in a data bank (arc_song_bank) that
; is mapped at $8000 while the driver reads them.
; Uses WRAM_SND ($7800+) and calls fixed-bank fm_write so the hardware
; $9010 (>=6 cycles) and $9030 (>=42 cycles) wait rules are always respected.

snd_song_none     = 0
snd_song_title    = 1   ; $36, $3C, $3D, $3A: the opening demo
snd_song_stage    = 2   ; $2B: sections 0-1 (graveyard and forest)
snd_song_boss     = 3   ; $2D: the section 0-1 boss pages
snd_song_clear    = 4   ; $3B: after the key is taken
snd_song_gameover = 5   ; $2F
snd_song_stage3   = 6   ; $33: sections 2-3
snd_song_stage5   = 7   ; $29: sections 4-5
snd_song_miss     = 8   ; $31: a death with lives left
snd_song_key      = 9   ; $3E: key appearance fanfare
snd_song_name_entry = 10 ; $26: high-score name entry theme
snd_song_map      = 11  ; $30: Makai progression map / stage start jingle
snd_song_max      = 11

snd_se_none       = 0
snd_se_shot      = 1
snd_se_damage    = 2
snd_se_key       = 3
snd_se_pickup    = 4
snd_se_hit       = 5
snd_se_kill      = 6
snd_se_clank     = 7
snd_se_death     = 8
snd_se_efire     = 9
snd_se_rise      = 10
snd_se_start     = 11
snd_se_stomp     = 12
snd_se_thunder   = 13
snd_se_jump      = 14
snd_se_land      = 15
snd_se_max       = 15

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
snd_call_x:         .res 1                  ; X kept across call_bank8_play_se
snd_se_ptr_lo:      .res 3                  ; effect streams: pulse 1, pulse 2, noise
snd_se_ptr_hi:      .res 3                  ; (0 = idle)
snd_se_chpri:       .res 3                  ; the priority of the effect on each channel
snd_se_last_hi:     .res 3                  ; the last $4003/$4007 value
snd_se_new_lo:      .res 1
snd_lock:           .res 1                  ; >0 while main or NMI owns Bank 8 / VRC7 audio
snd_nmi_missed:     .res 1                  ; >0 when NMI fired during a locked main call

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
    sta snd_nmi_missed
    jmp snd_silence_all

; Silence all 6 FM voices and all APU channels cleanly.
snd_silence_all:
    jsr snd_se_stop_all
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
; A = effect id. Each of its streams takes its APU channel unless an effect of higher
; priority is playing there. Keeps X and Y (enemy code calls this with an object index).
snd_play_se:
    stx snd_call_x
    cmp #snd_se_max + 1
    bcs @done
    tax
    beq @done
    stx snd_tmp0
    sty snd_tmp1
    lda snd_se_ch_a,x
    ldy snd_se_ptr_a_lo,x
    sty snd_se_new_lo
    ldy snd_se_ptr_a_hi,x
    jsr snd_se_take
    ldx snd_tmp0
    lda snd_se_ch_b,x
    ldy snd_se_ptr_b_lo,x
    sty snd_se_new_lo
    ldy snd_se_ptr_b_hi,x
    jsr snd_se_take
    lda snd_tmp0
    sta snd_se_id
    ldy snd_tmp1
@done:
    ldx snd_call_x
    rts

; A = channel ($ff none), snd_se_new_lo/Y = stream, snd_tmp0 = effect id.
snd_se_take:
    cmp #3
    bcs @no
    tax
    lda snd_se_ptr_hi,x
    beq @free
    stx snd_ch
    ldx snd_tmp0
    lda snd_se_pri,x
    ldx snd_ch
    cmp snd_se_chpri,x
    bcc @no
@free:
    tya
    sta snd_se_ptr_hi,x
    lda snd_se_new_lo
    sta snd_se_ptr_lo,x
    ldy snd_tmp0
    lda snd_se_pri,y
    sta snd_se_chpri,x
    lda #$ff
    sta snd_se_last_hi,x        ; the first pulse frame writes the period high byte
    lda apu_channels
    ora snd_se_enable,x
    jsr set_apu_channels
    cpx #2
    beq @noise
    txa
    asl a
    asl a
    tax
    lda #$08                    ; sweep off (negate, so low periods are not muted)
    sta $4001,x
@no:
    rts
@noise:
    lda #$08
    sta $400f
    rts

snd_se_enable:
    .byte $01, $02, $08

; Stop every effect (the pause, a new song).
snd_se_stop_all:
    lda #0
    sta snd_se_ptr_hi+0
    sta snd_se_ptr_hi+1
    sta snd_se_ptr_hi+2
    sta snd_se_id
    rts

; One frame of each playing stream.
snd_update_se:
    ldx #2
@ch:
    lda snd_se_ptr_hi,x
    beq @next
    sta snd_ptr+1
    lda snd_se_ptr_lo,x
    sta snd_ptr
    ldy #0
    lda (snd_ptr),y
    cmp #$ff
    beq @end
    cpx #2
    beq @noise
    pha
    txa
    asl a
    asl a
    sta snd_tmp0                ; the channel's register offset
    pla
    stx snd_ch
    ldx snd_tmp0
    sta $4000,x
    iny
    lda (snd_ptr),y
    sta $4002,x
    iny
    lda (snd_ptr),y
    ldx snd_ch
    cmp snd_se_last_hi,x
    beq :+
    sta snd_se_last_hi,x        ; a $4003 write restarts the phase: only on a change
    ldx snd_tmp0
    sta $4003,x
    ldx snd_ch
:
    lda #3
    bne @advance
@noise:
    sta $400c
    iny
    lda (snd_ptr),y
    sta $400e
    lda #2
@advance:
    clc
    adc snd_se_ptr_lo,x
    sta snd_se_ptr_lo,x
    bcc @next
    inc snd_se_ptr_hi,x
    bne @next
@end:
    lda #0
    sta snd_se_ptr_hi,x
    lda #$30                    ; silent, constant volume
    cpx #2
    beq :+
    stx snd_ch
    txa
    asl a
    asl a
    tax
    lda #$30
    sta $4000,x
    ldx snd_ch
    jmp @next
:
    sta $400c
@next:
    dex
    bmi :+
    jmp @ch
:
    lda snd_se_ptr_hi+0
    ora snd_se_ptr_hi+1
    ora snd_se_ptr_hi+2
    bne :+
    sta snd_se_id
:
    rts

; NMI lag-frame audio entry (Bank 8 mapped at $C000): keeps BGM/SFX running at 60 Hz even
; when update_game overruns VBlank, while preserving prg_bank0 and map_cached_col.
snd_nmi_entry:
    lda game_paused
    bne @skip
    lda scene
    cmp #stage_view_scene
    beq @skip
    lda snd_lock
    beq @run
    inc snd_nmi_missed
@skip:
    rts
@run:
    inc snd_lock
    cli
    lda #0
    sta snd_nmi_missed
    lda prg_bank0
    pha
    lda map_cached_col
    pha
    jsr snd_tick
    pla
    sta map_cached_col
    pla
    sta prg_bank0
    sta $8000
    dec snd_lock
    rts

; Per-frame sound driver update.
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
; Sound effect tables (in BANK8), from the effect list in the sound effect engine.
; Each effect has up to two streams: channel (0 pulse 1, 1 pulse 2, 2 noise, $ff none)
; and a frame list. A pulse frame is $4000 control, period low, period high; a noise
; frame is $400C control and the $400E period index. $FF ends the stream.
; ---------------------------------------------------------------------------
snd_se_pri:
    .byte 0, 1, 5, 4, 3, 2, 3, 2, 6, 1, 1, 5, 2, 4, 1, 1
snd_se_ch_a:
    .byte $ff, $01, $02, $00, $00, $02, $01, $01, $00, $01, $01, $00, $02, $02, $01, $02
snd_se_ch_b:
    .byte $ff, $ff, $00, $ff, $ff, $01, $ff, $02, $ff, $ff, $ff, $01, $01, $ff, $ff, $ff
snd_se_ptr_a_lo:
    .byte 0, <snd_se_shot_0, <snd_se_damage_0, <snd_se_key_0, <snd_se_pickup_0, <snd_se_hit_0, <snd_se_kill_0, <snd_se_clank_0, <snd_se_death_0, <snd_se_efire_0, <snd_se_rise_0, <snd_se_start_0, <snd_se_stomp_0, <snd_se_thunder_0, <snd_se_jump_0, <snd_se_land_0
snd_se_ptr_a_hi:
    .byte 0, >snd_se_shot_0, >snd_se_damage_0, >snd_se_key_0, >snd_se_pickup_0, >snd_se_hit_0, >snd_se_kill_0, >snd_se_clank_0, >snd_se_death_0, >snd_se_efire_0, >snd_se_rise_0, >snd_se_start_0, >snd_se_stomp_0, >snd_se_thunder_0, >snd_se_jump_0, >snd_se_land_0
snd_se_ptr_b_lo:
    .byte 0, <snd_se_none_stream, <snd_se_damage_1, <snd_se_none_stream, <snd_se_none_stream, <snd_se_hit_1, <snd_se_none_stream, <snd_se_clank_1, <snd_se_none_stream, <snd_se_none_stream, <snd_se_none_stream, <snd_se_start_1, <snd_se_stomp_1, <snd_se_none_stream, <snd_se_none_stream, <snd_se_none_stream
snd_se_ptr_b_hi:
    .byte 0, >snd_se_none_stream, >snd_se_damage_1, >snd_se_none_stream, >snd_se_none_stream, >snd_se_hit_1, >snd_se_none_stream, >snd_se_clank_1, >snd_se_none_stream, >snd_se_none_stream, >snd_se_none_stream, >snd_se_start_1, >snd_se_stomp_1, >snd_se_none_stream, >snd_se_none_stream, >snd_se_none_stream
snd_se_none_stream:
    .byte $ff
.include "revg-se-data.inc"
; Title START chime (modeled on arcade $35 coin/start bell trill:
; C#6 ($63, 1109 Hz) / D6 ($5E, 1175 Hz) alternating every 4 frames on Pulse 1,
; with octave-up shimmer echo C#7 ($31) / D7 ($2F) on Pulse 2).
snd_se_start_0:
    .byte $7f, $63, $00, $7d, $63, $00, $7a, $63, $00, $77, $63, $00
    .byte $7f, $5e, $00, $7d, $5e, $00, $7a, $5e, $00, $77, $5e, $00
    .byte $7e, $63, $00, $7c, $63, $00, $79, $63, $00, $76, $63, $00
    .byte $7e, $5e, $00, $7c, $5e, $00, $79, $5e, $00, $76, $5e, $00
    .byte $7d, $63, $00, $7b, $63, $00, $78, $63, $00, $75, $63, $00
    .byte $7c, $5e, $00, $7a, $5e, $00, $77, $5e, $00, $74, $5e, $00
    .byte $7b, $63, $00, $79, $63, $00, $76, $63, $00, $73, $63, $00
    .byte $7a, $5e, $00, $78, $5e, $00, $75, $5e, $00, $72, $5e, $00
    .byte $78, $63, $00, $76, $63, $00, $74, $63, $00, $72, $63, $00
    .byte $76, $5e, $00, $74, $5e, $00, $72, $5e, $00, $70, $5e, $00
    .byte $ff
snd_se_start_1:
    .byte $b0, $31, $00, $b0, $31, $00, $b0, $31, $00, $b0, $31, $00
    .byte $bd, $31, $00, $bb, $31, $00, $b9, $31, $00, $b7, $31, $00
    .byte $bd, $2f, $00, $bb, $2f, $00, $b9, $2f, $00, $b7, $2f, $00
    .byte $bc, $31, $00, $ba, $31, $00, $b8, $31, $00, $b6, $31, $00
    .byte $bc, $2f, $00, $ba, $2f, $00, $b8, $2f, $00, $b6, $2f, $00
    .byte $bb, $31, $00, $b9, $31, $00, $b7, $31, $00, $b5, $31, $00
    .byte $ba, $2f, $00, $b8, $2f, $00, $b6, $2f, $00, $b4, $2f, $00
    .byte $b9, $31, $00, $b7, $31, $00, $b5, $31, $00, $b3, $31, $00
    .byte $b8, $2f, $00, $b6, $2f, $00, $b4, $2f, $00, $b2, $2f, $00
    .byte $b6, $31, $00, $b4, $31, $00, $b2, $31, $00, $b1, $31, $00
    .byte $ff
; Opening thunder rumble (arcade $3C/$3D noise crash & rolling rumble).
snd_se_thunder_0:
    .byte $3f, $06, $3f, $07, $3e, $08, $3f, $09, $3e, $0a, $3d, $0b
    .byte $3e, $0a, $3d, $0b, $3c, $0c, $3d, $0b, $3c, $0c, $3b, $0d
    .byte $3c, $0c, $3b, $0d, $3a, $0e, $3a, $0d, $39, $0e, $38, $0f
    .byte $38, $0e, $37, $0f, $36, $0e, $36, $0f, $35, $0f, $34, $0f
    .byte $33, $0f, $32, $0f, $31, $0f, $ff
