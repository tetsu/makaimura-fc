; The encounter uses the same scrolling terrain and an independent sprite table.
enemy_x = 272
enemy_feet = 176
ranged_x = 742
ranged_feet = 160
ranged_range = 34
ranged_windup = 30
ranged_cooldown = 48
bolt_y = ranged_feet - 9
clear_scene = 5
player_height = 16
jump_fall = 18
ranged_slot = 5 * 4
bolt_slot = 7 * 4
crowd_start = 8 * 4
crowd_count = 8
crowd_end = crowd_start + crowd_count * 4

.segment "ZEROPAGE"
player_x: .res 2
player_feet: .res 1
player_hit: .res 1
player_left: .res 1
jump_step: .res 1
enemy_active: .res 1
shot_x: .res 2
shot_y: .res 1
shot_left: .res 1
shot_life: .res 1
ranged_active: .res 1
ranged_timer: .res 1
bolt_x: .res 1         ; This encounter and the bolt's entire path are on page 2.
bolt_active: .res 1
old_x: .res 2
ground: .res 1
draw_x: .res 2
draw_y: .res 1
draw_tile: .res 1
draw_palette: .res 1
draw_slot: .res 1
crowd_enabled: .res 1
crowd_first: .res 1
checkpoint_reached: .res 1
checkpoint_tile_x: .res 1

.segment "OAM"
sprites: .res 256

.segment "CODE"
.if PRG_ROM_UNITS = 32
campaign_stage_count = 2
arcade_stage_count = 7

start_arcade_stage:
    lda #1
    sta ap_new_game
    lda #0
    sta ap_weapon
    sta ap_score+0
    sta ap_score+1
    sta ap_score+2
    sta ap_score+3
    sta ap_loop
    sta ap_bounce_back
    jsr blank_display
    jsr reset_campaign_clock
    lda #arcade_stage_bank
    jsr select_bank0
    jmp show_pan_game

is_final_clear:
    lda arcade_mode
    bne @check_arcade_final
    lda campaign_stage
    cmp #campaign_stage_count
    rts
@check_arcade_final:
    lda campaign_stage
    cmp #arcade_stage_count
    bcc @not_final
    lda ap_loop
    bne @is_final
@not_final:
    clc
    rts
@is_final:
    sec
    rts

next_campaign_stage:
    lda arcade_mode
    bne @arcade_next
    lda campaign_stage
    cmp #campaign_stage_count
    bcc @advance_stage
    jsr blank_display
    jmp show_sound_screen
@arcade_next:
    lda campaign_stage
    cmp #6
    bne @check_stage7
    lda ap_weapon
    cmp #ap_weapon_shield
    beq @arcade_advance
    lda #5
    sta campaign_stage
    lda #1
    sta ap_bounce_back
    jmp start_campaign_stage
@check_stage7:
    cmp #arcade_stage_count
    bcc @arcade_advance
    lda ap_loop
    beq @begin_loop2
    jsr blank_display
    jmp show_title_screen
@begin_loop2:
    lda #1
    sta ap_loop
    sta campaign_stage
    lda #0
    sta ap_bounce_back
    jmp start_campaign_stage
@arcade_advance:
    lda #0
    sta ap_bounce_back
@advance_stage:
    inc campaign_stage
start_campaign_stage:
    jsr blank_display
    jsr reset_campaign_clock
    lda campaign_stage
    sec
    sbc #1
    ldx arcade_mode
    beq :+
    clc
    adc #arcade_stage_bank
:
    jsr select_bank0
    jmp show_pan_game
.endif

start_course:
    jsr blank_display
    lda pad
.ifdef NES_AREA_TRIAL
    and #$02            ; Hold Left with B to start the manifest trial.
    beq :+
    lda #nes_area_trial_bank
    bne @map_course
:
    lda pad
.endif
    and #$04            ; Hold Down with B to map the alternate course.
    lsr a
    lsr a
