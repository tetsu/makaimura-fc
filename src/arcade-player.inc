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
ap_weapon_shield = 3             ; the arcade's cross in the game build
ap_weapon_axe = 4                ; game build only

ap_max_shots = 3
ap_max_plats = 4
ap_max_items = 4
ap_max_enemies = 4
ap_max_ebolts = 2

; OAM slots 0..3 hold the player. Slots 4..12 are the priority block: three
; player shots, two enemy bolts and the boss's two pairs, drawn in that order
; from a start slot that moves by one each frame and wraps inside the block, so
; each of them is the last one in turn when a line holds more than eight.
; Each rotating group after the block reserves two adjacent slots for a pair.
; docs/revision-g-sprites.md (the FC thinning rule) explains the order.
ap_prio_first = 4
ap_prio_shots = 0
ap_prio_ebolts = ap_prio_shots + ap_max_shots
ap_prio_boss = ap_prio_ebolts + ap_max_ebolts
ap_prio_count = ap_prio_boss + 4
ap_prio_end = ap_prio_first + ap_prio_count
ap_oam_first = ap_prio_end
ap_oam_fixed_enemy = 0
ap_oam_pot_enemy = ap_oam_fixed_enemy + 2
ap_oam_enemies = ap_oam_pot_enemy + 2
ap_oam_platforms = ap_oam_enemies + ap_max_enemies * 2
ap_oam_items = ap_oam_platforms + ap_max_plats
ap_oam_groups = ap_oam_items + ap_max_items
ap_oam_end = ap_oam_first + ap_oam_groups * 2
.assert ap_oam_end < 64, error, "Arcade OAM clear loop requires fewer than 64 slots"
.assert ap_oam_groups .mod 7 <> 0, error, "The rotation step 7 must be coprime with the group count"

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
boss_head_tile = $77
boss_head_chr_address = chr_sprite_address + (boss_head_tile & $fe) * chr_tile_bytes

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

.segment "ZEROPAGE"
ap_sched_ptr: .res 2

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
en_sched_page: .res ap_max_enemies ; $6089 (turret owner page, $ff for legacy)
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
ap_oam_phase: .res 1
sched_turrets: .res sd_sched_pages
ap_prio_phase: .res 1           ; start of the priority block, 0..ap_prio_count-1
; Walking and jumping follow the arcade player record ($0500-$052F, see
; docs/revision-g-player.md). Fractions keep the arcade's sense: ap_y_frac is the fraction
; of the arcade height (up is positive), so player_feet = constant - arcade integer Y.
ap_x_frac: .res 1               ; arcade $0508
ap_y_frac: .res 1               ; arcade $050B
ap_vx: .res 3                   ; arcade $0517-$0519: integer, fraction high, fraction low
ap_vy: .res 3                   ; arcade $051B-$051D: up while rising, down while falling
ap_move_dir: .res 1             ; arcade $050E: 0 none, 1 right, 2 left
ap_jump_phase: .res 1           ; arcade $0504 in the jump: 0 start, 1 rise, 2 fall
ap_steps: .res 1
; The arcade acts on the controller one update after reading it ($6772 fills $46 for the
; next play update), so the player reads the pad of the previous frame.
ap_pad_next: .res 1
ap_pad: .res 1                  ; arcade $004B
ap_pressed: .res 1              ; arcade $004B & ~$004C
; The shot follows the arcade too (game build): ap_shoot_lock is $052A, the pose timer.
ap_shot_phase: .res 1           ; arcade $0505: 0 until the pose has been set up
ps_x_frac: .res ap_max_shots    ; kept when a slot is reused, as in the arcade
ps_start_lo: .res ap_max_shots  ; X where the shot was thrown, for its range
ps_start_hi: .res ap_max_shots
; The crouch follows the arcade too (game build): ap_crouch is $051F.
ap_stick: .res 1                ; arcade $0525: 3 after up, 4 after down, 0 neither
ap_crouch_wait: .res 1          ; arcade $0520: updates before standing up
ap_tomb_above: .res 1           ; arcade $052C: the support probe needs the player on top
ap_ladder_anim: .res 1          ; arcade $0527: getting on from above, getting off at the top
ap_kb_dying: .res 1             ; state 3 is the death (arcade $AC was 1 at the hit)
; Test hook for tools/compare-player-trace.py: 1 or 2 hits the player after the next
; player update, flying right or left, as if an enemy had touched him.
ap_test_hit: .res 1
ap_top_score: .res 4            ; 4 decimal digits (d3..d0, x100 pts; arcade default 010000)
.ifdef REVG_STAGE1
ap_clear_seq: .res 1            ; 0 playing, 1 key hold, 2 time bonus tally, 3 final hold
ap_clear_timer: .res 1
ap_clear_key_slot: .res 1       ; ob_* slot holding the raised key sprite
ap_clear_key_y: .res 1          ; base arcade Y of the raised key
ap_tomb_hits: .res revg_tomb_count ; hits remaining per tombstone (15 -> Magician spawns)
ap_mag_req: .res 1              ; 1 when a tombstone reached 15 hits
ap_mag_x: .res 2
ap_mag_y: .res 1                ; arcade Y (tombstone Y + 16)
ap_frog_timer: .res 2           ; 360..1 while transformed into a frog, 0 otherwise
.endif

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
    lda ap_top_score+0
    ora ap_top_score+1
    ora ap_top_score+2
    ora ap_top_score+3
    bne :+
    lda #1                      ; arcade default TOP SCORE = 010000
    sta ap_top_score+1
:
    lda #0
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
.ifndef REVG_STAGE1
    lda campaign_stage
    cmp #arcade_stage_count
    beq @keep_stage7_weapon
    lda #ap_weapon_lance
    sta ap_weapon
@keep_stage7_weapon:
.endif
    ; (The game build keeps the weapon through a death, as the arcade does; a new game
    ; starts with the lance, start_arcade_stage.)
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_oam_phase
    sta ap_prio_phase
    sta ap_invuln
    sta ap_crouch
    sta ap_shoot_lock
    sta ap_air_dir
    sta ap_move_dir
    sta ap_jump_phase
    sta ap_pad
    sta ap_pad_next
    sta ap_pressed
    sta ap_x_frac
    sta ap_y_frac
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
    jsr ap_reset_velocity

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
    sta ap_shot_phase
    sta ap_stick
    lda #10                     ; $8F52
    sta ap_crouch_wait
    lda #0
    sta ap_tomb_above
    sta ap_ladder_anim
    sta ap_kb_dying
    sta ap_test_hit
.ifdef REVG_STAGE1
    sta ap_clear_seq
.endif
:
    sta ps_active,x
    sta ps_y_hi,x
    sta ps_x_frac,x
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
    lda sd_flags
    and #sd_flag_sched_table
    beq @init_legacy_sched_page
    lda sd_sched_table_ptr
    sta ap_sched_ptr
    lda sd_sched_table_ptr+1
    sta ap_sched_ptr+1
    lda #$ff                       ; Allow the starting page to activate on the first tick.
    bne @store_sched_page
@init_legacy_sched_page:
    lda #<sd_base
    sta ap_sched_ptr
    lda #>sd_base
    sta ap_sched_ptr+1
    jsr ap_current_sched_page
@store_sched_page:
    sta sched_page
    lda #0
    sta sched_group
    sta sched_wait
    sta sched_zombies_live
    sta ap_turret_spawned
    ldx #sd_sched_pages - 1
:
    sta sched_turrets,x
    dex
    bpl :-
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
.ifdef REVG_STAGE1
    lda demo_active
    bne @keep_song              ; the opening starts its own music
    lda op_stage_song
    beq :+
    lda #0                      ; the opening already started the stage song
    sta op_stage_song
