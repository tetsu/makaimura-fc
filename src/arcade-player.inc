; Bank 3 ($A000-$BFFF): Arcade-accurate player physics, 2D tile collision,
; two-stage armor, 16-bit SoA entity pools (weapons, dynamic platforms,
; item drops). Uses WRAM ($6010+) so fixed bank and internal RAM stay compact.

ap_state_ground = 0
ap_state_air = 1
ap_state_ladder = 2
ap_state_knockback = 3
ap_state_dead = 4

ap_armor_dead = 0
ap_armor_naked = 1
ap_armor_full = 2

ap_weapon_lance = 0
ap_weapon_dagger = 1
ap_weapon_torch = 2

ap_max_shots = 3
ap_max_plats = 4
ap_max_items = 4

plat_none = 0
plat_tombstone = 1
plat_moving_h = 2
plat_moving_v = 3

item_none = 0
item_armor = 1
item_dagger = 2
item_torch = 3
item_lance = 4

ap_invuln_frames = 120
ap_shoot_lock_frames = 15
ap_kb_length = 16
ap_torch_flame_frames = 30

.segment "WRAM"
ap_wram_reserved: .res 16       ; $6000-$600F: clear of boot WRAM probe
ap_state: .res 1                ; $6010
ap_armor: .res 1                ; $6011
ap_invuln: .res 1               ; $6012
ap_crouch: .res 1               ; $6013
ap_shoot_lock: .res 1           ; $6014
ap_air_dir: .res 1              ; $6015 (0 = vertical, 1 = right, 2 = left)
ap_kb_dir: .res 1               ; $6016
ap_kb_step: .res 1              ; $6017
ap_enemy_active: .res 1         ; $6018
ap_armor_item_active: .res 1    ; $6019 (mirrored with item slot 0 for tests)

; Extended task 3-3 state (WRAM $601A+)
ap_weapon: .res 1               ; $601A (0=Lance, 1=Dagger, 2=Torch)
ap_riding_plat: .res 1          ; $601B ($ff = none, 0..3 = platform slot)
ap_pot_enemy_active: .res 1     ; $601C
ap_probe_x: .res 2              ; $601D
ap_probe_y: .res 1              ; $601F
ap_prev_feet: .res 1            ; $6020
ap_tile_row_top: .res 1         ; $6021
ap_tmp0: .res 1                 ; $6022
ap_tmp1: .res 1                 ; $6023
ap_tmp2: .res 1                 ; $6024
ap_loop_idx: .res 1             ; $6025

; Player shot pool (3 slots, 16-bit X)
ps_active: .res ap_max_shots    ; 0=free, 1=flying, 2=torch flame on ground
ps_kind: .res ap_max_shots
ps_x_lo: .res ap_max_shots
ps_x_hi: .res ap_max_shots
ps_y: .res ap_max_shots
ps_vx: .res ap_max_shots        ; signed px/frame
ps_vy: .res ap_max_shots        ; signed px/frame (torch parabola)
ps_timer: .res ap_max_shots     ; remaining flight / flame lifetime

; Dynamic platform pool (4 slots, 16-bit X)
plat_type: .res ap_max_plats
plat_x_lo: .res ap_max_plats
plat_x_hi: .res ap_max_plats
plat_y: .res ap_max_plats
plat_dx: .res ap_max_plats      ; signed delta applied this frame
plat_dy: .res ap_max_plats      ; signed delta applied this frame
plat_timer: .res ap_max_plats
plat_phase: .res ap_max_plats   ; 0 = positive direction, 1 = reverse
plat_span: .res ap_max_plats

; Item / drop pool (4 slots, 16-bit X; slot 0 mirrors ap_armor_item_active)
item_type: .res ap_max_items
item_x_lo: .res ap_max_items
item_x_hi: .res ap_max_items
item_y: .res ap_max_items

.segment "BANK3"
    .byte bank_tag_base + 3

ap_init_stage:
    lda #ap_state_ground
    sta ap_state
    lda #ap_armor_full
    sta ap_armor
    lda #ap_weapon_lance
    sta ap_weapon
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_invuln
    sta ap_crouch
    sta ap_shoot_lock
    sta ap_air_dir
    sta ap_kb_dir
    sta ap_kb_step
    sta ranged_active
    sta bolt_active
    sta enemy_active
    lda #1
    sta ap_enemy_active
    sta ap_pot_enemy_active
    sta ap_armor_item_active
    inc campaign_hud_dirty

    ; Clear shot pool
    ldx #ap_max_shots - 1
    lda #0
:
    sta ps_active,x
    dex
    bpl :-

    ; Clear item pool, then place armor recovery (slot 0) and dagger (slot 1)
    ldx #ap_max_items - 1
    lda #0