.ifdef NES_AREA_TRIAL
@map_course:
.endif
    jsr select_bank0
    jmp show_pan_game

show_game:
    jsr blank_display
    lda scene
    cmp #3
    beq show_pan_game
    cmp #clear_scene
    beq show_pan_game
    cmp #4
    beq retry_pan_game
    lda #2
    sta scene
    jsr draw_terrain
    jmp setup_game
retry_pan_game:
    lda player_hit
    beq show_pan_game
    lda checkpoint_reached
    bne draw_pan_game
show_pan_game:
    lda #0
    sta checkpoint_reached
draw_pan_game:
    lda #4
    sta scene
    lda #0
    sta pan_y_page
    ldx #0
    ldy #0
    lda checkpoint_reached
    beq @pan_start_page
    ldx stage_cp_cam_hi
    ldy stage_cp_cam_y
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    beq @pan_page_done
    lda sd_cp_cam_y_page
    sta pan_y_page
    jmp @pan_page_done
.endif
@pan_start_page:
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    beq @pan_page_done
    lda sd_flags
    and #sd_flag_start_page1
    beq @pan_page_done
    lda #1
    sta pan_y_page
.endif
@pan_page_done:
    txa
    sta camera_high
    jsr draw_pan
    lda #0
    sta camera_low
    lda #$a0
    sta pan_ctrl
setup_game:
    jsr draw_hud
    jsr draw_sprite_patterns
    ldx #0
    lda #$f8
:
    sta sprites,x
    inx
    bne :-
    lda stage_start_x_lo
    sta player_x
    ldx stage_start_x_hi
    stx player_x+1
    jsr floor_height
    sta player_feet
    lda #0
    sta player_hit
    sta player_left
    sta shot_life
    sta ranged_active
    sta ranged_timer
    sta bolt_active
    sta crowd_enabled
    sta crowd_first
    lda #$ff
    sta jump_step
    lda #1
    sta enemy_active
    ldx scene
    cpx #4
    bne setup_game_sprites
    sta ranged_active
    lda checkpoint_reached
    beq setup_game_sprites
    lda stage_cp_x_lo
    sta player_x
    lda stage_cp_x_hi
    sta player_x+1
    lda stage_cp_feet
    sta player_feet
setup_game_sprites:
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    beq :+
    jsr call_bank3_init
:
.endif
    jsr draw_sprites
    jsr upload_sprites
    jmp enable_terrain

update_game:
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    beq :+
    jmp call_bank3_update
:
    lda campaign_stage
    bne check_player_hit
.endif
    lda pressed
    and #$08            ; Up toggles the sprite evaluation test.
    beq :+
    lda crowd_enabled
    eor #1
    sta crowd_enabled
    bne :+
    jsr hide_crowd
:
check_player_hit:
    lda player_hit
    bne game_camera
    jsr move_player
    jsr move_vertically
    jsr update_shot
    jsr check_enemy_contact
    jsr update_ranged
.if PRG_ROM_UNITS = 32
    lda campaign_stage
    beq @after_campaign_tick
    jsr tick_campaign_clock
    lda player_hit
    beq @after_campaign_tick
    dec campaign_lives
    bne @campaign_retry_hud
    jmp show_game_over
@campaign_retry_hud:
    inc campaign_hud_dirty
    jmp game_camera
@after_campaign_tick:
.endif
    jsr check_course_progress
    bcc game_camera
    jmp show_course_clear
game_camera:
    jsr follow_player
    jmp submit_frame

follow_player:
    sec
    lda player_x
    sbc camera_low
    tay
    lda player_x+1
    sbc camera_high
    bmi follow_left
    bne follow_right    ; The player's center is at or past the right edge.
    cpy #192
    bcs follow_right
    cpy #64
    bcc follow_left
    lda #0
    beq camera_ready
follow_right:
    lda #1
    bne camera_ready
follow_left:
    lda #2