@keep_song:
    rts
:
.endif
    lda sd_bgm_stage
    jmp call_bank8_play_song

; Load per-stage BG/OBJ palettes and optional 64-byte CHR overrides (blanked PPU).
ap_load_stage_visuals:
    lda #$ff
    sta map_cached_col
    ; Upload authentic Makaimura sprite CHR (2048 B -> $1000) and Stage 1 BG CHR (704 B -> $0400) from BANK10.
    lda prg_bank0
    pha
    lda #arcade_gfx_bank
    jsr select_bank0
    lda #>chr_sprite_address
    sta $2006
    lda #<chr_sprite_address
    sta $2006
    lda #<arcade_spr_chr_data
    sta chr_source
    lda #>arcade_spr_chr_data
    sta chr_source+1
    ldx #>arcade_spr_chr_bytes
    lda #<arcade_spr_chr_bytes
    jsr upload_chr_patterns
.ifdef REVG_STAGE1
    ; The real Stage 1/2 stream their own background tiles into $25-$FF.
    lda revg_bg_active
    beq @placeholder_bg
    pla
    jsr select_bank0
    ldx #1
    jsr call_bank50_bg
    jmp @check_spr_pal
@placeholder_bg:
.endif

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
.ifdef REVG_STAGE1
    lda revg_bg_active
    bne @check_spr_chr
.endif
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
    ; Four CHR tiles replace the two 8x16 sprites in the boss's upper half.
    ; A zero pointer keeps the default patterns loaded from BANK10.
@check_spr_chr:
    lda sd_spr_chr_ptr
    ora sd_spr_chr_ptr+1
    beq @visuals_done
    lda sd_spr_chr_ptr
    sta map_ptr
    lda sd_spr_chr_ptr+1
    sta map_ptr+1
@upload_spr_chr:
    lda #>boss_head_chr_address
    sta $2006
    lda #<boss_head_chr_address
    sta $2006
    ldy #0
:
    lda (map_ptr),y
    sta $2007
    iny
    cpy #64
    bne :-
@visuals_done:
.ifdef REVG_STAGE1
    ; The arcade sprites: an empty tile cache and their palettes.
    jsr rs_init
    jsr op_start                ; also uploads ':' colon tile in BANK7
    lda #$3f
    sta $2006
    lda #$10
    sta $2006
    ldx #0
:
    lda revg_spr_palettes,x
    sta $2007
    inx
    cpx #16
    bne :-
    lda prg_bank2
    pha
    jsr ap_select_revg_enemy_bank
    jsr revg_enemy_init_vec
    pla
    jsr select_bank2
.endif
    rts

.ifdef REVG_STAGE1
ap_select_revg_enemy_bank:
    lda #6
    ldx campaign_stage
    cpx #2
    bne :+
    lda #49
    bne @sel
:
    cpx #3
    beq @b51
    cpx #4
    beq @b51
    cpx #5
    bcc @sel
    lda #56
    bne @sel
@b51:
    lda #51
@sel:
    jmp select_bank2
.endif

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
    ; Update TOP SCORE if 1UP score exceeds it.
    ldx #0
@top_cmp:
    lda ap_score,x
    cmp ap_top_score,x
    bcc @top_done
    bne @top_copy
    inx
    cpx #4
    bne @top_cmp
    beq @top_done
@top_copy:
    ldx #3
:
    lda ap_score,x
    sta ap_top_score,x
    dex
    bpl :-
@top_done:
    inc campaign_hud_dirty
    rts

; Map Bank 4 (enemies, enemy bolts, boss) at $C000. The audio tick maps Bank 8 there each frame.
ap_map_enemy_bank:
    lda #4
    jmp select_bank2

ap_update_frame:
.ifdef REVG_STAGE1
    lda demo_active
    beq :+
    jsr op_tick                 ; the opening demo: no play, only its frames
    jsr draw_sprites
    jmp submit_frame
:
    lda ap_clear_seq
    beq :+
    jmp call_bank6_clear_seq
:
.endif
    lda ap_pad
    eor #$ff
    sta ap_tmp0
    lda ap_pad_next
    sta ap_pad
    and ap_tmp0
    sta ap_pressed
    lda pad
    sta ap_pad_next
    lda player_hit
    beq @alive
    jmp @camera_and_submit
@alive:
.ifdef REVG_STAGE1
    ; Frog transformation countdown (360 frames): walk + jump only (no fire/crouch/ladder).
    lda ap_frog_timer
    ora ap_frog_timer+1
    beq @not_frog
    lda ap_frog_timer
    bne :+
    dec ap_frog_timer+1
:
    dec ap_frog_timer
    lda ap_frog_timer
    ora ap_frog_timer+1
    bne @frog_mask
    inc campaign_hud_dirty      ; restore HUD weapon icon when frog ends
    lda #snd_se_pickup
    jsr call_bank8_play_se
    jmp @not_frog
@frog_mask:
    lda ap_pad
    and #$b3                    ; mask out B ($40), Up ($08), Down ($04)
    sta ap_pad
    lda ap_pressed
    and #$b3
    sta ap_pressed
    lda #0
    sta ap_crouch
@not_frog:
.else
    lda ap_invuln
    beq :+
    dec ap_invuln
:
    lda ap_shoot_lock
    beq :+
    dec ap_shoot_lock
:
.endif
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
.ifdef REVG_STAGE1
    ; $EE63 counts the blink timer down after the player update, just before the hit
    ; checks, so the update that sets 120 ends at 119.
    lda ap_invuln
    beq :+
    dec ap_invuln
:
    ldx ap_test_hit             ; 1 or 2: a hit flying that way, 3: the time-out,
    beq @no_test_hit            ; 4: the time-out after the checkpoint
    lda #0
    sta ap_test_hit
    cpx #3
    bcs :+
    jsr ap_hit_toward
    jmp @no_test_hit
:
    beq :+
    lda #1                      ; the restart is at the checkpoint (arcade $0081)
    sta checkpoint_reached
:
    jsr ap_time_up
@no_test_hit:
.endif
.ifdef REVG_STAGE1
    ; The enemies, items and the shot checks of the game build are in Bank 6 (Stage 1)
    ; or Bank 49 (Stage 2).
    jsr ap_select_revg_enemy_bank
    jsr revg_enemy_update_vec
    jsr ap_update_shots
    jsr ap_check_hazard
.else
    jsr ap_map_enemy_bank      ; The enemy routines below and the shot checks are in Bank 4.
    jsr ap_update_rank
    jsr ap_update_scheduler
    jsr ap_update_enemies
    jsr ap_update_boss
    jsr ap_update_ebolts
    jsr ap_update_shots
    jsr ap_check_items
    jsr ap_check_enemies_contact
    jsr ap_check_hazard
.endif
.ifdef REVG_STAGE1
    ; The clock stops during the death, so it stays at 000 after a time-out. At 000 the
    ; arcade ($EC81) starts the death from lying down instead of a miss.
    lda player_hit
    bne @clock_done
    lda ap_state
    cmp #ap_state_knockback
    bne :+
    lda ap_kb_dying
    bne @clock_done
:
    jsr tick_campaign_clock
    lda player_hit
    beq @clock_done
    lda #0
    sta player_hit
    jsr ap_time_up
@clock_done:
.else
    jsr tick_campaign_clock
.endif
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
.ifdef REVG_STAGE1
    lda ap_clear_seq
    bne @camera_and_submit
.endif
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
    lda sd_flags
    and #sd_flag_boss_follow
    beq @locked_camera
    lda #1                     ; Scroll on to the end of the stage, then stay there.
    jsr ap_pan_camera
    jmp submit_frame
