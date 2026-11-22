; Bank 8 ($C000-$DFFF): VRC7a FM 6-voice + APU BGM/SE sound driver.
; Plays faithful arcade Makaimura (Revision G) music data stored in BANK9
; ($8000-$9FFF data window, temporarily banked in during snd_play_song/snd_tick).
; Uses WRAM_SND ($7800+) and calls fixed-bank fm_write so the hardware
; $9010 (>=6 cycles) and $9030 (>=42 cycles) wait rules are always respected.

snd_song_none     = 0
snd_song_title    = 1   ; Arcade Start Demo jingle
snd_song_stage    = 2   ; Arcade Stage 1 & 2 (Graveyard & Forest)
snd_song_boss     = 3   ; Arcade Boss theme
snd_song_clear    = 4   ; Arcade Stage Clear jingle
snd_song_gameover = 5   ; Arcade Game Over jingle
snd_song_stage3   = 6   ; Arcade Stage 3 & 4 (Cavern & Bridge)
snd_song_stage5   = 7   ; Arcade Stage 5 & 6 (Castle)
snd_song_max      = 7

snd_se_none       = 0
snd_se_shot       = 1
snd_se_damage     = 2
snd_se_key        = 3
snd_se_max        = snd_se_key

snd_fm_channels   = 6
snd_music_bank    = 9

; Sequence stream commands (in BANK9):
;   $00       : rest / key-off, followed by 1-byte duration (frames)
;   $01..$54  : note MIDI 24..107 (C1..B7), followed by 1-byte duration (frames)
;   $55       : hold current note without re-keyon, followed by 1-byte duration
;   $FE       : end of non-looping song
;   $FF lo hi : jump to 16-bit loop pointer in BANK9

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
snd_fm_fnum_hi:     .res snd_fm_channels    ; $20+ch shadow

snd_drm_ptr_lo:     .res 1                  ; 16-bit stream pointer for APU drum track
snd_drm_ptr_hi:     .res 1
snd_drm_wait:       .res 1

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

; A = song ID (0..snd_song_max). Resets all voices, loads the song's shared FM
; custom patch into $00..$07 if changed, and initializes all 6 FM + drum tracks.
snd_play_song:
    sta snd_tmp0
    lda prg_bank0
    sta snd_saved_bank0
    lda #snd_music_bank
    jsr select_bank0

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

    ; Load shared FM custom patch ($00..$07) for this song if needed.
    tax
    lda arc_song_patch_id,x
    cmp snd_custom_patch
    beq @patch_ready
    sta snd_custom_patch
    jsr snd_load_custom_patch
@patch_ready:
    ; Initialize FM channel instruments & 16-bit stream pointers.
    ldx snd_song_id
    lda arc_song_inst_base,x
    sta snd_tmp1
    lda arc_song_trk_offset,x
    sta snd_tmp0

    ldx #0
@init_fm_ch:
    lda #0
    sta snd_fm_fnum_lo,x
    sta snd_fm_fnum_hi,x
    lda #1
    sta snd_fm_wait,x

    ldy snd_tmp0
    lda arc_trk_lo,y
    sta snd_fm_ptr_lo,x
    lda arc_trk_hi,y
    sta snd_fm_ptr_hi,x
    inc snd_tmp0

    ldy snd_tmp1
    lda arc_inst_presets,y
    sta snd_fm_inst,x
    inc snd_tmp1
    ; Program $30+ch right away.
    tay
    txa
    ora #$30
    jsr fm_write

    inx
    cpx #snd_fm_channels
    bne @init_fm_ch

    ; Initialize APU drum track (7th track of the song).
    ldy snd_tmp0
    lda arc_trk_lo,y
    sta snd_drm_ptr_lo
    lda arc_trk_hi,y
    sta snd_drm_ptr_hi
    lda #1
    sta snd_drm_wait

    ; Enable APU Triangle ($04) + Noise ($08) = $0C for drums.
    lda #$0c
    jsr set_apu_channels
    lda #1
    sta snd_playing
    jmp @restore_bank0

@stop_song:
    lda #0
    sta snd_playing
@restore_bank0:
    lda snd_saved_bank0
    jmp select_bank0

; A = custom patch index (0 = arcade bright brass/organ lead, 1 = boss sharp FM lead).
snd_load_custom_patch:
    asl a
    asl a
    asl a
    tax
    ldy #0
    sty snd_tmp1
@patch_loop:
    lda snd_custom_patches,x
    tay
    lda snd_tmp1
    jsr fm_write
    inx
    inc snd_tmp1
    lda snd_tmp1
    cmp #8
    bne @patch_loop
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
    lda #snd_music_bank
    jsr select_bank0
    jsr snd_update_fm_bgm
    jsr snd_update_drum_bgm
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
    beq @clear_se_state
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

; Update all 6 FM BGM voices; channel 5 updates shadow only if preempted by SE.
snd_update_fm_bgm:
    lda #0
    sta snd_ch
@fm_ch_loop:
    ldx snd_ch
    dec snd_fm_wait,x
    beq @fm_fetch_note
    jmp @fm_next_ch