camera_ready:
    ldx scene
    cpx #4
    bne :+
    jmp follow_both_axes
:
    jsr step_camera
    jmp draw_sprites

; Carry set only on reaching the goal alive and on the ground.
check_course_progress:
    lda scene
    cmp #4
    bne :+
    lda player_hit
    bne :+
    lda player_x+1
    cmp stage_cp_x_hi
    bcc :+
    bne checkpoint_passed
    lda player_x
    cmp stage_cp_x_lo
    bcc :+
checkpoint_passed:
.if PRG_ROM_UNITS = 32
    ldx campaign_stage
    beq checkpoint_marked
    ldx checkpoint_reached
    bne checkpoint_marked
    inc campaign_hud_dirty
checkpoint_marked:
.endif
    lda #1
    sta checkpoint_reached
    lda player_x+1
    cmp stage_goal_x_hi
    bne :+
    lda player_x
    cmp stage_goal_x_lo
    bne :+
    lda player_feet
    cmp stage_goal_feet
    bne :+
    lda jump_step
    cmp #$ff
    beq course_finished
:
    clc
    rts
course_finished:
    sec
    rts

.if PRG_ROM_UNITS = 32
show_game_over:
    lda arcade_mode
    beq :+
    lda #snd_song_gameover
    jsr call_bank8_play_song
:
    lda #game_over_scene
    bne show_result
show_course_clear:
    lda arcade_mode
    beq :+
    lda #snd_song_clear
    jsr call_bank8_play_song
:
    lda #clear_scene
show_result:
    pha
    jsr blank_display
    pla
    sta scene
.else
show_course_clear:
    jsr blank_display
    lda #clear_scene
    sta scene
.endif
    jsr clear_screen
    lda #$21
    sta $2006
    lda #$8a
    sta $2006
    ldx #0
.if PRG_ROM_UNITS = 32
    ldy #game_over_title - clear_title
    lda scene
    cmp #game_over_scene
    beq :+
    ldy #0
    jsr is_final_clear
    bcc :+
    ldy #final_title - clear_title
:
.endif
:
.if PRG_ROM_UNITS = 32
    lda clear_title,y
    iny
.else
    lda clear_title,x
.endif
    sta $2007
    inx
    cpx #clear_title_length
    bne :-
    lda #$21
    sta $2006
    lda #$cb
    sta $2006
    ldx #0
.if PRG_ROM_UNITS = 32
    ldy #0
    lda campaign_stage
    beq :+
    ldy #final_prompt - clear_prompt
    lda scene
    cmp #game_over_scene
    beq :+
    ldy #next_prompt - clear_prompt
    lda arcade_mode
    beq :+
    ldy #arcade_next_prompt - clear_prompt
:
    jsr is_final_clear
    bcc :+
    ldy #final_prompt - clear_prompt
:
.endif
:
.if PRG_ROM_UNITS = 32
    lda clear_prompt,y
    iny
.else
    lda clear_prompt,x
.endif
    sta $2007
    inx
    cpx #clear_prompt_length
    bne :-
    lda #$21
    sta $2006
    lda #$eb
    sta $2006
    ldx #0
:
    lda clear_menu,x
    sta $2007
    inx
    cpx #clear_menu_length
    bne :-
    jmp enable_static_screen

follow_both_axes:
    ; Follow the player's feet toward scanline 160 by at most one pixel.
    ; Floors in this scene end at Y=208, so the camera stays on page zero.
    sta pan_directions
    sec
    lda player_feet
    sbc pan_y
    bcc pan_game_up
    cmp #160
    beq pan_game_ready
    bcc pan_game_up
    lda #$04
    bne pan_game_y
pan_game_up:
    lda #$08
pan_game_y:
    ora pan_directions
    sta pan_directions
pan_game_ready:
    jsr prepare_pan
    lda pan_x
    sta camera_low
    lda pan_x+1
    sta camera_high
    jmp draw_sprites

