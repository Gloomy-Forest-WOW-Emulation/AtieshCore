/*
 * Copyright (C) 2018+ AtieshCore <https://at-wow.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "GameTime.h"
#include "SpecialEvent.h"

SpecialEvent::SpecialEvent(bool active, bool enabled, uint32 id, uint32 cooldownTimer, uint32 durationTimer)
{
    _active = active;
    _enabled = enabled;
    _eventId = id;
    _timer = cooldownTimer * MINUTE * IN_MILLISECONDS;
    _noEventTime = durationTimer * MINUTE * IN_MILLISECONDS;
    _gameTimeNextEvent = uint32(GameTime::GetGameTime() + cooldownTimer * MINUTE);
    _plrCount = 0;
}

void SpecialEvent::Update(uint32 diff)
{
    if (!_enabled)
        return;

    _timer.Update(diff);
    if (_timer.Passed())
    {
        // battlefield ends on time
        if (IsActiveTime())
            EndSpecialEvent(true);
        else // time to start a new battle
            StartSpecialEvent();
    }
}

void SpecialEvent::StartSpecialEvent()
{
    if (_active)
        return;

    _timer.Reset(_EventTime);
    _active = true;

    OnSpecialEventStart();
}

void SpecialEvent::EndSpecialEvent(bool endByTimer)
{
    if (!_active)
        return;

    _active = false;

    //if (!endByTimer)
    //    SetDefenderTeam(GetAttackerTeam());

    OnSpecialEventEnd(endByTimer);

    // Reset timer
    _timer.Reset(_noEventTime);
}
