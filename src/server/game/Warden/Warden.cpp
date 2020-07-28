/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

#include "AccountMgr.h"
#include "ByteBuffer.h"
#include "Common.h"
#include "GameTime.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include <openssl/sha.h>
#include "Util.h"
#include "Warden.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

Warden::Warden() : _session(nullptr), _checkTimer(10000/*10 sec*/), _clientResponseTimer(0),
                   _module(nullptr), _state(WardenState::STATE_INITIAL)
{
    memset(_inputKey, 0, sizeof(_inputKey));
    memset(_outputKey, 0, sizeof(_outputKey));
    memset(_seed, 0, sizeof(_seed));
    _previousTimestamp = GameTime::GetGameTimeMS();
}

Warden::~Warden()
{
    delete[] _module->CompressedData;
    delete _module;
    _module = nullptr;
}

void Warden::InitializeModule()
{
    SetNewState(WardenState::STATE_INITIALIZE_MODULE);
}

void Warden::RequestHash()
{
    TC_LOG_DEBUG("warden", "Request hash");

    // Create packet structure
    WardenHashRequest Request;
    Request.Command = WARDEN_SMSG_HASH_REQUEST;
    memcpy(Request.Seed, _seed, 16);

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenHashRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenHashRequest));
    pkt.append((uint8*)&Request, sizeof(WardenHashRequest));
    _session->SendPacket(&pkt);

    SetNewState(WardenState::STATE_REQUESTED_HASH);
}

void Warden::SendModuleToClient()
{
    TC_LOG_DEBUG("warden", "Send module to client");

    // Create packet structure
    WardenModuleTransfer packet;

    uint32 sizeLeft = _module->CompressedSize;
    uint32 pos = 0;
    uint16 burstSize;
    while (sizeLeft > 0)
    {
        burstSize = sizeLeft < 500 ? sizeLeft : 500;
        packet.Command = WARDEN_SMSG_MODULE_CACHE;
        packet.DataSize = burstSize;
        memcpy(packet.Data, &_module->CompressedData[pos], burstSize);
        sizeLeft -= burstSize;
        pos += burstSize;

        EncryptData((uint8*)&packet, burstSize + 3);
        WorldPacket pkt1(SMSG_WARDEN_DATA, burstSize + 3);
        pkt1.append((uint8*)&packet, burstSize + 3);
        _session->SendPacket(&pkt1);
    }

    SetNewState(WardenState::STATE_SENT_MODULE);
}

void Warden::RequestModule()
{
    TC_LOG_DEBUG("warden", "Request module");

    // Create packet structure
    WardenModuleUse request;
    request.Command = WARDEN_SMSG_MODULE_USE;

    memcpy(request.ModuleId, _module->Id, 16);
    memcpy(request.ModuleKey, _module->Key, 16);
    request.Size = _module->CompressedSize;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&request, sizeof(WardenModuleUse));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenModuleUse));
    pkt.append((uint8*)&request, sizeof(WardenModuleUse));
    _session->SendPacket(&pkt);

    SetNewState(WardenState::STATE_REQUESTED_MODULE);
}

void Warden::Update()
{
    uint32 currentTimestamp = GameTime::GetGameTimeMS();
    uint32 diff = currentTimestamp - _previousTimestamp;
    _previousTimestamp = currentTimestamp;

    switch (_state)
    {
        case WardenState::STATE_INITIAL:
            break;
        case WardenState::STATE_REQUESTED_MODULE:
        case WardenState::STATE_SENT_MODULE:
        case WardenState::STATE_REQUESTED_HASH:
        case WardenState::STATE_REQUESTED_DATA:
        {
            uint32 maxClientResponseDelay = sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_RESPONSE_DELAY);

            if (maxClientResponseDelay > 0)
            {
                // Kick player if client response delays more than set in config
                if (_clientResponseTimer > maxClientResponseDelay * IN_MILLISECONDS)
                {
                    TC_LOG_WARN("warden", "%s (latency: %u, IP: %s) exceeded Warden module response delay on state %s for more than %s - disconnecting client",
                                   _session->GetPlayerInfo().c_str(), _session->GetLatency(), _session->GetRemoteAddress().c_str(), WardenState::to_string(_state), secsToTimeString(maxClientResponseDelay, TimeFormat::ShortText).c_str());
                    _session->KickPlayer("Warden::Update Warden module response delay exceeded");
                }
                else
                {
                    _clientResponseTimer += diff;
                }

            }
        }
        break;
        case WardenState::STATE_INITIALIZE_MODULE:
        case WardenState::STATE_RESTING:
        {
            if (diff >= _checkTimer)
            {
                RequestData();
            }
            else
            {
                _checkTimer -= diff;
            }
        }
        break;
        default:
            TC_LOG_DEBUG("warden", "Unimplemented warden state!");
            break;
    }
}

void Warden::DecryptData(uint8* buffer, uint32 length)
{
    _inputCrypto.UpdateData(buffer, length);
}

void Warden::EncryptData(uint8* buffer, uint32 length)
{
    _outputCrypto.UpdateData(buffer, length);
}

