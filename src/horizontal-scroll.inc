; At most one 16-pixel column becomes visible per frame at 1 px/frame.
; Vertical mirroring gives a 32-metatile ring across $2000 and $2400.
terrain_width = 48
camera_limit_high = (terrain_width - 16) / 16

.segment "ZEROPAGE"
camera_low: .res 1
camera_high: .res 1
column_pending: .res 1
terrain_frame: .res 1
world_column: .res 1
tile_address_low: .res 1
tile_address_high: .res 1
attribute_address: .res 1
attribute_value: .res 1
column_first_row: .res 1

.segment "BSS"
column_tiles: .res 30

.segment "CODE"
toggle_scene:
    jsr blank_display
    lda scene
    bne show_sound_screen
    lda #1
    sta scene
    jsr draw_terrain
    jmp enable_terrain

blank_display:
    jsr wait_terrain_frame
    sei
    lda #0
    sta $f000
    sta $2001
    sta $2000
    lda #$80
    sta $e000           ; Restore left/right nametables for the other scenes.
    rts

show_sound_screen:
    lda #0
    sta scene
    jsr draw_screen
enable_audio_screen:
    jsr enable_screen
    lda #0
    sta terrain_frame
    lda #$80
    sta $2000
    rts

enable_terrain:
    jsr enable_screen
    lda #0
    sta terrain_frame
    jsr apply_camera
    lda scene
    cmp #2
    beq enable_hud_sprites
    cmp #4
    bne terrain_enabled
    lda #$18            ; Match the clipped left edge of the two-axis camera.
    jmp enable_terrain_sprites
enable_hud_sprites:
    lda #$1e
enable_terrain_sprites:
    sta $2001
    cli
terrain_enabled:
    rts

draw_terrain:
    lda #0
    sta camera_low
    sta camera_high
    sta world_column
    jsr load_terrain_palette
fill_ring:
    jsr prepare_column
    lda #4
    jsr upload_column
    inc world_column
    lda world_column
    cmp #32
    bne fill_ring
    lda #0
    sta $2000
    rts

load_terrain_palette:
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    ldx #0
:
    lda terrain_colors,x
    sta $2007
    inx
    cpx #16
    bne :-
    rts

; Prepare new tiles before waiting for VBlank. The existing column stays
; resident until its replacement is about to enter the viewport.
; A: 0 = still, 1 = right, 2 = left.
step_camera:
    ldx #0
    stx column_pending
    cmp #2
    beq move_left
    cmp #1
    bne stationary
    lda camera_high
    cmp #camera_limit_high
    beq stationary
    inc camera_low
    bne :+
    inc camera_high
:
    lda camera_low
    and #15
    cmp #1
    bne stationary
    jsr camera_metatile
    clc
    adc #16
    jmp load_column
move_left:
    lda camera_low
    bne :+
    lda camera_high
    beq stationary
    dec camera_high
:
    dec camera_low
    lda camera_low
    and #15
    cmp #15
    bne stationary
    jsr camera_metatile
load_column:
    sta world_column
    jsr prepare_column
    inc column_pending
stationary:
    rts

camera_metatile:
    lda camera_high
    asl a
    asl a
    asl a
    asl a
    sta world_column
    lda camera_low
    lsr a
    lsr a
    lsr a
    lsr a
    ora world_column
    rts

prepare_column:
    lda world_column
    and #16
    lsr a
    lsr a
    ora #$20
    sta tile_address_high
    lda world_column
    and #15
    asl a
    sta tile_address_low
    lsr a
    lsr a
    ora #$c0
    sta attribute_address

    ; Both neighbors in each 32-pixel attribute cell use the same palette.
    lda world_column
    lsr a
    tax
    lda terrain_attributes,x
    sta attribute_value
    ldx world_column
    lda terrain_heights,x
    asl a
    tay
    ldx #0
    lda #0
:
    sta column_tiles,x
    inx
    dey
    bne :-
    lda #(terrain_tiles - glyphs) / 16
    sta column_tiles-1,x
    clc
    adc #1
    sta column_tiles,x
    inx
    lda #(terrain_tiles - glyphs) / 16 + 2
