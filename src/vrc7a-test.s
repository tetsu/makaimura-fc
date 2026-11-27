.setcpu "6502"

music_fm = 1
music_apu = 2
music_both = music_fm | music_apu
music_muted = 4

.segment "HEADER"
; NES 2.0, mapper $055, submapper 2, 8 KiB each of WRAM and CHR-RAM.
.ifndef PRG_ROM_UNITS
PRG_ROM_UNITS = 2
.endif
.byte "NES", $1a, PRG_ROM_UNITS, 0, $50, $58, $20, 0, 7, 7, 0, 0, 0, 0

fixed_bank = PRG_ROM_UNITS * 2 - 1
bank_tag_base = $80

; Every 8 KiB bank begins with its own tag, including banks with no game data.
.segment "BANK0"
.byte bank_tag_base + 0
.segment "BANK1"
.byte bank_tag_base + 1
.segment "BANK2"
.byte bank_tag_base + 2
.if fixed_bank > 3
; Bank 5 holds the later stage songs (arcade-music-data.inc).
.segment "BANK6"
    .byte bank_tag_base + 6
.segment "BANK49"
    .byte bank_tag_base + 49
.segment "BANK50"
    .byte bank_tag_base + 50
.segment "BANK51"
    .byte bank_tag_base + 51
.segment "BANK56"
    .byte bank_tag_base + 56
.ifdef NES_AREA_ART
.include "nes-areas.inc"
.else
.segment "PAD_11"
.ifdef REVG_STAGE1
.include "revg-map-data.inc"
revg_map_bank
.else
    .byte bank_tag_base + 11
    .res $1fff, $ff
.endif
.segment "PAD_19_48"
.ifdef REVG_STAGE1
; Stage 1 background art from bank 19 (revg-background.inc reads it through $C000).
.include "revg-section0-bg.inc"
.include "revg-sprite-data.inc"
.include "revg-frontend-data.inc"
.include "revg-section1-bg.inc"
.include "revg-section2-bg.inc"
.include "revg-section6-bg.inc"
revg_bg_banks
.reloc
; The Stage 1-7 sprites (revg-objects.inc) follow from bank 24 (7 banks: 5 tile + 2 meta).
.assert revg_spr_tile_bank = 19 + revg_bg_bank_count, error, "Sprite banks must follow the background"
revg_spr_banks
; The title and the opening demo (revg-opening.inc) after them (bank 31).
revg_frontend_banks
; Stage 2 background art follows from bank 32 (7 banks: 32..38).
.assert revg2_bg_map_bank = 19 + revg_bg_bank_count + revg_spr_bank_count + revg_frontend_bank_count, error, "Stage 2 BG banks must follow frontend"
revg2_bg_banks
.reloc
; Stage 3 background art follows from bank 39 (8 banks: 39..46).
.assert revg3_bg_map_bank = revg2_bg_map_bank + revg2_bg_bank_count, error, "Stage 3 BG banks must follow Stage 2 BG"
revg3_bg_banks
.reloc
; Stage 7 background art follows from bank 47 (2 banks: 47..48).
.assert revg7_bg_map_bank = revg3_bg_map_bank + revg3_bg_bank_count, error, "Stage 7 BG banks must follow Stage 3 BG"
revg7_bg_banks
.reloc
.repeat 30 - revg_bg_bank_count - revg_spr_bank_count - revg_frontend_bank_count - revg2_bg_bank_count - revg3_bg_bank_count - revg7_bg_bank_count, index
    .byte bank_tag_base + revg7_bg_map_bank + revg7_bg_bank_count + index
    .res $1fff, $ff
.endrepeat
.else
.repeat 30, index
    .byte bank_tag_base + 19 + index
    .res $1fff, $ff
.endrepeat
.endif
.segment "PAD_52_55"
.ifdef REVG_STAGE1
.include "revg-section3-bg.inc"
revg4_bg_banks
.reloc
.else
.repeat 4, index
    .byte bank_tag_base + 52 + index
    .res $1fff, $ff
.endrepeat
.endif
.segment "PAD_57_62"
.ifdef REVG_STAGE1
.include "revg-section4-bg.inc"
.include "revg-section5-bg.inc"
revg5_bg_banks
.reloc
.assert revg6_bg_map_bank = revg5_bg_map_bank + revg5_bg_bank_count, error, "Stage 6 BG banks must follow Stage 5 BG"
revg6_bg_banks
.reloc
.else
.repeat 6, index
    .byte bank_tag_base + 57 + index
    .res $1fff, $ff
.endrepeat
.endif
.endif
.endif
.segment "FIXED_TAG"
.byte bank_tag_base + fixed_bank

.segment "ZEROPAGE"
pad: .res 1
previous: .res 1
mode: .res 1
frames: .res 1
channel: .res 1
phrase: .res 1
note_index: .res 1
scene: .res 1
pressed: .res 1
pan_y: .res 1          ; Current Y within a 240 px nametable.
apu_channels: .res 1
shot_sound_pending: .res 1
shot_sound_frames: .res 1
prg_bank0: .res 1
prg_bank1: .res 1
prg_bank2: .res 1
map_ptr: .res 2
map_tmp: .res 1
map_cached_col: .res 1
.if PRG_ROM_UNITS = 32
campaign_stage: .res 1 ; Zero in diagnostics, otherwise the 1-based stage number.
campaign_lives: .res 1
campaign_subsecond: .res 1
campaign_clock: .res 3 ; Decimal digits stored as tile IDs for VBlank.
campaign_hud_dirty: .res 1
arcade_mode: .res 1    ; Non-zero when using the arcade stage data and visuals.
snd_ptr: .res 2        ; Bank 8 sound driver stream pointer.
.endif

.ifdef NES_AREA_TRIAL
nes_area_last_trial_bank = nes_area_trial_bank + nes_area_trial_count - 1
; Carry is clear when Bank 0 maps a manifest course.
.macro cmp_trial_bank
    lda prg_bank0
    sec
    sbc #nes_area_trial_bank
    cmp #nes_area_trial_count
.endmacro
.segment "CODE"
check_trial_bank:
    cmp_trial_bank
    rts
.else
nes_area_trial_count = 0
.endif

.include "fixed-hud.inc"
.include "chr-layout.inc"
.include "scenery.inc"
.include "terrain-data.inc"
.include "horizontal-scroll.inc"
.include "two-axis-scroll.inc"
.include "playable-scene.inc"
.if PRG_ROM_UNITS = 32
.include "campaign.inc"
.include "sound-driver.inc"
.include "arcade-gfx-data.inc"
.include "arcade-music-data.inc"
.include "arcade-stage.inc"
.include "arcade-stage2.inc"
.include "arcade-stage3.inc"
.include "arcade-stage4.inc"
.include "arcade-stage5.inc"
.include "arcade-stage6.inc"
.include "arcade-stage7.inc"
.include "arcade-player.inc"
.include "arcade-enemy.inc"
.include "arcade-draw.inc"
.include "sound-test.inc"
.include "sprite-view.inc"
.ifdef NES_AREA_ART
.include "area-view.inc"
.endif
.endif

.if PRG_ROM_UNITS = 32
title_scene = 7
stage_view_scene = 8
sound_test_scene = 9
sprite_view_scene = 10
.ifdef NES_AREA_ART
area_view_scene = 11
.endif
.ifdef REVG_STAGE1
opening_scene = 12              ; Scenes from here on run in Bank 2 (frontend.inc).
map_scene = 13
name_entry_scene = 15
.endif
option_scene = 14
.endif

