-- Boot to play in the game build (./build.ps1 -Hardware), with controller input only (TODO C-1a).
-- Power on -> title -> START -> opening (text) -> map (text) -> Stage 1 on the real map.
-- Then stand at the first tombstone holding right until the placeholder zombies use up
-- all three lives -> GAME OVER -> START -> title. The other route (boss, key, clear,
-- back to the title) is in revg-stage1.lua.
local SCENE = 0x07
local LIVES = 0x15
local CAMERA_X = 0x25
local PLAYER_X = 0x4c
local AP_STATE = 0x6010
local EN_TYPE, EN_X_LO, EN_X_HI = 0x6079, 0x607d, 0x6081
local SND_SONG_ID = 0x7800

local TITLE, OPENING, MAP, PLAY, GAME_OVER = 7, 12, 13, 4, 6
local STATE_MISS = 4
local OPENING_FRAMES, MAP_FRAMES = 240, 150
local ZOMBIE_AHEAD = 272            -- Camera X + 128 + 144, as for the arcade's first zombie

local frame, phase, phaseFrame = 0, "boot", 0
local input = {}
local deaths = 0
local zombieSeen = false
-- Each screen is drawn with the display off, and how many frames that spans moves with
-- the code layout (245 frames for the opening once the fixed bank grew), so allow a few.
local SLACK = 8

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function readNt(addr) return emu.read(addr - 0x2000, emu.memType.nesNametableRam) end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s scene=%d: %s", frame, phase, read(SCENE), message))
    emu.stop(1)
    return false
end

-- Glyph tiles of the text screens (the order of `glyphs` in src/vrc7a-test.s). Digits
-- without a glyph use the HUD digits from tile $1b (fe_glyph in src/frontend.inc).
local GLYPHS = "7ABCDEFGHIKLMNOPRSTUVW+YJ2"
local function textAt(addr, text)
    for i = 1, #text do
        local c = text:sub(i, i)
        local tile = c == " " and 0 or GLYPHS:find(c, 1, true) or 0x1b + tonumber(c)
        if readNt(addr + i - 1) ~= tile then return false end
    end
    return true
end

local function nextPhase(name)
    phase, phaseFrame = name, 0
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}
    if not check(frame < 6000, "timed out") then return end
    local scene = read(SCENE)

    if phase == "boot" then
        if frame == 128 then
            if not check(scene == TITLE and textAt(0x20ca, "MAKAIMURA FC")
                    and textAt(0x228a, "STAGE 1 ONLY"), "title screen after power on") then return end
            input = {start = true}
            nextPhase("opening")
        end
    elseif phase == "opening" then
        if phaseFrame == 4 then
            if not check(scene == OPENING and textAt(0x216a, "THE PRINCESS")
                    and read(SND_SONG_ID) == 1, "opening after START") then return end
        elseif scene ~= OPENING and phaseFrame > 4 then
            if not check(scene == MAP and math.abs(phaseFrame - OPENING_FRAMES) <= SLACK
                    and textAt(0x218c, "STAGE 1") and textAt(0x21cb, "GRAVEYARD"),
                    string.format("map after the opening (%d frames)", phaseFrame)) then return end
            nextPhase("map")
        end
    elseif phase == "map" then
        if scene ~= MAP then
            if not check(scene == PLAY and read(0x8000) == 0x8c and math.abs(phaseFrame - MAP_FRAMES) <= SLACK,
                    string.format("Stage 1 after the map (%d frames)", phaseFrame)) then return end
            nextPhase("stage")
        end
    elseif phase == "stage" then
        -- Drawing the stage with the display off takes a few frames.
        if read(SND_SONG_ID) == 2 then
            if not check(readWord(PLAYER_X) == 96 and read(LIVES) == 3,
                    string.format("Stage 1 start: x=%d lives=%d", readWord(PLAYER_X), read(LIVES))) then return end
            nextPhase("zombies")
        elseif not check(phaseFrame < 60, "Stage 1 music did not start") then return end
    elseif phase == "zombies" then
        -- Walk into the first tombstone and wait there for the zombies.
        input = {right = true}
        for slot = 0, 3 do
            if not zombieSeen and read(EN_TYPE + slot) ~= 0 then
                local ahead = read(EN_X_LO + slot) + 256 * read(EN_X_HI + slot) - readWord(CAMERA_X)
                if not check(ahead == ZOMBIE_AHEAD or ahead == ZOMBIE_AHEAD - 1,
                        string.format("zombie appeared %d px from the camera", ahead)) then return end
                zombieSeen = true
            end
        end
        if read(AP_STATE) == STATE_MISS then
            deaths = deaths + 1
            if not check(zombieSeen, "miss without a zombie") then return end
            nextPhase(deaths < 3 and "retry" or "game_over")
        end
    elseif phase == "retry" then
        if phaseFrame == 20 then
            if not check(read(LIVES) == 3 - deaths, "lives after a miss") then return end
            input = {start = true}
        elseif phaseFrame == 30 then
            if not check(scene == PLAY and readWord(PLAYER_X) == 96, "restart at the stage start") then return end
            nextPhase("zombies")
        end
    elseif phase == "game_over" then
        if phaseFrame == 20 then
            if not check(scene == GAME_OVER and textAt(0x218a, "GAME OVER") and read(SND_SONG_ID) == 5,
                    "GAME OVER after the third miss") then return end
            input = {start = true}
        elseif phaseFrame == 30 then
            if not check(scene == TITLE and textAt(0x20ca, "MAKAIMURA FC"), "title after GAME OVER") then return end
            print(string.format("PASS: title, opening, map, Stage 1, three misses, GAME OVER, title (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
