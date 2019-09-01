/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-01-19
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "GameController.h"

#include <GameCore/GameMath.h>
#include <GameCore/Log.h>

std::unique_ptr<GameController> GameController::Create(
    bool isStatusTextEnabled,
    bool isExtendedStatusTextEnabled,
    std::function<void()> swapRenderBuffersFunction,
    std::shared_ptr<ResourceLoader> resourceLoader,
    ProgressCallback const & progressCallback)
{
    // Load materials
    MaterialDatabase materialDatabase = MaterialDatabase::Load(*resourceLoader);

    // Create game dispatcher
    std::unique_ptr<GameEventDispatcher> gameEventDispatcher = std::make_unique<GameEventDispatcher>();

    // Create render context
    std::unique_ptr<Render::RenderContext> renderContext = std::make_unique<Render::RenderContext>(
        *resourceLoader,
        [&progressCallback](float progress, std::string const & message)
        {
            progressCallback(0.9f * progress, message);
        });

    // Create status text
    std::unique_ptr<StatusText> statusText = std::make_unique<StatusText>(
        isStatusTextEnabled,
        isExtendedStatusTextEnabled);

    //
    // Create controller
    //

    return std::unique_ptr<GameController>(
        new GameController(
            std::move(renderContext),
            std::move(swapRenderBuffersFunction),
            std::move(gameEventDispatcher),
            std::move(statusText),
            std::move(materialDatabase),
            resourceLoader));
}

GameController::GameController(
    std::unique_ptr<Render::RenderContext> renderContext,
    std::function<void()> swapRenderBuffersFunction,
    std::unique_ptr<GameEventDispatcher> gameEventDispatcher,
    std::unique_ptr<StatusText> statusText,
    MaterialDatabase materialDatabase,
    std::shared_ptr<ResourceLoader> resourceLoader)
    // State
    : mGameParameters()
    , mLastShipLoadedFilepath()
    , mIsPaused(false)
    , mIsMoveToolEngaged(false)
    , mHeatBlasterFlameToRender()
    , mFireExtinguisherSprayToRender()
    // State machines
    , mTsunamiNotificationStateMachine()
    , mThanosSnapStateMachines()
    // Parameters that we own
    , mShowTsunamiNotifications(true)
    , mDrawHeatBlasterFlame(true)
    // Doers
    , mRenderContext(std::move(renderContext))
    , mSwapRenderBuffersFunction(std::move(swapRenderBuffersFunction))
    , mGameEventDispatcher(std::move(gameEventDispatcher))
    , mResourceLoader(std::move(resourceLoader))
    , mStatusText(std::move(statusText))
    , mWorld(new Physics::World(
        mGameEventDispatcher,
        mGameParameters,
        *mResourceLoader))
    , mMaterialDatabase(std::move(materialDatabase))
    // Smoothing
    , mCurrentZoom(mRenderContext->GetZoom())
    , mTargetZoom(mCurrentZoom)
    , mStartingZoom(mCurrentZoom)
    , mStartZoomTimestamp()
    , mCurrentCameraPosition(mRenderContext->GetCameraWorldPosition())
    , mTargetCameraPosition(mCurrentCameraPosition)
    , mStartingCameraPosition(mCurrentCameraPosition)
    , mStartCameraPositionTimestamp()
    , mParameterSmoothers()
    // Stats
    , mTotalFrameCount(0u)
    , mLastFrameCount(0u)
    , mRenderStatsOriginTimestampReal(std::chrono::steady_clock::time_point::min())
    , mRenderStatsLastTimestampReal(std::chrono::steady_clock::time_point::min())
    , mTotalUpdateDuration(std::chrono::steady_clock::duration::zero())
    , mLastTotalUpdateDuration(std::chrono::steady_clock::duration::zero())
    , mTotalRenderDuration(std::chrono::steady_clock::duration::zero())
    , mLastTotalRenderDuration(std::chrono::steady_clock::duration::zero())
    , mOriginTimestampGame(GameWallClock::time_point::min())
    , mSkippedFirstStatPublishes(0)
{
    // Register ourselves as event handler for the events we care about
    mGameEventDispatcher->RegisterWavePhenomenaEventHandler(this);

    //
    // Initialize parameter smoothers
    //

    std::chrono::milliseconds constexpr ParameterSmoothingTrajectoryTime = std::chrono::milliseconds(1000);

    assert(mParameterSmoothers.size() == SpringStiffnessAdjustmentParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mGameParameters.SpringStiffnessAdjustment;
        },
        [this](float value)
        {
            this->mGameParameters.SpringStiffnessAdjustment = value;
        },
        ParameterSmoothingTrajectoryTime);

    assert(mParameterSmoothers.size() == SpringStrengthAdjustmentParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mGameParameters.SpringStrengthAdjustment;
        },
        [this](float value)
        {
            this->mGameParameters.SpringStrengthAdjustment = value;
        },
        ParameterSmoothingTrajectoryTime);

    assert(mParameterSmoothers.size() == SeaDepthParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mGameParameters.SeaDepth;
        },
        [this](float value)
        {
            this->mGameParameters.SeaDepth = value;
        },
        ParameterSmoothingTrajectoryTime);

    assert(mParameterSmoothers.size() == OceanFloorBumpinessParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mGameParameters.OceanFloorBumpiness;
        },
        [this](float value)
        {
            this->mGameParameters.OceanFloorBumpiness = value;
        },
        ParameterSmoothingTrajectoryTime);

    assert(mParameterSmoothers.size() == OceanFloorDetailAmplificationParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mGameParameters.OceanFloorDetailAmplification;
        },
        [this](float value)
        {
            this->mGameParameters.OceanFloorDetailAmplification = value;
        },
        ParameterSmoothingTrajectoryTime);

    assert(mParameterSmoothers.size() == FlameSizeAdjustmentParameterSmoother);
    mParameterSmoothers.emplace_back(
        [this]()
        {
            return this->mRenderContext->GetShipFlameSizeAdjustment();
        },
        [this](float value)
        {
            this->mRenderContext->SetShipFlameSizeAdjustment(value);
        },
        ParameterSmoothingTrajectoryTime);
}