.segment "CODE"
reset:
    sei
    cld
    ldx #$ff
    txs
    inx
    stx $2000
    stx $2001
    stx $4010
    stx $4015
    stx $f000
    lda #$40
    sta $4017
    lda #$c0
    sta $e000
    lda #$80
    sta $e000
    bit $2002
    jsr wait_frame
    jsr wait_frame

    lda #0
    jsr select_bank0
    lda #1
    jsr select_bank1
    lda #2
    jsr select_bank2
    lda #0
    sta $a000
    lda #1
    sta $a010
    lda #2
    sta $b000
    lda #3
    sta $b010
    lda #4
    sta $c000
    lda #5
    sta $c010
    lda #6
    sta $d000
    lda #7
    sta $d010
    jsr draw_screen
    lda $a000
    cmp #bank_tag_base + 1
    beq :+
    jmp memory_error
:
    lda $c000
    cmp #bank_tag_base + 2
    beq :+
    jmp memory_error
:
    lda $e000
    cmp #bank_tag_base + fixed_bank
    beq :+
    jmp memory_error
:
    ldx #0
check_bank:
    stx $8000
    txa
    ora #bank_tag_base
    cmp $8000
    bne memory_error
    inx
    cpx #fixed_bank + 1
    bne check_bank
    lda #0
    jsr select_bank0
    lda $6000            ; Preserve the byte on battery-backed donor boards.
    tax
    lda #$96
    sta $6000
    cmp $6000
    bne wram_error
    lda #$69
    sta $6000
    cmp $6000
    bne wram_error
    stx $6000
    lda #0
    sta previous
    sta mode
    sta frames
    sta channel
    sta phrase
    sta scene
    sta apu_channels
    sta shot_sound_pending
    sta shot_sound_frames
    sta render_pending
.if PRG_ROM_UNITS = 32
    sta campaign_stage
    sta arcade_mode
    sta game_paused
    sta demo_active
    sta demo_finish
    sta snd_lock
    sta snd_nmi_missed
    sta in_play_step
    sta title_cursor
    sta opt_cursor
    sta opt_invincible
    sta opt_start_loop
    sta opt_start_weapon
    sta opt_no_timer
    sta opt_lives_idx
    sta opt_return_to
    lda #1
    sta opt_start_stage
    lda #0
.ifdef REVG_STAGE1
    sta op_stage_song
.endif
    jsr show_title_screen
    jmp main
.else
    jsr enable_static_screen
    jmp main
.endif

wram_error:
    stx $6000
memory_error:
    lda #$22
    sta $2006
    lda #$07
    sta $2006
    ldx #0
:
    lda error_text,x
    sta $2007
    inx
    cpx #error_length
    bne :-
    jsr enable_screen
:
    jmp :-

enable_screen:
    jsr wait_frame
    lda #0
    sta $2005
    sta $2005
    lda #$0a
    sta $2001
    rts

main:
    jsr read_pad
    lda previous
    eor #$ff
    and pad
    ldx pad
    stx previous
    sta pressed
.ifdef REVG_STAGE1
    lda attract_active
    beq @no_attract
    lda pressed
    bne @end_attract
    lda player_hit
    bne @end_attract
    jsr step_attract_demo
    bcc @no_attract
@end_attract:
    lda #0
    sta attract_active
    sta pad
    sta pressed
    jsr restore_attract_opts
    jsr blank_display
    jsr show_title_screen
    jmp audio_tick
@no_attract:
.endif
    lda pressed
    and #$20            ; Select: switch between audio controls and terrain.
    beq same_scene
.ifdef REVG_STAGE1
    lda scene
    cmp #name_entry_scene
    beq same_scene      ; Ignore SELECT during Name Entry so initials are not abandoned.
.endif
    jsr toggle_scene
    jmp audio_tick
same_scene:
.if PRG_ROM_UNITS = 32
    lda scene
.ifdef NES_AREA_ART
    cmp #area_view_scene
    bne :+
    jsr update_area_view
    jmp main
:
.endif
    cmp #sprite_view_scene
    bne :+
    jsr update_sprite_view
    jmp main
:
    cmp #sound_test_scene
    bne :+
    lda #3
    jsr select_bank1
    jsr update_sound_test
    jsr call_bank8_audio_tick
    jmp main
:
    cmp #stage_view_scene
    bne :+
    jsr update_stage_view
    jmp main
:
    cmp #option_scene
    bne :+
    jsr update_option_screen
    jmp main
:
.ifdef REVG_STAGE1
    cmp #name_entry_scene
    bne :+
    jsr update_name_entry_screen
    jmp main
:
    cmp #opening_scene
    bcc :+
    jsr update_frontend_scene
    jmp main
:
.endif
.endif
.ifdef REVG_STAGE1
    lda demo_finish
    beq :+
    lda #0
    sta demo_finish
    sta demo_active
    jsr op_finish_to_play
    jmp audio_tick
:
    ; After a death with lives left, show the arcade Makai Progression Map ("PLAYER ONE READY")
    ; with the $30 jingle, then auto-restart the stage (or skip immediately with START).
    lda arcade_mode
    beq @no_auto_restart
    lda scene
    cmp #4
    bne @no_auto_restart
    lda player_hit
    bne :+
    sta death_timer
    sta death_timer+1
    beq @no_auto_restart
:
    inc death_timer
    bne :+
    inc death_timer+1
:
    lda death_timer+1
    cmp #>45
    bcc @no_auto_restart
    lda death_timer
    cmp #<45
    bcc @no_auto_restart
    lda #0
    sta death_timer
    sta death_timer+1
    jsr show_map_ready
    jmp audio_tick
@no_auto_restart:
.endif
    lda pressed
    and #$10            ; Start: enter or restart the playable scene.
.ifdef REVG_STAGE1
    bne :+
    lda game_paused
    beq @not_paused
    jsr wait_terrain_frame  ; Paused: the screen stays, nothing moves, no sound.
    jmp main
@not_paused:
    jmp dispatch_scene
:
    jsr toggle_pause        ; Start during play pauses and resumes.
    bcc :+
    jmp main
:
.else
    beq dispatch_scene
.endif
.if PRG_ROM_UNITS = 32
    lda campaign_stage
    beq check_campaign_start
    lda scene
    cmp #clear_scene
    beq advance_campaign
    cmp #game_over_scene
    beq finish_campaign
    lda player_hit
    bne :+
    jmp dispatch_scene
:
    jsr reset_campaign_clock
    jmp start_pressed
finish_campaign:
    jsr blank_display
    lda arcade_mode
    beq @finish_to_sound
.ifdef REVG_STAGE1
    lda attract_active
    bne @finish_to_title
    jsr check_highscore_b2
    bcc @finish_to_title
    jsr show_name_entry_screen
    jmp audio_tick
@finish_to_title:
.endif
    jsr show_title_screen
    jmp audio_tick
@finish_to_sound:
    jsr show_sound_screen
    jmp audio_tick
advance_campaign:
.ifdef REVG_STAGE1
    lda fe_map_mode
    beq @adv_next_stage
    lda #0
    sta fe_map_mode
    jsr retry_campaign_stage
    jmp audio_tick
@adv_next_stage:
.endif
    jsr next_campaign_stage
    jmp audio_tick
check_campaign_start:
    lda scene
    cmp #title_scene
    beq @title_start
    cmp #0
    bne start_pressed
    lda pad
    and #$08            ; Hold Up and press Start on the menu to start the campaign.
    bne @begin_campaign
    lda pad
    and #$01            ; Hold Right and press Start to enter Bank 3 arcade mode.
    beq start_pressed
@begin_arcade:
    jsr begin_arcade_game
    jmp audio_tick
