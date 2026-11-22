-- Normal 512 KiB game mode with -PlatformCourse or -CourseSequence.
-- Drive the entire route with controller input. Read gameplay state for assertions.
-- Zero page from vrc7a-test.s and playable-scene.inc, WRAM from arcade-player.inc.
local SCENE = 0x07
local STAGE = 0x14
local LIVES = 0x15
local CAMERA_X = 0x25
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local CHECKPOINT = 0x70
local AP_STATE = 0x6010
local AP_RIDING = 0x601b
local PLAT_TYPE = 0x603e
local PLAT_X = 0x6042
local LIFT_Y = 0x604b
local HORIZONTAL_PHASE = 0x605a
local SCHED_PAGE = 0x6074
local SCHED_GROUP = 0x6075
local EN_TYPE = 0x6079
local EN_X_LO = 0x607d
local EN_X_HI = 0x6081
local SCORE = 0x60c8

local frame = 0
local phase = "boot"
local clearFrame
local input = {}
local clearTitle = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}
local finalTitle = {0x07, 0x0a, 0x0e, 0x02, 0x0c, 0, 0x04, 0x0c, 0x06, 0x02, 0x11, 0}
local sequence = emu.getRomInfo().name:find("-course-sequence", 1, true) ~= nil
local scrollStart
local sawFirstEnemy = false

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function readWord(addr)
    return read(addr) + 256 * read(addr + 1)
end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s x=%d: %s", frame, phase, readWord(PLAYER_X), message))
    emu.stop(1)
    return false
end

local function resultMatches(title)
    for index, tile in ipairs(title) do
        if emu.read(0x2189 + index, emu.memType.nesPpuDebug) ~= tile then return false end
    end
    return true
end

-- Whether a zombie is within 48 px ahead, and whether any enemy is within 92 px ahead.
local function enemiesAhead(x)
    local zombie, any = false, false
    for slot = 0, 3 do
        local kind = read(EN_TYPE + slot)
        local distance = read(EN_X_LO + slot) + 256 * read(EN_X_HI + slot) - x
        if kind ~= 0 and distance > 0 and distance <= 92 then
            any = true
            zombie = zombie or (kind == 1 and distance <= 48)
        end
    end
    return zombie, any