@locked_camera:
    jsr ap_update_cam_y
    jsr draw_sprites
    jmp submit_frame
@unlock_camera:
    jsr ap_follow_camera
    jmp submit_frame

; 16-bit X and Y camera follower for both horizontal and vertical arcade stages.
.ifdef REVG_STAGE1
; Arcade $8001: the camera keeps the player at X - camera X = 128. Right of that it
; catches up unless it has already reached the right limit (checked before the move, as
; at $8015); left of that it follows back until camera X 0 ($8291, $82AE).
ap_cam_max_step = 4
ap_follow_camera:
    sec
    lda player_x
    sbc camera_low
    tay
    lda player_x+1
    sbc camera_high
    bmi @cam_left_far
    bne @cam_right_far
    cpy #128
    beq @cam_none
    bcc @cam_left
    lda camera_high
    cmp stage_cam_x_max_hi
    bcs @cam_none
    tya
    sec
    sbc #128
    ldx #1
    bne @cam_steps
@cam_right_far:
    lda camera_high
    cmp stage_cam_x_max_hi
    bcs @cam_none
    lda #ap_cam_max_step
    ldx #1
    bne @cam_steps
@cam_left:
    lda camera_low
    ora camera_high
    beq @cam_none
    sty ap_steps
    lda #128
    sec
    sbc ap_steps
    ldx #2
    bne @cam_steps
@cam_left_far:
    lda camera_low
    ora camera_high
    beq @cam_none
    lda #ap_cam_max_step
    ldx #2
@cam_steps:
    ; A = pixels (1 or more), X = direction. pan_directions takes the extra pixels in bits 6-4.
    cmp #ap_cam_max_step
    bcc :+
    lda #ap_cam_max_step
:
    sec
    sbc #1
    asl a
    asl a
    asl a
    asl a
    stx ap_steps
    ora ap_steps
    jmp ap_pan_camera
@cam_none:
    lda #0
    beq ap_pan_camera

; Arcade $9283, $92A9 (walk) and $93F0, $940F (jump): room between the player and the
; screen edge toward A (1 right: camera X + 256 - X, 2 left: X - camera X).
; Returns C clear when the room is under 10 px and Z set when it is exactly 10 px.
ap_edge_room:
    cmp #1
    bne @room_left
    lda camera_low
    sec
    sbc player_x
    tay
    lda camera_high
    sbc player_x+1
    clc
    adc #1
    jmp @room_check
@room_left:
    lda player_x
    sec
    sbc camera_low
    tay
    lda player_x+1
    sbc camera_high
@room_check:
    bne @room_far
    cpy #10
    rts
@room_far:
    sec
    lda #1
    rts
.else
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
    beq ap_pan_camera
@cam_right:
    lda #1
    bne ap_pan_camera
@cam_left:
    lda #2
.endif
; A = horizontal pan direction (0 none, 1 right, 2 left). The vertical axis follows the player.
ap_pan_camera:
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
.ifdef REVG_STAGE1
    jsr prepare_pan_steps
    ldx #2
    jsr call_bank50_bg
.else
    jsr prepare_pan
.endif
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
.ifdef REVG_STAGE1
    jmp ap_step_ground_revg
.endif
    lda ap_pad
    and #$08                    ; Up
    beq @check_ladder_down
    jsr ap_try_enter_ladder_up
    bcc @check_ladder_down
    rts
@check_ladder_down:
    lda ap_pad
    and #$04                    ; Down
    beq @not_crouching
    jsr ap_try_enter_ladder_down
    bcc @enter_crouch
    rts
@enter_crouch:
    lda #1
    sta ap_crouch
    jmp @check_ground_support
@not_crouching:
    lda #0
    sta ap_crouch
    jsr ap_update_move_dir
    bcs @check_ground_support
    lda ap_shoot_lock
    bne @check_ground_support
    jsr ap_walk_step

@check_ground_support:
    jsr ap_check_support
    bcs @check_ground_jump
    jmp ap_begin_fall
@check_ground_jump:
    lda ap_crouch
    bne @ground_done
    lda ap_pressed
    and #$80
    beq @ground_done
    ; The jump keeps the walking direction. A direction pressed with the jump in the
    ; same update has not been held for two updates yet, so that jump goes straight up.
    lda ap_move_dir
    sta ap_air_dir
    lda #$ff
    sta ap_riding_plat
    lda #ap_state_air
    sta ap_state
    lda #0
    sta ap_jump_phase
@ground_done:
    rts

.ifdef REVG_STAGE1
; Arcade $8FDF standing: up and down ($908B), shot ($8F71), direction ($9233), walk
; ($9265), ground ($9034), jump ($907B). Crouching ($9017): up and down, getting up
; ($98AB), jump, shot, ground. Crouching does not walk, and its jump goes straight up.
; The ladder is a flag of the ground state in the arcade ($0526); the FC keeps it as
; ap_state_ladder and runs this same update for it. On the ladder the stick climbs
; ($91CF) and nothing else happens: no walk, no jump, no shot.
ap_step_ground_revg:
    lda ap_crouch
    bne @crouching
    jsr ap_revg_stick
    bcc @stick_done
    lda ap_state
    cmp #ap_state_ladder
    beq @done
@stick_done:
    lda ap_state
    cmp #ap_state_ladder
    bne :+
    jmp ap_ladder_move
:
    jsr ap_shot_pose
    jsr ap_update_move_dir
    bcs @support
    lda ap_shoot_lock
    bne @support
    jsr ap_walk_step
@support:
    jsr ap_revg_ground
    bcs :+
    jmp ap_begin_fall
:
    jmp ap_revg_jump_check
@crouching:
    lda #0
    sta ap_move_dir
    jsr ap_revg_stick
    bcs @release
    lda ap_state
    cmp #ap_state_ladder
    bne @crouch_on
    lda #0                      ; $9023
    sta ap_shot_phase
    sta ap_crouch
    jmp ap_ladder_move
@release:
    jsr ap_crouch_release       ; $902A
@crouch_on:
    jsr ap_revg_jump_check
    jsr ap_shot_pose
    lda ap_state
    cmp #ap_state_air
    beq @done
    jsr ap_revg_ground
    bcs @done
    jmp ap_begin_fall
@done:
    rts

; Arcade $9034: C set when the player stands on something, and then the feet go onto its
; surface ($904F).
ap_revg_ground:
    jsr ap_check_support
    bcc :+
    lda ap_tile_row_top
    sta player_feet
    lda ap_tile_row_top_hi
    sta ap_player_y_hi
    sec
:
    rts

; Arcade $908B: up or down held in this update and the previous one, up first. Returns C
; as the arcade does: set when neither is held, while a ladder is being entered or left,
; and on entering a ladder with up; clear otherwise.
; - Up ($909E): a ladder tile at the player's height ($9174) puts the player on it, once
;   per press (ap_stick 3). Without one, on the ladder the top exit counts ($90D2);
;   off it, up stands up from a crouch ($90BC).
; - Down ($9133): a ladder tile 18 px lower ($91AD) puts a player who is not on a ladder
;   onto it 14 px lower and starts the climb-down ($9101). Without one, the player is off
;   the ladder and crouches, once per press (ap_stick 4) ($9157).
ap_revg_stick:
    jsr ap_held_buttons
    and #$08
    beq @not_up
    lda #<-15                   ; the tile at the player's height
    jsr ap_ladder_at
    bcc @up_no_ladder
    lda #3                      ; $90A3
    cmp ap_stick
    beq @clear
    sta ap_stick
    lda #ap_state_ladder
    sta ap_state
    lda #0
    sta ap_shoot_lock
    sta ap_move_dir
    sec
    rts
