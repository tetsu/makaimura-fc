; The OAM builder in Bank 7, mapped at $C000 (docs/vrc7a-architecture.md).
; call_bank7_draw_sprites maps it. It reads the stage data in $8000 and the WRAM state only.
.segment "BANK7"
    .byte bank_tag_base + 7

; --- 16-bit sprite Y helper ---
; Input: A = world_y_lo, X = world_y_hi, Y = positive pixel offset to subtract.
; Sets draw_y to screen Y, or $f8 if offscreen.
ap_set_sprite_y_off:
    sty ap_tmp3
    sec
    sbc ap_tmp3
    bcs :+
    dex
:
    sec
    sbc ap_cam_y_lo
    sta draw_y
    txa
    sbc ap_cam_y_hi
    bne @off_y
    lda draw_y
    cmp #240
    bcs @off_y
    rts
@off_y:
    lda #$f8
    sta draw_y
    rts

; --- OAM sprite rendering for arcade mode ---
; Helper: draw 16x16 pair (draw_tile, draw_tile+2) with optional horizontal flip
; when A != 0 (sets OAM bit 6 and swaps left/right tiles around world center).
ap_draw_pair_dir:
    beq @dpd_normal
    lda draw_palette
    ora #$40
    sta draw_palette
    inc draw_tile
    inc draw_tile
    lda ap_tmp0
    ldx ap_tmp1
    jsr set_sprite_x
    sec
    lda draw_x
    sbc #8
    sta draw_x
    bcs :+
    dec draw_x+1
:
    jsr emit_sprite
    clc
    lda draw_x
    adc #8
    sta draw_x
    bcc :+
    inc draw_x+1
:
    dec draw_tile
    dec draw_tile
    jmp emit_sprite
@dpd_normal:
    lda ap_tmp0
    ldx ap_tmp1
    jmp draw_pair

; A = logical group. Set draw_slot to the rotated pair, preserving Y.
ap_set_oam_group:
    clc
    adc ap_oam_phase
    cmp #ap_oam_groups
    bcc :+
    sbc #ap_oam_groups
:
    asl a
    asl a
    asl a
    adc #ap_oam_first * 4  ; ASL clears carry for all valid group indices
    sta draw_slot
    rts

; A = logical slot in the priority block. Set draw_slot to the slot it takes
; this frame, preserving X and Y.
ap_set_prio_slot:
    clc
    adc ap_prio_phase
    cmp #ap_prio_count
    bcc :+
    sbc #ap_prio_count
:
    clc
    adc #ap_prio_first
    asl a
    asl a
    sta draw_slot
    rts

; emit_sprite for the priority block: a pair that starts in its last slot
; continues at its first.
ap_prio_emit:
    lda draw_slot
    cmp #ap_prio_end * 4
    bcc :+
    sbc #ap_prio_count * 4
    sta draw_slot
:
    jmp emit_sprite

; draw_pair for the priority block (A/X = world X).
ap_prio_pair:
    jsr set_sprite_x
    sec
    lda draw_x
    sbc #8
    sta draw_x
    bcs :+
    dec draw_x+1
:
    jsr ap_prio_emit
    clc
    lda draw_x
    adc #8
    sta draw_x
    bcc :+
    inc draw_x+1
:
    inc draw_tile
    inc draw_tile
    jmp ap_prio_emit

; A is zero for idle, or 1/9 for the first/second walking pose.
ap_draw_player:
    sta ap_tmp1
    lda #0
    sta draw_slot
    ; Select palette: 0=armored silver, 1=naked flesh/red, 2=dead flash
    lda #2
    ldx ap_armor
    cpx #ap_armor_full
    bne :+
    lda #0
    beq @set_player_pal
:
    cpx #ap_armor_naked
    bne @set_player_pal
    lda #1
@set_player_pal:
    sta draw_palette

    ; Choose pose tiles (ap_tmp2 = top tile or 0 if crouch, ap_tmp3 = bottom tile).
    lda ap_armor
    cmp #ap_armor_naked
    beq @pose_naked
    ; Armored Arthur poses
    lda ap_crouch
    beq @arm_not_crouch
    lda #0
    sta ap_tmp2
    lda #$2d
    sta ap_tmp3
    jmp @emit_arthur
@arm_not_crouch:
    lda ap_state
    cmp #ap_state_ladder
    bne @arm_not_lad
    lda #$31
    sta ap_tmp2
    lda #$35
    sta ap_tmp3
    jmp @emit_arthur
