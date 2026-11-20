.setcpu "6502"

.segment "HEADER"
; NES 2.0, mapper $055, submapper 2, 32 KiB PRG, 8 KiB each of WRAM and CHR-RAM.
.byte "NES", $1a, 2, 0, $50, $58, $20, 0, 7, 7, 0, 0, 0, 0

.segment "BANK0"
.byte $a5
.segment "BANK1"
.byte $5a
.segment "BANK2"
.byte $3c
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

.include "fixed-hud.inc"
.include "horizontal-scroll.inc"
.include "playable-scene.inc"

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
    sta $8000
    lda #1
    sta $8010
    lda #2
    sta $9000
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
    lda $8000
    cmp #$a5
    bne memory_error
    lda $a000
    cmp #$5a
    bne memory_error
    lda $c000
    cmp #$3c
    bne memory_error
    lda #1
    sta $8000
    lda $8000
    cmp #$5a
    bne memory_error
    lda #0
    sta $8000
    lda #$96
    sta $6000
    cmp $6000
    bne memory_error
    lda #$69
    sta $6000
    cmp $6000
    bne memory_error
    lda #0
    sta previous
    sta mode
    sta frames
    sta channel
    sta phrase
    sta scene
    jsr enable_screen
    jmp main

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
    lda pressed
    and #$10            ; Start: enter or restart the playable scene.
    beq :+
    jsr show_game
    jmp audio_tick
:
    lda scene
    beq sound_screen
    cmp #2
    bne free_camera
    jsr update_game
    jmp render_terrain
free_camera:
    lda pad
    and #3
    jsr step_camera
render_terrain:
    jsr wait_terrain_frame
    lda column_pending
    beq :+
    lda #$84
    jsr upload_column
:
    jsr apply_camera
    lda scene
    cmp #2
    bne :+
    jsr upload_sprites
:
    jmp audio_tick
sound_screen:
    jsr wait_frame
    lda pressed
    and #$07
    beq audio_tick
    tay
    ldx #1              ; left: FM
    and #$02
    bne select_mode
    ldx #2              ; right: APU
    tya
    and #$01
    bne select_mode
    ldx #3              ; down: both
select_mode:
    stx mode
    jsr silence
    lda #0
    sta phrase
    sta frames
audio_tick:
    lda frames
    beq play
    dec frames
    jmp main
play:
    lda mode
    bne play_phrase
    jsr silence
    ldx channel
    txa
    tay
    jsr note_on
    inc channel
    lda channel
    cmp #6
    bne solo_wait
    lda #3
    sta mode
solo_wait:
    lda #15
    sta frames
    jmp main

play_phrase:
    jsr silence
    lda mode
    and #1
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
    and #2
    beq next_phrase
    lda #1
    sta $4015
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

silence:
    lda #0
    sta $4015
    ldx #5
:
    txa
    ora #$20
    ldy #0
    jsr fm_write
    dex
    bpl :-
    rts

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

draw_screen:
    lda #0
    sta $2006
    sta $2006
    ldx #0
:
    lda glyphs,x
    sta $2007
    inx
    bne :-
    ldx #0
:
    lda glyphs+256,x
    sta $2007
    inx
    cpx #glyph_size-256
    bne :-
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
    cpy #6
    bne next_line
    rts

.segment "RODATA"
text_address: .byte $08, $68, $88, $a8, $c8, $e8
text_start: .byte 0, line2-screen_text, line3-screen_text, line4-screen_text, line5-screen_text, line6-screen_text
screen_text: .byte $15, $11, $04, $01, $02, $00, $12, $0f, $14, $0e, $05, $00, $13, $06, $12, $13, $ff
line2: .byte $0c, $06, $07, $13, $00, $07, $0d, $ff
line3: .byte $11, $0a, $08, $09, $13, $00, $02, $10, $14, $ff
line4: .byte $05, $0f, $16, $0e, $00, $03, $0f, $13, $09, $ff
line5: .byte $12, $06, $0c, $06, $04, $13, $00, $13, $06, $11, $11, $02, $0a, $0e, $ff ; SELECT TERRAIN
line6: .byte $12, $13, $02, $11, $13, $00, $10, $0c, $02, $17, $ff ; START PLAY
error_text: .byte $03, $02, $0e, $0b, $00, $0f, $11, $00, $11, $02, $0d, $00, $06, $11, $11, $0f, $11
error_length = * - error_text
pulse_period:
    .byte $fc, 0, 0, 0, 0, 0, $d0, 0, 0, 0, 0, 0, $e0
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
terrain_tiles:
    ; Pebbles, ledge, and two alternating rock textures.
    .byte $00, $00, $10, $38, $10, $00, $02, $00, $00, $00, $00, $10, $00, $00, $00, $00
    .byte $ff, $ff, $b7, $42, $00, $10, $02, $00, $00, $6d, $00, $00, $00, $00, $00, $00
    .byte $c4, $28, $10, $03, $8f, $02, $10, $30, $ff, $ef, $ff, $ff, $3f, $ff, $e7, $ff
    .byte $0c, $10, $40, $84, $cc, $02, $00, $08, $ff, $e7, $ff, $7b, $ff, $ff, $ff, $f7
glyph_size = * - glyphs

.segment "VECTORS"
.word terrain_nmi, reset, hud_irq
