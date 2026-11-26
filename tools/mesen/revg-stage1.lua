-- Stage 1 with the real Revision G section 0 layout (the game build, ./build.ps1 -Hardware).
-- Drive the full Stage 1 route with controller input: tombstones, hill ladder, lake raft,
-- checkpoint miss & restart, forest pits, Unicorn boss entrance & defeat, key grab,
-- celebration + clear music, STAGE1 CLEAR, and return to title.
-- Keeps non-boss regular enemies cleared so the route test is deterministic.
local SCENE = 0x07
local LIVES = 0x15
local CAMERA_X = 0x25
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local CHECKPOINT = 0x70
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local PLAT_X_LO = 0x6042
local PLAT_X_HI = 0x6046
local PLAT_PHASE = 0x605a
local KEY_COLLECTED = 0x60ab
local OB_KIND = 0x699b
local OB_FLAGS = 0x6b8f
local SND_SONG_ID = 0x7800

local TITLE, PLAY, CLEAR = 7, 4, 5
local STATE_GROUND, STATE_LADDER, STATE_MISS = 0, 2, 4
local FLOOR, HILL, RAFT_FEET = 192, 112, 194
local TOMBS = {[FLOOR] = {256, 416, 528, 1264, 1520}, [HILL] = {768, 864, 960}}
local PITS = {1952, 2016, 2464, 2784}
local LADDER_X = 720
local LAKE_STOP = 1628
local RAFT_BOARD = 1712
local RAFT_LEAVE = 1772
local CHECKPOINT_X = 1920

local frame = 0
local phase = "boot"
local input = {}
local clearFrame
local climbing = false
local airborne = false
local jumpFrame
local bossFrame
local sawBoss = false
local sawKey = false
local sawClearSong = false

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s x=%d feet=%d camera=%d: %s", frame, phase,
        readWord(PLAYER_X), read(PLAYER_FEET), readWord(CAMERA_X), message))
    emu.stop(1)
    return false
end

local function jumpWindow(x, edges, before, after)
    for _, edge in ipairs(edges) do
        if x >= edge - before and x <= edge - after then return true end
    end
    return false
end

local function clearRegularEnemies()
    for i = 0, 19 do
        local k = read(OB_KIND + i)
        -- Keep Unicorn (7), Key (15), Boom (11), Popup (12)
        if k ~= 0 and k ~= 7 and k ~= 15 and k ~= 11 and k ~= 12 then
            write(OB_KIND + i, 0)
            write(OB_FLAGS + i, 0)
        end
    end
end

