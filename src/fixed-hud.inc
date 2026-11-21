; The HUD occupies four rows from the first nametable. Scanline 31 is
; blank, so the IRQ can change scroll before the playfield starts at 32.
hud_rows = 4
hud_y = hud_rows * 8 - 1
hud_irq_lines = 52          ; (262 - 241) + 31 NTSC scanlines

.segment "ZEROPAGE"
split_x: .res 1
split_y: .res 1
split_address_high: .res 1
split_address_low: .res 1

.segment "CODE"
draw_hud:
    ; Initial terrain upload populated these attributes. Use palette zero
    ; across the HUD, independently of the terrain palettes.
    lda #$23
    sta $2006
    lda #$c0
    sta $2006
    lda #0
    ldx #8
:
    sta $2007
    dex
    bne :-
    lda #$20
    sta $2006
    lda #$24
    sta $2006
    ldx #0
:
    lda hud_controls,x
    sta $2007
    inx
    cpx #hud_controls_length
    bne :-
    lda #$20
    sta $2006
    lda #$44
    sta $2006
    ldx #0
:
    lda hud_options,x
    sta $2007
    inx
    cpx #hud_options_length
    bne :-
    lda #256-hud_irq_lines
    sta $e010
    rts

hud_irq:
    pha
    sta $f010           ; Any value acknowledges. Save two cycles before the split.
    bit $2002           ; First write of the $2006/$2005 sequence.
    lda split_address_high
    sta $2006
    lda split_y
    sta $2005
    lda split_x
    sta $2005
    lda split_address_low
    sta $2006           ; Commit both X and Y to the current VRAM address.
    pla
    rti

.segment "RODATA"
hud_controls:
    ; A JUMP   B FIRE
    .byte $02, $00, $18, $14, $0d, $10, $00, $00, $00, $03, $00, $07, $0a, $11, $06
hud_controls_length = * - hud_controls
hud_options:
    ; UP LOAD   START RETRY
    .byte $14, $10, $00, $0c, $0f, $02, $05, $00, $00, $00
    .byte $12, $13, $02, $11, $13, $00, $11, $06, $13, $11, $17
hud_options_length = * - hud_options
