; Bank 8 ($C000-$DFFF): VRC7a FM 6-voice + APU BGM/SE sound driver.
; Uses WRAM_SND ($7800+) and calls fixed-bank fm_write so the hardware
; $9010 (>=6 cycles) and $9030 (>=42 cycles) wait rules are always respected.

snd_song_none     = 0
snd_song_title    = 1
snd_song_stage    = 2
snd_song_boss     = 3
snd_song_clear    = 4
snd_song_gameover = 5

snd_se_none       = 0
snd_se_shot       = 1
snd_se_damage     = 2
snd_se_key        = 3

snd_fm_channels   = 6
snd_apu_channels  = 3   ; 0=Pulse1, 1=Triangle, 2=Noise

; Sequence stream commands:
;   $00..$7F : note index (0 = rest, 1..24 = pitch table index), followed by 1-byte duration (frames)
;   $FE      : end of track (stop playback, non-looping jingles)
;   $FF      : loop back to start of track

.segment "WRAM_SND"
snd_song_id:        .res 1                  ; $7800
snd_playing:        .res 1                  ; $7801
snd_custom_patch:   .res 1                  ; $7802 ($ff = unloaded)
snd_se_id:          .res 1                  ; $7803
snd_se_timer:       .res 1                  ; $7804
snd_se_fm_mask:     .res 1                  ; $7805 (bit 5 = FM ch5 preempted by SE)
snd_se_apu_mask:    .res 1                  ; $7806 (bit 1 = Pulse2, bit 3 = Noise)
snd_tick_count:     .res 1                  ; $7807
snd_fm_pos:         .res snd_fm_channels    ; $7808..$780D
snd_fm_wait:        .res snd_fm_channels    ; $780E..$7813
snd_fm_inst:        .res snd_fm_channels    ; $7814..$7819 ($30+ch shadow)
snd_fm_fnum_lo:     .res snd_fm_channels    ; $781A..$781F ($10+ch shadow)
snd_fm_fnum_hi:     .res snd_fm_channels    ; $7820..$7825 ($20+ch shadow)
snd_apu_pos:        .res snd_apu_channels   ; $7826..$7828
snd_apu_wait:       .res snd_apu_channels   ; $7829..$782B
snd_tmp0:           .res 1                  ; $782C
snd_tmp1:           .res 1                  ; $782D
snd_ch:             .res 1                  ; $782E

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

; A = song ID (0..5). Resets all voices, loads the song's shared FM custom
; patch into $00..$07 if changed, and initializes all 6 FM + 3 APU tracks.
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
    cmp #6
    bcs @stop_song

    ; Load shared FM custom patch ($00..$07) for this song if needed.
    tax
    lda snd_song_patch_id,x
    cmp snd_custom_patch
    beq @patch_ready
    sta snd_custom_patch
    jsr snd_load_custom_patch
@patch_ready:
    ; Initialize FM channel instruments & stream cursors.
    ldx snd_song_id
    lda snd_song_inst_base,x
    sta snd_tmp1
    ldx #0
@init_fm_ch:
    lda #0
    sta snd_fm_pos,x
    sta snd_fm_fnum_lo,x
    sta snd_fm_fnum_hi,x
    lda #1
    sta snd_fm_wait,x
    ldy snd_tmp1
    lda snd_inst_presets,y
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

    ; Initialize APU tracks (Pulse1, Triangle, Noise).
    ldx #0
@init_apu_ch:
    lda #0
    sta snd_apu_pos,x
    lda #1
    sta snd_apu_wait,x
    inx
    cpx #snd_apu_channels
    bne @init_apu_ch

    ; Enable APU Pulse1 ($01), Triangle ($04), Noise ($08) = $0D.
    lda #$0d
    jsr set_apu_channels
    lda #1
    sta snd_playing
    rts

@stop_song:
    lda #0
    sta snd_playing
    rts

