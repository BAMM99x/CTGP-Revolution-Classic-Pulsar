#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Menu/CupSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Page/Other/GhostSelect.hpp>
#include <MarioKartWii/UI/Page/Other/Votes.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpCupSelect.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Network/PacketExpansion.hpp>
#include <core/nw4r/lyt/TextBox.hpp>

namespace Pulsar {
namespace UI {

static void BuildBlockedTrackName(wchar_t* dest, const wchar_t* src, u32 maxLen);

bool IsTrackBlocked(PulsarId id) {
    System* system = System::sInstance;
    if (!system || !system->IsContext(PULSAR_CT)) return false;

    const u32 blockingCount = system->GetInfo().GetTrackBlocking();
    PulsarId* arr = system->netMgr.lastTracks;
    if (blockingCount == 0 || arr == nullptr) return false;

    for (u32 i = 0; i < blockingCount; i++) {
        PulsarId cur = arr[i];

        if (cur == PULSARID_NONE || cur == 0xFFFF)
            continue;

        if (cur == id)
            return true;
    }
    return false;
}

// Change brctr names
kmWrite24(0x808a85ef, 'PUL'); //used by 807e5754

static void LoadCtrlMenuCourseSelectCupBRCTR(ControlLoader& loader, const char* folderName, const char* ctrName,
                                             const char* variant, const char** animNames) {
    loader.Load(UI::buttonFolder, "PULrseSelectCup", variant, animNames);  // Move to button to avoid duplication of cup icon tpls
}
kmCall(0x807e4538, LoadCtrlMenuCourseSelectCupBRCTR);

static void LoadCorrectTrackListBox(ControlLoader& loader, const char* folder, const char* normalBrctr, const char* variant, const char** anims) {
    loader.Load(folder, "PULSelectNULL", variant, anims);
}
kmCall(0x807e5f24, LoadCorrectTrackListBox);

/*
    Variant names do not live next to the main track names. The pack creator writes them
    at VARIANT_TRACKS_BASE + (trackIdx << 4) + variantIdx, so a track can carry up to 15
    variants without colliding with the next one. Variant 0 is the main track and keeps
    its ordinary BMG_TRACKS id.
*/
int GetTrackVariantBMGId(PulsarId pulsarId, u8 variantIdx) {
    const u32 realId = CupsConfig::ConvertTrack_PulsarIdToRealId(pulsarId);
    if (CupsConfig::IsReg(pulsarId)) {
        const u32 base = (realId > 32) ? BMG_BATTLE : BMG_REGS;
        return base + realId;
    }
    if (variantIdx == 8) variantIdx = 0;
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (variantIdx == 0 || cupsConfig == nullptr || cupsConfig->GetTrack(pulsarId).variantCount == 0) {
        return BMG_TRACKS + realId;
    }
    return VARIANT_TRACKS_BASE + (realId << 4) + static_cast<u32>(variantIdx);
}

/*
    Same as GetTrackBMGId(id, false), except the variant comes from the caller instead of
    from GetCurVariantIdx(). Online that matters: GetCurVariantIdx() is what *this* console
    picked, so using it for someone else's vote names the wrong variant.
*/
int GetTrackBMGIdForVariant(PulsarId pulsarId, u8 variantIdx) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig == nullptr) return BMG_TRACKS;
    if (CupsConfig::IsReg(pulsarId)) return GetTrackVariantBMGId(pulsarId, 0);

    const u8 variantCount = cupsConfig->GetTrack(pulsarId).variantCount;
    if (variantCount == 0) return GetTrackVariantBMGId(pulsarId, 0);

    // Out of range means the sender never told us; fall back to the common name rather
    // than naming a variant at random.
    if (variantIdx > variantCount) return BMG_TRACKS + CupsConfig::ConvertTrack_PulsarIdToRealId(pulsarId);
    return GetTrackVariantBMGId(pulsarId, variantIdx);
}