end

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < (sequence and 2000 or 900), "route timed out") then return end

    local x = read(PLAYER_X)
    local y = read(PLAYER_FEET)
    local state = read(AP_STATE)
    local riding = read(AP_RIDING)
    local platformX = read(PLAT_X)
    local liftY = read(LIFT_Y)
    if phase ~= "fall" and phase ~= "retry" and not check(state ~= 4, "unexpected miss") then return end

    if phase == "boot" then
        if frame == 128 then
            input = {start = true}
            phase = "start"
        end
    elseif phase == "start" then
        if read(SCENE) == 4 and riding == 0xff and read(0x8000) == 0x8c and platformX ~= 0 then
            if not check(x == 32 and y == 176 and read(LIVES) == 3
                    and read(CHECKPOINT) == 0, "fixture start state") then return end
            phase = "fall"
        end
    elseif phase == "fall" then
        input = {right = true}
        if state == 4 then
            if not check(read(CHECKPOINT) == 1 and read(LIVES) == 2 and y >= 232,
                    "miss after reaching the checkpoint") then return end
            input = {start = true}
            phase = "retry"
        end
    elseif phase == "retry" then
        if state == 0 and read(0x8000) == 0x8c and platformX ~= 0 then
            if not check(x == 48 and y == 176 and read(LIVES) == 2 and read(CHECKPOINT) == 1,
                    "checkpoint restart position or lives") then return end
            if not check(platformX == 70 and liftY == 176, "both movers reset") then return end
            phase = "approach"
        end
    elseif phase == "approach" then
        if x < 62 then
            input = {right = true}
        elseif platformX == 122 and read(HORIZONTAL_PHASE) == 1 then
            -- Meet the platform on its return path.
            input = {right = true, a = true}
            phase = "land_horizontal"
        end
    elseif phase == "land_horizontal" then
        if riding == 0 then
            if not check(state == 0 and y == 176, "horizontal platform landing") then return end
            phase = "ride_horizontal"
        end
    elseif phase == "ride_horizontal" then
        if not check(riding == 0 and y == 176, "lost horizontal ride") then return end
        if x >= 115 and read(HORIZONTAL_PHASE) == 0 then
            -- The lift will be rising from its lower end when the jump descends.
            input = {right = true, a = true}
            phase = "land_lift"
        end
    elseif phase == "land_lift" then
        if riding == 1 then
            if not check(state == 0 and y == liftY, "lift landing") then return end
            phase = "ride_lift"
        end
    elseif phase == "ride_lift" then
        if not check(riding == 1 and y == liftY, "lost vertical ride") then return end
        if y <= 140 then
            input = {right = true, a = true}
            phase = "shelf"
        end
    elseif phase == "shelf" then
        if state == 0 and riding == 0xff then
            if not check(y == 144 and x >= 160 and x < 192, "landing on ladder shelf") then return end
            phase = "ladder"
        end
    elseif phase == "ladder" then
        if state == 2 or (x >= 176 and x < 192) then
            input = {up = true}
        else
            input = {right = true}
        end
        if state == 2 and y < 128 then
            phase = "climb"
        end
    elseif phase == "climb" then
        input = {up = true}
        if state == 0 then
            if not check(y == 80, "ladder exit height") then return end
            phase = "goal"
        end
    elseif phase == "goal" then
        input = {right = true}
        if read(SCENE) == 5 then
            if not check(x == 232 and y == 80 and read(LIVES) == 2,
                    "clear without another miss") then return end
            clearFrame = frame
            phase = "verify_clear"
        end
    elseif phase == "verify_clear" then
        if frame == clearFrame + 4 then
            if not check(resultMatches(clearTitle), "COURSE CLEAR title missing") then return end
            if sequence then
                if not check(readWord(SCORE) == 1, "first course item should award 100 points") then return end
                input = {start = true}
                phase = "scroll_entry"
            else
                print("PASS: checkpoint restart, horizontal-to-vertical transfer, ladder, course clear")
                emu.stop(0)
            end
        end
    elseif phase == "scroll_entry" then
        if read(SCENE) == 4 and read(0x8000) == 0x8d and read(SCHED_GROUP) == 1 then
            if not check(read(STAGE) == 2 and x == 32 and y == 176 and readWord(CAMERA_X) == 0
                    and read(CHECKPOINT) == 0 and read(SCHED_PAGE) == 0 and riding == 0xff,
                    "second course initialization") then return end
            if not check(readWord(SCORE) == 1 and read(LIVES) == 2, "score and lives did not carry") then return end
            for slot = 0, 3 do
                if not check(read(PLAT_TYPE + slot) == 0 and read(EN_TYPE + slot) == 0,
                        "second course platform or enemy pool was not reset") then return end
            end
            scrollStart = frame
            phase = "scroll"
        end
    elseif phase == "scroll" then
        x = readWord(PLAYER_X)
        if readWord(CAMERA_X) == 0 and read(EN_TYPE) == 1 then
            local enemyX = read(EN_X_LO) + 256 * read(EN_X_HI)
            sawFirstEnemy = sawFirstEnemy or enemyX == 225 or enemyX == 224
        end
        -- With two zombies at most, one rises past the pit soon after the last is shot.
        -- Shoot a close zombie quickly and wait at the pit edge while one is close.
        -- Firing more often than every 18 frames otherwise keeps the player standing.
        local zombie, any = enemiesAhead(x)
        local tick = frame - scrollStart
        input = {right = x ~= 575 or not zombie, b = (zombie and tick % 8 == 0) or (any and tick % 18 == 0),
            a = x == 575 and state == 0 and not zombie}
        if read(SCENE) == 5 then
            local score = readWord(SCORE)
            local expectedLives = score >= 20 and 3 or 2
            if not check(x == 724 and y == 176 and read(LIVES) == expectedLives and sawFirstEnemy and score > 1,
                    "second course goal and enemy score") then return end
            clearFrame = frame
            phase = "final_clear"
        end
    elseif phase == "final_clear" and frame == clearFrame + 4 then
        if not check(resultMatches(finalTitle), "FINAL CLEAR title missing") then return end
        input = {start = true}
        phase = "return_to_title"
    elseif phase == "return_to_title" then
        if read(SCENE) == 7 then
            if not check(read(STAGE) == 0, "final clear did not return to title") then return end
            if frame >= clearFrame + 12 then
                input = {start = true}
                phase = "new_game"
            end
        else
            if not check(read(STAGE) == 2, "final clear advanced beyond the sequence") then return end
        end
    elseif phase == "new_game" then
        if read(SCENE) == 4 and read(0x8000) == 0x8c and read(PLAT_TYPE) ~= 0 then
            if not check(read(STAGE) == 1 and x == 32 and y == 176 and read(CHECKPOINT) == 0
                    and read(LIVES) == 3 and readWord(SCORE) == 0, "fresh sequence did not reset") then return end
            print("PASS: two-course sequence, score/lives carry, final clear, fresh start")
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
