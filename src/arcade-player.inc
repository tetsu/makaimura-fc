; Bank 3 ($A000-$BFFF): Data-driven arcade player physics, 16-bit X/Y 2D tile
; collision, two-stage armor, 16-bit SoA entity pools, vertical/horizontal
; camera tracking, multi-part boss, and score/extend system.
; Reads all stage-specific parameters from the $8080 arcade_stage_desc block
; in the active $8000 stage bank (Bank 12+).

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
ap_weapon_shield = 3

ap_max_shots = 3
ap_max_plats = 4
ap_max_items = 4
ap_max_enemies = 4
ap_max_ebolts = 2

en_none = 0
en_zombie = 1
en_turret = 2

plat_none = 0
plat_tombstone = 1
plat_moving_h = 2
plat_moving_v = 3

item_none = 0
item_armor = 1
item_dagger = 2
item_torch = 3
item_lance = 4
item_key = 5
item_shield = 6

boss_idle = 0
boss_active = 1
boss_defeated = 2

ap_invuln_frames = 120
ap_shoot_lock_frames = 15
ap_kb_length = 16
ap_torch_flame_frames = 30
ap_rank_step_frames = 240
ap_turret_cooldown = 72
ap_turret_windup = 24

; Score values (in 100-point units) and extend thresholds
ap_score_zombie = 1             ; 100 pts
ap_score_enemy  = 2             ; 200 pts
ap_score_item   = 1             ; 100 pts
ap_score_key    = 5             ; 500 pts
ap_score_boss   = 10            ; 1,000 pts
ap_extend_first = 20            ; 2,000 pts
ap_extend_step  = 50            ; +5,000 pts
ap_max_lives    = 9

.segment "WRAM"
; Existing WRAM layout $6000-$60AB preserved verbatim for regression tests.
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

ap_weapon: .res 1               ; $601A (0=Lance, 1=Dagger, 2=Torch, 3=Shield)
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

ps_active: .res ap_max_shots    ; $6026
ps_kind: .res ap_max_shots
ps_x_lo: .res ap_max_shots
ps_x_hi: .res ap_max_shots
ps_y: .res ap_max_shots
ps_vx: .res ap_max_shots
ps_vy: .res ap_max_shots
ps_timer: .res ap_max_shots

plat_type: .res ap_max_plats
plat_x_lo: .res ap_max_plats
plat_x_hi: .res ap_max_plats
plat_y: .res ap_max_plats
plat_dx: .res ap_max_plats
plat_dy: .res ap_max_plats
plat_timer: .res ap_max_plats
plat_phase: .res ap_max_plats
plat_span: .res ap_max_plats

item_type: .res ap_max_items
item_x_lo: .res ap_max_items
item_x_hi: .res ap_max_items
item_y: .res ap_max_items

ap_rank: .res 1                 ; $6072
ap_rank_timer: .res 1           ; $6073
sched_page: .res 1              ; $6074
sched_group: .res 1             ; $6075
sched_wait: .res 1              ; $6076
sched_zombies_live: .res 1      ; $6077
ap_turret_spawned: .res 1       ; $6078

en_type: .res ap_max_enemies    ; $6079
en_x_lo: .res ap_max_enemies    ; $607D
en_x_hi: .res ap_max_enemies    ; $6081
en_y: .res ap_max_enemies       ; $6085
en_dir: .res ap_max_enemies     ; $6089
en_timer: .res ap_max_enemies   ; $608D
en_hp: .res ap_max_enemies      ; $6091

eb_active: .res ap_max_ebolts   ; $6095
eb_x_lo: .res ap_max_ebolts     ; $6097
eb_x_hi: .res ap_max_ebolts     ; $6099
eb_y: .res ap_max_ebolts        ; $609B
eb_vx: .res ap_max_ebolts       ; $609D
eb_timer: .res ap_max_ebolts    ; $609F

ap_boss_state: .res 1           ; $60A1
ap_boss_lock: .res 1            ; $60A2
ap_boss_hp: .res 1              ; $60A3
ap_boss_shield_hp: .res 1       ; $60A4
ap_boss_x_lo: .res 1            ; $60A5
ap_boss_x_hi: .res 1            ; $60A6
ap_boss_y: .res 1               ; $60A7
ap_boss_dir: .res 1             ; $60A8
ap_boss_timer: .res 1           ; $60A9
ap_boss_flash: .res 1           ; $60AA
ap_key_collected: .res 1        ; $60AB

