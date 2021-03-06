/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2019-12-27
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <GameCore/GameTypes.h>
#include <GameCore/Log.h>

#include <wx/bitmap.h>
#include <wx/stattext.h>
#include <wx/tooltip.h>
#include <wx/wx.h>

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class ElectricalElementControl : public wxPanel
{
public:

    virtual ~ElectricalElementControl()
    {}

    bool IsInteractive() const
    {
        return mIsInteractive;
    }

    void SetKeyboardShortcutLabel(std::string const & label)
    {
        mImageBitmap->SetToolTip(label);
    }

    ElectricalState GetState() const
    {
        return mCurrentState;
    }

    void SetState(ElectricalState state)
    {
        mCurrentState = state;

        SetImageForCurrentState();
    }

    void SetEnabled(bool isEnabled)
    {
        mIsEnabled = isEnabled;

        SetImageForCurrentState();
    }

protected:

    ElectricalElementControl(
        wxWindow * parent,
        bool isInteractive,
        wxBitmap const & onEnabledImage,
        wxBitmap const & offEnabledImage,
        wxBitmap const & onDisabledImage,
        wxBitmap const & offDisabledImage,
        std::string const & label,
        ElectricalState currentState)
        : wxPanel(
            parent,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxBORDER_NONE)
        , mIsInteractive(isInteractive)
        , mCurrentState(currentState)
        , mIsEnabled(true)
        , mImageBitmap(nullptr)
        ///////////
        , mOnEnabledImage(onEnabledImage)
        , mOffEnabledImage(offEnabledImage)
        , mOnDisabledImage(onDisabledImage)
        , mOffDisabledImage(offDisabledImage)
    {
        wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);

        mImageBitmap = new wxStaticBitmap(this, wxID_ANY, GetImageForCurrentState(), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        vSizer->Add(mImageBitmap, 0, wxALIGN_CENTRE_HORIZONTAL);

        vSizer->AddSpacer(4);

        wxPanel * labelPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN);
        {
            labelPanel->SetBackgroundColour(wxColour(165, 167, 156));

            wxStaticText * labelStaticText = new wxStaticText(
                labelPanel, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
            labelStaticText->SetForegroundColour(wxColour(0x20, 0x20, 0x20));
            wxFont font = labelStaticText->GetFont();
            font.SetPointSize(7);
            labelStaticText->SetFont(font);

            wxBoxSizer* labelSizer = new wxBoxSizer(wxVERTICAL);
            labelSizer->Add(labelStaticText, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 6);
            labelPanel->SetSizerAndFit(labelSizer);
        }
        vSizer->Add(labelPanel, 0, wxEXPAND);

        this->SetSizerAndFit(vSizer);
    }

private:

    wxBitmap const & GetImageForCurrentState() const
    {
        if (mIsEnabled)
        {
            if (mCurrentState == ElectricalState::On)
            {
                return mOnEnabledImage;
            }
            else
            {
                assert(mCurrentState == ElectricalState::Off);
                return mOffEnabledImage;
            }
        }
        else
        {
            if (mCurrentState == ElectricalState::On)
            {
                return mOnDisabledImage;
            }
            else
            {
                assert(mCurrentState == ElectricalState::Off);
                return mOffDisabledImage;
            }
        }
    }

    void SetImageForCurrentState()
    {
        mImageBitmap->SetBitmap(GetImageForCurrentState());

        Refresh();
    }

protected:

    bool const mIsInteractive;

    ElectricalState mCurrentState;
    bool mIsEnabled;

    wxStaticBitmap * mImageBitmap;

private:

    wxBitmap const & mOnEnabledImage;
    wxBitmap const & mOffEnabledImage;
    wxBitmap const & mOnDisabledImage;
    wxBitmap const & mOffDisabledImage;
};

class InteractiveSwitchElectricalElementControl : public ElectricalElementControl
{
public:

    virtual void OnKeyboardShortcutDown() = 0;

    virtual void OnKeyboardShortcutUp() = 0;

protected:

    InteractiveSwitchElectricalElementControl(
        wxWindow * parent,
        wxBitmap const & onEnabledImage,
        wxBitmap const & offEnabledImage,
        wxBitmap const & onDisabledImage,
        wxBitmap const & offDisabledImage,
        std::string const & label,
        std::function<void(ElectricalState)> onSwitchToggled,
        ElectricalState currentState)
        : ElectricalElementControl(
            parent,
            true,
            onEnabledImage,
            offEnabledImage,
            onDisabledImage,
            offDisabledImage,
            label,
            currentState)
        , mOnSwitchToggled(std::move(onSwitchToggled))
    {
    }

protected:

    std::function<void(ElectricalState)> const mOnSwitchToggled;
};

class InteractiveToggleSwitchElectricalElementControl : public InteractiveSwitchElectricalElementControl
{
public:

