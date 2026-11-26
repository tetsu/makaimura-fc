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
    jmp fe_draw_progress_map

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

; --- Stage Progression Map Screen (Bank 2, REVG_STAGE1) -----------------------
; Draws the authentic arcade Makai panorama (from Bank 11: revg-map-data.inc) in
; rows 16..27 ($2200/$2600, continuous 399px scrolled 0..143 via VRC7a scanline IRQ
; at scanline 127), stationary arcade HUD ("PLAYER 1", "TOP SCORE", live scores,
; "PLAYER ONE READY") in rows 0..15 ($2000), and the real 16x16 blinking Arthur
; helmet pin sprite ($25B) at exact arcade $6485/$6493 coordinates. Display must be off.
fe_draw_progress_map:
    jsr load_menu_patterns
    ; Keep 'X' at tile $38 ($0380) for "NEXT STAGE" / clear banners if needed.
    lda #$03
    sta $2006
    lda #$80
    sta $2006
    ldx #0
@x_chr_loop:
    lda opt_tile_x_chr,x
    sta $2007
    inx
    cpx #16
    bne @x_chr_loop
    jsr clear_screen
    ; Compute target stage index (0..6):
    ; - fe_map_mode != 0 (Opening / Miss Retry): current stage (campaign_stage - 1)
    ; - fe_map_mode == 0 (Stage Clear): next stage (campaign_stage mod 7, or Stage 5 on S6 bounce)
    lda fe_map_mode
    beq @map_mode_clear
    lda campaign_stage
    beq @map_stage_zero
    sec
    sbc #1
    bpl @have_next
@map_stage_zero:
    lda #0
    beq @have_next
@map_mode_clear:
    lda campaign_stage
    cmp #6
    bne @not_s6_bounce
    lda ap_weapon
    cmp #ap_weapon_shield
    beq @normal_next
    lda #4                      ; bounce back to Stage 5
    bne @have_next
@not_s6_bounce:
    cmp #7
    bcc @normal_next
    lda #0                      ; Loop 1 Stage 7 -> Loop 2 Stage 1
    beq @have_next
@normal_next:
    lda campaign_stage
@have_next:
    sta map_tmp                 ; save target stage index (0..6)

    ; Map Bank 11 ($8000) to upload the continuous 399px arcade Makai panorama ($2000 + $2400)
    ; and the authentic 16x16 Arthur marker pin sprite ($1000..$103F).
    lda #$80
    sta $e000                   ; Vertical mirroring: $2000 left half + $2400 right half
    lda #0
    sta fe_map_scroll_x
    lda prg_bank0
    pha
    lda #revg_map_bank_id
    jsr select_bank0
    ; Upload shared panorama CHR tiles to $0400 (tile $40+).
    lda #$04
    sta $2006
    lda #$00
    sta $2006
    lda #<revg_map_tiles
    sta chr_source
    lda #>revg_map_tiles
    sta chr_source+1
    ldx #>(revg_map_tile_count * 16)
    lda #<(revg_map_tile_count * 16)
    jsr upload_chr_patterns
    ; Upload authentic 16x16 Arthur pin sprite (two 8x16 tiles, 64 bytes) to $1000..$103F.
    lda #$10
    sta $2006
    lda #$00
    sta $2006
    ldx #0
@pin_spr_chr:
    lda revg_map_pin_chr,x
    sta $2007
    inx
    cpx #64
    bne @pin_spr_chr
    ; Shared BG palette ($3F00..$3F0F) and Arthur pin sprite palette 0 ($3F10..$3F13).
    lda #$3f
    sta $2006
    lda #$00
    sta $2006
    ldx #0
:
    lda revg_map_palette,x
    sta $2007
    inx
    cpx #16
    bne :-
    ldx #0
:
    lda revg_map_pin_palette,x
    sta $2007
    inx
    cpx #4
    bne :-
    ; Clear right nametable ($2400..$27FF) so upper half and right padding are clean black.
    lda #$24
    sta $2006
    lda #$00
    sta $2006
    ldx #4
    ldy #0
    lda #0