@up_no_ladder:
    lda ap_state
    cmp #ap_state_ladder
    beq @top_exit
    lda #3                      ; $90BC
    cmp ap_stick
    beq @clear
    sta ap_stick
    lda #0
    sta ap_crouch
@clear:
    clc
    rts
@top_exit:
    ; $90D2: 17 updates in place, then off the ladder.
    lda ap_ladder_anim
    cmp #$11
    beq :+
    inc ap_ladder_anim
    sec
    rts
:
    lda #0
    sta ap_ladder_anim
    lda #ap_state_ground
    sta ap_state
    sec
    rts
@not_up:
    jsr ap_held_buttons
    and #$04
    beq @neither
    lda #3                      ; the tile 18 px lower
    jsr ap_ladder_at
    bcc @down_no_ladder
    lda ap_stick                ; $9138
    cmp #4
    beq @on_ladder
    lda ap_state
    cmp #ap_state_ladder
    beq @on_ladder
    lda #4
    sta ap_stick
    lda #14
    jsr ap_add_player_y
    lda #ap_state_ladder
    sta ap_state
    lda #0
    sta ap_move_dir
    beq @climb_down_anim
@on_ladder:
    lda ap_ladder_anim          ; $916D
    bne @climb_down_anim
    clc
    rts
@climb_down_anim:
    ; $9101: 17 updates from the first, then the climb.
    lda ap_ladder_anim
    bne :+
    lda #$11
    sta ap_ladder_anim
:
    dec ap_ladder_anim
    beq @clear
    sec
    rts
@down_no_ladder:
    lda ap_state                ; $9157
    cmp #ap_state_ladder
    bne :+
    lda #ap_state_ground
    sta ap_state
:
    lda #4
    cmp ap_stick
    beq @clear
    sta ap_stick
    lda #1
    sta ap_crouch
    clc
    rts
@neither:
    sta ap_stick
    sec
    rts

; Arcade $9174/$91AD: C set when the map cell at player_feet + A (signed) is a ladder,
; and then X moves onto the ladder's centre line, the edge it shares with the other ladder
; column. The arcade tile at height Y is FC row (255 - Y) / 16 = (feet + 1) / 16, and the
; converter puts each ladder cell one row higher, so the arcade's own height is A = -15
; and 18 px lower is A = +3.
ap_ladder_at:
    tax
    clc
    adc player_feet
    sta ap_probe_y
    txa
    and #$80
    beq :+
    lda #$ff
:
    adc ap_player_y_hi
    sta ap_probe_y_hi
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    beq :+
    clc
    rts
:
    ; Snap to the left edge of this column, or to the right edge when the ladder
    ; continues in the next column.
    lda player_x
    and #$f0
    sta player_x
    lda ap_probe_x
    clc
    adc #16
    sta ap_probe_x
    bcc :+
    inc ap_probe_x+1
:
    jsr ap_tile_type_at
    cmp #tile_type_ladder
    bne :+
    lda player_x
    clc
    adc #16
    sta player_x
    bcc :+
    inc player_x+1
:
    sec
    rts

; Arcade $91CF: 1.375 px up ($0160) after up, else down ($FEA0).
ap_ladder_move:
    lda ap_stick
    cmp #3
    bne @down
    lda ap_y_frac
    clc
    adc #$60
    sta ap_y_frac
    lda #$ff                    ; feet go up by 1, and by 2 with the carry
    bcc :+
    lda #$fe
:
    jmp ap_add_player_y
@down:
    lda ap_y_frac
    clc
    adc #$a0
    sta ap_y_frac
    lda #2                      ; height + $FE + carry
    bcc :+
    lda #1
:
    jmp ap_add_player_y

; A = the buttons held in this update and the previous one.
ap_held_buttons:
    lda ap_pressed
    eor #$ff
    and ap_pad
    rts

; Arcade $98AB, crouching with neither up nor down held. A direction that the player does
; not face turns them; the facing direction stands them up after ap_crouch_wait updates
; (7 after the first time). With no direction they stand up 15 updates after the pose
; ends. The count shares the shot phase ($0505) with the pose.
ap_crouch_release:
    jsr ap_held_dir
    beq @no_dir
    sec
    sbc #1
    cmp player_left
    beq @facing
    sta player_left
    rts
@facing:
    lda ap_shoot_lock
    bne @wait
    lda ap_crouch_wait
    beq @stand_now
    dec ap_crouch_wait
    bne @wait
@stand_now:
    lda #7
    sta ap_crouch_wait
    lda #0
    sta ap_crouch
@wait:
    rts
@no_dir:
    lda ap_shoot_lock
    bne @wait
    lda ap_shot_phase
    bne :+
    lda #15
    sta ap_crouch_wait
    inc ap_shot_phase
:
    dec ap_crouch_wait
    bne @wait
    lda #0
    sta ap_crouch
    sta ap_shot_phase
    rts

; Arcade $907B: a jump press starts the jump in the walking direction and clears the shot
; phase.
ap_revg_jump_check:
    lda ap_pressed
    and #$80
    beq @no_jump
    lda ap_move_dir
    sta ap_air_dir
    lda #$ff
    sta ap_riding_plat
    lda #ap_state_air
    sta ap_state
    lda #0
    sta ap_jump_phase
    sta ap_shot_phase
@no_jump:
    rts
.endif

; Arcade $9233: a direction counts once it is held in this update and the previous one,
; left before right.
; Returns C set when the player must not walk in this update.
ap_update_move_dir:
    jsr ap_held_dir
    beq @no_dir
    cmp ap_move_dir
    beq @same_dir
    ; A new direction turns the player and walks in the same update (CLRB at $9263).
    sta ap_move_dir
    jsr ap_face_dir
.ifdef REVG_STAGE1
    ; Turning ends the shot pose ($9259).
    lda #0
    sta ap_shoot_lock
.endif
@same_dir:
    clc
    rts
@no_dir:
    sta ap_move_dir
    sec
    rts

; A = 2 when left is held for two updates, else 1 for right, else 0 (Z set).
ap_held_dir:
    lda ap_pressed
    eor #$ff
    and ap_pad
    tax
    and #$02
    bne @held_left
    txa
    and #$01
    rts
@held_left:
    lda #2
    rts

; A = 1 (right) or 2 (left).
ap_face_dir:
    sec
    sbc #1
    sta player_left
    rts

; Arcade $92DA with the walk table $93B6: 288/256 px per update.
ap_walk_speed_frac = $20
ap_walk_step:
.ifdef REVG_STAGE1
    ; No step into a tombstone ($9271, $9298): the probe is 1 px ahead and 1 px up.
    ldx #1
    lda ap_move_dir
    cmp #1
    beq :+
    ldx #$ff
:
    txa
    jsr ap_tomb_probe_ahead
    bcc :+
    rts
:
    ; No step within 10 px of the screen edge ($9283, $92A9).
    lda ap_move_dir
    jsr ap_edge_room
    bcs :+
    rts
:
.endif
    lda ap_move_dir
    cmp #1
    bne @walk_left
    lda ap_x_frac
    clc
    adc #ap_walk_speed_frac
    sta ap_x_frac
    bcc :+
    jsr ap_move_right_1px
:
    jmp ap_move_right_1px
@walk_left:
    lda ap_x_frac
    sec
    sbc #ap_walk_speed_frac
    sta ap_x_frac
    bcs :+
    jsr ap_move_left_1px
:
    jmp ap_move_left_1px

; Arcade $93C2, copied into the record on landing ($953E) and at the start ($8F6B).
ap_reset_velocity:
    lda #0
    sta ap_vx
    sta ap_vx+2
    sta ap_vy+2
    lda #$80
    sta ap_vx+1
    lda #4
    sta ap_vy
    lda #$40
    sta ap_vy+1
    rts