@title_start:
    lda title_cursor
    beq :+
    jsr show_option_screen
    jmp audio_tick
:
    lda pad
    and #$08            ; Hold Up and press Start on the title for legacy campaign.
    bne @begin_campaign
.ifdef REVG_STAGE1
    lda pad
    and #$01            ; Right+START skips the opening and the map (Mesen tests).
    bne @begin_arcade
    lda opt_start_stage
    cmp #1
    bne @begin_arcade
    lda opt_start_loop
    bne @begin_arcade
    lda #1              ; (the opening starts its own music, op_tick)              ; The opening demo, then Stage 1 (revg-opening.inc).
    sta demo_active
    jsr begin_arcade_game
    jmp audio_tick
.else
    jmp @begin_arcade
.endif
@begin_campaign:
    lda #0
    sta arcade_mode
    lda #1
    sta campaign_stage
    lda #campaign_initial_lives
    sta campaign_lives
    jsr start_campaign_stage
    jmp audio_tick
start_pressed:
.endif
    jsr show_game
    jmp audio_tick
dispatch_scene:
    lda scene
    bne :+
    jmp sound_screen
:
    cmp #3
    bne :+
    jmp pan_screen
:
    cmp #2
    beq game_screen
    cmp #clear_scene
    beq clear_screen_idle
.if PRG_ROM_UNITS = 32
    cmp #game_over_scene
    beq clear_screen_idle
    cmp #title_scene
    bne :+
    jmp title_screen_tick
:
.endif
    cmp #4
    beq game_screen
    jmp free_camera
game_screen:
.if PRG_ROM_UNITS = 32
    lda #1
    sta in_play_step
.endif
    jsr update_game
.if PRG_ROM_UNITS = 32
    lda #0
    sta in_play_step
.endif
    jmp audio_tick
clear_screen_idle:
.ifdef REVG_STAGE1
    lda fe_end_active
    beq @csi_wait
    lda #2
    jsr select_bank2
    jsr fe_ending_tick
    bcs @csi_adv
    jsr submit_frame
    jmp @csi_audio
@csi_adv:
    jmp advance_campaign
@csi_wait:
.endif
    jsr wait_terrain_frame
.ifdef REVG_STAGE1
    lda arcade_mode
    bne :+
    jmp @csi_audio
:
    lda scene
    cmp #clear_scene
    beq :+
    jmp @csi_chk_gameover
:
    lda #2
    jsr select_bank2
    jsr fe_map_tick
    bcc @csi_audio
    jmp advance_campaign
@csi_chk_gameover:
    cmp #game_over_scene
    bne @csi_audio
    lda #2
    jsr select_bank2
    jsr fe_gameover_tick
    bcc @csi_audio
    jmp finish_campaign
@csi_audio:
.endif
    jmp audio_tick
.if PRG_ROM_UNITS = 32
title_screen_tick:
    lda pressed
.ifdef REVG_STAGE1
    beq @title_idle_tick
    lda #0
    sta attract_idle_lo
    sta attract_idle_hi
    lda pressed
@title_idle_tick:
.endif
    and #$0c            ; Up / Down toggles 1P START <-> OPTION
    beq @title_chk_a
    lda title_cursor
    eor #1
    sta title_cursor
    jsr submit_frame
    jmp audio_tick
@title_chk_a:
    lda title_cursor
    beq @title_wait
    lda pressed
    and #$80            ; A opens OPTION when highlighted
    beq @title_wait
    jsr show_option_screen
    jmp audio_tick
@title_wait:
.ifdef REVG_STAGE1
    inc attract_idle_lo
    bne :+
    inc attract_idle_hi
:
    lda attract_idle_hi
    cmp #>600
    bne :+
    lda attract_idle_lo
    cmp #<600
    bcc :+
    jsr start_attract_demo
    jmp audio_tick
:
.endif
    jsr wait_terrain_frame
    jmp audio_tick
.endif
free_camera:
    lda pad
    and #3
    jsr step_camera
    jsr submit_frame
    jmp audio_tick
pan_screen:
    jsr advance_pan
    jmp audio_tick

; NMI owns the staged PPU and OAM data until this routine returns.
commit_frame:
    lda scene
    cmp #3
    beq commit_pan_preview
    cmp #4
    bne commit_horizontal
.if PRG_ROM_UNITS = 32
    lda campaign_stage
    beq commit_pan_game
    lda campaign_hud_dirty
    beq commit_pan_game
    jsr upload_campaign_hud
commit_pan_game:
.endif
    jsr upload_pan
    jmp commit_render
commit_horizontal:
.if PRG_ROM_UNITS = 32
.ifdef NES_AREA_ART
    cmp #area_view_scene
    bne :+
    jmp upload_sprites
:
.endif
    cmp #stage_view_scene
    beq commit_pan_preview
    cmp #sound_test_scene
    bne :+
    jmp commit_sound_test
:
    cmp #sprite_view_scene
    bne :+
    jmp commit_sprite_view
:
    cmp #title_scene
    bne :+
    jmp commit_title_cursor
:
    cmp #option_scene
    bne :+
    jmp commit_option_screen
:
.ifdef REVG_STAGE1
    cmp #name_entry_scene
    beq @commit_b2_sc
    cmp #clear_scene
    bne :+
@commit_b2_sc:
    jmp commit_name_entry
:
.endif
.endif
    lda column_pending
    beq commit_render
    lda #$84
    jsr upload_column
commit_render:
    jsr apply_camera
    lda scene
    cmp #2
    beq :+
    cmp #4
    bne :++
:
    jmp upload_sprites
:
    rts
commit_pan_preview:
    jsr upload_pan
    jmp apply_pan

sound_screen:
    lda pressed
    and #$80           ; A opens the two-axis camera from the audio screen.
    beq @no_a
.if PRG_ROM_UNITS = 32
    lda pad
    and #$08           ; Up+A previews the stage banks.
    beq @check_sound_test
    jsr silence_music
    lda #arcade_stage_bank
    jsr show_stage_view
    jmp main
@check_sound_test:
    lda pad
    and #$04           ; Down+A opens the Bank 8 sound test.
    beq @check_sprite_view
    lda #3
    jsr select_bank1
    jsr show_sound_test
    jmp main
@check_sprite_view:
    lda pad
    and #$01           ; Right+A opens the sprite viewer.
    beq @legacy_pan
    lda #3
    jsr select_bank1
    jsr show_sprite_view
    jmp main
@legacy_pan:
.ifdef NES_AREA_ART
    lda pad
    and #$02           ; Left+A opens the manifest area viewer.
    beq @show_pan
    lda #nes_area_viewer_bank
    jsr select_bank1
    jsr show_area_view
    jmp main
@show_pan:
.endif
.endif
    jsr show_pan
    jmp audio_tick
@no_a:
    lda pressed
    and #$40           ; B starts the two-axis course directly from the menu.
    beq :+
    jsr start_course
    jmp audio_tick
:
    jsr wait_terrain_frame
    lda pressed
    and #$0f
    beq audio_tick
    tay
    ldx #music_fm       ; left: FM
    and #$02
    bne select_mode
    ldx #music_apu      ; right: APU
    tya
    and #$01
    bne select_mode
    ldx #music_both     ; down: both
    tya
    and #$04
    bne select_mode
    ldx #music_muted    ; up: mute music
select_mode:
    stx mode
    jsr silence_music
    lda #0
    sta phrase
    sta frames
audio_tick:
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    bne @bank8_audio_tick
    lda scene
    cmp #title_scene
    beq @bank8_audio_tick
    cmp #option_scene
    beq @bank8_audio_tick