move_player:
    lda pad
    and #3
    beq read_ground
    cmp #3
    beq read_ground
    ldx player_x
    stx old_x
    ldx player_x+1
    stx old_x+1
    cmp #1
    bne player_moves_left
    lda #0
    sta player_left
    lda player_x+1
    cmp stage_goal_x_hi
    bne advance_player
    lda player_x
    cmp stage_goal_x_lo
    beq read_ground
advance_player:
    inc player_x
    bne check_wall
    inc player_x+1
    jmp check_wall
player_moves_left:
    lda #1
    sta player_left
    lda player_x
    bne :+
    dec player_x+1
    jmp retreat_player
:
    cmp #8
    bne retreat_player
    ldx player_x+1
    beq read_ground
retreat_player:
    dec player_x
check_wall:
    jsr read_ground
    cmp player_feet
    bcs move_done
    lda old_x
    sta player_x
    lda old_x+1
    sta player_x+1
read_ground:
    lda player_x
    ldx player_x+1
    jsr floor_height
    sta ground
move_done:
    rts

; A = world X low byte, X = high byte. A returns first solid floor Y in pixels.
floor_height:
    lsr a
    lsr a
    lsr a
    lsr a
    ora page_columns,x
    jsr set_map_col_ptr
    ldy #0
@scan_floor:
    lda (map_ptr),y
    tax
    lda metatile_attr,x
    and #tile_type_mask
    cmp #tile_type_solid
    beq @found_floor
    iny
    cpy stage_height_rows
    bne @scan_floor
@found_floor:
    tya
    asl a
    asl a
    asl a
    asl a
    rts

move_vertically:
    ldx jump_step
    bpl air_step
    lda pressed
    and #$80
    bne begin_jump
    lda player_feet
    cmp ground
    beq vertical_done
    ldx #jump_fall
    bne air_step
begin_jump:
    ldx #0
air_step:
    lda player_feet
    clc
    adc jump_delta,x
    cmp ground
    bcs land
    sta player_feet
    inx
    cpx #jump_length
    bcc :+
    dex                 ; Continue falling until the ground catches up.
:
    stx jump_step
vertical_done:
    rts
land:
    lda ground
    sta player_feet
    lda #$ff
    sta jump_step
    rts

update_shot:
    lda shot_life
    bne travel_shot
    lda pressed
    and #$40
    beq shot_done
    lda player_x
    sta shot_x
    lda player_x+1
    sta shot_x+1
    lda player_feet
    sec
    sbc #9
    sta shot_y
    lda player_left
    sta shot_left
    lda #48
    sta shot_life
    lda #1
    sta shot_sound_pending
travel_shot:
    lda shot_left
    bne shot_moves_left
    clc
    lda shot_x
    adc #4
    sta shot_x
    lda shot_x+1
    adc #0
    jmp shot_position
shot_moves_left:
    sec
    lda shot_x
    sbc #4
    sta shot_x
    lda shot_x+1
    sbc #0
shot_position:
    sta shot_x+1
    cmp #3
    bcs remove_shot
    tax
    lda shot_x
    jsr floor_height
    cmp shot_y
    bcc remove_shot
    beq remove_shot
    jsr check_ranged_shot
    bcs remove_shot
    lda enemy_active
    beq age_shot
    lda shot_x+1
    cmp #>enemy_x
    bne age_shot
    lda shot_x
    cmp #<(enemy_x-8)
    bcc age_shot
    cmp #<(enemy_x+9)
    bcs age_shot
    lda shot_y
    cmp #enemy_feet-player_height
    bcc age_shot
    cmp #enemy_feet
    bcs age_shot
    lda #0
    sta enemy_active
remove_shot:
    lda #0
    sta shot_life
    rts
age_shot:
    dec shot_life
shot_done:
    rts