; Walking off an edge (arcade $9055) falls straight down at the reset speed.
ap_begin_fall:
    lda #ap_state_air
    sta ap_state
    lda #2
    sta ap_jump_phase
    lda #0
    sta ap_air_dir
.ifndef REVG_STAGE1
    sta ap_crouch               ; the arcade keeps $051F while falling off ($9055)
.endif
    lda #$ff
    sta ap_riding_plat
    jmp ap_reset_velocity

; --- Air state: arcade jump ($93CA, $942B rise, $946A fall, $952D landing) ---
ap_jump_vx_accel = $0fa0        ; $93BA: added while rising, taken off while falling
ap_jump_gravity = $3d90
ap_knock_vx_accel = $0df0       ; $9A59, the knockback
ap_knock_gravity = $4590
ap_zombie_box_x = 9             ; $F122: X -9 to +9
ap_zombie_box_below = 19        ; height -19 to +19, crouching -19 to +13 ($F1BA)
ap_zombie_box_crouch_above = 13
; $F6BE: the miss when the height above the camera is 32 or less. player_feet is
; 254 - height (the start stands at height 62, feet 192).
ap_pit_feet = 254 - 32
ap_jump_vy_max = 5
ap_step_air:
.ifdef REVG_STAGE1
    ; $93CA, before every phase: within 10 px of the screen edge the jump loses its
    ; sideways direction. A jump with a direction also clears the crouch and the stick
    ; ($93FF); a straight-up jump from a crouch keeps the crouch.
    lda ap_air_dir
    beq :+
    lda #0
    sta ap_crouch
    sta ap_stick
    lda ap_air_dir
    jsr ap_edge_room
    bcc @air_edge
    bne :+
@air_edge:
    lda #0
    sta ap_air_dir
:
.endif
    lda ap_jump_phase
    bne @air_moving
    ; The update after the press only starts the jump ($9415), which ends the shot pose.
    inc ap_jump_phase
.ifdef REVG_STAGE1
    lda #0
    sta ap_shoot_lock
.endif
    rts
@air_moving:
    ; Only the facing follows the controller in the air ($9572).
    jsr ap_held_dir
    beq :+
    jsr ap_face_dir
:
.ifdef REVG_STAGE1
    jsr ap_shot_pose            ; $9433, $9472
.endif
; The moves of the jump without the controller, shared with the knockback ($9436, $9475).
ap_air_motion:
    jsr ap_air_move_x
    lda ap_jump_phase
    cmp #1
    beq ap_air_rise
    jmp ap_air_fall

; The acceleration table ($0523): $93BA for the jump and the fall off an edge, $9A59
; for the knockback. Y = 1 in the knockback.
ap_air_table:
    ldy #0
    lda ap_state
    cmp #ap_state_knockback
    bne :+
    iny
:
    rts
ap_air_accel_lo: .byte <ap_jump_vx_accel, <ap_knock_vx_accel
ap_air_accel_hi: .byte >ap_jump_vx_accel, >ap_knock_vx_accel
ap_air_gravity_lo: .byte <ap_jump_gravity, <ap_knock_gravity
ap_air_gravity_hi: .byte >ap_jump_gravity, >ap_knock_gravity

ap_air_rise:
    ; Height += vy ($94B0): feet go up by the integer part and the fraction's carry.
    lda ap_y_frac
    clc
    adc ap_vy+1
    sta ap_y_frac
    lda #0
    adc ap_vy
    eor #$ff
    clc
    adc #1
    jsr ap_add_player_y
    jsr ap_air_ceiling
    bcs @air_rise_done
    ; vx += accel, vy -= gravity ($94EA). The rise ends when vy is below the gravity.
    jsr ap_air_table
    lda ap_vx+2
    clc
    adc ap_air_accel_lo,y
    sta ap_vx+2
    lda ap_vx+1
    adc ap_air_accel_hi,y
    sta ap_vx+1
    bcc :+
    inc ap_vx
:
    lda ap_vy+2
    sec
    sbc ap_air_gravity_lo,y
    sta ap_vy+2
    lda ap_vy+1
    sbc ap_air_gravity_hi,y
    sta ap_vy+1
    bcs :+
    dec ap_vy
:
    lda ap_vy
    bne @air_rise_done
    lda ap_vy+1
    cmp ap_air_gravity_hi,y
    bcs @air_rise_done
    lda #2
    sta ap_jump_phase
@air_rise_done:
    rts

; The head hits a solid cell while rising: start falling from below it.
; The arcade rise has no ceiling test; section 0 has no ceilings.
ap_air_ceiling:
    lda player_feet
    sec
    sbc #player_height
    sta ap_probe_y
    lda ap_player_y_hi
    sbc #0
    sta ap_probe_y_hi
    bmi @no_ceiling
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    jsr ap_tile_type_at
    cmp #tile_type_solid
    bne @no_ceiling
    lda ap_probe_y
    and #$f0
    clc
    adc #16 + player_height
    sta player_feet
    lda ap_probe_y_hi
    adc #0
    sta ap_player_y_hi
    lda #2
    sta ap_jump_phase
    sec
    rts
@no_ceiling:
    clc
    rts

ap_air_fall:
    ; Height -= vy ($94BD): feet go down by the integer part and the fraction's borrow.
    lda ap_y_frac
    sec
    sbc ap_vy+1
    sta ap_y_frac
    lda ap_vy
    bcs :+
    adc #1                      ; borrow from the fraction (C is clear here)
:
    jsr ap_add_player_y
    jsr ap_check_support
    bcc @air_fall_accel
    lda ap_tile_row_top
    sta player_feet
    lda ap_tile_row_top_hi
    sta ap_player_y_hi
.ifdef REVG_STAGE1
    ; $9532: in the knockback the landing only moves on to the next step.
    lda ap_state
    cmp #ap_state_knockback
    bne :+
    inc ap_kb_step
    rts
:
.endif
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
    lda #0
    sta ap_air_dir
    sta ap_move_dir
.ifdef REVG_STAGE1
    sta ap_shoot_lock           ; $9544
.endif
    jmp ap_reset_velocity
@air_fall_accel:
    ; $950C keeps vx when its 16-bit fraction would turn negative.
    jsr ap_air_table
    lda ap_vx+2
    sec
    sbc ap_air_accel_lo,y
    tax
    lda ap_vx+1
    sbc ap_air_accel_hi,y
    bmi :+
    sta ap_vx+1
    stx ap_vx+2
:
    lda ap_vy
    cmp #ap_jump_vy_max
    bcs @air_fall_done
    lda ap_vy+2
    clc
    adc ap_air_gravity_lo,y
    sta ap_vy+2
    lda ap_vy+1
    adc ap_air_gravity_hi,y
    sta ap_vy+1
    bcc @air_fall_done
    inc ap_vy
@air_fall_done:
    rts

; X += vx toward ap_air_dir ($94CA right, $94A3 left), one pixel at a time for the walls.
ap_air_move_x:
    lda ap_air_dir
    beq @air_x_done
    cmp #1
    bne @air_x_left
    lda ap_x_frac
    clc
    adc ap_vx+1
    sta ap_x_frac
    lda ap_vx
    adc #0
    sta ap_steps
    beq @air_x_done
:
    jsr ap_move_right_1px
    dec ap_steps
    bne :-
@air_x_done:
    rts
@air_x_left:
    lda ap_x_frac
    sec
    sbc ap_vx+1
    sta ap_x_frac
    lda ap_vx
    bcs :+
    adc #1                      ; borrow from the fraction (C is clear here)