.ifdef REVG_STAGE1
    cmp #name_entry_scene
    beq @bank8_audio_tick
.endif
    bne @legacy_audio_tick
@bank8_audio_tick:
    jsr call_bank8_audio_tick
    jmp main
@legacy_audio_tick:
.endif
    jsr tick_shot_sound
    lda frames
    beq play
    dec frames
    jmp main
play:
    lda mode
    beq play_solo
    cmp #music_muted
    bne play_phrase
    jmp main
play_solo:
    jsr silence_music
    ldx channel
    txa
    tay
    jsr note_on
    inc channel
    lda channel
    cmp #6
    bne solo_wait
    lda #music_both
    sta mode
solo_wait:
    lda #15
    sta frames
    jmp main

play_phrase:
    jsr silence_music
    lda mode
    and #music_fm
    beq play_apu
    lda phrase
    tay
    ldx #0
:
    jsr note_on
    iny
    inx
    cpx #6
    bne :-
play_apu:
    lda mode
    and #music_apu
    beq next_phrase
    lda apu_channels
    ora #1
    jsr set_apu_channels
    lda #$b5
    sta $4000
    lda #0
    sta $4001
    ldy phrase
    lda pulse_period,y
    sta $4002
    lda #1
    sta $4003
next_phrase:
    lda phrase
    clc
    adc #6
    cmp #18
    bcc :+
    lda #0
:
    sta phrase
    lda #29
    sta frames
    jmp main

; X = channel (0–5), Y = note index. Block 3, instrument 1.
note_on:
    sty note_index
    txa
    ora #$30
    ldy #$17
    jsr fm_write
    ldy note_index
    lda note_low,y
    tay
    txa
    ora #$10
    jsr fm_write
    ldy note_index
    lda note_high,y
    tay
    txa
    ora #$20
    jsr fm_write
    ldy note_index
    rts

silence_music:
    lda apu_channels
    and #2
    jsr set_apu_channels
    ldx #5
:
    txa
    ora #$20
    ldy #0
    jsr fm_write
    dex
    bpl :-
    rts

; Pulse 2 is reserved for the shot. Keep it enabled when music retriggers
; the VRC7 voices or changes the note on pulse 1.
tick_shot_sound:
    lda shot_sound_pending
    beq shot_sound_playing
    lda #0
    sta shot_sound_pending
    sta $4005
    lda apu_channels
    ora #2
    jsr set_apu_channels
    lda #0
    sta $4007
    lda #shot_sound_length
    sta shot_sound_frames
shot_sound_playing:
    ldx shot_sound_frames
    beq mute_shot_channel
    lda shot_sound_level-1,x
    sta $4004
    lda shot_sound_period-1,x
    sta $4006
    dec shot_sound_frames
    rts

stop_shot_sound:
    lda #0
    sta shot_sound_pending
    sta shot_sound_frames
mute_shot_channel:
    lda apu_channels
    and #2
    beq shot_channel_muted
    lda apu_channels
    and #1
    jmp set_apu_channels
shot_channel_muted:
    rts

set_apu_channels:
    sta apu_channels
    sta $4015
    rts

select_bank0:
    sta prg_bank0
    sta $8000
    lda #$ff
    sta map_cached_col
    lda prg_bank0
    rts

select_bank1:
    sta prg_bank1
    sta $8010
    rts

select_bank2:
    sta prg_bank2
    sta $9000
    rts

.if PRG_ROM_UNITS = 32
.ifdef NES_AREA_TRIAL
load_trial_background_banked:
    lda prg_bank1
    pha
    lda #nes_area_viewer_bank
    jsr select_bank1
    jsr load_trial_background
    pla
    jmp select_bank1
load_trial_player_banked:
    lda prg_bank1
    pha
    lda #nes_area_viewer_bank
    jsr select_bank1
    jsr load_trial_player
    pla
    jmp select_bank1
draw_trial_player_banked:
    lda prg_bank1
    pha
    lda #nes_area_viewer_bank
    jsr select_bank1
    jsr draw_trial_player
    pla
    jmp select_bank1
.endif
call_bank3_init:
    lda #3
    jsr select_bank1
    jmp ap_init_stage

call_bank3_update:
    lda #3
    jsr select_bank1
    jmp ap_update_frame

call_bank7_draw_sprites:
    lda #7
    jsr select_bank2
.ifdef REVG_STAGE1
    lda revg_bg_active
    beq :+
    jmp revg_draw_sprites
:
.endif
    jmp ap_draw_sprites

call_bank3_load_stage_visuals:
    lda #3
    jsr select_bank1
    jmp ap_load_stage_visuals

call_bank8_audio_init:
    inc snd_lock
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    jsr snd_init
    jmp snd_unlock_bank8

; Song and effect calls put the caller's $C000 bank back, so Bank 4 and 7 code can use them.
; snd_lock prevents NMI audio re-entrancy while Bank 8 / VRC7 registers are being updated.
call_bank8_play_song:
    inc snd_lock
    sta snd_call_arg
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    lda snd_call_arg
    jsr snd_play_song
    jmp snd_unlock_bank8

call_bank8_play_se:
    inc snd_lock
    sta snd_call_arg
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    lda snd_call_arg
    jsr snd_play_se
snd_unlock_bank8:
    pla
    jsr select_bank2
    dec snd_lock
    lda snd_nmi_missed
    beq @unlock_done
    txa
    pha
    tya
    pha
    jsr nmi_audio_tick
    pla
    tay
    pla
    tax
@unlock_done:
    rts

call_bank8_audio_tick:
    inc snd_lock
    lda #8
    jsr select_bank2
    jsr snd_tick
    dec snd_lock
    rts

; Called on gameplay lag frames from terrain_nmi after HUD view restore: keeps arcade
; BGM/SFX running at 60 Hz without slowing down when enemies crowd the screen.
nmi_audio_tick:
    lda arcade_mode
    beq @skip
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    jsr snd_nmi_entry
    pla
    jmp select_bank2
@skip:
    rts
.endif

; A = register, Y = value. Preserve X and Y.
fm_write:
    sta $9010
    nop
    nop
    nop
    sty $9030
    txa
    pha
    ldx #9
:
    dex
    bne :-
    pla
    tax
    rts

wait_frame:
    bit $2002
    bpl wait_frame
    rts

read_pad:
    lda #1
    sta $4016
    lsr a
    sta $4016
    sta pad
    ldx #8
:
    lda $4016
    lsr a
    rol pad
    dex
    bne :-
    rts

.ifdef REVG_STAGE1
; The title's picture fills the background patterns, so the stage puts back what the
; old title loaded: the text glyphs, then the arcade ground tiles at $0400 (Bank 10).
load_game_patterns:
    jsr load_menu_patterns
    lda prg_bank0
    pha
    lda #arcade_gfx_bank
    jsr select_bank0
    lda #>arcade_bg_chr_vram
    sta $2006
    lda #<arcade_bg_chr_vram
    sta $2006
    lda #<arcade_bg_chr_data
    sta chr_source
    lda #>arcade_bg_chr_data
    sta chr_source+1
    ldx #>arcade_bg_chr_bytes
    lda #<arcade_bg_chr_bytes
    jsr upload_chr_patterns
    pla
    jmp select_bank0
.endif

load_menu_patterns:
.if PRG_ROM_UNITS = 32
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
.endif
    lda #>chr_background_address
    sta $2006
    lda #<chr_background_address
    sta $2006
    lda #<glyphs
    sta chr_source
    lda #>glyphs
    sta chr_source+1
    ldx #>CHR_BACKGROUND_BYTES
    lda #<CHR_BACKGROUND_BYTES
