; Enemies, enemy bolts and the boss in Bank 4, mapped at $C000 (docs/vrc7a-architecture.md).
; Bank 3 stays mapped at $A000 while this runs, so it calls the player routines directly.
; Bank 3 maps this bank with ap_map_enemy_bank before calling in.
.segment "BANK4"
    .byte bank_tag_base + 4

; Two at every rank: the arcade allows three only in the first difficulty row,
; and a third zombie on the ground line is what overflows sixteen FC sprites.
ap_rank_zombie_cap:  .byte 2, 2, 2, 2
ap_rank_spawn_wait:  .byte 48, 36, 28, 20

; --- Difficulty rank progression ---
ap_update_rank:
    dec ap_rank_timer
    bne @rank_done
    lda #ap_rank_step_frames
    sta ap_rank_timer
    lda ap_rank
    cmp #3
    bcs @rank_done
    inc ap_rank
@rank_done:
    rts

; Helper: test if shot slot X hits 16x16 enemy centered at (ap_probe_x, ap_probe_y_hi:ap_probe_y).
; Carry set on hit. Preserves Y.
ap_shot_hits_probe:
    sec
    lda ps_x_lo,x
    sbc ap_probe_x
    sta ap_tmp0
    lda ps_x_hi,x
    sbc ap_probe_x+1
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #8
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @shp_miss
    lda ap_tmp0
    cmp #17
    bcs @shp_miss
    ; Check probe_y - 16 <= ps_y < probe_y => 0 < probe_y - ps_y <= 16
    sec
    lda ap_probe_y
    sbc ps_y,x
    sta ap_tmp0
    lda ap_probe_y_hi
    sbc ps_y_hi,x
    bne @shp_miss
    lda ap_tmp0
    beq @shp_miss
    cmp #player_height + 1
    bcs @shp_miss
    sec
    rts
@shp_miss:
    clc
    rts

; Carry set if shot slot ap_loop_idx hit an enemy or boss. (The game build's is in
; revg-enemies.inc.)
.ifdef REVG_STAGE1
ap_shot_check_enemies_proto:
.else
ap_shot_check_enemies:
.endif
    ldx ap_loop_idx
    lda ap_enemy_active
    beq @check_pot_enemy
    lda sd_enemy1_x
    sta ap_probe_x
    lda sd_enemy1_x+1
    sta ap_probe_x+1
    lda sd_enemy1_y
    sta ap_probe_y
    lda sd_enemy1_y+1
    sta ap_probe_y_hi
    jsr ap_shot_hits_probe
    bcc @check_pot_enemy
    lda #0
    sta ap_enemy_active
    lda #ap_score_enemy
    jsr ap_add_score
    sec
    rts

@check_pot_enemy:
    lda ap_pot_enemy_active
    beq @no_enemy_hit
    lda sd_pot_x
    sta ap_probe_x
    lda sd_pot_x+1
    sta ap_probe_x+1
    lda sd_pot_y
    sta ap_probe_y
    lda sd_pot_y+1
    sta ap_probe_y_hi
    jsr ap_shot_hits_probe
    bcc @no_enemy_hit
    lda #0
    sta ap_pot_enemy_active
    lda sd_pot_drop
    sta item_type+2
    lda sd_pot_x
    sta item_x_lo+2
    lda sd_pot_x+1
    sta item_x_hi+2
    lda sd_pot_y
    sta item_y+2
    lda sd_pot_y+1
    sta item_y_hi+2
    lda #ap_score_enemy
    jsr ap_add_score
    sec
    rts

@no_enemy_hit:
    ldy #0
@pool_shot_loop:
    lda en_type,y
    beq @next_pool_shot
    lda en_x_lo,y
    sta ap_probe_x
    lda en_x_hi,y
    sta ap_probe_x+1
    lda en_y,y
    sta ap_probe_y
    lda en_y_hi,y
    sta ap_probe_y_hi
    ldx ap_loop_idx
    jsr ap_shot_hits_probe
    bcc @next_pool_shot
    lda en_hp,y
    sec
    sbc #1
    sta en_hp,y
    bne @pool_hit_survived
    lda en_type,y
    cmp #en_zombie
    bne @turret_killed
    dec sched_zombies_live
    lda #ap_score_zombie
    jmp @pool_kill_score