ShipMetadata GameController::ResetAndLoadShip(std::filesystem::path const & shipDefinitionFilepath)
{
    // Create a new world
    auto newWorld = std::make_unique<Physics::World>(
        mGameEventDispatcher,
        mGameParameters,
        *mResourceLoader);

    // Load ship definition
    auto shipDefinition = ShipDefinition::Load(shipDefinitionFilepath);

    // Validate ship
    mRenderContext->ValidateShip(shipDefinition);

    // Save metadata
    ShipMetadata shipMetadata(shipDefinition.Metadata);

    // Add ship to new world
    ShipId shipId = newWorld->AddShip(
        shipDefinition,
        mMaterialDatabase,
        mGameParameters);

    //
    // No errors, so we may continue
    //

    Reset(std::move(newWorld));

    OnShipAdded(
        std::move(shipDefinition),
        shipDefinitionFilepath,
        shipId);

    return shipMetadata;
}

ShipMetadata GameController::AddShip(std::filesystem::path const & shipDefinitionFilepath)
{
    // Load ship definition
    auto shipDefinition = ShipDefinition::Load(shipDefinitionFilepath);

    // Validate ship
    mRenderContext->ValidateShip(shipDefinition);

    // Save metadata
    ShipMetadata shipMetadata(shipDefinition.Metadata);

    // Load ship into current world
    ShipId shipId = mWorld->AddShip(
        shipDefinition,
        mMaterialDatabase,
        mGameParameters);

    //
    // No errors, so we may continue
    //

    OnShipAdded(
        std::move(shipDefinition),
        shipDefinitionFilepath,
        shipId);

    return shipMetadata;
}