int GetTrackBMGId(PulsarId pulsarId, bool useCommonName) {
    const u32 realId = CupsConfig::ConvertTrack_PulsarIdToRealId(pulsarId);
    if (CupsConfig::IsReg(pulsarId)) {
        const u32 base = (realId > 32) ? BMG_BATTLE : BMG_REGS;
        return base + realId;
    }

    u8 variantIdx = 0;
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    if (cupsConfig != nullptr && !useCommonName && cupsConfig->GetTrack(pulsarId).variantCount > 0) {
        if (pulsarId == cupsConfig->GetWinning()) variantIdx = cupsConfig->GetCurVariantIdx();
    }
    return GetTrackVariantBMGId(pulsarId, variantIdx);
}

int GetTrackBMGByRowIdx(u32 cupTrackIdx) {
    const Pages::CupSelect* cup = SectionMgr::sInstance->curSection->Get<Pages::CupSelect>();
    PulsarCupId curCupId = cup ? 
        static_cast<PulsarCupId>(cup->ctrlMenuCupSelectCup.curCupID) :
        PULSARCUPID_FIRSTREG;

    PulsarId trackId = CupsConfig::sInstance->ConvertTrack_PulsarCupToTrack(curCupId, cupTrackIdx);
    return GetTrackBMGId(trackId, true);
}
kmWrite32(0x807e6184, 0x7FA3EB78);
kmCall(0x807e6188, &GetTrackBMGByRowIdx);
kmWrite32(0x807e6088, 0x7F63DB78);
kmCall(0x807e608c, GetTrackBMGByRowIdx);

static wchar_t s_blockedCupPreviewBuffer[4][0x100];

static void SetCupPreviewTrackMessageImpl(LayoutUIControl* control, u32 bmgId, const Text::Info* info, u32 trackIdx){
    const Pages::CupSelect* cup = SectionMgr::sInstance->curSection->Get<Pages::CupSelect>();
    PulsarCupId curCupId = cup ?
        static_cast<PulsarCupId>(cup->ctrlMenuCupSelectCup.curCupID) :
        PULSARCUPID_FIRSTREG;

    PulsarId trackId =
        CupsConfig::sInstance->ConvertTrack_PulsarCupToTrack(curCupId, trackIdx);

    if (IsTrackBlocked(trackId)) {

    const wchar_t* originalText =
    CupsConfig::IsReg(trackId) ? GetCommonMsg(bmgId, control) : GetCustomMsg(bmgId);


        if (originalText != nullptr) {
            BuildBlockedTrackName(s_blockedCupPreviewBuffer[trackIdx], originalText, 0x100);

            Text::Info blockedInfo;
            blockedInfo.strings[0] = s_blockedCupPreviewBuffer[trackIdx];
            control->SetMessage(BMG_TEXT, &blockedInfo);
            return;
        }
    }

    control->SetMessage(bmgId, info);
}

static void SetCupPreviewTrackMessage_R27(LayoutUIControl* control, u32 bmgId, const Text::Info* info) {
    register u32 trackIdx;
    asm(mr trackIdx, r27;);
    SetCupPreviewTrackMessageImpl(control, bmgId, info, trackIdx);
}
kmCall(0x807e609c, SetCupPreviewTrackMessage_R27);

static void SetCupPreviewTrackMessage_R29(LayoutUIControl* control, u32 bmgId, const Text::Info* info) {
    register u32 trackIdx;
    asm(mr trackIdx, r29;);
    SetCupPreviewTrackMessageImpl(control, bmgId, info, trackIdx);
}
kmCall(0x807e6198, SetCupPreviewTrackMessage_R29);

int GetCurTrackBMG() {
    return GetTrackBMGId(CupsConfig::sInstance->GetWinning(), false);
}

static void SetVSIntroBmgId(LayoutUIControl* trackName) {
    u32 bmgId = GetCurTrackBMG();
    Text::Info info;
    info.bmgToPass[0] = bmgId;
    u32 authorId;
    if (bmgId < BMG_TRACKS) authorId = BMG_NINTENDO;
    else authorId = bmgId + BMG_AUTHORS - BMG_TRACKS;
    info.bmgToPass[1] = authorId;
    trackName->SetMessage(BMG_INFO_DISPLAY, &info);
}
kmCall(0x808552cc, SetVSIntroBmgId);