:
    sta ap_steps
    beq @air_x_done
:
    jsr ap_move_left_1px
    dec ap_steps
    bne :-
    rts

; --- Ladder state: vertical climbing only (16-bit Y safe) ---
ap_step_ladder:
.ifdef REVG_STAGE1
    jmp ap_step_ground_revg
.endif
    lda #0
    sta ap_crouch
    lda #$ff
    sta ap_riding_plat
    lda ap_pad
    and #$08
    bne @climb_up
    lda ap_pad
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
    jmp ap_begin_fall

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
.ifdef REVG_STAGE1
    jmp ap_step_knock_revg
.endif
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
    jmp ap_begin_fall
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

.ifdef REVG_STAGE1
; Arcade state 3 ($995F, the knockback $9975 and the death $9AA8). ap_kb_step:
;   0  the hurt only ($997F): 120 blink updates, the speeds reset
;   1  away from the hit, facing that way ($99ED, $9A12), the speeds reset
;   2  the jump's rise and fall with the $9A59 table, no controller and no shot, until
;      the landing ($9436, $9475)
;   3  the hurt stands up without the armor ($99FB); the death lies down ($9A61)
;   4  the death: 20 updates ($9A78)
;   5  the death: 40 more updates, then the miss ($9A8D sets $90)
ap_step_knock_revg:
    lda ap_kb_step
    bne @knock_dir
    lda #ap_invuln_frames
    sta ap_invuln
    jsr ap_reset_velocity
    inc ap_kb_step
    jmp @knock_after
@knock_dir:
    cmp #1
    bne @knock_air
    lda ap_kb_dir
    sta ap_air_dir
    sec
    sbc #1
    sta player_left
    jsr ap_reset_velocity
    lda #1
    sta ap_jump_phase
    inc ap_kb_step
    jmp @knock_after
@knock_air:
    cmp #2
    bne @knock_landed
    jsr ap_air_motion
    jmp @knock_after
@knock_landed:
    cmp #3
    bne @knock_wait
    lda ap_kb_dying
    bne @knock_lie
    lda #ap_state_ground
    sta ap_state
    lda #$ff
    sta jump_step
    lda #0
    sta ap_shot_phase
    sta ap_jump_phase
    jsr ap_reset_velocity
    jmp @knock_after
@knock_lie:
    ; Steps 3 and 4 run on into the next step in the same update ($9A76, $9A8B).
    lda #20
    sta ap_crouch_wait          ; the arcade counts in $0520 too
    inc ap_kb_step
    jmp @knock_count
@knock_wait:
    cmp #4
    bne @knock_last
@knock_count:
    dec ap_crouch_wait
    bne @knock_after
    lda #40
    sta ap_crouch_wait
    inc ap_kb_step
    ; $8D22 with B = 2: the size class $050F goes from 1 to 2, which lowers the player
    ; by 8 px (the $8D58 row).
    lda #8
    jsr ap_add_player_y
@knock_last:
    lda ap_crouch_wait
    beq @knock_miss
    dec ap_crouch_wait
    jmp @knock_after
@knock_miss:
    lda #1
    sta player_hit
@knock_after:
    ; $996B after every step: no crouch and no stick, and within 10 px of the screen
    ; edge on the facing side the flight loses its direction ($93E5, $9405).
    lda #0
    sta ap_crouch
    sta ap_stick
    lda player_left
    clc
    adc #1
    jsr ap_edge_room
    bcc :+
    bne @knock_done
:
    lda #0
    sta ap_air_dir
@knock_done:
    rts
.endif

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
.ifdef REVG_STAGE1
ap_foot_probe = 8
.else
ap_foot_probe = 4
.endif
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

    ; Static 2D tile support (X - ap_foot_probe, then X + ap_foot_probe). The arcade $8B0A looks at
    ; X - 8 first and then the tile 16 px to the right ($8C09 and $8BDD for size class 1).
    lda player_feet
    sta ap_probe_y
    and #$f0
    sta ap_tile_row_top
    lda ap_player_y_hi
    sta ap_probe_y_hi
    sta ap_tile_row_top_hi
    lda player_x
    sec
    sbc #ap_foot_probe
    sta ap_probe_x
    lda player_x+1
    sbc #0
    sta ap_probe_x+1
    jsr ap_probe_foot
    bcs @supported
    lda player_x
    clc
    adc #ap_foot_probe
    sta ap_probe_x
    lda player_x+1
    adc #0
    sta ap_probe_x+1
.ifdef REVG_STAGE1
    jsr ap_probe_foot
    bcs @supported
    jmp ap_tomb_support
.else
    jmp ap_probe_foot
.endif
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
.ifdef REVG_STAGE1
    ; Arcade $8B0A: a floor tile of height H ($448E) holds the feet anywhere from its
    ; surface down to its in-tile height 0, not only when they come from above. In the FC
    ; cell that is from offset 14 - H (0 for mt_floor, the cell's offset for mt_floor_top
    ; and up) to 14. The update then lifts the feet onto the surface ($904F).
    lda #0
    cpx #mt_floor_top
    bcc :+
    txa
    sbc #mt_floor_top - 1       ; C is set
:
    sta ap_tmp0
    lda player_feet
    and #$0f
    cmp #15
    bcs @foot_no
    cmp ap_tmp0
    bcc @foot_no
    lda ap_tile_row_top
    ora ap_tmp0
    sta ap_tile_row_top
    sec
    rts
.endif
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
.ifdef REVG_STAGE1
    lda player_feet             ; the top ladder tile is also a floor
    and #$0f
    sta ap_tmp0
    lda #14
    cmp ap_tmp0
    bcs @check_above_ladder
    rts
.endif
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
    lda stage_cam_y_max_page
    bne @vert_fall_check
    lda ap_player_y_hi
    bne @fatal_hazard
    lda player_feet
.ifdef REVG_STAGE1
    cmp #ap_pit_feet
.else
    cmp #232
.endif
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
.ifdef REVG_STAGE1
    ; $F6BE: the miss song, unless Arthur is already dying (it started then).
    lda ap_state
    cmp #ap_state_knockback
    bne :+
    lda ap_kb_dying
    bne @hazard_done
:
    jmp ap_miss_song
@hazard_done:
.endif
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
    lda ap_probe_x+1     ; Metatile column = X / 16, up to 255 (16 pages).
    asl a
    asl a
    asl a
    asl a
    sta ap_tmp0
    lda ap_probe_x
    lsr a
    lsr a
    lsr a
    lsr a
    ora ap_tmp0
    jsr read_map_cell
    tax
    lda metatile_attr,x
    and #tile_type_mask
    rts

; --- Multi-weapon 16-bit shot pool (Lance 2, Dagger 3, Torch 2 + flame, Shield 2) ---
ap_update_shots:
.ifndef REVG_STAGE1
    jsr ap_try_spawn_shot
.endif
    ldx #0
@shot_loop:
    stx ap_loop_idx
    lda ps_active,x
    bne :+
    jmp @next_shot
:
.ifdef REVG_STAGE1
    ; The game build throws the shot in the player update (ap_throw_shot).
    cmp #ap_shot_thrown
    bne :+
    lda #1                      ; The update after the throw only sets it up ($9C9F).
    sta ps_active,x
    jmp @next_shot
:
    cmp #ap_shot_spent
    bne :+
    dec ps_timer,x
    beq @revg_reclaim
    jmp @next_shot
:
    lda ps_kind,x
    cmp #ap_weapon_lance
    bne @revg_not_lance
    jsr ap_step_lance
    ldx ap_loop_idx
    bcc :+
    ; Out of range: two more updates in states 2 and 3 ($9BEB, $9CF1), then gone.
    lda #ap_shot_spent
    sta ps_active,x
    lda #3
    sta ps_timer,x
    jmp @next_shot