:
    sta item_type,x
    dex
    bpl :-
    lda #item_armor
    sta item_type+0
    lda #<arcade_armor_item_x
    sta item_x_lo+0
    lda #>arcade_armor_item_x
    sta item_x_hi+0
    lda #arcade_armor_item_feet
    sta item_y+0

    lda #item_dagger
    sta item_type+1
    lda #<arcade_dagger_item_x
    sta item_x_lo+1
    lda #>arcade_dagger_item_x
    sta item_x_hi+1
    lda #arcade_dagger_item_feet
    sta item_y+1

    ; Initialize dynamic platforms:
    ; Slot 0: Tombstone obstacle/platform at col 26 (X=416, Y=160)
    ; Slot 1: Horizontal moving platform across X=512 page boundary (X=500..544, Y=160)
    ; Slot 2: Vertical moving platform at col 36 (X=576, Y=176..136)
    ; Slot 3: Free
    ldx #ap_max_plats - 1
    lda #0
:
    sta plat_type,x
    sta plat_dx,x
    sta plat_dy,x
    sta plat_timer,x
    sta plat_phase,x
    dex
    bpl :-

    lda #plat_tombstone
    sta plat_type+0
    lda #<arcade_tombstone_x
    sta plat_x_lo+0
    lda #>arcade_tombstone_x
    sta plat_x_hi+0
    lda #arcade_tombstone_y
    sta plat_y+0

    lda #plat_moving_h
    sta plat_type+1
    lda #<arcade_hplat_x
    sta plat_x_lo+1
    lda #>arcade_hplat_x
    sta plat_x_hi+1
    lda #arcade_hplat_y
    sta plat_y+1
    lda #arcade_hplat_span
    sta plat_span+1

    lda #plat_moving_v
    sta plat_type+2
    lda #<arcade_vplat_x
    sta plat_x_lo+2
    lda #>arcade_vplat_x
    sta plat_x_hi+2
    lda #arcade_vplat_y
    sta plat_y+2
    lda #arcade_vplat_span
    sta plat_span+2
    rts

ap_update_frame:
    lda player_hit
    beq @alive
    jmp @camera_and_submit
@alive:
    lda ap_invuln
    beq :+
    dec ap_invuln
:
    lda ap_shoot_lock
    beq :+
    dec ap_shoot_lock
:
    jsr ap_update_platforms
    jsr ap_carry_player_on_platform
    lda player_feet
    sta ap_prev_feet

    lda ap_state
    cmp #ap_state_ground
    bne @not_ground
    jsr ap_step_ground
    jmp @post_move
@not_ground:
    cmp #ap_state_air
    bne @not_air
    jsr ap_step_air
    jmp @post_move
@not_air:
    cmp #ap_state_ladder
    bne @not_ladder
    jsr ap_step_ladder
    jmp @post_move
@not_ladder:
    cmp #ap_state_knockback
    bne @post_move
    jsr ap_step_knockback

@post_move:
    jsr ap_update_shots
    jsr ap_check_items
    jsr ap_check_enemies_contact
    jsr ap_check_hazard
    ; Tick stage clock while alive; simultaneous timeout + hit deducts one life.
    jsr tick_campaign_clock
    lda player_hit
    beq @check_goal
    lda #ap_armor_dead
    sta ap_armor
    lda #ap_state_dead
    sta ap_state
    dec campaign_lives
    bne @retry_hud
    jmp show_game_over
@retry_hud:
    inc campaign_hud_dirty
    jmp @camera_and_submit

@check_goal:
    jsr check_course_progress
    bcc @camera_and_submit
    jmp show_course_clear

@camera_and_submit:
    jsr follow_player
    jmp submit_frame

; --- Dynamic platforms: tombstone, horizontal moving, vertical moving ---
ap_update_platforms:
    ldx #0
@plat_loop:
    lda #0
    sta plat_dx,x
    sta plat_dy,x
    lda plat_type,x
    cmp #plat_moving_h
    beq @do_moving_h
    cmp #plat_moving_v
    beq @do_moving_v
    jmp @next_plat
@do_moving_h:
    lda plat_phase,x
    bne @h_left
    inc plat_x_lo,x
    bne :+
    inc plat_x_hi,x
:
    lda #1
    sta plat_dx,x
    jmp @advance_plat_timer
@h_left:
    lda plat_x_lo,x
    bne :+
    dec plat_x_hi,x
:
    dec plat_x_lo,x
    lda #$ff
    sta plat_dx,x
    jmp @advance_plat_timer
@do_moving_v:
    lda plat_phase,x
    bne @v_down
    dec plat_y,x
    lda #$ff
    sta plat_dy,x
    jmp @advance_plat_timer
@v_down:
    inc plat_y,x
    lda #1
    sta plat_dy,x