void GameController::ReloadLastShip()
{
    // Create a new world
    auto newWorld = std::make_unique<Physics::World>(
        mGameEventDispatcher,
        mGameParameters,
        *mResourceLoader);

    // Load ship definition

    if (mLastShipLoadedFilepath.empty())
    {
        throw std::runtime_error("No ship has been loaded yet");
    }

    auto shipDefinition = ShipDefinition::Load(mLastShipLoadedFilepath);

    // Load ship into new world
    ShipId shipId = newWorld->AddShip(
        shipDefinition,
        mMaterialDatabase,
        mGameParameters);

    //
    // No errors, so we may continue
    //

    Reset(std::move(newWorld));

    OnShipAdded(
        std::move(shipDefinition),
        mLastShipLoadedFilepath,
        shipId);
}

RgbImageData GameController::TakeScreenshot()
{
    return mRenderContext->TakeScreenshot();
}

void GameController::RunGameIteration()
{
    ///////////////////////////////////////////////////////////
    // Update simulation
    ///////////////////////////////////////////////////////////

    // Make sure we're not paused
    if (!mIsPaused && !mIsMoveToolEngaged)
    {
        auto const startTime = std::chrono::steady_clock::now();

        InternalUpdate();

        auto const endTime = std::chrono::steady_clock::now();
        mTotalUpdateDuration += endTime - startTime;
    }


    ///////////////////////////////////////////////////////////
    // Render
    ///////////////////////////////////////////////////////////

    //
    // Initialize render stats, if needed
    //

    if (mRenderStatsOriginTimestampReal == std::chrono::steady_clock::time_point::min())
    {
        assert(mRenderStatsLastTimestampReal == std::chrono::steady_clock::time_point::min());

        std::chrono::steady_clock::time_point nowReal = std::chrono::steady_clock::now();
        mRenderStatsOriginTimestampReal = nowReal;
        mRenderStatsLastTimestampReal = nowReal;

        mTotalFrameCount = 0u;
        mLastFrameCount = 0u;

        // In order to start from zero at first render, take global origin here
        mOriginTimestampGame = nowReal;

        // Render initial status text
        PublishStats(nowReal);
    }


    //
    // Render
    //

    auto const startTime = std::chrono::steady_clock::now();

    // Flip the (previous) back buffer onto the screen
    mSwapRenderBuffersFunction();

    // Render
    InternalRender();

    auto const endTime = std::chrono::steady_clock::now();
    mTotalRenderDuration += endTime - startTime;


    //
    // Update stats
    //

    ++mTotalFrameCount;
    ++mLastFrameCount;
}

void GameController::LowFrequencyUpdate()
{
    std::chrono::steady_clock::time_point nowReal = std::chrono::steady_clock::now();

    if (mSkippedFirstStatPublishes >= 1)
    {
        //
        // Publish stats
        //


        PublishStats(nowReal);

        //
        // Reset stats
        //

        mLastFrameCount = 0u;
        mLastTotalUpdateDuration = mTotalUpdateDuration;
        mLastTotalRenderDuration = mTotalRenderDuration;
    }
    else
    {
        //
        // Skip the first few publish as rates are too polluted
        //

        mTotalFrameCount = 0u;
        mLastFrameCount = 0u;
        mRenderStatsOriginTimestampReal = nowReal;

        ++mSkippedFirstStatPublishes;
    }

    mRenderStatsLastTimestampReal = nowReal;
}

void GameController::Update()
{
    InternalUpdate();
}

void GameController::Render()
{
    InternalRender();
}

/////////////////////////////////////////////////////////////
// Interactions
/////////////////////////////////////////////////////////////

void GameController::SetPaused(bool isPaused)
{
    // Freeze time
    GameWallClock::GetInstance().SetPaused(isPaused);

    // Change state
    mIsPaused = isPaused;
}

void GameController::SetMoveToolEngaged(bool isEngaged)
{
    mIsMoveToolEngaged = isEngaged;
}