void Warden::SetNewState(WardenState::Value state)
{
    //if we pass all initial checks, allow change
    if (state < WardenState::STATE_REQUESTED_DATA)
    {
        if (state < _state)
        {
            TC_LOG_DEBUG("warden", "Warden Error: jump from %s to %s which is lower by initialization routine", WardenState::to_string(_state), WardenState::to_string(state));
            return;
        }
    }

    _state = state;

    //Reset timers
    // Set hold off timer, minimum timer should at least be 1 second    
    uint32 holdOff = sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_CHECK_HOLDOFF);
    _checkTimer = (holdOff < 1 ? 1 : holdOff) * IN_MILLISECONDS;

    _clientResponseTimer = 0;
}

bool Warden::IsValidCheckSum(uint32 checksum, const uint8* data, const uint16 length)
{
    uint32 newChecksum = BuildChecksum(data, length);

    if (checksum != newChecksum)
    {
        TC_LOG_DEBUG("warden", "CHECKSUM IS NOT VALID");
        return false;
    }
    else
    {
        TC_LOG_DEBUG("warden", "CHECKSUM IS VALID");
        return true;
    }
}

struct keyData {
    union
    {
        struct
        {
            uint8 bytes[20];
        } bytes;

        struct
        {
            uint32 ints[5];
        } ints;
    };
};

uint32 Warden::BuildChecksum(const uint8* data, uint32 length)
{
    keyData hash;
    SHA1(data, length, hash.bytes.bytes);
    uint32 checkSum = 0;
    for (uint8 i = 0; i < 5; ++i)
    {
        checkSum = checkSum ^ hash.ints.ints[i];
    }

    return checkSum;
}

std::string Warden::Penalty(WardenCheck* check /*= nullptr*/)
{
    uint8 action = 0;

    if (check)
        action = WardenActions(check->Action);
    else
        action = WardenActions(sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_FAIL_ACTION));

    switch (action)
    {
    case WARDEN_ACTION_LOG:
        return "None";
        break;
    case WARDEN_ACTION_KICK:
        _session->KickPlayer("Warden::Penalty");
        return "Kick";
        break;
    case WARDEN_ACTION_BAN:
    {
        std::stringstream duration;
        duration << sWorld->getIntConfig(CONFIG_WARDEN_CLIENT_BAN_DURATION) << "s";
        std::string accountName;
        AccountMgr::GetName(_session->GetAccountId(), accountName);
        std::stringstream banReason;
        banReason << "Warden Anticheat Violation";
        // Check can be NULL, for example if the client sent a wrong signature in the warden packet (CHECKSUM FAIL)
        if (check)
            banReason << ": " << check->Comment << " (CheckId: " << check->CheckId << ")";

        sWorld->BanAccount(BAN_ACCOUNT, accountName, duration.str(), banReason.str(), "Server");

        return "Ban";
    }
    default:
        break;
    }
    return "Undefined";
}

void WorldSession::HandleWardenDataOpcode(WorldPacket& recvData)
{
    if (!_warden || recvData.empty())
    {
        return;
    }

    _warden->DecryptData(const_cast<uint8*>(recvData.contents()), recvData.size());
    uint8 opcode;
    recvData >> opcode;
    TC_LOG_DEBUG("warden", "Got packet, opcode %02X, size %u", opcode, uint32(recvData.size()));
    recvData.hexlike();

    switch (opcode)
    {
        case WARDEN_CMSG_MODULE_MISSING:
            _warden->SendModuleToClient();
            break;
        case WARDEN_CMSG_MODULE_OK:
            _warden->RequestHash();
            break;
        case WARDEN_CMSG_CHEAT_CHECKS_RESULT:
            _warden->HandleData(recvData);
            break;
        case WARDEN_CMSG_MEM_CHECKS_RESULT:
            TC_LOG_DEBUG("warden", "NYI WARDEN_CMSG_MEM_CHECKS_RESULT received!");
            break;
        case WARDEN_CMSG_HASH_RESULT:
            _warden->HandleHashResult(recvData);
            _warden->InitializeModule();
            break;
        case WARDEN_CMSG_MODULE_FAILED:
            TC_LOG_DEBUG("warden", "NYI WARDEN_CMSG_MODULE_FAILED received!");
            break;
        default:
            TC_LOG_DEBUG("warden", "Got unknown warden opcode %02X of size %u.", opcode, uint32(recvData.size() - 1));
            break;
    }
}

void Warden::RequestData()
{
    SetNewState(WardenState::STATE_REQUESTED_DATA);
}

void Warden::HandleData(ByteBuffer& /*buff*/)
{
    SetNewState(WardenState::STATE_RESTING);
}

void Warden::LogPositiveToDB(WardenCheck* check)
{
    if (!check || !_session)
        return;

    if (check->Action < sWorld->getIntConfig(CONFIG_WARDEN_DB_LOGLEVEL))
        return;

    LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_WARDEN);
    stmt->setUInt16(0, check->CheckId);
    stmt->setUInt8(1, check->Action);
    stmt->setUInt32(2, _session->GetAccountId());

    if (Player* pl = _session->GetPlayer())
    {
        stmt->setUInt64(3, pl->GetGUID().GetCounter());
        stmt->setUInt32(4, pl->GetMapId());
        stmt->setFloat(5, pl->GetPositionX());
        stmt->setFloat(6, pl->GetPositionY());
        stmt->setFloat(7, pl->GetPositionZ());
    }
    else
    {
        stmt->setUInt64(3, 0);
        stmt->setUInt32(4, 0);
        stmt->setFloat(5, 0.0f);
        stmt->setFloat(6, 0.0f);
        stmt->setFloat(7, 0.0f);
    }

    LoginDatabase.Execute(stmt);
}