@advance_plat_timer:
    inc plat_timer,x
    lda plat_timer,x
    cmp plat_span,x
    bcc @next_plat
    lda #0
    sta plat_timer,x
    lda plat_phase,x
    eor #1
    sta plat_phase,x
@next_plat:
    inx
    cpx #ap_max_plats
    bne @plat_loop
    rts

ap_carry_player_on_platform:
    lda ap_state
    cmp #ap_state_ground
    bne @carry_done
    ldx ap_riding_plat
    bmi @carry_done
    lda plat_dx,x
    beq @carry_y
    bmi @carry_left
    stx ap_tmp2
    jsr ap_move_right_1px
    ldx ap_tmp2
    jmp @carry_y
@carry_left:
    stx ap_tmp2
    jsr ap_move_left_1px
    ldx ap_tmp2
@carry_y:
    lda plat_dy,x
    beq @carry_done
    clc
    adc player_feet
    sta player_feet
@carry_done:
    rts

; --- Ground state: walk, crouch, ladder entry, shoot lock, jump ---
ap_step_ground:
    lda pad
    and #$08                    ; Up
    beq @check_ladder_down
    jsr ap_try_enter_ladder_up
    bcc @check_ladder_down
    rts
@check_ladder_down:
    lda pad
    and #$04                    ; Down
    beq @not_crouching
    jsr ap_try_enter_ladder_down
    bcc @enter_crouch
    rts
@enter_crouch:
    lda #1
    sta ap_crouch
    jmp @check_ground_jump
@not_crouching:
    lda #0
    sta ap_crouch
@check_ground_jump:
    lda ap_crouch
    bne @check_ground_walk
    lda pressed
    and #$80
    beq @check_ground_walk
    ; Lock horizontal air direction at takeoff (arcade GnG behavior).
    lda #0
    sta ap_air_dir
    lda #$ff
    sta ap_riding_plat
    lda ap_shoot_lock
    bne @begin_air_jump
    lda pad
    and #3
    cmp #1
    beq @lock_right
    cmp #2
    beq @lock_left
    bne @begin_air_jump
@lock_right:
    lda #0
    sta player_left
    lda #1
    sta ap_air_dir
    bne @begin_air_jump
@lock_left:
    lda #1
    sta player_left
    lda #2
    sta ap_air_dir
@begin_air_jump:
    lda #ap_state_air
    sta ap_state
    lda #0
    sta jump_step
    jmp ap_step_air_vertical

@check_ground_walk:
    lda ap_crouch
    bne @check_ground_support
    lda ap_shoot_lock
    bne @check_ground_support
    lda pad
    and #3
    beq @check_ground_support
    cmp #3
    beq @check_ground_support
    cmp #1
    bne @walk_left
    lda #0
    sta player_left
    jsr ap_move_right_1px
    jmp @check_ground_support
@walk_left:
    lda #1
    sta player_left
    jsr ap_move_left_1px

@check_ground_support:
    jsr ap_check_support
    bcs @ground_done
    lda #ap_state_air
    sta ap_state
    lda #0
    sta ap_air_dir
    sta ap_crouch
    lda #$ff
    sta ap_riding_plat
    lda #jump_fall
    sta jump_step
@ground_done:
    rts

; --- Air state: fixed horizontal velocity, ceiling, landing ---
ap_step_air:
    lda ap_air_dir
    cmp #1
    bne @air_not_right
    jsr ap_move_right_1px
    jmp ap_step_air_vertical
@air_not_right:
    cmp #2
    bne ap_step_air_vertical
    jsr ap_move_left_1px

ap_step_air_vertical:
    ldx jump_step
    lda jump_delta,x
    bmi @air_rising
    clc
    adc player_feet
    sta player_feet
    jsr ap_advance_jump_step
    jsr ap_check_support
    bcc @air_done
    lda ap_tile_row_top
    sta player_feet
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
    lda #0
    sta ap_air_dir
@air_done:
    rts

@air_rising:
    clc
    adc player_feet
    sta player_feet
    sec
    sbc #player_height
    sta ap_probe_y
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    jsr ap_tile_type_at
    cmp #tile_type_solid
    bne @air_advance_rise
    lda ap_probe_y
    and #$f0
    clc
    adc #16 + player_height
    sta player_feet
    lda #jump_fall
    sta jump_step
    rts
@air_advance_rise:
    jmp ap_advance_jump_step

ap_advance_jump_step:
    ldx jump_step
    inx
    cpx #jump_length
    bcc :+
    dex
:
    stx jump_step
    rts

; --- Ladder state: vertical climbing only ---
ap_step_ladder:
    lda #0
    sta ap_crouch
    lda #$ff
    sta ap_riding_plat
    lda pad
    and #$08
    bne @climb_up
    lda pad
    and #$04
    bne @climb_down
    rts