.if PRG_ROM_UNITS = 32
    jsr upload_chr_patterns
    pla
    jmp select_bank2
.else
    jmp upload_chr_patterns
.endif

.if PRG_ROM_UNITS = 32
draw_screen:
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
    jsr draw_screen_b2
    pla
    jmp select_bank2
.segment "BANK2"
draw_screen_b2:
.else
draw_screen:
.endif
    jsr load_menu_patterns
    jsr clear_screen
    ldy #0
next_line:
    lda #$21
    sta $2006
    lda text_address,y
    sta $2006
    ldx text_start,y
:
    lda screen_text,x
    cmp #$ff
    beq :+
    sta $2007
    inx
    bne :-
:
    iny
    cpy #static_line_count
    bne next_line
.if PRG_ROM_UNITS = 32
    lda #$22
    sta $2006
    lda #$68
    sta $2006
    ldx #0
:
    lda campaign_menu,x
    sta $2007
    inx
    cpx #campaign_menu_length
    bne :-
    lda #$22
    sta $2006
    lda #$88
    sta $2006
    ldx #0
:
    lda stage_view_menu,x
    sta $2007
    inx
    cpx #stage_view_menu_length
    bne :-
    lda #$22
    sta $2006
    lda #$a8
    sta $2006
    ldx #0
:
    lda sound_test_menu,x
    sta $2007
    inx
    cpx #sound_test_menu_length
    bne :-
    lda #$22
    sta $2006
    lda #$c8
    sta $2006
    ldx #0
:
    lda sprite_view_menu,x
    sta $2007
    inx
    cpx #sprite_view_menu_length
    bne :-
.ifdef NES_AREA_ART
    lda #$22
    sta $2006
    lda #$e8
    sta $2006
    ldx #0
:
    lda area_view_menu,x
    sta $2007
    inx
    cpx #area_view_menu_length
    bne :-
.ifdef NES_AREA_TRIAL
    lda prg_bank1
    pha
    lda #nes_area_viewer_bank
    jsr select_bank1
    jsr draw_area_trial_menu
    pla
    jmp select_bank1
.endif
.endif
    rts
campaign_menu: .byte $14, $10, $18, $12, $13, $02, $11, $13, $00, $04, $02, $0d, $10, $02, $0a, $08, $0e ; UP+START CAMPAIGN
campaign_menu_length = * - campaign_menu
stage_view_menu: .byte $14, $10, $18, $02, 0, $12, $13, $02, $08, $06, 0, $15, $0a, $06, $16 ; UP+A STAGE VIEW
stage_view_menu_length = * - stage_view_menu
sound_test_menu: .byte $05, $0f, $16, $0e, $17, $02, $00, $12, $0f, $14, $0e, $05, $00, $13, $06, $12, $13 ; DOWN+A SOUND TEST
sound_test_menu_length = * - sound_test_menu
text_address: .byte $08, $68, $88, $a8, $c8, $e8, $48, $28
static_line_count = * - text_address
text_start: .byte 0, line2-screen_text, line3-screen_text, line4-screen_text, line5-screen_text, line6-screen_text, line7-screen_text, line8-screen_text
screen_text: .byte $15, $11, $04, $01, $02, $00, $13, $06, $12, $13, $00, $0d, $06, $0e, $14, $ff ; VRC7A TEST MENU
line2: .byte $0c, $06, $07, $13, $00, $07, $0d, $ff
line3: .byte $11, $0a, $08, $09, $13, $00, $02, $10, $14, $ff
line4: .byte $05, $0f, $16, $0e, $00, $03, $0f, $13, $09, $ff
line5: .byte $12, $06, $0c, $06, $04, $13, $00, $13, $06, $11, $11, $02, $0a, $0e, $ff ; SELECT TERRAIN
line6: .byte $12, $13, $02, $11, $13, $00, $10, $0c, $02, $17, $ff ; START PLAY
line7: .byte $02, $00, $19, $05, $00, $04, $02, $0d, $06, $11, $02, $ff ; A 2D CAMERA
line8: .byte $03, $00, $04, $0f, $14, $11, $12, $06, $00, $00, $05, $0f, $16, $0e, $00, $03, $00, $02, $0c, $13, $ff ; B COURSE  DOWN B ALT
.segment "CODE"
.else
    rts
.endif

.if PRG_ROM_UNITS = 32
show_title_screen:
    lda #0
    jsr select_bank0
    sta campaign_stage
    sta arcade_mode
    sta title_cursor
    sta opt_return_to
.ifdef REVG_STAGE1
    sta game_paused
    sta demo_active
    sta demo_finish
    sta death_timer
    sta death_timer+1
    sta attract_active
    sta attract_idle_lo
    sta attract_idle_hi
    sta attract_prev_pad
    sta pan_y_steps
    sta gate_open_pending
    sta fe_map_mode
    sta fe_idle_lo
    sta fe_idle_hi
.endif
    lda #1
    jsr select_bank1
    lda #title_scene
    sta scene
    jsr call_bank8_audio_init
.ifndef REVG_STAGE1
    lda #snd_song_title
    jsr call_bank8_play_song
.endif
    lda #2
    jsr select_bank2
    jsr draw_title_screen
    jmp enable_static_screen

commit_title_cursor:
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
    jsr title_commit_vram
    jmp commit_bank2_done

show_option_screen:
    jsr blank_display
show_option_screen_blanked:
    lda #option_scene
    sta scene
    jsr call_bank8_audio_init
    lda #2
    jsr select_bank2
    jsr opt_draw_screen
    jmp enable_static_screen

update_option_screen:
    lda #2
    jsr select_bank2
    jsr opt_tick
    beq @opt_wait
    cmp #1
    beq @opt_redraw
    cmp #2
    beq @opt_exit
    ; A == 3: launch Sound Test and return to Option on SELECT.
    lda #1
    sta opt_return_to
    lda #3
    jsr select_bank1
    jmp show_sound_test
@opt_exit:
    jsr blank_display
    jmp show_title_screen
@opt_redraw:
    jsr submit_frame
    lda #snd_se_shot
    jsr call_bank8_play_se
    jmp call_bank8_audio_tick
@opt_wait:
    jsr wait_terrain_frame
    jmp call_bank8_audio_tick

commit_option_screen:
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
    jsr opt_commit_vram
commit_bank2_done:
    pla
    jsr select_bank2
    lda #0
    sta $2005
    sta $2005
    rts

opt_lives_table:
    .byte 3, 5, 9

; Start in the arcade game with the player alive: pause or resume. C set when it did.
toggle_pause:
    lda arcade_mode
    beq @no
.ifdef REVG_STAGE1
    lda demo_active
    beq :+
    lda #1              ; START skips the opening.
    sta demo_finish
    sec
    rts
:
.endif
    lda scene
    cmp #2
    beq :+
    cmp #4
    bne @no
:
    lda player_hit
    bne @no
    lda game_paused
    eor #1
    sta game_paused
    beq @done
    inc snd_lock
    lda #8                  ; Pausing: stop the voices (the driver keys on the next notes).
    jsr select_bank2
    jsr snd_silence_all
    dec snd_lock
@done:
    sec
    rts
@no:
    clc
    rts

begin_arcade_game:
    lda #0
    sta game_paused
    lda #1
    sta arcade_mode
    lda opt_start_stage
    bne :+
    lda #1
:
    sta campaign_stage
    ldx opt_lives_idx
    lda opt_lives_table,x
    sta campaign_lives
    jsr start_arcade_stage