    InteractiveToggleSwitchElectricalElementControl(
        wxWindow * parent,
        wxBitmap const & onEnabledImage,
        wxBitmap const & offEnabledImage,
        wxBitmap const & onDisabledImage,
        wxBitmap const & offDisabledImage,
        std::string const & label,
        std::function<void(ElectricalState)> onSwitchToggled,
        ElectricalState currentState)
        : InteractiveSwitchElectricalElementControl(
            parent,
            onEnabledImage,
            offEnabledImage,
            onDisabledImage,
            offDisabledImage,
            label,
            std::move(onSwitchToggled),
            currentState)
    {
        mImageBitmap->Bind(wxEVT_LEFT_DOWN, (wxObjectEventFunction)&InteractiveToggleSwitchElectricalElementControl::OnLeftDown, this);
    }

    void OnKeyboardShortcutDown() override
    {
        OnDown();
    }

    void OnKeyboardShortcutUp() override
    {
        // Ignore
    }

private:

    void OnLeftDown(wxMouseEvent & /*event*/)
    {
        OnDown();
    }

    void OnDown()
    {
        if (mIsEnabled)
        {
            //
            // Just invoke the callback, we'll end up being toggled when the event travels back
            //

            ElectricalState const newState = (mCurrentState == ElectricalState::On)
                ? ElectricalState::Off
                : ElectricalState::On;

            mOnSwitchToggled(newState);
        }
    }
};

class InteractivePushSwitchElectricalElementControl : public InteractiveSwitchElectricalElementControl
{
public:

    InteractivePushSwitchElectricalElementControl(
        wxWindow * parent,
        wxBitmap const & onEnabledImage,
        wxBitmap const & offEnabledImage,
        wxBitmap const & onDisabledImage,
        wxBitmap const & offDisabledImage,
        std::string const & label,
        std::function<void(ElectricalState)> onSwitchToggled,
        ElectricalState currentState)
        : InteractiveSwitchElectricalElementControl(
            parent,
            onEnabledImage,
            offEnabledImage,
            onDisabledImage,
            offDisabledImage,
            label,
            std::move(onSwitchToggled),
            currentState)
        , mIsPushed(false)
    {
        mImageBitmap->Bind(wxEVT_LEFT_DOWN, (wxObjectEventFunction)&InteractivePushSwitchElectricalElementControl::OnLeftDown, this);
        mImageBitmap->Bind(wxEVT_LEFT_UP, (wxObjectEventFunction)&InteractivePushSwitchElectricalElementControl::OnLeftUp, this);
        mImageBitmap->Bind(wxEVT_LEAVE_WINDOW, (wxObjectEventFunction)&InteractivePushSwitchElectricalElementControl::OnLeftUp, this);
    }

    void OnKeyboardShortcutDown() override
    {
        OnDown();
    }

    void OnKeyboardShortcutUp() override
    {
        OnUp();
    }

private:

    void OnLeftDown(wxMouseEvent & /*event*/)
    {
        OnDown();
    }

    void OnLeftUp(wxMouseEvent & /*event*/)
    {
        OnUp();
    }

    void OnLeaveWindow(wxMouseEvent & /*event*/)
    {
        OnUp();
    }

    void OnDown()
    {
        if (mIsEnabled && !mIsPushed)
        {
            //
            // Just invoke the callback, we'll end up being toggled when the event travels back
            //

            ElectricalState const newState = (mCurrentState == ElectricalState::On)
                ? ElectricalState::Off
                : ElectricalState::On;

            mOnSwitchToggled(newState);

            mIsPushed = true;
        }
    }

    void OnUp()
    {
        if (mIsPushed)
        {
            //
            // Just invoke the callback, we'll end up being toggled when the event travels back
            //

            ElectricalState const newState = (mCurrentState == ElectricalState::On)
                ? ElectricalState::Off
                : ElectricalState::On;

            mOnSwitchToggled(newState);

            mIsPushed = false;
        }
    }

private:

    bool mIsPushed;
};

class AutomaticSwitchElectricalElementControl : public ElectricalElementControl
{
public:

    AutomaticSwitchElectricalElementControl(
        wxWindow * parent,
        wxBitmap const & onEnabledImage,
        wxBitmap const & offEnabledImage,
        wxBitmap const & onDisabledImage,
        wxBitmap const & offDisabledImage,
        std::string const & label,
        ElectricalState currentState)
        : ElectricalElementControl(
            parent,
            false,
            onEnabledImage,
            offEnabledImage,
            onDisabledImage,
            offDisabledImage,
            label,
            currentState)
    {
    }
};

class PowerMonitorElectricalElementControl : public ElectricalElementControl
{
public:

    PowerMonitorElectricalElementControl(
        wxWindow * parent,
        wxBitmap const & onImage,
        wxBitmap const & offImage,
        std::string const & label,
        ElectricalState currentState)
        : ElectricalElementControl(
            parent,
            false,
            onImage,
            offImage,
            onImage,
            offImage,
            label,
            currentState)
    {
    }
};