; Task 4-2 extensions ($60AC+): 16-bit Y coordinates & score/extend state
ap_player_y_hi: .res 1          ; $60AC
ap_prev_feet_hi: .res 1         ; $60AD
ap_probe_y_hi: .res 1           ; $60AE
ap_tile_row_top_hi: .res 1      ; $60AF
ap_cam_y_lo: .res 1             ; $60B0
ap_cam_y_hi: .res 1             ; $60B1
ap_boss_y_hi: .res 1            ; $60B2
ps_y_hi: .res ap_max_shots      ; $60B3
plat_y_hi: .res ap_max_plats    ; $60B6
item_y_hi: .res ap_max_items    ; $60BA
en_y_hi: .res ap_max_enemies    ; $60BE
eb_y_hi: .res ap_max_ebolts     ; $60C2
ap_score: .res 4                ; $60C4: 4 decimal digits (d3..d0, x100 pts)
ap_score_units: .res 2          ; $60C8: 16-bit cumulative 100-pt units
ap_next_extend: .res 2          ; $60CA: next extend threshold in 100-pt units
ap_new_game: .res 1             ; $60CC: 1 on fresh game start from title
ap_tmp3: .res 1                 ; $60CD
ap_loop: .res 1                 ; $60CE: 0 = Loop 1, 1 = Loop 2
ap_bounce_back: .res 1          ; $60CF: 1 when Stage 6 bounced back to Stage 5

.segment "BANK3"
    .byte bank_tag_base + 3

ap_init_stage:
    ; Reset score/extend/rank/loop only on fresh game start from title.
    lda ap_new_game
    beq @keep_score
    lda #0
    sta ap_new_game
    sta ap_score+0
    sta ap_score+1
    sta ap_score+2
    sta ap_score+3
    sta ap_score_units+0
    sta ap_score_units+1
    sta ap_rank
    sta ap_loop
    sta ap_bounce_back
    lda #<ap_extend_first
    sta ap_next_extend+0
    lda #>ap_extend_first
    sta ap_next_extend+1
@keep_score:
    lda #ap_state_ground
    sta ap_state
    lda #ap_armor_full
    sta ap_armor
    lda campaign_stage
    cmp #arcade_stage_count
    beq @keep_stage7_weapon
    lda #ap_weapon_lance
    sta ap_weapon
@keep_stage7_weapon:
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
    sta ap_enemy_active
    sta ap_pot_enemy_active
    sta ap_armor_item_active
    sta ap_boss_state
    sta ap_boss_lock
    sta ap_boss_hp
    sta ap_boss_shield_hp
    sta ap_boss_timer
    sta ap_boss_flash
    sta ap_boss_y_hi
    sta ap_key_collected
    inc campaign_hud_dirty

    ; Load 16-bit player Y from stage descriptor (start or checkpoint).
    lda checkpoint_reached
    bne @init_cp_y
    lda sd_start_y_lo
    sta player_feet
    lda sd_start_y_hi
    sta ap_player_y_hi
    jmp @init_clear_pools
@init_cp_y:
    lda sd_cp_y_lo
    sta player_feet
    lda sd_cp_y_hi
    sta ap_player_y_hi

@init_clear_pools:
    ldx #ap_max_shots - 1
    lda #0
:
    sta ps_active,x
    sta ps_y_hi,x
    dex
    bpl :-

    ldx #ap_max_items - 1
    lda #0
:
    sta item_type,x
    sta item_y_hi,x
    dex
    bpl :-

    ldx #ap_max_plats - 1
    lda #0
:
    sta plat_type,x
    sta plat_dx,x
    sta plat_dy,x
    sta plat_timer,x
    sta plat_phase,x
    sta plat_y_hi,x
    dex
    bpl :-

    ; Load stage-specific palettes and CHR overrides while PPU rendering is off.
    jsr ap_load_stage_visuals

    ; Populate fixed enemies from stage descriptor ($8080+).
    lda sd_enemy1_x
    ora sd_enemy1_x+1
    beq :+
    lda #1
    sta ap_enemy_active
:
    lda sd_pot_x
    ora sd_pot_x+1
    beq :+
    lda #1
    sta ap_pot_enemy_active
:

    ; Populate initial items from sd_items_ptr (5 bytes per entry).
    lda sd_item_count
    beq @load_plats
    sta ap_tmp2
    lda #$ff
    sta map_cached_col
    lda sd_items_ptr
    sta map_ptr
    lda sd_items_ptr+1
    sta map_ptr+1
    ldx #0
    ldy #0
@load_item_loop:
    lda (map_ptr),y
    sta item_type,x
    iny
    lda (map_ptr),y
    sta item_x_lo,x
    iny
    lda (map_ptr),y
    sta item_x_hi,x
    iny
    lda (map_ptr),y
    sta item_y,x
    iny
    lda (map_ptr),y
    sta item_y_hi,x
    iny
    inx
    cpx ap_tmp2
    bne @load_item_loop
    lda item_type+0
    cmp #item_armor
    bne @load_plats
    lda #1
    sta ap_armor_item_active

@load_plats:
    ; Populate dynamic platforms from sd_plats_ptr (6 bytes per entry).
    lda sd_plat_count
    beq @init_scheduler_state
    sta ap_tmp2
    lda #$ff
    sta map_cached_col
    lda sd_plats_ptr
    sta map_ptr
    lda sd_plats_ptr+1
    sta map_ptr+1
    ldx #0
    ldy #0
