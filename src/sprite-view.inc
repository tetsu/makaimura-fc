; Sprite diagnostics use the gameplay player renderer in Bank 3.
.segment "BSS"
sprite_view_pose: .res 1
sprite_view_tick: .res 1
sprite_view_paused: .res 1

.segment "BANK3"
show_sprite_view:
    jsr blank_display
    jsr call_bank8_audio_init
    lda #sprite_view_scene
    sta scene
    lda #arcade_stage_bank
    jsr select_bank0
    jsr ap_load_stage_visuals  ; Loads the OBJ patterns and palette.
    jsr load_menu_patterns
    jsr clear_screen
    ldx #0
    ldy #0
@row:
    lda sprite_view_row_hi,y
    sta $2006
    lda sprite_view_row_lo,y
    sta $2006
@tile:
    lda sprite_view_text,x
    inx
    cmp #$ff
    beq @next_row
    sta $2007
    jmp @tile
@next_row:
    iny
    cpy #sprite_view_row_count
    bne @row
    lda #0
    sta sprite_view_pose
    sta sprite_view_tick
    sta sprite_view_paused
    sta player_left
    sta player_x+1
    sta ap_player_y_hi
    sta camera_low
    sta camera_high
    sta ap_cam_y_lo
    sta ap_cam_y_hi
    lda #120
    sta player_feet
    lda #128
    sta player_x
    lda #$f8
    ldx #0
@hide:
    sta sprites,x
    inx
    bne @hide
    jsr draw_sprite_view
    jsr commit_sprite_view
    jsr enable_static_screen
    lda #$a0           ; NMI on, 8x16 sprites.
    sta $2000
    lda #$1e
    sta $2001
    rts

update_sprite_view:
    ldx sprite_view_pose
    lda pressed
    and #$03
    cmp #$01
    beq @next
    cmp #$02
    bne @flip
    dex
    bpl @save_pose
    ldx #sprite_view_pose_count - 1
    bne @save_pose
@next:
    inx
    cpx #sprite_view_pose_count
    bcc @save_pose
    ldx #0
@save_pose:
    stx sprite_view_pose
    lda #0
    sta sprite_view_tick
@flip:
    lda pressed
    and #$40           ; B flips the current pose.
    beq @play_pause
    lda player_left
    eor #1
    sta player_left
@play_pause:
    lda pressed
    and #$80           ; A pauses and advances one frame.
    beq @start
    lda #1
    sta sprite_view_paused
    lda sprite_view_tick
    eor #8
    sta sprite_view_tick
    jmp @draw
@start:
    lda pressed
    and #$10           ; START toggles playback.
    beq @advance
    lda sprite_view_paused
    eor #1
    sta sprite_view_paused
@advance:
    lda sprite_view_paused
    bne @draw
    inc sprite_view_tick
@draw:
    jsr draw_sprite_view
    jmp submit_frame

draw_sprite_view:
    lda #$f8           ; Crouching leaves the top two slots unused.
    sta sprites
    sta sprites+4
    sta sprites+8
    sta sprites+12
    ldx sprite_view_pose
    lda sprite_view_armor,x
    sta ap_armor
    lda sprite_view_state,x
    sta ap_state
    lda sprite_view_crouch,x
    sta ap_crouch
    lda sprite_view_walk,x
    beq @draw
    lda sprite_view_tick
    and #8
    ora #1
@draw:
    jmp ap_draw_player

; Called during forced blank on entry and by NMI while Bank 3 stays mapped.
commit_sprite_view:
    lda #$21
    sta $2006
    lda #$08
    sta $2006
    ldx sprite_view_pose
    ldy #0
    lda sprite_view_armor,x
    cmp #ap_armor_full
    beq @armor_text
    ldy #4
@armor_text:
    lda sprite_view_armor_names,y
    sta $2007
    lda sprite_view_armor_names+1,y
    sta $2007
    lda sprite_view_armor_names+2,y
    sta $2007
    lda sprite_view_armor_names+3,y
    sta $2007
    lda #0
    sta $2007
    sta $2007
    lda sprite_view_name_offset,x
    tay
    ldx #6
@name:
    lda sprite_view_names,y
    sta $2007
    iny
    dex
    bne @name
    lda #0
    sta $2007
    sta $2007
    ldx sprite_view_pose
    lda sprite_view_walk,x
    beq @digit
    lda sprite_view_tick
    and #8
    beq @digit
    lda #1
@digit:
    clc
    adc #campaign_zero + 1
    sta $2007
    lda #0
    sta $2005
    sta $2005
    jmp upload_sprites

sprite_view_armor:
    .byte ap_armor_full, ap_armor_full, ap_armor_full, ap_armor_full, ap_armor_full
    .byte ap_armor_naked, ap_armor_naked, ap_armor_naked, ap_armor_naked
sprite_view_pose_count = * - sprite_view_armor
sprite_view_state:
    .byte ap_state_ground, ap_state_ground, ap_state_air, ap_state_ground, ap_state_ladder
    .byte ap_state_ground, ap_state_ground, ap_state_air, ap_state_ground
sprite_view_crouch: .byte 0, 0, 0, 1, 0, 0, 0, 0, 1
sprite_view_walk: .byte 0, 1, 0, 0, 0, 0, 1, 0, 0
sprite_view_name_offset: .byte 0, 6, 12, 18, 24, 0, 6, 12, 18
sprite_view_armor_names: .byte $07, $14, $0c, $0c, $0e, $0f, $0e, $06 ; FULL / NONE
sprite_view_names:
    .byte $12, $13, $02, $0e, $05, $00 ; STAND
    .byte $16, $02, $0c, $0b, $00, $00 ; WALK
    .byte $19, $14, $0d, $10, $00, $00 ; JUMP
    .byte $04, $11, $0f, $14, $04, $09 ; CROUCH
    .byte $0c, $02, $05, $05, $06, $11 ; LADDER
sprite_view_row_hi: .byte $20, $20, $22, $22, $22, $22, $22
sprite_view_row_count = * - sprite_view_row_hi
sprite_view_row_lo: .byte $88, $e8, $08, $28, $48, $68, $a8
sprite_view_text:
    .byte $12, $10, $11, $0a, $13, $06, $00, $15, $0a, $06, $16, $ff ; SPRITE VIEW
    .byte $02, $11, $0d, $0f, $11, $00, $10, $0f, $12, $06, $00, $00, $07, $11, $02, $0d, $06, $ff ; ARMOR POSE  FRAME
    .byte $0c, $06, $07, $13, $00, $11, $0a, $08, $09, $13, $00, $10, $0f, $12, $06, $ff ; LEFT RIGHT POSE
    .byte $12, $13, $02, $11, $13, $00, $10, $0c, $02, $18, $00, $10, $02, $14, $12, $06, $ff ; START PLAY PAUSE
    .byte $02, $00, $12, $13, $06, $10, $ff ; A STEP
    .byte $03, $00, $07, $0c, $0a, $10, $ff ; B FLIP
    .byte $12, $06, $0c, $06, $04, $13, $00, $0d, $06, $0e, $14, $ff ; SELECT MENU

.segment "RODATA"
sprite_view_menu: .byte $11, $0a, $08, $09, $13, $17, $02, $00, $12, $10, $11, $0a, $13, $06, $00, $15, $0a, $06, $16 ; RIGHT+A SPRITE VIEW
sprite_view_menu_length = * - sprite_view_menu
