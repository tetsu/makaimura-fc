; Bank 2 ($C000): the title screen and, in the game build (./build.ps1 -Hardware), the opening and map
; screens shown between START and Stage 1. The fixed bank keeps the entry points
; (show_title_screen, show_frontend_scene, update_frontend_scene) because they also call
; the sound driver, which maps Bank 8 into $C000. Nothing here may map another $C000 bank.

.segment "BSS"
fe_timer: .res 1

.segment "BANK2"

; Text tiles follow the order of `glyphs`: blank, 7, A, B, ... Digits use the HUD digits.
; Extra option-screen tiles: $38 = 'X', $39 = '>' (uploaded after load_menu_patterns).
.define FE_GLYPHS "7ABCDEFGHIKLMNOPRSTUVW+YJ2"
opt_x_tile = $38
opt_arrow_tile = $39
.ifdef REVG_STAGE1
title_text_base = revg_title_tile_count
title_arrow_tile = revg_title_tile_count + (CHR_TEXT_BYTES / 16)
.else
title_text_base = 0
title_arrow_tile = opt_arrow_tile
.endif

.macro fe_glyph c
    .local found
    found .set 0
    .if c = ' '
        .byte 0
        found .set 1
    .elseif c = 'X'
        .byte opt_x_tile
        found .set 1
    .elseif c = '>'
        .byte opt_arrow_tile
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

; Title-screen glyph macro (offsets into title_text_base for REVG_STAGE1).
.macro fe_title_line address, text
    .byte >(address), <(address)
    .repeat .strlen(text), index
        .local ch
        ch .set .strat(text, index)
        .if ch = ' '
            .byte 0
        .else
            .local found
            found .set 0
            .repeat .strlen(FE_GLYPHS), gi
                .if found = 0 .and .strat(FE_GLYPHS, gi) = ch
                    .byte title_text_base + gi + 1
                    found .set 1
                .endif
            .endrepeat
            .if found = 0 .and ch >= '0' .and ch <= '9'
                .byte title_text_base + campaign_zero + ch - '0'
                found .set 1
            .endif
            .if found = 0
                .error "A title string uses a character with no glyph"
            .endif
        .endif
    .endrepeat
    .byte $ff
.endmacro

; One text line: nametable address, the text, then $ff.
.macro fe_line address, text
    .byte >(address), <(address)
    .repeat .strlen(text), index
        fe_glyph {.strat(text, index)}
    .endrepeat
    .byte $ff
.endmacro

.ifdef REVG_STAGE1
; The arcade title (tools/convert-revg-frontend.py): its tiles, name table, attributes and
; palettes from the frontend bank. Display off.
draw_title_screen:
    lda prg_bank0
    pha
    lda #revg_frontend_bank
    jsr select_bank0
    lda #>chr_background_address
    sta $2006
    lda #<chr_background_address
    sta $2006
    lda #<revg_title_tiles
    sta chr_source
    lda #>revg_title_tiles
    sta chr_source+1
    ldx #>(revg_title_tile_count * 16)
    lda #<(revg_title_tile_count * 16)
    jsr upload_chr_patterns
    ; Upload menu font glyphs + '>' cursor right after the 196 arcade title tiles ($C4..$E9).
    lda #<glyphs
    sta chr_source
    lda #>glyphs
    sta chr_source+1
    ldx #>CHR_TEXT_BYTES
    lda #<CHR_TEXT_BYTES
    jsr upload_chr_patterns
    ldx #0
:
    lda opt_tile_arrow_chr,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda #$20
    sta $2006
    lda #$00
    sta $2006
    lda #<revg_title_names
    sta map_ptr
    lda #>revg_title_names
    sta map_ptr+1
    ldx #4                      ; 960 bytes: three pages and 192
    ldy #0
@names:
    lda (map_ptr),y
    sta $2007
    iny
    cpx #1
    bne :+
    cpy #192
    beq @attributes
:
    cpy #0
    bne @names
    inc map_ptr+1
    dex
    bne @names
@attributes:
    ldx #0