@load_plat_loop:
    lda (map_ptr),y
    sta plat_type,x
    iny
    lda (map_ptr),y
    sta plat_x_lo,x
    iny
    lda (map_ptr),y
    sta plat_x_hi,x
    iny
    lda (map_ptr),y
    sta plat_y,x
    iny
    lda (map_ptr),y
    sta plat_y_hi,x
    iny
    lda (map_ptr),y
    sta plat_span,x
    iny
    inx
    cpx ap_tmp2
    bne @load_plat_loop

@init_scheduler_state:
    ; On Loop 2, retain difficulty rank with a minimum floor of 1.
    lda ap_loop
    beq @keep_rank_on_retry
    lda ap_rank
    bne @keep_rank_on_retry
    lda #1
    sta ap_rank
@keep_rank_on_retry:
    lda #ap_rank_step_frames
    sta ap_rank_timer
    jsr ap_current_sched_page
    sta sched_page
    lda #0
    sta sched_group
    sta sched_wait
    sta sched_zombies_live
    sta ap_turret_spawned
    ldx #ap_max_enemies - 1
:
    sta en_type,x
    sta en_y_hi,x
    dex
    bpl :-
    ldx #ap_max_ebolts - 1
:
    sta eb_active,x
    sta eb_y_hi,x
    dex
    bpl :-
    jsr ap_update_cam_y
    lda sd_bgm_stage
    jmp call_bank8_play_song

; Load per-stage BG/OBJ palettes and optional 64-byte CHR overrides (blanked PPU).
ap_load_stage_visuals:
    lda #$ff
    sta map_cached_col
    ; BG palette ($3F00, 16 bytes)
    lda sd_bg_pal_ptr
    ora sd_bg_pal_ptr+1
    beq @check_spr_pal
    lda sd_bg_pal_ptr
    sta map_ptr
    lda sd_bg_pal_ptr+1
    sta map_ptr+1
    lda #$3f
    sta $2006
    lda #$00
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #16
    bne :-
@check_spr_pal:
    ; OBJ palette ($3F10, 16 bytes)
    lda sd_spr_pal_ptr
    ora sd_spr_pal_ptr+1
    beq @check_bg_chr
    lda sd_spr_pal_ptr
    sta map_ptr
    lda sd_spr_pal_ptr+1
    sta map_ptr+1
    lda #$3f
    sta $2006
    lda #$10
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #16
    bne :-
@check_bg_chr:
    ; BG terrain tiles (4 tiles = 64 bytes at terrain_tiles - glyphs).
    ; Restore default terrain_tiles when sd_bg_chr_ptr == 0.
    lda sd_bg_chr_ptr
    ora sd_bg_chr_ptr+1
    bne @custom_bg_chr
    lda #<terrain_tiles
    sta map_ptr
    lda #>terrain_tiles
    sta map_ptr+1
    jmp @upload_bg_chr
@custom_bg_chr:
    lda sd_bg_chr_ptr
    sta map_ptr
    lda sd_bg_chr_ptr+1
    sta map_ptr+1
@upload_bg_chr:
    lda #>(terrain_tiles - glyphs)
    sta $2006
    lda #<(terrain_tiles - glyphs)
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #64
    bne :-
    ; OBJ enemy/boss tiles (4 tiles = 64 bytes at $1040, 8x16 sprite tiles 5 and 7).
    ; Restore default sprite_patterns+64 when sd_spr_chr_ptr == 0.
    lda sd_spr_chr_ptr
    ora sd_spr_chr_ptr+1
    bne @custom_spr_chr
    lda #<(sprite_patterns + 64)
    sta map_ptr
    lda #>(sprite_patterns + 64)
    sta map_ptr+1
    jmp @upload_spr_chr
@custom_spr_chr:
    lda sd_spr_chr_ptr
    sta map_ptr
    lda sd_spr_chr_ptr+1
    sta map_ptr+1
@upload_spr_chr:
    lda #$10
    sta $2006
    lda #$40
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #64
    bne :-
@visuals_done:
    rts

; Compute 16-bit ap_cam_y = pan_y_page * 240 + pan_y.
ap_update_cam_y:
    lda pan_y
    sta ap_cam_y_lo
    lda #0
    sta ap_cam_y_hi
    ldx pan_y_page
    beq @cam_y_done
@cam_y_add:
    lda ap_cam_y_lo
    clc
    adc #240
    sta ap_cam_y_lo
    bcc :+
    inc ap_cam_y_hi
:
    dex
    bne @cam_y_add
@cam_y_done:
    rts

; Add signed 8-bit delta in A to 16-bit (ap_player_y_hi : player_feet).
ap_add_player_y:
    bmi @add_y_neg
    clc
    adc player_feet
    sta player_feet
    bcc :+
    inc ap_player_y_hi
