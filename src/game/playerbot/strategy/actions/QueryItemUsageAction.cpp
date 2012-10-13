#include "../../../pchdef.h"
#include "../../playerbot.h"
#include "QueryItemUsageAction.h"


using namespace ai;


bool QueryItemUsageAction::Execute(Event event)
{
    WorldPacket& data = event.getPacket();
    if (!data.empty())
    {
        data.rpos(0);

        ObjectGuid guid;
        data >> guid;
        if (guid != bot->GetObjectGuid())
            return false;

        uint32 received, created, isShowChatMessage, notUsed, itemId,
            suffixFactor, itemRandomPropertyId, count, invCount;
        uint8 bagSlot;

        data >> received;                               // 0=looted, 1=from npc
        data >> created;                                // 0=received, 1=created
        data >> isShowChatMessage;                                      // IsShowChatMessage
        data >> bagSlot;
                                                                // item slot, but when added to stack: 0xFFFFFFFF
        data >> notUsed;
        data >> itemId;
        data >> suffixFactor;
        data >> itemRandomPropertyId;
        data >> count;
        data >> invCount;

        ItemPrototype const *item = sItemStorage.LookupEntry<ItemPrototype>(itemId);
        if (!item)
            return false;

        ostringstream out; out << chat->formatItem(item, count);
        if (created)
            out << " created";
        else if (received)
            out << " received";
        ai->TellMaster(out);

        QueryItemUsage(item);
        QueryQuestItem(itemId);
        return true;
    }

    string text = event.getParam();

    ItemIds items = chat->parseItems(text);
    QueryItemsUsage(items);
    return true;
}

void QueryItemUsageAction::QueryItemUsage(ItemPrototype const *item)
{
    Player *bot = ai->GetBot();
    if (bot->CanUseItem(item) != EQUIP_ERR_OK)
        return;

    if (item->InventoryType == INVTYPE_NON_EQUIP)
        return;

    uint16 eDest;
    uint8 msg = bot->CanEquipNewItem(NULL_SLOT, eDest, item->ItemId, true);
    if( msg != EQUIP_ERR_OK )
        return;

    Item* existingItem = bot->GetItemByPos(eDest);
    if (!existingItem)
    {
        ai->TellMaster("Equip");
        return;
    }

    bool equip = false;
    const ItemPrototype* oldItem = existingItem->GetProto();
    if (oldItem->ItemLevel < item->ItemLevel && oldItem->ItemId != item->ItemId)
    {
        switch (item->Class)
        {
        case ITEM_CLASS_ARMOR:
            equip = (oldItem->SubClass <= item->SubClass);
            break;
        default:
            equip = true;
        }
    }

    if (equip)
    {
        ostringstream out;
        out << "Replace +";
        out << (item->ItemLevel - oldItem->ItemLevel);
        out << " lvl";
        ai->TellMaster(out.str());
    }
}

void QueryItemUsageAction::QueryItemsUsage(ItemIds items)
{
    for (ItemIds::iterator i = items.begin(); i != items.end(); i++)
    {
        ItemPrototype const *item = sItemStorage.LookupEntry<ItemPrototype>(*i);
        QueryItemUsage(item);
        QueryQuestItem(*i);
    }
}

void QueryItemUsageAction::QueryQuestItem(uint32 itemId)
{
    Player *bot = ai->GetBot();
    QuestStatusMap const& questMap = bot->GetQuestStatusMap();
    for (QuestStatusMap::const_iterator i = questMap.begin(); i != questMap.end(); i++)
    {
        const Quest *questTemplate = sObjectMgr.GetQuestTemplate( i->first );
        if( !questTemplate )
            continue;

        QuestStatus status = bot->GetQuestStatus(questTemplate->GetQuestId());
        if (status != QUEST_STATUS_INCOMPLETE)
            continue;

        QuestStatusData const& questStatus = i->second;
        QueryQuestItem(itemId, questTemplate, &questStatus);
    }
}


void QueryItemUsageAction::QueryQuestItem(uint32 itemId, const Quest *questTemplate, const QuestStatusData *questStatus)
{
    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
    {
        if (questTemplate->ReqItemId[i] != itemId)
            continue;

        int required = questTemplate->ReqItemCount[i];
        int available = questStatus->m_itemcount[i];

        if (!required)
            continue;

        ai->TellMaster(chat->formatQuestObjective(chat->formatQuest(questTemplate), available, required));
    }
}