:
    lda revg_title_attributes,x
    sta $2007
    inx
    cpx #64
    bne :-
    ; Set palette 3 (bright white) for the 1P START / OPTION rows (rows 23-24, cols 8-23).
    lda #$23
    sta $2006
    lda #$ea
    sta $2006
    ldx #0
:
    lda revg_title_attributes + 42,x
    ora #$f0
    sta $2007
    inx
    cpx #4
    bne :-
    lda #$23
    sta $2006
    lda #$f2
    sta $2006
    ldx #0
:
    lda revg_title_attributes + 50,x
    ora #$0f
    sta $2007
    inx
    cpx #4
    bne :-
    lda #$3f
    sta $2006
    lda #0
    sta $2006
    ldx #0
:
    lda revg_title_palette,x
    sta $2007
    inx
    cpx #16
    bne :-
    pla
    jsr select_bank0
    lda #<revg_title_menu_lines
    ldx #>revg_title_menu_lines
    jsr fe_draw_lines
    jmp title_commit_vram
.else
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
    ; Upload '>' cursor tile to $0390 (tile $39) for title menu selection.
    lda #$03
    sta $2006
    lda #$90
    sta $2006
    ldx #0
:
    lda opt_tile_arrow_chr,x
    sta $2007
    inx
    cpx #16
    bne :-
    jmp title_commit_vram

.endif

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
    fe_line $21ab, "1P START"
    fe_line $21cb, "OPTION"
    fe_line $2208, "SELECT TEST MENU"
    fe_line $2248, "A JUMP  B ATTACK"
.ifdef REVG_STAGE1
    fe_line $228a, "STAGE 1 ONLY"
.else
    fe_line $2288, "7 STAGES 2 LOOPS"
.endif
    .byte 0
.ifdef REVG_STAGE1
revg_title_menu_lines:
    fe_title_line $22eb, "1P START"
    fe_title_line $230b, "OPTION"
    .byte 0
.endif

; --- Title Cursor Commit (Bank 2) ---------------------------------------------
title_commit_vram:
.ifdef REVG_STAGE1
    lda #$22
    sta $2006
    lda #$e9
    sta $2006
.else
    lda #$21
    sta $2006
    lda #$a9
    sta $2006
.endif
    ldx #0
    lda title_cursor
    bne :+
    ldx #title_arrow_tile
:
    stx $2007
.ifdef REVG_STAGE1
    lda #$23
    sta $2006
    lda #$09
    sta $2006
.else
    lda #$21
    sta $2006
    lda #$c9
    sta $2006
.endif
    ldx #0
    lda title_cursor
    beq :+
    ldx #title_arrow_tile
:
    stx $2007
    rts

; --- Option Screen (Bank 2) ---------------------------------------------------
opt_row_count = 8
.ifdef REVG_STAGE1
opt_weapon_count = 5
.else
opt_weapon_count = 4
.endif

opt_draw_screen:
    jsr load_menu_patterns
    ; Upload 'X' ($38) and '>' ($39) tiles to $0380-$039F.
    lda #$03
    sta $2006
    lda #$80
    sta $2006
    ldx #0
:
    lda opt_tile_x_chr,x
    sta $2007
    inx
    cpx #32
    bne :-
    jsr clear_screen
    lda #$3f
    sta $2006
    lda #$00
    sta $2006
    ldx #0
:
    lda opt_palette,x
    sta $2007
    inx
    cpx #4
    bne :-
    lda #<opt_static_lines
    ldx #>opt_static_lines
    jsr fe_draw_lines
    jmp opt_commit_vram

; Updates cursors and option values during VBlank or forced blank.
opt_commit_vram:
    ldx #0
@cur_loop:
    lda opt_row_hi,x
    sta $2006
    lda opt_row_cur_lo,x
    sta $2006
    lda #0
    cpx opt_cursor
    bne :+
    lda #opt_arrow_tile
:
    sta $2007
    inx
    cpx #opt_row_count
    bne @cur_loop

    ; Row 0: INVINCIBLE (OFF / ON )
    lda opt_row_hi+0
    sta $2006
    lda opt_row_val_lo+0
    sta $2006
    ldx #0
    lda opt_invincible
    beq :+
    ldx #3