@arm_not_lad:
    cmp #ap_state_air
    bne @arm_ground
    lda #$25
    sta ap_tmp2
    lda #$29
    sta ap_tmp3
    jmp @emit_arthur
@arm_ground:
    lda ap_tmp1
    beq @arm_stand
    and #$08
    bne @arm_walk2
    lda #$15
    sta ap_tmp2
    lda #$19
    sta ap_tmp3
    jmp @emit_arthur
@arm_walk2:
    lda #$1d
    sta ap_tmp2
    lda #$21
    sta ap_tmp3
    jmp @emit_arthur
@arm_stand:
    lda #$11
    sta ap_tmp2
    lda #$01
    sta ap_tmp3
    jmp @emit_arthur

@pose_naked:
    lda ap_crouch
    beq @nk_not_crouch
    lda #0
    sta ap_tmp2
    lda #$51
    sta ap_tmp3
    bne @emit_arthur
@nk_not_crouch:
    lda ap_state
    cmp #ap_state_air
    bne @nk_ground
    lda #$49
    sta ap_tmp2
    lda #$4d
    sta ap_tmp3
    bne @emit_arthur
@nk_ground:
    lda ap_tmp1
    beq @nk_stand
    and #$08
    beq @nk_stand
    lda #$41
    sta ap_tmp2
    lda #$45
    sta ap_tmp3
    bne @emit_arthur
@nk_stand:
    lda #$39
    sta ap_tmp2
    lda #$3d
    sta ap_tmp3

@emit_arthur:
    ; Draw bottom 16x16 in OAM slots 0-1 (feet - 17)
    lda ap_tmp3
    sta draw_tile
    lda player_x
    sta ap_tmp0
    lda player_x+1
    sta ap_tmp1
    ldy #player_height + 1
    lda ap_crouch
    beq :+
    ldy #player_height - 3
:
    lda player_feet
    ldx ap_player_y_hi
    jsr ap_set_sprite_y_off
    lda player_left
    jsr ap_draw_pair_dir

    ; Draw top 16x16 in OAM slots 2-3 (feet - 33) if standing/jumping/ladder
    lda ap_tmp2
    beq @player_done
    lda #2 * 4
    sta draw_slot
    lda draw_palette
    and #$3f                    ; restore unflipped palette before ap_draw_pair_dir
    sta draw_palette
    ldy #player_height + 17
    lda player_feet
    ldx ap_player_y_hi
    jsr ap_set_sprite_y_off
    lda ap_tmp2
    sta draw_tile
    lda player_left
    jsr ap_draw_pair_dir

@player_done:
    rts

ap_draw_sprites:
    lda #$f8
    ldx #0
:
    sta sprites,x
    inx
    inx
    inx
    inx
    cpx #ap_oam_end * 4
    bne :-

    ; Advance by 7, coprime with 20 groups, to spread turns across frames.
    lda ap_oam_phase
    clc
    adc #7
    cmp #ap_oam_groups
    bcc :+
    sbc #ap_oam_groups
:
    sta ap_oam_phase

    ; The priority block starts one slot later each frame.
    ldx ap_prio_phase
    inx
    cpx #ap_prio_count
    bcc :+
    ldx #0
:
    stx ap_prio_phase

    ; Slots 0-1 (bottom) and 2-3 (top): keep both halves ahead of other actors.
    lda #0
    sta draw_slot
    lda ap_state
    cmp #ap_state_dead
    beq @draw_player_now
    cmp #ap_state_knockback
    beq @draw_player_now
    lda ap_invuln
    and #4
    beq @draw_player_now
    jmp @draw_enemies
@draw_player_now:
    lda pad
    and #$03
    beq :+
    lda player_x
    and #$08
    ora #1
:
    jsr ap_draw_player
@draw_enemies:
    ; Fixed enemy 1 (Zombie)
    lda ap_enemy_active
    beq @draw_pot_enemy
    lda #ap_oam_fixed_enemy
    jsr ap_set_oam_group
    lda #2                      ; Zombie blue/red/white palette
    sta draw_palette
    ldy #player_height + 1
    lda sd_enemy1_y
    ldx sd_enemy1_y+1
    jsr ap_set_sprite_y_off
    lda #$05
    sta draw_tile
    lda sd_enemy1_x
    ldx sd_enemy1_x+1
    jsr draw_pair
    lda #ap_oam_fixed_enemy + 1
    jsr ap_set_oam_group
    ldy #player_height + 17
    lda sd_enemy1_y
    ldx sd_enemy1_y+1
    jsr ap_set_sprite_y_off
    lda #$55
    sta draw_tile
    lda sd_enemy1_x
    ldx sd_enemy1_x+1
    jsr draw_pair
