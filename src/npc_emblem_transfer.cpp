#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "Language.h"

enum Actions
{
    ACTION_NONE                 = 0,
    ACTION_RETRIEVE_EMBLEMS     = 1001,
    ACTION_TRANSFER_FROST       = 1002,
    ACTION_TRANSFER_TRIUMPH     = 1003,
    ACTION_TRANSFER_CONQUEST    = 1004,
    ACTION_TRANSFER_HEROISM     = 1005,
    ACTION_TRANSFER_VALOR       = 1006,
    ACTION_CLOSE                = 1007
};

enum Items
{
    ITEM_EMBLEM_OF_FROST    = 49426,
    ITEM_EMBLEM_OF_TRIUMPH  = 47241,
    ITEM_EMBLEM_OF_CONQUEST = 45624,
    ITEM_EMBLEM_OF_HEROISM  = 40752,
    ITEM_EMBLEM_OF_VALOR    = 40753
};

enum SenderMenu
{
    GOSSIP_SENDER_TRANSFER_FROST    = 1001,
    GOSSIP_SENDER_TRANSFER_TRIUMPH  = 1002,
    GOSSIP_SENDER_TRANSFER_CONQUEST = 1003,
    GOSSIP_SENDER_TRANSFER_HEROISM  = 1004,
    GOSSIP_SENDER_TRANSFER_VALOR    = 1005
};

/*
 * How does this works?
 * 1) Select the type of emblem you want to transfer
 * 2) Select the character you want to transfer to (from your account)
 * 3) Input the amount of emblems to transfer
 */

class npc_emblem_transfer : public CreatureScript
{
public:
    npc_emblem_transfer() : CreatureScript("npc_emblem_transfer") { }

    // Step 1
    bool OnGossipHello(Player* player, Creature* creature)
    {
        float penalty = sConfigMgr->GetOption<float>("EmblemTransfer.penalty", 0.0f);
        if (penalty > 0.0f)
        {
            std::stringstream ss;
            ss << "Transferences will be applied a " << (penalty * 100.0f) << "% penalty. For every 10, you will receive " << (10 * (1.0f - penalty)) << ".";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, ss.str().c_str(), GOSSIP_SENDER_MAIN, ACTION_NONE);
        }

        if (sConfigMgr->GetOption<bool>("EmblemTransfer.allowEmblemsFrost", true))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Transfer my Emblems of Frost", GOSSIP_SENDER_MAIN, ACTION_TRANSFER_FROST);

        if (sConfigMgr->GetOption<bool>("EmblemTransfer.allowEmblemsTriumph", false))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Transfer my Emblems of Triumph", GOSSIP_SENDER_MAIN, ACTION_TRANSFER_TRIUMPH);

        if (sConfigMgr->GetOption<bool>("EmblemTransfer.allowEmblemsConquest", false))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Transfer my Emblems of Conquest", GOSSIP_SENDER_MAIN, ACTION_TRANSFER_CONQUEST);

        if (sConfigMgr->GetOption<bool>("EmblemTransfer.allowEmblemsHeroism", false))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Transfer my Emblems of Heroism", GOSSIP_SENDER_MAIN, ACTION_TRANSFER_HEROISM);

        if (sConfigMgr->GetOption<bool>("EmblemTransfer.allowEmblemsValor", false))
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Transfer my Emblems of Valor", GOSSIP_SENDER_MAIN, ACTION_TRANSFER_VALOR);

        QueryResult result = CharacterDatabase.Query("SELECT 1 FROM emblem_transferences WHERE receiver_guid = {} AND active = 1 LIMIT 1", player->GetSession()->GetGuidLow());
        if (result)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Get my transfered emblems", GOSSIP_SENDER_MAIN, ACTION_RETRIEVE_EMBLEMS);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();

        if (action == ACTION_CLOSE)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (action == ACTION_NONE)
        {
            return OnGossipHello(player, creature);
        }