:
    jsr opt_write_3

    ; Row 1: STAGE (1..7)
    lda opt_row_hi+1
    sta $2006
    lda opt_row_val_lo+1
    sta $2006
    lda opt_start_stage
    clc
    adc #campaign_zero
    sta $2007

    ; Row 2: LOOP (1..2)
    lda opt_row_hi+2
    sta $2006
    lda opt_row_val_lo+2
    sta $2006
    lda opt_start_loop
    clc
    adc #campaign_zero + 1
    sta $2007

    ; Row 3: WEAPON (6 chars)
    lda opt_row_hi+3
    sta $2006
    lda opt_row_val_lo+3
    sta $2006
    ldx opt_start_weapon
    lda opt_weapon_off,x
    tax
    ldy #6
@w_loop:
    lda opt_weapon_names,x
    sta $2007
    inx
    dey
    bne @w_loop

    ; Row 4: TIME LIMIT (ON  / OFF)
    lda opt_row_hi+4
    sta $2006
    lda opt_row_val_lo+4
    sta $2006
    ldx #3                      ; "ON " when opt_no_timer == 0
    lda opt_no_timer
    beq :+
    ldx #0                      ; "OFF" when opt_no_timer == 1
:
    jsr opt_write_3

    ; Row 5: LIVES (3 / 5 / 9)
    lda opt_row_hi+5
    sta $2006
    lda opt_row_val_lo+5
    sta $2006
    ldx opt_lives_idx
    lda opt_lives_values,x
    clc
    adc #campaign_zero
    sta $2007
    rts

opt_write_3:
    ldy #3
:
    lda opt_off_on_text,x
    sta $2007
    inx
    dey
    bne :-
    rts

; Per-frame Option input. Returns A: 0=stay, 1=redraw+SE, 2=exit to title, 3=sound test.
opt_tick:
    lda pressed
    and #$40                    ; B -> exit to title
    beq @chk_up
    lda #2
    rts
@chk_up:
    lda pressed
    and #$08                    ; Up
    beq @chk_down
    dec opt_cursor
    bpl :+
    lda #opt_row_count - 1
    sta opt_cursor
:
    lda #1
    rts
@chk_down:
    lda pressed
    and #$04                    ; Down
    beq @chk_left_right
    inc opt_cursor
    lda opt_cursor
    cmp #opt_row_count
    bcc :+
    lda #0
    sta opt_cursor
:
    lda #1
    rts
@chk_left_right:
    lda pressed
    and #$03                    ; Right ($01) / Left ($02)
    bne @adjust
    lda pressed
    and #$90                    ; A ($80) / START ($10)
    beq @stay
    ldx opt_cursor
    cpx #6
    beq @go_sound_test
    cpx #7
    beq @go_exit
    lda #$01                    ; A/START on value row cycles forward
    bne @adjust
@go_sound_test:
    lda #3
    rts
@go_exit:
    lda #2
    rts
@stay:
    lda #0
    rts

@adjust:
    tay                         ; Y bit 0 = right/inc, bit 1 = left/dec
    ldx opt_cursor
    beq @adj_inv
    cpx #1
    beq @adj_stage
    cpx #2
    beq @adj_loop
    cpx #3
    beq @adj_weapon
    cpx #4
    beq @adj_timer
    cpx #5
    beq @adj_lives
    lda #0
    rts
@adj_inv:
    lda opt_invincible
    eor #1
    sta opt_invincible
    lda #1
    rts
@adj_stage:
    tya
    and #$02
    bne @stage_dec
    inc opt_start_stage
    lda opt_start_stage
    cmp #8
    bcc @adj_done
    lda #1
    sta opt_start_stage
    bne @adj_done
@stage_dec:
    dec opt_start_stage
    bne @adj_done
    lda #7
    sta opt_start_stage
    bne @adj_done
@adj_loop:
    lda opt_start_loop
    eor #1
    sta opt_start_loop
    lda #1
    rts
@adj_weapon:
    tya
    and #$02
    bne @w_dec
    inc opt_start_weapon
    lda opt_start_weapon
    cmp #opt_weapon_count
    bcc @adj_done
    lda #0
    sta opt_start_weapon
    beq @adj_done