local function findKind(kind)
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then return i end
    end
    return nil
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 6500, "route timed out") then return end

    local scene = read(SCENE)
    if scene == PLAY then clearRegularEnemies() end

    local x = readWord(PLAYER_X)
    local feet = read(PLAYER_FEET)
    local state = read(AP_STATE)
    local camera = readWord(CAMERA_X)
    local raftX = read(PLAT_X_LO) + 256 * read(PLAT_X_HI)
    local raftLeft = read(PLAT_PHASE) == 1
    local onGround = state == STATE_GROUND
    if phase ~= "pit" and phase ~= "retry" and phase ~= "boss" and not check(state ~= STATE_MISS, "unexpected miss") then return end

    if phase == "boot" then
        if frame == 128 then
            input = {start = true, right = true}
            phase = "start"
        end
    elseif phase == "start" then
        if scene == PLAY and read(0x8000) == 0x8c and onGround and raftX ~= 0 then
            if not check(x == 96 and feet == FLOOR and camera == 0 and read(LIVES) == 3
                    and read(CHECKPOINT) == 0, "arcade start position") then return end
            phase = "graveyard"
        end
    elseif phase == "graveyard" then
        input = {right = true, a = onGround and jumpWindow(x, TOMBS[FLOOR], 16, 14)}
        if onGround and x >= LADDER_X - 2 and x < LADDER_X + 6 then
            input = {up = true}
            phase = "ladder"
        end
    elseif phase == "ladder" then
        input = {up = true}
        climbing = climbing or state == STATE_LADDER
        if climbing and state == STATE_GROUND then
            if not check(x == LADDER_X and feet == HILL, "top of the first hill ladder") then return end
            phase = "hill"
        end
    elseif phase == "hill" then
        input = {right = true, a = onGround and feet == HILL and jumpWindow(x, TOMBS[HILL], 16, 14)}
        if onGround and feet == FLOOR then
            if not check(x >= 1120 and x < 1160, "drop from the end of the hill") then return end
            phase = "lake_approach"
        end
    elseif phase == "lake_approach" then
        input = {right = x < LAKE_STOP, a = onGround and jumpWindow(x, TOMBS[FLOOR], 16, 14)}
        if x >= LAKE_STOP and onGround and raftLeft and raftX == RAFT_BOARD then
            jumpFrame = frame + 2
            phase = "board"
        end
    elseif phase == "board" then
        input = {right = true, a = frame == jumpFrame}
        airborne = airborne or not onGround
        if airborne and onGround then
            if not check(feet == RAFT_FEET and math.abs(x - raftX) <= 12, "landing on the raft") then return end
            phase = "raft"
        end
    elseif phase == "raft" then
        if not check(feet == RAFT_FEET, "fell off the raft") then return end
        if onGround and not raftLeft and raftX >= RAFT_LEAVE then
            jumpFrame = frame + 2
            phase = "far_bank"
        end
    elseif phase == "far_bank" then
        input = {right = true, a = frame == jumpFrame}
        if onGround and feet == FLOOR then
            if not check(x >= 1792, "far bank of the lake") then return end
            phase = "checkpoint"
        end
    elseif phase == "checkpoint" then
        input = {right = true}
        if x >= CHECKPOINT_X then
            if not check(read(CHECKPOINT) == 1, "checkpoint not reached at X 1920") then return end
            phase = "pit"
        end
    elseif phase == "pit" then
        input = {right = true}
        if state == STATE_MISS then
            if not check(read(LIVES) == 2 and x >= PITS[1] and x < PITS[1] + 32, "miss in the first pit") then return end
            input = {start = true}
            phase = "retry"
        end
    elseif phase == "retry" then
        if scene == PLAY and read(0x8000) == 0x8c and onGround and state ~= STATE_MISS then
            if not check(x == CHECKPOINT_X and feet == FLOOR and camera == 1792 and read(LIVES) == 2,
                    "checkpoint restart position, camera or lives") then return end
            phase = "forest"
        end
    elseif phase == "forest" then
        input = {right = true, a = onGround and jumpWindow(x, PITS, 5, 4)}
        if camera >= 3248 and findKind(7) ~= nil then
            if not check(read(SND_SONG_ID) == 3, "boss music on Page 12") then return end
            sawBoss = true
            bossFrame = frame
            phase = "boss"
        end
    elseif phase == "boss" then
        -- Fight Unicorn deterministically (keep armor full while testing boss -> key -> clear)
        write(AP_ARMOR, 2)
        local bslot = findKind(7)
        if not check(frame < bossFrame + 2400,
                bslot and string.format("boss not beaten (bx=%d by=%d st=%d hp=%d fl=%02x)",
                    read(0x6a27 + bslot) + 256 * read(0x6a3b + bslot),
                    read(0x6a63 + bslot) + 256 * read(0x6a77 + bslot),
                    read(0x69af + bslot), read(0x6b67 + bslot), read(0x6b8f + bslot)) or "boss not beaten (no slot)") then return end
        if bslot ~= nil then
            local bx = read(0x6a27 + bslot) + 256 * read(0x6a3b + bslot)
            local fire = (frame % 6 == 0)
            local hop = (frame % 37 == 0)
            if bx > x + 80 then
                input = {right = true, b = fire, a = hop}
            elseif bx < x - 80 then
                input = {left = true, b = fire, a = hop}
            elseif bx > x then
                input = {right = (frame % 20 == 1), b = fire, a = hop}
            else
                input = {left = (frame % 20 == 1), b = fire, a = hop}
            end
        else
            if findKind(15) ~= nil then
                sawKey = true
                phase = "key"
            elseif x < 3300 then
                -- Arcade $72CC: advance toward the gate (X >= 3280) to trigger key drop.
                input = {right = true}
            end
        end
    elseif phase == "key" then
        if read(SND_SONG_ID) == 4 then sawClearSong = true end
        local kslot = findKind(15)
        if kslot ~= nil then
            local kx = read(0x6a27 + kslot) + 256 * read(0x6a3b + kslot)
            if x < kx - 4 then input = {right = true}
            elseif x > kx + 4 then input = {left = true} end
        end
        if scene == CLEAR then
            local cam = readWord(CAMERA_X)
            local gateOpen = (emu.read(0x06f3, emu.memType.nesNametableRam) == 0 and
                              emu.read(0x0653, emu.memType.nesNametableRam) == 0)
            if not check(sawBoss and sawKey and sawClearSong and gateOpen and read(KEY_COLLECTED) == 1 and cam == 3328,
                    string.format("clear after taking the key with gate open, clear song and cam==3328 (cam=%d gateOpen=%s)",
                        cam, tostring(gateOpen))) then return end
            clearFrame = frame
            phase = "clear"
        end
    elseif phase == "clear" then
        if frame == clearFrame + 20 then
            input = {start = true}
        elseif frame == clearFrame + 35 then
            if not check(scene == PLAY and read(0x14) == 2 and read(0x0d) == 13,
                    "START after Stage 1 clear advances to Stage 2 (bank 13)") then return end
            print(string.format("PASS: Stage 1 full route, ladder, raft, checkpoint, Unicorn, key, clear song, -> Stage 2 (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
