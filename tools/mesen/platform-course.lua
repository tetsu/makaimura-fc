-- Normal 512 KiB game mode with the original -PlatformCourse fixture.
-- Drive the entire route with controller input. Read gameplay state for assertions.
-- Zero page from vrc7a-test.s and playable-scene.inc, WRAM from arcade-player.inc.
local SCENE = 0x07
local LIVES = 0x15
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local CHECKPOINT = 0x70
local AP_STATE = 0x6010
local AP_RIDING = 0x601b
local PLAT_X = 0x6042
local LIFT_Y = 0x604b
local HORIZONTAL_PHASE = 0x605a

local frame = 0
local phase = "boot"
local clearFrame
local input = {}
local clearTitle = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s: %s", frame, phase, message))
    emu.stop(1)
    return false
end

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 900, "route timed out") then return end

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
            for index, tile in ipairs(clearTitle) do
                if not check(emu.read(0x2189 + index, emu.memType.nesPpuDebug) == tile,
                        "COURSE CLEAR title missing") then return end
            end
            print("PASS: checkpoint restart, horizontal-to-vertical transfer, ladder, course clear")
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