void GameController::SetStatusTextEnabled(bool isEnabled)
{
    assert(!!mStatusText);
    mStatusText->SetStatusTextEnabled(isEnabled);
}

void GameController::SetExtendedStatusTextEnabled(bool isEnabled)
{
    assert(!!mStatusText);
    mStatusText->SetExtendedStatusTextEnabled(isEnabled);
}

float GameController::GetCurrentSimulationTime() const
{
    return mWorld->GetCurrentSimulationTime();
}

bool GameController::IsUnderwater(vec2f const & screenCoordinates) const
{
    return mWorld->IsUnderwater(ScreenToWorld(screenCoordinates));
}

void GameController::PickObjectToMove(
    vec2f const & screenCoordinates,
    std::optional<ElementId> & elementId)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->PickPointToMove(
        worldCoordinates,
        elementId,
        mGameParameters);
}

void GameController::PickObjectToMove(
    vec2f const & screenCoordinates,
    std::optional<ShipId> & shipId)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    auto elementIndex = mWorld->GetNearestPointAt(worldCoordinates, 1.0f);
    if (!!elementIndex)
        shipId = elementIndex->GetShipId();
    else
        shipId = std::nullopt;
}

void GameController::MoveBy(
    ElementId elementId,
    vec2f const & screenOffset,
    vec2f const & inertialScreenOffset)
{
    vec2f const worldOffset = mRenderContext->ScreenOffsetToWorldOffset(screenOffset);
    vec2f const inertialVelocity = mRenderContext->ScreenOffsetToWorldOffset(inertialScreenOffset);

    // Apply action
    assert(!!mWorld);
    mWorld->MoveBy(
        elementId,
        worldOffset,
        inertialVelocity,
        mGameParameters);
}

void GameController::MoveBy(
    ShipId shipId,
    vec2f const & screenOffset,
    vec2f const & inertialScreenOffset)
{
    vec2f const worldOffset = mRenderContext->ScreenOffsetToWorldOffset(screenOffset);
    vec2f const inertialVelocity = mRenderContext->ScreenOffsetToWorldOffset(inertialScreenOffset);

    // Apply action
    assert(!!mWorld);
    mWorld->MoveBy(
        shipId,
        worldOffset,
        inertialVelocity,
        mGameParameters);
}

void GameController::RotateBy(
    ElementId elementId,
    float screenDeltaY,
    vec2f const & screenCenter,
    float inertialScreenDeltaY)
{
    float const angle =
        2.0f * Pi<float>
        / static_cast<float>(mRenderContext->GetCanvasHeight())
        * screenDeltaY;

    vec2f const worldCenter = mRenderContext->ScreenToWorld(screenCenter);

    float const inertialAngle =
        2.0f * Pi<float>
        / static_cast<float>(mRenderContext->GetCanvasHeight())
        * inertialScreenDeltaY;

    // Apply action
    assert(!!mWorld);
    mWorld->RotateBy(
        elementId,
        angle,
        worldCenter,
        inertialAngle,
        mGameParameters);
}

void GameController::RotateBy(
    ShipId shipId,
    float screenDeltaY,
    vec2f const & screenCenter,
    float inertialScreenDeltaY)
{
    float const angle =
        2.0f * Pi<float>
        / static_cast<float>(mRenderContext->GetCanvasHeight())
        * screenDeltaY;

    vec2f const worldCenter = mRenderContext->ScreenToWorld(screenCenter);

    float const inertialAngle =
        2.0f * Pi<float>
        / static_cast<float>(mRenderContext->GetCanvasHeight())
        * inertialScreenDeltaY;

    // Apply action
    assert(!!mWorld);
    mWorld->RotateBy(
        shipId,
        angle,
        worldCenter,
        inertialAngle,
        mGameParameters);
}

void GameController::DestroyAt(
    vec2f const & screenCoordinates,
    float radiusFraction)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->DestroyAt(
        worldCoordinates,
        radiusFraction,
        mGameParameters);
}

