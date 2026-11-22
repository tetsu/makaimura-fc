; Original platform-and-ladder course for the 512 KiB gameplay test build.
platform_course_header:
    stage_header platform_course_width, platform_course_height, 0, 0, 32, 48, 176, 0, 16, 232, 80, platform_course_map, platform_course_palettes
.assert platform_course_header = stage_header_base, lderror, "Platform course header offset mismatch"
.res $7f - (* - platform_course_header), $ff

platform_course_desc:
.assert platform_course_desc = sd_base, lderror, "Platform course descriptor offset mismatch"
    .byte 0                             ; Horizontal course with a ground goal
    .word 176, 176                      ; Start and checkpoint Y
    .byte 0                             ; Checkpoint camera page
    .word 80                            ; Goal Y
    .byte snd_song_stage, snd_song_boss
    .word 0, 0                          ; Default BG and OBJ palettes
    .word 0, 0                          ; Default BG and OBJ CHR
    .word platform_course_enemy1_x, platform_course_enemy1_y
    .word platform_course_pot_x, platform_course_pot_y
    .byte platform_course_pot_drop
    .byte platform_course_item_count
    .word platform_course_items
    .byte platform_course_plat_count
    .word platform_course_plats
    .byte platform_course_sched_page
    .word platform_course_turret_x, platform_course_turret_y
    .word platform_course_zombie_x, platform_course_zombie_y
    .res sd_sched_table_ptr + 2 - sd_boss_trig_x, 0
.res $100 - 1 - (* - platform_course_header), $ff

.include "platform-course-map.inc"