; Carry set if the player's shot destroyed the kiln.
check_ranged_shot:
    lda ranged_active
    beq ranged_shot_missed
    lda shot_x+1
    cmp #>ranged_x
    bne ranged_shot_missed
    lda shot_x
    cmp #<(ranged_x-8)
    bcc ranged_shot_missed
    cmp #<(ranged_x+8)
    bcs ranged_shot_missed
    lda shot_y
    cmp #ranged_feet-player_height
    bcc ranged_shot_missed
    cmp #ranged_feet
    bcs ranged_shot_missed
    lda #0
    sta ranged_active
    sec
    rts
ranged_shot_missed:
    clc
    rts

check_enemy_contact:
    lda enemy_active
    beq contact_done
    lda player_x+1
    cmp #>enemy_x
    bne contact_done
    lda player_x
    cmp #<(enemy_x-10)
    bcc contact_done
    cmp #<(enemy_x+11)
    bcs contact_done
    lda player_feet
    cmp #enemy_feet-player_height+1
    bcc contact_done
    cmp #enemy_feet+player_height
    bcs contact_done
    lda #1
    sta player_hit
contact_done:
    rts

update_ranged:
    ; Continue the launched bolt even if the shooter is destroyed.
    lda bolt_active
    beq aim_bolt
    lda bolt_x
    sec
    sbc #2
    bcc reclaim_bolt    ; Leave page 2 without wrapping to the right.
    sta bolt_x
    ; Check the left edge against the cliff before testing the player.
    sbc #2
    bcc reclaim_bolt
    ldx #>ranged_x
    jsr floor_height
    cmp #bolt_y+3
    bcc reclaim_bolt    ; The bolt's bottom pixel entered solid terrain.
    lda player_x+1
    cmp #>ranged_x
    bne aim_bolt
    lda player_x
    sec
    sbc bolt_x
    clc
    adc #8
    cmp #17
    bcs aim_bolt
    lda player_feet
    cmp #bolt_y
    bcc aim_bolt
    cmp #bolt_y+player_height+3
    bcs aim_bolt
    lda #1
    sta player_hit
reclaim_bolt:
    lda #0
    sta bolt_active
aim_bolt:
    lda ranged_active
    beq ranged_done
    lda ranged_timer
    beq begin_aim
    dec ranged_timer
    lda ranged_timer
    cmp #ranged_cooldown
    bne ranged_done
    lda #<(ranged_x-10)
    sta bolt_x
    lda #1
    sta bolt_active
    rts
begin_aim:
    lda bolt_active
    bne ranged_done
    lda player_x+1
    cmp #>ranged_x
    bne ranged_done
    lda player_x
    cmp #<(ranged_x-ranged_range)
    bcc ranged_done
    cmp #<(ranged_x-8)
    bcs ranged_done
    lda #ranged_cooldown+ranged_windup
    sta ranged_timer
ranged_done:
    rts

draw_sprites:
.if PRG_ROM_UNITS = 32
    lda arcade_mode
    beq :+
    jmp call_bank3_draw_sprites
:
.endif
    ; Hide the gameplay entries before drawing visible pieces.
    lda #$f8
    ldx #bolt_slot
:
    sta sprites,x
    dex
    dex
    dex
    dex
    bpl :-
    lda #0
    sta draw_slot
    lda player_hit
    asl a
    sta draw_palette
    lda player_x+1
    cmp #>lantern_x
    bne player_in_front
    lda player_x
    cmp #<(lantern_x-7)
    bcc player_in_front
    cmp #<(lantern_x+24)
    bcs player_in_front
    lda draw_palette
    ora #$20            ; Color zero in the lantern tiles leaves openings.
    sta draw_palette
player_in_front:
    sec
    lda player_feet
    sbc #player_height+1
    jsr set_sprite_y
    lda #1
    sta draw_tile
    lda player_x
    ldx player_x+1
    jsr draw_pair
    lda enemy_active
    beq draw_shot
    lda #8
    sta draw_slot
    lda #1
    sta draw_palette
    lda #enemy_feet-player_height-1
    jsr set_sprite_y
    lda #5
    sta draw_tile
    lda #<enemy_x
    ldx #>enemy_x
    jsr draw_pair
