/*
 * Copyright (C) 2008-2019 TrinityCore <https://www.trinitycore.org/>
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

#include "GuildMgr.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Guild.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "World.h"

GuildMgr::GuildMgr() : NextGuildId(1)
{ }

GuildMgr::~GuildMgr()
{
    for (GuildContainer::iterator itr = GuildStore.begin(); itr != GuildStore.end(); ++itr)
        delete itr->second;
}

void GuildMgr::AddGuild(Guild* guild)
{
    GuildStore[guild->GetId()] = guild;
}

void GuildMgr::RemoveGuild(ObjectGuid::LowType guildId)
{
    GuildStore.erase(guildId);
}

// Atiesh Features
std::string GuildMgr::GetGuildNameByIdWithLvl(ObjectGuid::LowType guildId) const
{
    std::string glvl = "";
    if (Guild * guild = GetGuildById(guildId))
        return GetGuildNameWithGLvl(guild->GetName(), guild->GetGuildLevel());

    return "";
}

std::string GuildMgr::GetGuildNameWithGLvl(std::string const& guildName, uint32 level) const
{
    std::ostringstream str;
    str << guildName << " (" << level << " level)";

    return str.str();
}

std::string GuildMgr::GetGuildEnemy(ObjectGuid::LowType guildId) const
{
    std::ostringstream str;
    for (GuildWarsContainer::const_iterator itr = _guildWarStore.begin(); itr != _guildWarStore.end(); ++itr)
    {
        if (itr->second.attackerGuildId == guildId)
        {
            if (sWorld->getBoolConfig(CONFIG_GSYSTEM_IN_GUILDENEMY_LIST))
                str << GetGuildNameByIdWithLvl(itr->second.defenderGuildId) << "\n|cFFF00000";
            else
                str << GetGuildNameById(itr->second.defenderGuildId) << "\n|cFFF00000";
        }

        if (itr->second.defenderGuildId == guildId)
        {
            if (sWorld->getBoolConfig(CONFIG_GSYSTEM_IN_GUILDENEMY_LIST))
                str << GetGuildNameByIdWithLvl(itr->second.attackerGuildId) << "\n|cFFF00000";
            else
                str << GetGuildNameById(itr->second.attackerGuildId) << "\n|cFFF00000";
        }
    }

    return str.str();
}


time_t GuildMgr::GetTimeOfLastWarStart(ObjectGuid::LowType guildId)
{
    time_t tmpTime = 0;
    for (GuildWarsHistoryContainer::const_iterator itr = _guildWarHistoryStore.begin(); itr != _guildWarHistoryStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == guildId;
        if (firstGIsAttacker)
            if (itr->second.timeOfStartWar > tmpTime)
                tmpTime = itr->second.timeOfStartWar;

        bool secondGIsAttacker = itr->second.defenderGuildId == guildId;
        if (secondGIsAttacker)
            if (itr->second.timeOfStartWar > tmpTime)
                tmpTime = itr->second.timeOfStartWar;
    }

    return tmpTime;
}

time_t GuildMgr::GetTimeOfLastWarEnd(ObjectGuid::LowType guildId)
{
    time_t tmpTime = 0;
    for (GuildWarsHistoryContainer::const_iterator itr = _guildWarHistoryStore.begin(); itr != _guildWarHistoryStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == guildId;
        if (firstGIsAttacker)
            if (itr->second.timeOfEndWar > tmpTime)
                tmpTime = itr->second.timeOfEndWar;

        bool secondGIsAttacker = itr->second.defenderGuildId == guildId;
        if (secondGIsAttacker)
            if (itr->second.timeOfEndWar > tmpTime)
                tmpTime = itr->second.timeOfEndWar;
    }

    return tmpTime;
}

bool GuildMgr::GuildHasWarState(ObjectGuid::LowType guildId)
{
    for (GuildWarsContainer::const_iterator itr = _guildWarStore.begin(); itr != _guildWarStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == guildId;
        if (firstGIsAttacker)
            return true;

        bool secondGIsAttacker = itr->second.defenderGuildId == guildId;
        if (secondGIsAttacker)
            return true;
    }

    return false;
}

bool GuildMgr::IsGuildsInWar(ObjectGuid::LowType firstguildId, ObjectGuid::LowType secondguildId)
{
    for (GuildWarsContainer::const_iterator itr = _guildWarStore.begin(); itr != _guildWarStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == firstguildId && itr->second.defenderGuildId == secondguildId;
        if (firstGIsAttacker)
            return true;

        bool secondGIsAttacker = itr->second.attackerGuildId == secondguildId && itr->second.defenderGuildId == firstguildId;
        if (secondGIsAttacker)
            return true;
    }

    return false;
}

bool GuildMgr::StartNewWar(GuildWars& data)
{
    // find max id
    uint32 new_id = 0;
    for (GuildWarsHistoryContainer::const_iterator itr = _guildWarHistoryStore.begin(); itr != _guildWarHistoryStore.end(); ++itr)
        if (itr->first > new_id)
            new_id = itr->first;

    // use next
    ++new_id;

    std::string attackerName;
    std::string defenderName;
    if (sWorld->getBoolConfig(CONFIG_GSYSTEM_IN_GUILDENEMY_LIST))
    {
        attackerName = GetGuildNameByIdWithLvl(data.attackerGuildId);
        defenderName = GetGuildNameByIdWithLvl(data.defenderGuildId);
    }
    else
    {
        attackerName = GetGuildNameById(data.attackerGuildId);
        defenderName = GetGuildNameById(data.defenderGuildId);
    }
    

    GuildWarsHistory gwh;
    gwh.attackerGuildId = data.attackerGuildId;
    gwh.attackerGuild = attackerName;
    gwh.defenderGuildId = data.defenderGuildId;
    gwh.defenderGuild = defenderName;
    gwh.timeOfStartWar = uint32(GameTime::GetGameTime());
    gwh.timeOfEndWar = 0;
    gwh.winnerGuild = "";

    _guildWarStore[new_id] = data;
    _guildWarHistoryStore[new_id] = gwh;

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_GUILD_WAR_START);
    stmt->setUInt32(0, new_id);
    stmt->setUInt32(1, data.attackerGuildId);
    stmt->setUInt32(2, data.defenderGuildId);
    CharacterDatabase.Execute(stmt);

    PreparedStatement* stmt2 = CharacterDatabase.GetPreparedStatement(CHAR_INS_GUILD_WAR_START_HISTORY);
    stmt2->setUInt32(0, new_id);
    stmt2->setUInt32(1, gwh.attackerGuildId);
    stmt2->setString(2, gwh.attackerGuild);
    stmt2->setUInt32(3, gwh.defenderGuildId);
    stmt2->setString(4, gwh.defenderGuild);
    stmt2->setUInt32(5, gwh.timeOfStartWar);
    stmt2->setUInt32(6, gwh.timeOfEndWar);
    stmt2->setString(7, gwh.winnerGuild);
    CharacterDatabase.Execute(stmt2);

    Guild* firstguild = GetGuildById(data.attackerGuildId);
    Guild* secondguild = GetGuildById(data.defenderGuildId);
    if (!firstguild || !secondguild)
        return false;

    firstguild->UpdateGuildWarFlag(true);
    secondguild->UpdateGuildWarFlag(true);
    sScriptMgr->OnGuildEnteredInGuildWar(firstguild, defenderName);
    sScriptMgr->OnGuildEnteredInGuildWar(secondguild, attackerName);
    return true;
}

void GuildMgr::StopWarBetween(ObjectGuid::LowType firstguildId, ObjectGuid::LowType secondguildId, ObjectGuid::LowType winnerguildId)
{
    uint32 WarId = 0;
    for (GuildWarsContainer::const_iterator itr = _guildWarStore.begin(); itr != _guildWarStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == firstguildId && itr->second.defenderGuildId == secondguildId;
        if (firstGIsAttacker)
        {
            WarId = itr->first;
            break;
        }

        bool secondGIsAttacker = itr->second.attackerGuildId == secondguildId && itr->second.defenderGuildId == firstguildId;
        if (secondGIsAttacker)
        {
            WarId = itr->first;
            break;
        }
    }

    _guildWarStore.erase(WarId);

    std::string winnerName = GetGuildNameByIdWithLvl(winnerguildId);
    GuildWarsHistoryContainer::iterator itr = _guildWarHistoryStore.find(WarId);
    if (itr != _guildWarHistoryStore.end())
    {
        itr->second.winnerGuild = winnerName;
        itr->second.timeOfEndWar = GameTime::GetGameTime();
    }

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_GUILD_WAR_STOP);
    stmt->setUInt32(0, WarId);
    CharacterDatabase.Execute(stmt);

    PreparedStatement* stmt2 = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GUILD_WAR_STOP_HISTORY);
    stmt2->setUInt32(0, uint32(GameTime::GetGameTime()));
    stmt2->setString(1, winnerName);
    stmt2->setUInt32(2, WarId);
    CharacterDatabase.Execute(stmt2);

    Guild* firstguild = GetGuildById(firstguildId);
    Guild* secondguild = GetGuildById(secondguildId);
    if (!firstguild || !secondguild)
        return;

    if (!GuildHasWarState(firstguildId))
        firstguild->UpdateGuildWarFlag(false);
    if (!GuildHasWarState(secondguildId))
        secondguild->UpdateGuildWarFlag(false);

    sScriptMgr->OnGuildLeftInGuildWar(firstguild, GetGuildNameByIdWithLvl(secondguildId), winnerName);
    sScriptMgr->OnGuildLeftInGuildWar(secondguild, GetGuildNameByIdWithLvl(firstguildId), winnerName);
}

void GuildMgr::StopAllGuildWarsFor(ObjectGuid::LowType guildId)
{
    for (GuildWarsHistoryContainer::const_iterator itr = _guildWarHistoryStore.begin(); itr != _guildWarHistoryStore.end(); ++itr)
    {
        bool firstGIsAttacker = itr->second.attackerGuildId == guildId && itr->second.winnerGuild == "";
        if (firstGIsAttacker)
            StopWarBetween(guildId, itr->second.defenderGuildId, itr->second.defenderGuildId);

        bool secondGIsAttacker = itr->second.defenderGuildId == guildId && itr->second.winnerGuild == "";
        if (secondGIsAttacker)
            StopWarBetween(guildId, itr->second.attackerGuildId, itr->second.attackerGuildId);
    }
}
// Atiesh Features end

ObjectGuid::LowType GuildMgr::GenerateGuildId()
{
    if (NextGuildId >= 0xFFFFFFFE)
    {
        TC_LOG_ERROR("guild", "Guild ids overflow!! Can't continue, shutting down server. ");
        World::StopNow(ERROR_EXIT_CODE);
    }
    return NextGuildId++;
}

// Guild collection
Guild* GuildMgr::GetGuildById(ObjectGuid::LowType guildId) const
{
    GuildContainer::const_iterator itr = GuildStore.find(guildId);
    if (itr != GuildStore.end())
        return itr->second;

    return nullptr;
}

Guild* GuildMgr::GetGuildByName(const std::string& guildName) const
{
    std::string search = guildName;
    std::transform(search.begin(), search.end(), search.begin(), ::toupper);
    for (GuildContainer::const_iterator itr = GuildStore.begin(); itr != GuildStore.end(); ++itr)
    {
        std::string gname = itr->second->GetName();
        std::transform(gname.begin(), gname.end(), gname.begin(), ::toupper);
        if (search == gname)
            return itr->second;
    }
    return nullptr;
}

std::string GuildMgr::GetGuildNameById(ObjectGuid::LowType guildId) const
{
    if (Guild* guild = GetGuildById(guildId))
        return guild->GetName();

    return "";
}

GuildMgr* GuildMgr::instance()
{
    static GuildMgr instance;
    return &instance;
}

Guild* GuildMgr::GetGuildByLeader(ObjectGuid guid) const
{
    for (GuildContainer::const_iterator itr = GuildStore.begin(); itr != GuildStore.end(); ++itr)
        if (itr->second->GetLeaderGUID() == guid)
            return itr->second;

    return nullptr;
}

void GuildMgr::LoadGuilds()
{
    // 1. Load all guilds
    TC_LOG_INFO("server.loading", "Loading guilds definitions...");
    {
        uint32 oldMSTime = getMSTime();

                                                     //          0          1       2             3              4              5              6
        QueryResult result = CharacterDatabase.Query("SELECT g.guildid, g.name, g.leaderguid, g.EmblemStyle, g.EmblemColor, g.BorderStyle, g.BorderColor, "
                                                     //   7                  8       9       10            11           12              13              14             15
                                                     "g.BackgroundColor, g.info, g.motd, g.createdate, g.BankMoney, g.GuildLevel, g.GuildExperience, g.GuildFaction, COUNT(gbt.guildid) "
                                                     "FROM guild g LEFT JOIN guild_bank_tab gbt ON g.guildid = gbt.guildid GROUP BY g.guildid ORDER BY g.guildid ASC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild definitions. DB table `guild` is empty.");
            return;
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                Guild* guild = new Guild();

                if (!guild->LoadFromDB(fields))
                {
                    delete guild;
                    continue;
                }

                AddGuild(guild);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 2. Load all guild ranks
    TC_LOG_INFO("server.loading", "Loading guild ranks...");
    {
        uint32 oldMSTime = getMSTime();

        // Delete orphaned guild rank entries before loading the valid ones
        CharacterDatabase.DirectExecute("DELETE gr FROM guild_rank gr LEFT JOIN guild g ON gr.guildId = g.guildId WHERE g.guildId IS NULL");

        //                                                         0    1      2       3                4
        QueryResult result = CharacterDatabase.Query("SELECT guildid, rid, rname, rights, BankMoneyPerDay FROM guild_rank ORDER BY guildid ASC, rid ASC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild ranks. DB table `guild_rank` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadRankFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild ranks in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 3. Load all guild members
    TC_LOG_INFO("server.loading", "Loading guild members...");
    {
        uint32 oldMSTime = getMSTime();

        // Delete orphaned guild member entries before loading the valid ones
        CharacterDatabase.DirectExecute("DELETE gm FROM guild_member gm LEFT JOIN guild g ON gm.guildId = g.guildId WHERE g.guildId IS NULL");
        CharacterDatabase.DirectExecute("DELETE gm FROM guild_member_withdraw gm LEFT JOIN guild_member g ON gm.guid = g.guid WHERE g.guid IS NULL");

                                                //           0        1         2      3      4        5       6       7       8       9       10
        QueryResult result = CharacterDatabase.Query("SELECT guildid, gm.guid, `rank` , pnote, offnote, w.tab0, w.tab1, w.tab2, w.tab3, w.tab4, w.tab5, "
                                                //    11       12      13       14       15        16      17         18
                                                     "w.money, c.name, c.level, c.class, c.gender, c.zone, c.account, c.logout_time "
                                                     "FROM guild_member gm "
                                                     "LEFT JOIN guild_member_withdraw w ON gm.guid = w.guid "
                                                     "LEFT JOIN characters c ON c.guid = gm.guid ORDER BY guildid ASC");

        if (!result)
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild members. DB table `guild_member` is empty.");
        else
        {
            uint32 count = 0;

            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadMemberFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild members in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 4. Load all guild bank tab rights
    TC_LOG_INFO("server.loading", "Loading bank tab rights...");
    {
        uint32 oldMSTime = getMSTime();

        // Delete orphaned guild bank right entries before loading the valid ones
        CharacterDatabase.DirectExecute("DELETE gbr FROM guild_bank_right gbr LEFT JOIN guild g ON gbr.guildId = g.guildId WHERE g.guildId IS NULL");

                                                     //      0        1      2    3        4
        QueryResult result = CharacterDatabase.Query("SELECT guildid, TabId, rid, gbright, SlotPerDay FROM guild_bank_right ORDER BY guildid ASC, TabId ASC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild bank tab rights. DB table `guild_bank_right` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadBankRightFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u bank tab rights in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 5. Load all event logs
    TC_LOG_INFO("server.loading", "Loading guild event logs...");
    {
        uint32 oldMSTime = getMSTime();

        CharacterDatabase.DirectPExecute("DELETE FROM guild_eventlog WHERE LogGuid > %u", sWorld->getIntConfig(CONFIG_GUILD_EVENT_LOG_COUNT));

                                                     //          0        1        2          3            4            5        6
        QueryResult result = CharacterDatabase.Query("SELECT guildid, LogGuid, EventType, PlayerGuid1, PlayerGuid2, NewRank, TimeStamp FROM guild_eventlog ORDER BY TimeStamp DESC, LogGuid DESC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild event logs. DB table `guild_eventlog` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadEventLogFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild event logs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 6. Load all bank event logs
    TC_LOG_INFO("server.loading", "Loading guild bank event logs...");
    {
        uint32 oldMSTime = getMSTime();

        // Remove log entries that exceed the number of allowed entries per guild
        CharacterDatabase.DirectPExecute("DELETE FROM guild_bank_eventlog WHERE LogGuid > %u", sWorld->getIntConfig(CONFIG_GUILD_BANK_EVENT_LOG_COUNT));

                                                     //          0        1      2        3          4           5            6               7          8
        QueryResult result = CharacterDatabase.Query("SELECT guildid, TabId, LogGuid, EventType, PlayerGuid, ItemOrMoney, ItemStackCount, DestTabId, TimeStamp FROM guild_bank_eventlog ORDER BY TimeStamp DESC, LogGuid DESC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild bank event logs. DB table `guild_bank_eventlog` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadBankEventLogFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild bank event logs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 7. Load all guild bank tabs
    TC_LOG_INFO("server.loading", "Loading guild bank tabs...");
    {
        uint32 oldMSTime = getMSTime();

        // Delete orphaned guild bank tab entries before loading the valid ones
        CharacterDatabase.DirectExecute("DELETE gbt FROM guild_bank_tab gbt LEFT JOIN guild g ON gbt.guildId = g.guildId WHERE g.guildId IS NULL");

                                                     //         0        1      2        3        4
        QueryResult result = CharacterDatabase.Query("SELECT guildid, TabId, TabName, TabIcon, TabText FROM guild_bank_tab ORDER BY guildid ASC, TabId ASC");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild bank tabs. DB table `guild_bank_tab` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[0].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadBankTabFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild bank tabs in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 8. Fill all guild bank tabs
    TC_LOG_INFO("guild", "Filling bank tabs with items...");
    {
        uint32 oldMSTime = getMSTime();

        // Delete orphan guild bank items
        CharacterDatabase.DirectExecute("DELETE gbi FROM guild_bank_item gbi LEFT JOIN guild g ON gbi.guildId = g.guildId WHERE g.guildId IS NULL");

                                                     //          0            1                2      3         4        5      6             7                 8           9           10
        QueryResult result = CharacterDatabase.Query("SELECT creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, durability, playedTime, text, "
                                                     //   11       12     13      14         15
                                                     "guildid, TabId, SlotId, item_guid, itemEntry FROM guild_bank_item gbi INNER JOIN item_instance ii ON gbi.item_guid = ii.guid");

        if (!result)
        {
            TC_LOG_INFO("server.loading", ">> Loaded 0 guild bank tab items. DB table `guild_bank_item` or `item_instance` is empty.");
        }
        else
        {
            uint32 count = 0;
            do
            {
                Field* fields = result->Fetch();
                uint32 guildId = fields[11].GetUInt32();

                if (Guild* guild = GetGuildById(guildId))
                    guild->LoadBankItemFromDB(fields);

                ++count;
            }
            while (result->NextRow());

            TC_LOG_INFO("server.loading", ">> Loaded %u guild bank tab items in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
        }
    }

    // 9. Validate loaded guild data
    TC_LOG_INFO("guild", "Validating data of loaded guilds...");
    {
        uint32 oldMSTime = getMSTime();

        for (GuildContainer::iterator itr = GuildStore.begin(); itr != GuildStore.end();)
        {
            Guild* guild = itr->second;
            ++itr;
            if (guild && !guild->Validate())
                delete guild;
        }

        TC_LOG_INFO("server.loading", ">> Validated data of loaded guilds in %u ms", GetMSTimeDiffToNow(oldMSTime));
    }
}

void GuildMgr::LoadGuildWarData()
{
    uint32 oldMSTime = getMSTime();

    _guildWarStore.clear();                                  // for reload case

    //                                                     0                1
    QueryResult result = CharacterDatabase.Query("SELECT id, Attacker_Guild_ID, Defender_Guild_ID FROM guild_wars");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Guild Wars. DB table `guild_wars` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();

        GuildWars gw;
        gw.attackerGuildId = fields[1].GetUInt32();
        gw.defenderGuildId = fields[2].GetUInt32();

        _guildWarStore[id] = gw;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Guild Wars in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void GuildMgr::LoadGuildWarHistory()
{
    uint32 oldMSTime = getMSTime();

    _guildWarHistoryStore.clear();                                  // for reload case

    //                                                     0                1
    QueryResult result = CharacterDatabase.Query("SELECT id, Attacker_Guild_ID, Attacker_Guild, Defender_Guild_ID, Defender_Guild, Time_Of_Start, Time_Of_End, Winner FROM guild_wars_history");

    if (!result)
    {
        TC_LOG_INFO("server.loading", ">> Loaded 0 Guild Wars History. DB table `guild_wars_history` is empty!");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 id = fields[0].GetUInt32();

        GuildWarsHistory gwh;
        gwh.attackerGuildId = fields[1].GetUInt32();
        gwh.attackerGuild = fields[2].GetString();
        gwh.defenderGuildId = fields[3].GetUInt32();
        gwh.defenderGuild = fields[4].GetString();
        gwh.timeOfStartWar = time_t(fields[5].GetUInt32());
        gwh.timeOfEndWar = time_t(fields[6].GetUInt32());
        gwh.winnerGuild = fields[7].GetString();

        _guildWarHistoryStore[id] = gwh;

        ++count;
    } while (result->NextRow());

    TC_LOG_INFO("server.loading", ">> Loaded %u Guild Wars History in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void GuildMgr::ResetTimes()
{
    for (GuildContainer::const_iterator itr = GuildStore.begin(); itr != GuildStore.end(); ++itr)
        if (Guild* guild = itr->second)
            guild->ResetTimes();

    CharacterDatabase.DirectExecute("TRUNCATE guild_member_withdraw");
}