@w_dec:
    dec opt_start_weapon
    bpl @adj_done
    lda #opt_weapon_count - 1
    sta opt_start_weapon
    bne @adj_done
@adj_timer:
    lda opt_no_timer
    eor #1
    sta opt_no_timer
    lda #1
    rts
@adj_lives:
    tya
    and #$02
    bne @l_dec
    inc opt_lives_idx
    lda opt_lives_idx
    cmp #3
    bcc @adj_done
    lda #0
    sta opt_lives_idx
    beq @adj_done
@l_dec:
    dec opt_lives_idx
    bpl @adj_done
    lda #2
    sta opt_lives_idx
@adj_done:
    lda #1
    rts

opt_palette:
    .byte $0f, $30, $28, $16
opt_tile_x_chr:
    .byte $00, $11, $11, $0a, $04, $0a, $11, $11, $00, $00, $00, $00, $00, $00, $00, $00 ; X ($38)
opt_tile_arrow_chr:
    .byte $00, $10, $18, $1c, $1e, $1c, $18, $10, $00, $00, $00, $00, $00, $00, $00, $00 ; > ($39)
opt_row_hi:
    .byte $20, $21, $21, $21, $21, $22, $22, $22
opt_row_cur_lo:
    .byte $e4, $24, $64, $a4, $e4, $24, $64, $a4
opt_row_val_lo:
    .byte $f4, $34, $74, $b4, $f4, $34
opt_lives_values:
    .byte 3, 5, 9
opt_off_on_text:
    fe_glyph 'O'
    fe_glyph 'F'
    fe_glyph 'F'
    fe_glyph 'O'
    fe_glyph 'N'
    fe_glyph ' '
opt_weapon_off:
    .byte 0, 6, 12, 18, 24
opt_weapon_names:
    fe_glyph 'L'
    fe_glyph 'A'
    fe_glyph 'N'
    fe_glyph 'C'
    fe_glyph 'E'
    fe_glyph ' '
    fe_glyph 'D'
    fe_glyph 'A'
    fe_glyph 'G'
    fe_glyph 'G'
    fe_glyph 'E'
    fe_glyph 'R'
    fe_glyph 'T'
    fe_glyph 'O'
    fe_glyph 'R'
    fe_glyph 'C'
    fe_glyph 'H'
    fe_glyph ' '
.ifdef REVG_STAGE1
    fe_glyph 'C'
    fe_glyph 'R'
    fe_glyph 'O'
    fe_glyph 'S'
    fe_glyph 'S'
    fe_glyph ' '
    fe_glyph 'A'
    fe_glyph 'X'
    fe_glyph 'E'
    fe_glyph ' '
    fe_glyph ' '
    fe_glyph ' '
.else
    fe_glyph 'S'
    fe_glyph 'H'
    fe_glyph 'I'
    fe_glyph 'E'
    fe_glyph 'L'
    fe_glyph 'D'
.endif
opt_static_lines:
    fe_line $208c, "OPTION"
    fe_line $20e6, "INVINCIBLE"
    fe_line $2126, "STAGE"
    fe_line $2166, "LOOP"
    fe_line $21a6, "WEAPON"
    fe_line $21e6, "TIME LIMIT"
    fe_line $2226, "LIVES"
    fe_line $2266, "SOUND TEST"
    fe_line $22a6, "EXIT"
    fe_line $2326, "UP DOWN  LEFT RIGHT"
    fe_line $2346, "A START  B EXIT"
    .byte 0

title_grave_row_top:
    .byte $48, $49, $4c, $4d, $48, $49, $50, $51, $48, $49, $54, $55, $48, $49, $4c, $4d
    .byte $48, $49, $50, $51, $48, $49, $54, $55, $48, $49, $4c, $4d, $48, $49, $50, $51
title_grave_row_bot:
    .byte $4a, $4b, $4e, $4f, $4a, $4b, $52, $53, $4a, $4b, $56, $57, $4a, $4b, $4e, $4f
    .byte $4a, $4b, $52, $53, $4a, $4b, $56, $57, $4a, $4b, $4e, $4f, $4a, $4b, $52, $53