; A = custom patch index (0 = stage gothic lead, 1 = boss sharp brass).
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
    jsr snd_update_fm_bgm
    jsr snd_update_apu_bgm
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
    ; Sweep FM ch5 pitch down for armor-break impact.
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
    ; Rising 3-note chime on FM ch5 (every 6 frames).
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
    ; Compute pointer to song's FM track table: index = song_id * 6 + ch.
    ldy snd_song_id
    lda snd_song_fm_offset,y
    clc
    adc snd_ch
    tay
    lda snd_fm_track_lo,y
    sta snd_ptr
    lda snd_fm_track_hi,y
    sta snd_ptr+1

    ldx snd_ch
    ldy snd_fm_pos,x
    lda (snd_ptr),y
    cmp #$ff
    bne @fm_check_end
    ; Loop track
    lda #0
    sta snd_fm_pos,x
    tay
    lda (snd_ptr),y
@fm_check_end:
    cmp #$fe
    bne @fm_play_note
    ; Non-looping jingle finished: silence all and stop playback cleanly.
    jsr snd_silence_all
    lda #0
    sta snd_playing
    sta snd_se_id
    sta snd_se_timer
    sta snd_se_fm_mask
    sta snd_se_apu_mask
    rts

@fm_play_note:
    sta snd_tmp0                ; note index (0 = rest)
    iny
    lda (snd_ptr),y
    sta snd_fm_wait,x
    iny
    tya
    sta snd_fm_pos,x

    ; Key-off previous note in shadow and hardware (unless ch5 preempted).
    lda #0
    sta snd_fm_fnum_hi,x
    lda snd_tmp0
    beq @fm_write_keyoff

    ; Look up F-number low/high from pitch table.
    tay
    lda snd_fm_pitch_lo,y
    sta snd_fm_fnum_lo,x
    lda snd_fm_pitch_hi,y
    sta snd_fm_fnum_hi,x

    ; Check SE preemption on ch5.
    cpx #5
    bne @fm_emit_note
    lda snd_se_fm_mask
    and #$20
    bne @fm_next_ch

@fm_emit_note:
    ; Key-off first so repeated notes retrigger cleanly.
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

@fm_write_keyoff:
    cpx #5
    bne @do_keyoff
    lda snd_se_fm_mask
    and #$20
    bne @fm_next_ch
@do_keyoff:
    txa
    ora #$20
    ldy #0
    jsr fm_write

@fm_next_ch:
    inc snd_ch
    lda snd_ch
    cmp #snd_fm_channels
    beq :+
    jmp @fm_ch_loop
:
    rts

; Update APU BGM tracks: Pulse 1 (ch 0), Triangle (ch 1), Noise (ch 2).
snd_update_apu_bgm:
    ; --- Pulse 1 (APU ch 0) ---
    dec snd_apu_wait+0
    bne @apu_tri
    ldx snd_song_id
    lda snd_apu_p1_lo,x
    sta snd_ptr
    lda snd_apu_p1_hi,x
    sta snd_ptr+1
    ldy snd_apu_pos+0
    lda (snd_ptr),y
    cmp #$ff
    bne @p1_check_end
    ldy #0
    lda (snd_ptr),y
@p1_check_end:
    cmp #$fe
    beq @p1_end
    sta snd_tmp0
    iny
    lda (snd_ptr),y
    sta snd_apu_wait+0
    iny
    sty snd_apu_pos+0
    ldy snd_tmp0
    beq @p1_rest
    lda #$75                    ; 25% duty, constant vol 5 (balanced under FM)
    sta $4000
    lda #0
    sta $4001
    lda snd_apu_per_lo,y
    sta $4002
    lda snd_apu_per_hi,y
    sta $4003
    jmp @apu_tri
@p1_end:
    lda #60
    sta snd_apu_wait+0
@p1_rest:
    lda #$30
    sta $4000

@apu_tri:
    ; --- Triangle bass (APU ch 1) ---
    dec snd_apu_wait+1
    bne @apu_noise
    ldx snd_song_id
    lda snd_apu_tri_lo,x
    sta snd_ptr
    lda snd_apu_tri_hi,x
    sta snd_ptr+1
    ldy snd_apu_pos+1
    lda (snd_ptr),y
    cmp #$ff
    bne @tri_check_end
    ldy #0
    lda (snd_ptr),y