:
    sta column_tiles,x
    eor #1              ; Alternate between the two rock textures.
    inx
    cpx #30
    bne :-
    jmp decorate_column

decorate_column:
    ldy world_column
    cpy #lantern_column
    beq stage_scenery
    cpy #carving_column
    bne column_decorated
stage_scenery:
    ldx #scenery_first_row
:
    txa
    ldy world_column
    jsr scenery_tile
    bcc :+
    sta column_tiles,x
:
    inx
    cpx #carving_row+1
    bne :--
column_decorated:
    rts

; With rendering enabled this routine runs only in VBlank. In gameplay,
; skip the four HUD rows and their attribute byte in both nametables.
; A selects increment-by-32 and retains the current NMI enable bit.
upload_column:
    ldx #0
    stx column_first_row
    cmp #$84
    bne set_column_control
    ldx scene
    cpx #2
    bne set_column_control
    ldx #hud_rows
    stx column_first_row
    lda tile_address_low
    ora #hud_rows*32
    sta tile_address_low
    lda attribute_address
    ora #8
    sta attribute_address
    lda #$a4
set_column_control:
    sta $2000
    lda tile_address_high
    sta $2006
    lda tile_address_low
    sta $2006
    jsr write_tiles
    lda tile_address_high
    sta $2006
    lda tile_address_low
    ora #1
    sta $2006
    jsr write_tiles
    lda tile_address_high
    ora #3
    tay
    lda attribute_address
    tax
:
    sty $2006
    stx $2006
    lda attribute_value
    sta $2007
    txa
    clc
    adc #8
    tax
    bcc :-
    rts

write_tiles:
    ldx column_first_row
:
    lda column_tiles,x
    sta $2007
    inx
    cpx #30
    bne :-
    rts

apply_camera:
    ldx scene
    cpx #2
    beq reset_hud_scroll
    cpx #4
    beq reset_hud_scroll
    lda camera_high
    and #1
    ora #$80
    sta $2000
    lda camera_low
    sta $2005
    lda #0
    sta $2005
    rts

reset_hud_scroll:
    lda #hud_y
    cpx #4
    bne :+
    clc
    adc pan_y
:
    sta split_y
    lda camera_high
    and #1
    asl a
    asl a
    sta split_address_high
    lda split_y
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    ora split_address_high
    sta split_address_high
    lda split_y
    and #$38
    asl a
    asl a
    sta split_address_low
    lda camera_low
    sta split_x
    lsr a
    lsr a
    lsr a
    ora split_address_low
    sta split_address_low
    lda #$a0            ; Nametable 0, 8x16 sprites, NMI enabled.
    sta $2000
    lda #0
    sta $2005
    sta $2005
    rts

wait_terrain_frame:
    lda terrain_frame
    beq wait_terrain_frame
    lda #0
    sta terrain_frame
    rts

terrain_nmi:
    pha
    lda scene
    cmp #2
    beq :+
    cmp #4
    bne :++
:
    lda #2              ; One IRQ in scanline mode, then disable on ack.
    sta $f000
:
    inc terrain_frame
    pla
    rti

.segment "RODATA"
terrain_colors:
    .byte $0f, $2a, $09, $19,  $0f, $38, $17, $27
    .byte $0f, $31, $11, $21,  $0f, $34, $14, $24

; Original terraced terrain. Each height counts 16-pixel rows from the top.
terrain_heights:
    .byte 12, 12, 11, 11, 10, 10, 11, 12, 12, 13, 11, 11, 11, 11, 11, 11
    .byte 11, 11, 12, 12, 12, 12, 12, 11, 11, 10, 9,  8,  8,  9,  10, 11
    .byte 12, 13, 12, 11, 10, 9,  8,  9,  9,  10, 11, 12, 11, 10, 10, 11
.assert * - terrain_heights = terrain_width, error, "Unexpected terrain width"

terrain_attributes:
    .byte $00, $00, $55, $55, $ff, $ff, $aa, $aa
    .byte $ff, $ff, $aa, $55, $55, $00, $ff, $ff
    .byte $55, $55, $aa, $aa, $ff, $00, $55, $55
.assert * - terrain_attributes = terrain_width / 2, error, "Unexpected attribute count"
