; Bank 2 ($C000): the title screen and, in the -RevgStage1 game, the opening and map
; screens shown between START and Stage 1. The fixed bank keeps the entry points
; (show_title_screen, show_frontend_scene, update_frontend_scene) because they also call
; the sound driver, which maps Bank 8 into $C000. Nothing here may map another $C000 bank.

.segment "BSS"
fe_timer: .res 1

.segment "BANK2"

; Text tiles follow the order of `glyphs`: blank, 7, A, B, ... Digits use the HUD digits.
.define FE_GLYPHS "7ABCDEFGHIKLMNOPRSTUVW+YJ2"

.macro fe_glyph c
    .local found
    found .set 0
    .if c = ' '
        .byte 0
        found .set 1
    .endif
    .repeat .strlen(FE_GLYPHS), index
        .if found = 0 .and .strat(FE_GLYPHS, index) = c
            .byte index + 1
            found .set 1
        .endif
    .endrepeat
    .if found = 0 .and c >= '0' .and c <= '9'
        .byte campaign_zero + c - '0'
        found .set 1
    .endif
    .if found = 0
        .error "A frontend string uses a character with no glyph"
    .endif
.endmacro

; One text line: nametable address, the text, then $ff.
.macro fe_line address, text
    .byte >(address), <(address)
    .repeat .strlen(text), index
        fe_glyph {.strat(text, index)}
    .endrepeat
    .byte $ff
.endmacro

draw_title_screen:
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
    jsr upload_chr_patterns
    ; Upload authentic Makaimura graveyard/ground BG tiles to $0400 from BANK10
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
    jsr select_bank0
    jsr clear_screen
    ; Title palettes: 0=white text, 1=gold/crimson banner, 2=grass/earth, 3=graveyard fence/cross
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    ldx #0
:
    lda title_palette,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda #<title_lines
    ldx #>title_lines
    jsr fe_draw_lines
    ; Draw authentic Makaimura graveyard silhouette across rows 22-23 ($22C0..$22FF)
    lda #$22
    sta $2006
    lda #$c0
    sta $2006
    ldx #0
@title_grave_top:
    lda title_grave_row_top,x
    sta $2007
    inx
    cpx #32
    bne @title_grave_top
    ldx #0
@title_grave_bot:
    lda title_grave_row_bot,x
    sta $2007
    inx
    cpx #32
    bne @title_grave_bot
    ; Draw authentic grassy ground across rows 24-25 ($2300..$233F)
    lda #$23
    sta $2006
    lda #$00
    sta $2006
    ldx #16
:
    lda #$40
    sta $2007
    lda #$41
    sta $2007
    dex
    bne :-
    ldx #16
:
    lda #$42
    sta $2007
    lda #$43
    sta $2007
    dex
    bne :-
    ; Draw authentic soil across rows 26-27 ($2340..$237F)
    ldx #16
:
    lda #$44
    sta $2007
    lda #$45
    sta $2007
    dex
    bne :-
    ldx #16
:
    lda #$46
    sta $2007
    lda #$47
    sta $2007
    dex
    bne :-
    ; Attribute table: palette 1 (gold) for banner rows 4..11, palette 3 (graveyard) for rows 20..23, palette 2 (grass/earth) for rows 24..27
    lda #$23
    sta $2006
    lda #$c8
    sta $2006
    lda #$55
    ldx #16
:
    sta $2007
    dex
    bne :-
    lda #$23
    sta $2006
    lda #$e8
    sta $2006
    lda #$ff
    ldx #8
:
    sta $2007
    dex
    bne :-
    lda #$aa
    ldx #8
:
    sta $2007
    dex
    bne :-
    rts

; A/X = low/high address of fe_line records ended by a zero byte.
fe_draw_lines:
    sta map_ptr
    stx map_ptr+1
    ldy #0
@line:
    lda (map_ptr),y
    beq @done
    sta $2006
    iny
    lda (map_ptr),y
    sta $2006
    iny
@char:
    lda (map_ptr),y
    iny
    cmp #$ff
    beq @line
    sta $2007
    jmp @char
@done:
    rts

.ifdef REVG_STAGE1
; The arcade plays the kidnapping and then the map of the route after 1P START. Both are
; text for now (TODO C-1b replaces them with the extracted pictures).
fe_opening_frames = 240
fe_map_frames = 150

; Draw the scene in `scene` while the display is off and start its timer.
fe_draw_scene:
    jsr clear_screen
    lda scene
    cmp #opening_scene
    bne @map
    lda #fe_opening_frames
    sta fe_timer
    lda #<fe_opening_lines
    ldx #>fe_opening_lines
    jmp fe_draw_lines
@map:
    lda #fe_map_frames
    sta fe_timer
    lda #<fe_map_lines
    ldx #>fe_map_lines
    jmp fe_draw_lines

; Once per frame. A = 0 to stay, the next scene, or $ff to start Stage 1. START skips.
fe_tick:
    lda pressed
    and #$10
    bne @next
    dec fe_timer
    bne @stay
@next:
    lda scene
    cmp #opening_scene
    bne @start
    lda #map_scene
    rts
@start:
    lda #$ff
    rts
@stay:
    lda #0
    rts

fe_opening_lines:
    fe_line $216a, "THE PRINCESS"
    fe_line $21ab, "WAS TAKEN"
    fe_line $21ec, "BY SATAN"
    .byte 0
fe_map_lines:
    fe_line $218c, "STAGE 1"
    fe_line $21cb, "GRAVEYARD"
    .byte 0
.endif

title_palette:
    .byte $0f, $30, $16, $28
    .byte $0f, $28, $16, $30
    .byte $0f, $07, $17, $1a
    .byte $0f, $00, $10, $1a
title_lines:
    fe_line $20ca, "MAKAIMURA FC"
    fe_line $2109, "VRC7A EDITION"
    fe_line $21aa, "PRESS START"
    fe_line $2208, "SELECT TEST MENU"
    fe_line $2248, "A JUMP  B ATTACK"
.ifdef REVG_STAGE1
    fe_line $228a, "STAGE 1 ONLY"
.else
    fe_line $2288, "7 STAGES 2 LOOPS"
.endif
    .byte 0
title_grave_row_top:
    .byte $48, $49, $4c, $4d, $48, $49, $50, $51, $48, $49, $54, $55, $48, $49, $4c, $4d
    .byte $48, $49, $50, $51, $48, $49, $54, $55, $48, $49, $4c, $4d, $48, $49, $50, $51
title_grave_row_bot:
    .byte $4a, $4b, $4e, $4f, $4a, $4b, $52, $53, $4a, $4b, $56, $57, $4a, $4b, $4e, $4f
    .byte $4a, $4b, $52, $53, $4a, $4b, $56, $57, $4a, $4b, $4e, $4f, $4a, $4b, $52, $53
