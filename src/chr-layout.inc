; The cartridge has two 4 KiB pattern tables. Background/HUD use
; $0000 and odd OAM tile indices select the $1000 table in 8x16 mode.
chr_background_address = $0000
chr_sprite_address = $1000
chr_table_bytes = $1000
chr_tile_bytes = 16

.segment "ZEROPAGE"
chr_source: .res 2
chr_tail_bytes: .res 1

.segment "CODE"
; With rendering off, copy X full pages and A remaining bytes from
; chr_source to the current PPU address.
upload_chr_patterns:
    sta chr_tail_bytes
    ldy #0
    cpx #0
    beq chr_tail
chr_page:
:
    lda (chr_source),y
    sta $2007
    iny
    bne :-
    inc chr_source+1
    dex
    bne chr_page
chr_tail:
    cpy chr_tail_bytes
    beq chr_loaded
:
    lda (chr_source),y
    sta $2007
    iny
    cpy chr_tail_bytes
    bne :-
chr_loaded:
    rts
