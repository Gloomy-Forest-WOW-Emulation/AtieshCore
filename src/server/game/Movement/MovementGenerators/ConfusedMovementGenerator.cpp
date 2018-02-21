/*
 * Copyright (C) 2008-2018 TrinityCore <https://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
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

#include "ConfusedMovementGenerator.h"
#include "Creature.h"
#include "MoveSpline.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "Player.h"
#include "Random.h"
#include "VMapFactory.h"
#include "Map.h"

template<class T>
ConfusedMovementGenerator<T>::~ConfusedMovementGenerator()
{
    delete _path;
}

template<class T>
void ConfusedMovementGenerator<T>::DoInitialize(T* owner)
{
    if (!owner || !owner->IsAlive())
        return;

    //owner->ClearUnitState(UNIT_STATE_MOVING);
    owner->AddUnitState(UNIT_STATE_CONFUSED);
    owner->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED);
    owner->AddUnitState(UNIT_STATE_CONFUSED_MOVE);
    
    owner->GetPosition(_x, _y, _z);

    if (owner->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_DISTRACTED) || owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
        _timer.Reset(200);
    else
    {
        Position startdest;
        owner->GetPosition(startdest.m_positionX, startdest.m_positionY, startdest.m_positionZ);
        owner->GetNearPoint(owner, startdest.m_positionX, startdest.m_positionY, startdest.m_positionZ, 2.0f, 1.0f, owner->GetOrientation() * float(M_PI));

        float distance = owner->GetExactDist2d(startdest.m_positionX, startdest.m_positionY);
        owner->MovePositionToFirstCollision(startdest, distance, owner->GetOrientation() * float(M_PI));

        Movement::MoveSplineInit init(owner);
        init.MoveTo(startdest.m_positionX, startdest.m_positionY, startdest.m_positionZ);
        init.SetWalk(true);

        float _speed = owner->GetSpeed(MOVE_WALK);
        float _checkspeed = owner->IsMounted() ? 5.0f : 2.5f;
        if (_speed > _checkspeed)
            _speed = _checkspeed;

        init.SetVelocity(_speed);

        int32 traveltime = init.Launch();
        _timer.Reset(traveltime + urand(150, 250));
    }
}

template<class T>
void ConfusedMovementGenerator<T>::DoReset(T* owner)
{
    DoInitialize(owner);
}

template<class T>
bool ConfusedMovementGenerator<T>::DoUpdate(T* owner, uint32 diff)
{
    if (!owner || !owner->IsAlive())
        return false;

    if (owner->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED | UNIT_STATE_DISTRACTED))
    {
        if (owner->HasUnitState(UNIT_STATE_CONFUSED_MOVE))
        {
            owner->StopMoving();
            owner->ClearUnitState(UNIT_STATE_CONFUSED_MOVE);
            return true;
        }
        else
            return true;
    }

    if (owner->HasUnitState(UNIT_STATE_NOT_MOVE) || owner->IsMovementPreventedByCasting())
    {
        _interrupt = true;
        // remove old flag of movement
        if (owner->HasUnitState(UNIT_STATE_CONFUSED_MOVE))
        {
            owner->StopMoving();
            owner->ClearUnitState(UNIT_STATE_CONFUSED_MOVE);
        }
    }
    else
        _interrupt = false;

    // waiting for next move
    _timer.Update(diff);
    if (!_interrupt && _timer.Passed() && owner->movespline->Finalized())
    {
        // remove old flag of movement
        owner->ClearUnitState(UNIT_STATE_CONFUSED_MOVE);        

        // start moving
        Position destination(_x, _y, _z);
        float distance = 4.0f * frand(0.0f, 1.0f) - 2.0f;
        float angle = frand(0.0f, 1.0f) * float(M_PI) * 2.0f;
        owner->MovePositionToFirstCollision(destination, distance, angle);

        // additional vmap checking, related with not full algorythm in MovePositionToFirstCollision
        bool col = VMAP::VMapFactory::createOrGetVMapManager()->getObjectHitPos(owner->GetMapId(), owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ() + 0.5f, destination.m_positionX, destination.m_positionY, destination.m_positionZ + 0.5f, destination.m_positionX, destination.m_positionY, destination.m_positionZ, -0.5f);
        // collision occured
        if (col)
        {
            // move back a bit
            destination.m_positionX -= CONTACT_DISTANCE * std::cos(owner->GetOrientation());
            destination.m_positionY -= CONTACT_DISTANCE * std::sin(owner->GetOrientation());
            if (Map* map = owner->GetMap())
                destination.m_positionZ = map->GetHeight(owner->GetPhaseMask(), destination.m_positionX, destination.m_positionY, destination.m_positionZ + 2.8f, true);
        }

        if (!_path)
            _path = new PathGenerator(owner);

        _path->SetPathLengthLimit(30.0f);
        bool result = _path->CalculatePath(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
        if (!result || (_path->GetPathType() & PATHFIND_NOPATH))
        {
            _timer.Reset(100);
            return true;
        }        

        owner->AddUnitState(UNIT_STATE_CONFUSED_MOVE);

        Movement::MoveSplineInit init(owner);
        init.MovebyPath(_path->GetPath());
        init.SetWalk(true);

        float _speed = owner->GetSpeed(MOVE_WALK);
        float _checkspeed = owner->IsMounted() ? 5.0f : 2.5f;
        if (_speed > _checkspeed)
            _speed = _checkspeed;

        init.SetVelocity(_speed);

        int32 traveltime = init.Launch();
        _timer.Reset(traveltime + urand(800, 1500));        

        // update position for server and others units/players
        owner->UpdateSplinePosition();
    }

    return true;
}

template<class T>
void ConfusedMovementGenerator<T>::DoFinalize(T*) { }

template<>
void ConfusedMovementGenerator<Player>::DoFinalize(Player* unit)
{
    unit->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED);
    unit->ClearUnitState(UNIT_STATE_CONFUSED);
    unit->StopMoving();
}

template<>
void ConfusedMovementGenerator<Creature>::DoFinalize(Creature* unit)
{
    unit->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED);
    unit->ClearUnitState(UNIT_STATE_CONFUSED | UNIT_STATE_CONFUSED_MOVE);
    if (unit->GetVictim())
        unit->SetTarget(unit->EnsureVictim()->GetGUID());
}

template ConfusedMovementGenerator<Player>::~ConfusedMovementGenerator();
template ConfusedMovementGenerator<Creature>::~ConfusedMovementGenerator();
template void ConfusedMovementGenerator<Player>::DoInitialize(Player*);
template void ConfusedMovementGenerator<Creature>::DoInitialize(Creature*);
template void ConfusedMovementGenerator<Player>::DoReset(Player*);
template void ConfusedMovementGenerator<Creature>::DoReset(Creature*);
template bool ConfusedMovementGenerator<Player>::DoUpdate(Player*, uint32 diff);
template bool ConfusedMovementGenerator<Creature>::DoUpdate(Creature*, uint32 diff);