@clr_rnt:
    sta $2007
    iny
    bne @clr_rnt
    dex
    bne @clr_rnt
    ; Left half nametable rows 16..27 ($2200..$237F) + attributes ($23E0..$23F7)
    lda #<revg_map_left_names
    sta map_ptr
    lda #>revg_map_left_names
    sta map_ptr+1
    lda #$22
    jsr fe_map_upload_nt
    lda #$23
    sta $2006
    lda #$e0
    sta $2006
    ldx #0
:
    lda revg_map_left_attrs,x
    sta $2007
    inx
    cpx #24
    bne :-
    ; Right half nametable rows 16..27 ($2600..$277F) + attributes ($27E0..$27F7)
    lda #<revg_map_right_names
    sta map_ptr
    lda #>revg_map_right_names
    sta map_ptr+1
    lda #$26
    jsr fe_map_upload_nt
    lda #$27
    sta $2006
    lda #$e0
    sta $2006
    ldx #0
:
    lda revg_map_right_attrs,x
    sta $2007
    inx
    cpx #24
    bne :-
@map_bank_done:
    pla
    jsr select_bank0

    ; Draw stationary arcade top HUD + center banner on $2000 (rows 0..15 stay fixed via IRQ).
    lda #<fe_map_hud_lines
    ldx #>fe_map_hud_lines
    jsr fe_draw_lines
    ; Live 6-digit PLAYER 1 score at $2043..$2048
    lda #$20
    sta $2006
    lda #$43
    sta $2006
    ldx #0
@map_p1_sc:
    lda ap_score,x
    clc
    adc #campaign_zero
    sta $2007
    inx
    cpx #4
    bne @map_p1_sc
    lda #campaign_zero
    sta $2007
    sta $2007
    ; Live 6-digit TOP SCORE at $204e..$2053
    lda #$20
    sta $2006
    lda #$4e
    sta $2006
    ldx #0
@map_top_sc:
    lda ap_top_score,x
    clc
    adc #campaign_zero
    sta $2007
    inx
    cpx #4
    bne @map_top_sc
    lda #campaign_zero
    sta $2007
    sta $2007

    lda fe_map_mode
    bne @setup_map_pin
    ; On Stage Clear, also show "STAGE X CLEAR" above "PLAYER ONE READY".
    lda #<fe_map_clear_lines
    ldx #>fe_map_clear_lines
    jsr fe_draw_lines
    lda #$21
    sta $2006
    lda #$2f
    sta $2006
    lda campaign_stage
    bne :+
    lda #1
:
    clc
    adc #campaign_zero
    sta $2007

@setup_map_pin:
    ; Select exact arcade Arthur marker pin coordinates ($6485 start / $6493 checkpoint).
    ldx map_tmp
    lda fe_map_pin_y_tbl,x
    sta fe_map_pin_y
    lda fe_map_mode
    beq @pin_use_start
    lda checkpoint_reached
    beq @pin_use_start
    lda fe_map_pin_cp_x,x
    jmp @pin_have_x
@pin_use_start:
    lda fe_map_pin_start_x,x
@pin_have_x:
    sta fe_map_pin_x
    lda #0
    cpx #4
    bcc :+
    lda #1                      ; Stages 5..7: spawn pin at Castle once scroll reaches 143
:
    sta fe_map_pin_late

    ; Clear OAM buffer to $FE (offscreen), configure Arthur pin tiles/attr,
    ; and place initial Arthur pin for Stages 1..4 (Stages 5..7 appear after scroll=143).
    ldx #0
    lda #$fe
@clr_map_oam:
    sta sprites,x
    inx
    bne @clr_map_oam
    lda #$01                    ; Tile $01 -> $1000..$101F (left 8x16 half of Arthur pin)
    sta sprites+1
    lda #$03                    ; Tile $03 -> $1020..$103F (right 8x16 half of Arthur pin)
    sta sprites+5
    lda #$00                    ; Sprite palette 0
    sta sprites+2
    sta sprites+6
    lda fe_map_pin_late
    bne @arm_map_split
    lda fe_map_pin_y
    sta sprites+0
    sta sprites+4
    lda fe_map_pin_x
    sta sprites+3
    clc
    adc #8
    sta sprites+7