        // Player wants to get its emblems
        if (action == ACTION_RETRIEVE_EMBLEMS)
        {
            QueryResult result = CharacterDatabase.Query("SELECT emblem_entry, amount FROM emblem_transferences WHERE receiver_guid = {} AND active = 1", player->GetSession()->GetGuidLow());
            if (result)
            {
                do
                {
                    Field* fields = result->Fetch();
                    uint32 emblemId = fields[0].Get<uint32>();
                    uint32 amount = fields[1].Get<uint32>();

                    // The next block of code was copied from .additem command
                    // <START>

                    // check space and find places
                    uint32 noSpaceForCount = 0;
                    ItemPosCountVec dest;
                    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, emblemId, amount, &noSpaceForCount);
                    if (msg != EQUIP_ERR_OK)
                        amount -= noSpaceForCount;

                    if (amount == 0 || dest.empty())
                    {
                        ChatHandler(player->GetSession()).SendNotification(LANG_ITEM_CANNOT_CREATE, emblemId, noSpaceForCount);
                        continue;
                    }

                    Item* item = player->StoreNewItem(dest, emblemId, true, Item::GenerateItemRandomPropertyId(emblemId));
                    if (amount > 0 && item)
                        player->SendNewItem(item, amount, true, false);
                    // <END>
                } while (result->NextRow());

                CharacterDatabase.Execute("UPDATE emblem_transferences SET active = 0, received_timestamp = CURRENT_TIMESTAMP WHERE receiver_guid = {} AND active = 1", player->GetSession()->GetGuidLow());
                ChatHandler(player->GetSession()).SendNotification("Thank you for using the emblem transfer service!");
                return OnGossipSelect(player, creature, sender, ACTION_CLOSE);
            }
        }

        // Player selected one of the emblem transfer options
        if (sender == GOSSIP_SENDER_MAIN)
        {
            uint32 minAmount = sConfigMgr->GetOption<uint32>("EmblemTransfer.minAmount", 10);

            // Get the character's emblems of the selected type
            uint32 emblems = 0;
            uint32 newSender = sender;
            switch (action)
            {
                case ACTION_TRANSFER_FROST:
                    newSender = GOSSIP_SENDER_TRANSFER_FROST;
                    emblems = player->GetItemCount(ITEM_EMBLEM_OF_FROST);
                    break;

                case ACTION_TRANSFER_TRIUMPH:
                    newSender = GOSSIP_SENDER_TRANSFER_TRIUMPH;
                    emblems = player->GetItemCount(ITEM_EMBLEM_OF_TRIUMPH);
                    break;

                case ACTION_TRANSFER_CONQUEST:
                    newSender = GOSSIP_SENDER_TRANSFER_CONQUEST;
                    emblems = player->GetItemCount(ITEM_EMBLEM_OF_CONQUEST);
                    break;

                case ACTION_TRANSFER_HEROISM:
                    newSender = GOSSIP_SENDER_TRANSFER_HEROISM;
                    emblems = player->GetItemCount(ITEM_EMBLEM_OF_HEROISM);
                    break;

                case ACTION_TRANSFER_VALOR:
                    newSender = GOSSIP_SENDER_TRANSFER_VALOR;
                    emblems = player->GetItemCount(ITEM_EMBLEM_OF_VALOR);
                    break;
            }

            if (emblems < minAmount)
            {
                ChatHandler(player->GetSession()).SendNotification("You don't have enough emblems! The minimum amount is %d", minAmount);
                return OnGossipSelect(player, creature, sender, ACTION_CLOSE);
            }

            SendCharactersList(player, creature, newSender, action);
        }
        // Player selected a character to transfer
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Last step: Amount of emblems", sender, action, "Enter the amount of emblems to transfer:", 0, true);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    // Step 3
    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code)
    {
        if (!isNumber(code))
        {
            ChatHandler(player->GetSession()).SendNotification("Please enter a valid number!");
            return OnGossipSelect(player, creature, sender, ACTION_CLOSE);
        }

        uint32 transferAmount;
        std::stringstream ss(code);
        ss >> transferAmount;

        uint32 emblemsCount = 0;
        uint32 emblemId = 0;
        // uint32 newSender = sender;
        float penalty = sConfigMgr->GetOption<float>("EmblemTransfer.penalty", 0.1f);

        switch (sender)
        {
            case GOSSIP_SENDER_TRANSFER_FROST:
                emblemId = ITEM_EMBLEM_OF_FROST;
                break;

            case GOSSIP_SENDER_TRANSFER_TRIUMPH:
                emblemId = ITEM_EMBLEM_OF_TRIUMPH;
                break;

            case GOSSIP_SENDER_TRANSFER_CONQUEST:
                emblemId = ITEM_EMBLEM_OF_CONQUEST;
                break;

            case GOSSIP_SENDER_TRANSFER_HEROISM:
                emblemId = ITEM_EMBLEM_OF_HEROISM;
                break;

            case GOSSIP_SENDER_TRANSFER_VALOR:
                emblemId = ITEM_EMBLEM_OF_VALOR;
                break;
        }
        // Deku: emblemId should NEVER be 0
        if (emblemId == 0)
        {
            ChatHandler(player->GetSession()).SendNotification("There was a problem processing your request. Please notify an administrator.");
            return OnGossipSelect(player, creature, sender, ACTION_CLOSE);
        }

        emblemsCount = player->GetItemCount(emblemId);
        if (emblemsCount < transferAmount)
        {
            ChatHandler(player->GetSession()).SendNotification("You don't have enough emblems!");
            return OnGossipSelect(player, creature, sender, ACTION_CLOSE);
        }

        ObjectGuid targetGuid = ObjectGuid::Create<HighGuid::Player>(action);
        uint32 receivedAmount = transferAmount * (1.0f - penalty);
        CharacterDatabase.Execute("INSERT INTO emblem_transferences(sender_guid, receiver_guid, emblem_entry, amount) VALUES ({}, {}, {}, {})", player->GetSession()->GetGuidLow(), targetGuid.GetCounter(), emblemId, receivedAmount);
        player->DestroyItemCount(emblemId, transferAmount, true, false);

        player->PlayerTalkClass->ClearMenus(); // Clear window before farewell
        AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Transfer completed! Log in with your other character to retrieve the emblems", GOSSIP_SENDER_MAIN, ACTION_CLOSE);
        SendGossipMenuFor(player,DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    void SendCharactersList(Player* player, Creature* /*creature*/, uint32 sender, uint32 /*action*/)
    {
        // Send characters list
        auto* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_GUID_NAME_BY_ACC);
        stmt->SetData(0, player->GetSession()->GetAccountId());
        PreparedQueryResult result = CharacterDatabase.Query(stmt);

        if (result) {
            do
            {
                Field* characterFields  = result->Fetch();
                uint32 guid             = characterFields[0].Get<uint32>();
                std::string name        = characterFields[1].Get<std::string>();
                if (!(guid == player->GetSession()->GetGuidLow()))
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, name, sender, guid);
            } while (result->NextRow());
        }
    }

    bool isNumber(const char* c)
    {
        const std::string s = c;
        // C++11
        return !s.empty() && std::find_if(s.begin(), s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
    }
};

void AddNpcEmblemTransferScripts()
{
    new npc_emblem_transfer();
}