static void SetAwardsResultCupInfo(LayoutUIControl& awardType, const char* textBoxName, u32 bmgId, Text::Info& info) {
    PulsarCupId id = CupsConfig::sInstance->lastSelectedCup;
    if (!CupsConfig::IsRegCup(id)) {
        awardType.layout.GetPaneByName("cup_icon")->flag &= ~1;
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        u32 cupBmgId;
        u16 iconCount = System::sInstance->GetInfo().GetCupIconCount();
        if (realCupId > iconCount - 1) {
            wchar_t cupName[0x20];
            swprintf(cupName, 0x20, L"Cup %d", realCupId);
            info.strings[0] = cupName;
            cupBmgId = BMG_TEXT;
        } else
            cupBmgId = BMG_CUPS + realCupId;
        info.bmgToPass[1] = cupBmgId;
    }
    awardType.SetTextBoxMessage(textBoxName, bmgId, &info);
}
kmCall(0x805bcb88, SetAwardsResultCupInfo);

static void SetGPIntroInfo(LayoutUIControl& titleText, u32 bmgId, Text::Info& info) {
    PulsarCupId id = CupsConfig::sInstance->lastSelectedCup;
    if (!CupsConfig::IsRegCup(id)) {
        titleText.layout.GetPaneByName("cup_icon")->flag &= ~1;
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        u32 cupBmgId;
        u16 iconCount = System::sInstance->GetInfo().GetCupIconCount();
        if (realCupId > iconCount - 1) {
            wchar_t cupName[0x20];
            swprintf(cupName, 0x20, L"Cup %d", realCupId);
            info.strings[0] = cupName;
            cupBmgId = BMG_TEXT;
        } else
            cupBmgId = BMG_CUPS + realCupId;
        info.bmgToPass[1] = cupBmgId;
    }
    titleText.SetMessage(bmgId, &info);
}
kmCall(0x808553b4, SetGPIntroInfo);

static void SetGPBottomText(CtrlMenuInstructionText& bottomText, u32 bmgId, Text::Info& info) {
    register ExpCupSelect* cupPage;
    asm(mr cupPage, r31;);
    PulsarCupId id = static_cast<PulsarCupId>(cupPage->ctrlMenuCupSelectCup.curCupID);

    if (!CupsConfig::IsRegCup(id)) {
        u32 realCupId = CupsConfig::ConvertCup_PulsarIdToRealId(id);
        register u32 cc;
        asm(mr cc, r28;);
        u8 status = Settings::Mgr::GetGPStatus(realCupId, cc);
        u32 trophyBmg;
        u32 rankBmg;
        if (status == 0xFF) {
            trophyBmg = BMG_GP_BLANK;
            rankBmg = BMG_GP_BLANK;
        } else {
            trophyBmg = BMG_GP_GOLD_TROPHY + Settings::Mgr::ComputeTrophyFromStatus(status);
            rankBmg = BMG_GP_RANK_3STARS + Settings::Mgr::ComputeRankFromStatus(status);
        }
        info.bmgToPass[1] = trophyBmg;
        info.bmgToPass[2] = rankBmg;
    }
    bottomText.SetMessage(bmgId, &info);
}
kmCall(0x80841720, SetGPBottomText);

static void SetGhostInfoTrackBMG(GhostInfoControl* control, const char* textBoxName) {
    control->SetTextBoxMessage(textBoxName, GetCurTrackBMG());
}
kmCall(0x805e2a4c, SetGhostInfoTrackBMG);