:
    rts
@add_y_neg:
    clc
    adc player_feet
    sta player_feet
    bcs :+
    dec ap_player_y_hi
:
    rts

; Add A (1..25) in 100-point units to score and check 1UP extend threshold.
ap_add_score:
    sta ap_tmp3
    clc
    adc ap_score_units+0
    sta ap_score_units+0
    bcc :+
    inc ap_score_units+1
:
    ; Decimal digit add into ap_score[3] with multi-carry propagation
    lda ap_score+3
    clc
    adc ap_tmp3
    ldx #3
@score_carry:
    cmp #10
    bcc @score_store
    ldy #0
@score_div10:
    sec
    sbc #10
    iny
    cmp #10
    bcs @score_div10
    sta ap_score,x
    dex
    bmi @score_clamp
    tya
    clc
    adc ap_score,x
    jmp @score_carry
@score_clamp:
    lda #9
    sta ap_score+0
    sta ap_score+1
    sta ap_score+2
    sta ap_score+3
    jmp @score_extend_chk
@score_store:
    sta ap_score,x
@score_extend_chk:
    ; Check if ap_score_units >= ap_next_extend
    lda ap_score_units+1
    cmp ap_next_extend+1
    bcc @score_dirty
    bne @grant_extend
    lda ap_score_units+0
    cmp ap_next_extend+0
    bcc @score_dirty
@grant_extend:
    lda ap_next_extend+0
    clc
    adc #<ap_extend_step
    sta ap_next_extend+0
    lda ap_next_extend+1
    adc #>ap_extend_step
    sta ap_next_extend+1
    lda campaign_lives
    cmp #ap_max_lives
    bcs @score_dirty
    inc campaign_lives
    lda #snd_se_key
    jsr call_bank8_play_se
@score_dirty:
    inc campaign_hud_dirty
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
    lda ap_player_y_hi
    sta ap_prev_feet_hi

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
    jsr ap_update_rank
    jsr ap_update_scheduler
    jsr ap_update_enemies
    jsr ap_update_boss
    jsr ap_update_ebolts
    jsr ap_update_shots
    jsr ap_check_items
    jsr ap_check_enemies_contact
    jsr ap_check_hazard
    jsr tick_campaign_clock
    lda player_hit
    beq @check_goal
    lda #ap_armor_dead
    sta ap_armor
    lda #ap_state_dead
    sta ap_state
    lda ap_rank
    beq :+
    dec ap_rank
:
    dec campaign_lives
    bne @retry_hud
    jmp show_game_over
@retry_hud:
    inc campaign_hud_dirty
    jmp @camera_and_submit

@check_goal:
    lda sd_flags
    and #sd_flag_vertical
    bne @check_vert_progress
    jsr check_course_progress
    php
    lda sd_flags
    and #sd_flag_has_boss
    beq @non_boss_goal
    plp
    lda ap_key_collected
    beq @camera_and_submit
    jmp show_course_clear
@non_boss_goal:
    plp
    bcc @camera_and_submit
    jmp show_course_clear

@check_vert_progress:
    ; Vertical climbing stage: checkpoint when player_y <= sd_cp_y.
    lda checkpoint_reached
    bne @check_vert_goal
    lda ap_player_y_hi
    cmp sd_cp_y_hi
    bcc @vert_cp_hit
    bne @check_vert_goal
    lda player_feet
    cmp sd_cp_y_lo
    beq @vert_cp_hit
    bcs @check_vert_goal
@vert_cp_hit:
    lda #1
    sta checkpoint_reached
    inc campaign_hud_dirty
@check_vert_goal:
    lda sd_flags
    and #sd_flag_has_boss
    beq @camera_and_submit
    lda ap_key_collected
    beq @camera_and_submit
    jmp show_course_clear

@camera_and_submit:
    lda ap_boss_lock
    beq @unlock_camera
    jsr ap_update_cam_y
    jsr draw_sprites
    jmp submit_frame
@unlock_camera:
    jsr ap_follow_camera
    jmp submit_frame

; 16-bit X and Y camera follower for both horizontal and vertical arcade stages.
ap_follow_camera:
    ; Horizontal axis
    sec
    lda player_x
    sbc camera_low
    tay
    lda player_x+1
    sbc camera_high
    bmi @cam_left
    bne @cam_right
    cpy #192
    bcs @cam_right
    cpy #64
    bcc @cam_left
    lda #0
    beq @cam_x_ready
@cam_right:
    lda #1
    bne @cam_x_ready
@cam_left:
    lda #2
@cam_x_ready:
    sta pan_directions

    ; Vertical axis: follow 16-bit player_y toward screen Y = 160
    jsr ap_update_cam_y
    sec
    lda player_feet
    sbc ap_cam_y_lo
    tay
    lda ap_player_y_hi
    sbc ap_cam_y_hi
    bmi @cam_up
    bne @cam_down
    cpy #160
    beq @cam_y_ready
    bcc @cam_up
