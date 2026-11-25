-- Boot to play in the game build (./build.ps1 -Hardware), with controller input only.
-- Power on -> title -> START -> arcade opening cutscene + blackout -> Stage 1 (arcade HUD)
-- -> walk into first tombstone until real Stage 1 zombies use up all 3 lives -> GAME OVER -> title.
local SCENE = 0x07
local LIVES = 0x15
local PLAYER_X = 0x4c
local DEMO_ACTIVE = 0x0381
local AP_STATE = 0x6010
local OB_KIND = 0x698a
local SND_SONG_ID = 0x7800
local SND_SE_ID = 0x7803

local TITLE, PLAY, GAME_OVER = 7, 4, 6
local STATE_MISS = 4

local frame, phase, phaseFrame = 0, "boot", 0
local input = {}
local deaths = 0
local sawStartSe = false
local sawTitleSong = false
local blackoutFrames = 0
local zombieSeen = false

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function readNt(addr) return emu.read(addr - 0x2000, emu.memType.nesNametableRam) end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s scene=%d: %s", frame, phase, read(SCENE), message))
    emu.stop(1)
    return false
end

local function nextPhase(name)
    phase, phaseFrame = name, 0
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}
    if not check(frame < 7500, "timed out") then return end
    local scene = read(SCENE)

    if phase == "boot" then
        if frame == 128 then
            if not check(scene == TITLE, "title screen after power on") then return end
            input = {start = true}
            nextPhase("opening")
        end
    elseif phase == "opening" then
        if read(SND_SONG_ID) == 1 then sawTitleSong = true end
        if read(SND_SE_ID) == 11 then sawStartSe = true end
        local demo = read(DEMO_ACTIVE)
        local pal1 = emu.read(1, emu.memType.nesPaletteRam)
        if phaseFrame > 515 and demo == 1 and pal1 == 0x0f then
            blackoutFrames = blackoutFrames + 1
        end
        if phaseFrame > 500 and demo == 0 then
            if not check(scene == PLAY and sawTitleSong and sawStartSe and blackoutFrames >= 35,
                    string.format("opening -> blackout (%dF) -> Stage 1 (song=%s se=%s)",
                        blackoutFrames, tostring(sawTitleSong), tostring(sawStartSe))) then return end
            nextPhase("stage")
        end
    elseif phase == "stage" then
        if read(SND_SONG_ID) == 2 then
            local hud1up = (readNt(0x2022) == 0x1c and readNt(0x2023) == 0x14 and readNt(0x2024) == 0x10)
            if not check(readWord(PLAYER_X) == 96 and read(LIVES) == 3 and hud1up,
                    string.format("Stage 1 start: x=%d lives=%d hud1up=%s",
                        readWord(PLAYER_X), read(LIVES), tostring(hud1up))) then return end
            nextPhase("zombies")
        elseif not check(phaseFrame < 60, "Stage 1 music did not start") then return end
    elseif phase == "zombies" then
        -- Walk into the first tombstone and wait for real Stage 1 zombies to land hits.
        input = {right = true}
        for slot = 0, 19 do
            if read(OB_KIND + slot) == 1 then zombieSeen = true end
        end
        if read(AP_STATE) == STATE_MISS then
            deaths = deaths + 1
            if not check(zombieSeen, "miss without a zombie") then return end
            nextPhase(deaths < 3 and "retry" or "game_over")
        end
    elseif phase == "retry" then
        if read(AP_STATE) ~= STATE_MISS and read(SCENE) == PLAY and readWord(PLAYER_X) == 96 then
            if not check(read(LIVES) == 3 - deaths, "lives after a miss") then return end
            nextPhase("zombies")
        elseif phaseFrame == 50 then
            input = {start = true}
        end
    elseif phase == "game_over" then
        if scene == GAME_OVER and phaseFrame >= 20 then
            if not check(read(SND_SONG_ID) == 5, "GAME OVER BGM after third miss") then return end
            input = {start = true}
            nextPhase("to_title")
        end
    elseif phase == "to_title" then
        if scene == TITLE then
            print(string.format("PASS: title, opening+blackout(%dF), Stage 1, 3 misses, GAME OVER, title (frame %d)",
                blackoutFrames, frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