draw_shot:
    lda shot_life
    beq draw_ranged
    lda #16
    sta draw_slot
    lda #3
    sta draw_palette    ; Keep the projectile visible over the lanterns.
    lda #9
    sta draw_tile
    lda shot_y
    sec
    sbc #2
    jsr set_sprite_y
    lda shot_x
    ldx shot_x+1
    jsr set_sprite_x
    jsr emit_sprite
draw_ranged:
    lda ranged_active
    beq draw_bolt
    lda #ranged_slot
    sta draw_slot
    lda #ranged_feet-player_height-1
    jsr set_sprite_y
    lda #3
    ldx ranged_timer
    cpx #ranged_cooldown+1
    bcc :+
    lda #2              ; The lit face is visible throughout the windup.
:
    sta draw_palette
    lda #11
    sta draw_tile
    lda #<ranged_x
    ldx #>ranged_x
    jsr draw_pair
draw_bolt:
    lda bolt_active
    beq sprites_done
    lda #bolt_slot
    sta draw_slot
    lda #2
    sta draw_palette
    lda #15
    sta draw_tile
    lda #bolt_y-3
    jsr set_sprite_y
    lda bolt_x
    sec
    sbc #2
    ldx #>ranged_x
    jsr set_sprite_x
    jsr emit_sprite
sprites_done:
    lda crowd_enabled
    bne draw_crowd
    rts

hide_crowd:
    ldx #crowd_start
    lda #$f8
:
    sta sprites,x
    inx
    inx
    inx
    inx
    cpx #crowd_end
    bne :-
    rts

draw_crowd:
    ; Gameplay sprites precede the rotating stress markers in OAM.
    ldx #crowd_start
    ldy crowd_first
:
    lda sprites
    sta sprites,x
    lda #5
    sta sprites+1,x
    lda #3
    sta sprites+2,x
    tya
    asl a
    asl a
    asl a
    asl a
    adc #8              ; Carry is clear because Y is 0–7.
    sta sprites+3,x
    tya
    clc
    adc #1
    and #crowd_count-1
    tay
    inx
    inx
    inx
    inx
    cpx #crowd_end
    bne :-
    iny
    tya
    and #crowd_count-1
    sta crowd_first
    rts

; Draw adjacent 8x16 pieces around world center X (A low, X high).
draw_pair:
    jsr set_sprite_x
    sec
    lda draw_x
    sbc #8
    sta draw_x
    bcs :+
    dec draw_x+1
:
    jsr emit_sprite
    clc
    lda draw_x
    adc #8
    sta draw_x
    bcc :+
    inc draw_x+1
:
    inc draw_tile
    inc draw_tile
    jmp emit_sprite

set_sprite_x:
    sec
    sbc camera_low
    sta draw_x
    txa
    sbc camera_high
    sta draw_x+1
    rts

set_sprite_y:
    ldx scene
    cpx #4
    bne :+
    sec
    sbc pan_y
:
    sta draw_y
    rts

emit_sprite:
    ldx draw_slot
    lda draw_x+1
    bne next_sprite
    lda draw_y
    cmp #hud_rows*8-1
    bcc next_sprite
    sta sprites,x
    lda draw_tile
    sta sprites+1,x
    lda draw_palette
    sta sprites+2,x
    lda draw_x
    sta sprites+3,x
next_sprite:
    txa
    clc
    adc #4
    sta draw_slot
    rts

upload_sprites:
    lda #0
    sta $2003
    lda #>sprites
    sta $4014
    rts