void GameController::RepairAt(
    vec2f const & screenCoordinates,
    float radiusMultiplier,
    RepairSessionId sessionId,
    RepairSessionStepId sessionStepId)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->RepairAt(
        worldCoordinates,
        radiusMultiplier,
        sessionId,
        sessionStepId,
        mGameParameters);
}

void GameController::SawThrough(
    vec2f const & startScreenCoordinates,
    vec2f const & endScreenCoordinates)
{
    vec2f const startWorldCoordinates = mRenderContext->ScreenToWorld(startScreenCoordinates);
    vec2f const endWorldCoordinates = mRenderContext->ScreenToWorld(endScreenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->SawThrough(
        startWorldCoordinates,
        endWorldCoordinates,
        mGameParameters);
}

bool GameController::ApplyHeatBlasterAt(
    vec2f const & screenCoordinates,
    HeatBlasterActionType action)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Calculate radius
    float radius = mGameParameters.HeatBlasterRadius;
    if (mGameParameters.IsUltraViolentMode)
        radius *= 10.0f;

    // Apply action
    assert(!!mWorld);
    bool isApplied = mWorld->ApplyHeatBlasterAt(
        worldCoordinates,
        action,
        radius,
        mGameParameters);

    if (isApplied)
    {
        if (mDrawHeatBlasterFlame)
        {
            // Remember to render the flame at the next Render() step
            mHeatBlasterFlameToRender.emplace(
                worldCoordinates,
                radius,
                action);
        }
    }

    return isApplied;
}

bool GameController::ExtinguishFireAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Calculate radius
    float radius = mGameParameters.FireExtinguisherRadius;
    if (mGameParameters.IsUltraViolentMode)
        radius *= 10.0f;

    // Apply action
    assert(!!mWorld);
    bool isApplied = mWorld->ExtinguishFireAt(
        worldCoordinates,
        radius,
        mGameParameters);

    if (isApplied)
    {
        // Remember to render the extinguisher at the next Render() step
        mFireExtinguisherSprayToRender.emplace(
            worldCoordinates,
            radius);
    }

    return isApplied;
}

void GameController::DrawTo(
    vec2f const & screenCoordinates,
    float strengthFraction)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->DrawTo(
        worldCoordinates,
        strengthFraction,
        mGameParameters);
}

void GameController::SwirlAt(
    vec2f const & screenCoordinates,
    float strengthFraction)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->SwirlAt(
        worldCoordinates,
        strengthFraction,
        mGameParameters);
}

void GameController::TogglePinAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->TogglePinAt(
        worldCoordinates,
        mGameParameters);
}

bool GameController::InjectBubblesAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    return mWorld->InjectBubblesAt(
        worldCoordinates,
        mGameParameters);
}

bool GameController::FloodAt(
    vec2f const & screenCoordinates,
    float waterQuantityMultiplier)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    return mWorld->FloodAt(
        worldCoordinates,
        waterQuantityMultiplier,
        mGameParameters);
}

void GameController::ToggleAntiMatterBombAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->ToggleAntiMatterBombAt(
        worldCoordinates,
        mGameParameters);
}

void GameController::ToggleImpactBombAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->ToggleImpactBombAt(
        worldCoordinates,
        mGameParameters);
}

void GameController::ToggleRCBombAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->ToggleRCBombAt(
        worldCoordinates,
        mGameParameters);
}

void GameController::ToggleTimerBombAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    // Apply action
    assert(!!mWorld);
    mWorld->ToggleTimerBombAt(
        worldCoordinates,
        mGameParameters);
}

void GameController::DetonateRCBombs()
{
    // Apply action
    assert(!!mWorld);
    mWorld->DetonateRCBombs();
}

void GameController::DetonateAntiMatterBombs()
{
    // Apply action
    assert(!!mWorld);
    mWorld->DetonateAntiMatterBombs();
}

