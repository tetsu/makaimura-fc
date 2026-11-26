-- End-to-end verification of:
-- 1. Title Attract Demo (600F idle auto-launch, scripted play, button exit to Title)
-- 2. Z80-decoded Jump ($04, SE 14) & Landing ($05, SE 15) SEs
-- 3. Visual Makai Stage Progression Map on Stage 1 Clear (landmarks + Arthur marker)
-- 4. Name Entry (scene 15) with authentic Z80 BGM ($26, Song 10), 3-letter input,
--    High Score table insertion & sorting, and TOP SCORE sync.
local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local LIVES = 0x15
local PLAYER_X = 0x4c
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local KEY_COLLECTED = 0x60ab
local AP_SCORE = 0x60c4
local AP_TOP_SCORE = 0x60fe
local AP_CLEAR_SEQ = 0x6102
local HS_MAGIC = 0x6eb6
local HS_SCORES = 0x6eb7
local HS_NAMES = 0x6ecb
local ATTRACT_ACTIVE = 0x6ee1
local SND_SONG_ID = 0x7800
local SND_SE_ID = 0x7803

local TITLE, PLAY, CLEAR, GAME_OVER, NAME_ENTRY = 7, 4, 5, 6, 15

local frame, step, sf = 0, 1, 0
local input = {}
local sawJumpSe, sawLandSe, sawStartSe = false, false, false
local attractStartX = 0

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end
local function readNt(addr) return emu.read(addr - 0x2000, emu.memType.nesNametableRam) end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d step=%d sf=%d scene=%d song=%d se=%d: %s",
        frame, step, sf, read(SCENE), read(SND_SONG_ID), read(SND_SE_ID), msg))
    emu.stop(1)
    return false
end