@arm_map_split:
    ; Arm VRC7a scanline IRQ at scanline 127 (21 vblank + 127 visible = 148 lines)
    ; so rows 0..15 stay fixed at scroll X=0 while rows 16..27 scroll 0..143.
    lda #$02                    ; Y=128 (coarse_y=16, fine_y=0, nt=0) -> $2006 hi = $02
    sta split_address_high
    lda #$80                    ; split_y = 128
    sta split_y
    lda #0
    sta split_x
    sta split_address_low
    lda #256 - 148
    sta $e010
    lda #1
    sta fe_map_active
    rts

; Upload 384 nametable bytes from (map_ptr) to (A:$00)..(A+1:$7F) (rows 16..27).
fe_map_upload_nt:
    sta $2006
    lda #$00
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    bne :-
    inc map_ptr+1
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #128
    bne :-
    rts

; Per-frame update for the Makai Map screen (called from FIXED clear_screen_idle with BANK2 mapped).
; Authentic Revision G timeline ($6424/$6430/$6449):
;   frames   0.. 63: hold at scroll X = 0 (left side Stages 1..4)
;   frames  64..206: scroll X = 0 -> 143 at 1 px/frame
;   frames 207..285: hold at scroll X = 143 (right side Castle Stages 5..7)
;   frame       286: returns C=1 to auto-advance into stage; C=0 otherwise.
fe_map_tick:
    inc fe_idle_lo
    bne :+
    inc fe_idle_hi
:
    lda fe_idle_hi
    bne @do_pan
    lda fe_idle_lo
    cmp #64
    bcc @apply_split
@do_pan:
    lda fe_map_scroll_x
    cmp #143
    beq @apply_split
    inc fe_map_scroll_x
@apply_split:
    ; Update VRC7a split scroll target (scanline 128, coarse_y=16, fine_y=0, nt=0).
    lda fe_map_scroll_x
    sta split_x
    lsr a
    lsr a
    lsr a
    sta split_address_low
    ; Update blinking 16x16 Arthur pin OAM sprite (8F on / 8F off, matching arcade $6473).
    lda #$fe
    sta sprites+0
    sta sprites+4
    lda fe_idle_lo
    and #$08
    bne @chk_end
    lda fe_map_pin_late
    bne @pin_late
    lda fe_map_pin_x
    sec
    sbc fe_map_scroll_x
    bcc @chk_end                ; Scrolled off left edge
    beq @chk_end
    jmp @pin_show
@pin_late:
    lda fe_map_scroll_x
    cmp #143
    bcc @chk_end                ; Wait until Castle reaches full view
    lda fe_map_pin_x
@pin_show:
    sta sprites+3
    clc
    adc #8
    sta sprites+7
    lda fe_map_pin_y
    sta sprites+0
    sta sprites+4
@chk_end:
    lda fe_idle_hi
    cmp #>286
    bcc @stay
    bne @done
    lda fe_idle_lo
    cmp #<286
    bcc @stay
@done:
    sec
    rts
@stay:
    clc
    rts

; Per-frame auto-advance timer for GAME OVER screen (240F -> returns C=1).
fe_gameover_tick:
    inc fe_idle_lo
    bne :+
    inc fe_idle_hi
:
    lda fe_idle_hi
    cmp #>240
    bcc @go_stay
    bne @go_done
    lda fe_idle_lo
    cmp #<240
    bcc @go_stay
@go_done:
    sec
    rts
@go_stay:
    clc
    rts

fe_map_hud_lines:
    fe_line $2022, "PLAYER 1"
    fe_line $202d, "TOP SCORE"
    fe_line $2188, "PLAYER ONE READY"
    .byte 0

fe_map_clear_lines:
    fe_line $2129, "STAGE 1 CLEAR"
    .byte 0