void GameController::AdjustOceanSurfaceTo(std::optional<vec2f> const & screenCoordinates)
{
    std::optional<vec2f> const worldCoordinates = !!screenCoordinates
        ? mRenderContext->ScreenToWorld(*screenCoordinates)
        : std::optional<vec2f>();

    // Apply action
    assert(!!mWorld);
    mWorld->AdjustOceanSurfaceTo(worldCoordinates);
}

bool GameController::AdjustOceanFloorTo(vec2f const & startScreenCoordinates, vec2f const & endScreenCoordinates)
{
    vec2f const startWorldCoordinates = mRenderContext->ScreenToWorld(startScreenCoordinates);
    vec2f const endWorldCoordinates = mRenderContext->ScreenToWorld(endScreenCoordinates);

    assert(!!mWorld);
    return mWorld->AdjustOceanFloorTo(
        startWorldCoordinates.x,
        startWorldCoordinates.y,
        endWorldCoordinates.x,
        endWorldCoordinates.y);
}

bool GameController::ScrubThrough(
    vec2f const & startScreenCoordinates,
    vec2f const & endScreenCoordinates)
{
    vec2f const startWorldCoordinates = mRenderContext->ScreenToWorld(startScreenCoordinates);
    vec2f const endWorldCoordinates = mRenderContext->ScreenToWorld(endScreenCoordinates);

    // Apply action
    assert(!!mWorld);
    return mWorld->ScrubThrough(
        startWorldCoordinates,
        endWorldCoordinates,
        mGameParameters);
}

void GameController::ApplyThanosSnapAt(vec2f const & screenCoordinates)
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    StartThanosSnapStateMachine(worldCoordinates.x, mWorld->GetCurrentSimulationTime());
}

std::optional<ElementId> GameController::GetNearestPointAt(vec2f const & screenCoordinates) const
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    assert(!!mWorld);
    return mWorld->GetNearestPointAt(worldCoordinates, 1.0f);
}

void GameController::QueryNearestPointAt(vec2f const & screenCoordinates) const
{
    vec2f const worldCoordinates = mRenderContext->ScreenToWorld(screenCoordinates);

    assert(!!mWorld);
    mWorld->QueryNearestPointAt(worldCoordinates, 1.0f);
}

void GameController::TriggerTsunami()
{
    assert(!!mWorld);
    mWorld->TriggerTsunami();
}

void GameController::TriggerRogueWave()
{
    assert(!!mWorld);
    mWorld->TriggerRogueWave();
}

//
// Render controls
//

void GameController::SetCanvasSize(int width, int height)
{
    mRenderContext->SetCanvasSize(width, height);

    // Pickup eventual changes
    mTargetCameraPosition = mCurrentCameraPosition = mRenderContext->GetCameraWorldPosition();
    mTargetZoom = mCurrentZoom = mRenderContext->GetZoom();
}

void GameController::Pan(vec2f const & screenOffset)
{
    vec2f worldOffset = mRenderContext->ScreenOffsetToWorldOffset(screenOffset);
    vec2f newTargetCameraPosition = mRenderContext->ClampCameraWorldPosition(mTargetCameraPosition + worldOffset);

    mCurrentCameraPosition = mRenderContext->SetCameraWorldPosition(mTargetCameraPosition); // Skip straight to current target, in case we're already smoothing
    mStartingCameraPosition = mCurrentCameraPosition;
    mTargetCameraPosition = newTargetCameraPosition;

    mStartCameraPositionTimestamp = std::chrono::steady_clock::now();
}

void GameController::PanImmediate(vec2f const & screenOffset)
{
    vec2f const worldOffset = mRenderContext->ScreenOffsetToWorldOffset(screenOffset);

    auto const newCameraWorldPosition = mRenderContext->SetCameraWorldPosition(
        mRenderContext->GetCameraWorldPosition() + worldOffset);

    mTargetCameraPosition = mCurrentCameraPosition = newCameraWorldPosition;
}

void GameController::ResetPan()
{
    auto const newCameraWorldPosition = mRenderContext->SetCameraWorldPosition(vec2f(0, 0));

    mTargetCameraPosition = mCurrentCameraPosition = newCameraWorldPosition;
}