:
    ; The lance passes through the ground; only tombstones and enemies stop it.
    jsr ap_shot_hits_tombstone
    ldx ap_loop_idx
    bcs @revg_reclaim
    jsr revg_enemy_shot_vec
    ldx ap_loop_idx
    bcs @revg_reclaim
    jmp @next_shot
@revg_reclaim:
    jmp @reclaim_shot
@revg_not_lance:
    lda ps_active,x
.endif
    cmp #2
    bne @shot_flying
.ifdef REVG_STAGE1
    jsr revg_enemy_shot_vec
.else
    jsr ap_shot_check_enemies
.endif
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
.ifdef REVG_STAGE1
    beq :+
    cmp #ap_weapon_axe
:
.endif
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
    ; Keep shots inside the camera's 256 px; this also drops shots left of X 0.
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
.ifdef REVG_STAGE1
    jsr revg_enemy_shot_vec
.else
    jsr ap_shot_check_enemies
.endif
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
    lda ap_pressed
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

.ifdef REVG_STAGE1
; --- Arcade shot ($8F71 pose, $9DB7 throw, $9B3A lance flight) ---
ap_shot_thrown = 4              ; ps_active: thrown in this update, not moved yet
ap_shot_spent = 3               ; ps_active: out of range, waiting ps_timer updates
ap_lance_speed = $0430          ; $9C73: 4 3/16 px per update
ap_lance_range = 192            ; $9C51

; $8F71, called by the player update on the ground, crouching and in the air. A press of
; the shot button throws when the pose timer is 5 or less; the pose then lasts 15 updates,
; and walking waits for it to end ($9265). A press with every slot in use still restarts
; the pose, as the arcade clears $0505 either way.
ap_shot_pose:
    lda ap_shoot_lock
    cmp #6
    bcs @pose
    lda ap_pressed
    and #$40                    ; B, the arcade shot button ($10)
    beq @pose
    jsr ap_throw_shot
    lda #0
    sta ap_shot_phase
@pose:
    lda ap_shoot_lock
    beq @done
    lda ap_shot_phase
    bne @count
    lda #ap_shoot_lock_frames   ; $8F9C
    sta ap_shoot_lock
    inc ap_shot_phase
@count:
    dec ap_shoot_lock           ; $8FC2
    bne @done
    lda #0
    sta ap_shot_phase
@done:
    rts

; $9DB7: the first free slot of the weapon's count ($9E0F), 15 px ahead of the player's X
; when facing right and at it when facing left, 9 px above the player's Y standing and
; 3 px below it crouching ($9DF7).
ap_throw_shot:
    ldx ap_weapon
    lda ap_revg_max_shots,x
    sta ap_tmp0
    ldx #0
@find:
    lda ps_active,x
    beq @found
    inx
    cpx ap_tmp0
    bne @find
    rts
@found:
    lda #ap_shot_thrown
    sta ps_active,x
    ldy ap_weapon
    tya
    sta ps_kind,x
    lda player_left
    bne @left
    lda player_x
    clc
    adc #15
    sta ps_x_lo,x
    lda player_x+1
    adc #0
    sta ps_x_hi,x
    lda ap_weapon_vx_pos,y
    jmp @set_vx
@left:
    lda player_x
    sta ps_x_lo,x
    lda player_x+1
    sta ps_x_hi,x
    lda ap_weapon_vx_neg,y
@set_vx:
    sta ps_vx,x
    lda ap_weapon_vy_init,y
    sta ps_vy,x
    lda ap_weapon_life,y
    sta ps_timer,x
    lda ps_x_lo,x
    sta ps_start_lo,x
    lda ps_x_hi,x
    sta ps_start_hi,x
    lda player_feet
    ldy ap_crouch
    bne @crouch
    sec
    sbc #9
    sta ps_y,x
    lda ap_player_y_hi
    sbc #0
    jmp @set_y_hi
@crouch:
    clc
    adc #3
    sta ps_y,x
    lda ap_player_y_hi
    adc #0
@set_y_hi:
    sta ps_y_hi,x
    lda #1
    sta shot_sound_pending
    lda #ap_shoot_lock_frames
    sta ap_shoot_lock
    rts

; $9B3A: move the lance, keeping the fraction of the slot. C set once it is 192 px or more
; from where it was thrown (X - start + 192 >= 384, unsigned).
ap_step_lance:
    lda ps_vx,x
    bmi @left
    lda ps_x_frac,x
    clc
    adc #<ap_lance_speed
    sta ps_x_frac,x
    lda ps_x_lo,x
    adc #>ap_lance_speed
    sta ps_x_lo,x
    bcc @range
    inc ps_x_hi,x
    jmp @range
@left:
    lda ps_x_frac,x
    sec
    sbc #<ap_lance_speed
    sta ps_x_frac,x
    lda ps_x_lo,x
    sbc #>ap_lance_speed
    sta ps_x_lo,x
    bcs @range
    dec ps_x_hi,x
@range:
    lda ps_x_lo,x
    sec
    sbc ps_start_lo,x
    sta ap_tmp0
    lda ps_x_hi,x
    sbc ps_start_hi,x
    sta ap_tmp1
    lda ap_tmp0
    clc
    adc #<ap_lance_range
    sta ap_tmp0
    lda ap_tmp1
    adc #>ap_lance_range
    cmp #>(2 * ap_lance_range)
    bne :+
    lda ap_tmp0
    cmp #<(2 * ap_lance_range)
:
    rts

ap_revg_max_shots: .byte 3, 3, 2, 3, 2 ; lance, dagger, torch, cross, axe ($9E0F)

; Arcade $92BF/$92C9 then $9E14: A = +1 or -1. Probes X + A at 1 px above the feet.
ap_tomb_probe_ahead:
    tax
    clc
    adc player_x
    sta ap_probe_x
    txa
    and #$80
    beq :+
    lda #$ff
:
    adc player_x+1
    sta ap_probe_x+1
    ldx player_feet
    dex
    stx ap_probe_y
    jmp ap_tomb_box

; Arcade $94D7 then $9E14, after the floor tiles found nothing: the player stands on a
; tombstone when X is in its box 1 px below the feet and the player is not below its
; centre. Returns C set with ap_tile_row_top at the (possibly lifted) feet.
ap_tomb_support:
    lda #1
    sta ap_tomb_above
    lda player_x
    sta ap_probe_x
    lda player_x+1
    sta ap_probe_x+1
    ldx player_feet
    inx
    stx ap_probe_y
    jsr ap_tomb_box
    bcc @off
    lda player_feet
    sta ap_tile_row_top
    lda ap_player_y_hi
    sta ap_tile_row_top_hi
@off:
    rts

; Arcade $9E14 for the tombstones (support type 1, box $9EB8: 12, 24, 19, 38). C set when
; the probe (ap_probe_x, feet ap_probe_y) is within 12 px of a tombstone's centre X and
; 19 px of its centre height (the feet of its floor). With ap_tomb_above set, the player
; must not be below the centre. A player more than 3 px from the centre height is put
; on top, 19 px above it. A miss clears ap_tomb_above ($9EAD).
ap_tomb_box:
    lda campaign_stage
    cmp #1
    bne @miss
    lda ap_player_y_hi
    bne @miss                   ; section 0 is one row of pages
    lda ap_probe_x
    clc
    adc #12
    sta ap_tmp0
    lda ap_probe_x+1
    adc #0
    sta ap_tmp1
    ldx #revg_tomb_count - 1