kmWrite32(0x808406e8, 0x388000ff); //store 0xFF on timeout instead of -1
kmWrite32(0x808415ac, 0x388000ff);
kmWrite32(0x80643004, 0x3be000ff);
kmWrite32(0x808394e8, 0x388000ff);
kmWrite32(0x80644104, 0x3b5b0000);
static void CourseVoteBMG(VoteControl* vote, bool isCourseIdInvalid, PulsarId courseVote, MiiGroup& miiGroup, u32 playerId, bool isLocalPlayer, u32 team) {
    u32 bmgId = courseVote;
    if (bmgId != 0x1101 && bmgId < 0x2498) bmgId = GetTrackBMGId(courseVote, true);
    vote->Fill(isCourseIdInvalid, bmgId, miiGroup, playerId, isLocalPlayer, team);
    SetVoteControlMessage(*vote, bmgId, courseVote, playerId);
}
kmCall(0x806441b8, CourseVoteBMG);

static bool BattleArenaBMGFix(SectionId sectionId) {
    register PulsarId id;
    asm(mr id, r28;);
    CupsConfig::sInstance->SetWinning(id, 0);
    return IsOnlineSection(sectionId);
}
kmCall(0x8083d02c, BattleArenaBMGFix);

//kmWrite32(0x80644340, 0x7F64DB78);
static void WinningTrackBMG(PulsarId winningCourse) {
    register Pages::Vote* vote;
    asm(mr vote, r27;);

    u8 variantIdx = 0;
    if (!CupsConfig::IsReg(winningCourse)) {
        const CupsConfig* cupsConfig = CupsConfig::sInstance;
        if (cupsConfig->GetTrack(winningCourse).variantCount > 0) {
            RKNet::Controller* controller = RKNet::Controller::sInstance;
            RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
            const Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
            const u8 hostAid = sub.hostAid;

            if (hostAid == sub.localAid) {
                variantIdx = handler.toSendPacket.variantIdx;
            } else {
                variantIdx = handler.receivedPackets[hostAid].variantIdx;
            }
        }
    }
    vote->trackBmgId = GetTrackVariantBMGId(winningCourse, variantIdx);
}
kmCall(0x80644344, WinningTrackBMG);

//Rewrote InitSelf to start with correct TPLs
static void ExtCupSelectCupInitSelf(CtrlMenuCupSelectCup* cups) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    PulsarCupId selCup = cupsConfig->lastSelectedCup;
    cups->curCupID = selCup;
    PushButton** buttons = reinterpret_cast<PushButton**>(cups->childrenGroup.controlArray);

    for (int i = 0; i < 8; ++i) {
        const PulsarCupId id = cupsConfig->GetNextCupId(selCup, i - cupsConfig->lastSelectedCupButtonIdx);
        buttons[i]->buttonId = id;
        ExpCupSelect::UpdateCupData(id, *buttons[i]);
        buttons[i]->SetOnClickHandler(cups->onCupButtonClickHandler, 0);
        buttons[i]->SetOnSelectHandler(cups->onCupButtonSelectHandler);
        buttons[i]->SetPlayerBitfield(SectionMgr::sInstance->curSection->Get<Pages::CupSelect>()->GetPlayerBitfield());
    }
    buttons[cupsConfig->lastSelectedCupButtonIdx]->SelectInitial(0);
};
kmWritePointer(0x808d324c, ExtCupSelectCupInitSelf); //807e5894

