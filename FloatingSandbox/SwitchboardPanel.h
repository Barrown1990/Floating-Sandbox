/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2019-12-27
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <UIControls/BitmappedCheckbox.h>
#include <UIControls/ElectricalElementControl.h>

#include <Game/IGameController.h>
#include <Game/IGameEventHandlers.h>
#include <Game/ResourceLoader.h>

#include <wx/bitmap.h>
#include <wx/custombgwin.h>
#include <wx/gbsizer.h>
#include <wx/scrolwin.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <memory>
#include <string>
#include <unordered_map>

class SwitchboardPanel
    : public wxCustomBackgroundWindow<wxPanel>
    , public ILifecycleGameEventHandler
    , public IElectricalElementGameEventHandler
{
public:

    static std::unique_ptr<SwitchboardPanel> Create(
        wxWindow * parent,
        wxWindow * parentLayoutWindow,
        wxSizer * parentLayoutSizer,
        std::shared_ptr<IGameController> gameController,
        ResourceLoader & resourceLoader);

    virtual ~SwitchboardPanel();

    bool IsShowing() const
    {
        return mShowingMode != ShowingMode::NotShowing;
    }

    void HideFully();

    void ShowPartially();

    void ShowFullyFloating();

    void ShowFullyDocked();

public:

    //
    // Game event handlers
    //

    void RegisterEventHandler(IGameController & gameController)
    {
        gameController.RegisterLifecycleEventHandler(this);
        gameController.RegisterElectricalElementEventHandler(this);
    }

    virtual void OnGameReset() override;

    virtual void OnElectricalElementAnnouncementsBegin() override;

    virtual void OnSwitchCreated(
        SwitchId switchId,
        ElectricalElementInstanceIndex instanceIndex,
        SwitchType type,
        ElectricalState state,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override;

    virtual void OnPowerMonitorCreated(
        PowerMonitorId powerMonitorId,
        ElectricalElementInstanceIndex instanceIndex,
        ElectricalState state,
        std::optional<ElectricalPanelElementMetadata> const & panelElementMetadata) override;

    virtual void OnElectricalElementAnnouncementsEnd() override;

    virtual void OnSwitchEnabled(
        SwitchId switchId,
        bool isEnabled) override;

    virtual void OnSwitchToggled(
        SwitchId switchId,
        ElectricalState newState) override;

    virtual void OnPowerMonitorToggled(
        PowerMonitorId powerMonitorId,
        ElectricalState newState) override;

private:

    SwitchboardPanel(
        wxWindow * parent,
        wxWindow * parentLayoutWindow,
        wxSizer * parentLayoutSizer,
        std::shared_ptr<IGameController> gameController,
        ResourceLoader & resourceLoader);

    void MakeSwitchPanel();

    void ShowDockCheckbox(bool doShow);

    void InstallMouseTracking(bool isActive);

    void AddControl(
        ElectricalElementControl * ctrl,
        ElectricalElementInstanceIndex instanceIndex);

    void LayoutParent();

    void OnLeaveWindowTimer(wxTimerEvent & event);

    void OnDockCheckbox(wxCommandEvent & event);

    void OnEnterWindow(wxMouseEvent & event);

    void OnLeaveWindow();

private:

    enum class ShowingMode
    {
        NotShowing,
        ShowingHint,
        ShowingFullyFloating,
        ShowingFullyDocked
    };

    ShowingMode mShowingMode;


    wxBoxSizer * mMainVSizer;

    wxPanel * mHintPanel;
    BitmappedCheckbox * mDockCheckbox;
    wxBoxSizer * mHintPanelSizer;

    wxScrolled<wxPanel> * mSwitchPanel;
    wxGridBagSizer * mSwitchPanelSizer;

    std::unique_ptr<wxTimer> mLeaveWindowTimer;

private:

    struct ElectricalElementInfo
    {
        ElectricalElementControl * Control;
        std::optional<ElectricalPanelElementMetadata> PanelElementMetadata;
        bool IsEnabled;

        ElectricalElementInfo(
            ElectricalElementControl * control,
            std::optional<ElectricalPanelElementMetadata> panelElementMetadata)
            : Control(control)
            , PanelElementMetadata(panelElementMetadata)
            , IsEnabled(true)
        {}
    };

    std::unordered_map<SwitchId, ElectricalElementInfo> mSwitchMap;
    std::unordered_map<PowerMonitorId, ElectricalElementInfo> mPowerMonitorMap;

private:

    std::shared_ptr<IGameController> const mGameController;

    wxWindow * const mParentLayoutWindow;
    wxSizer * const mParentLayoutSizer;

    //
    // Bitmaps
    //

    wxBitmap mAutomaticSwitchOnEnabledBitmap;
    wxBitmap mAutomaticSwitchOffEnabledBitmap;
    wxBitmap mAutomaticSwitchOnDisabledBitmap;
    wxBitmap mAutomaticSwitchOffDisabledBitmap;

    wxBitmap mInteractivePushSwitchOnEnabledBitmap;
    wxBitmap mInteractivePushSwitchOffEnabledBitmap;
    wxBitmap mInteractivePushSwitchOnDisabledBitmap;
    wxBitmap mInteractivePushSwitchOffDisabledBitmap;

    wxBitmap mInteractiveToggleSwitchOnEnabledBitmap;
    wxBitmap mInteractiveToggleSwitchOffEnabledBitmap;
    wxBitmap mInteractiveToggleSwitchOnDisabledBitmap;
    wxBitmap mInteractiveToggleSwitchOffDisabledBitmap;

    wxBitmap mPowerMonitorOnBitmap;
    wxBitmap mPowerMonitorOffBitmap;
};