; Authentic Revision G Arthur marker pin coordinates ($6485 start, $6493 checkpoint).
; OAM_Y = arcade_y - 17 ($BF->174, $AF->158, $9C->139).
fe_map_pin_start_x: .byte 24, 86, 159, 209, 137, 165, 165
fe_map_pin_cp_x:    .byte 69, 110, 181, 236, 137, 165, 165
fe_map_pin_y_tbl:   .byte 174, 174, 174, 174, 174, 158, 139

; --- High Score Table, Name Entry, and Title Attract Demo (Bank 2) ------------
hs_magic_val = $a5

; Initialize default 5-entry high score table in WRAM once per cold boot.
hs_ensure_init:
    lda hs_magic
    cmp #hs_magic_val
    beq @hs_ready
    ldx #19
:
    lda hs_default_scores,x
    sta hs_scores,x
    dex
    bpl :-
    ldx #14
:
    lda hs_default_names,x
    sta hs_names,x
    dex
    bpl :-
    lda #hs_magic_val
    sta hs_magic
@hs_ready:
    rts

; Compare ap_score (4 bytes) against hs_scores (5 entries).
; Returns C=1 if qualified (shifts table down, inserts ap_score at ne_rank,
; clears ne_chars to AAA and ne_pos=0), or C=0 if below 5th place.
fe_check_highscore:
    jsr hs_ensure_init
    ldx #0                      ; rank 0..4
@cmp_rank:
    txa
    asl a
    asl a
    tay                         ; y = rank * 4
    lda ap_score+0
    cmp hs_scores+0,y
    bcc @next_rank
    bne @qualifies
    lda ap_score+1
    cmp hs_scores+1,y
    bcc @next_rank
    bne @qualifies
    lda ap_score+2
    cmp hs_scores+2,y
    bcc @next_rank
    bne @qualifies
    lda ap_score+3
    cmp hs_scores+3,y
    bcs @qualifies
@next_rank:
    inx
    cpx #5
    bne @cmp_rank
    clc
    rts
@qualifies:
    stx ne_rank
    ; Shift lower score entries down (from rank 3 down to ne_rank).
    ldx #3
@shift_loop:
    cpx ne_rank
    bcc @shift_done
    ; Score: copy 4 bytes from x*4 to (x+1)*4
    txa
    asl a
    asl a
    tay
    lda hs_scores+0,y
    sta hs_scores+4,y
    lda hs_scores+1,y
    sta hs_scores+5,y
    lda hs_scores+2,y
    sta hs_scores+6,y
    lda hs_scores+3,y
    sta hs_scores+7,y
    ; Name: copy 3 bytes from x*3 to (x+1)*3
    stx map_tmp
    txa
    asl a
    clc
    adc map_tmp
    tay
    lda hs_names+0,y
    sta hs_names+3,y
    lda hs_names+1,y
    sta hs_names+4,y
    lda hs_names+2,y
    sta hs_names+5,y
    dex
    bpl @shift_loop
@shift_done:
    ; Insert ap_score at ne_rank*4 and clear initials to AAA.
    lda ne_rank
    asl a
    asl a
    tay
    lda ap_score+0
    sta hs_scores+0,y
    lda ap_score+1
    sta hs_scores+1,y
    lda ap_score+2
    sta hs_scores+2,y
    lda ap_score+3
    sta hs_scores+3,y
    lda #0
    sta ne_pos
    sta ne_chars+0
    sta ne_chars+1
    sta ne_chars+2
    lda ne_rank
    asl a
    clc
    adc ne_rank
    tay
    lda #0
    sta hs_names+0,y
    sta hs_names+1,y
    sta hs_names+2,y
    sec
    rts

; Draw full Name Entry + Top 5 High Score Ranking screen. Display must be off.
fe_draw_name_entry:
    jsr hs_ensure_init
    jsr load_menu_patterns
    ; Upload X ($38), > ($39), Q ($3A), Z ($3B) to CHR-RAM $0380..$03BF.
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
    ldx #0
:
    lda ne_extra_glyphs,x
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
    lda #<ne_static_lines
    ldx #>ne_static_lines
    jsr fe_draw_lines
    ; Draw all 5 ranking rows (1ST..5TH: score + 3-letter name).
    ldx #0
