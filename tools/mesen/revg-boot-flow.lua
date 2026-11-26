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

local TITLE, PLAY, CLEAR, GAME_OVER = 7, 4, 5, 6
local STATE_MISS = 4

local frame, phase, phaseFrame = 0, "boot", 0
local input = {}
local deaths = 0
local sawStartSe = false
local sawTitleSong = false
local sawOpenMap = false
local sawRetryMap = false
local blackoutFrames = 0
local openingOamMax = 0
local oobVramWrites = 0
local ppuRendering = false
local zombieSeen = false

emu.addMemoryCallback(function(_, v)
    ppuRendering = ((v & 0x18) == 0x18)
end, emu.callbackType.write, 0x2001, 0x2001, emu.memType.nesMemory)

emu.addMemoryCallback(function()
    if ppuRendering then
        local sl = emu.getState()["ppu.scanline"]
        if sl >= 0 and sl < 241 then
            oobVramWrites = oobVramWrites + 1
        end
    end
end, emu.callbackType.write, 0x2007, 0x2007, emu.memType.nesMemory)

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
            if not check(read(SND_SONG_ID) == 0, "title screen must be silent like the arcade cabinet") then return end
            input = {start = true}
            nextPhase("opening")
        end
    elseif phase == "opening" then
        if read(SND_SONG_ID) == 1 then sawTitleSong = true end
        if read(SND_SE_ID) == 11 then sawStartSe = true end
        local demo = read(DEMO_ACTIVE)
        if demo == 1 and phaseFrame >= 30 and phaseFrame <= 450 then
            local oam = 0
            for i = 0, 63 do if read(0x200 + i * 4) < 0xef then oam = oam + 1 end end
            if oam > openingOamMax then openingOamMax = oam end
        end
        local pal1 = emu.read(1, emu.memType.nesPaletteRam)
        if phaseFrame > 515 and demo == 1 and pal1 == 0x0f then
            blackoutFrames = blackoutFrames + 1
        end
        if phaseFrame > 500 and demo == 0 and scene == CLEAR then
            if not check(sawTitleSong and sawStartSe and blackoutFrames >= 35 and openingOamMax >= 8,
                    string.format("opening -> blackout (%dF) -> Makai Map (scene=%d song=%s se=%s oamMax=%d)",
                        blackoutFrames, scene, tostring(sawTitleSong), tostring(sawStartSe), openingOamMax)) then return end
            nextPhase("open_map")
        elseif not check(phaseFrame < 700, string.format("opening did not reach Makai Map (scene=%d demo=%d)", scene, demo)) then return end
    elseif phase == "open_map" then
        if scene == CLEAR and read(SND_SONG_ID) == 11 then sawOpenMap = true end
        if phaseFrame == 30 then
            if not check(sawOpenMap, "Makai Map $30 jingle (song 11) did not play after opening") then return end
            input = {start = true}
        elseif scene == PLAY then
            nextPhase("stage")
        elseif not check(phaseFrame < 400, "Makai Map after opening did not enter Stage 1") then return end
    elseif phase == "stage" then
        if read(SND_SONG_ID) == 2 then
            local hud1up = (readNt(0x2022) == 0x1c and readNt(0x2023) == 0x14 and readNt(0x2024) == 0x10)
            local chr00Clean = true
            for b = 0, 15 do
                if emu.read(b, emu.memType.nesChrRam) ~= 0 then chr00Clean = false end
            end
            if not check(oobVramWrites == 0 and chr00Clean,
                    string.format("Stage 1 sky/VBlank corrupted (oob=%d chr00Clean=%s)",
                        oobVramWrites, tostring(chr00Clean))) then return end
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
        if scene == CLEAR and read(SND_SONG_ID) == 11 then sawRetryMap = true end
        if scene == CLEAR and phaseFrame >= 70 and (phaseFrame % 30) == 10 then
            input = {start = true}
        elseif read(AP_STATE) ~= STATE_MISS and read(SCENE) == PLAY and readWord(PLAYER_X) == 96 then
            if not check(sawRetryMap and read(LIVES) == 3 - deaths,
                    string.format("retry after miss (sawRetryMap=%s lives=%d)", tostring(sawRetryMap), read(LIVES))) then return end
            nextPhase("zombies")
        end
    elseif phase == "game_over" then
        if scene == GAME_OVER and phaseFrame >= 20 then
            if not check(read(SND_SONG_ID) == 5, "GAME OVER BGM after third miss") then return end
            input = {start = true}
            nextPhase("to_title")
        end
    elseif phase == "to_title" then
        if scene == TITLE then
            print(string.format("PASS: title, opening+blackout(%dF), openMap, Stage 1, 3 misses+retryMap, GAME OVER, title (frame %d)",
                blackoutFrames, frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