@climb_up:
    dec player_feet
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sec
    sbc #1
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    beq @ladder_done
    lda player_feet
    and #$f0
    sta player_feet
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
@ladder_done:
    rts
@climb_down:
    inc player_feet
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_solid
    bne @check_still_ladder
    lda player_feet
    and #$f0
    sta player_feet
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
    rts
@check_still_ladder:
    cmp #tile_type_ladder
    beq @ladder_done
    lda #ap_state_air
    sta ap_state
    lda #0
    sta ap_air_dir
    lda #jump_fall
    sta jump_step
    rts

ap_try_enter_ladder_up:
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sec
    sbc #4
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    bne @no_ladder
    jsr ap_snap_to_ladder
    sec
    rts
@no_ladder:
    clc
    rts

ap_try_enter_ladder_down:
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    bne @no_ladder_down
    jsr ap_snap_to_ladder
    inc player_feet
    sec
    rts
@no_ladder_down:
    clc
    rts

ap_snap_to_ladder:
    lda player_x
    and #$f0
    ora #$08
    sta player_x
    lda #ap_state_ladder
    sta ap_state
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_crouch
    sta ap_air_dir
    rts

; --- Knockback state after losing armor ---
ap_step_knockback:
    lda #$ff
    sta ap_riding_plat
    lda ap_kb_dir
    cmp #1
    bne @kb_left
    jsr ap_move_right_1px
    jmp @kb_vert
@kb_left:
    jsr ap_move_left_1px
@kb_vert:
    ldx ap_kb_step
    lda ap_kb_delta,x
    clc
    adc player_feet
    sta player_feet
    inx
    stx ap_kb_step
    cpx #ap_kb_length
    bcs @kb_finish
    lda ap_kb_delta-1,x
    bmi @kb_done
    jsr ap_check_support
    bcc @kb_done
    lda ap_tile_row_top
    sta player_feet
@kb_finish:
    jsr ap_check_support
    bcs @kb_land
    lda #ap_state_air
    sta ap_state
    lda #0
    sta ap_air_dir
    lda #jump_fall
    sta jump_step
    rts
@kb_land:
    lda ap_tile_row_top
    sta player_feet
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
@kb_done:
    rts

; --- Horizontal movement with solid wall check ---
ap_move_right_1px:
    lda player_x
    sta old_x
    lda player_x+1
    sta old_x+1
    cmp stage_goal_x_hi
    bne @do_inc_x
    lda player_x
    cmp stage_goal_x_lo
    beq @right_blocked
@do_inc_x:
    inc player_x
    bne :+
    inc player_x+1
:
    lda player_x
    clc
    adc #6
    sta ap_probe_x
    lda player_x+1
    adc #0
    sta ap_probe_x+1
    jsr ap_check_side_wall
    bcc @right_blocked
    lda old_x
    sta player_x
    lda old_x+1
    sta player_x+1
@right_blocked:
    rts

ap_move_left_1px:
    lda player_x
    sta old_x
    lda player_x+1
    sta old_x+1
    lda player_x+1
    bne @do_dec_x
    lda player_x
    cmp #8
    beq @left_blocked
@do_dec_x:
    lda player_x
    bne :+
    dec player_x+1
:
    dec player_x
    lda player_x
    sec
    sbc #6
    sta ap_probe_x
    lda player_x+1
    sbc #0
    sta ap_probe_x+1
    jsr ap_check_side_wall
    bcc @left_blocked
    lda old_x
    sta player_x
    lda old_x+1
    sta player_x+1
@left_blocked:
    rts

ap_check_side_wall:
    lda player_feet
    sec
    sbc #2
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_solid
    beq @wall_hit
    lda player_feet
    sec
    ldx ap_crouch
    beq @stand_wall
    sbc #8
    jmp @test_upper_wall
@stand_wall:
    sbc #12
@test_upper_wall:
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_solid
    beq @wall_hit
    clc
    rts
@wall_hit:
    sec
    rts

; --- Support check: dynamic platforms first (+/-12 px), then two-point 2D tiles ---
ap_check_support:
    lda #$ff
    sta ap_riding_plat
    ldx #0
@check_plat_loop:
    lda plat_type,x
    beq @next_plat_chk
    ; Check 16-bit |player_x - plat_x[x]| <= 12
    sec
    lda player_x
    sbc plat_x_lo,x
    sta ap_tmp0
    lda player_x+1
    sbc plat_x_hi,x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #12
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @next_plat_chk
    lda ap_tmp0
    cmp #25
    bcs @next_plat_chk
    ; Check vertical crossing: prev_feet <= plat_y and player_feet >= plat_y
    lda plat_y,x
    cmp ap_prev_feet
    bcc @next_plat_chk
    cmp player_feet
    beq @landed_on_plat
    bcs @next_plat_chk