@row_loop:
    stx map_tmp
    lda ne_row_hi,x
    sta $2006
    lda ne_row_lo,x
    sta $2006
    ; Rank digit '1'..'5'
    txa
    clc
    adc #campaign_zero + 1
    sta $2007
    lda #0
    sta $2007
    sta $2007
    ; 6-digit score (4 BCD digits + "00")
    txa
    asl a
    asl a
    tay
    lda hs_scores+0,y
    clc
    adc #campaign_zero
    sta $2007
    lda hs_scores+1,y
    clc
    adc #campaign_zero
    sta $2007
    lda hs_scores+2,y
    clc
    adc #campaign_zero
    sta $2007
    lda hs_scores+3,y
    clc
    adc #campaign_zero
    sta $2007
    lda #campaign_zero
    sta $2007
    sta $2007
    lda #0
    sta $2007
    sta $2007
    ; 3-letter name
    ldx map_tmp
    txa
    asl a
    clc
    adc map_tmp
    tay
    ldx hs_names+0,y
    lda ne_alpha_tiles,x
    sta $2007
    ldx hs_names+1,y
    lda ne_alpha_tiles,x
    sta $2007
    ldx hs_names+2,y
    lda ne_alpha_tiles,x
    sta $2007
    ldx map_tmp
    inx
    cpx #5
    bne @row_loop
    jmp fe_commit_name_entry

; VBlank / forced-blank commit: draws active 3 letters, cursor, and live rank row initials.
fe_commit_name_entry:
    ; Active 3 letters at $210d..$210f
    lda #$21
    sta $2006
    lda #$0d
    sta $2006
    ldx ne_chars+0
    lda ne_alpha_tiles,x
    sta $2007
    ldx ne_chars+1
    lda ne_alpha_tiles,x
    sta $2007
    ldx ne_chars+2
    lda ne_alpha_tiles,x
    sta $2007
    ; Cursor '>' under active letter at $212d..$212f
    lda #$21
    sta $2006
    lda #$2d
    sta $2006
    ldx #0
@cur_c:
    lda #0
    cpx ne_pos
    bne :+
    lda #opt_arrow_tile
:
    sta $2007
    inx
    cpx #3
    bne @cur_c
    ; Mirror live initials into the player's rank row in the High Score table.
    ldx ne_rank
    lda ne_row_hi,x
    sta $2006
    lda ne_row_lo,x
    clc
    adc #11
    sta $2006
    ldx ne_chars+0
    lda ne_alpha_tiles,x
    sta $2007
    ldx ne_chars+1
    lda ne_alpha_tiles,x
    sta $2007
    ldx ne_chars+2
    lda ne_alpha_tiles,x
    sta $2007
    rts

; Process one frame of Name Entry input.
; Returns A=0 (idle), A=1 (redraw), A=2 (confirmed & saved).
fe_update_name_entry:
    lda pressed
    and #$10                    ; START: confirm immediately
    bne @confirm
    lda pressed
    and #$08                    ; Up: next letter (A->B->...->Z->A)
    beq @chk_down
    ldx ne_pos
    lda ne_chars,x
    clc
    adc #1
    cmp #26
    bcc :+
    lda #0
:
    sta ne_chars,x
    lda #1
    rts
@chk_down:
    lda pressed
    and #$04                    ; Down: previous letter (A->Z->...->B->A)
    beq @chk_left
    ldx ne_pos
    lda ne_chars,x
    sec
    sbc #1
    bpl :+
    lda #25
:
    sta ne_chars,x
    lda #1
    rts
@chk_left:
    lda pressed
    and #$42                    ; Left or B: previous character slot
    beq @chk_right
    lda ne_pos
    beq @idle
    dec ne_pos
    lda #1
    rts
@chk_right:
    lda pressed
    and #$81                    ; Right or A: next slot (A on slot 2 confirms)
    beq @idle
    lda ne_pos
    cmp #2
    bcc @adv_pos
    lda pressed
    and #$80                    ; Only A confirms on slot 2
    bne @confirm
@idle:
    lda #0
    rts