@turret_killed:
    lda #ap_score_enemy
@pool_kill_score:
    pha
    lda #en_none
    sta en_type,y
    pla
    jsr ap_add_score
@pool_hit_survived:
    sec
    rts
@next_pool_shot:
    iny
    cpy #ap_max_enemies
    bne @pool_shot_loop
    jmp ap_shot_check_boss

; --- Enemy contact (fixed enemies, enemy pool, and boss) ---
ap_check_enemies_contact:
    lda ap_invuln
    beq :+
    rts
:
    lda ap_enemy_active
    beq @check_pot_contact
    lda sd_enemy1_x
    sta ap_probe_x
    lda sd_enemy1_x+1
    sta ap_probe_x+1
    lda sd_enemy1_y
    sta ap_probe_y
    lda sd_enemy1_y+1
    sta ap_probe_y_hi
    jsr ap_test_contact_at_probe
    bcc :+
    jmp ap_apply_player_damage
:
@check_pot_contact:
    lda ap_pot_enemy_active
    beq @check_pool_contact
    lda sd_pot_x
    sta ap_probe_x
    lda sd_pot_x+1
    sta ap_probe_x+1
    lda sd_pot_y
    sta ap_probe_y
    lda sd_pot_y+1
    sta ap_probe_y_hi
    jsr ap_test_contact_at_probe
    bcc :+
    jmp ap_apply_player_damage
:
@check_pool_contact:
    ldy #0
@pool_contact_loop:
    lda en_type,y
    beq @next_pool_contact
    sty ap_loop_idx
    lda en_x_lo,y
    sta ap_probe_x
    lda en_x_hi,y
    sta ap_probe_x+1
    lda en_y,y
    sta ap_probe_y
    lda en_y_hi,y
    sta ap_probe_y_hi
    jsr ap_test_contact_at_probe
    ldy ap_loop_idx
    bcc @next_pool_contact
    jmp ap_apply_player_damage  ; In Bank 3, out of branch range.
@next_pool_contact:
    iny
    cpy #ap_max_enemies
    bne @pool_contact_loop

    ; Check contact with active boss (16x32 hitbox)
    lda ap_boss_state
    cmp #boss_active
    beq :+
    rts
:
    sec
    lda player_x
    sbc ap_boss_x_lo
    sta ap_tmp0
    lda player_x+1
    sbc ap_boss_x_hi
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #14
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @contact_none
    lda ap_tmp0
    cmp #29
    bcs @contact_none
    ; 16-bit Y: boss_y - 31 <= player_y < boss_y + 16 => (player_y - boss_y + 31) < 47
    sec
    lda player_feet
    sbc ap_boss_y
    sta ap_tmp0
    lda ap_player_y_hi
    sbc ap_boss_y_hi
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #31
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @contact_none
    lda ap_tmp0
    cmp #47
    bcs @contact_none
    jmp ap_apply_player_damage
@contact_none:
    rts

; --- Data-driven scroll-linked enemy scheduler & fixed turret spawner ---
ap_update_scheduler:
    lda ap_boss_state
    beq @sched_run
    rts
@sched_run:
    ; Both formats share the record layout. Their index ranges do not overlap.
    ldy #sd_sched_page - sd_base
    lda sd_flags
    and #sd_flag_sched_table
    beq @legacy_record
    jsr ap_current_sched_page
    tax
    cpx #sd_sched_pages
    bcs @no_record
    ldy ap_sched_record_offsets,x
    lda (ap_sched_ptr),y
    cmp #$ff
    beq @no_record
    txa
    jmp @select_record
@no_record:
    stx sched_page
    lda #0
    sta sched_group
    rts
@legacy_record:
    lda sd_sched_page
    cmp #$ff
    bne :+
    rts
