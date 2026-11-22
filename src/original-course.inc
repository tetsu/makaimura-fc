; Optional original Stage 1 courses for the 512 KiB game mode.
.ifdef SCROLL_COURSE
    course_cp_x = 416
    course_cp_page = 1
    course_goal_x = 724
    course_goal_y = 176
    course_flags = sd_flag_sched_table
    course_scheduler = course_sched_table
.else
    course_cp_x = 48
    course_cp_page = 0
    course_goal_x = 232
    course_goal_y = 80
    course_flags = 0
    course_scheduler = 0
.endif

course_header:
    stage_header course_width, course_height, course_width / 16 - 1, 0, 32, course_cp_x, 176, course_cp_page, 16, course_goal_x, course_goal_y, course_map, course_palettes
.assert course_header = stage_header_base, lderror, "Course header offset mismatch"
.res $7f - (* - course_header), $ff

course_desc:
.assert course_desc = sd_base, lderror, "Course descriptor offset mismatch"
    .byte course_flags
    .word 176, 176                      ; Start and checkpoint Y
    .byte 0                             ; Checkpoint camera Y page
    .word course_goal_y
    .byte snd_song_stage, snd_song_boss
    .word 0, 0                          ; Default BG and OBJ palettes
    .word 0, 0                          ; Default BG and OBJ CHR
    .word course_enemy1_x, course_enemy1_y
    .word course_pot_x, course_pot_y
    .byte course_pot_drop
    .byte course_item_count
    .word course_items
    .byte course_plat_count
    .word course_plats
    .byte course_sched_page
    .word course_turret_x, course_turret_y
    .word course_zombie_x, course_zombie_y
    .res sd_sched_table_ptr - sd_boss_trig_x, 0
    .word course_scheduler
.res $100 - 1 - (* - course_header), $ff

.ifdef SCROLL_COURSE
    .include "scroll-course-map.inc"
.else
    .include "platform-course-map.inc"
.endif