@adv_pos:
    inc ne_pos
    lda #1
    rts
@confirm:
    ; Save ne_chars into hs_names[ne_rank*3 .. +2]
    lda ne_rank
    asl a
    clc
    adc ne_rank
    tay
    lda ne_chars+0
    sta hs_names+0,y
    lda ne_chars+1
    sta hs_names+1,y
    lda ne_chars+2
    sta hs_names+2,y
    ; Sync ap_top_score if 1st place
    lda ne_rank
    bne @done_confirm
    ldx #3
:
    lda hs_scores,x
    sta ap_top_score,x
    dex
    bpl :-
@done_confirm:
    lda #2
    rts

; Scripted input generator for Title Attract Demo (Stage 1 graveyard, 360 frames).
; Decrements attract_timer; returns C=1 when expired, else C=0 with scripted pad/pressed.
fe_attract_step:
    lda attract_timer_lo
    ora attract_timer_hi
    beq @expired
    lda attract_timer_lo
    bne :+
    dec attract_timer_hi
:
    dec attract_timer_lo
    ; Script: walk right continuously, throw lance every 32 frames (mod 32 == 4), jump every 64 frames (mod 64 == 18).
    lda #$01                    ; Right
    ldx attract_timer_lo
    txa
    and #$1f
    cmp #4
    bne @no_b
    lda #$41                    ; Right + B (shoot)
    bne @apply_pad
@no_b:
    txa
    and #$3f
    cmp #18
    bne @only_r
    lda #$81                    ; Right + A (jump)
    bne @apply_pad
@only_r:
    lda #$01
@apply_pad:
    sta pad
    lda attract_prev_pad
    eor #$ff
    and pad
    sta pressed
    lda pad
    sta attract_prev_pad
    clc
    rts
@expired:
    sec
    rts

; 26-letter alphabet tile lookup (A..Z -> tile ID in menu font + $38=X, $3A=Q, $3B=Z).
ne_alpha_tiles:
    fe_glyph 'A'
    fe_glyph 'B'
    fe_glyph 'C'
    fe_glyph 'D'
    fe_glyph 'E'
    fe_glyph 'F'
    fe_glyph 'G'
    fe_glyph 'H'
    fe_glyph 'I'
    fe_glyph 'J'
    fe_glyph 'K'
    fe_glyph 'L'
    fe_glyph 'M'
    fe_glyph 'N'
    fe_glyph 'O'
    fe_glyph 'P'
    .byte $3a                   ; Q
    fe_glyph 'R'
    fe_glyph 'S'
    fe_glyph 'T'
    fe_glyph 'U'
    fe_glyph 'V'
    fe_glyph 'W'
    .byte $38                   ; X
    fe_glyph 'Y'
    .byte $3b                   ; Z

ne_extra_glyphs:
    .byte $00,$0e,$11,$11,$11,$15,$12,$0d, $00,$00,$00,$00,$00,$00,$00,$00 ; Q ($3A)
    .byte $00,$1f,$01,$02,$04,$08,$10,$1f, $00,$00,$00,$00,$00,$00,$00,$00 ; Z ($3B)

hs_default_scores:
    .byte 0, 1, 0, 0            ; 010000
    .byte 0, 0, 5, 0            ; 005000
    .byte 0, 0, 3, 0            ; 003000
    .byte 0, 0, 2, 0            ; 002000
    .byte 0, 0, 1, 0            ; 001000
hs_default_names:
    .byte 2, 0, 15              ; CAP
    .byte 2, 14, 12             ; COM
    .byte 0, 17, 19             ; ART
    .byte 7, 20, 17             ; HUR
    .byte 6, 13, 6              ; GNG

ne_row_hi: .byte $21, $22, $22, $22, $22
ne_row_lo: .byte $c9, $09, $49, $89, $c9

ne_static_lines:
    fe_line $208b, "NAME ENTRY"
    fe_line $20c7, "ENTER YOUR INITIALS"
    fe_line $218a, "HIGH SCORES"
    fe_line $2324, "UP DN  A NEXT  START OK"
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