@draw_pot_enemy:
    ; Pot-carrying Zombie
    lda ap_pot_enemy_active
    beq @draw_shots
    lda #ap_oam_pot_enemy
    jsr ap_set_oam_group
    lda #3
    sta draw_palette
    ldy #player_height + 1
    lda sd_pot_y
    ldx sd_pot_y+1
    jsr ap_set_sprite_y_off
    lda #$59
    sta draw_tile
    lda sd_pot_x
    ldx sd_pot_x+1
    jsr draw_pair
    lda #ap_oam_pot_enemy + 1
    jsr ap_set_oam_group
    ldy #player_height + 17
    lda sd_pot_y
    ldx sd_pot_y+1
    jsr ap_set_sprite_y_off
    lda #$5d
    sta draw_tile
    lda sd_pot_x
    ldx sd_pot_x+1
    jsr draw_pair

@draw_shots:
    ; Player shots: logical slots 0-2 of the priority block.
    ldy #0
@draw_shot_loop:
    lda ps_active,y
    beq @next_shot_draw
    tya
    clc
    adc #ap_prio_shots
    jsr ap_set_prio_slot
    sty ap_loop_idx
    ldx ps_active,y
    cpx #2
    bne @shot_not_flame
    lda #1                      ; Flame: red/orange/yellow
    sta draw_palette
    lda #$65                    ; Arcade ground flame sprite
    bne @shot_tile_ready
@shot_not_flame:
    ldx ps_kind,y
    lda ap_shot_pal_tbl,x
    ldx ps_vx,y
    bpl :+
    ora #$40                    ; Flip projectile horizontally when flying left
:
    sta draw_palette
    ldx ps_kind,y
    lda ap_shot_tile_tbl,x
@shot_tile_ready:
    sta draw_tile
    lda ps_y,y
    ldx ps_y_hi,y
    ldy #2
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda ps_x_lo,y
    ldx ps_x_hi,y
    jsr set_sprite_x
    jsr emit_sprite
    ldy ap_loop_idx
@next_shot_draw:
    iny
    cpy #ap_max_shots
    bne @draw_shot_loop

    ; Enemy bolts (arcade glowing fireball): logical slots 3-4.
    ldy #0
@draw_ebolt_loop:
    lda eb_active,y
    beq @next_ebolt_draw
    tya
    clc
    adc #ap_prio_ebolts
    jsr ap_set_prio_slot
    sty ap_loop_idx
    lda #1
    sta draw_palette
    lda #$0f
    sta draw_tile
    lda eb_y,y
    ldx eb_y_hi,y
    ldy #3
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda eb_x_lo,y
    ldx eb_x_hi,y
    jsr set_sprite_x
    jsr emit_sprite
    ldy ap_loop_idx
@next_ebolt_draw:
    iny
    cpy #ap_max_ebolts
    bne @draw_ebolt_loop

    ; Generic enemy pool: zombies use two pairs, turrets use the lower pair.
    ldy #0
@draw_pool_en_loop:
    lda en_type,y
    bne :+
    jmp @next_pool_en_draw
:
    sty ap_loop_idx
    tya
    asl a
    adc #ap_oam_enemies
    jsr ap_set_oam_group
    lda en_type,y
    cmp #en_turret
    beq @draw_pool_turret
    ; Zombie: palette 2 (blue/red/white), 2-frame walk animation, 16x32
    lda #2
    sta draw_palette
    lda frames
    and #$10
    bne @zb_f2
    lda #$05
    sta ap_tmp3
    lda #$55
    sta ap_tmp2
    bne @zb_draw
@zb_f2:
    lda #$59
    sta ap_tmp3
    lda #$5d
    sta ap_tmp2
@zb_draw:
    lda ap_tmp3
    sta draw_tile
    lda en_y,y
    ldx en_y_hi,y
    ldy #player_height + 1
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda en_x_lo,y
    ldx en_x_hi,y
    jsr draw_pair
    ; Top half follows the lower pair in the rotation.
    tya
    asl a
    adc #ap_oam_enemies + 1
    jsr ap_set_oam_group
    lda en_y,y
    ldx en_y_hi,y
    ldy #player_height + 17
    jsr ap_set_sprite_y_off
    lda ap_tmp2
    sta draw_tile
    ldy ap_loop_idx
    lda en_x_lo,y
    ldx en_x_hi,y
    jsr draw_pair
    ldy ap_loop_idx
    jmp @next_pool_en_draw