void GameController::AdjustZoom(float amount)
{
    float newTargetZoom = mRenderContext->ClampZoom(mTargetZoom * amount);

    mCurrentZoom = mRenderContext->SetZoom(mTargetZoom); // Skip straight to current target, in case we're already smoothing
    mStartingZoom = mCurrentZoom;
    mTargetZoom = newTargetZoom;

    mStartZoomTimestamp = std::chrono::steady_clock::now();
}

void GameController::ResetZoom()
{
    auto const newZoom = mRenderContext->SetZoom(1.0);

    mTargetZoom = mCurrentZoom = newZoom;
}

vec2f GameController::ScreenToWorld(vec2f const & screenCoordinates) const
{
    return mRenderContext->ScreenToWorld(screenCoordinates);
}

////////////////////////////////////////////////////////////////////////////////////////

void GameController::OnTsunami(float x)
{
    if (mShowTsunamiNotifications)
    {
        // Start state machine
        StartTsunamiNotificationStateMachine(x);
    }
}

void GameController::InternalUpdate()
{
    auto now = GameWallClock::GetInstance().Now();

    // Update parameter smoothers
    std::for_each(
        mParameterSmoothers.begin(),
        mParameterSmoothers.end(),
        [&now](auto & ps)
        {
            ps.Update(now);
        });

    // Update world
    assert(!!mWorld);
    mWorld->Update(
        mGameParameters,
        *mRenderContext);

    // Flush events
    mGameEventDispatcher->Flush();

    // Update state machines
    UpdateStateMachines(mWorld->GetCurrentSimulationTime());
}

void GameController::InternalRender()
{
    //
    // Do zoom smoothing
    //

    if (mCurrentZoom != mTargetZoom)
    {
        SmoothToTarget(
            mCurrentZoom,
            mStartingZoom,
            mTargetZoom,
            mStartZoomTimestamp);

        mRenderContext->SetZoom(mCurrentZoom);
    }


    //
    // Do camera smoothing
    //

    if (mCurrentCameraPosition != mTargetCameraPosition)
    {
        SmoothToTarget(
            mCurrentCameraPosition.x,
            mStartingCameraPosition.x,
            mTargetCameraPosition.x,
            mStartCameraPositionTimestamp);

        SmoothToTarget(
            mCurrentCameraPosition.y,
            mStartingCameraPosition.y,
            mTargetCameraPosition.y,
            mStartCameraPositionTimestamp);

        mRenderContext->SetCameraWorldPosition(mCurrentCameraPosition);
    }


    //
    // Start rendering
    //

    mRenderContext->RenderStart();


    //
    // Render world
    //

    assert(!!mWorld);
    mWorld->Render(mGameParameters, *mRenderContext);


    //
    // Render HeatBlaster flame, if any
    //

    if (!!mHeatBlasterFlameToRender)
    {
        mRenderContext->UploadHeatBlasterFlame(
            std::get<0>(*mHeatBlasterFlameToRender),
            std::get<1>(*mHeatBlasterFlameToRender),
            std::get<2>(*mHeatBlasterFlameToRender));

        mHeatBlasterFlameToRender.reset();
    }

    //
    // Render fire extinguisher spray, if any

    if (!!mFireExtinguisherSprayToRender)
    {
        mRenderContext->UploadFireExtinguisherSpray(
            std::get<0>(*mFireExtinguisherSprayToRender),
            std::get<1>(*mFireExtinguisherSprayToRender));

        mFireExtinguisherSprayToRender.reset();
    }

    //
    // Render status text
    //

    assert(!!mStatusText);
    mStatusText->Render(*mRenderContext);


    //
    // Finish rendering
    //

    mRenderContext->RenderEnd();
}

