; A lantern gate stands in front of the player. The carving is part
; of the background. Both are visual details on the lower terrace.
lantern_column = 20
lantern_x = lantern_column * 16
carving_column = 22
scenery_first_row = 21
carving_row = 23

.segment "CODE"
; A = world tile row and Y = metatile column. Carry set returns a scenery
; tile, otherwise the caller should use the terrain. Preserve X.
scenery_tile:
    cpy #lantern_column
    bne test_carving
    sec
    sbc #scenery_first_row
    cmp #3
    bcs no_scenery
    clc
    adc #(scenery_patterns-glyphs)/16
    sec
    rts
test_carving:
    cpy #carving_column
    bne no_scenery
    cmp #carving_row
    bne no_scenery
    lda #(scenery_patterns-glyphs)/16+3
    sec
    rts
no_scenery:
    clc
    rts