local function nextStep()
    step = step + 1
    sf = 0
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    sf = sf + 1
    input = {}
    if not check(frame < 4000, "timed out") then return end
    local scene = read(SCENE)

    if step == 1 then
        -- Wait on Title screen for 600F idle -> Attract Demo auto-launch.
        if scene == PLAY and read(ATTRACT_ACTIVE) == 1 then
            if not check(frame >= 600 and read(CAMPAIGN_STAGE) == 1, "Attract Demo launched Stage 1 after idle") then return end
            attractStartX = readWord(PLAYER_X)
            nextStep()
        elseif not check(frame < 850, "Attract Demo did not start after 600F idle") then return end
    elseif step == 2 then
        -- Let Attract Demo run 60 frames and verify Arthur moves autonomously, then press A to exit.
        if sf == 60 then
            if not check(readWord(PLAYER_X) > attractStartX + 20, "Attract Demo did not move Arthur") then return end
            if not check(read(SND_SONG_ID) == 0, "Attract Demo should stay silent") then return end
            input = {a = true}
        elseif sf == 76 then
            if not check(scene == TITLE and read(ATTRACT_ACTIVE) == 0, "Button press did not exit Attract Demo to Title") then return end
            nextStep()
        end
    elseif step == 3 then
        -- Start Stage 1 (Right+START, skipping opening) and verify START SE (11) still plays.
        if sf == 4 then input = {right = true, start = true} end
        if sf >= 5 and read(SND_SE_ID) == 11 then sawStartSe = true end
        if sf == 24 then
            if not check(scene == PLAY and read(CAMPAIGN_STAGE) == 1 and sawStartSe,
                    string.format("Stage 1 start / START SE failed (scene=%d stg=%d se=%s)",
                        scene, read(CAMPAIGN_STAGE), tostring(sawStartSe))) then return end
            nextStep()
        end
    elseif step == 4 then
        -- Jump and land to verify Z80-decoded Jump SE (14) and Land SE (15).
        if sf == 4 then input = {a = true} end
        if read(SND_SE_ID) == 14 then sawJumpSe = true end
        if read(SND_SE_ID) == 15 then sawLandSe = true end
        if sf == 60 then
            if not check(sawJumpSe and sawLandSe,
                    string.format("Jump/Land SEs not heard (jump=%s land=%s)", tostring(sawJumpSe), tostring(sawLandSe))) then return end
            -- Trigger Stage 1 Clear -> Makai Stage Progression Map.
            write(KEY_COLLECTED, 1)
            write(AP_CLEAR_SEQ, 0)
            nextStep()
        end
    elseif step == 5 then
        if scene == CLEAR and sf >= 10 then
            -- Verify authentic arcade Makai Map: 'X' ($38) in "NEXT STAGE" at $2108,
            -- Arthur pin marker ($39) at $22cb (above Door 1), and panorama tiles ($40+) in rows 16..27.
            local mapTiles = 0
            for a = 0x2200, 0x237f do
                if readNt(a) >= 0x40 then mapTiles = mapTiles + 1 end
            end
            if not check(readNt(0x2108) == 0x38 and readNt(0x22cb) == 0x39 and mapTiles >= 100,
                    string.format("Authentic Makai Map missing ($2108=%02x $22cb=%02x mapTiles=%d)",
                        readNt(0x2108), readNt(0x22cb), mapTiles)) then return end
            input = {start = true}
            nextStep()
        elseif not check(sf < 200, "Stage 1 did not reach CLEAR map") then return end
    elseif step == 6 then
        if sf >= 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            -- Also verify the Right half of the authentic arcade Makai Map (Stages 5-7 Castle) via Stage 4 clear.
            write(CAMPAIGN_STAGE, 4)
            write(KEY_COLLECTED, 1)
            write(AP_CLEAR_SEQ, 0)
            nextStep()
        elseif not check(sf < 120, "START on Makai Map did not enter Stage 2") then return end
    elseif step == 7 then
        if scene == CLEAR and sf >= 10 then
            local mapTiles = 0
            for a = 0x2200, 0x237f do
                if readNt(a) >= 0x40 then mapTiles = mapTiles + 1 end
            end
            if not check(readNt(0x2108) == 0x38 and readNt(0x22d1) == 0x39 and mapTiles >= 140,
                    string.format("Authentic Makai Map (Right/Castle) missing ($2108=%02x $22d1=%02x mapTiles=%d)",
                        readNt(0x2108), readNt(0x22d1), mapTiles)) then return end
            input = {start = true}
            nextStep()
        elseif not check(sf < 200, "Stage 4 did not reach Right Makai Map") then return end
    elseif step == 8 then
        if scene == PLAY and read(CAMPAIGN_STAGE) == 5 then
            -- Set qualifying high score (025000 > default 1st 010000), 1 life, trigger Game Over.
            write(AP_SCORE + 0, 0)
            write(AP_SCORE + 1, 2)
            write(AP_SCORE + 2, 5)
            write(AP_SCORE + 3, 0)
            write(LIVES, 1)
            write(AP_ARMOR, 1)
            -- Set clock to 0:01 with subsecond=1 so next tick triggers time-up -> GAME OVER.
            write(0x17, 0x1b); write(0x18, 0x1b); write(0x19, 0x1c); write(0x16, 1)
            nextStep()
        elseif not check(sf < 120, "START on Right Makai Map did not enter Stage 5") then return end
    elseif step == 9 then
        if scene == GAME_OVER and sf >= 20 then
            input = {start = true}
            nextStep()
        elseif not check(sf < 300, "Did not reach GAME OVER") then return end
    elseif step == 10 then
        if scene == NAME_ENTRY and sf >= 12 then
            if not check(read(SND_SONG_ID) == 10, "Name Entry BGM ($26, Song 10) did not start") then return end
            nextStep()
        elseif not check(sf < 120, "Qualifying score on GAME OVER did not enter Name Entry") then return end
    elseif step == 11 then
        -- Verify SELECT is ignored during Name Entry, then enter "BCZ" and confirm.
        if sf == 2 then input = {select = true}
        elseif sf == 6 then
            if not check(scene == NAME_ENTRY, "SELECT should not exit Name Entry") then return end
            input = {up = true}
        elseif sf == 12 then input = {a = true}
        elseif sf == 18 then input = {up = true}
        elseif sf == 24 then input = {up = true}
        elseif sf == 30 then input = {a = true}
        elseif sf == 36 then input = {down = true}
        elseif sf == 42 then input = {start = true}
        elseif sf == 58 then
            if not check(scene == TITLE, "Name Entry confirm did not return to Title") then return end
            if not check(read(HS_MAGIC) == 0xa5, "High score table magic not set") then return end
            local scOk = (read(HS_SCORES+0) == 0 and read(HS_SCORES+1) == 2 and read(HS_SCORES+2) == 5 and read(HS_SCORES+3) == 0)
            local nmOk = (read(HS_NAMES+0) == 1 and read(HS_NAMES+1) == 2 and read(HS_NAMES+2) == 25)
            local shiftOk = (read(HS_SCORES+4) == 0 and read(HS_SCORES+5) == 1 and read(HS_SCORES+6) == 0 and read(HS_SCORES+7) == 0
                and read(HS_NAMES+3) == 2 and read(HS_NAMES+4) == 0 and read(HS_NAMES+5) == 15)
            local topOk = (read(AP_TOP_SCORE+0) == 0 and read(AP_TOP_SCORE+1) == 2 and read(AP_TOP_SCORE+2) == 5 and read(AP_TOP_SCORE+3) == 0)
            if not check(scOk and nmOk and shiftOk and topOk,
                    string.format("High score saved mismatch sc=%s nm=%s shift=%s top=%s",
                        tostring(scOk), tostring(nmOk), tostring(shiftOk), tostring(topOk))) then return end
            print(string.format("PASS frame=%d (AttractDemo/JumpLandSE/MakaiMap/NameEntry+Song10/HighScoreShift)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
