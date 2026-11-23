-- Stage 1 with the real Revision G section 0 layout (the game build, ./build.ps1 -Hardware).
-- Drive the route with controller input only: jump the tombstones, climb the first hill
-- ladder, cross the lake on the raft, miss once after the checkpoint and restart there,
-- jump the forest pits, beat the placeholder boss at the gate, take the key and return
-- to the title. Positions come from build/revg-section0-map.inc
-- (tools/convert-revg-section.py). Right+START skips the opening and the map, which
-- revg-boot-flow.lua checks. Like the MAME traces, the test keeps the zombies out: it
-- holds the live-zombie count at the cap so the scheduler places none.
local SCENE = 0x07
local LIVES = 0x15
local CAMERA_X = 0x25
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local CHECKPOINT = 0x70
local AP_STATE = 0x6010
local PLAT_X_LO = 0x6042
local PLAT_X_HI = 0x6046
local PLAT_PHASE = 0x605a
local SCHED_ZOMBIES_LIVE = 0x6077   -- sched_zombies_live in src/arcade-player.inc
local ZOMBIE_CAP = 2
local EB_ACTIVE, EB_X_LO, EB_X_HI, EB_Y = 0x6095, 0x6097, 0x6099, 0x609b
local BOSS_STATE, BOSS_HP = 0x60a1, 0x60a3
local KEY_COLLECTED = 0x60ab
local BOSS_ACTIVE, BOSS_DEFEATED = 1, 2
local TITLE, CLEAR = 7, 5

local STATE_GROUND, STATE_LADDER, STATE_MISS = 0, 2, 4
local FLOOR, HILL = 192, 112
-- Left edges of the solid tombstone cells, by the height the player walks at.
local TOMBS = {[FLOOR] = {256, 416, 528, 1264, 1520}, [HILL] = {768, 864, 960}}
-- Left edges of the 32 px pits after the lake.
local PITS = {1952, 2016, 2464, 2784}
local LADDER_X = 712            -- Centre of the first ladder's left column (44)
local LAKE_STOP = 1628          -- Stand back from the lake edge (column 103 is water)
local RAFT_BOARD = 1712         -- Raft X, heading left, that meets a jump from the edge
local RAFT_LEAVE = 1772         -- Raft X, heading right, to jump to the far bank
local CHECKPOINT_X = 1920
local BOSS_TRIGGER_X, BOSS_HP_START, CAMERA_END = 3392, 10, 3328
local LOW_BOLT_Y = 183              -- Boss feet 192 - 9: hits a standing or crouching player

local frame = 0
local phase = "boot"
local input = {}
local clearFrame
local rendered = false
local climbing = false
local airborne = false
local jumpFrame
local bossFrame
local sawBolt = false

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

-- True while x is in the window where a jump clears the obstacle starting at edge.
local function jumpWindow(x, edges, before, after)
    for _, edge in ipairs(edges) do
        if x >= edge - before and x <= edge - after then return true end
    end
    return false
end

-- Background tile at a world column on the floor row, read from the nametable.
-- The playfield of a one-page-high stage is in nametable $2000; the ring has 32 columns.
local function floorTile(worldX)
    local column = (worldX // 8) % 32
    return emu.read(0x2000 + 24 * 32 + column, emu.memType.nesPpuDebug)
end

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

-- True when a low boss bolt is close enough that a straight jump now clears it.
local function lowBoltAhead(x)
    for i = 0, 1 do
        if read(EB_ACTIVE + i) ~= 0 then
            sawBolt = true
            local dx = read(EB_X_LO + i) + 256 * read(EB_X_HI + i) - x
            if read(EB_Y + i) == LOW_BOLT_Y and dx > 0 and dx < 40 then return true end
        end
    end
    return false
end

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    emu.write(SCHED_ZOMBIES_LIVE, ZOMBIE_CAP, emu.memType.nesMemory)
    if not check(frame < 5000, "route timed out") then return end

    local x = readWord(PLAYER_X)
    local feet = read(PLAYER_FEET)
    local state = read(AP_STATE)
    local camera = readWord(CAMERA_X)
    local raftX = read(PLAT_X_LO) + 256 * read(PLAT_X_HI)
    local raftLeft = read(PLAT_PHASE) == 1
    local onGround = state == STATE_GROUND
    if phase ~= "pit" and phase ~= "retry" and not check(state ~= STATE_MISS, "unexpected miss") then return end

    if phase == "boot" then
        if frame == 128 then
            input = {start = true, right = true}
            phase = "start"
        end
    elseif phase == "start" then
        if read(SCENE) == 4 and read(0x8000) == 0x8c and onGround and raftX ~= 0 then
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
        -- The player reads the controller one frame late, so the climb starts a frame later.
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
        -- A jump only goes forward once the direction has been held for two updates.
        input = {right = true, a = frame == jumpFrame}
        airborne = airborne or not onGround
        if airborne and onGround then
            if not check(feet == 218 and math.abs(x - raftX) <= 12, "landing on the raft") then return end
            phase = "raft"
        end
    elseif phase == "raft" then
        if not check(feet == 218, "fell off the raft") then return end
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
        input = {right = true}  -- Walk into the first forest pit.
        if state == STATE_MISS then
            if not check(read(LIVES) == 2 and x >= PITS[1] and x < PITS[1] + 32, "miss in the first pit") then return end
            input = {start = true}
            phase = "retry"
        end
    elseif phase == "retry" then
        if read(SCENE) == 4 and read(0x8000) == 0x8c and onGround then
            if not check(x == CHECKPOINT_X and feet == FLOOR and camera == 1792 and read(LIVES) == 2,
                    "checkpoint restart position, camera or lives") then return end
            phase = "forest"
        end
    elseif phase == "forest" then
        -- Take off with the right foot (X + 4) still on the edge.
        input = {right = true, a = onGround and jumpWindow(x, PITS, 5, 4)}
        if camera >= 2304 and camera < 2432 and onGround and not rendered then
            -- Past page 8 the renderer needs bit 8 of the tile column: pit 2464 shows sky,
            -- the floor right of it shows grass.
            if not check(floorTile(2464) ~= floorTile(2496) and floorTile(2464) == floorTile(2472),
                    "forest pit at X 2464 not drawn") then return end
            rendered = true
        end
        if read(BOSS_STATE) == BOSS_ACTIVE then
            if not check(rendered and x >= BOSS_TRIGGER_X and read(BOSS_HP) == BOSS_HP_START,
                    "boss starts at the gate") then return end
            bossFrame = frame
            phase = "boss"
        end
    elseif phase == "boss" then
        -- Stand still facing the boss, shoot every 12 frames and jump the low bolts.
        if not check(frame < bossFrame + 900, "boss not beaten") then return end
        input = {b = frame % 12 == 0, a = onGround and lowBoltAhead(x)}
        if read(BOSS_STATE) == BOSS_DEFEATED then
            if not check(sawBolt and camera == CAMERA_END, "boss bolts and the camera at the end") then return end
            phase = "key"
        end
    elseif phase == "key" then
        input = {right = true}
        if read(SCENE) == CLEAR then
            if not check(read(KEY_COLLECTED) == 1 and read(LIVES) == 2, "clear after taking the key") then return end
            clearFrame = frame
            phase = "clear"
        end
    elseif phase == "clear" then
        if frame == clearFrame + 20 then
            input = {start = true}
        elseif frame == clearFrame + 30 then
            if not check(read(SCENE) == TITLE, "START after the clear returns to the title") then return end
            print(string.format("PASS: real section 0 layout, ladder, raft, checkpoint restart, boss, key, title (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