.ifdef REVG_STAGE1
    lda demo_active
    bne :+
    lda #snd_se_start           ; Play arcade START chime even when skipping opening
    jsr call_bank8_play_se
:
.endif
    rts

.ifdef REVG_STAGE1
; A = opening_scene or map_scene.
show_frontend_scene:
    pha
    jsr blank_display
    pla
    sta scene
    cmp #map_scene
    bne :+
    lda #snd_song_map
    jsr call_bank8_play_song
:
    lda #2
    jsr select_bank2
    jsr fe_draw_scene
    jmp enable_static_screen

update_frontend_scene:
    jsr wait_terrain_frame
    lda #2
    jsr select_bank2
    jsr fe_tick
    beq @tick
    cmp #$ff
    beq @start
    jsr show_frontend_scene
    jmp @tick
@start:
    jsr begin_arcade_game
@tick:
    jmp call_bank8_audio_tick

check_highscore_b2:
    lda #2
    jsr select_bank2
    jmp fe_check_highscore

show_name_entry_screen:
    lda #name_entry_scene
    sta scene
    lda #0
    sta campaign_stage
    sta arcade_mode
    lda #snd_song_name_entry
    jsr call_bank8_play_song
    lda #2
    jsr select_bank2
    jsr fe_draw_name_entry
    jmp enable_static_screen

update_name_entry_screen:
    lda #2
    jsr select_bank2
    jsr fe_update_name_entry
    beq @ne_wait
    cmp #2
    beq @ne_done
    jsr submit_frame
    jmp call_bank8_audio_tick
@ne_done:
    jsr blank_display
    jsr show_title_screen
    jmp call_bank8_audio_tick
@ne_wait:
    jsr wait_terrain_frame
    jmp call_bank8_audio_tick

commit_name_entry:
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
    jsr fe_commit_name_entry
    jmp commit_bank2_done

start_attract_demo:
    ; Save user Option settings and run Attract Demo with neutral arcade defaults.
    lda opt_invincible
    sta attract_saved_opts+0
    lda opt_start_loop
    sta attract_saved_opts+1
    lda opt_start_weapon
    sta attract_saved_opts+2
    lda opt_no_timer
    sta attract_saved_opts+3
    lda #0
    sta opt_invincible
    sta opt_start_loop
    sta opt_start_weapon
    sta opt_no_timer
    sta attract_idle_lo
    sta attract_idle_hi
    sta attract_prev_pad
    sta game_paused
    lda #<360
    sta attract_timer_lo
    lda #>360
    sta attract_timer_hi
    lda #1
    sta attract_active
    sta arcade_mode
    sta campaign_stage
    lda #3
    sta campaign_lives
    jsr start_arcade_stage
    ; Attract demo runs silently like the arcade cabinet.
    jmp call_bank8_audio_init

restore_attract_opts:
    lda attract_saved_opts+0
    sta opt_invincible
    lda attract_saved_opts+1
    sta opt_start_loop
    lda attract_saved_opts+2
    sta opt_start_weapon
    lda attract_saved_opts+3
    sta opt_no_timer
    rts

step_attract_demo:
    lda prg_bank2
    pha
    lda #2
    jsr select_bank2
    jsr fe_attract_step
    pla
    php
    jsr select_bank2
    plp
    rts
.endif
.endif

clear_screen:
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    lda #$0f
    sta $2007
    lda #$30
    sta $2007
    lda #$20
    sta $2006
    lda #0
    sta $2006
    ldy #4
    ldx #0
    lda #0
:
    sta $2007
    inx
    bne :-
    dey
    bne :-
    rts

.segment "RODATA"
clear_title: .byte $04, $0f, $14, $11, $12, $06, $00, $04, $0c, $06, $02, $11 ; COURSE CLEAR
clear_title_length = * - clear_title
.if PRG_ROM_UNITS = 32
final_title: .byte $07, $0a, $0e, $02, $0c, $00, $04, $0c, $06, $02, $11, $00 ; FINAL CLEAR
game_over_title: .byte $08, $02, $0d, $06, $00, $0f, $15, $06, $11, $00, $00, $00 ; GAME OVER
.endif
clear_prompt: .byte $12, $13, $02, $11, $13, $00, $10, $0c, $02, $17 ; START PLAY
clear_prompt_length = * - clear_prompt
.if PRG_ROM_UNITS = 32
next_prompt: .byte $12, $13, $02, $11, $13, $00, $12, $13, $08, $1a ; START STG2
arcade_next_prompt: .byte $12, $13, $02, $11, $13, $00, $04, $0f, $0e, $13 ; START CONT
final_prompt: .byte $12, $13, $02, $11, $13, $00, $0d, $06, $0e, $14 ; START MENU
.if nes_area_trial_count > 1
trial_next_prompt: .byte $12, $13, $02, $11, $13, $00, $0e, $06, $18, $13 ; START NEXT
.endif
.endif
clear_menu: .byte $12, $06, $0c, $06, $04, $13, $00, $0d, $06, $0e, $14 ; SELECT MENU
clear_menu_length = * - clear_menu
.if PRG_ROM_UNITS <> 32
text_address: .byte $08, $68, $88, $a8, $c8, $e8, $48, $28
static_line_count = * - text_address
text_start: .byte 0, line2-screen_text, line3-screen_text, line4-screen_text, line5-screen_text, line6-screen_text, line7-screen_text, line8-screen_text
screen_text: .byte $15, $11, $04, $01, $02, $00, $13, $06, $12, $13, $00, $0d, $06, $0e, $14, $ff ; VRC7A TEST MENU
line2: .byte $0c, $06, $07, $13, $00, $07, $0d, $ff
line3: .byte $11, $0a, $08, $09, $13, $00, $02, $10, $14, $ff
line4: .byte $05, $0f, $16, $0e, $00, $03, $0f, $13, $09, $ff
line5: .byte $12, $06, $0c, $06, $04, $13, $00, $13, $06, $11, $11, $02, $0a, $0e, $ff ; SELECT TERRAIN
line6: .byte $12, $13, $02, $11, $13, $00, $10, $0c, $02, $17, $ff ; START PLAY
line7: .byte $02, $00, $19, $05, $00, $04, $02, $0d, $06, $11, $02, $ff ; A 2D CAMERA
line8: .byte $03, $00, $04, $0f, $14, $11, $12, $06, $00, $00, $05, $0f, $16, $0e, $00, $03, $00, $02, $0c, $13, $ff ; B COURSE  DOWN B ALT
.endif
error_text: .byte $03, $02, $0e, $0b, $00, $0f, $11, $00, $11, $02, $0d, $00, $06, $11, $11, $0f, $11
error_length = * - error_text
pulse_period:
    .byte $fc, 0, 0, 0, 0, 0, $d0, 0, 0, 0, 0, 0, $e0
; Table playback goes from the last entry to the first.
shot_sound_period: .byte $ea, $c8, $aa, $8c, $71, $58
shot_sound_length = * - shot_sound_period
shot_sound_level: .byte $71, $72, $73, $74, $76, $78
note_low: .byte $ac, $d9, $02, $59, $b3, $05, $c2, $e6, $22, $83, $cc, $44, $a3, $c2, $02, $46, $83, $05
note_high: .byte $16, $16, $17, $17, $17, $16, $16, $16, $17, $17, $17, $16, $16, $16, $17, $17, $17, $16
.if PRG_ROM_UNITS = 32
.segment "BANK2"
.endif
glyphs:
    .res 16, 0