@draw_pool_turret:
    lda #3
    ldx en_timer,y
    cpx #ap_turret_windup
    bcs :+
    lda #1                      ; Flash red/orange during windup
:
    sta draw_palette
    lda #$6d                    ; Arcade Green Monster plant sprite
    sta draw_tile
    lda en_y,y
    ldx en_y_hi,y
    ldy #player_height + 1
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda en_x_lo,y
    ldx en_x_hi,y
    jsr draw_pair
    ldy ap_loop_idx
@next_pool_en_draw:
    iny
    cpy #ap_max_enemies
    beq :+
    jmp @draw_pool_en_loop
:
    ; Boss: logical slots 5-8, ahead of the rotating groups.
    lda ap_boss_state
    cmp #boss_active
    bne @draw_plats
    jsr ap_draw_boss

@draw_plats:
    ; Dynamic platforms (Tombstone $69/$6b, Moving platforms $0b/$0d)
    ldy #0
@draw_plat_loop:
    lda plat_type,y
    beq @next_plat_draw
    sty ap_loop_idx
    tya
    clc
    adc #ap_oam_platforms
    jsr ap_set_oam_group
    lda plat_type,y
    cmp #plat_tombstone
    bne @plat_is_moving
    lda #0                      ; Tombstone: stone gray/white
    sta draw_palette
    lda #$69                    ; Authentic arcade tombstone
    bne @plat_tile_ready
@plat_is_moving:
    lda #3                      ; Moving platform: wood/stone
    sta draw_palette
    lda #$0b
@plat_tile_ready:
    sta draw_tile
    lda plat_y,y
    ldx plat_y_hi,y
    ldy #1
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda plat_x_lo,y
    ldx plat_x_hi,y
    jsr draw_pair
    ldy ap_loop_idx
@next_plat_draw:
    iny
    cpy #ap_max_plats
    beq :+
    jmp @draw_plat_loop
:

    ; Items use the first sprite of each pair.
    ldy #0
@draw_item_loop:
    lda item_type,y
    beq @next_item_draw
    sty ap_loop_idx
    tya
    clc
    adc #ap_oam_items
    jsr ap_set_oam_group
    ldx item_type,y
    lda ap_item_pal_tbl,x
    sta draw_palette
    lda ap_item_tile_tbl,x
    sta draw_tile
    lda item_y,y
    ldx item_y_hi,y
    ldy #15
    jsr ap_set_sprite_y_off
    ldy ap_loop_idx
    lda item_x_lo,y
    ldx item_x_hi,y
    jsr set_sprite_x
    jsr emit_sprite
    ldy ap_loop_idx
@next_item_draw:
    iny
    cpy #ap_max_items
    bne @draw_item_loop
    rts

; Draw 16x32 Unicorn Cyclops boss in OAM slots 11-14 (4 sprites):
; Head/torso ($77/$79) + Legs/armor ($7b/$7d); max scanline = 8 sprites.
ap_draw_boss:
    lda #ap_prio_boss
    jsr ap_set_prio_slot
    lda #3
    ldx ap_boss_flash
    beq :+
    lda #1
:
    sta draw_palette
    lda #boss_head_tile
    sta draw_tile
    ldy #33
    lda ap_boss_y
    ldx ap_boss_y_hi
    jsr ap_set_sprite_y_off
    lda ap_boss_x_lo
    ldx ap_boss_x_hi
    jsr ap_prio_pair

    lda #ap_prio_boss + 2
    jsr ap_set_prio_slot
    lda #3
    ldx ap_boss_shield_hp
    beq :+
    lda #0
:
    ldx ap_boss_flash
    beq :+
    lda #1
:
    sta draw_palette
    lda #$7b
    sta draw_tile
    ldy #17
    lda ap_boss_y
    ldx ap_boss_y_hi
    jsr ap_set_sprite_y_off
    lda ap_boss_x_lo
    ldx ap_boss_x_hi
    jmp ap_prio_pair

; Weapon projectile sprite/palette lookup tables (0=Lance, 1=Dagger, 2=Torch, 3=Shield)
ap_shot_tile_tbl:
    .byte $09, $61, $63, $67
ap_shot_pal_tbl:
    .byte 0, 0, 1, 1

; Item sprite/palette lookup tables (0=none, 1=armor, 2=dagger, 3=torch, 4=lance, 5=key, 6=shield)
ap_item_tile_tbl:
    .byte $75, $73, $61, $63, $09, $71, $67
ap_item_pal_tbl:
    .byte 1, 0, 0, 1, 0, 1, 1