@cam_down:
    lda #$04
    bne @cam_apply_y
@cam_up:
    lda #$08
@cam_apply_y:
    ora pan_directions
    sta pan_directions
@cam_y_ready:
    jsr prepare_pan
    lda pan_x
    sta camera_low
    lda pan_x+1
    sta camera_high
    jsr ap_update_cam_y
    jmp draw_sprites

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
    lda plat_y,x
    bne :+
    dec plat_y_hi,x
:
    dec plat_y,x
    lda #$ff
    sta plat_dy,x
    jmp @advance_plat_timer
@v_down:
    inc plat_y,x
    bne :+
    inc plat_y_hi,x
:
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
    jsr ap_add_player_y
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
    jsr ap_add_player_y
    jsr ap_advance_jump_step
    jsr ap_check_support
    bcc @air_done
    lda ap_tile_row_top
    sta player_feet
    lda ap_tile_row_top_hi
    sta ap_player_y_hi
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
    lda #0
    sta ap_air_dir
@air_done:
    rts

@air_rising:
    jsr ap_add_player_y
    lda player_feet
    sec
    sbc #player_height
    sta ap_probe_y
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
    bmi @air_advance_rise
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
    lda ap_probe_y_hi
    adc #0
    sta ap_player_y_hi
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

; --- Ladder state: vertical climbing only (16-bit Y safe) ---
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
    lda #$ff
    jsr ap_add_player_y
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sec
    sbc #1
    sta ap_probe_y
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
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
    lda #1
    jsr ap_add_player_y
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sta ap_probe_y
    lda ap_player_y_hi
    sta ap_probe_y_hi
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
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
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
    lda ap_player_y_hi
    sta ap_probe_y_hi
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    bne @no_ladder_down
    jsr ap_snap_to_ladder
    lda #1
    jsr ap_add_player_y
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
    jsr ap_add_player_y
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
    lda ap_tile_row_top_hi
    sta ap_player_y_hi
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
    lda ap_tile_row_top_hi
    sta ap_player_y_hi
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
    lda sd_flags
    and #sd_flag_vertical
    bne @check_vert_right_edge
    lda player_x+1
    cmp stage_goal_x_hi
    bcc @do_inc_x
    bne @right_blocked
    lda player_x
    cmp stage_goal_x_lo
    bcs @right_blocked
    jmp @do_inc_x
@check_vert_right_edge:
    lda player_x
    cmp #240
    bcs @right_blocked
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
    lda ap_boss_lock
    beq @check_stage_left
    lda player_x+1
    cmp sd_boss_arena_l+1
    bcc @left_blocked
    bne @do_dec_x
    lda player_x
    cmp sd_boss_arena_l
    beq @left_blocked
    bcc @left_blocked
    bcs @do_dec_x
@check_stage_left:
    lda player_x+1
    bne @do_dec_x
    lda player_x
    cmp #9
    bcc @left_blocked
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
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
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
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
    jsr ap_tile_type_at
    cmp #tile_type_solid
    beq @wall_hit
    clc
    rts
@wall_hit:
    sec
    rts

; --- Support check: 16-bit dynamic platforms (+/-12 px), then 2D tiles ---
ap_check_support:
    lda #$ff
    sta ap_riding_plat
    ldx #0
@check_plat_loop:
    lda plat_type,x
    beq @next_plat_chk
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
    ; 16-bit vertical crossing: prev_feet <= plat_y and player_feet >= plat_y
    lda plat_y_hi,x
    cmp ap_prev_feet_hi
    bcc @next_plat_chk
    bne @chk_plat_bot
    lda plat_y,x
    cmp ap_prev_feet
    bcc @next_plat_chk
@chk_plat_bot:
    lda ap_player_y_hi
    cmp plat_y_hi,x
    bcc @next_plat_chk
    bne @landed_on_plat
    lda player_feet
    cmp plat_y,x
    bcc @next_plat_chk
@landed_on_plat:
    lda plat_y,x
    sta ap_tile_row_top
    lda plat_y_hi,x
    sta ap_tile_row_top_hi
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
    lda ap_player_y_hi
    sta ap_probe_y_hi
    sta ap_tile_row_top_hi
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
    lda ap_prev_feet_hi
    cmp ap_tile_row_top_hi
    bcc @foot_solid
    bne @foot_no
    lda ap_prev_feet
    cmp ap_tile_row_top
    beq @foot_solid
    bcc @foot_solid
@foot_no:
    clc
    rts
@foot_ladder_top:
    lda ap_prev_feet_hi
    cmp ap_tile_row_top_hi
    bcc @check_above_ladder
    bne @foot_no
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
    lda ap_probe_y_hi
    sbc #0
    sta ap_probe_y_hi
    jsr ap_tile_type_at
    ldx ap_tile_row_top
    stx ap_probe_y
    ldx ap_tile_row_top_hi
    stx ap_probe_y_hi
    cmp #tile_type_ladder
    beq @not_ladder_top
    sec
    rts