@fm_fetch_note:
    lda snd_fm_ptr_lo,x
    sta snd_ptr
    lda snd_fm_ptr_hi,x
    sta snd_ptr+1

    ldy #0
    lda (snd_ptr),y
    cmp #$ff
    bne @fm_check_end
    ; Jump to 16-bit loop address in BANK9
    iny
    lda (snd_ptr),y
    sta snd_fm_ptr_lo,x
    iny
    lda (snd_ptr),y
    sta snd_fm_ptr_hi,x
    lda snd_fm_ptr_lo,x
    sta snd_ptr
    lda snd_fm_ptr_hi,x
    sta snd_ptr+1
    ldy #0
    lda (snd_ptr),y
@fm_check_end:
    cmp #$fe
    bne @fm_play_note
    ; Non-looping jingle finished: silence all and stop playback cleanly.
    jsr snd_silence_all
    lda #0
    sta snd_playing
    rts

@fm_play_note:
    sta snd_tmp0                ; note cmd ($00=rest, $01..$54=MIDI, $55=hold)
    iny
    lda (snd_ptr),y             ; duration in frames
    sta snd_fm_wait,x

    ; Advance 16-bit stream pointer by 2
    lda snd_fm_ptr_lo,x
    clc
    adc #2
    sta snd_fm_ptr_lo,x
    bcc :+
    inc snd_fm_ptr_hi,x
:
    lda snd_tmp0
    cmp #$55
    bne :+
    jmp @fm_next_ch             ; Hold current note without re-keyon
:
    cmp #0
    beq @fm_rest

    ; Key-on: load F-num low/high from BANK9 84-note table
    tay
    lda arc_fm_fnum_lo,y
    sta snd_fm_fnum_lo,x
    lda arc_fm_fnum_hi,y
    sta snd_fm_fnum_hi,x

    ; If ch5 is preempted by SE, keep shadow updated but do not touch hardware.
    cpx #5
    bne @fm_hw_keyon
    lda snd_se_fm_mask
    and #$20
    bne @fm_next_ch

@fm_hw_keyon:
    ; Key-off first so consecutive notes re-trigger attack cleanly
    txa
    ora #$20
    ldy #0
    jsr fm_write

    ldx snd_ch
    txa
    ora #$10
    ldy snd_fm_fnum_lo,x
    jsr fm_write

    ldx snd_ch
    txa
    ora #$20
    ldy snd_fm_fnum_hi,x
    jsr fm_write
    jmp @fm_next_ch

@fm_rest:
    lda #0
    sta snd_fm_fnum_hi,x
    cpx #5
    bne @fm_hw_keyoff
    lda snd_se_fm_mask
    and #$20
    bne @fm_next_ch
@fm_hw_keyoff:
    txa
    ora #$20
    ldy #0
    jsr fm_write

@fm_next_ch:
    inc snd_ch
    lda snd_ch
    cmp #snd_fm_channels
    beq @fm_done
    jmp @fm_ch_loop
@fm_done:
    rts

; Update APU percussion (drum) track: $00=rest, $01=kick(Tri), $02=snare(Noise), $03=hihat(Noise)
snd_update_drum_bgm:
    dec snd_drm_wait
    beq :+
    rts
:

    lda snd_drm_ptr_lo
    sta snd_ptr
    lda snd_drm_ptr_hi
    sta snd_ptr+1

    ldy #0
    lda (snd_ptr),y
    cmp #$ff
    bne @drm_check_end
    iny
    lda (snd_ptr),y
    sta snd_drm_ptr_lo
    iny
    lda (snd_ptr),y
    sta snd_drm_ptr_hi
    lda snd_drm_ptr_lo
    sta snd_ptr
    lda snd_drm_ptr_hi
    sta snd_ptr+1
    ldy #0
    lda (snd_ptr),y
@drm_check_end:
    cmp #$fe
    beq @drm_done

    sta snd_tmp0
    iny
    lda (snd_ptr),y
    sta snd_drm_wait

    lda snd_drm_ptr_lo
    clc
    adc #2
    sta snd_drm_ptr_lo
    bcc :+
    inc snd_drm_ptr_hi
:
    lda snd_tmp0
    beq @drm_done
    cmp #1
    beq @drm_kick
    ; Snare (2) or Hi-hat (3) on APU Noise (skip if SE owns Noise)
    lda snd_se_apu_mask
    and #$08
    bne @drm_done
    lda snd_tmp0
    cmp #2
    beq @drm_snare
    ; Hi-hat: short crisp noise
    lda #$15
    sta $400c
    lda #$02
    sta $400e
    lda #$08
    sta $400f
    rts
@drm_snare:
    lda #$19
    sta $400c
    lda #$07
    sta $400e
    lda #$10
    sta $400f
    rts
@drm_kick:
    ; Short punchy low triangle kick
    lda #$0a
    sta $4008
    lda #$90
    sta $400a
    lda #$02
    sta $400b
@drm_done:
    rts

; ---------------------------------------------------------------------------
; Sound Driver Data Tables (in BANK8)
; ---------------------------------------------------------------------------

; Two 8-byte VRC7 custom FM patches ($00..$07):
;   Patch 0: Arcade Makaimura bright gothic lead (brass/organ hybrid)
;   Patch 1: Arcade Boss sharp biting FM brass
snd_custom_patches:
    .byte $21, $21, $18, $07, $f4, $f2, $24, $23  ; Patch 0: stage lead
    .byte $31, $21, $14, $0f, $f5, $f3, $13, $13  ; Patch 1: boss brass

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