@landed_on_plat:
    sta ap_tile_row_top
    stx ap_riding_plat
    sec
    rts
@next_plat_chk:
    inx
    cpx #ap_max_plats
    bne @check_plat_loop

    ; Static 2D tile support (left foot X-4, right foot X+4)
    lda player_feet
    sta ap_probe_y
    and #$f0
    sta ap_tile_row_top
    lda player_x
    sec
    sbc #4
    sta ap_probe_x
    lda player_x+1
    sbc #0
    sta ap_probe_x+1
    jsr ap_probe_foot
    bcs @supported
    lda player_x
    clc
    adc #4
    sta ap_probe_x
    lda player_x+1
    adc #0
    sta ap_probe_x+1
    jmp ap_probe_foot
@supported:
    rts

ap_probe_foot:
    jsr ap_tile_type_at
    cmp #tile_type_solid
    beq @foot_solid
    cmp #tile_type_oneway
    beq @foot_oneway
    cmp #tile_type_ladder
    beq @foot_ladder_top
    clc
    rts
@foot_solid:
    sec
    rts
@foot_oneway:
    lda ap_prev_feet
    cmp ap_tile_row_top
    beq @foot_solid
    bcc @foot_solid
    clc
    rts
@foot_ladder_top:
    lda ap_prev_feet
    cmp ap_tile_row_top
    beq @check_above_ladder
    bcc @check_above_ladder
    clc
    rts
@check_above_ladder:
    lda ap_probe_y
    sec
    sbc #16
    sta ap_probe_y
    jsr ap_tile_type_at
    ldx ap_tile_row_top
    stx ap_probe_y
    cmp #tile_type_ladder
    beq @not_ladder_top
    sec
    rts
@not_ladder_top:
    clc
    rts

; --- Hazard / pit check (fatal even with full armor) ---
ap_check_hazard:
    lda player_feet
    cmp #232
    bcs @fatal_hazard
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sec
    sbc #1
    sta ap_probe_y
    jsr ap_tile_type_at
    cmp #tile_type_hazard
    beq @fatal_hazard
    rts
@fatal_hazard:
    lda #1
    sta player_hit
    rts

; --- Tile lookup helper: (ap_probe_x, ap_probe_y) -> A = tile_type_* ---
ap_tile_type_at:
    lda ap_probe_y
    lsr a
    lsr a
    lsr a
    lsr a
    tay
    ldx ap_probe_x+1
    lda ap_probe_x
    lsr a
    lsr a
    lsr a
    lsr a
    ora page_columns,x
    jsr read_map_cell
    tax
    lda metatile_attr,x
    and #tile_type_mask
    rts

; --- Multi-weapon 16-bit shot pool (Lance 2, Dagger 3, Torch 2 + flame) ---
ap_update_shots:
    jsr ap_try_spawn_shot
    ldx #0
@shot_loop:
    stx ap_loop_idx
    lda ps_active,x
    bne :+
    jmp @next_shot
:
    cmp #2
    bne @shot_flying
    ; Torch ground flame: count down and still test enemy hits.
    jsr ap_shot_check_enemies
    ldx ap_loop_idx
    dec ps_timer,x
    bne :+
    lda #0
    sta ps_active,x
:
    jmp @next_shot

@shot_flying:
    ; Horizontal 16-bit step by signed ps_vx[x]
    lda ps_vx,x
    bmi @shot_step_left
    clc
    adc ps_x_lo,x
    sta ps_x_lo,x
    bcc @shot_vert
    inc ps_x_hi,x
    jmp @shot_vert
@shot_step_left:
    clc
    adc ps_x_lo,x
    sta ps_x_lo,x
    bcs @shot_vert
    dec ps_x_hi,x

@shot_vert:
    ; Torch parabola: apply vertical velocity and gravity every 2 frames.
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne @shot_bounds
    lda ps_vy,x
    clc
    adc ps_y,x
    sta ps_y,x
    lda ps_timer,x
    and #1
    bne @shot_bounds
    lda ps_vy,x
    cmp #4
    beq @shot_bounds
    clc
    adc #1
    sta ps_vy,x