@not_ladder_top:
    clc
    rts

; --- Hazard / pit / bottom-of-camera fall check (fatal even with full armor) ---
ap_check_hazard:
    lda sd_flags
    and #sd_flag_vertical
    bne @vert_fall_check
    lda ap_player_y_hi
    bne @fatal_hazard
    lda player_feet
    cmp #232
    bcs @fatal_hazard
    jmp @tile_hazard_check
@vert_fall_check:
    ; Fatal if player falls below bottom of viewport (player_y - ap_cam_y >= 236).
    sec
    lda player_feet
    sbc ap_cam_y_lo
    tay
    lda ap_player_y_hi
    sbc ap_cam_y_hi
    bmi @tile_hazard_check
    bne @fatal_hazard
    cpy #236
    bcs @fatal_hazard
@tile_hazard_check:
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    lda player_feet
    sec
    sbc #1
    sta ap_probe_y
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
    jsr ap_tile_type_at
    cmp #tile_type_hazard
    beq @fatal_hazard
    rts
@fatal_hazard:
    lda #1
    sta player_hit
    rts

; --- Tile lookup helper: (ap_probe_x, ap_probe_y_hi:ap_probe_y) -> A = tile_type_* ---
ap_tile_type_at:
    lda ap_probe_y_hi
    asl a
    asl a
    asl a
    asl a
    sta ap_tmp0
    lda ap_probe_y
    lsr a
    lsr a
    lsr a
    lsr a
    ora ap_tmp0
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

; --- Multi-weapon 16-bit shot pool (Lance 2, Dagger 3, Torch 2 + flame, Shield 2) ---
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
    jsr ap_shot_check_enemies
    ldx ap_loop_idx
    dec ps_timer,x
    bne :+
    lda #0
    sta ps_active,x
:
    jmp @next_shot

@shot_flying:
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
    lda ps_kind,x
    cmp #ap_weapon_torch
    bne @shot_bounds
    lda ps_vy,x
    bmi @torch_vy_neg
    clc
    adc ps_y,x
    sta ps_y,x
    bcc @torch_grav
    inc ps_y_hi,x
    jmp @torch_grav
@torch_vy_neg:
    clc
    adc ps_y,x
    sta ps_y,x
    bcs @torch_grav
    dec ps_y_hi,x
@torch_grav:
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
    lda ps_x_hi,x
    cmp #3
    bcc :+
    jmp @reclaim_shot
:
    sec
    lda ps_x_lo,x
    sbc camera_low
    sta ap_tmp0
    lda ps_x_hi,x
    sbc camera_high
    beq :+
    jmp @reclaim_shot
:

    lda ps_x_lo,x
    sta ap_probe_x
    lda ps_x_hi,x
    sta ap_probe_x+1
    lda ps_y,x
    sta ap_probe_y
    lda ps_y_hi,x
    sta ap_probe_y_hi
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
    bcs :+
    dec ps_y_hi,x
:
    lda #2
    sta ps_active,x
    lda #ap_torch_flame_frames
    sta ps_timer,x
    jmp @next_shot

@shot_check_tombstone:
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
    ldx ap_weapon
    lda ap_weapon_max_shots,x
    sta ap_tmp0
    lda #0
    sta ap_tmp1
    lda #$ff
    sta ap_tmp2
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
    lda ap_player_y_hi
    sbc #0
    sta ps_y_hi,x
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
    ; 16-bit Y: plat_y <= ps_y <= plat_y + 16
    sec
    lda ps_y,x
    sbc plat_y,y
    sta ap_tmp0
    lda ps_y_hi,x
    sbc plat_y_hi,y
    bne @next_tomb
    lda ap_tmp0
    cmp #17
    bcs @next_tomb
    sec
    rts
@next_tomb:
    iny
    cpy #ap_max_plats
    bne @tomb_loop
    clc
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

; Carry set if shot slot ap_loop_idx hit an enemy or boss.
ap_shot_check_enemies:
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

; --- Item / weapon drop pickup (16-bit X and Y safe) ---
ap_check_items:
    ldx #0
@item_loop:
    lda item_type,x
    bne :+
    jmp @next_item
:
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
    beq :+
    jmp @next_item
:
    lda ap_tmp0
    cmp #21
    bcc :+
    jmp @next_item
:
    ; 16-bit |player_y - item_y| <= 10
    sec
    lda player_feet
    sbc item_y,x
    sta ap_tmp0
    lda ap_player_y_hi
    sbc item_y_hi,x
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #10
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    beq :+
    jmp @next_item
:
    lda ap_tmp0
    cmp #21
    bcc :+
    jmp @next_item