@loop:
    lda ap_tmp0
    sec
    sbc revg_tomb_x_lo,x
    tay
    lda ap_tmp1
    sbc revg_tomb_x_hi,x
    bne @next
    cpy #25
    bcs @next
    ; Height above the centre = centre feet - probe feet.
    lda revg_tomb_feet,x
    sec
    sbc ap_probe_y
    clc
    adc #19
    cmp #39
    bcs @next
    lda ap_tomb_above
    beq @inside
    lda revg_tomb_feet,x
    cmp player_feet
    bcc @next
@inside:
    lda revg_tomb_feet,x
    sec
    sbc player_feet
    clc
    adc #3
    cmp #6
    bcc @hit
    lda revg_tomb_feet,x
    sec
    sbc #19
    sta player_feet
@hit:
    sec
    rts
@next:
    dex
    bpl @loop
@miss:
    lda #0
    sta ap_tomb_above
    clc
    rts
.endif

; Carry set if shot slot ap_loop_idx hits any tombstone platform.
ap_shot_hits_tombstone:
.ifdef REVG_STAGE1
    lda campaign_stage
    cmp #1
    bne @revg_miss
    ; The section 0 tombstones (revg_tomb_*), arcade $F5FE: the shot point within 14 px of
    ; the centre X and from 8 px below to 4 px above the stone's Y (the floor; the box at
    ; $F67E + 8). The standing shot is 9 px above the feet and passes over; the crouching
    ; one, 3 px below them, hits.
    ldx ap_loop_idx
    lda ps_y_hi,x
    bne @revg_miss
    lda ps_x_lo,x
    clc
    adc #14
    sta ap_tmp0
    lda ps_x_hi,x
    adc #0
    sta ap_tmp1
    ldy #revg_tomb_count - 1
@revg_loop:
    lda ap_tmp0
    sec
    sbc revg_tomb_x_lo,y
    sta ap_tmp2
    lda ap_tmp1
    sbc revg_tomb_x_hi,y
    bne @revg_next
    lda ap_tmp2
    cmp #29
    bcs @revg_next
    lda ps_y,x
    sec
    sbc revg_tomb_feet,y
    clc
    adc #4
    cmp #13
    bcs @revg_next
    ; Arcade $F656: decrement tombstone hit counter; on 15th hit request Magician ($07).
    lda ap_tomb_hits,y
    beq @tomb_hit_se
    sec
    sbc #1
    sta ap_tomb_hits,y
    bne @tomb_hit_se
    lda revg_tomb_x_lo,y
    sta ap_mag_x
    lda revg_tomb_x_hi,y
    sta ap_mag_x+1
    lda #254
    sec
    sbc revg_tomb_feet,y
    clc
    adc #16
    sta ap_mag_y
    lda #1
    sta ap_mag_req
@tomb_hit_se:
    lda #snd_se_hit
    jsr call_bank8_play_se
    sec
    rts
@revg_next:
    dey
    bpl @revg_loop
@revg_miss:
    clc
    rts
.else
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
.endif

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

ap_apply_player_damage:
.ifdef REVG_STAGE1
    ; $F0A1 keeps $AE = attacker X - player X: Arthur flies away from that side.
    lda ap_probe_x
    sec
    sbc player_x
    lda ap_probe_x+1
    sbc player_x+1
    ldx #2
    bpl ap_hit_toward
    dex
; X = the direction to fly, 1 right or 2 left. The hit starts state 3 ($0503) for the
; next update: with the armor on it is the knockback, without it the death ($995F reads
; $AC). No hit lands during state 3 ($0500 is 2) or the miss.
ap_hit_toward:
    lda #0                      ; Arcade: taking damage as a frog cancels frog form
    sta ap_frog_timer
    sta ap_frog_timer+1
    lda ap_state
    cmp #ap_state_knockback
    bcs @hit_ignored
    stx ap_kb_dir
    lda #ap_state_knockback
    sta ap_state
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_shot_phase
    sta ap_kb_dying
    tax
    lda ap_armor
    cmp #ap_armor_full
    beq :+
    inc ap_kb_dying
    inx                         ; the death starts with the direction ($9A12)
:
    stx ap_kb_step
    ; The FC draws Arthur without the armor from the hit; the arcade $AC turns 1 on landing.
    lda #ap_armor_naked
    sta ap_armor
    lda ap_kb_dying
    beq :+
    jmp ap_miss_song            ; $9A12
:
    lda #snd_se_damage
    jmp call_bank8_play_se
@hit_ignored:
    rts

; $EC81, the time-out: $AC = 1, $0500 = 2 and state 3 at sub-step 3 of the death, so
; Arthur lies down where he is, even in the air or during the knockback, and the miss
; follows 60 updates later.
ap_time_up:
    lda ap_state
    cmp #ap_state_dead
    beq @time_up_done
    lda #ap_state_knockback
    sta ap_state
    lda #$ff
    sta ap_riding_plat
    lda #0
    sta ap_shot_phase
    lda #1
    sta ap_kb_dying
    lda #3
    sta ap_kb_step
    lda #ap_armor_naked
    sta ap_armor
    jmp ap_miss_song            ; $EC81
@time_up_done:
    rts

; $9A12, $EC81, $F6E0: the miss song $31, or the game over song $2F for the last life
; ($9A17), plus the APU death SFX so the hit/fall is clearly audible immediately.
ap_miss_song:
    lda campaign_lives
    cmp #2
    lda #snd_song_miss
    bcs :+
    lda #snd_song_gameover
:
    jsr call_bank8_play_song
    lda #snd_se_death
    jmp call_bank8_play_se
.endif
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
.ifdef REVG_STAGE1
    ; $F057 with the zombie's box ($F0CA + 8 x 11; crouching $F162): hit when
    ; enemy X - player X + 9 <= 18 and enemy height - player height + 19 <= 38
    ; (crouching 32), both unsigned. The heights are up-positive, so the height difference
    ; is player_feet - probe_y. The other kinds get their boxes with their arcade AI.
    sec
    lda ap_probe_x
    sbc player_x
    tay
    lda ap_probe_x+1
    sbc player_x+1
    tax
    tya
    clc
    adc #ap_zombie_box_x
    tay
    txa
    adc #0
    bne @revg_miss
    cpy #ap_zombie_box_x * 2 + 1
    bcs @revg_miss
    sec
    lda player_feet
    sbc ap_probe_y
    tay
    lda ap_player_y_hi
    sbc ap_probe_y_hi
    tax
    tya
    clc
    adc #ap_zombie_box_below
    tay
    txa
    adc #0
    bne @revg_miss
    lda #ap_zombie_box_below * 2 + 1
    ldx ap_crouch
    beq :+
    lda #ap_zombie_box_below + ap_zombie_box_crouch_above + 1
:
    sta ap_tmp0
    cpy ap_tmp0             ; C set (miss) when the offset is past the box.
    bcs @revg_miss
    sec
    rts
@revg_miss:
    clc
    rts
.else
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
.endif

; Lance, dagger, torch, cross (shield), axe. The axe (game build) is thrown up like the
; torch, higher, and passes through enemies; the cross flies a short way.
ap_weapon_max_shots: .byte 2, 3, 2, 2, 2
ap_weapon_vx_pos:    .byte 3, 5, 2, 4, 2
ap_weapon_vx_neg:    .byte $fd, $fb, $fe, $fc, $fe
ap_weapon_vy_init:   .byte 0, 0, $fc, 0, $fb
ap_weapon_life:      .byte 48, 48, 56, 26, 64

ap_kb_delta:
    .byte $fe, $fe, $ff, $ff, $ff, $00, $00, $00
    .byte $01, $01, $01, $02, $02, $02, $03, $03

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