static void ExtCourseSelectCupInitSelf(CtrlMenuCourseSelectCup* courseCups) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    for (int i = 0; i < 8; ++i) {
        CtrlMenuCourseSelectCupSub& cur = courseCups->cupIcons[i];
        const PulsarCupId id = cupsConfig->GetNextCupId(cupsConfig->lastSelectedCup, i - cupsConfig->lastSelectedCupButtonIdx);
        ExpCupSelect::UpdateCupData(id, cur);
        cur.animator.GetAnimationGroupById(0).PlayAnimationAtFrame(0, 0.0f);
        const bool clicked = cupsConfig->lastSelectedCupButtonIdx == i ? true : false;
        cur.animator.GetAnimationGroupById(1).PlayAnimationAtFrame(!clicked, 0.0f);
        cur.animator.GetAnimationGroupById(2).PlayAnimationAtFrame(!clicked, 0.0f);
        cur.animator.GetAnimationGroupById(3).PlayAnimationAtFrame(clicked, 0.0f);
        cur.selected = clicked;
        cur.SetRelativePosition(courseCups->positionAndscale[1]);
    }
    const Section* curSection = SectionMgr::sInstance->curSection;
    Pages::CupSelect* cup = curSection->Get<Pages::CupSelect>();
    NoteModelControl* positionArray = cup->modelPosition;

    switch (cup->extraControlNumber) {
        case (2):
            positionArray[0].positionAndscale[1].position.x = -52.0f;
            positionArray[0].positionAndscale[1].position.y = -8.0f;
            positionArray[0].positionAndscale[1].scale.x = 0.875f;
            positionArray[0].positionAndscale[1].scale.z = 0.875f;
            positionArray[1].positionAndscale[1].position.x = -52.0f;
            positionArray[1].positionAndscale[1].position.y = -13.0f;
            positionArray[1].positionAndscale[1].scale.x = 0.875f;
            positionArray[1].positionAndscale[1].scale.z = 0.875f;
            break;
        case (1):
            positionArray[0].positionAndscale[1].position.x = -32.0f;
            positionArray[0].positionAndscale[1].position.y = -32.0f;
            positionArray = curSection->Get<Pages::CourseSelect>()->modelPosition;
            positionArray[0].positionAndscale[1].position.x = -32.0f;
            positionArray[0].positionAndscale[1].position.y = -32.0f;
            break;
        case (4):
            positionArray[3].positionAndscale[1].position.x = 64.0f;
            positionArray[3].positionAndscale[1].position.y = -55.25f;
            positionArray[3].positionAndscale[1].scale.x = 0.6875f;
            positionArray[3].positionAndscale[1].scale.z = 0.6875f;
        case (3):
            positionArray[0].positionAndscale[1].position.x = 64.0f;
            positionArray[0].positionAndscale[1].position.y = -64.0f;
            positionArray[0].positionAndscale[1].scale.x = 0.6875f;
            positionArray[0].positionAndscale[1].scale.z = 0.6875f;
            positionArray[1].positionAndscale[1].position.x = 64.0f;
            positionArray[1].positionAndscale[1].position.y = -64.0f;
            positionArray[1].positionAndscale[1].scale.x = 0.6875f;
            positionArray[1].positionAndscale[1].scale.z = 0.6875f;
            positionArray[2].positionAndscale[1].position.x = 64.0f;
            positionArray[2].positionAndscale[1].position.y = -55.25f;
            positionArray[2].positionAndscale[1].scale.x = 0.6875f;
            positionArray[2].positionAndscale[1].scale.z = 0.6875f;
            break;
    }
};
kmWritePointer(0x808d3190, ExtCourseSelectCupInitSelf); //807e45c0

//BMG escape that switches the text colour to red, as a raw wchar_t sequence.
static const wchar_t COLOR_ESCAPE_RED[] = {0x001A, 0x0800, 0x0001, 0x0017, 0x0000};
static const u32 COLOR_ESCAPE_RED_LEN = 4;

/*
    Strips every 0x1A escape out of a BMG string. A track name that already carries its own
    colour would otherwise cancel the red we put in front of it, so the name has to be flattened
    before it is recoloured.
*/
static void RemoveAllEscapeSequences(wchar_t* dest, const wchar_t* src, u32 maxLen) {
    u32 out = 0;
    while (*src != L'\0' && out + 1 < maxLen) {
        if (src[0] == 0x001A) {
            const u8* escapeBytes = reinterpret_cast<const u8*>(src);
            const u8 escapeLength = escapeBytes[2];
            src = reinterpret_cast<const wchar_t*>(escapeBytes + escapeLength);
        }
        else {
            dest[out++] = *src++;
        }
    }
    dest[out] = L'\0';
}