:

    lda item_type,x
    cmp #item_key
    bne @check_armor_pickup
    lda #1
    sta ap_key_collected
    txa
    pha
    lda #ap_score_key
    jsr ap_add_score
    pla
    tax
    jmp @consume_item
@check_armor_pickup:
    cmp #item_armor
    bne @check_weapon_drop
    lda #ap_armor_full
    sta ap_armor
    cpx #0
    bne @score_item_pick
    lda #0
    sta ap_armor_item_active
    beq @score_item_pick
@check_weapon_drop:
    cmp #item_dagger
    bne @check_torch_drop
    lda #ap_weapon_dagger
    jmp @set_weapon_hud
@check_torch_drop:
    cmp #item_torch
    bne @check_shield_drop
    lda #ap_weapon_torch
    jmp @set_weapon_hud
@check_shield_drop:
    cmp #item_shield
    bne @set_lance_drop
    lda #ap_weapon_shield
    jmp @set_weapon_hud
@set_lance_drop:
    lda #ap_weapon_lance
@set_weapon_hud:
    sta ap_weapon
    inc campaign_hud_dirty
@score_item_pick:
    txa
    pha
    lda #ap_score_item
    jsr ap_add_score
    pla
    tax
@consume_item:
    lda #item_none
    sta item_type,x
    txa
    pha
    lda #snd_se_key
    jsr call_bank8_play_se
    pla
    tax
@next_item:
    inx
    cpx #ap_max_items
    beq :+
    jmp @item_loop
:
    rts

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
    bcs ap_apply_player_damage
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
    bcc ap_apply_player_damage
@contact_none:
    rts

ap_apply_player_damage:
    lda #snd_se_damage
    jsr call_bank8_play_se
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
    ; 16-bit Y: probe_y - 15 <= player_y < probe_y + (crouch ? 10 : 16)
    sec
    lda player_feet
    sbc ap_probe_y
    sta ap_tmp0
    lda ap_player_y_hi
    sbc ap_probe_y_hi
    sta ap_tmp1
    clc
    lda ap_tmp0
    adc #15
    sta ap_tmp0
    lda ap_tmp1
    adc #0
    bne @miss
    lda ap_tmp0
    ldx ap_crouch
    beq @stand_c
    cmp #25
    bcs @miss
    sec
    rts
@stand_c:
    cmp #31
    bcs @miss
    sec
    rts
@miss:
    clc
    rts

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
ap_draw_sprites:
    lda #$f8
    ldx #30 * 4
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
    ldy #player_height + 1
    lda ap_crouch
    beq :+
    ldy #player_height - 3
:
    lda player_feet
    ldx ap_player_y_hi
    jsr ap_set_sprite_y_off
    lda #1
    sta draw_tile
    lda player_x
    ldx player_x+1
    jsr draw_pair

@draw_enemies:
    ; Slots 2-3: Fixed enemy 1
    lda ap_enemy_active
    beq @draw_pot_enemy
    lda #2 * 4
    sta draw_slot
    lda #1
    sta draw_palette
    ldy #player_height + 1
    lda sd_enemy1_y
    ldx sd_enemy1_y+1
    jsr ap_set_sprite_y_off
    lda #5
    sta draw_tile
    lda sd_enemy1_x
    ldx sd_enemy1_x+1
    jsr draw_pair
@draw_pot_enemy:
    ; Slots 9-10: Pot-carrying enemy
    lda ap_pot_enemy_active
    beq @draw_shots
    lda #9 * 4
    sta draw_slot
    lda #3
    sta draw_palette
    ldy #player_height + 1
    lda sd_pot_y
    ldx sd_pot_y+1
    jsr ap_set_sprite_y_off
    lda #5
    sta draw_tile
    lda sd_pot_x
    ldx sd_pot_x+1
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
    lda #2
:
    sta draw_palette
    lda #9
    cpx #2
    bne :+
    lda #15
:
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

    ; Slots 7-8: High-priority enemy bolts
    lda #7 * 4
    sta draw_slot
    ldy #0
@draw_ebolt_loop:
    lda eb_active,y
    beq @skip_ebolt_draw
    sty ap_loop_idx
    lda #2
    sta draw_palette
    lda #15
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
    jmp @next_ebolt_draw
@skip_ebolt_draw:
    lda draw_slot
    clc
    adc #4
    sta draw_slot
@next_ebolt_draw:
    iny
    cpy #ap_max_ebolts
    bne @draw_ebolt_loop

    ; Slots 11-18: Generic enemy pool
    lda #11 * 4
    sta draw_slot
    ldy #0
@draw_pool_en_loop:
    lda en_type,y
    beq @next_pool_en_draw
    sty ap_loop_idx
    lda #1
    cmp en_type,y
    beq @zombie_pal
    lda #3
    ldx en_timer,y
    cpx #ap_turret_windup
    bcs @set_en_pal
    lda #2
    bne @set_en_pal