@shot_bounds:
    ; Off-map or off-camera reclamation (16-bit camera check)
    lda ps_x_hi,x
    cmp #3
    bcs @reclaim_shot
    sec
    lda ps_x_lo,x
    sbc camera_low
    sta ap_tmp0
    lda ps_x_hi,x
    sbc camera_high
    bne @reclaim_shot

    ; Check solid tile / torch landing on floor or one-way
    lda ps_x_lo,x
    sta ap_probe_x
    lda ps_x_hi,x
    sta ap_probe_x+1
    lda ps_y,x
    sta ap_probe_y
    jsr ap_tile_type_at
    ldx ap_loop_idx
    cmp #tile_type_solid
    beq @shot_hit_terrain
    cmp #tile_type_oneway
    bne @shot_check_tombstone
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne @shot_check_tombstone
    lda ps_vy,x
    bmi @shot_check_tombstone
    bpl @torch_ignite

@shot_hit_terrain:
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne @reclaim_shot
    lda ps_vy,x
    bmi @reclaim_shot
@torch_ignite:
    lda ps_y,x
    and #$f0
    sec
    sbc #4
    sta ps_y,x
    lda #2
    sta ps_active,x
    lda #ap_torch_flame_frames
    sta ps_timer,x
    jmp @next_shot

@shot_check_tombstone:
    ; Indestructible tombstone blocks player projectiles.
    jsr ap_shot_hits_tombstone
    ldx ap_loop_idx
    bcs @reclaim_shot
    jsr ap_shot_check_enemies
    ldx ap_loop_idx
    bcs @reclaim_shot
    dec ps_timer,x
    bne @next_shot
@reclaim_shot:
    lda #0
    sta ps_active,x
@next_shot:
    inx
    cpx #ap_max_shots
    beq :+
    jmp @shot_loop
:
    rts

ap_try_spawn_shot:
    lda ap_state
    cmp #ap_state_ladder
    bne :+
    rts
:
    cmp #ap_state_knockback
    bne :+
    rts
:
    lda pressed
    and #$40
    bne :+
    rts
:
    ; Enforce per-weapon active shot cap (Lance=2, Dagger=3, Torch=2).
    ldx ap_weapon
    lda ap_weapon_max_shots,x
    sta ap_tmp0                 ; cap
    lda #0
    sta ap_tmp1                 ; active count
    lda #$ff
    sta ap_tmp2                 ; first free slot
    ldx #0
@count_shots:
    lda ps_active,x
    beq @found_free
    inc ap_tmp1
    jmp @next_count
@found_free:
    lda ap_tmp2
    bpl @next_count
    stx ap_tmp2
@next_count:
    inx
    cpx #ap_max_shots
    bne @count_shots
    lda ap_tmp1
    cmp ap_tmp0
    bcc :+
    rts
:
    ldx ap_tmp2
    bpl :+
    rts
:
    lda #1
    sta ps_active,x
    lda ap_weapon
    sta ps_kind,x
    lda player_x
    sta ps_x_lo,x
    lda player_x+1
    sta ps_x_hi,x
    lda player_feet
    sec
    ldy ap_crouch
    beq @stand_y
    sbc #5
    jmp @set_sy
@stand_y:
    sbc #9
@set_sy:
    sta ps_y,x
    ldy ap_weapon
    lda player_left
    bne @neg_vx
    lda ap_weapon_vx_pos,y
    jmp @set_vx
@neg_vx:
    lda ap_weapon_vx_neg,y
@set_vx:
    sta ps_vx,x
    lda ap_weapon_vy_init,y
    sta ps_vy,x
    lda ap_weapon_life,y
    sta ps_timer,x
    lda #1
    sta shot_sound_pending
    lda ap_state
    cmp #ap_state_ground
    bne @spawn_done
    lda #ap_shoot_lock_frames
    sta ap_shoot_lock
@spawn_done:
    rts

; Carry set if shot slot ap_loop_idx hits any tombstone platform.
ap_shot_hits_tombstone:
    ldy #0
@tomb_loop:
    lda plat_type,y
    cmp #plat_tombstone
    bne @next_tomb
    ldx ap_loop_idx
    sec
    lda ps_x_lo,x
    sbc plat_x_lo,y
    sta ap_tmp0
    lda ps_x_hi,x
    sbc plat_x_hi,y
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #8
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @next_tomb
    lda ap_tmp0
    cmp #17
    bcs @next_tomb
    lda ps_y,x
    cmp plat_y,y
    bcc @next_tomb
    lda plat_y,y
    clc
    adc #16
    cmp ps_y,x
    bcc @next_tomb
    sec
    rts
@next_tomb:
    iny
    cpy #ap_max_plats
    bne @tomb_loop
    clc
    rts

; Carry set if shot slot ap_loop_idx destroyed an enemy.
ap_shot_check_enemies:
    ldx ap_loop_idx
    lda ap_enemy_active
    beq @check_pot_enemy
    sec
    lda ps_x_lo,x
    sbc #<arcade_enemy_x
    sta ap_tmp0
    lda ps_x_hi,x
    sbc #>arcade_enemy_x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #8
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @check_pot_enemy
    lda ap_tmp0
    cmp #17
    bcs @check_pot_enemy
    lda ps_y,x
    cmp #arcade_enemy_feet - player_height
    bcc @check_pot_enemy
    cmp #arcade_enemy_feet
    bcs @check_pot_enemy
    lda #0
    sta ap_enemy_active
    sec
    rts

