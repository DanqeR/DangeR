/*
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PoolHandler.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Language.h"
#include "Log.h"
#include "MapManager.h"
#include "Policies/SingletonImp.h"

INSTANTIATE_SINGLETON_1(PoolHandler);

PoolHandler::PoolHandler()
{
    isSystemInit = false;
}

void PoolHandler::LoadFromDB()
{
    QueryResult *result = WorldDatabase.Query("SELECT MAX(entry) FROM pool_template");
    if (!result)
    {
        sLog.outString(">> Table pool_template is empty.");
        sLog.outString();
        return;
    }
    else
    {
        Field *fields = result->Fetch();
        max_pool_id = fields[0].GetUInt16();
        delete result;
    }

    mPoolTemplate.resize(max_pool_id + 1);

    result = WorldDatabase.Query("SELECT entry,max_limit FROM pool_template");
    if (!result)
    {
        mPoolTemplate.clear();
        sLog.outString(">> Table pool_template is empty:");
        sLog.outString();
        return;
    }

    uint32 count = 0;

    barGoLink bar(result->GetRowCount());
    do
    {
        ++count;
        Field *fields = result->Fetch();

        bar.step();

        uint16 pool_id = fields[0].GetUInt16();

        PoolTemplateData& pPoolTemplate = mPoolTemplate[pool_id];
        pPoolTemplate.MaxLimit  = fields[1].GetUInt32();

    } while (result->NextRow());

    sLog.outString();
    sLog.outString( ">> Loaded %u objects pools", count );
    delete result;

    // Creatures

    mPoolCreatureGroups.resize(max_pool_id + 1);
    mCreatureSearchMap.clear();
    //                                   1     2           3
    result = WorldDatabase.Query("SELECT guid, pool_entry, chance FROM pool_creature");

    count = 0;
    if (!result)
    {
        barGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools", count );
    }
    else
    {

        barGoLink bar2(result->GetRowCount());
        do
        {
            Field *fields = result->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();

            CreatureData const* data = objmgr.GetCreatureData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_creature` has a non existing creature spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id );
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_creature` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.",pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_creature` has an invalid chance (%f) for creature guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }
            PoolTemplateData *pPoolTemplate = &mPoolTemplate[pool_id];
            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<Creature>& cregroup = mPoolCreatureGroups[pool_id];
            cregroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mCreatureSearchMap.insert(p);

        } while (result->NextRow());
        sLog.outString();
        sLog.outString( ">> Loaded %u creatures in pools", count );
        delete result;
    }

    // Gameobjects

    mPoolGameobjectGroups.resize(max_pool_id + 1);
    mGameobjectSearchMap.clear();
    //                                   1     2           3
    result = WorldDatabase.Query("SELECT guid, pool_entry, chance FROM pool_gameobject");

    count = 0;
    if (!result)
    {
        barGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools", count );
    }
    else
    {

        barGoLink bar2(result->GetRowCount());
        do
        {
            Field *fields = result->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();

            GameObjectData const* data = objmgr.GetGOData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_gameobject` has a non existing gameobject spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id );
                continue;
            }
            GameObjectInfo const* goinfo = objmgr.GetGameObjectInfo(data->id);
            if (goinfo->type != GAMEOBJECT_TYPE_CHEST &&
                goinfo->type != GAMEOBJECT_TYPE_GOOBER &&
                goinfo->type != GAMEOBJECT_TYPE_FISHINGHOLE)
            {
                sLog.outErrorDb("`pool_gameobject` has a not lootable gameobject spawn (GUID: %u, type: %u) defined for pool id (%u), skipped.", guid, goinfo->type, pool_id );
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_gameobject` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.",pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_gameobject` has an invalid chance (%f) for gameobject guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }
            PoolTemplateData *pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<GameObject>& gogroup = mPoolGameobjectGroups[pool_id];
            gogroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mGameobjectSearchMap.insert(p);

        } while( result->NextRow() );
        sLog.outString();
        sLog.outString( ">> Loaded %u gameobject in pools", count );
        delete result;
    }
}

// The initialize method will spawn all pools not in an event and not in another pool, this is why there is 2 left joins with 2 null checks
void PoolHandler::Initialize()
{
    QueryResult *result = WorldDatabase.Query("SELECT DISTINCT pool_template.entry FROM pool_template LEFT JOIN game_event_pool ON pool_template.entry=game_event_pool.pool_entry LEFT JOIN pool_pool ON pool_template.entry=pool_pool.pool_id WHERE game_event_pool.pool_entry IS NULL AND pool_pool.pool_id IS NULL");
    uint32 count=0;
    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            uint16 pool_entry = fields[0].GetUInt16();
            if (!CheckPool(pool_entry))
            {
                sLog.outErrorDb("Pool Id (%u) has all creatures or gameobjects with explicit chance sum <>100 and no equal chance defined. The pool system cannot pick one to spawn.", pool_entry);
                continue;
            }
            SpawnPool(pool_entry);
            count++;
        } while (result->NextRow());
    }

    sLog.outBasic("Pool handling system initialized, %u pools spawned.", count);
    isSystemInit = true;
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the gameobject/creature is respawned only (added back to map)
void PoolHandler::SpawnPool(uint16 pool_id, bool cache)
{
    if (!mPoolGameobjectGroups[pool_id].isEmpty())
        mPoolGameobjectGroups[pool_id].SpawnObject(mPoolTemplate[pool_id].MaxLimit, cache);
    if (!mPoolCreatureGroups[pool_id].isEmpty())
        mPoolCreatureGroups[pool_id].SpawnObject(mPoolTemplate[pool_id].MaxLimit, cache);
}

// Call to despawn a pool, all gameobjects/creatures in this pool are removed
void PoolHandler::DespawnPool(uint16 pool_id)
{
    if (!mPoolGameobjectGroups[pool_id].isEmpty())
        mPoolGameobjectGroups[pool_id].DespawnObject();
    if (!mPoolCreatureGroups[pool_id].isEmpty())
        mPoolCreatureGroups[pool_id].DespawnObject();
}

// Call to update the pool when a gameobject/creature part of pool [pool_id] is ready to respawn
// Here we cache only the creature/gameobject whose guid is passed as parameter
// Then the spawn pool call will use this cache to decide
void PoolHandler::UpdatePool(uint16 pool_id, uint32 guid, uint32 type)
{
    if (type == TYPEID_GAMEOBJECT && !mPoolGameobjectGroups[pool_id].isEmpty())
        mPoolGameobjectGroups[pool_id].DespawnObject(guid);
    else if (type != TYPEID_GAMEOBJECT && !mPoolCreatureGroups[pool_id].isEmpty())
        mPoolCreatureGroups[pool_id].DespawnObject(guid);

    SpawnPool(pool_id, true);
}

// Method that tell if the gameobject/creature is part of a pool and return the pool id if yes
uint16 PoolHandler::IsPartOfAPool(uint32 guid, uint32 type)
{
    if (type == TYPEID_GAMEOBJECT)
    {
        SearchMap::const_iterator itr = mGameobjectSearchMap.find(guid);
        if (itr != mGameobjectSearchMap.end())
            return itr->second;
    }
    else // creature
    {
        SearchMap::const_iterator itr = mCreatureSearchMap.find(guid);
        if (itr != mCreatureSearchMap.end())
            return itr->second;
    }
    return 0;
}

// Method that check chance integrity of the creatures and gameobjects in this pool
bool PoolHandler::CheckPool(uint16 pool_id)
{
    return mPoolCreatureGroups[pool_id].CheckPool() && mPoolGameobjectGroups[pool_id].CheckPool();
}

// Method that tell if a creature or gameobject in pool_id is spawned currently
bool PoolHandler::IsSpawnedObject(uint16 pool_id, uint32 guid, uint32 type)
{
    if (pool_id > max_pool_id)
        return false;
    if (type == TYPEID_GAMEOBJECT)
        return mPoolGameobjectGroups[pool_id].IsSpawnedObject(guid);
    else
        return mPoolCreatureGroups[pool_id].IsSpawnedObject(guid);
}

////////////////////////////////////////////////////////////
// Methods of templated Pool Group class above

template <class T>
PoolHandler::PoolGroup<T>::PoolGroup()
{
    Spawned = 0;
}

// Method to add a gameobject/creature guid to the proper list depending on pool type and chance value
template <class T>
void PoolHandler::PoolGroup<T>::AddEntry(PoolObject& poolitem, uint32 maxentries)
{
    if (poolitem.chance != 0 && maxentries == 1)
        ExplicitlyChanced.push_back(poolitem);
    else
        EqualChanced.push_back(poolitem);
}

// Method to check the chances are proper in this object pool
template <class T>
bool PoolHandler::PoolGroup<T>::CheckPool(void)
{
    if (EqualChanced.size() == 0)
    {
        float chance = 0;
        for (uint32 i=0; i<ExplicitlyChanced.size(); ++i)
            chance += ExplicitlyChanced[i].chance;
        if (chance != 100 && chance != 0)
            return false;
    }
    return true;
}

// Method that tell if the gameobject, creature or pool is spawned currently
template <class T>
bool PoolHandler::PoolGroup<T>::IsSpawnedObject(uint32 guid)
{
    for (uint32 i=0; i<ExplicitlyChanced.size(); ++i)
        if (ExplicitlyChanced[i].guid == guid)
            return ExplicitlyChanced[i].spawned;
    for (uint32 i=0; i<EqualChanced.size(); ++i)
        if (EqualChanced[i].guid == guid)
            return EqualChanced[i].spawned;
    return false;
}

// Method that return a guid of a rolled creature or gameobject
// Note: Copy from loot system because it's very similar and only few things change
template <class T>
uint32 PoolHandler::PoolGroup<T>::RollOne(void)
{
    if (!ExplicitlyChanced.empty())                         // First explicitly chanced entries are checked
    {
        float roll = rand_chance();

        for (uint32 i=0; i<ExplicitlyChanced.size(); ++i)
        {
            roll -= ExplicitlyChanced[i].chance;
            if (roll < 0)
                return ExplicitlyChanced[i].guid;
        }
    }
    if (!EqualChanced.empty())
        return EqualChanced[irand(0, EqualChanced.size()-1)].guid;

    return 0;                                               // None found
}

// Main method to despawn a creature or gameobject in a pool
// If no guid is passed, the pool is just removed (event end case)
// If guid is filled, cache will be used and no removal will occur, it just fill the cache
template<class T>
void PoolHandler::PoolGroup<T>::DespawnObject(uint32 guid)
{
    for (int i=0; i<EqualChanced.size(); ++i)
    {
        if (EqualChanced[i].spawned)
        {
            if (!guid || EqualChanced[i].guid == guid)
            {
                if (guid)
                    CacheValue = EqualChanced[i].guid;
                else
                    Despawn1Object(EqualChanced[i].guid);

                EqualChanced[i].spawned = false;
                Spawned--;
            }
        }
    }
}

// Method that is actualy doing the removal job on one creature
template<>
void PoolHandler::PoolGroup<Creature>::Despawn1Object(uint32 guid)
{
    if (CreatureData const* data = objmgr.GetCreatureData(guid))
    {
        objmgr.RemoveCreatureFromGrid(guid, data);

        if (Creature* pCreature = ObjectAccessor::Instance().GetObjectInWorld(MAKE_NEW_GUID(guid, data->id, HIGHGUID_UNIT), (Creature*)NULL))
        {
            pCreature->CleanupsBeforeDelete();
            pCreature->AddObjectToRemoveList();
        }
    }
}

// Same on one gameobject
template<>
void PoolHandler::PoolGroup<GameObject>::Despawn1Object(uint32 guid)
{
    if (GameObjectData const* data = objmgr.GetGOData(guid))
    {
        objmgr.RemoveGameobjectFromGrid(guid, data);

        if (GameObject* pGameobject = ObjectAccessor::Instance().GetObjectInWorld(MAKE_NEW_GUID(guid, data->id, HIGHGUID_GAMEOBJECT), (GameObject*)NULL))
            pGameobject->AddObjectToRemoveList();
    }
}

// Method that Spawn 1+ creatures or gameobject
// if cache is false (initialization or event start), X creatures are spawned with X <= limit (< if limit higher that the number of creatures in pool)
// if cache is true, this means only one has to be spawned (or respawned if the rolled one is same as cached one)
template <class T>
void PoolHandler::PoolGroup<T>::SpawnObject(uint32 limit, bool cache)
{
    if (limit == 1)                                         // This is the only case where explicit chance is used
    {
        uint32 roll = RollOne();
        if (cache && CacheValue != roll)
            Despawn1Object(CacheValue);
        CacheValue = Spawn1Object(roll);
    }
    else if (limit < EqualChanced.size() && Spawned < limit)
    {
        std::vector<uint32> IndexList;
        for (int i=0; i<EqualChanced.size(); ++i)
            if (!EqualChanced[i].spawned)
                IndexList.push_back(i);

        while (Spawned < limit && IndexList.size() > 0)
        {
            uint32 roll = urand(1, IndexList.size()) - 1;
            uint32 index = IndexList[roll];
            if (!cache || (cache && EqualChanced[index].guid != CacheValue))
            {
                if (cache)
                    Despawn1Object(CacheValue);
                EqualChanced[index].spawned = Spawn1Object(EqualChanced[index].guid);
            }
            else
                EqualChanced[index].spawned = ReSpawn1Object(EqualChanced[index].guid);

            if (EqualChanced[index].spawned)
                ++Spawned;                                  // limited group use the Spawned variable to store the number of actualy spawned creatures
            std::vector<uint32>::iterator itr = IndexList.begin()+roll;
            IndexList.erase(itr);
        }
        CacheValue = 0;
    }
    else  // Not enough objects in pool, so spawn all
    {
        for (int i=0; i<EqualChanced.size(); ++i)
            EqualChanced[i].spawned = Spawn1Object(EqualChanced[i].guid);
    }
}

// Method that is actualy doing the spawn job on 1 creature
template <>
bool PoolHandler::PoolGroup<Creature>::Spawn1Object(uint32 guid)
{
    CreatureData const* data = objmgr.GetCreatureData(guid);
    if (data)
    {
        objmgr.AddCreatureToGrid(guid, data);

        // Spawn if necessary (loaded grids only)
        Map* map = const_cast<Map*>(MapManager::Instance().GetBaseMap(data->mapid));
        // We use spawn coords to spawn
        if (!map->Instanceable() && !map->IsRemovalGrid(data->posX, data->posY))
        {
            Creature* pCreature = new Creature;
            //sLog.outDebug("Spawning creature %u",guid);
            if (!pCreature->LoadFromDB(guid, map))
            {
                delete pCreature;
            }
            else
            {
                map->Add(pCreature);
            }
        }
        return true;
    }
    return false;
}

// Same for 1 gameobject
template <>
bool PoolHandler::PoolGroup<GameObject>::Spawn1Object(uint32 guid)
{
    GameObjectData const* data = objmgr.GetGOData(guid);
    if (data)
    {
        objmgr.AddGameobjectToGrid(guid, data);
        // Spawn if necessary (loaded grids only)
        // this base map checked as non-instanced and then only existed
        Map* map = const_cast<Map*>(MapManager::Instance().GetBaseMap(data->mapid));
        // We use current coords to unspawn, not spawn coords since creature can have changed grid
        if (!map->Instanceable() && !map->IsRemovalGrid(data->posX, data->posY))
        {
            GameObject* pGameobject = new GameObject;
            //sLog.outDebug("Spawning gameobject %u", guid);
            if (!pGameobject->LoadFromDB(guid, map))
            {
                delete pGameobject;
            }
            else
            {
                if (pGameobject->isSpawnedByDefault())
                    map->Add(pGameobject);
            }
        }
        return true;
    }
    return false;
}

// Method that does the respawn job on the specified creature
template <>
bool PoolHandler::PoolGroup<Creature>::ReSpawn1Object(uint32 guid)
{
    CreatureData const* data = objmgr.GetCreatureData(guid);
    if (data)
    {
        if (Creature* pCreature = ObjectAccessor::Instance().GetObjectInWorld(MAKE_NEW_GUID(guid, data->id, HIGHGUID_UNIT), (Creature*)NULL))
            pCreature->GetMap()->Add(pCreature);
        return true;
    }
    return false;
}

// Same for 1 gameobject
template <>
bool PoolHandler::PoolGroup<GameObject>::ReSpawn1Object(uint32 guid)
{
    GameObjectData const* data = objmgr.GetGOData(guid);
    if (data)
    {
        if (GameObject* pGameobject = ObjectAccessor::Instance().GetObjectInWorld(MAKE_NEW_GUID(guid, data->id, HIGHGUID_GAMEOBJECT), (GameObject*)NULL))
            pGameobject->GetMap()->Add(pGameobject);
        return true;
    }
    return false;
}