@zombie_pal:
    lda #0
@set_en_pal:
    sta draw_palette
    lda #5
    ldx en_type,y
    cpx #en_turret
    bne :+
    lda #11
:
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
    bne @draw_pool_en_loop

    ; Slots 11-14 reused for 16x32 multi-part boss during boss battle
    lda ap_boss_state
    cmp #boss_active
    bne @draw_plats
    jsr ap_draw_boss

@draw_plats:
    ; Slots 19-26: Dynamic platforms (up to 4 x 2 sprites)
    lda #19 * 4
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
    bne @draw_plat_loop

    ; Slots 27-30: Items / weapon drops / key (up to 4 sprites)
    lda #27 * 4
    sta draw_slot
    ldy #0
@draw_item_loop:
    lda item_type,y
    beq @next_item_draw
    sty ap_loop_idx
    lda #3
    ldx item_type,y
    cpx #item_key
    bne :+
    lda #2
:
    sta draw_palette
    lda #15
    sta draw_tile
    lda item_y,y
    ldx item_y_hi,y
    ldy #9
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

ap_weapon_max_shots: .byte 2, 3, 2, 2
ap_weapon_vx_pos:    .byte 3, 4, 2, 3
ap_weapon_vx_neg:    .byte $fd, $fc, $fe, $fd
ap_weapon_vy_init:   .byte 0, 0, $fc, 0
ap_weapon_life:      .byte 48, 48, 56, 40

ap_rank_zombie_cap:  .byte 2, 2, 3, 3
ap_rank_spawn_wait:  .byte 48, 36, 28, 20

ap_kb_delta:
    .byte $fe, $fe, $ff, $ff, $ff, $00, $00, $00
    .byte $01, $01, $01, $02, $02, $02, $03, $03

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

; Return active scheduler page axis in A (camera_high for horizontal, pan_y_page for vertical).
ap_current_sched_page:
    lda sd_flags
    and #sd_flag_vertical
    bne @use_vert_page
    lda camera_high
    rts
@use_vert_page:
    lda pan_y_page
    rts

; --- Data-driven scroll-linked enemy scheduler & fixed turret spawner ---
ap_update_scheduler:
    lda sd_sched_page
    cmp #$ff
    beq @sched_early_exit
    lda ap_boss_state
    beq @sched_run
@sched_early_exit:
    rts
@sched_run:
    jsr ap_current_sched_page
    cmp sched_page
    beq @check_group_spawns
    sta sched_page
    cmp sd_sched_page
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
    jsr ap_current_sched_page
    cmp sd_sched_page
    bne @check_zombie_sched
    lda sd_turret_x
    ora sd_turret_x+1
    beq @check_zombie_sched
    lda ap_turret_spawned
    bne @check_zombie_sched
    jsr ap_alloc_enemy_slot
    bcc @check_zombie_sched
    lda #en_turret
    sta en_type,x
    lda sd_turret_x
    sta en_x_lo,x
    lda sd_turret_x+1
    sta en_x_hi,x
    lda sd_turret_y
    sta en_y,x
    lda sd_turret_y+1
    sta en_y_hi,x
    lda #0
    sta en_dir,x
    lda #ap_turret_cooldown
    sta en_timer,x
    lda #2
    sta en_hp,x
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
    lda sd_zombie_x
    sta en_x_lo,x
    lda sd_zombie_x+1
    sta en_x_hi,x
    lda sd_zombie_y
    sta en_y,x
    lda sd_zombie_y+1
    sta en_y_hi,x
    lda #0
    sta en_dir,x
    sta en_timer,x
    lda #1
    sta en_hp,x
    inc sched_zombies_live
    ldy ap_rank
    lda ap_rank_spawn_wait,y
    sta sched_wait
@sched_done:
    rts

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

; Draw 16x32 multi-part boss in OAM slots 11-14 (4 sprites):
; Head 2 + Body 2; max scanline = player(2) + boss(2) + 3 daggers(3) + bolt(1) = 8.
ap_draw_boss:
    lda #11 * 4
    sta draw_slot
    lda #1
    ldx ap_boss_flash
    beq :+
    lda #2
:
    sta draw_palette
    lda #5
    sta draw_tile
    ldy #33
    lda ap_boss_y
    ldx ap_boss_y_hi
    jsr ap_set_sprite_y_off
    lda ap_boss_x_lo
    ldx ap_boss_x_hi
    jsr draw_pair

    lda #0
    ldx ap_boss_shield_hp
    beq :+
    lda #3
:
    ldx ap_boss_flash
    beq :+
    lda #2
:
    sta draw_palette
    lda #11
    sta draw_tile
    ldy #17
    lda ap_boss_y
    ldx ap_boss_y_hi
    jsr ap_set_sprite_y_off
    lda ap_boss_x_lo
    ldx ap_boss_x_hi
    jmp draw_pair