@check_pot_enemy:
    lda ap_pot_enemy_active
    beq @no_enemy_hit
    sec
    lda ps_x_lo,x
    sbc #<arcade_pot_enemy_x
    sta ap_tmp0
    lda ps_x_hi,x
    sbc #>arcade_pot_enemy_x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #8
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @no_enemy_hit
    lda ap_tmp0
    cmp #17
    bcs @no_enemy_hit
    lda ps_y,x
    cmp #arcade_pot_enemy_feet - player_height
    bcc @no_enemy_hit
    cmp #arcade_pot_enemy_feet
    bcs @no_enemy_hit
    ; Pot enemy defeated: drop Torch weapon item at its 16-bit position!
    lda #0
    sta ap_pot_enemy_active
    lda #item_torch
    sta item_type+2
    lda #<arcade_pot_enemy_x
    sta item_x_lo+2
    lda #>arcade_pot_enemy_x
    sta item_x_hi+2
    lda #arcade_pot_enemy_feet
    sta item_y+2
    sec
    rts
@no_enemy_hit:
    clc
    rts

; --- Item / weapon drop pickup (16-bit safe) ---
ap_check_items:
    ldx #0
@item_loop:
    lda item_type,x
    beq @next_item
    sec
    lda player_x
    sbc item_x_lo,x
    sta ap_tmp0
    lda player_x+1
    sbc item_x_hi,x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #10
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @next_item
    lda ap_tmp0
    cmp #21
    bcs @next_item
    lda player_feet
    sec
    sbc item_y,x
    clc
    adc #10
    cmp #21
    bcs @next_item
    ; Apply pickup effect
    lda item_type,x
    cmp #item_armor
    bne @check_weapon_drop
    lda #ap_armor_full
    sta ap_armor
    cpx #0
    bne @consume_item
    lda #0
    sta ap_armor_item_active
    beq @consume_item
@check_weapon_drop:
    cmp #item_dagger
    bne @check_torch_drop
    lda #ap_weapon_dagger
    jmp @set_weapon_hud
@check_torch_drop:
    cmp #item_torch
    bne @set_lance_drop
    lda #ap_weapon_torch
    jmp @set_weapon_hud
@set_lance_drop:
    lda #ap_weapon_lance
@set_weapon_hud:
    sta ap_weapon
    inc campaign_hud_dirty
@consume_item:
    lda #item_none
    sta item_type,x
@next_item:
    inx
    cpx #ap_max_items
    bne @item_loop
    rts

; --- Enemy contact (both test enemy and pot enemy) ---
ap_check_enemies_contact:
    lda ap_invuln
    beq :+
    rts
:
    lda ap_enemy_active
    beq @check_pot_contact
    lda #<arcade_enemy_x
    sta ap_probe_x
    lda #>arcade_enemy_x
    sta ap_probe_x+1
    lda #arcade_enemy_feet
    sta ap_probe_y
    jsr ap_test_contact_at_probe
    bcs @do_player_damage
@check_pot_contact:
    lda ap_pot_enemy_active
    beq @contact_none
    lda #<arcade_pot_enemy_x
    sta ap_probe_x
    lda #>arcade_pot_enemy_x
    sta ap_probe_x+1
    lda #arcade_pot_enemy_feet
    sta ap_probe_y
    jsr ap_test_contact_at_probe
    bcc @contact_none
@do_player_damage:
    lda ap_armor
    cmp #ap_armor_full
    bne @fatal_contact
    lda #ap_armor_naked
    sta ap_armor
    lda #ap_invuln_frames
    sta ap_invuln
    lda #ap_state_knockback
    sta ap_state
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_kb_step
    sta ap_crouch
    sta ap_shoot_lock
    lda player_left
    beq @kb_push_left
    lda #1
    sta ap_kb_dir
    rts
@kb_push_left:
    lda #2
    sta ap_kb_dir
    rts
@fatal_contact:
    lda #1
    sta player_hit
@contact_none:
    rts

ap_test_contact_at_probe:
    sec
    lda player_x
    sbc ap_probe_x
    sta ap_tmp0
    lda player_x+1
    sbc ap_probe_x+1
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #10
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @miss
    lda ap_tmp0
    cmp #21
    bcs @miss
    lda ap_probe_y
    sec
    sbc #player_height - 1
    cmp player_feet
    beq @check_bottom
    bcs @miss
