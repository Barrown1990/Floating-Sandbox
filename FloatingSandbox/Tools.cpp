/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-06-17
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Tools.h"

#include <GameCore/GameException.h>

#include <wx/bitmap.h>
#include <wx/image.h>

#include <cassert>

static constexpr int CursorStep = 30;

std::vector<std::unique_ptr<wxCursor>> MakeCursors(
    std::string const & cursorName,
    int hotspotX,
    int hotspotY,
    ResourceLoader & resourceLoader)
{
    auto filepath = resourceLoader.GetCursorFilepath(cursorName);
    wxBitmap* bmp = new wxBitmap(filepath.string(), wxBITMAP_TYPE_PNG);
    if (nullptr == bmp)
    {
        throw GameException("Cannot load cursor '" + filepath.string() + "'");
    }

    wxImage img = bmp->ConvertToImage();
    int const imageWidth = img.GetWidth();
    int const imageHeight = img.GetHeight();

    // Set hotspots
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, hotspotX);
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, hotspotY);

    //
    // Build series of cursors for each power step: 0 (base), 1-CursorStep
    //

    std::vector<std::unique_ptr<wxCursor>> cursors;

    // Create base
    cursors.emplace_back(std::make_unique<wxCursor>(img));

    // Create power steps

    static_assert(CursorStep > 0, "CursorStep is at least 1");

    unsigned char * data = img.GetData();

    for (int c = 1; c <= CursorStep; ++c)
    {
        int powerHeight = static_cast<int>(floorf(
            static_cast<float>(imageHeight) * static_cast<float>(c) / static_cast<float>(CursorStep)
        ));

        // Start from top
        for (int y = imageHeight - powerHeight; y < imageHeight; ++y)
        {
            int rowStartIndex = (imageWidth * y);

            // Red   = 0xDB0F0F
            // Green = 0x039B0A (final)

            float const targetR = ((c == CursorStep) ? 0x03 : 0xDB) / 255.0f;
            float const targetG = ((c == CursorStep) ? 0x9B : 0x0F) / 255.0f;
            float const targetB = ((c == CursorStep) ? 0x0A : 0x0F) / 255.0f;

            for (int x = 0; x < imageWidth; ++x)
            {
                data[(rowStartIndex + x) * 3] = static_cast<unsigned char>(targetR * 255.0f);
                data[(rowStartIndex + x) * 3 + 1] = static_cast<unsigned char>(targetG * 255.0f);
                data[(rowStartIndex + x) * 3 + 2] = static_cast<unsigned char>(targetB * 255.0f);
            }
        }

        cursors.emplace_back(std::make_unique<wxCursor>(img));
    }

    delete (bmp);

    return cursors;
}

std::unique_ptr<wxCursor> MakeCursor(
    std::string const & cursorName,
    int hotspotX,
    int hotspotY,
    ResourceLoader & resourceLoader)
{
    auto filepath = resourceLoader.GetCursorFilepath(cursorName);
    wxBitmap* bmp = new wxBitmap(filepath.string(), wxBITMAP_TYPE_PNG);
    if (nullptr == bmp)
    {
        throw GameException("Cannot load cursor '" + filepath.string() + "'");
    }

    wxImage img = bmp->ConvertToImage();

    // Set hotspots
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, hotspotX);
    img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, hotspotY);

    return std::make_unique<wxCursor>(img);
}

/////////////////////////////////////////////////////////////////////////
// One-Shot Tool
/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
// Continuous Tool
/////////////////////////////////////////////////////////////////////////

void ContinuousTool::Update(InputState const & inputState)
{
    // We apply the tool only if the left mouse button is down
    if (inputState.IsLeftMouseDown)
    {
        auto const now = std::chrono::steady_clock::now();

        // Accumulate total time iff we haven't moved since last time
        if (mPreviousMousePosition == inputState.MousePosition)
        {
            mCumulatedTime += std::chrono::duration_cast<std::chrono::microseconds>(now - mPreviousTimestamp);
        }
        else
        {
            // We've moved, don't accumulate but use time
            // that was built up so far
        }

        // Remember new position and timestamp
        mPreviousMousePosition = inputState.MousePosition;
        mPreviousTimestamp = now;

        // Apply current tool
        ApplyTool(
            mCumulatedTime,
            inputState);
    }
}

