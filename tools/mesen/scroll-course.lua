-- Play the original three-page course using only controller input.
local SCENE = 0x07
local LIVES = 0x15
local CAMERA_X = 0x25
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local CHECKPOINT = 0x70
local AP_STATE = 0x6010
local SCHED_PAGE = 0x6074
local SCHED_GROUP = 0x6075
local EN_TYPE = 0x6079
local EN_X_LO = 0x607d
local EN_X_HI = 0x6081
local EN_Y = 0x6085
local EN_PAGE = 0x6089

local frame = 0
local phase = "title"
local input = {}
local clearFrame
local seen = {}
local clearTitle = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function readWord(addr)
    return read(addr) + 256 * read(addr + 1)
end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s x=%d feet=%d camera=%d: %s", frame, phase,
        readWord(PLAYER_X), read(PLAYER_FEET), readWord(CAMERA_X), message))
    emu.stop(1)
    return false
end

local function zombieAt(x)
    for slot = 0, 3 do
        local enemyX = read(EN_X_LO + slot) + 256 * read(EN_X_HI + slot)
        if read(EN_TYPE + slot) == 1 and read(EN_Y + slot) == 176
                and (enemyX == x or enemyX == x - 1) then
            return true
        end
    end
    return false
end

local function turretAt(page, x)
    for slot = 0, 3 do
        if read(EN_TYPE + slot) == 2 then
            local enemyX = read(EN_X_LO + slot) + 256 * read(EN_X_HI + slot)
            if read(EN_PAGE + slot) == page and enemyX == x and read(EN_Y + slot) == 176 then
                return true
            end
        end
    end
    return false
end

-- The arcade walk is 288/256 px per frame, so fire every 12 frames to clear enemies in time.
local function fireAhead(x)
    if frame % 12 ~= 0 then return false end
    for slot = 0, 3 do
        local distance = read(EN_X_LO + slot) + 256 * read(EN_X_HI + slot) - x
        if read(EN_TYPE + slot) ~= 0 and distance > 0 and distance <= 92 then
            return true
        end
    end
    return false
end

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 2000, "route timed out") then return end

    local x = readWord(PLAYER_X)
    local camera = readWord(CAMERA_X)
    local page = read(CAMERA_X + 1)
    local state = read(AP_STATE)
    local scene = read(SCENE)
    local ready = scene == 4 and read(0x8000) == 0x8c
    if phase == "title" then
        if frame == 128 then
            input.start = true
            phase = "start"
        end
    elseif phase == "start" then
        if ready and read(SCHED_GROUP) == 1 then
            if not check(x == 32 and camera == 0 and read(CHECKPOINT) == 0 and read(LIVES) == 3
                    and read(SCHED_PAGE) == 0, "page 0 did not initialize at the start") then return end
            phase = "fall"
        end
    elseif phase == "fall" then
        input = {right = true, b = fireAhead(x)}
        if page == 0 then
            seen.startZombie = seen.startZombie or zombieAt(225)
            if not check(not turretAt(1, 492), "page 1 turret appeared before scrolling") then return end
        elseif page == 1 and read(SCHED_PAGE) == 1 and read(SCHED_GROUP) == 1 then
            seen.middleTurret = seen.middleTurret or turretAt(1, 492)
            seen.middleZombie = seen.middleZombie or zombieAt(600)
        end
        if state == 4 then
            if not check(seen.startZombie and seen.middleTurret and seen.middleZombie,
                    "missing page 0 or 1 enemy spawn") then return end
            if not check(page == 1 and x >= 576 and x < 592 and read(PLAYER_FEET) >= 232
                    and read(CHECKPOINT) == 1 and read(LIVES) == 2,
                    "expected a pit miss after the page 1 checkpoint") then return end
            phase = "retry"
            input = {start = true}
        end
    elseif phase == "retry" then
        -- With the screen off, the restart sets the player a frame or two before it clears
        -- the enemies, and the first update after it can already spawn the page's turret.
        -- So the pool must be seen empty at some point before the screen comes back on.
        if ready and state == 0 then
            local empty = true
            for slot = 0, 3 do
                if read(EN_TYPE + slot) ~= 0 then empty = false end
            end
            seen.emptyPool = seen.emptyPool or empty
            if emu.getState()["ppu.mask.backgroundEnabled"] then
                if not check(x == 416 and read(PLAYER_FEET) == 176 and camera == 256
                        and read(CHECKPOINT) == 1 and read(LIVES) == 2,
                        "checkpoint camera or player state did not reset") then return end
                if not check(seen.emptyPool, "enemy pool was not reset") then return end
                phase = "respawn"
            end
        end
    elseif phase == "respawn" then
        if turretAt(1, 492) then
            if not check(camera == 256 and read(SCHED_PAGE) == 1 and read(SCHED_GROUP) == 1,
                    "checkpoint group did not activate from its camera page") then return end
            phase = "finish"
        end
    elseif phase == "finish" then
        if not check(state ~= 4, "unexpected miss after restarting") then return end
        if page == 2 and read(SCHED_PAGE) == 2 and read(SCHED_GROUP) == 1 then
            seen.lastTurret = seen.lastTurret or turretAt(2, 760)
            seen.lastZombie = seen.lastZombie or zombieAt(753)
        else
            if not check(not turretAt(2, 760), "page 2 turret appeared before scrolling") then return end
            seen.retryZombie = seen.retryZombie or zombieAt(600)
        end
        -- Wait well short of the goal for the last group to spawn, while defending.
        input = {right = x < 710 or (seen.lastTurret and seen.lastZombie), b = fireAhead(x)}
        -- The walk skips a pixel every 8 frames, so take off anywhere in a 4 px window.
        if x >= 571 and x <= 574 and state == 0 then input.a = true end
        if scene == 5 then
            if not check(x == 724 and read(PLAYER_FEET) == 176 and read(LIVES) == 2
                    and seen.retryZombie and seen.lastTurret and seen.lastZombie,
                    "goal reached without the expected encounters after restart") then return end
            clearFrame = frame
            phase = "clear"
        end
    elseif phase == "clear" and frame == clearFrame + 4 then
        for index, tile in ipairs(clearTitle) do
            if not check(emu.read(0x2189 + index, emu.memType.nesPpuDebug) == tile,
                    "COURSE CLEAR title missing") then return end
        end
        print("PASS: three camera pages, page-linked enemies, checkpoint reset, and course clear")
        emu.stop(0)
    end
end, emu.eventType.endFrame)
