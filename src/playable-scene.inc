; The encounter uses the same scrolling terrain and an independent sprite table.
enemy_x = 272
enemy_feet = 176
player_height = 16
jump_fall = 18

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
old_x: .res 2
ground: .res 1
draw_x: .res 2
draw_y: .res 1
draw_tile: .res 1
draw_palette: .res 1
draw_slot: .res 1

.segment "OAM"
sprites: .res 256

.segment "CODE"
show_game:
    jsr blank_display
    lda #2
    sta scene
    jsr draw_terrain
    jsr draw_sprite_patterns
    ldx #0
    lda #$f8
:
    sta sprites,x
    inx
    bne :-
    lda #176
    sta player_x
    sta player_feet
    lda #0
    sta player_x+1
    sta player_hit
    sta player_left
    sta shot_life
    lda #$ff
    sta jump_step
    lda #1
    sta enemy_active
    jsr draw_sprites
    jsr upload_sprites
    jmp enable_terrain

update_game:
    lda player_hit
    bne game_camera
    jsr move_player
    jsr move_vertically
    jsr update_shot
    jsr check_enemy_contact
game_camera:
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
    jsr step_camera
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
    cmp #2
    bne advance_player
    lda player_x
    cmp #248
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

; A = world X low byte, X = high byte (0–2). A returns floor Y in pixels.
floor_height:
    lsr a
    lsr a
    lsr a
    lsr a
    ora page_columns,x
    tax
    lda terrain_heights,x
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

draw_sprites:
    ; Hide the five used entries before drawing visible pieces.
    lda #$f8
    ldx #16
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
    sec
    lda player_feet
    sbc #player_height+1
    sta draw_y
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
    sta draw_y
    lda #5
    sta draw_tile
    lda #<enemy_x
    ldx #>enemy_x
    jsr draw_pair
draw_shot:
    lda shot_life
    beq sprites_done
    lda #16
    sta draw_slot
    lda #3
    sta draw_palette
    lda #9
    sta draw_tile
    lda shot_y
    sec
    sbc #2
    sta draw_y
    lda shot_x
    ldx shot_x+1
    jsr set_sprite_x
    jsr emit_sprite
sprites_done:
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

emit_sprite:
    ldx draw_slot
    lda draw_x+1
    bne next_sprite
    lda draw_y
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
    lda #$10
    sta $2006
    lda #0
    sta $2006
    ldx #0
:
    lda sprite_patterns,x
    sta $2007
    inx
    cpx #sprite_pattern_size
    bne :-
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
sprite_pattern_size = * - sprite_patterns