void GameController::SmoothToTarget(
    float & currentValue,
    float startingValue,
    float targetValue,
    std::chrono::steady_clock::time_point startingTime)
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    // Amplitude - summing up pieces from zero to PI yields PI/2
    float amp = (targetValue - startingValue) / (Pi<float> / 2.0f);

    // X - after SmoothMillis we want PI
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startingTime);
    float x = static_cast<float>(elapsed.count()) * Pi<float> / static_cast<float>(SmoothMillis);
    float dv = amp * sinf(x) * sinf(x);

    float oldCurrentValue = currentValue;
    currentValue += dv;

    // Check if we've overshot
    if ((targetValue - oldCurrentValue) * (targetValue - currentValue) < 0.0f)
    {
        // Overshot
        currentValue = targetValue;
    }
}

void GameController::Reset(std::unique_ptr<Physics::World> newWorld)
{
    // Reset world
    assert(!!mWorld);
    mWorld = std::move(newWorld);

    // Reset state
    ResetStateMachines();

    // Reset rendering engine
    assert(!!mRenderContext);
    mRenderContext->Reset();

    // Notify
    mGameEventDispatcher->OnGameReset();
}

void GameController::OnShipAdded(
    ShipDefinition shipDefinition,
    std::filesystem::path const & shipDefinitionFilepath,
    ShipId shipId)
{
    // Add ship to rendering engine
    mRenderContext->AddShip(
        shipId,
        mWorld->GetShipPointCount(shipId),
        mWorld->GetShipPointBufferElementCount(shipId),
        std::move(shipDefinition.TextureLayerImage),
        shipDefinition.TextureOrigin);

    // Notify
    mGameEventDispatcher->OnShipLoaded(
        shipId,
        shipDefinition.Metadata.ShipName,
        shipDefinition.Metadata.Author);

    // Remember last loaded ship
    mLastShipLoadedFilepath = shipDefinitionFilepath;
}

void GameController::PublishStats(std::chrono::steady_clock::time_point nowReal)
{
    // Calculate fps

    auto totalElapsedReal = std::chrono::duration<float>(nowReal - mRenderStatsOriginTimestampReal);
    auto lastElapsedReal = std::chrono::duration<float>(nowReal - mRenderStatsLastTimestampReal);

    float const totalFps =
        totalElapsedReal.count() != 0.0f
        ? static_cast<float>(mTotalFrameCount) / totalElapsedReal.count()
        : 0.0f;

    float const lastFps =
        lastElapsedReal.count() != 0.0f
        ? static_cast<float>(mLastFrameCount) / lastElapsedReal.count()
        : 0.0f;

    // Calculate UR ratio

    auto totalUpdateDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(mTotalUpdateDuration);
    auto totalRenderDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(mTotalRenderDuration);
    auto lastUpdateDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(mTotalUpdateDuration - mLastTotalUpdateDuration);
    auto lastRenderDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(mTotalRenderDuration - mLastTotalRenderDuration);

    float const totalURRatio = totalRenderDurationNs.count() != 0
        ? static_cast<float>(totalUpdateDurationNs.count()) / static_cast<float>(totalRenderDurationNs.count())
        : 0.0f;

    float const lastURRatio = lastRenderDurationNs.count() != 0
        ? static_cast<float>(lastUpdateDurationNs.count()) / static_cast<float>(lastRenderDurationNs.count())
        : 0.0f;


    // Publish frame rate
    assert(!!mGameEventDispatcher);
    mGameEventDispatcher->OnFrameRateUpdated(lastFps, totalFps);

    // Publish UR ratio
    assert(!!mGameEventDispatcher);
    mGameEventDispatcher->OnUpdateToRenderRatioUpdated(lastURRatio);

    // Update status text
    assert(!!mStatusText);
    mStatusText->SetText(
        lastFps,
        totalFps,
        std::chrono::duration<float>(GameWallClock::GetInstance().Now() - mOriginTimestampGame),
        mIsPaused,
        mRenderContext->GetZoom(),
        mRenderContext->GetCameraWorldPosition(),
        totalURRatio,
        lastURRatio,
        mRenderContext->GetStatistics());
}