@check_bottom:
    lda ap_probe_y
    clc
    ldx ap_crouch
    beq @stand_bottom
    adc #10
    jmp @cmp_bottom
@stand_bottom:
    adc #player_height
@cmp_bottom:
    cmp player_feet
    bcc @miss
    beq @miss
    sec
    rts
@miss:
    clc
    rts

; --- OAM sprite rendering for arcade mode ---
ap_draw_sprites:
    lda #$f8
    ldx #24 * 4
:
    sta sprites,x
    dex
    dex
    dex
    dex
    bpl :-

    ; Slots 0-1: Player
    lda #0
    sta draw_slot
    lda ap_state
    cmp #ap_state_dead
    beq @draw_player_now
    cmp #ap_state_knockback
    beq @draw_player_now
    lda ap_invuln
    and #4
    bne @draw_enemies
@draw_player_now:
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
    sec
    lda player_feet
    sbc #player_height+1
    ldx ap_crouch
    beq :+
    clc
    adc #4
:
    jsr set_sprite_y
    lda #1
    sta draw_tile
    lda player_x
    ldx player_x+1
    jsr draw_pair

@draw_enemies:
    ; Slots 2-3: Test enemy 1
    lda ap_enemy_active
    beq @draw_pot_enemy
    lda #2 * 4
    sta draw_slot
    lda #1
    sta draw_palette
    lda #arcade_enemy_feet - player_height - 1
    jsr set_sprite_y
    lda #5
    sta draw_tile
    lda #<arcade_enemy_x
    ldx #>arcade_enemy_x
    jsr draw_pair
@draw_pot_enemy:
    ; Slots 7-8: Pot-carrying enemy 2
    lda ap_pot_enemy_active
    beq @draw_shots
    lda #7 * 4
    sta draw_slot
    lda #3
    sta draw_palette
    lda #arcade_pot_enemy_feet - player_height - 1
    jsr set_sprite_y
    lda #5
    sta draw_tile
    lda #<arcade_pot_enemy_x
    ldx #>arcade_pot_enemy_x
    jsr draw_pair

@draw_shots:
    ; Slots 4-6: Player shot pool (slot 4 = $210 preserved for compatibility)
    lda #4 * 4
    sta draw_slot
    ldy #0
@draw_shot_loop:
    lda ps_active,y
    beq @skip_shot_draw
    sty ap_loop_idx
    lda #3
    ldx ps_active,y
    cpx #2
    bne :+
    lda #2                      ; Ground flame uses bright red/orange palette
:
    sta draw_palette
    lda #9
    cpx #2
    bne :+
    lda #15                     ; Flame spark tile
:
    sta draw_tile
    lda ps_y,y
    sec
    sbc #2
    jsr set_sprite_y
    ldy ap_loop_idx
    lda ps_x_lo,y
    ldx ps_x_hi,y
    jsr set_sprite_x
    jsr emit_sprite
    ldy ap_loop_idx
    jmp @next_shot_draw
@skip_shot_draw:
    lda draw_slot
    clc
    adc #4
    sta draw_slot
@next_shot_draw:
    iny
    cpy #ap_max_shots
    bne @draw_shot_loop

    ; Slots 9-14: Dynamic platforms (2 tiles wide each)
    lda #9 * 4
    sta draw_slot
    ldy #0
@draw_plat_loop:
    lda plat_type,y
    beq @next_plat_draw
    sty ap_loop_idx
    lda #3
    cmp plat_type,y
    bne :+
    lda #0
:
    sta draw_palette
    lda #11
    sta draw_tile
    lda plat_y,y
    sec
    sbc #1
    jsr set_sprite_y
    ldy ap_loop_idx
    lda plat_x_lo,y
    ldx plat_x_hi,y
    jsr draw_pair
    ldy ap_loop_idx
@next_plat_draw:
    iny
    cpy #ap_max_plats
    bne @draw_plat_loop

    ; Slots 17-20: Items / weapon drops
    lda #17 * 4
    sta draw_slot
    ldy #0
@draw_item_loop:
    lda item_type,y
    beq @next_item_draw
    sty ap_loop_idx
    lda #3
    sta draw_palette
    lda #15
    sta draw_tile
    lda item_y,y
    sec
    sbc #9
    jsr set_sprite_y
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

ap_weapon_max_shots: .byte 2, 3, 2
ap_weapon_vx_pos:    .byte 3, 4, 2
ap_weapon_vx_neg:    .byte $fd, $fc, $fe
ap_weapon_vy_init:   .byte 0, 0, $fc
ap_weapon_life:      .byte 48, 48, 56

ap_kb_delta:
    .byte $fe, $fe, $ff, $ff, $ff, $00, $00, $00
    .byte $01, $01, $01, $02, $02, $02, $03, $03
