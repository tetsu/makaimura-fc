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
.segment "PAD_5_6"
.repeat 2, index
    .byte bank_tag_base + 5 + index
    .res $1fff, $ff
.endrepeat
.ifdef NES_AREA_ART
.include "nes-areas.inc"
.else
.segment "PAD_11"
    .byte bank_tag_base + 11
    .res $1fff, $ff
.segment "PAD_19_62"
.ifdef REVG_STAGE1
; Stage 1 background art from bank 19 (revg-background.inc reads it through $C000).
.include "revg-section0-bg.inc"
revg_bg_banks
.reloc
.repeat 44 - revg_bg_bank_count, index
    .byte bank_tag_base + 19 + revg_bg_bank_count + index
    .res $1fff, $ff
.endrepeat
.else
.repeat 44, index
    .byte bank_tag_base + 19 + index
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
.include "arcade-music-data.inc"
.include "arcade-gfx-data.inc"
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
.endif
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
    bne memory_error
    lda $c000
    cmp #bank_tag_base + 2
    bne memory_error
    lda $e000
    cmp #bank_tag_base + fixed_bank
    bne memory_error
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
    and #$20            ; Select: switch between audio controls and terrain.
    beq same_scene
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
.ifdef REVG_STAGE1
    cmp #opening_scene
    bcc :+
    jsr update_frontend_scene
    jmp main
:
.endif
.endif
    lda pressed
    and #$10            ; Start: enter or restart the playable scene.
.ifdef REVG_STAGE1
    bne :+
    jmp dispatch_scene
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
    beq dispatch_scene
    jsr reset_campaign_clock
    jmp start_pressed
finish_campaign:
    jsr blank_display
    lda arcade_mode
    beq @finish_to_sound
    jsr show_title_screen
    jmp audio_tick
@finish_to_sound:
    jsr show_sound_screen
    jmp audio_tick
advance_campaign:
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
    lda pad
    and #$08            ; Hold Up and press Start on the title for legacy campaign.
    bne @begin_campaign
.ifdef REVG_STAGE1
    lda pad
    and #$01            ; Right+START skips the opening and the map (Mesen tests).
    bne @begin_arcade
    lda #snd_song_title ; The arcade starts the opening music after 1P START.
    jsr call_bank8_play_song
    lda #opening_scene
    jsr show_frontend_scene
    jsr call_bank8_audio_tick
    jmp main
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
    beq pan_screen
    cmp #2
    beq game_screen
    cmp #clear_scene
    beq clear_screen_idle
.if PRG_ROM_UNITS = 32
    cmp #game_over_scene
    beq clear_screen_idle
    cmp #title_scene
    beq clear_screen_idle
.endif
    cmp #4
    bne free_camera
game_screen:
    jsr update_game
    jmp audio_tick
clear_screen_idle:
    jsr wait_terrain_frame
    jmp audio_tick
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
    jmp ap_draw_sprites

call_bank3_load_stage_visuals:
    lda #3
    jsr select_bank1
    jmp ap_load_stage_visuals

call_bank8_audio_init:
    lda #8
    jsr select_bank2
    jmp snd_init

; Song and effect calls put the caller's $C000 bank back, so Bank 4 and 7 code can use them.
call_bank8_play_song:
    sta snd_call_arg
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    lda snd_call_arg
    jsr snd_play_song
    pla
    jmp select_bank2

call_bank8_play_se:
    sta snd_call_arg
    lda prg_bank2
    pha
    lda #8
    jsr select_bank2
    lda snd_call_arg
    jsr snd_play_se
    pla
    jmp select_bank2

call_bank8_audio_tick:
    lda #8
    jsr select_bank2
    jmp snd_tick
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

load_menu_patterns:
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
    jmp upload_chr_patterns

draw_screen:
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
.endif
    rts

.if PRG_ROM_UNITS = 32
show_title_screen:
    lda #0
    jsr select_bank0
    sta campaign_stage
    sta arcade_mode
    lda #1
    jsr select_bank1
    lda #title_scene
    sta scene
    jsr call_bank8_audio_init
    lda #snd_song_title
    jsr call_bank8_play_song
    lda #2
    jsr select_bank2
    jsr draw_title_screen
    jmp enable_static_screen

begin_arcade_game:
    lda #1
    sta arcade_mode
    sta campaign_stage
    lda #campaign_initial_lives
    sta campaign_lives
    jmp start_arcade_stage

.ifdef REVG_STAGE1
; A = opening_scene or map_scene.
show_frontend_scene:
    pha
    jsr blank_display
    pla
    sta scene
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
.ifdef REVG_STAGE1
final_title: .byte $12, $13, $02, $08, $06, campaign_zero + 1, $00, $04, $0c, $06, $02, $11 ; STAGE1 CLEAR
.else
final_title: .byte $07, $0a, $0e, $02, $0c, $00, $04, $0c, $06, $02, $11, $00 ; FINAL CLEAR
.endif
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
campaign_menu: .byte $14, $10, $18, $12, $13, $02, $11, $13, $00, $04, $02, $0d, $10, $02, $0a, $08, $0e ; UP+START CAMPAIGN
campaign_menu_length = * - campaign_menu
stage_view_menu: .byte $14, $10, $18, $02, 0, $12, $13, $02, $08, $06, 0, $15, $0a, $06, $16 ; UP+A STAGE VIEW
stage_view_menu_length = * - stage_view_menu
sound_test_menu: .byte $05, $0f, $16, $0e, $17, $02, $00, $12, $0f, $14, $0e, $05, $00, $13, $06, $12, $13 ; DOWN+A SOUND TEST
sound_test_menu_length = * - sound_test_menu
.endif
clear_menu: .byte $12, $06, $0c, $06, $04, $13, $00, $0d, $06, $0e, $14 ; SELECT MENU
clear_menu_length = * - clear_menu
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
glyphs:
    .res 16, 0
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
pan_tile_x_hi: .res 1           ; Bit 8 of pan_tile_x (two-axis-scroll.inc)
pan_data_x_hi: .res 1
goal_tile_x_hi: .res 1
checkpoint_tile_x_hi: .res 1
pan_row_start: .res 1            ; First ring column of a prepared row

.if PRG_ROM_UNITS = 32
.include "frontend.inc"
.endif
.ifdef REVG_STAGE1
.include "revg-background.inc"
.endif

.segment "VECTORS"
.word terrain_nmi, reset, hud_irq