@tri_check_end:
    cmp #$fe
    beq @tri_end
    sta snd_tmp0
    iny
    lda (snd_ptr),y
    sta snd_apu_wait+1
    iny
    sty snd_apu_pos+1
    ldy snd_tmp0
    beq @tri_rest
    lda #$ff
    sta $4008
    lda snd_apu_per_lo,y
    sta $400a
    lda snd_apu_per_hi,y
    sta $400b
    jmp @apu_noise
@tri_end:
    lda #60
    sta snd_apu_wait+1
@tri_rest:
    lda #$00
    sta $4008

@apu_noise:
    ; --- Noise percussion (APU ch 2), skipped while damage SE preempts Noise ---
    dec snd_apu_wait+2
    bne @apu_done
    ldx snd_song_id
    lda snd_apu_noi_lo,x
    sta snd_ptr
    lda snd_apu_noi_hi,x
    sta snd_ptr+1
    ldy snd_apu_pos+2
    lda (snd_ptr),y
    cmp #$ff
    bne @noi_check_end
    ldy #0
    lda (snd_ptr),y
@noi_check_end:
    cmp #$fe
    beq @noi_end
    sta snd_tmp0
    iny
    lda (snd_ptr),y
    sta snd_apu_wait+2
    iny
    sty snd_apu_pos+2
    lda snd_se_apu_mask
    and #$08
    bne @apu_done
    ldy snd_tmp0
    beq @noi_rest
    lda #$34                    ; Short crisp percussion
    sta $400c
    lda snd_noi_period,y
    sta $400e
    lda #$08
    sta $400f
    rts
@noi_end:
    lda #60
    sta snd_apu_wait+2
@noi_rest:
    lda #$30
    sta $400c
@apu_done:
    rts

; --- Tables: Shared FM custom patches ($00..$07) ---
; Patch 0: Gothic harpsichord/lead for Stage & Title.
; Patch 1: Sharp aggressive brass/synth for Boss battle.
snd_custom_patches:
    .byte $21, $21, $18, $07, $f2, $f3, $25, $24 ; Patch 0
    .byte $11, $01, $0e, $05, $f1, $f1, $14, $13 ; Patch 1

snd_song_patch_id:
    .byte 0, 0, 0, 1, 0, 0      ; none, title, stage, boss, clear, gameover

snd_song_inst_base:
    .byte 0, 0, 6, 12, 18, 24

; Per-song FM instrument/volume ($30+ch): ch0 uses custom patch (inst 0).
snd_inst_presets:
    ; Title (0..5): custom lead, organ, strings, strings, bass, chime
    .byte $04, $76, $27, $27, $55, $36
    ; Stage (6..11): custom lead, organ, strings, brass, bass, arp
    .byte $03, $75, $26, $66, $54, $35
    ; Boss (12..17): custom brass, overdrive, strings, synth, bass, pulse
    .byte $02, $64, $25, $c5, $53, $74
    ; Clear (18..23): custom lead, brass, strings, strings, bass, chime
    .byte $03, $64, $25, $25, $54, $34
    ; Game Over (24..29): custom lead, organ, strings, strings, bass, chime
    .byte $04, $75, $26, $26, $55, $36

; SE tables
snd_shot_per:  .byte $ea, $c8, $aa, $8c, $71, $58
snd_shot_vol:  .byte $71, $72, $73, $74, $76, $78
snd_dmg_fnum:  .byte $30, $38, $40, $48, $50, $58, $60, $68, $70, $78, $80, $88, $90, $98
snd_dmg_noise: .byte $31, $31, $32, $32, $33, $34, $35, $36, $37, $38, $39, $3a, $3b, $3c
snd_noi_period: .byte $00, $04, $08, $0c

