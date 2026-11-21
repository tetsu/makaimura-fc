; At most one 16-pixel column becomes visible per frame at 1 px/frame.
; Vertical mirroring gives a 32-metatile ring across $2000 and $2400.

.segment "ZEROPAGE"
camera_low: .res 1
camera_high: .res 1
column_pending: .res 1
terrain_frame: .res 1
render_pending: .res 1
world_column: .res 1
tile_address_low: .res 1
tile_address_high: .res 1
attribute_address: .res 1
attribute_value: .res 1
column_first_row: .res 1

.segment "BSS"
column_tiles: .res 30
column_right_tiles: .res 30

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
    sta render_pending
    lda #$80
    sta $e000           ; Restore left/right nametables for the other scenes.
    jmp stop_shot_sound

show_sound_screen:
    lda #0
    jsr select_bank0    ; Previews and horizontal play use the original terrain.
    sta scene
.if PRG_ROM_UNITS = 32
    sta campaign_stage
    sta arcade_mode
    lda #1
    jsr select_bank1
.endif
    jsr draw_screen
enable_static_screen:
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
    cmp stage_cam_x_max_hi
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
    lda #$ff
    sta map_cached_col
    lda stage_col_pal_ptr
    sta map_ptr
    lda stage_col_pal_ptr+1
    sta map_ptr+1
    ldy world_column
    lda (map_ptr),y
    tay
    lda terrain_palette_full,y
    sta attribute_value
    lda world_column
    jsr set_map_col_ptr
    ldy #0
    ldx #0
:
    tya
    pha
    lda (map_ptr),y
    asl a
    asl a
    tay
    lda terrain_metatiles,y
    sta column_tiles,x
    lda terrain_metatiles+1,y
    sta column_right_tiles,x
    inx
    lda terrain_metatiles+2,y
    sta column_tiles,x
    lda terrain_metatiles+3,y
    sta column_right_tiles,x
    inx
    pla
    tay
    iny
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
    sta column_right_tiles,x
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
    jsr write_right_tiles
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

write_right_tiles:
    ldx column_first_row
:
    lda column_right_tiles,x
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
restore_hud_view:
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

; Publish only after preparing all VRAM and OAM data. Main keeps the
; buffers intact until the NMI acknowledges the entire transfer.
submit_frame:
    lda #1
    sta render_pending
:
    lda render_pending
    bne :-
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
    lda render_pending
    bne nmi_commit
    ; A repeated frame still needs the fixed HUD view above the raster split.
    lda scene
    cmp #2
    beq nmi_keep_hud
    cmp #4
    bne nmi_done
nmi_keep_hud:
    jsr restore_hud_view
    jmp nmi_done
nmi_commit:
    txa
    pha
    tya
    pha
    jsr commit_frame
    lda #0
    sta render_pending
    pla
    tay
    pla
    tax
nmi_done:
    pla
    rti