:
    jsr ap_current_sched_page
@select_record:
    cmp sched_page
    beq @check_group_spawns
    sta sched_page
    cmp (ap_sched_ptr),y
    beq @enter_sched_page
    lda #0
    sta sched_group
    beq @check_group_spawns
@enter_sched_page:
    lda #1
    sta sched_group
    lda sched_wait
    bne @check_group_spawns
    ldx ap_rank
    lda ap_rank_spawn_wait,x
    sta sched_wait

@check_group_spawns:
    lda sched_page
    cmp (ap_sched_ptr),y
    bne @check_zombie_sched
    sty ap_tmp0                    ; Record start; enemy allocation preserves Y.
    iny
    lda (ap_sched_ptr),y
    iny
    ora (ap_sched_ptr),y
    bne @check_turret_owner
    ldy ap_tmp0
    jmp @check_zombie_sched
@check_turret_owner:
    ldy ap_tmp0
    cpy #sd_sched_page - sd_base
    beq @check_legacy_turret
    ldx sched_page
    lda sched_turrets,x
    jmp @check_turret_flag
@check_legacy_turret:
    lda ap_turret_spawned
@check_turret_flag:
    bne @check_zombie_sched
    jsr ap_alloc_enemy_slot
    bcc @check_zombie_sched
    lda #en_turret
    sta en_type,x
    iny
    lda (ap_sched_ptr),y
    sta en_x_lo,x
    iny
    lda (ap_sched_ptr),y
    sta en_x_hi,x
    iny
    lda (ap_sched_ptr),y
    sta en_y,x
    iny
    lda (ap_sched_ptr),y
    sta en_y_hi,x
    ldy ap_tmp0
    lda #ap_turret_cooldown
    sta en_timer,x
    lda #2
    sta en_hp,x
    cpy #sd_sched_page - sd_base
    beq @mark_legacy_turret
    lda sched_page
    sta en_sched_page,x
    tax
    lda #1
    sta sched_turrets,x
    jmp @check_zombie_sched
@mark_legacy_turret:
    lda #$ff
    sta en_sched_page,x
    lda #1
    sta ap_turret_spawned

@check_zombie_sched:
    lda sched_group
    beq @sched_done
    ldx ap_rank
    lda sched_zombies_live
    cmp ap_rank_zombie_cap,x
    bcs @sched_done
    lda sched_wait
    beq @spawn_zombie_now
    dec sched_wait
    bne @sched_done
@spawn_zombie_now:
    jsr ap_alloc_enemy_slot
    bcc @sched_done
    lda #en_zombie
    sta en_type,x
    tya
    clc
    adc #5
    tay
    lda (ap_sched_ptr),y
    sta en_x_lo,x
    iny
    lda (ap_sched_ptr),y
    bpl @zombie_x_ready
    and #$7f                       ; Relative to the camera, as in the arcade ($EAEC).
    sta en_x_hi,x
    lda en_x_lo,x
    clc
    adc camera_low
    sta en_x_lo,x
    lda en_x_hi,x
    adc camera_high
@zombie_x_ready:
    sta en_x_hi,x
    iny
    lda (ap_sched_ptr),y
    sta en_y,x
    iny
    lda (ap_sched_ptr),y
    sta en_y_hi,x
    lda #0
    sta en_timer,x
    lda #1
    sta en_hp,x
    inc sched_zombies_live
    ldy ap_rank
    lda ap_rank_spawn_wait,y
    sta sched_wait
@sched_done:
    rts

ap_sched_record_offsets:
    .repeat sd_sched_pages, page
        .byte page * sd_sched_stride
    .endrepeat

ap_alloc_enemy_slot:
    ldx #0
@alloc_loop:
    lda en_type,x
    beq @alloc_found
    inx
    cpx #ap_max_enemies
    bne @alloc_loop
    clc
    rts
@alloc_found:
    sec
    rts

; --- Generic enemy pool update + offscreen reclamation ---
ap_update_enemies:
    ldx #0