; FM pitch table (index 1..18; $10+ch low, $20+ch key-on + block + MSB).
; D minor / gothic scale spanning octaves 2..4.
snd_fm_pitch_lo:
    .byte $00
    .byte $ad, $b7, $c2, $cd, $d9, $e6, $f4, $02, $12, $22, $34, $46 ; 1..12 (block 2/3)
    .byte $59, $6e, $83, $9a, $b3, $cc                             ; 13..18 (upper)
snd_fm_pitch_hi:
    .byte $00
    .byte $14, $14, $14, $14, $14, $14, $14, $15, $15, $15, $15, $15
    .byte $17, $17, $17, $17, $17, $17

; APU period table (index 1..18, NTSC 12-TET C4..F5 matching FM pitch table).
snd_apu_per_lo:
    .byte $00
    .byte $ab, $93, $7c, $67, $52, $3f, $2d, $1c, $0c, $fd, $ef, $e1
    .byte $d5, $c9, $bd, $b3, $a9, $9f
snd_apu_per_hi:
    .byte $00
    .byte $01, $01, $01, $01, $01, $01, $01, $01, $01, $00, $00, $00
    .byte $00, $00, $00, $00, $00, $00

snd_song_fm_offset:
    .byte 0, 0, 6, 12, 18, 24

snd_fm_track_lo:
    ; Title (0..5)
    .byte <trk_title_0, <trk_title_1, <trk_title_2, <trk_title_3, <trk_title_4, <trk_title_5
    ; Stage (6..11)
    .byte <trk_stage_0, <trk_stage_1, <trk_stage_2, <trk_stage_3, <trk_stage_4, <trk_stage_5
    ; Boss (12..17)
    .byte <trk_boss_0,  <trk_boss_1,  <trk_boss_2,  <trk_boss_3,  <trk_boss_4,  <trk_boss_5
    ; Clear (18..23)
    .byte <trk_clear_0, <trk_clear_1, <trk_clear_2, <trk_clear_3, <trk_clear_4, <trk_clear_5
    ; Game Over (24..29)
    .byte <trk_over_0,  <trk_over_1,  <trk_over_2,  <trk_over_3,  <trk_over_4,  <trk_over_5

snd_fm_track_hi:
    .byte >trk_title_0, >trk_title_1, >trk_title_2, >trk_title_3, >trk_title_4, >trk_title_5
    .byte >trk_stage_0, >trk_stage_1, >trk_stage_2, >trk_stage_3, >trk_stage_4, >trk_stage_5
    .byte >trk_boss_0,  >trk_boss_1,  >trk_boss_2,  >trk_boss_3,  >trk_boss_4,  >trk_boss_5
    .byte >trk_clear_0, >trk_clear_1, >trk_clear_2, >trk_clear_3, >trk_clear_4, >trk_clear_5
    .byte >trk_over_0,  >trk_over_1,  >trk_over_2,  >trk_over_3,  >trk_over_4,  >trk_over_5

snd_apu_p1_lo:  .byte <trk_rest, <trk_title_1, <trk_stage_1, <trk_boss_1, <trk_clear_1, <trk_over_1
snd_apu_p1_hi:  .byte >trk_rest, >trk_title_1, >trk_stage_1, >trk_boss_1, >trk_clear_1, >trk_over_1
snd_apu_tri_lo: .byte <trk_rest, <trk_title_4, <trk_stage_4, <trk_boss_4, <trk_clear_4, <trk_over_4
snd_apu_tri_hi: .byte >trk_rest, >trk_title_4, >trk_stage_4, >trk_boss_4, >trk_clear_4, >trk_over_4
snd_apu_noi_lo: .byte <trk_rest, <trk_noi_slow, <trk_noi_stage, <trk_noi_boss, <trk_rest, <trk_rest
snd_apu_noi_hi: .byte >trk_rest, >trk_noi_slow, >trk_noi_stage, >trk_noi_boss, >trk_rest, >trk_rest

; --- Original compositions (D minor gothic action motif) ---
trk_rest:
    .byte 0, 60, $ff