////////////////////////////////////////////////////////////////////////
// Move
////////////////////////////////////////////////////////////////////////

MoveTool::MoveTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : BaseMoveTool(
        ToolType::Move,
        parentWindow,
        std::move(gameController),
        std::move(soundController),
        MakeCursor("move_cursor_up", 13, 5, resourceLoader),
        MakeCursor("move_cursor_down", 13, 5, resourceLoader),
        MakeCursor("move_cursor_rotate_up", 13, 5, resourceLoader),
        MakeCursor("move_cursor_rotate_down", 13, 5, resourceLoader))
{
}

MoveAllTool::MoveAllTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : BaseMoveTool(
        ToolType::MoveAll,
        parentWindow,
        std::move(gameController),
        std::move(soundController),
        MakeCursor("move_all_cursor_up", 13, 5, resourceLoader),
        MakeCursor("move_all_cursor_down", 13, 5, resourceLoader),
        MakeCursor("move_all_cursor_rotate_up", 13, 5, resourceLoader),
        MakeCursor("move_all_cursor_rotate_down", 13, 5, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// Smash
////////////////////////////////////////////////////////////////////////

SmashTool::SmashTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : ContinuousTool(
        ToolType::Smash,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpCursor(MakeCursor("smash_cursor_up", 6, 9, resourceLoader))
    , mDownCursors(MakeCursors("smash_cursor_down", 6, 9, resourceLoader))
{
}

void SmashTool::ApplyTool(
    std::chrono::microseconds const & cumulatedTime,
    InputState const & inputState)
{
    // Calculate radius fraction
    // 0-500ms      = 0.1
    // ...
    // 5000ms-+INF = 1.0

    static constexpr float MinFraction = 0.1f;

    float millisecondsElapsed = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(cumulatedTime).count());
    float radiusFraction = MinFraction + (1.0f - MinFraction) * std::min(1.0f, millisecondsElapsed / 5000.0f);

    // Modulate down cursor
    ModulateCursor(mDownCursors, radiusFraction);

    // Destroy
    mGameController->DestroyAt(
        inputState.MousePosition,
        radiusFraction);
}

////////////////////////////////////////////////////////////////////////
// Saw
////////////////////////////////////////////////////////////////////////

SawTool::SawTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::Saw,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpCursor(MakeCursor("chainsaw_cursor_up", 8, 20, resourceLoader))
    , mDownCursor1(MakeCursor("chainsaw_cursor_down_1", 8, 20, resourceLoader))
    , mDownCursor2(MakeCursor("chainsaw_cursor_down_2", 8, 20, resourceLoader))
    , mCurrentCursor(nullptr)
    , mPreviousMousePos()
    , mDownCursorCounter(0)
    , mIsUnderwater(false)
{
}

////////////////////////////////////////////////////////////////////////
// HeatBlaster
////////////////////////////////////////////////////////////////////////

HeatBlasterTool::HeatBlasterTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::HeatBlaster,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mIsEngaged(false)
    , mCurrentAction(HeatBlasterActionType::Heat)
    , mHeatUpCursor(MakeCursor("heat_blaster_heat_cursor_up", 5, 1, resourceLoader))
    , mCoolUpCursor(MakeCursor("heat_blaster_cool_cursor_up", 5, 30, resourceLoader))
    , mHeatDownCursor(MakeCursor("heat_blaster_heat_cursor_down", 5, 1, resourceLoader))
    , mCoolDownCursor(MakeCursor("heat_blaster_cool_cursor_down", 5, 30, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// FireExtinguisher
////////////////////////////////////////////////////////////////////////

FireExtinguisherTool::FireExtinguisherTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::FireExtinguisher,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mIsEngaged(false)
    , mUpCursor(MakeCursor("fire_extinguisher_cursor_up", 6, 3, resourceLoader))
    , mDownCursor(MakeCursor("fire_extinguisher_cursor_down", 6, 3, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// Grab
////////////////////////////////////////////////////////////////////////

GrabTool::GrabTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : ContinuousTool(
        ToolType::Grab,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpPlusCursor(MakeCursor("drag_cursor_up_plus", 15, 15, resourceLoader))
    , mUpMinusCursor(MakeCursor("drag_cursor_up_minus", 15, 15, resourceLoader))
    , mDownPlusCursors(MakeCursors("drag_cursor_down_plus", 15, 15, resourceLoader))
    , mDownMinusCursors(MakeCursors("drag_cursor_down_minus", 15, 15, resourceLoader))
{
}

void GrabTool::ApplyTool(
    std::chrono::microseconds const & cumulatedTime,
    InputState const & inputState)
{
    // Calculate strength multiplier
    // 0-500ms      = 0.1
    // ...
    // 5000ms-+INF = 1.0

    static constexpr float MinFraction = 0.1f;

    float millisecondsElapsed = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(cumulatedTime).count());
    float strengthFraction = MinFraction + (1.0f - MinFraction) * std::min(1.0f, millisecondsElapsed / 5000.0f);

    // Modulate down cursor
    ModulateCursor(
        inputState.IsShiftKeyDown ? mDownMinusCursors : mDownPlusCursors,
        strengthFraction);

    // Draw
    mGameController->DrawTo(
        inputState.MousePosition,
        inputState.IsShiftKeyDown
            ? -strengthFraction
            : strengthFraction);
}

////////////////////////////////////////////////////////////////////////
// Swirl
////////////////////////////////////////////////////////////////////////

SwirlTool::SwirlTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : ContinuousTool(
        ToolType::Grab,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpPlusCursor(MakeCursor("swirl_cursor_up_cw", 15, 15, resourceLoader))
    , mUpMinusCursor(MakeCursor("swirl_cursor_up_ccw", 15, 15, resourceLoader))
    , mDownPlusCursors(MakeCursors("swirl_cursor_down_cw", 15, 15, resourceLoader))
    , mDownMinusCursors(MakeCursors("swirl_cursor_down_ccw", 15, 15, resourceLoader))
{
}

void SwirlTool::ApplyTool(
    std::chrono::microseconds const & cumulatedTime,
    InputState const & inputState)
{
    // Calculate strength multiplier
    // 0-500ms      = 0.1
    // ...
    // 5000ms-+INF = 1.0

    static constexpr float MinFraction = 0.1f;

    float millisecondsElapsed = static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(cumulatedTime).count());
    float strengthFraction = MinFraction + (1.0f - MinFraction) * std::min(1.0f, millisecondsElapsed / 5000.0f);

    // Modulate down cursor
    ModulateCursor(
        inputState.IsShiftKeyDown ? mDownMinusCursors : mDownPlusCursors,
        strengthFraction);

    // Draw
    mGameController->SwirlAt(
        inputState.MousePosition,
        inputState.IsShiftKeyDown
            ? -strengthFraction
            : strengthFraction);
}

////////////////////////////////////////////////////////////////////////
// Pin
////////////////////////////////////////////////////////////////////////

PinTool::PinTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::Pin,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCursor(MakeCursor("pin_cursor", 4, 27, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// InjectAirBubbles
////////////////////////////////////////////////////////////////////////

InjectAirBubblesTool::InjectAirBubblesTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::InjectAirBubbles,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mIsEngaged(false)
    , mUpCursor(MakeCursor("air_tank_cursor_up", 12, 1, resourceLoader))
    , mDownCursor(MakeCursor("air_tank_cursor_down", 12, 1, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// FloodHose
////////////////////////////////////////////////////////////////////////

FloodHoseTool::FloodHoseTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::FloodHose,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mIsEngaged(false)
    , mUpCursor(MakeCursor("flood_cursor_up", 20, 0, resourceLoader))
    , mDownCursor1(MakeCursor("flood_cursor_down_1", 20, 0, resourceLoader))
    , mDownCursor2(MakeCursor("flood_cursor_down_2", 20, 0, resourceLoader))
    , mDownCursorCounter(0)
{
}

////////////////////////////////////////////////////////////////////////
// AntiMatterBomb
////////////////////////////////////////////////////////////////////////

AntiMatterBombTool::AntiMatterBombTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::AntiMatterBomb,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCursor(MakeCursor("am_bomb_cursor", 16, 16, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// ImpactBomb
////////////////////////////////////////////////////////////////////////

ImpactBombTool::ImpactBombTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::ImpactBomb,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCursor(MakeCursor("impact_bomb_cursor", 18, 10, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// RCBomb
////////////////////////////////////////////////////////////////////////

RCBombTool::RCBombTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::RCBomb,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCursor(MakeCursor("rc_bomb_cursor", 16, 21, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// TimerBomb
////////////////////////////////////////////////////////////////////////

TimerBombTool::TimerBombTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::TimerBomb,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCursor(MakeCursor("timer_bomb_cursor", 16, 19, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// WaveMaker
////////////////////////////////////////////////////////////////////////

WaveMakerTool::WaveMakerTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::WaveMaker,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpCursor(MakeCursor("wave_maker_cursor_up", 15, 15, resourceLoader))
    , mDownCursor(MakeCursor("wave_maker_cursor_down", 15, 15, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// TerrainAdjust
////////////////////////////////////////////////////////////////////////

TerrainAdjustTool::TerrainAdjustTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::TerrainAdjust,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mCurrentTrajectoryPreviousPosition()
    , mUpCursor(MakeCursor("terrain_adjust_cursor_up", 15, 15, resourceLoader))
    , mDownCursor(MakeCursor("terrain_adjust_cursor_down", 15, 15, resourceLoader))
{
}

////////////////////////////////////////////////////////////////////////
// Scrub
////////////////////////////////////////////////////////////////////////

ScrubTool::ScrubTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::Scrub,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpCursor(MakeCursor("scrub_cursor_up", 15, 15, resourceLoader))
    , mDownCursor(MakeCursor("scrub_cursor_down", 15, 15, resourceLoader))
    , mCurrentCursor(nullptr)
    , mPreviousMousePos()
    , mPreviousScrub()
    , mPreviousScrubTimestamp(std::chrono::steady_clock::time_point::min())
{
}

////////////////////////////////////////////////////////////////////////
// Repair Structure
////////////////////////////////////////////////////////////////////////

RepairStructureTool::RepairStructureTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : Tool(
        ToolType::RepairStructure,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mEngagementStartTimestamp()
    , mCurrentSessionId(0)
    , mCurrentSessionStepId(0)
    , mCurrentCursor(nullptr)
    , mUpCursor(MakeCursor("repair_structure_cursor_up", 8, 8, resourceLoader))
    , mDownCursors{
        MakeCursor("repair_structure_cursor_down_0", 8, 8, resourceLoader),
        MakeCursor("repair_structure_cursor_down_1", 8, 8, resourceLoader),
        MakeCursor("repair_structure_cursor_down_2", 8, 8, resourceLoader),
        MakeCursor("repair_structure_cursor_down_3", 8, 8, resourceLoader),
        MakeCursor("repair_structure_cursor_down_4", 8, 8, resourceLoader) }
{
}

////////////////////////////////////////////////////////////////////////
// ThanosSnap
////////////////////////////////////////////////////////////////////////

ThanosSnapTool::ThanosSnapTool(
    wxWindow * parentWindow,
    std::shared_ptr<IGameController> gameController,
    std::shared_ptr<SoundController> soundController,
    ResourceLoader & resourceLoader)
    : OneShotTool(
        ToolType::ThanosSnap,
        parentWindow,
        std::move(gameController),
        std::move(soundController))
    , mUpCursor(MakeCursor("thanos_snap_cursor_up", 15, 15, resourceLoader))
    , mDownCursor(MakeCursor("thanos_snap_cursor_down", 15, 15, resourceLoader))
{
}