-- End-to-end verification of Title Menu, Option Screen, Invincible Mode,
-- Stage/Loop/Weapon/Timer/Lives options, and Sound Test round-trip.
local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local LIVES = 0x15
local CLOCK_MIN = 0x17
local CLOCK_TENS = 0x18
local CLOCK_ONES = 0x19
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_WEAPON = 0x601a
local AP_LOOP = 0x60ce
local REVG_BG_ACTIVE = 0x6118
local OB_KIND = 0x699b

local TITLE, PLAY, SOUND_TEST, OPTION = 7, 4, 9, 14

local frame, step, stepFrame = 0, 1, 0
local input = {}
local zombieSeen = false
local startClock = nil

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readNt(addr) return emu.read(addr - 0x2000, emu.memType.nesNametableRam) end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d step=%d scene=%d: %s", frame, step, read(SCENE), msg))
    emu.stop(1)
    return false
end

local function nextStep()
    step = step + 1
    stepFrame = 0
end

-- Scripted button press helper: press on stepFrame == 4, check on stepFrame == 10.
local function pulse(btn, delay)
    delay = delay or 4
    if stepFrame == delay then input = btn end
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    stepFrame = stepFrame + 1
    input = {}
    if not check(frame < 4000, "timed out") then return end

    local scene = read(SCENE)

    if step == 1 then
        -- Wait for title screen after boot.
        if frame == 128 then
            if not check(scene == TITLE, "title screen on boot") then return end
            if not check(readNt(0x22e9) == 0xe9 and readNt(0x2309) == 0x00,
                    string.format("default title cursor on 1P START ($22e9=%02x $2309=%02x)",
                        readNt(0x22e9), readNt(0x2309))) then return end
            nextStep()
        end
    elseif step == 2 then
        -- Down moves cursor to OPTION.
        pulse({down = true}, 2)
        if stepFrame == 8 then
            if not check(readNt(0x22e9) == 0x00 and readNt(0x2309) == 0xe9,
                    string.format("title cursor moved to OPTION ($22e9=%02x $2309=%02x)",
                        readNt(0x22e9), readNt(0x2309))) then return end
            nextStep()
        end
    elseif step == 3 then
        -- START opens Option screen.
        pulse({start = true}, 2)
        if stepFrame == 10 then
            if not check(scene == OPTION, "entered Option screen") then return end
            nextStep()
        end
    elseif step == 4 then
        -- Row 0 (INVINCIBLE): Right -> ON.
        pulse({right = true}, 2)
        if stepFrame == 6 then nextStep() end
    elseif step == 5 then
        -- Move down to Row 3 (WEAPON): 3x Down.
        if stepFrame == 2 or stepFrame == 6 or stepFrame == 10 then input = {down = true} end
        if stepFrame == 14 then nextStep() end
    elseif step == 6 then
        -- WEAPON: 3x Right -> CROSS (3).
        if stepFrame == 2 or stepFrame == 6 or stepFrame == 10 then input = {right = true} end
        if stepFrame == 14 then nextStep() end
    elseif step == 7 then
        -- Row 4 (TIME LIMIT): Down, then Right -> OFF.
        if stepFrame == 2 then input = {down = true} end
        if stepFrame == 6 then input = {right = true} end
        if stepFrame == 10 then nextStep() end
    elseif step == 8 then
        -- Row 5 (LIVES): Down, then 2x Right -> 9.
        if stepFrame == 2 then input = {down = true} end
        if stepFrame == 6 or stepFrame == 10 then input = {right = true} end
        if stepFrame == 14 then nextStep() end
    elseif step == 9 then
        -- Row 6 (SOUND TEST): Down, then A -> enters Sound Test.
        if stepFrame == 2 then input = {down = true} end
        if stepFrame == 6 then input = {a = true} end
        if stepFrame == 16 then
            if not check(scene == SOUND_TEST, "entered Sound Test from Option") then return end
            nextStep()
        end
    elseif step == 10 then
        -- SELECT from Sound Test returns to Option screen.
        pulse({select = true}, 2)
        if stepFrame == 12 then
            if not check(scene == OPTION, "SELECT in Sound Test returned to Option") then return end
            nextStep()
        end
    elseif step == 11 then
        -- B exits Option back to Title (cursor reset to 1P START).
        pulse({b = true}, 2)
        if stepFrame == 12 then
            if not check(scene == TITLE, "B exited Option to Title") then return end
            if not check(readNt(0x22e9) == 0xe9, "title cursor reset to 1P START") then return end
            nextStep()
        end
    elseif step == 12 then
        -- Right+START skips opening and enters Stage 1 with configured options.
        pulse({right = true, start = true}, 2)
        if stepFrame == 20 then
            if not check(scene == PLAY and read(CAMPAIGN_STAGE) == 1, "started Stage 1") then return end
            if not check(read(LIVES) == 9 and read(AP_WEAPON) == 3,
                    string.format("options applied: lives=%d weapon=%d", read(LIVES), read(AP_WEAPON))) then return end
            startClock = {read(CLOCK_MIN), read(CLOCK_TENS), read(CLOCK_ONES)}
            nextStep()
        end
    elseif step == 13 then
        -- Walk into Stage 1 zombies for 360 frames while invincible + timer OFF.
        input = {right = true}
        for slot = 0, 19 do
            if read(OB_KIND + slot) == 1 then zombieSeen = true end
        end
        if not check(read(AP_ARMOR) == 2 and read(AP_STATE) < 3 and read(LIVES) == 9,
                string.format("invincible broken! armor=%d state=%d lives=%d",
                    read(AP_ARMOR), read(AP_STATE), read(LIVES))) then return end
        if stepFrame == 360 then
            if not check(zombieSeen, "zombies should spawn during invincibility test") then return end
            local sameClock = (read(CLOCK_MIN) == startClock[1] and
                               read(CLOCK_TENS) == startClock[2] and
                               read(CLOCK_ONES) == startClock[3])
            if not check(sameClock, "TIME LIMIT OFF should freeze clock") then return end
            nextStep()
        end
    elseif step == 14 then
        -- SELECT returns to Title.
        pulse({select = true}, 2)
        if stepFrame == 12 then
            if not check(scene == TITLE, "SELECT returned to Title") then return end
            nextStep()
        end
    elseif step == 15 then
        -- Open Option again: Down -> START.
        if stepFrame == 2 then input = {down = true} end
        if stepFrame == 8 then input = {start = true} end
        if stepFrame == 18 then
            if not check(scene == OPTION, "re-entered Option screen") then return end
            nextStep()
        end
    elseif step == 16 then
        -- Cursor was on row 6 (SOUND TEST) -> 3x Down wraps to row 1 (STAGE): 6->7->0->1.
        if stepFrame == 2 or stepFrame == 6 or stepFrame == 10 then input = {down = true} end
        -- STAGE: 4x Right -> Stage 5.
        if stepFrame == 14 or stepFrame == 18 or stepFrame == 22 or stepFrame == 26 then input = {right = true} end
        -- LOOP: Down -> Right -> Loop 2.
        if stepFrame == 30 then input = {down = true} end
        if stepFrame == 34 then input = {right = true} end
        -- B -> exit to Title.
        if stepFrame == 38 then input = {b = true} end
        if stepFrame == 48 then
            if not check(scene == TITLE, "returned to Title for Stage 5 Loop 2") then return end
            nextStep()
        end
    elseif step == 17 then
        -- Plain START (no Right) should skip opening and launch Stage 5 Loop 2 directly.
        pulse({start = true}, 2)
        if stepFrame == 24 then
            if not check(scene == PLAY and read(CAMPAIGN_STAGE) == 5 and read(AP_LOOP) == 1
                    and read(REVG_BG_ACTIVE) == 6 and read(AP_WEAPON) == 3 and read(LIVES) == 9,
                    string.format("Stage 5 Loop 2 start: scene=%d stage=%d loop=%d bg=%d wpn=%d lives=%d",
                        scene, read(CAMPAIGN_STAGE), read(AP_LOOP), read(REVG_BG_ACTIVE),
                        read(AP_WEAPON), read(LIVES))) then return end
            print(string.format("PASS frame=%d (Title/Option/Invincible/TimerOFF/SoundTest/Stage5Loop2)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
