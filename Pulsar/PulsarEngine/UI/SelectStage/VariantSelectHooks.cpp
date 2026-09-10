#include <kamek.hpp>
#include <runtimeWrite.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuCourse.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <UI/SelectStage/VariantSelect.hpp>
#include <UI/UI.hpp>
#include <Network/PacketExpansion.hpp>

namespace Pulsar {
namespace UI {

/*
    The host decides the winning variant from the SELECT packets, so the variant the local
    player picked has to travel with the vote. It is written here rather than in the vote
    packet asm so there is exactly one place that knows what was chosen.
*/
static void StoreLocalVoteVariant(u8 variantIdx) {
    if(RKNet::SELECTHandler::sInstance == nullptr) return;
    Network::ExpSELECTHandler& handler = Network::ExpSELECTHandler::Get();
    handler.toSendPacket.voteVariantIdx[0] = variantIdx;
    handler.toSendPacket.voteVariantIdx[1] = variantIdx;
}

/*
    Clicking a course button normally goes straight to the next page. Here it is intercepted:
    a track that declares variants routes to VariantSelect first, and the click on that page
    stores the chosen variant before continuing. Everything else falls through to vanilla.
*/
kmRuntimeUse(0x807e5434);
static void CourseSelect_OnCourseButtonClick(CtrlMenuCourseSelectCourse* self, PushButton& courseButton, u32 hudSlotId) {
    CupsConfig* cups = CupsConfig::sInstance;
    ExpSection* section = ExpSection::GetSection();
    typedef void (*OrigFn)(CtrlMenuCourseSelectCourse*, PushButton&, u32);
    OrigFn orig = (OrigFn)kmRuntimeAddr(0x807e5434);

    VariantSelect* variantPage = section->GetPulPage<VariantSelect>();
    bool isVariantContext = (variantPage != nullptr && self == &variantPage->CtrlMenuCourseSelectCourse);
    bool handled = false;

    if(!isVariantContext) {
        PulsarCupId lastCup = cups->lastSelectedCup;
        PulsarId selected = cups->ConvertTrack_PulsarCupToTrack(lastCup, courseButton.buttonId);

        bool hasVariants = false;
        if(!cups->IsReg(selected)) {
            const Track& track = cups->GetTrack(selected);
            if(track.variantCount > 0) hasVariants = true;
        }

        Pages::CourseSelect* coursePage = SectionMgr::sInstance->curSection->Get<Pages::CourseSelect>();
        if(hasVariants && variantPage != nullptr && coursePage != nullptr) {
            cups->SetSelected(selected);
            variantPage->SetBaseRowIdx(static_cast<u8>(courseButton.buttonId));
            coursePage->LoadNextPageById(static_cast<PageId>(PULPAGE_VARIANTSELECT), courseButton);
            return;
        }

        if(coursePage != nullptr) {
            cups->SetSelected(selected);
            StoreLocalVoteVariant(0);
            coursePage->LoadNextPage(coursePage->CtrlMenuCourseSelectCourse, courseButton, hudSlotId);
            return;
        }
    }
    else if(variantPage != nullptr) {
        u32 variantIdx = variantPage->GetVariantIndexForButton(courseButton);
        if(variantIdx != 0xFFFFFFFF) {
            cups->SetLastSelectedVariant(cups->GetSelected(), static_cast<u8>(variantIdx));
            cups->SetPendingVariant(static_cast<u8>(variantIdx));
            StoreLocalVoteVariant(static_cast<u8>(variantIdx));
            variantPage->LoadNextPage(variantPage->CtrlMenuCourseSelectCourse, courseButton, hudSlotId);
            handled = true;
        }
    }

    if(!handled) orig(self, courseButton, hudSlotId);
}
kmBranch(0x807e5434, CourseSelect_OnCourseButtonClick);

}  // namespace UI
}  // namespace Pulsar
