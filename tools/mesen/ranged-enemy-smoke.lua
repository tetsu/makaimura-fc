-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local trialLength = 730
local trials = {"jump", "shoot", "hit"}
local cueFrame, launchFrame, reclaimedFrame, firstX, firstY, lastX
local cuePixel, litFace, sparkVisible, secondLaunch

local function sprite(offset)
    return emu.read(0x200 + offset, emu.memType.nesMemory)
end

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

emu.addEventCallback(function()
    local elapsed = frame - 144
    local trial = elapsed >= 0 and trials[elapsed // trialLength + 1]
    local t = elapsed % trialLength
    emu.setInput({
        start = trial and t == 0,
        right = trial and t >= 16 and t <= 584,
        a = frame == 122 or (trial and t >= 170 and t <= 584 and (t + 144) % 19 == 0)
            or (trial == "jump" and (t == 593 or t == 669)),
        b = trial and (t == 94 or (trial == "shoot" and t == 590)),
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    local elapsed = frame - 145
    if elapsed < 0 then return end
    local trial = trials[elapsed // trialLength + 1]
    if not trial then emu.stop(0); return end
    local t = elapsed % trialLength
    if t == 15 then
        cueFrame, launchFrame, reclaimedFrame = nil, nil, nil
        cuePixel, litFace, sparkVisible, secondLaunch = nil, false, false, false
        if sprite(2) ~= 0 or sprite(28) ~= 0xf8 then
            fail(110, "START did not reset the encounter")
            return
        end
    end
    if t < 555 or t > 718 then return end

    local kilnY, boltY = sprite(20), sprite(28)
    if kilnY ~= 0xf8 and not litFace then
        local picture = emu.getScreenBuffer()
        local pixel = picture[(kilnY + 6) * 256 + sprite(23) + 4]
        if sprite(22) == 2 then
            cueFrame = cueFrame or t
            if cuePixel and cuePixel ~= pixel then litFace = true end
        else
            cuePixel = pixel
        end
    end
    if boltY ~= 0xf8 then
        local x = sprite(31)
        if not launchFrame then
            launchFrame, firstX, firstY = t, x, boltY
        elseif not reclaimedFrame and x ~= lastX - 2 then
            fail(112, "enemy bolt did not move two pixels left")
            return
        elseif reclaimedFrame then
            secondLaunch = true
        end
        lastX = x
    elseif launchFrame and not reclaimedFrame then
        reclaimedFrame = t
    end

    -- The OAM shadow may already describe the next frame. Sample both frames
    -- on the right half of the spark, beyond the standing player's sprite.
    if launchFrame and t <= launchFrame + 1 then
        local picture = emu.getScreenBuffer()
        local spark = picture[(firstY + 3) * 256 + firstX + 3]
        sparkVisible = sparkVisible or spark ~= picture[1]
    end

    if t == 718 then
        if not litFace then fail(113, "aiming cue did not change the kiln's color"); return end
        if trial == "shoot" then
            if kilnY ~= 0xf8 or launchFrame or sprite(2) == 2 then
                fail(114, "player shot did not cancel the windup")
            end
            return
        end
        if not launchFrame or launchFrame - cueFrame ~= 30 or firstX ~= 218 then
            fail(115, "wrong first launch time or position")
            return
        end
        if not sparkVisible then fail(111, "enemy bolt is missing pixels"); return end
        if trial == "hit" and (sprite(2) ~= 2 or reclaimedFrame ~= launchFrame + 1) then
            fail(116, "standing player did not take the hit")
        elseif trial == "jump" and (sprite(2) == 2 or reclaimedFrame ~= launchFrame + 38
            or not secondLaunch) then
            fail(117, "jump did not avoid the shot and free the slot for another")
        end
    end
end, emu.eventType.endFrame)
