#include <AutoTrackSelect/ExpFroomMessages.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Gamemodes/OnlineTT/OTTRegional.hpp>

namespace Pulsar {
namespace UI {
bool ExpFroomMessages::isOnModeSelection = false;
s32 ExpFroomMessages::clickedButtonIdx = 0;

// void ExpFroomMessages::OnModeButtonClick(PushButton& button, u32 hudSlotId) {
//     this->clickedButtonIdx = button.buttonId;
//     this->OnActivate();
// }

// void ExpFroomMessages::OnCourseButtonClick(PushButton& button, u32 hudSlotId) {
//     CupsConfig* cupsConfig = CupsConfig::sInstance;
//     u32 clickedIdx = clickedButtonIdx;
//     s32 id = button.buttonId;
//     PulsarId pulsarId = static_cast<PulsarId>(id);
//     if (clickedIdx < 2) {
//         if (id == this->msgCount - 1) {
//             pulsarId = cupsConfig->RandomizeTrack();
//         }
//         else {
//             PulsarCupId cupId =  static_cast<PulsarCupId>(cupsConfig->ConvertTrack_IdxToPulsarId(id) / 4);
//             pulsarId = cupsConfig->ConvertTrack_PulsarCupToTrack(cupId, id % 4);
//             //pulsarId = cupsConfig->ConvertTrack_IdxToPulsarId(id); //vs or teamvs
//         }
//     }
//     else pulsarId = static_cast<PulsarId>(pulsarId + 0x20U); //Battle
//     cupsConfig->SetWinning(pulsarId);
//     PushButton& clickedButton = this->messages[0].buttons[clickedIdx];
//     clickedButton.buttonId = clickedIdx;
//     Pages::FriendRoomMessages::OnModeButtonClick(clickedButton, 0); //hudslot is unused
// }

// //kmWrite32(0x805dc47c, 0x7FE3FB78); //Get Page in r3
// static void OnStartButtonFroomMsgActivate() {
//     register ExpFroomMessages* msg;
//     asm(mr msg, r31;);

//     if (!Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS)) {
//         msg->onModeButtonClickHandler.ptmf = &Pages::FriendRoomMessages::OnModeButtonClick;
//         msg->msgCount = 4;
//     }
//     else {
//         for (int i = 0; i < 4; ++i) msg->messages[0].buttons[i].HandleDeselect(0, -1);
//         if (msg->isOnModeSelection) {
//             msg->isOnModeSelection = false;
//             if (msg->clickedButtonIdx >= 2) msg->msgCount = 10;
//             else msg->msgCount = CupsConfig::sInstance->GetEffectiveTrackCount() + 1;
//             msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnCourseButtonClick;

//         }
//         else {
//             msg->isOnModeSelection = true;
//             msg->msgCount = 4;
//             msg->onModeButtonClickHandler.ptmf = &ExpFroomMessages::OnModeButtonClick;
//         }
//     }
// }
// kmCall(0x805dc480, OnStartButtonFroomMsgActivate);
// //kmWrite32(0x805dc498, 0x60000000);
// //kmWrite32(0x805dc4c0, 0x60000000);

// static void OnBackPress(ExpFroomMessages& msg) {
//     if (Settings::Mgr::Get().GetUserSettingValue(Settings::SETTINGSTYPE_FROOM2, RADIO_HOSTWINS) && msg.location == 1) {
//         if (!msg.isOnModeSelection) {
//             msg.isEnding = false;
//             msg.OnActivate();
//         }
//         else msg.isOnModeSelection = false;
//     }
// }
// kmBranch(0x805dd32c, OnBackPress);

// static void OnBackButtonClick() {
//     OnBackPress(*SectionMgr::sInstance->curSection->Get<ExpFroomMessages>());
// }
// kmBranch(0x805dd314, OnBackButtonClick);

void CorrectRoomStartButton(Pages::Globe::MessageWindow& control, u32 bmgId, Text::Info* info) {
    if (bmgId == BMG_PLAY_GP || bmgId == BMG_PLAY_TEAM_GP) {
        const u32 hostContext   = System::sInstance->netMgr.hostContext;
        const bool isOTT        = hostContext & (1 << PULSAR_MODE_OTT);
        const bool isKO         = hostContext & (1 << PULSAR_MODE_KO);
        const bool isStartOPTWW = hostContext & (1 << PULSAR_STARTOPTWW);
        const bool isStartOTTWW = hostContext & (1 << PULSAR_STARTOTTWW);
        const bool isExtendedTeams = hostContext & (1 << PULSAR_EXTENDEDTEAMS);

        // WW start must win over KO/OTT
        if (isStartOPTWW) {
            bmgId = BMG_OPTWW_START_MESSAGE;
        }
        else if (isStartOTTWW) {
            bmgId = BMG_OTTWW_START_MESSAGE;
        }
        else if (isExtendedTeams) {
            bmgId = BMG_EXTENDEDTEAMS_PLAY;
        }
        else if (isOTT || isKO) {
            const bool isTeam = (bmgId == BMG_PLAY_TEAM_GP);
            bmgId = (BMG_PLAY_OTT - 1) + isOTT + isKO * 2 + isTeam * 3;
        }
    }

    control.SetMessage(bmgId, info);
}
kmCall(0x805e4df4, CorrectRoomStartButton);

static void OnStartButtonFroomMsgActivate() {
    register ExpFroomMessages* msg;
    asm(mr msg, r31;);
    msg->msgCount = 6;  // 4 normal + 6 worldwide options
}
kmCall(0x805dc480, OnStartButtonFroomMsgActivate);

u32 CorrectModeButtonsBMG(const RKNet::ROOMPacket& packet) {
    register u32 rowIdx;
    asm(mr rowIdx, r24;);  // r24 contains the actual message index
    register const ExpFroomMessages* messages;
    asm(mr messages, r19;);
    u32 bmgId;
    bmgId = Pages::FriendRoomManager::GetMessageBmg(packet, 0);

    switch (rowIdx) {
        case 4:
            return BMG_OPTWW_START_MESSAGE;
        case 5:
            return BMG_OTTWW_START_MESSAGE;
    }

    if (rowIdx == 0) {
        const Settings::Mgr& settings = Settings::Mgr::Get();
        const u32 isOTT = settings.GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ONLINE) == OTTSETTING_ONLINE_NORMAL;
        const u32 isKO = settings.GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_ENABLED) != KOSETTING_DISABLED;
        /*
            Read from the settings and not from the context: this is the host's own button,
            drawn before any ROOM packet has gone out, so the context is still empty here.
        */
        const bool isExtendedTeams = settings.GetUserSettingValue(Settings::SETTINGSTYPE_EXTENDEDTEAMS, RADIO_EXTENDEDTEAMSENABLED) == EXTENDEDTEAMS_ENABLED;

        if (isOTT && isKO) {
            bmgId = BMG_PLAY_OTTKO;
        } else if (isOTT) {
            bmgId = BMG_PLAY_OTT;
        } else if (isKO) {
            bmgId = BMG_PLAY_KO;
        } else if (isExtendedTeams) {
            bmgId = BMG_EXTENDEDTEAMS_PLAY;
        } else {
            bmgId = BMG_PLAY_GP;
        }
    }
    return bmgId;
}
kmCall(0x805dcb74, CorrectModeButtonsBMG);

static void RemapAndStoreSentMessage() {
    register u32 packet;
    register u32 manager;
    asm(mr packet, r30;);
    asm(mr manager, r28;);

    // Extract message from bits 8-23 (button ID shifted left by 8)
    u32 message = (packet >> 8) & 0xFFFF;
    if (message >= 4 && message <= 9) {
        // Clear message bits and set to 0 (treat as VS mode locally)
        packet = packet & 0xFF0000FF;
    }

    // Perform the original store: stw r30, 0x2c60(r28)
    *(volatile u32*)((u8*)manager + 0x2c60) = packet;
}
kmCall(0x805dce38, RemapAndStoreSentMessage);

}  // namespace UI
}  // namespace Pulsar