draw_sprite_patterns:
    lda #>chr_sprite_address
    sta $2006
    lda #<chr_sprite_address
    sta $2006
    lda #<sprite_patterns
    sta chr_source
    lda #>sprite_patterns
    sta chr_source+1
    ldx #>CHR_SPRITE_BYTES
    lda #<CHR_SPRITE_BYTES
    jsr upload_chr_patterns
    lda #$3f
    sta $2006
    lda #$10
    sta $2006
    ldx #0
:
    lda sprite_colors,x
    sta $2007
    inx
    cpx #16
    bne :-
    rts

.segment "RODATA"
page_columns: .byte 0, 16, 32
jump_delta:
    .byte $fc, $fc, $fc, $fc, $fd, $fd, $fd, $fd
    .byte $fe, $fe, $fe, $fe, $ff, $ff, $ff, $ff
    .byte 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4
jump_length = * - jump_delta
sprite_colors:
    .byte $0f, $12, $21, $30,  $0f, $06, $26, $36
    .byte $0f, $16, $27, $30,  $0f, $17, $28, $30
sprite_patterns:
    .byte $00, $00, $07, $0f, $0f, $03, $00, $07
    .byte $00, $07, $0f, $1f, $1d, $0c, $1f, $3f
    .byte $0f, $0d, $00, $00, $1c, $1c, $3c, $3c
    .byte $3f, $3f, $3f, $1f, $03, $00, $00, $00
    .byte $00, $00, $80, $c0, $40, $00, $00, $00
    .byte $00, $c0, $e0, $f0, $f0, $e0, $e0, $f0
    .byte $80, $9c, $00, $00, $e0, $e0, $f0, $f0
    .byte $f8, $e0, $e0, $e0, $00, $00, $00, $00
    .byte $00, $00, $0c, $12, $21, $20, $48, $48
    .byte $00, $00, $00, $0c, $1e, $1f, $3f, $3f
    .byte $40, $40, $47, $20, $1f, $1b, $3b, $00
    .byte $3f, $3f, $38, $1f, $00, $00, $00, $00
    .byte $00, $00, $30, $48, $84, $04, $24, $24
    .byte $00, $00, $00, $30, $78, $f8, $f8, $f8
    .byte $04, $04, $c4, $08, $f0, $60, $70, $00
    .byte $f8, $f8, $38, $f0, $00, $00, $00, $00
    .byte $00, $70, $70, $00, $00, $00, $00, $00
    .byte $00, $f0, $f0, $00, $00, $00, $00, $00
    .byte $00, $00, $00, $00, $00, $00, $00, $00
    .byte $00, $00, $00, $00, $00, $00, $00, $00
    ; Kiln shooter: clay shell, inset face and stone foot.
    .byte $00, $03, $0f, $1f, $3f, $7f, $7f, $ff
    .byte $00, $00, $03, $0c, $10, $23, $23, $43
    .byte $ff, $ff, $7f, $7f, $3f, $1f, $3f, $7f
    .byte $43, $20, $1f, $10, $0f, $0f, $00, $7f
    .byte $00, $c0, $f0, $f8, $fc, $fe, $fe, $ff
    .byte $00, $00, $c0, $30, $08, $c4, $c4, $c2
    .byte $ff, $ff, $fe, $fe, $fc, $f8, $fc, $fe
    .byte $c2, $04, $f8, $08, $f0, $f0, $00, $fe
    ; Four-by-four spark, drawn around its world center.
    .byte $00, $60, $f0, $f0, $60, $00, $00, $00
    .byte $00, $00, $60, $60, $00, $00, $00, $00
    .byte $00, $00, $00, $00, $00, $00, $00, $00
    .byte $00, $00, $00, $00, $00, $00, $00, $00
CHR_SPRITE_BYTES = * - sprite_patterns
.export CHR_SPRITE_BYTES
.assert CHR_SPRITE_BYTES <= chr_table_bytes, error, "Sprites exceed their pattern table"
.assert CHR_SPRITE_BYTES .mod (2 * chr_tile_bytes) = 0, error, "Incomplete 8x16 sprite"