@en_loop:
    stx ap_loop_idx
    lda en_type,x
    bne :+
    jmp @en_next
:
    sec
    lda en_x_lo,x
    sbc camera_low
    sta ap_tmp0
    lda en_x_hi,x
    sbc camera_high
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #32
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bmi @reclaim_enemy
    cmp #2
    bcs @reclaim_enemy
    cmp #1
    bne @check_en_y_bounds
    lda ap_tmp0
    cmp #64
    bcs @reclaim_enemy

@check_en_y_bounds:
    ; Also reclaim enemies scrolled offscreen vertically (-48 .. +287 relative to ap_cam_y).
    sec
    lda en_y,x
    sbc ap_cam_y_lo
    sta ap_tmp0
    lda en_y_hi,x
    sbc ap_cam_y_hi
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #48
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bmi @reclaim_enemy
    cmp #2
    bcs @reclaim_enemy
    cmp #1
    bne @en_onscreen
    lda ap_tmp0
    cmp #80
    bcs @reclaim_enemy

@en_onscreen:
    lda en_type,x
    cmp #en_zombie
    beq @update_zombie
    cmp #en_turret
    beq @update_turret
    jmp @en_next

@reclaim_enemy:
    lda en_type,x
    cmp #en_zombie
    bne @check_reclaim_turret
    dec sched_zombies_live
    jmp @clear_en_slot
@check_reclaim_turret:
    cmp #en_turret
    bne @clear_en_slot
    ldy en_sched_page,x
    bmi @reclaim_legacy_turret
    lda #0
    sta sched_turrets,y
    beq @clear_en_slot
@reclaim_legacy_turret:
    lda #0
    sta ap_turret_spawned
@clear_en_slot:
    lda #en_none
    sta en_type,x
    jmp @en_next

@update_zombie:
    inc en_timer,x
    lda ap_rank
    cmp #2
    bcs @zombie_step_left
    lda en_timer,x
    and #1
    bne @en_next
@zombie_step_left:
    lda en_x_lo,x
    bne :+
    dec en_x_hi,x
:
    dec en_x_lo,x
    jmp @en_next

@update_turret:
    dec en_timer,x
    bne @en_next
    lda #ap_turret_cooldown
    sta en_timer,x
    jsr ap_try_spawn_ebolt
    ldx ap_loop_idx

@en_next:
    inx
    cpx #ap_max_enemies
    beq :+
    jmp @en_loop
:
    rts

ap_try_spawn_ebolt:
    ldy #0
@eb_find:
    lda eb_active,y
    beq @eb_alloc
    iny
    cpy #ap_max_ebolts
    bne @eb_find
    rts
@eb_alloc:
    lda #1
    sta eb_active,y
    lda en_x_lo,x
    sec
    sbc #8
    sta eb_x_lo,y
    lda en_x_hi,x
    sbc #0
    sta eb_x_hi,y
    lda en_y,x
    sec
    sbc #9
    sta eb_y,y
    lda en_y_hi,x
    sbc #0
    sta eb_y_hi,y
    lda #$fe
    sta eb_vx,y
    lda #120
    sta eb_timer,y
    rts

; --- Enemy bolt pool update, offscreen reclamation, player hit ---
ap_update_ebolts:
    ldx #0
@eb_loop:
    stx ap_loop_idx
    lda eb_active,x
    bne :+
    jmp @eb_next
:
    lda eb_vx,x
    bmi @eb_left
    clc
    adc eb_x_lo,x
    sta eb_x_lo,x
    bcc @eb_bounds
    inc eb_x_hi,x
    jmp @eb_bounds
@eb_left:
    clc
    adc eb_x_lo,x
    sta eb_x_lo,x
    bcs @eb_bounds
    dec eb_x_hi,x