; Title theme (solemn gothic organ)
trk_title_0: .byte 13, 24, 15, 24, 16, 24, 15, 24, 14, 24, 13, 24, 11, 48, $ff
trk_title_1: .byte 8, 48, 10, 48, 9, 48, 8, 48, $ff
trk_title_2: .byte 5, 48, 6, 48, 5, 48, 4, 48, $ff
trk_title_3: .byte 3, 96, 1, 96, $ff
trk_title_4: .byte 1, 48, 4, 48, 3, 48, 1, 48, $ff
trk_title_5: .byte 0, 24, 13, 24, 0, 24, 15, 24, $ff

; Stage theme (driving 150 BPM original action loop)
trk_stage_0: .byte 13, 12, 13, 12, 16, 12, 15, 12, 13, 12, 11, 12, 13, 24
             .byte 15, 12, 16, 12, 18, 12, 16, 12, 15, 12, 13, 12, 11, 24, $ff
trk_stage_1: .byte 8, 12, 10, 12, 11, 12, 10, 12, 8, 12, 6, 12, 8, 24
             .byte 10, 12, 11, 12, 13, 12, 11, 12, 10, 12, 8, 12, 6, 24, $ff
trk_stage_2: .byte 5, 24, 6, 24, 5, 24, 4, 24, $ff
trk_stage_3: .byte 3, 24, 4, 24, 3, 24, 1, 24, $ff
trk_stage_4: .byte 1, 12, 1, 12, 4, 12, 4, 12, 3, 12, 3, 12, 1, 12, 5, 12, $ff
trk_stage_5: .byte 13, 12, 16, 12, 18, 12, 16, 12, 11, 12, 15, 12, 16, 12, 15, 12, $ff

; Boss theme (fast urgent ostinato with custom brass patch)
trk_boss_0:  .byte 16, 8, 15, 8, 16, 8, 18, 8, 16, 8, 15, 8, 13, 16
             .byte 15, 8, 14, 8, 15, 8, 16, 8, 14, 8, 13, 8, 11, 16, $ff
trk_boss_1:  .byte 11, 8, 10, 8, 11, 8, 13, 8, 11, 8, 10, 8, 8, 16, $ff
trk_boss_2:  .byte 6, 16, 5, 16, 4, 16, 5, 16, $ff
trk_boss_3:  .byte 3, 16, 2, 16, 1, 16, 2, 16, $ff
trk_boss_4:  .byte 1, 8, 1, 8, 2, 8, 2, 8, 3, 8, 3, 8, 2, 8, 1, 8, $ff
trk_boss_5:  .byte 18, 8, 16, 8, 15, 8, 16, 8, $ff

; Stage Clear fanfare (non-looping)
trk_clear_0: .byte 13, 10, 15, 10, 16, 10, 18, 36, $fe
trk_clear_1: .byte 8, 10, 10, 10, 11, 10, 13, 36, $fe
trk_clear_2: .byte 5, 30, 8, 36, $fe
trk_clear_3: .byte 3, 30, 5, 36, $fe
trk_clear_4: .byte 1, 30, 1, 36, $fe
trk_clear_5: .byte 0, 10, 13, 10, 15, 10, 18, 36, $fe

; Game Over jingle (non-looping)
trk_over_0:  .byte 16, 18, 15, 18, 13, 18, 11, 42, $fe
trk_over_1:  .byte 11, 18, 10, 18, 8, 18, 6, 42, $fe
trk_over_2:  .byte 6, 36, 4, 60, $fe
trk_over_3:  .byte 3, 36, 1, 60, $fe
trk_over_4:  .byte 4, 36, 1, 60, $fe
trk_over_5:  .byte 0, 18, 13, 18, 11, 18, 8, 42, $fe

; Noise percussion patterns
trk_noi_slow:  .byte 1, 24, 0, 24, 2, 24, 0, 24, $ff
trk_noi_stage: .byte 1, 12, 0, 12, 2, 12, 1, 12, $ff
trk_noi_boss:  .byte 1, 8, 2, 8, 1, 8, 3, 8, $ff