.ifdef REVG_STAGE1
    ; Authentic Revision G arcade FG font (gg1.bin): plane 0 = bright stroke, plane 1 = 1px drop shadow.
    .byte $1f, $01, $02, $04, $08, $10, $20, $00, $20, $3e, $04, $08, $10, $20, $40, $00 ; 7
    .byte $0c, $14, $24, $24, $7c, $44, $82, $00, $02, $0a, $12, $12, $02, $3a, $44, $83 ; A
    .byte $f8, $44, $78, $44, $42, $62, $9c, $00, $04, $ba, $04, $3a, $25, $11, $62, $9c ; B
    .byte $38, $44, $84, $80, $82, $44, $38, $00, $04, $3a, $60, $40, $41, $a2, $44, $38 ; C
    .byte $d8, $64, $42, $42, $42, $44, $b8, $00, $24, $9a, $25, $21, $21, $22, $44, $b8 ; D
    .byte $dc, $62, $40, $70, $40, $44, $f8, $00, $22, $9d, $20, $08, $30, $22, $04, $f8 ; E
    .byte $dc, $62, $40, $70, $40, $40, $80, $00, $22, $9d, $20, $08, $30, $20, $40, $80 ; F
    .byte $3c, $42, $80, $9e, $84, $4c, $32, $00, $02, $3c, $40, $41, $4a, $a2, $4d, $30 ; G
    .byte $c2, $44, $44, $7c, $44, $44, $84, $00, $21, $a2, $22, $02, $3a, $22, $42, $84 ; H
    .byte $30, $10, $10, $10, $10, $10, $20, $00, $08, $08, $08, $08, $08, $08, $10, $20 ; I
    .byte $c6, $48, $50, $70, $48, $4a, $84, $00, $21, $a6, $28, $08, $34, $25, $4a, $84 ; K
    .byte $60, $20, $20, $20, $40, $62, $9c, $00, $10, $50, $10, $10, $20, $11, $62, $9c ; L
    .byte $c2, $66, $56, $4a, $4a, $42, $82, $00, $21, $91, $29, $35, $25, $29, $41, $82 ; M
    .byte $c2, $62, $52, $4a, $46, $46, $82, $00, $20, $91, $29, $25, $29, $21, $45, $83 ; N
    .byte $38, $44, $82, $82, $82, $44, $38, $00, $04, $3a, $65, $41, $41, $a2, $44, $38 ; O
    .byte $dc, $62, $42, $62, $5c, $40, $80, $00, $22, $9d, $21, $11, $22, $3c, $40, $80 ; P
    .byte $dc, $62, $42, $44, $78, $4a, $84, $00, $22, $9d, $21, $22, $04, $34, $4a, $84 ; R
    .byte $3c, $42, $30, $08, $44, $84, $78, $00, $02, $3d, $48, $14, $0a, $42, $84, $78 ; S
    .byte $7e, $88, $50, $10, $20, $20, $20, $00, $01, $76, $a8, $08, $10, $10, $10, $20 ; T
    .byte $c2, $22, $22, $42, $44, $44, $38, $00, $20, $d1, $11, $21, $22, $22, $44, $38 ; U
    .byte $c2, $42, $24, $24, $28, $30, $10, $00, $21, $a1, $52, $12, $14, $08, $28, $10 ; V
    .byte $84, $42, $52, $52, $52, $74, $48, $00, $42, $a1, $29, $29, $29, $0a, $34, $48 ; W
    .byte $00, $08, $08, $08, $7f, $08, $08, $08, $00, $10, $10, $10, $80, $f7, $10, $10 ; +
    .byte $c6, $44, $28, $10, $10, $20, $20, $00, $21, $a2, $54, $28, $08, $10, $10, $20 ; Y
    .byte $06, $04, $04, $44, $84, $88, $70, $00, $01, $02, $02, $22, $42, $44, $88, $70 ; J
    .byte $1e, $21, $11, $04, $10, $3f, $00, $00, $20, $5e, $20, $03, $0c, $40, $7f, $00 ; 2
campaign_digit_patterns:
    .byte $0c, $12, $21, $21, $21, $1e, $00, $00, $10, $2c, $52, $42, $42, $21, $1e, $00 ; 0
    .byte $0c, $04, $04, $04, $04, $0e, $00, $00, $10, $18, $08, $08, $08, $10, $1e, $00 ; 1
    .byte $1e, $21, $11, $04, $10, $3f, $00, $00, $20, $5e, $20, $03, $0c, $40, $7f, $00 ; 2
    .byte $3f, $02, $04, $0e, $01, $1e, $00, $00, $00, $3d, $02, $00, $0e, $21, $3e, $00 ; 3
    .byte $02, $06, $0a, $12, $3f, $02, $00, $00, $04, $08, $10, $20, $40, $7d, $06, $00 ; 4
    .byte $13, $1c, $10, $1e, $01, $1e, $00, $00, $20, $23, $2c, $20, $3e, $21, $3e, $00 ; 5
    .byte $02, $04, $0a, $11, $11, $0e, $00, $00, $04, $08, $14, $2a, $20, $11, $0e, $00 ; 6
    .byte $1f, $01, $02, $04, $08, $10, $20, $00, $20, $3e, $04, $08, $10, $20, $40, $00 ; 7
    .byte $0e, $11, $11, $0e, $11, $0e, $00, $00, $10, $2e, $22, $11, $22, $11, $0e, $00 ; 8
    .byte $0e, $11, $11, $0d, $02, $04, $08, $00, $10, $2e, $22, $12, $0d, $02, $04, $00 ; 9
.else
    .byte $00, $1f, $01, $02, $04, $08, $08, $08, $00, $00, $00, $00, $00, $00, $00, $00 ; 7
    .byte $00, $0e, $11, $11, $1f, $11, $11, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; A
    .byte $00, $1e, $11, $11, $1e, $11, $11, $1e, $00, $00, $00, $00, $00, $00, $00, $00 ; B
    .byte $00, $0f, $10, $10, $10, $10, $10, $0f, $00, $00, $00, $00, $00, $00, $00, $00 ; C
    .byte $00, $1e, $11, $11, $11, $11, $11, $1e, $00, $00, $00, $00, $00, $00, $00, $00 ; D
    .byte $00, $1f, $10, $10, $1e, $10, $10, $1f, $00, $00, $00, $00, $00, $00, $00, $00 ; E
    .byte $00, $1f, $10, $10, $1e, $10, $10, $10, $00, $00, $00, $00, $00, $00, $00, $00 ; F
    .byte $00, $0f, $10, $10, $17, $11, $11, $0f, $00, $00, $00, $00, $00, $00, $00, $00 ; G
    .byte $00, $11, $11, $11, $1f, $11, $11, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; H
    .byte $00, $0e, $04, $04, $04, $04, $04, $0e, $00, $00, $00, $00, $00, $00, $00, $00 ; I
    .byte $00, $11, $12, $14, $18, $14, $12, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; K
    .byte $00, $10, $10, $10, $10, $10, $10, $1f, $00, $00, $00, $00, $00, $00, $00, $00 ; L
    .byte $00, $11, $1b, $15, $15, $11, $11, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; M
    .byte $00, $11, $19, $15, $13, $11, $11, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; N
    .byte $00, $0e, $11, $11, $11, $11, $11, $0e, $00, $00, $00, $00, $00, $00, $00, $00 ; O
    .byte $00, $1e, $11, $11, $1e, $10, $10, $10, $00, $00, $00, $00, $00, $00, $00, $00 ; P
    .byte $00, $1e, $11, $11, $1e, $14, $12, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; R
    .byte $00, $0f, $10, $10, $0e, $01, $01, $1e, $00, $00, $00, $00, $00, $00, $00, $00 ; S
    .byte $00, $1f, $04, $04, $04, $04, $04, $04, $00, $00, $00, $00, $00, $00, $00, $00 ; T
    .byte $00, $11, $11, $11, $11, $11, $11, $0e, $00, $00, $00, $00, $00, $00, $00, $00 ; U
    .byte $00, $11, $11, $11, $11, $11, $0a, $04, $00, $00, $00, $00, $00, $00, $00, $00 ; V
    .byte $00, $11, $11, $11, $15, $15, $1b, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; W
    .byte $00, $00, $04, $04, $1f, $04, $04, $00, $00, $00, $00, $00, $00, $00, $00, $00 ; +
    .byte $00, $11, $11, $0a, $04, $04, $04, $04, $00, $00, $00, $00, $00, $00, $00, $00 ; Y
    .byte $00, $07, $02, $02, $02, $12, $12, $0c, $00, $00, $00, $00, $00, $00, $00, $00 ; J
    .byte $00, $0e, $11, $01, $02, $04, $08, $1f, $00, $00, $00, $00, $00, $00, $00, $00 ; 2
