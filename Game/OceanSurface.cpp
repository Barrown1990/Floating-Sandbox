﻿/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-04-14
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

#include <algorithm>

namespace Physics {

// The multiplicative factor to transform wind speed into wave speed
constexpr float WindSpeedToWaveSpeedFactor = 1.0f / 6.0f; // Waves move slower than wind

// The number of slices we want to render the water surface as;
// this is the graphical resolution
template<typename T>
constexpr T RenderSlices = 500;

//
// SWE Layer
//

// The rest height of the height field - indirectly determines velocity
// of waves (via dv/dt <= dh/dx, with dh/dt <= h*dv/dx)
constexpr float SWEHeightFieldOffset = 100.0f;

// The factor by which we amplify the height field perturbations;
// higher values allow for smaller height field variations with the same visual height,
// and smaller height field variations allow for greater stability
constexpr float SWEHeightFieldAmplification = 50.0f;

// The number of samples we set apart in the SWE buffers for wave generation at each end of a buffer
constexpr size_t SWEWaveGenerationSamples = 1;

// The number of samples we set apart in the SWE buffers for boundary conditions at each end of a buffer
constexpr size_t SWEBoundaryConditionsSamples = 1;

constexpr size_t SWEOuterLayerSamples =
    SWEWaveGenerationSamples
    + SWEBoundaryConditionsSamples;

// The total number of samples in the SWE buffers
constexpr size_t SWETotalSamples =
    SWEOuterLayerSamples
    + OceanSurface::SamplesCount
    + SWEOuterLayerSamples;

OceanSurface::OceanSurface()
    : mSamples(new Sample[SamplesCount + 1])
    , mHeightFieldBuffer1(new float[SWETotalSamples + 1]) // One extra cell just to ease interpolations
    , mHeightFieldBuffer2(new float[SWETotalSamples + 1]) // One extra cell just to ease interpolations
    , mVelocityFieldBuffer1(new float[SWETotalSamples + 1]) // One extra cell just to ease interpolations
    , mVelocityFieldBuffer2(new float[SWETotalSamples + 1]) // One extra cell just to ease interpolations
    ////////
    , mSWEExternalWaveStateMachine()
{
    //
    // Initialize SWE layer
    //

    // Initialize buffer pointers
    mCurrentHeightField = mHeightFieldBuffer1.get();
    mNextHeightField = mHeightFieldBuffer2.get();
    mCurrentVelocityField = mVelocityFieldBuffer1.get();
    mNextVelocityField = mVelocityFieldBuffer2.get();

    // Initialize all values - including extra unused sample
    //
    // Velocity boundary conditions are initialized here once and for all
    for (size_t i = 0; i <= SWETotalSamples; ++i)
    {
        mCurrentHeightField[i] = SWEHeightFieldOffset;
        mNextHeightField[i] = SWEHeightFieldOffset;

        mCurrentVelocityField[i] = 0.0f;
        mNextVelocityField[i] = 0.0f;
    }
}

void OceanSurface::Update(
    float currentSimulationTime,
    Wind const & wind,
    GameParameters const & gameParameters)
{
    //
    // 1. SWE Wave Genesis
    //

    // TODO


    //
    // 2. Advance SWE Waves
    //

    if (!!mSWEExternalWaveStateMachine)
    {
        auto heightValue = mSWEExternalWaveStateMachine->Update(currentSimulationTime);
        if (!heightValue)
        {
            // Done
            mSWEExternalWaveStateMachine.reset();
        }
        else
        {
            mCurrentHeightField[mSWEExternalWaveStateMachine->GetSampleIndex()] = *heightValue;
        }
    }

    // TODO: wave genesis


    //
    // 3. SWE Update
    //
    // - Current -> Next
    //

    AdvectHeightField();
    AdvectVelocityField();

    UpdateHeightField();
    UpdateVelocityField();

    // Set reflective boundary conditions
    for (size_t i = 0; i < SWEBoundaryConditionsSamples; ++i)
    {
        // Height mirrors height of actual samples
        mNextHeightField[i] = mNextHeightField[i + SWEBoundaryConditionsSamples];
        mNextHeightField[SWETotalSamples - 1 - i] = mNextHeightField[SWETotalSamples - 1 - SWEBoundaryConditionsSamples - i];

        // Velocity is zero
        assert(mNextVelocityField[i] == 0.0f);
        assert(mNextVelocityField[SWETotalSamples - 1 - i] == 0.0f);
    }

    ////// Calc avg height among all samples
    ////float avgHeight = 0.0f;
    ////for (size_t i = SWEOuterLayerSamples; i < SWEOuterLayerSamples + SamplesCount; ++i)
    ////{
    ////    avgHeight += mNextHeightField[i];
    ////}
    ////avgHeight /= static_cast<float>(SamplesCount);
    ////LogMessage("AVG:", avgHeight);



    //
    // 4. Swap Buffers
    //

    std::swap(mCurrentHeightField, mNextHeightField);
    std::swap(mCurrentVelocityField, mNextVelocityField);


    //
    // 5. Generate samples
    //
    // - Samples values are a combination of:
    //  - SWE's height field
    //  - Basal waves
    //  - Wind gust ripples
    //

    ////float constexpr BasalWaveLength = 60.0f; // metres
    ////float constexpr BasalWavePeriod = 2.5f; // seconds

    static constexpr float WindGustRippleSpatialFrequency = 0.5f;

    float const windSpeedAbsoluteMagnitude = wind.GetCurrentWindSpeed().length();
    float const windSpeedGustRelativeAmplitude = wind.GetMaxSpeedMagnitude() - wind.GetBaseSpeedMagnitude();
    float const rawWindNormalizedIncisiveness = (windSpeedGustRelativeAmplitude == 0.0f)
        ? 0.0f
        : std::max(0.0f, windSpeedAbsoluteMagnitude - abs(wind.GetBaseSpeedMagnitude()))
            / abs(windSpeedGustRelativeAmplitude);

    float const windRipplesTimeFrequency = (gameParameters.WindSpeedBase >= 0.0f)
        ? 128.0f
        : -128.0f;

    float const smoothedWindNormalizedIncisiveness = mWindIncisivenessRunningAverage.Update(rawWindNormalizedIncisiveness);
    float const windRipplesWaveHeight = 0.7f * smoothedWindNormalizedIncisiveness;

    // sample index = 0
    float previousSampleValue;
    {
        float const rippleValue = sinf(-currentSimulationTime * windRipplesTimeFrequency);
        previousSampleValue =
            (mCurrentHeightField[SWEOuterLayerSamples + 0] - SWEHeightFieldOffset) * SWEHeightFieldAmplification
            + rippleValue * windRipplesWaveHeight;

        mSamples[0].SampleValue = previousSampleValue;
    }

    // sample index = 1...SamplesCount - 1
    float x = Dx;
    for (int64_t i = 1; i < SamplesCount; i++, x += Dx)
    {
        float const rippleValue = sinf(x * WindGustRippleSpatialFrequency - currentSimulationTime * windRipplesTimeFrequency);
        float const sampleValue =
            (mCurrentHeightField[SWEOuterLayerSamples + i] - SWEHeightFieldOffset) * SWEHeightFieldAmplification
            + rippleValue * windRipplesWaveHeight;

        mSamples[i].SampleValue = sampleValue;
        mSamples[i - 1].SampleValuePlusOneMinusSampleValue = sampleValue - previousSampleValue;

        previousSampleValue = sampleValue;
    }

    // Populate last delta (extra sample will have same value as this sample)
    mSamples[SamplesCount - 1].SampleValuePlusOneMinusSampleValue = 0.0f;

    // Populate extra sample - same value as last sample
    mSamples[SamplesCount].SampleValue = mSamples[SamplesCount - 1].SampleValue;
    mSamples[SamplesCount].SampleValuePlusOneMinusSampleValue = 0.0f; // Never used
}

// TODOOLD
////void OceanSurface::Update(
////    float currentSimulationTime,
////    Wind const & wind,
////    GameParameters const & gameParameters)
////{
////    // Waves
////
////    float const waveSpeed = gameParameters.WindSpeedBase / 6.0f; // Water moves slower than wind
////    float const waveTheta = currentSimulationTime * (0.5f + waveSpeed) / 3.0f;
////    float const waveHeight = gameParameters.WaveHeight;
////
////    // Ripples
////
////    float const windSpeedAbsoluteMagnitude = wind.GetCurrentWindSpeed().length();
////    float const windSpeedGustRelativeAmplitude = wind.GetMaxSpeedMagnitude() - wind.GetBaseSpeedMagnitude();
////    float const rawWindNormalizedIncisiveness = (windSpeedGustRelativeAmplitude == 0.0f)
////        ? 0.0f
////        : std::max(0.0f, windSpeedAbsoluteMagnitude - abs(wind.GetBaseSpeedMagnitude()))
////          / abs(windSpeedGustRelativeAmplitude);
////
////    float const windRipplesTimeFrequency = (gameParameters.WindSpeedBase >= 0.0f)
////        ? 128.0f
////        : -128.0f;
////
////    float const smoothedWindNormalizedIncisiveness = mWindIncisivenessRunningAverage.Update(rawWindNormalizedIncisiveness);
////    float const windRipplesWaveHeight = 0.7f * smoothedWindNormalizedIncisiveness;
////
////    // Spatial frequencies of the wave components
////
////    static constexpr float SpatialFrequency1 = 0.1f;
////    static constexpr float SpatialFrequency2 = 0.3f;
////    static constexpr float SpatialFrequency3 = 0.5f; // Wind component
////
////    //
////    // Create samples
////    //
////
////    // sample index = 0
////    float previousSampleValue;
////    {
////        float const c1 = sinf(waveTheta) * 0.5f;
////        float const c2 = sinf(-waveTheta * 1.1f) * 0.3f;
////        float const c3 = sinf(-currentSimulationTime * windRipplesTimeFrequency);
////        previousSampleValue = (c1 + c2) * waveHeight + c3 * windRipplesWaveHeight;
////        mSamples[0].SampleValue = previousSampleValue;
////    }
////
////    // sample index = 1...SamplesCount - 1
////    float x = Dx;
////    for (int64_t i = 1; i < SamplesCount; i++, x += Dx)
////    {
////        float const c1 = sinf(x * SpatialFrequency1 + waveTheta) * 0.5f;
////        float const c2 = sinf(x * SpatialFrequency2 - waveTheta * 1.1f) * 0.3f;
////        float const c3 = sinf(x * SpatialFrequency3 - currentSimulationTime * windRipplesTimeFrequency);
////        float const sampleValue = (c1 + c2) * waveHeight + c3 * windRipplesWaveHeight;
////        mSamples[i].SampleValue = sampleValue;
////        mSamples[i - 1].SampleValuePlusOneMinusSampleValue = sampleValue - previousSampleValue;
////
////        previousSampleValue = sampleValue;
////    }
////
////    // Populate last delta (extra sample has same value as this sample)
////    mSamples[SamplesCount - 1].SampleValuePlusOneMinusSampleValue = 0.0f;
////
////    // Populate extra sample - same value as last sample
////    mSamples[SamplesCount].SampleValue = mSamples[SamplesCount - 1].SampleValue;
////    mSamples[SamplesCount].SampleValuePlusOneMinusSampleValue = 0.0f; // Never used
////}

void OceanSurface::Upload(
    GameParameters const & gameParameters,
    Render::RenderContext & renderContext) const
{
    //
    // We want to upload at most RenderSlices slices
    //

    // Find index of leftmost sample, and its corresponding world X
    auto sampleIndex = FastTruncateInt64((renderContext.GetVisibleWorldLeft() + GameParameters::HalfMaxWorldWidth) / Dx);
    float sampleIndexX = -GameParameters::HalfMaxWorldWidth + (Dx * sampleIndex);

    // Calculate number of samples required to cover screen from leftmost sample
    // up to the visible world right (included)
    float const coverageWidth = renderContext.GetVisibleWorldRight() - sampleIndexX;
    auto const numberOfSamplesToRender = static_cast<int64_t>(ceil(coverageWidth / Dx));

    if (numberOfSamplesToRender >= RenderSlices<int64_t>)
    {
        //
        // Have to take more than 1 sample per slice
        //

        renderContext.UploadOceanStart(RenderSlices<int>);

        // Calculate dx between each pair of slices with want to upload
        float const sliceDx = coverageWidth / RenderSlices<float>;

        // We do one extra iteration as the number of slices is the number of quads, and the last vertical
        // quad side must be at the end of the width
        for (int64_t s = 0; s <= RenderSlices<int64_t>; ++s, sampleIndexX += sliceDx)
        {
            renderContext.UploadOcean(
                sampleIndexX,
                GetHeightAt(sampleIndexX),
                gameParameters.SeaDepth);
        }
    }
    else
    {
        //
        // We just upload the required number of samples, which is less than
        // the max number of slices we're prepared to upload, and we let OpenGL
        // interpolate on our behalf
        //

        renderContext.UploadOceanStart(numberOfSamplesToRender);

        // We do one extra iteration as the number of slices is the number of quads, and the last vertical
        // quad side must be at the end of the width
        for (std::int64_t s = 0; s <= numberOfSamplesToRender; ++s, sampleIndexX += Dx)
        {
            renderContext.UploadOcean(
                sampleIndexX,
                mSamples[s + sampleIndex].SampleValue,
                gameParameters.SeaDepth);
        }
    }

    renderContext.UploadOceanEnd();
}

void OceanSurface::AdjustTo(
    std::optional<vec2f> const & worldCoordinates,
    float currentSimulationTime)
{
    if (!!worldCoordinates)
    {
        // Calculate target height
        float targetHeight =
            (worldCoordinates->y / SWEHeightFieldAmplification)
            + SWEHeightFieldOffset;

        // Check whether we are already advancing a wave
        if (!mSWEExternalWaveStateMachine)
        {
            //
            // Start advancing a new wave
            //

            // Calculate sample index, minimizing error
            float const sampleIndexF = (worldCoordinates->x + GameParameters::HalfMaxWorldWidth) / Dx;
            int32_t sampleIndexI = FastTruncateInt32(sampleIndexF + 0.5f);
            assert(sampleIndexI >= 0 && sampleIndexI <= SamplesCount);

            // Start wave
            mSWEExternalWaveStateMachine.emplace(
                sampleIndexI,
                mCurrentHeightField[SWEOuterLayerSamples + sampleIndexI], // LowHeight
                targetHeight,
                SWEWaveStateMachine::ReleaseModeType::OnCue,
                currentSimulationTime);
        }
        else
        {
            //
            // Restart currently-advancing wave
            //

            mSWEExternalWaveStateMachine->Restart(
                targetHeight,
                currentSimulationTime);
        }
    }
    else
    {
        //
        // Start release of currently-advancing wave
        //

        assert(!!mSWEExternalWaveStateMachine);
        mSWEExternalWaveStateMachine->Release(currentSimulationTime);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void OceanSurface::AdvectHeightField()
{
    //
    // Semi-Lagrangian method
    //

    // Process all height samples, except for boundary condition samples
    for (size_t i = SWEBoundaryConditionsSamples; i < SWETotalSamples - SWEBoundaryConditionsSamples; ++i)
    {
        // The height field values are at the center of the cell,
        // while velocities are at the edges - hence we need to take
        // the two neighboring velocities
        float const v = (mCurrentVelocityField[i] + mCurrentVelocityField[i + 1]) / 2.0f;

        // Calculate the (fractional) index that this height sample had one time step ago
        float const prevCellIndex =
            static_cast<float>(i) /*+ 0.5f*/
            - v * GameParameters::SimulationStepTimeDuration<float> / Dx;

        // Transform index to ease interpolations, constraining the cell
        // to our grid at the same time
        float const prevCellIndex2 = std::min(
            std::max(0.0f, prevCellIndex /*- 0.5f*/),
            static_cast<float>(SWETotalSamples - 1));

        // Calculate integral and fractional parts of the index
        int32_t const prevCellIndexI = FastTruncateInt32(prevCellIndex2);
        float const prevCellIndexF = prevCellIndex2 - prevCellIndexI;
        assert(prevCellIndexF >= 0.0f && prevCellIndexF < 1.0f);

        // Set this height field sample as the previous (in time) sample,
        // interpolated between its two neighbors
        mNextHeightField[i] =
            (1.0f - prevCellIndexF) * mCurrentHeightField[prevCellIndexI]
            + prevCellIndexF * mCurrentHeightField[prevCellIndexI + 1];
    }
}

void OceanSurface::AdvectVelocityField()
{
    //
    // Semi-Lagrangian method
    //

    // Process all velocity samples, except for boundary condition samples
    for (size_t i = SWEBoundaryConditionsSamples; i < SWETotalSamples - SWEBoundaryConditionsSamples; ++i)
    {
        // Velocity values are at the edges of the cell
        float const v = mCurrentVelocityField[i];

        // Calculate the (fractional) index that this velocity sample had one time step ago
        float const prevCellIndex =
            static_cast<float>(i)
            - v * GameParameters::SimulationStepTimeDuration<float> / Dx;

        // Transform index to ease interpolations, constraining the cell
        // to our grid at the same time
        float const prevCellIndex2 = std::min(
            std::max(0.0f, prevCellIndex),
            static_cast<float>(SWETotalSamples - 1));

        // Calculate integral and fractional parts of the index
        int32_t const prevCellIndexI = FastTruncateInt32(prevCellIndex2);
        float const prevCellIndexF = prevCellIndex2 - prevCellIndexI;
        assert(prevCellIndexF >= 0.0f && prevCellIndexF < 1.0f);

        // Set this velocity field sample as the previous (in time) sample,
        // interpolated between its two neighbors
        mNextVelocityField[i] =
            (1.0f - prevCellIndexF) * mCurrentVelocityField[prevCellIndexI]
            + prevCellIndexF * mCurrentVelocityField[prevCellIndexI + 1];
    }
}

void OceanSurface::UpdateHeightField()
{
    // Process all samples, except for boundary condition samples
    for (size_t i = SWEBoundaryConditionsSamples; i < SWETotalSamples - SWEBoundaryConditionsSamples; ++i)
    {
        mNextHeightField[i] -=
            mNextHeightField[i]
            * (mNextVelocityField[i + 1] - mNextVelocityField[i]) / Dx
            * GameParameters::SimulationStepTimeDuration<float>;
    }
}

void OceanSurface::UpdateVelocityField()
{
    // Process all samples, except for boundary condition samples
    for (size_t i = SWEBoundaryConditionsSamples; i < SWETotalSamples - SWEBoundaryConditionsSamples; ++i)
    {
        mNextVelocityField[i] +=
            GameParameters::GravityMagnitude
            * (mNextHeightField[i - 1] - mNextHeightField[i]) / Dx
            * GameParameters::SimulationStepTimeDuration<float>;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

OceanSurface::SWEWaveStateMachine::SWEWaveStateMachine(
    int32_t sampleIndex,
    float startHeight,
    float targetHeight,
    ReleaseModeType releaseMode,
    float currentSimulationTime)
    : mSampleIndex(sampleIndex)
    , mLowHeight(startHeight)
    , mCurrentPhaseStartHeight(startHeight)
    , mCurrentPhaseTargetHeight(targetHeight)
    , mCurrentHeight(startHeight)
    , mCurrentProgress(0.0f)
    , mStartSimulationTime(currentSimulationTime)
    , mCurrentWavePhase(WavePhaseType::Rise)
    , mReleaseMode(releaseMode)
    , mSmoothingDelay(CalculateSmoothingDelay())
{}

void OceanSurface::SWEWaveStateMachine::Restart(
    float restartHeight,
    float currentSimulationTime)
{
    // Rise in any case, and our new target is the restart height

    mCurrentPhaseStartHeight = mCurrentHeight;
    mCurrentPhaseTargetHeight = restartHeight;
    mCurrentProgress = 0.0f;
    mStartSimulationTime = currentSimulationTime;
    mCurrentWavePhase = WavePhaseType::Rise;

    // Recalculate delay
    mSmoothingDelay = CalculateSmoothingDelay();
}

void OceanSurface::SWEWaveStateMachine::Release(float currentSimulationTime)
{
    if (mCurrentWavePhase == WavePhaseType::Rise)
    {
        // Start falling
        StartFallPhase(currentSimulationTime);
    }
    else
    {
        // Stop altogether
        mCurrentProgress = 1.0f;
    }
}

std::optional<float> OceanSurface::SWEWaveStateMachine::Update(
    float currentSimulationTime)
{
    // Advance iff we are not done yet
    if (mCurrentProgress < 1.0f)
    {
        mCurrentProgress =
            (currentSimulationTime - mStartSimulationTime)
            / mSmoothingDelay;
    }

    // Calculate sinusoidal progress
    float const sinProgress = sin(Pi<float> / 2.0f * std::min(mCurrentProgress, 1.0f));

    // Calculate new height value
    mCurrentHeight =
        mCurrentPhaseStartHeight + (mCurrentPhaseTargetHeight - mCurrentPhaseStartHeight) * sinProgress;

    // Check whether it's time to switch phase
    if (mCurrentProgress >= 1.0f)
    {
        if (mCurrentWavePhase == WavePhaseType::Rise)
        {
            if (mReleaseMode == ReleaseModeType::Automatic)
            {
                // Start falling
                StartFallPhase(currentSimulationTime);
            }
        }
        else
        {
            // We're done
            return std::nullopt;
        }
    }

    return mCurrentHeight;
}

float OceanSurface::SWEWaveStateMachine::CalculateSmoothingDelay()
{
    float const deltaH = std::min(
        abs(mCurrentPhaseTargetHeight - mCurrentHeight),
        SWEHeightFieldOffset / 5.0f);

    float delayTicks;
    if (mCurrentWavePhase == WavePhaseType::Rise)
    {
        //
        // Number of ticks must fit:
        //  DeltaH=0.0  => Ticks=0.0
        //  DeltaH=0.2  => Ticks=8.0
        //  DeltaH=2.0  => Ticks=150.0
        //  DeltaH=4.0  => Ticks=200.0
        //  DeltaH>4.0  => Ticks~=200.0
        // y = -19.88881 - (-147.403/0.6126081)*(1 - e^(-0.6126081*x))
        delayTicks =
            -19.88881f
            + (147.403f / 0.6126081f) * (1.0f - exp(-0.6126081f * deltaH));
    }
    else
    {
        //
        // Number of ticks must fit:
        //  DeltaH=0.1  => Ticks=2.0
        //  DeltaH=0.25 => Ticks=3.0
        //  DeltaH=1.0  => Ticks=7.0
        //  DeltaH=2.0  => Ticks=10.0
        // y = 1.220013 - (-7.8394/0.6485749)*(1 - e^(-0.6485749*x))
        delayTicks =
            1.220013f
            + (7.8394f / 0.6485749f) * (1.0f - exp(-0.6485749f * deltaH));
    }

    float const delay =
        std::max(delayTicks, 1.0f)
        * GameParameters::SimulationStepTimeDuration<float>;

    return delay;
}

void OceanSurface::SWEWaveStateMachine::StartFallPhase(float currentSimulationTime)
{
    assert(mCurrentWavePhase == WavePhaseType::Rise);

    mCurrentPhaseStartHeight = mCurrentHeight;
    mCurrentPhaseTargetHeight = mLowHeight;
    mCurrentProgress = 0.0f;
    mStartSimulationTime = currentSimulationTime;
    mCurrentWavePhase = WavePhaseType::Fall;
    mSmoothingDelay = CalculateSmoothingDelay();
}

}