@eb_bounds:
    sec
    lda eb_x_lo,x
    sbc camera_low
    lda eb_x_hi,x
    sbc camera_high
    bne @eb_reclaim
    dec eb_timer,x
    beq @eb_reclaim
    lda ap_invuln
    bne @eb_next
    sec
    lda player_x
    sbc eb_x_lo,x
    sta ap_tmp0
    lda player_x+1
    sbc eb_x_hi,x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #8
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @eb_next
    lda ap_tmp0
    cmp #17
    bcs @eb_next
    ; 16-bit Y: eb_y <= player_y < eb_y + (crouch ? 10 : 16)
    sec
    lda player_feet
    sbc eb_y,x
    sta ap_tmp0
    lda ap_player_y_hi
    sbc eb_y_hi,x
    bne @eb_next
    lda ap_tmp0
    ldy ap_crouch
    beq @eb_stand_h
    cmp #10
    bcs @eb_next
    bcc @eb_hit_player
@eb_stand_h:
    cmp #player_height
    bcs @eb_next
@eb_hit_player:
.ifdef REVG_STAGE1
    ; No enemy shot is checked during state 3 ($F48C needs bit 0 of $0500).
    lda ap_state
    cmp #ap_state_knockback
    bcs @eb_next
    lda eb_x_lo,x
    sta ap_probe_x
    lda eb_x_hi,x
    sta ap_probe_x+1
.endif
    lda #0
    sta eb_active,x
    jsr ap_apply_player_damage
    ldx ap_loop_idx
    jmp @eb_next
@eb_reclaim:
    lda #0
    sta eb_active,x
@eb_next:
    inx
    cpx #ap_max_ebolts
    beq :+
    jmp @eb_loop
:
    rts

; --- Data-driven multi-part boss encounter, arena lock, key drop ---
ap_update_boss:
    lda sd_flags
    and #sd_flag_has_boss
    bne :+
    rts
:
    lda ap_boss_state
    cmp #boss_active
    bne :+
    jmp @boss_step
:
    cmp #boss_idle
    beq :+
    rts
:
    lda sd_flags
    and #sd_flag_vertical
    bne @check_vert_boss_trig
    ; Horizontal stage: trigger when player_x >= sd_boss_trig_x
    lda player_x+1
    cmp sd_boss_trig_x+1
    bcs :+
    rts
:
    bne @start_boss
    lda player_x
    cmp sd_boss_trig_x
    bcs @start_boss
    rts
@check_vert_boss_trig:
    ; Vertical stage: trigger when grounded and player_y <= sd_boss_trig_y
    lda ap_state
    cmp #ap_state_ground
    beq :+
    rts
:
    lda ap_player_y_hi
    cmp sd_boss_trig_y+1
    bcc @start_boss
    beq :+
    rts
:
    lda player_feet
    cmp sd_boss_trig_y
    beq @start_boss
    bcc @start_boss
    rts
@start_boss:
    lda #boss_active
    sta ap_boss_state
    lda #1
    sta ap_boss_lock
    lda sd_boss_hp
    sta ap_boss_hp
    lda sd_boss_shield
    sta ap_boss_shield_hp
    lda sd_boss_start_x
    sta ap_boss_x_lo
    lda sd_boss_start_x+1
    sta ap_boss_x_hi
    lda sd_boss_y
    sta ap_boss_y
    lda sd_boss_y+1
    sta ap_boss_y_hi
    lda #0
    sta ap_boss_dir
    sta ap_boss_flash
    sta sched_group
    sta sched_zombies_live
    lda sd_boss_fire_per
    sta ap_boss_timer
    ; Clear active regular enemies, bolts, platforms, and stray items (<=8 sprites/scanline).
    ldx #ap_max_enemies - 1
    lda #0
:
    sta en_type,x
    dex
    bpl :-
    ldx #ap_max_ebolts - 1
:
    sta eb_active,x
    dex
    bpl :-
    ldx #ap_max_plats - 1
:
    sta plat_type,x
    dex
    bpl :-
    ldx #ap_max_items - 1
:
    sta item_type,x
    dex
    bpl :-
    lda sd_bgm_boss
    jmp call_bank8_play_song
@boss_done:
    rts

@boss_step:
    lda ap_boss_flash
    beq :+
    dec ap_boss_flash
