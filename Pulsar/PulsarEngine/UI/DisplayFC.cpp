#include <UI/DisplayFC.hpp>
#include <UI/ExtendedTeamSelect/ExtendedTeamManager.hpp>

namespace Pulsar {
namespace UI {

//Switch between Names, Times, FCs by Rambo
void ExpGPVSLeaderboardTotal::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

void ExpGPVSLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}

// Apply old toggle logic, set default display type
void ExpGPVSLeaderboardUpdate::BeforeEntranceAnimations() {
    if(System::sInstance->IsContext(PULSAR_MODE_OTT)) {
        setLeaderboardDisplayType(LEADERBOARD_DISPLAY_TIMES);
    } else {
        setLeaderboardDisplayType(LEADERBOARD_DISPLAY_NAMES);
    }

    fillLeaderboardResults(GetRowCount(), this->results);
}

/*
    How many players a team may hold before the regular board stops fitting them, indexed by
    how many teams are actually in play. With 0 or 1 team it is always irregular.
*/
static const int ALLOWED_PLAYER_PER_TEAM_COUNT[TEAM_COUNT + 1] = { 0, 0, 6, 4, 3, 2, 2 };

/*
    The only way into the extended team total pages: without this override they are created
    but nothing reaches them, and the end of a GP shows the ordinary total board instead.
*/
PageId ExpGPVSLeaderboardUpdate::GetNextPage() const {
    if(ExtendedTeamManager::IsActivated()) {
        RacedataScenario& scenario = Racedata::sInstance->racesScenario;
        int teamCount = 0;
        int numPlayerPerTeam[TEAM_COUNT] = { 0 };

        for(int i = 0; i < scenario.playerCount; ++i) {
            const ExtendedTeamID team = ExtendedTeamManager::sInstance->GetPlayerTeam(i);
            if(team < TEAM_COUNT) ++numPlayerPerTeam[team];
        }

        for(int i = 0; i < TEAM_COUNT; ++i) {
            if(numPlayerPerTeam[i] > 0) ++teamCount;
        }

        for(int i = 0; i < TEAM_COUNT; ++i) {
            if(numPlayerPerTeam[i] > ALLOWED_PLAYER_PER_TEAM_COUNT[teamCount]) {
                return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL_IRREGULAR);
            }
        }

        return static_cast<PageId>(PULPAGE_EXTENDEDTEAMS_RESULT_TOTAL);
    }

    return Pages::GPVSLeaderboardUpdate::GetNextPage();
}

void ExpWWLeaderboardUpdate::OnUpdate() {
    if (checkLeaderboardDisplaySwapInputs()) {
        nextLeaderboardDisplayType();
        fillLeaderboardResults(GetRowCount(), this->results);
    }
}
static LeaderboardDisplayType displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;

void setLeaderboardDisplayType(LeaderboardDisplayType type) {
    displayLeaderboardType = type;
}

LeaderboardDisplayType getLeaderboardDisplayType() {
    return displayLeaderboardType;
}

// In case we want to add more display types (FC, flags, whatever..)
void nextLeaderboardDisplayType() {
    if (displayLeaderboardType == LEADERBOARD_DISPLAY_NAMES) {
        displayLeaderboardType = LEADERBOARD_DISPLAY_TIMES;
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_TIMES) {
        if (RKNet::Controller::sInstance->roomType == RKNet::ROOMTYPE_NONE) {
            displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;
        } else {
            displayLeaderboardType = LEADERBOARD_DISPLAY_FC;
        }
    } else if (displayLeaderboardType == LEADERBOARD_DISPLAY_FC) {
        displayLeaderboardType = LEADERBOARD_DISPLAY_NAMES;
    }
}

bool isSectionSpectatorLiveView(SectionId id) {
    return id == SECTION_P1_WIFI_VS_LIVEVIEW || id == SECTION_P2_WIFI_VS_LIVEVIEW || id == SECTION_P1_WIFI_BT_LIVEVIEW || id == SECTION_P2_WIFI_BT_LIVEVIEW;
}

void fillLeaderboardResults(int count, CtrlRaceResult** results) {
    for(int i = 0; i < (count & 0xff); ++i) {
        const int position = (i + 1) & 0xff;
        const u8 playerId = Raceinfo::sInstance->playerIdInEachPosition[position - 1];

        if(displayLeaderboardType == LEADERBOARD_DISPLAY_TIMES) {
            results[i]->FillFinishTime(playerId);
        } else if(displayLeaderboardType == LEADERBOARD_DISPLAY_NAMES) {
            results[i]->FillName(playerId);
        } else if(displayLeaderboardType == LEADERBOARD_DISPLAY_FC) {
            if (playerId < 12) {
                u32 hudSlot = Racedata::sInstance->GetHudSlotId(playerId);
                u64 fc = RKNet::USERHandler::sInstance->receivedPackets[playerId].fc;

                // This is the local player, so we need to get the FC from the packet we send.
                if (hudSlot < 2 && !isSectionSpectatorLiveView(SectionMgr::sInstance->curSection->sectionId)) {
                    fc = RKNet::USERHandler::sInstance->toSendPacket.fc;
                }

                u32 fcParts[3];
                for (int j = 0; j < 3; ++j) {
                    fcParts[j] = fc % 10000;
                    fc /= 10000;
                }

                wchar_t fcText[16];
                swprintf(fcText, 16, L"%04d-%04d-%04d", fcParts[2], fcParts[1], fcParts[0]);

                if (wcscmp(fcText, L"0000-0000-0000") == 0) {
                    wcsncpy(fcText, L"----", 5);
                    continue;
                }

                Text::Info textInfo;
                textInfo.strings[0] = fcText;

                results[i]->SetTextBoxMessage("player_name", UI::BMG_TEXT, &textInfo);
                results[i]->ResetTextBoxMessage("time");
            }
        }
    }
}

bool checkLeaderboardDisplaySwapInputs() {
    const Input::RealControllerHolder* controllerHolder = SectionMgr::sInstance->pad.padInfos[0].controllerHolder;
    const ControllerType controllerType = controllerHolder->curController->GetType();
    const u16 inputs = controllerHolder->inputStates[0].buttonRaw;
    const u16 newInputs = (inputs & ~controllerHolder->inputStates[1].buttonRaw);

    bool swapDisplayType = false;
    switch (controllerType) {
    case NUNCHUCK:
    case WHEEL:
        swapDisplayType = (newInputs & WIIMOTE_DPAD_BUTTONS) != 0;
        break;
    case CLASSIC:
        swapDisplayType = (newInputs & (WPAD::WPAD_CL_TRIGGER_L | WPAD::WPAD_CL_TRIGGER_R | CLASSIC_DPAD_BUTTONS)) != 0;
        break;
    default:
        swapDisplayType = (newInputs & (PAD::PAD_BUTTON_L | PAD::PAD_BUTTON_R | GC_DPAD_BUTTONS)) != 0;
        break;
    }

    return swapDisplayType;
}
}//namespace UI
}//namespace Pulsar