.if PRG_ROM_UNITS = 32
campaign_digit_patterns:
    .byte $00, $0e, $11, $11, $11, $11, $11, $0e
    .res 8, 0 ; 0
    .byte $00, $04, $0c, $04, $04, $04, $04, $0e
    .res 8, 0 ; 1
    .byte $00, $0e, $11, $01, $02, $04, $08, $1f
    .res 8, 0 ; 2
    .byte $00, $1e, $01, $01, $0e, $01, $01, $1e
    .res 8, 0 ; 3
    .byte $00, $02, $06, $0a, $12, $1f, $02, $02
    .res 8, 0 ; 4
    .byte $00, $1f, $10, $10, $1e, $01, $01, $1e
    .res 8, 0 ; 5
    .byte $00, $0e, $10, $10, $1e, $11, $11, $0e
    .res 8, 0 ; 6
    .byte $00, $1f, $01, $02, $04, $08, $08, $08
    .res 8, 0 ; 7
    .byte $00, $0e, $11, $11, $0e, $11, $11, $0e
    .res 8, 0 ; 8
    .byte $00, $0e, $11, $11, $0f, $01, $01, $0e
    .res 8, 0 ; 9
.endif
.endif
terrain_tiles:
    ; Pebbles, ledge, and two alternating rock textures.
    .byte $00, $00, $10, $38, $10, $00, $02, $00, $00, $00, $00, $10, $00, $00, $00, $00
    .byte $ff, $ff, $b7, $42, $00, $10, $02, $00, $00, $6d, $00, $00, $00, $00, $00, $00
    .byte $c4, $28, $10, $03, $8f, $02, $10, $30, $ff, $ef, $ff, $ff, $3f, $ff, $e7, $ff
    .byte $0c, $10, $40, $84, $cc, $02, $00, $08, $ff, $e7, $ff, $7b, $ff, $ff, $ff, $f7
scenery_patterns:
    ; Lantern posts with transparent openings, followed by a stone carving.
    .byte $1c, $22, $5d, $d5, $22, $14, $14, $08, $00, $1c, $3e, $3e, $1c, $08, $08, $1c
    .byte $14, $08, $f7, $00, $08, $14, $14, $08, $08, $1c, $1c, $ff, $1c, $08, $08, $1c
    .byte $08, $14, $36, $2a, $2a, $6b, $c1, $ff, $1c, $08, $08, $1c, $1c, $1c, $3e, $00
    .byte $00, $24, $00, $14, $4a, $28, $42, $ff, $00, $00, $08, $08, $1c, $1c, $3c, $00
checkpoint_pattern:
    ; Diamond and post on the ledge before the kiln.
    .byte $18, $24, $42, $42, $24, $18, $18, $3c, $00, $18, $3c, $3c, $18, $00, $00, $00
goal_pattern:
    ; Pennant and its post, followed by the base.
    .byte $7e, $42, $7a, $46, $78, $40, $40, $40, $00, $3c, $04, $38, $00, $00, $00, $00
    .byte $40, $40, $40, $40, $40, $40, $e0, $e0, $00, $00, $00, $00, $00, $00, $40, $00
.if PRG_ROM_UNITS = 32
oneway_pattern:
    ; Thin timber plank at the top of the cell.
    .byte $ff, $aa, $ff, $00, $00, $00, $00, $00, $ff, $55, $00, $00, $00, $00, $00, $00
ladder_pattern:
    ; Side rails and two rungs.
    .byte $42, $7e, $42, $42, $42, $7e, $42, $42, $42, $42, $42, $42, $42, $42, $42, $42
hazard_pattern:
    ; Water / spike wave surface.
    .byte $00, $18, $3c, $7e, $ff, $db, $ff, $ff, $00, $00, $18, $3c, $7e, $ff, $ff, $ff
.endif
CHR_TEXT_BYTES = terrain_tiles - glyphs
CHR_TERRAIN_BYTES = * - terrain_tiles
CHR_BACKGROUND_BYTES = CHR_TEXT_BYTES + CHR_TERRAIN_BYTES
.export CHR_TEXT_BYTES, CHR_TERRAIN_BYTES
.assert CHR_BACKGROUND_BYTES <= chr_table_bytes, error, "Text and terrain exceed the background pattern table"
.assert CHR_TEXT_BYTES .mod chr_tile_bytes = 0, error, "Incomplete text tile"
.assert CHR_TERRAIN_BYTES .mod chr_tile_bytes = 0, error, "Incomplete terrain tile"

; Kept last in BSS so earlier variables keep the addresses the Mesen tests read.
.segment "BSS"
game_paused: .res 1             ; Start pauses the arcade game (toggle_pause)
demo_active: .res 1             ; the opening demo plays over Stage 1 (revg-opening.inc)
demo_finish: .res 1             ; set at its end (or START): Stage 1 starts
death_timer: .res 2             ; frames since the player died (the automatic restart)
pan_tile_x_hi: .res 1           ; Bit 8 of pan_tile_x (two-axis-scroll.inc)
pan_data_x_hi: .res 1
goal_tile_x_hi: .res 1
checkpoint_tile_x_hi: .res 1
pan_row_start: .res 1            ; First ring column of a prepared row
.if PRG_ROM_UNITS = 32
in_play_step: .res 1             ; 1 while update_game is running (lag-frame NMI audio hook)
title_cursor: .res 1             ; 0 = 1P START, 1 = OPTION
opt_cursor: .res 1               ; 0..7 selected row in Option screen
opt_invincible: .res 1           ; 0 = OFF, 1 = ON (ignore enemy/shot contact & frog)
opt_start_stage: .res 1          ; 1..7 starting stage
opt_start_loop: .res 1           ; 0 = Loop 1, 1 = Loop 2
opt_start_weapon: .res 1         ; 0=Lance, 1=Dagger, 2=Torch, 3=Cross/Shield, 4=Axe
opt_no_timer: .res 1             ; 0 = normal timer, 1 = timer disabled
opt_lives_idx: .res 1            ; 0=3, 1=5, 2=9 initial lives
opt_return_to: .res 1            ; 1 when Sound Test was opened from Option screen
.endif

.if PRG_ROM_UNITS = 32
.include "frontend.inc"
.endif
.ifdef REVG_STAGE1
.include "revg-background.inc"
.include "revg-objects.inc"
.include "revg-enemies.inc"
.include "revg-enemies2.inc"
.include "revg-enemies34.inc"
.include "revg-enemies567.inc"
.include "revg-opening.inc"
.endif

.segment "VECTORS"
.word terrain_nmi, reset, hud_irq