static void BuildBlockedTrackName(
    wchar_t* dest,
    const wchar_t* src,
    u32 maxLen
) {
    if (maxLen <= COLOR_ESCAPE_RED_LEN + 1) {
        dest[0] = L'\0';
        return;
    }

    for (u32 i = 0; i < COLOR_ESCAPE_RED_LEN; ++i) dest[i] = COLOR_ESCAPE_RED[i];
    RemoveAllEscapeSequences(dest + COLOR_ESCAPE_RED_LEN, src, maxLen - COLOR_ESCAPE_RED_LEN);
}

static wchar_t s_blockedTrackNameBuffer[4][0x100];

void SetCourseButtonMessage(PushButton& button, u32 bmgId, PulsarId trackId, u32 buttonIdx) {
    if (IsTrackBlocked(trackId)) {
    const wchar_t* originalText =
    CupsConfig::IsReg(trackId) ? GetCommonMsg(bmgId, &button) : GetCustomMsg(bmgId);

        if (originalText != nullptr) {
            BuildBlockedTrackName(s_blockedTrackNameBuffer[buttonIdx], originalText, 0x100);

            Text::Info info;
            info.strings[0] = s_blockedTrackNameBuffer[buttonIdx];
            button.SetMessage(BMG_TEXT, &info);
            return;
        }
    }

    button.SetMessage(bmgId);
}

static wchar_t s_blockedVoteNameBuffer[12][0x100];

void SetVoteControlMessage(VoteControl& vote, u32 bmgId, PulsarId courseVote, u32 playerId) {
    if (IsTrackBlocked(courseVote) && playerId < 12) {

const wchar_t* originalText =
    CupsConfig::IsReg(courseVote) ? GetCommonMsg(bmgId, &vote) : GetCustomMsg(bmgId);


        if (originalText != nullptr) {
            BuildBlockedTrackName(s_blockedVoteNameBuffer[playerId], originalText, 0x100);

            Text::Info info;
            info.strings[0] = s_blockedVoteNameBuffer[playerId];
            vote.SetMessage(BMG_TEXT, &info);
            return;
        }
    }
}

void ApplyBlockedColorToString(wchar_t* dest, const wchar_t* src, u32 maxLen) {
    BuildBlockedTrackName(dest, src, maxLen);
}

static void ExtCourseSelectCourseInitSelf(CtrlMenuCourseSelectCourse* course) {
    const CupsConfig* cupsConfig = CupsConfig::sInstance;
    const Section* curSection = SectionMgr::sInstance->curSection;
    const Pages::CupSelect* cupPage = curSection->Get<Pages::CupSelect>();
    Pages::CourseSelect* coursePage = curSection->Get<Pages::CourseSelect>();
    //channel ldb stuff ignored
    const u32 cupId = cupPage->clickedCupId;

    PushButton* toSelect = &course->courseButtons[0];
    for (int i = 0; i < 4; ++i) {
        PushButton& curButton = course->courseButtons[i];
        curButton.buttonId = i;
        const u32 bmgId = GetTrackBMGByRowIdx(i);
        PulsarId trackId = cupsConfig->ConvertTrack_PulsarCupToTrack(cupsConfig->lastSelectedCup, i);
        SetCourseButtonMessage(curButton, bmgId, trackId, i);

        if (trackId == cupsConfig->GetSelected()) {
            toSelect = &curButton;
        }
    };
    coursePage->SelectButton(*toSelect);
};
kmWritePointer(0x808d30d8, ExtCourseSelectCourseInitSelf); //807e5118

//Multiplayer Fix
kmWrite32(0x807e56d4, 0x60000000);
kmWrite32(0x807e5f04, 0x60000000);

//TPL
//CupSelectCup patch, disable picture panes
kmWrite32(0x807e57a4, 0x60000000);
kmWrite32(0x807e57bc, 0x60000000);
kmWrite32(0x807e57d4, 0x60000000);

//CourseSelectCup patch, disable picture panes
kmWrite32(0x807e4550, 0x60000000);
kmWrite32(0x807e4568, 0x60000000);
kmWrite32(0x807e4580, 0x60000000);
}//namespace UI
}//namespace Pulsar