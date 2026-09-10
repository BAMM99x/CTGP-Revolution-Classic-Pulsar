#include <UI/SelectStage/VariantSelect.hpp>
#include <Ghost/UI/ExpGhostSelect.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/UI/Page/Menu/CourseSelect.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCourse.hpp>
#include <MarioKartWii/UI/Page/Other/SELECTStageMgr.hpp>
#include <MarioKartWii/UI/Ctrl/CountDown.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>

namespace Pulsar {
namespace UI {

static bool IsTimeTrialVariantMenu() {
    return Racedata::sInstance != nullptr && Racedata::sInstance->menusScenario.settings.gamemode == MODE_TIME_TRIAL;
}

VariantSelect::VariantSelect() {
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &VariantSelect::OnBackPress;
    this->onBackClickHandler.subject = this;
    this->onBackClickHandler.ptmf = &VariantSelect::OnBackButtonClick;
    this->onVariantButtonSelectHandler.subject = this;
    this->onVariantButtonSelectHandler.ptmf = &VariantSelect::OnVariantButtonSelect;
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, this->onBackPressHandler, false, false);
    selectedPulsarId = PULSARID_NONE;
    baseRowIdx = 0;
    highlightedVariantIdx = 0;
    variantButtonsPopulated = false;
    ResetVariantButtonState();
}

void VariantSelect::OnActivate() {
    Pages::CourseSelect::OnActivate();
    ToggleCourseSelectDecor(true);
    selectedPulsarId = CupsConfig::sInstance->GetSelected();
    PopulateVariantButtons();
    UpdateBottomText();
}

void VariantSelect::OnDeactivate() {
    variantButtonsPopulated = false;
    ResetVariantButtonState();
    baseRowIdx = 0;
    highlightedVariantIdx = 0;
    if(CupsConfig::sInstance != nullptr) CupsConfig::sInstance->ClearPendingVariant();
    ToggleCourseSelectDecor(false);
    Pages::CourseSelect::OnDeactivate();
}

UIControl* VariantSelect::CreateControl(u32 controlId) { return Pages::CourseSelect::CreateControl(controlId); }

/*
    Online the vote timer keeps running while this page is up. If it expires here the section
    still expects a vote, so hand it a random track rather than letting the page sit there.
*/
void VariantSelect::BeforeControlUpdate() {
    Pages::SELECTStageMgr* selectStageMgr = SectionMgr::sInstance->curSection->Get<Pages::SELECTStageMgr>();
    if(selectStageMgr != nullptr) {
        CountDown* timer = &selectStageMgr->countdown;
        if(timer->countdown <= 0) {
            CupsConfig* cups = CupsConfig::sInstance;
            if(cups != nullptr) {
                cups->ClearPendingVariant();
                cups->SetSelected(static_cast<PulsarId>(RANDOM));
            }
            this->OnTimeout();
            return;
        }
    }
    Pages::CourseSelect::BeforeControlUpdate();
}

void VariantSelect::AfterControlUpdate() {
    if(variantButtonsPopulated) ApplyVariantButtonState();
}

void VariantSelect::ToggleCourseSelectDecor(bool hidden) {
    this->ctrlMenuCourseSelectCup.isHidden = hidden;
    if(this->titleText) this->titleText->isHidden = hidden;
    if(this->bottomText) this->bottomText->isHidden = hidden;
    for(u32 i = 0; i < this->externControlCount; ++i) {
        PushButton* ctrl = this->externControls[i];
        if(ctrl) {
            ctrl->isHidden = hidden;
            ctrl->manipulator.inaccessible = hidden;
        }
    }
}

void VariantSelect::OnInit() {
    Pages::CourseSelect::OnInit();
    this->backButton.SetOnClickHandler(this->onBackClickHandler, 0);
    for(u32 i = 0; i < 4; ++i) {
        this->CtrlMenuCourseSelectCourse.courseButtons[i].SetOnSelectHandler(this->onVariantButtonSelectHandler);
    }
}

void VariantSelect::UpdateBottomText() {
    if(this->bottomText == nullptr) return;
    if(!IsTimeTrialVariantMenu() || this->selectedPulsarId == PULSARID_NONE) return;

    u32 bmgId = 0;
    const Text::Info text = GetCourseBottomText(this->selectedPulsarId, this->highlightedVariantIdx, &bmgId);
    this->bottomText->isHidden = false;
    this->bottomText->SetMessage(bmgId, &text);
}

void VariantSelect::OnBackPress(u32 hudSlotId) {
    if(CupsConfig::sInstance != nullptr) CupsConfig::sInstance->ClearPendingVariant();
    this->PlaySound(SOUND_ID_BACK_PRESS, 0);
    this->LoadPrevPageById(PAGE_COURSE_SELECT, this->backButton);
}

void VariantSelect::OnBackButtonClick(PushButton& button, u32 hudSlotId) {
    OnBackPress(hudSlotId);
}

void VariantSelect::OnVariantButtonSelect(PushButton& button, u32 hudSlotId) {
    const u32 variantIdx = this->GetVariantIndexForButton(button);
    if(variantIdx == 0xFFFFFFFF) return;
    this->highlightedVariantIdx = static_cast<u8>(variantIdx);
    this->UpdateBottomText();
}

void VariantSelect::PopulateVariantButtons() {
    CupsConfig* cups = CupsConfig::sInstance;
    if(!cups) return;
    if(selectedPulsarId == PULSARID_NONE) return;

    ResetVariantButtonState();

    if(!cups->IsReg(selectedPulsarId)) {
        const Track& track = cups->GetTrack(selectedPulsarId);
        const u32 maxButtons = 4;
        u32 displayCount = track.variantCount + 1;  // +1 for the main track
        if(displayCount > maxButtons) displayCount = maxButtons;
        for(u32 i = 0; i < displayCount; ++i) {
            variantButtonVariants[i] = static_cast<u8>(i);
        }
        variantButtonsPopulated = (displayCount > 0);
    }
    else {
        variantButtonsPopulated = false;
    }

    ApplyVariantButtonState();
    if(variantButtonsPopulated) {
        const u8 desiredVariantIdx = cups->GetLastSelectedVariant(selectedPulsarId);
        highlightedVariantIdx = desiredVariantIdx;
        u32 desiredButtonIdx = 0;
        for(u32 i = 0; i < 4; ++i) {
            if(variantButtonVariants[i] == desiredVariantIdx) {
                desiredButtonIdx = i;
                break;
            }
        }
        this->CtrlMenuCourseSelectCourse.courseButtons[desiredButtonIdx].Select(0);
    }
    else {
        highlightedVariantIdx = 0;
    }
}

void VariantSelect::ApplyVariantButtonState() {
    CupsConfig* cups = CupsConfig::sInstance;
    if(!cups) return;
    for(u32 i = 0; i < 4; ++i) {
        CourseButton& btn = this->CtrlMenuCourseSelectCourse.courseButtons[i];
        const u8 variantIdx = variantButtonVariants[i];
        if(selectedPulsarId == PULSARID_NONE || variantIdx == 0xFF) {
            btn.manipulator.inaccessible = true;
            btn.UIControl::isHidden = true;
            continue;
        }
        btn.UIControl::isHidden = false;
        btn.manipulator.inaccessible = false;
        // The row index is what SaveSelectedCourse turns back into a PulsarId, so every
        // variant button has to keep pointing at the row the player originally clicked.
        btn.buttonId = static_cast<s32>(baseRowIdx);

        const u32 bmgId = GetTrackVariantBMGId(selectedPulsarId, variantIdx);
        if(bmgId != 0) {
            SetCourseButtonMessage(btn, bmgId, selectedPulsarId, i);
            continue;
        }

        // No name in the pack: fall back to a generated label so the button is not blank.
        wchar_t* nameBuf = variantButtonNames[i];
        if(variantIdx == 0) swprintf(nameBuf, 128, L"%ls", L"Default");
        else swprintf(nameBuf, 128, L"Variant %u", static_cast<u32>(variantIdx));

        Text::Info info;
        memset(&info, 0, sizeof(info));
        info.strings[0] = nameBuf;
        btn.SetMessage(BMG_TEXT, &info);
    }
}

u32 VariantSelect::GetVariantIndexForButton(const PushButton& button) const {
    for(u32 i = 0; i < 4; ++i) {
        if(&this->CtrlMenuCourseSelectCourse.courseButtons[i] == &button) {
            if(variantButtonVariants[i] == 0xFF) return 0xFFFFFFFF;
            return variantButtonVariants[i];
        }
    }
    return 0xFFFFFFFF;
}

void VariantSelect::ResetVariantButtonState() {
    for(u32 i = 0; i < 4; ++i) {
        variantButtonVariants[i] = 0xFF;
        variantButtonNames[i][0] = L'\0';
    }
}

void VariantSelect::SetBaseRowIdx(u8 rowIdx) {
    baseRowIdx = rowIdx;
}

}  // namespace UI
}  // namespace Pulsar