:
    lda ap_boss_timer
    and #1
    bne @boss_fire_check
    lda ap_boss_dir
    bne @boss_move_right
    lda ap_boss_x_lo
    bne :+
    dec ap_boss_x_hi
:
    dec ap_boss_x_lo
    lda ap_boss_x_hi
    cmp sd_boss_min_x+1
    bne @boss_fire_check
    lda ap_boss_x_lo
    cmp sd_boss_min_x
    bne @boss_fire_check
    lda #1
    sta ap_boss_dir
    bne @boss_fire_check
@boss_move_right:
    inc ap_boss_x_lo
    bne :+
    inc ap_boss_x_hi
:
    lda ap_boss_x_hi
    cmp sd_boss_max_x+1
    bne @boss_fire_check
    lda ap_boss_x_lo
    cmp sd_boss_max_x
    bne @boss_fire_check
    lda #0
    sta ap_boss_dir

@boss_fire_check:
    dec ap_boss_timer
    bne @boss_done
    lda sd_boss_fire_per
    sta ap_boss_timer
    jmp ap_boss_fire_bolt

ap_boss_fire_bolt:
    ; Cap boss bolts to 1 active (slot 0) so total scanline sprites stay <= 8.
    ldy #0
    lda eb_active+0
    beq @bfb_alloc
    rts
@bfb_alloc:
    lda #1
    sta eb_active,y
    lda ap_boss_x_lo
    sec
    sbc #12
    sta eb_x_lo,y
    lda ap_boss_x_hi
    sbc #0
    sta eb_x_hi,y
    lda ap_boss_y
    sec
    ldx ap_boss_dir
    bne @bfb_high
    sbc #9
    jmp @bfb_set_y
@bfb_high:
    sbc #22
@bfb_set_y:
    sta eb_y,y
    lda ap_boss_y_hi
    sbc #0
    sta eb_y_hi,y
    lda #$fe
    sta eb_vx,y
    lda #120
    sta eb_timer,y
    rts

; Carry set if shot slot ap_loop_idx hits active boss.
ap_shot_check_boss:
    lda ap_boss_state
    cmp #boss_active
    beq :+
    clc
    rts
:
    ldx ap_loop_idx
    sec
    lda ps_x_lo,x
    sbc ap_boss_x_lo
    sta ap_tmp0
    lda ps_x_hi,x
    sbc ap_boss_x_hi
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #14
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @boss_shot_miss
    lda ap_tmp0
    cmp #29
    bcs @boss_shot_miss
    ; 16-bit Y: boss_y - 32 <= ps_y < boss_y => 1 <= boss_y - ps_y <= 32
    sec
    lda ap_boss_y
    sbc ps_y,x
    sta ap_tmp0
    lda ap_boss_y_hi
    sbc ps_y_hi,x
    bne @boss_shot_miss
    lda ap_tmp0
    beq @boss_shot_miss
    cmp #33
    bcs @boss_shot_miss
    ; Hit confirmed!
    lda #8
    sta ap_boss_flash
    lda ap_boss_shield_hp
    beq @boss_core_hit
    lda ap_tmp0
    cmp #17
    bcs @boss_core_hit          ; Shot hit upper head region (boss_y - ps_y >= 17)
    dec ap_boss_shield_hp
    sec
    rts
@boss_core_hit:
    dec ap_boss_hp
    bne @boss_hit_done
    lda #boss_defeated
    sta ap_boss_state
    ldy #ap_max_ebolts - 1
    lda #0
:
    sta eb_active,y
    dey
    bpl :-
    lda #item_key
    sta item_type+3
    lda ap_boss_x_lo
    sta item_x_lo+3
    lda ap_boss_x_hi
    sta item_x_hi+3
    lda ap_boss_y
    sta item_y+3
    lda ap_boss_y_hi
    sta item_y_hi+3
    lda #ap_score_boss
    jsr ap_add_score
@boss_hit_done:
    sec
    rts
@boss_shot_miss:
    clc
    rts
