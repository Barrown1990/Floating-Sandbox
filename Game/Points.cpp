/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-05-06
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

#include <GameCore/GameRandomEngine.h>
#include <GameCore/Log.h>
#include <GameCore/PrecalculatedFunction.h>

#include <cmath>
#include <limits>

namespace Physics {

void Points::Add(
    vec2f const & position,
    StructuralMaterial const & structuralMaterial,
    ElectricalMaterial const * electricalMaterial,
    bool isRope,
    ElementIndex electricalElementIndex,
    bool isLeaking,
    vec4f const & color,
    vec2f const & textureCoordinates)
{
    ElementIndex const pointIndex = static_cast<ElementIndex>(mMaterialsBuffer.GetCurrentPopulatedSize());

    mMaterialsBuffer.emplace_back(&structuralMaterial, electricalMaterial);
    mIsRopeBuffer.emplace_back(isRope);

    mPositionBuffer.emplace_back(position);
    mVelocityBuffer.emplace_back(vec2f::zero());
    mForceBuffer.emplace_back(vec2f::zero());
    mAugmentedMaterialMassBuffer.emplace_back(structuralMaterial.GetMass());
    mMassBuffer.emplace_back(structuralMaterial.GetMass());
    mDecayBuffer.emplace_back(1.0f);
    mIntegrationFactorTimeCoefficientBuffer.emplace_back(CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations));

    mIntegrationFactorBuffer.emplace_back(vec2f::zero());
    mForceRenderBuffer.emplace_back(vec2f::zero());

    mMaterialIsHullBuffer.emplace_back(structuralMaterial.IsHull);
    mMaterialWaterVolumeFillBuffer.emplace_back(structuralMaterial.WaterVolumeFill);
    mMaterialWaterIntakeBuffer.emplace_back(structuralMaterial.WaterIntake);
    mMaterialWaterRestitutionBuffer.emplace_back(1.0f - structuralMaterial.WaterRetention);
    mMaterialWaterDiffusionSpeedBuffer.emplace_back(structuralMaterial.WaterDiffusionSpeed);

    mWaterBuffer.emplace_back(0.0f);
    mWaterVelocityBuffer.emplace_back(vec2f::zero());
    mWaterMomentumBuffer.emplace_back(vec2f::zero());
    mCumulatedIntakenWater.emplace_back(0.0f);
    mIsLeakingBuffer.emplace_back(isLeaking);
    if (isLeaking)
        SetLeaking(pointIndex);
    mFactoryIsLeakingBuffer.emplace_back(isLeaking);

    // Heat dynamics
    mTemperatureBuffer.emplace_back(GameParameters::InitialTemperature);
    mMaterialHeatCapacityBuffer.emplace_back(structuralMaterial.GetHeatCapacity());
    mMaterialIgnitionTemperatureBuffer.emplace_back(structuralMaterial.IgnitionTemperature);
    mCombustionStateBuffer.emplace_back(CombustionState());

    // Electrical dynamics
    mElectricalElementBuffer.emplace_back(electricalElementIndex);
    mLightBuffer.emplace_back(0.0f);

    // Wind dynamics
    mMaterialWindReceptivityBuffer.emplace_back(structuralMaterial.WindReceptivity);

    // Rust dynamics
    mMaterialRustReceptivityBuffer.emplace_back(structuralMaterial.RustReceptivity);

    // Ephemeral particles
    mEphemeralTypeBuffer.emplace_back(EphemeralType::None);
    mEphemeralStartTimeBuffer.emplace_back(0.0f);
    mEphemeralMaxLifetimeBuffer.emplace_back(0.0f);
    mEphemeralStateBuffer.emplace_back(EphemeralState::DebrisState());

    // Structure
    mConnectedSpringsBuffer.emplace_back();
    mFactoryConnectedSpringsBuffer.emplace_back();
    mConnectedTrianglesBuffer.emplace_back();
    mFactoryConnectedTrianglesBuffer.emplace_back();

    mConnectedComponentIdBuffer.emplace_back(NoneConnectedComponentId);
    mPlaneIdBuffer.emplace_back(NonePlaneId);
    mPlaneIdFloatBuffer.emplace_back(0.0f);
    mCurrentConnectivityVisitSequenceNumberBuffer.emplace_back();

    mIsPinnedBuffer.emplace_back(false);

    mRepairStateBuffer.emplace_back();

    mColorBuffer.emplace_back(color);
    mTextureCoordinatesBuffer.emplace_back(textureCoordinates);
}

void Points::CreateEphemeralParticleAirBubble(
    vec2f const & position,
    float initialSize,
    float vortexAmplitude,
    float vortexPeriod,
    StructuralMaterial const & structuralMaterial,
    float currentSimulationTime,
    PlaneId planeId)
{
    // Get a free slot (but don't steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, false);
    if (NoneElementIndex == pointIndex)
        return; // No luck

    //
    // Store attributes
    //

    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = vec2f::zero();
    mForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mDecayBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations);
    mMaterialsBuffer[pointIndex] = Materials(&structuralMaterial, nullptr);

    mMaterialWaterVolumeFillBuffer[pointIndex] = structuralMaterial.WaterVolumeFill;
    mMaterialWaterIntakeBuffer[pointIndex] = structuralMaterial.WaterIntake;
    mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - structuralMaterial.WaterRetention;
    mMaterialWaterDiffusionSpeedBuffer[pointIndex] = structuralMaterial.WaterDiffusionSpeed;
    mWaterBuffer[pointIndex] = 0.0f;
    assert(false == mIsLeakingBuffer[pointIndex]);

    mTemperatureBuffer[pointIndex] = GameParameters::InitialTemperature;
    mMaterialHeatCapacityBuffer[pointIndex] = structuralMaterial.GetHeatCapacity();
    mMaterialIgnitionTemperatureBuffer[pointIndex] = structuralMaterial.IgnitionTemperature;
    mCombustionStateBuffer[pointIndex] = CombustionState();

    mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 0.0f;

    mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralTypeBuffer[pointIndex] = EphemeralType::AirBubble;
    mEphemeralStartTimeBuffer[pointIndex] = currentSimulationTime;
    mEphemeralMaxLifetimeBuffer[pointIndex] = std::numeric_limits<float>::max();
    mEphemeralStateBuffer[pointIndex] = EphemeralState::AirBubbleState(
        GameRandomEngine::GetInstance().Choose<TextureFrameIndex>(2),
        initialSize,
        vortexAmplitude,
        vortexPeriod);

    mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    assert(false == mIsPinnedBuffer[pointIndex]);

    mColorBuffer[pointIndex] = structuralMaterial.RenderColor;
}

void Points::CreateEphemeralParticleDebris(
    vec2f const & position,
    vec2f const & velocity,
    StructuralMaterial const & structuralMaterial,
    float currentSimulationTime,
    std::chrono::milliseconds maxLifetime,
    PlaneId planeId)
{
    // Get a free slot (or steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, true);
    assert(NoneElementIndex != pointIndex);

    //
    // Store attributes
    //

    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = velocity;
    mForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mDecayBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations);
    mMaterialsBuffer[pointIndex] = Materials(&structuralMaterial, nullptr);

    mMaterialWaterVolumeFillBuffer[pointIndex] = 0.0f; // No buoyancy
    mMaterialWaterIntakeBuffer[pointIndex] = structuralMaterial.WaterIntake;
    mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - structuralMaterial.WaterRetention;
    mMaterialWaterDiffusionSpeedBuffer[pointIndex] = structuralMaterial.WaterDiffusionSpeed;
    mWaterBuffer[pointIndex] = 0.0f;
    assert(false == mIsLeakingBuffer[pointIndex]);

    mTemperatureBuffer[pointIndex] = GameParameters::InitialTemperature;
    mMaterialHeatCapacityBuffer[pointIndex] = structuralMaterial.GetHeatCapacity();
    mMaterialIgnitionTemperatureBuffer[pointIndex] = structuralMaterial.IgnitionTemperature;
    mCombustionStateBuffer[pointIndex] = CombustionState();

    mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 3.0f; // Debris are susceptible to wind

    mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralTypeBuffer[pointIndex] = EphemeralType::Debris;
    mEphemeralStartTimeBuffer[pointIndex] = currentSimulationTime;
    mEphemeralMaxLifetimeBuffer[pointIndex] = std::chrono::duration_cast<std::chrono::duration<float>>(maxLifetime).count();
    mEphemeralStateBuffer[pointIndex] = EphemeralState::DebrisState();

    mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    assert(false == mIsPinnedBuffer[pointIndex]);

    mColorBuffer[pointIndex] = structuralMaterial.RenderColor;

    // Remember that ephemeral points are dirty now
    mAreEphemeralPointsDirty = true;
}

void Points::CreateEphemeralParticleSparkle(
    vec2f const & position,
    vec2f const & velocity,
    StructuralMaterial const & structuralMaterial,
    float currentSimulationTime,
    std::chrono::milliseconds maxLifetime,
    PlaneId planeId)
{
    // Get a free slot (or steal one)
    auto pointIndex = FindFreeEphemeralParticle(currentSimulationTime, true);
    assert(NoneElementIndex != pointIndex);

    //
    // Store attributes
    //

    mPositionBuffer[pointIndex] = position;
    mVelocityBuffer[pointIndex] = velocity;
    mForceBuffer[pointIndex] = vec2f::zero();
    mAugmentedMaterialMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mMassBuffer[pointIndex] = structuralMaterial.GetMass();
    mDecayBuffer[pointIndex] = 1.0f;
    mIntegrationFactorTimeCoefficientBuffer[pointIndex] = CalculateIntegrationFactorTimeCoefficient(mCurrentNumMechanicalDynamicsIterations);
    mMaterialsBuffer[pointIndex] = Materials(&structuralMaterial, nullptr);

    mMaterialWaterVolumeFillBuffer[pointIndex] = 0.0f; // No buoyancy
    mMaterialWaterIntakeBuffer[pointIndex] = structuralMaterial.WaterIntake;
    mMaterialWaterRestitutionBuffer[pointIndex] = 1.0f - structuralMaterial.WaterRetention;
    mMaterialWaterDiffusionSpeedBuffer[pointIndex] = structuralMaterial.WaterDiffusionSpeed;
    mWaterBuffer[pointIndex] = 0.0f;
    assert(false == mIsLeakingBuffer[pointIndex]);

    mTemperatureBuffer[pointIndex] = 773.15f; // 500 Celsius, arbitrary
    mMaterialHeatCapacityBuffer[pointIndex] = structuralMaterial.GetHeatCapacity();
    mMaterialIgnitionTemperatureBuffer[pointIndex] = structuralMaterial.IgnitionTemperature;
    mCombustionStateBuffer[pointIndex] = CombustionState();

    mLightBuffer[pointIndex] = 0.0f;

    mMaterialWindReceptivityBuffer[pointIndex] = 5.0f; // Sparkles are susceptible to wind

    mMaterialRustReceptivityBuffer[pointIndex] = 0.0f;

    mEphemeralTypeBuffer[pointIndex] = EphemeralType::Sparkle;
    mEphemeralStartTimeBuffer[pointIndex] = currentSimulationTime;
    mEphemeralMaxLifetimeBuffer[pointIndex] = std::chrono::duration_cast<std::chrono::duration<float>>(maxLifetime).count();
    mEphemeralStateBuffer[pointIndex] = EphemeralState::SparkleState(
        GameRandomEngine::GetInstance().Choose<TextureFrameIndex>(2));

    mConnectedComponentIdBuffer[pointIndex] = NoneConnectedComponentId;
    mPlaneIdBuffer[pointIndex] = planeId;
    mPlaneIdFloatBuffer[pointIndex] = static_cast<float>(planeId);
    mIsPlaneIdBufferEphemeralDirty = true;

    assert(false == mIsPinnedBuffer[pointIndex]);
}

void Points::Detach(
    ElementIndex pointElementIndex,
    vec2f const & velocity,
    DetachOptions detachOptions,
    float currentSimulationTime,
    GameParameters const & gameParameters)
{
    // Invoke detach handler
    if (!!mDetachHandler)
    {
        mDetachHandler(
            pointElementIndex,
            !!(detachOptions & Points::DetachOptions::GenerateDebris),
            !!(detachOptions & Points::DetachOptions::FireDestroyEvent),
            currentSimulationTime,
            gameParameters);
    }

    // Imprint velocity, unless the point is pinned
    if (!mIsPinnedBuffer[pointElementIndex])
    {
        mVelocityBuffer[pointElementIndex] = velocity;
    }
}

void Points::OnOrphaned(ElementIndex pointElementIndex)
{
    //
    // If we're in flames, make the flame tiny
    //

    if (mCombustionStateBuffer[pointElementIndex].State == CombustionState::StateType::Burning)
    {
        mCombustionStateBuffer[pointElementIndex].FlameDevelopment = GameRandomEngine::GetInstance()
            .GenerateRandomReal(0.1f, 0.14f);
    }
}

void Points::DestroyEphemeralParticle(
    ElementIndex pointElementIndex)
{
    // Invoke handler
    if (!!mEphemeralParticleDestroyHandler)
    {
        mEphemeralParticleDestroyHandler(pointElementIndex);
    }

    // Fire destroy event
    mGameEventHandler->OnDestroy(
        GetStructuralMaterial(pointElementIndex),
        mParentWorld.IsUnderwater(GetPosition(pointElementIndex)),
        1u);

    // Expire particle
    ExpireEphemeralParticle(pointElementIndex);
}

void Points::UpdateForGameParameters(GameParameters const & gameParameters)
{
    //
    // Check parameter changes
    //

    float const numMechanicalDynamicsIterations = gameParameters.NumMechanicalDynamicsIterations<float>();
    if (numMechanicalDynamicsIterations != mCurrentNumMechanicalDynamicsIterations)
    {
        // Recalc integration factor time coefficients
        for (ElementIndex i : *this)
        {
            mIntegrationFactorTimeCoefficientBuffer[i] = CalculateIntegrationFactorTimeCoefficient(numMechanicalDynamicsIterations);
        }

        // Remember the new values
        mCurrentNumMechanicalDynamicsIterations = numMechanicalDynamicsIterations;
    }

    float const cumulatedIntakenWaterThresholdForAirBubbles = gameParameters.CumulatedIntakenWaterThresholdForAirBubbles;
    if (cumulatedIntakenWaterThresholdForAirBubbles != mCurrentCumulatedIntakenWaterThresholdForAirBubbles)
    {
        // Randomize cumulated water intaken for each leaking point
        for (ElementIndex i : NonEphemeralPoints())
        {
            if (IsLeaking(i))
            {
                mCumulatedIntakenWater[i] = RandomizeCumulatedIntakenWater(cumulatedIntakenWaterThresholdForAirBubbles);
            }
        }

        // Remember the new values
        mCurrentCumulatedIntakenWaterThresholdForAirBubbles = cumulatedIntakenWaterThresholdForAirBubbles;
    }
}

void Points::UpdateCombustionLowFrequency(
    ElementIndex pointOffset,
    ElementIndex pointStride,
    float /*currentSimulationTime*/,
    float dt,
    GameParameters const & gameParameters)
{
    //
    // Take care of following:
    // - NotBurning->Developing transition (Ignition)
    // - Burning->Decay, Extinguishing transition
    //

    // Prepare candidates for ignition; we'll pick the top N ones
    // based on the ignition temperature delta
    mIgnitionCandidates.clear();

    // Decay rate - the higher this value, the slower fire consumes materials
    float const effectiveCombustionDecayRate = (90.0f / (gameParameters.CombustionSpeedAdjustment * dt));

    // No real reason not to do ephemeral points as well, other than they're
    // currently not expected to burn
    for (ElementIndex pointIndex = pointOffset; pointIndex < mShipPointCount; pointIndex += pointStride)
    {
        auto const currentState = mCombustionStateBuffer[pointIndex].State;
        if (currentState == CombustionState::StateType::NotBurning)
        {
            //
            // See if this point should start burning
            //

            float const effectiveIgnitionTemperature =
                mMaterialIgnitionTemperatureBuffer[pointIndex] * gameParameters.IgnitionTemperatureAdjustment;

            if (GetTemperature(pointIndex) >= effectiveIgnitionTemperature + GameParameters::IgnitionTemperatureHighWatermark
                && !mParentWorld.IsUnderwater(GetPosition(pointIndex))
                && GetWater(pointIndex) < GameParameters::SmotheringWaterLowWatermark
                && GetDecay(pointIndex) > GameParameters::SmotheringDecayHighWatermark)
            {
                // Store point as candidate
                mIgnitionCandidates.emplace_back(
                    pointIndex,
                    (GetTemperature(pointIndex) - effectiveIgnitionTemperature) / effectiveIgnitionTemperature);
            }
        }
        else if (currentState == CombustionState::StateType::Burning)
        {
            //
            // See if this point should start extinguishing...
            //

            // ...for water or sea: we do this check at high frequency

            // ...for temperature or decay:

            float const effectiveIgnitionTemperature =
                mMaterialIgnitionTemperatureBuffer[pointIndex] * gameParameters.IgnitionTemperatureAdjustment;

            if (GetTemperature(pointIndex) <= effectiveIgnitionTemperature + GameParameters::IgnitionTemperatureLowWatermark
                || GetDecay(pointIndex) < GameParameters::SmotheringDecayLowWatermark)
            {
                //
                // Transition to Extinguishing - by consumption
                //

                mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Extinguishing_Consumed;

                // Notify combustion end
                mGameEventHandler->OnPointCombustionEnd();
            }
            else
            {
                // Apply effects of burning

                //
                // 1. Decay - proportionally to mass
                //
                // Our goal:
                // - An iron hull mass (750Kg) decays completely (goes to 0.01)
                //   in 30 (simulated) seconds
                // - A smaller (larger) mass decays in shorter (longer) time,
                //   but a very small mass shouldn't burn in too short of a time
                //

                float const massMultiplier = pow(
                    mMaterialsBuffer[pointIndex].Structural->GetMass() / 750.0f,
                    0.15f); // Magic number: one tenth of the mass is 0.70 times the number of steps

                float const totalDecaySteps =
                    effectiveCombustionDecayRate
                    * massMultiplier;

                // decay(@ step i) = alpha^i
                // decay(@ step T) = min_decay => alpha^T = min_decay => alpha = min_decay^(1/T)
                float const decayAlpha = pow(0.01f, 1.0f / totalDecaySteps);

                // Decay point
                mDecayBuffer[pointIndex] *= decayAlpha;


                //
                // 2. Decay neighbors
                //

                for (auto const s : GetConnectedSprings(pointIndex).ConnectedSprings)
                {
                    mDecayBuffer[s.OtherEndpointIndex] *= decayAlpha;
                }
            }
        }
    }


    //
    // Now pick candidates for ignition
    //

    // Randomly choose the max number of points we want to ignite now
    size_t maxPoints = std::min(
        std::min(
            size_t(4) + GameRandomEngine::GetInstance().Choose(size_t(6)),
            mBurningPoints.size() < gameParameters.MaxBurningParticles
            ? gameParameters.MaxBurningParticles - mBurningPoints.size()
            : size_t(0)),
        mIgnitionCandidates.size());

    // Sort top N candidates by ignition temperature delta
    std::nth_element(
        mIgnitionCandidates.data(),
        mIgnitionCandidates.data() + maxPoints,
        mIgnitionCandidates.data() + mIgnitionCandidates.size(),
        [](auto const & t1, auto const & t2)
        {
            return std::get<1>(t1) > std::get<1>(t2);
        });

    // Ignite these points
    for (size_t i = 0; i < maxPoints; ++i)
    {
        assert(i < mIgnitionCandidates.size());

        auto const pointIndex = std::get<0>(mIgnitionCandidates[i]);

        //
        // Ignite!
        //

        mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Developing_1;

        // Initial development depends on how deep this particle is in its burning zone
        mCombustionStateBuffer[pointIndex].FlameDevelopment =
            0.1f + 0.5f * SmoothStep(0.0f, 2.0f, std::get<1>(mIgnitionCandidates[i]));

        // Assign a personality, we'll use it for noise
        mCombustionStateBuffer[pointIndex].Personality = GameRandomEngine::GetInstance().GenerateRandomNormalizedReal();

        // Max development: random and depending on number of springs connected to this point
        // (so chains have smaller flames)
        float const deltaSizeDueToConnectedSprings =
            static_cast<float>(mConnectedSpringsBuffer[pointIndex].ConnectedSprings.size())
            * 0.0625f; // 0.0625 -> 0.50 (@8)
        mCombustionStateBuffer[pointIndex].MaxFlameDevelopment = std::max(
            0.25f + deltaSizeDueToConnectedSprings + 0.5f * mCombustionStateBuffer[pointIndex].Personality, // 0.25 + dsdtcs -> 0.75 + dsdtcs
            mCombustionStateBuffer[pointIndex].FlameDevelopment);

        // Add point to vector of burning points, sorted by plane ID
        assert(mBurningPoints.cend() == std::find(mBurningPoints.cbegin(), mBurningPoints.cend(), pointIndex));
        mBurningPoints.insert(
            std::lower_bound( // Earlier than others at same plane ID, so it's drawn behind them
                mBurningPoints.cbegin(),
                mBurningPoints.cend(),
                pointIndex,
                [this](auto p1, auto p2)
                {
                    return this->mPlaneIdBuffer[p1] < mPlaneIdBuffer[p2];
                }),
            pointIndex);

        // Notify
        mGameEventHandler->OnPointCombustionBegin();
    }
}

void Points::UpdateCombustionHighFrequency(
    float /*currentSimulationTime*/,
    float dt,
    GameParameters const & gameParameters)
{
    //
    // For all burning points, take care of following:
    // - Developing points: development up
    // - Burning points: heat generation
    // - Extinguishing points: development down
    //

    // Heat generated by combustion in this step
    float const effectiveCombustionHeat =
        100.0f * 1000.0f // 100KJ
        * dt
        * gameParameters.CombustionHeatAdjustment;

    for (auto const pointIndex : mBurningPoints)
    {
        //
        // Check if this point should stop developing/burning or start extinguishing faster
        //

        auto const currentState = mCombustionStateBuffer[pointIndex].State;

        if ((currentState == CombustionState::StateType::Developing_1
            || currentState == CombustionState::StateType::Developing_2
            || currentState == CombustionState::StateType::Burning
            || currentState == CombustionState::StateType::Extinguishing_Consumed)
            && (mParentWorld.IsUnderwater(GetPosition(pointIndex))
                || GetWater(pointIndex) > GameParameters::SmotheringWaterHighWatermark))
        {
            //
            // Transition to Extinguishing - by smothering
            //

            SmotherCombustion(pointIndex);
        }
        else if (currentState == CombustionState::StateType::Burning)
        {
            //
            // Generate heat at:
            // - point itself: fix to constant temperature = ignition temperature + 10%
            // - neighbors: 100Kw * C, scaled by directional alpha
            //

            mTemperatureBuffer[pointIndex] =
                mMaterialIgnitionTemperatureBuffer[pointIndex]
                * gameParameters.IgnitionTemperatureAdjustment
                * 1.1f;

            for (auto const s : GetConnectedSprings(pointIndex).ConnectedSprings)
            {
                auto const otherEndpointIndex = s.OtherEndpointIndex;

                // Calculate direction coefficient so to prefer upwards direction:
                // 0.2 + 1.5*(1 - cos(theta)): 3.2 N, 0.2 S, 1.7 W and E
                vec2f const springDir = (GetPosition(otherEndpointIndex) - GetPosition(pointIndex)).normalise();
                float const dirAlpha =
                    (0.2f + 1.5f * (1.0f - springDir.dot(GameParameters::GravityNormalized)));
                // No normalization: if using normalization flame does not propagate along rope

                // Add heat to point
                mTemperatureBuffer[otherEndpointIndex] +=
                    effectiveCombustionHeat
                    * dirAlpha
                    / mMaterialHeatCapacityBuffer[otherEndpointIndex];
            }
        }


        //
        // Run development/extinguishing state machine now
        //

        switch (mCombustionStateBuffer[pointIndex].State)
        {
            case CombustionState::StateType::Developing_1:
            {
                //
                // Develop
                //
                // f(n-1) + 0.105*f(n-1): when starting from 0.1, after 25 steps (0.5s) it's 1.21
                // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:0.1,%22valid%22:true}]&expression=f(n-1)%20+%200.105*f(n-1)&target=0&endTarget=25&range=true
                //

                mCombustionStateBuffer[pointIndex].FlameDevelopment +=
                    0.105f * mCombustionStateBuffer[pointIndex].FlameDevelopment;

                // Check whether it's time to transition to the next development phase
                if (mCombustionStateBuffer[pointIndex].FlameDevelopment > mCombustionStateBuffer[pointIndex].MaxFlameDevelopment + 0.2f)
                {
                    mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Developing_2;
                }

                break;
            }

            case CombustionState::StateType::Developing_2:
            {
                //
                // Develop
                //
                // f(n-1) - 0.2*f(n-1): when starting from 0.2, after 10 steps (0.2s) it's below 0.02
                // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:0.2,%22valid%22:true}]&expression=f(n-1)%20-%200.2*f(n-1)&target=0&endTarget=25&range=true
                //

                // FlameDevelopment is now in the (MFD, MFD + 0.2) range
                auto extraFlameDevelopment = mCombustionStateBuffer[pointIndex].FlameDevelopment - mCombustionStateBuffer[pointIndex].MaxFlameDevelopment;
                extraFlameDevelopment =
                    extraFlameDevelopment
                    - 0.2f * extraFlameDevelopment;

                mCombustionStateBuffer[pointIndex].FlameDevelopment =
                    mCombustionStateBuffer[pointIndex].MaxFlameDevelopment + extraFlameDevelopment;

                // Check whether it's time to transition to burning
                if (extraFlameDevelopment < 0.02f)
                {
                    mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::Burning;
                    mCombustionStateBuffer[pointIndex].FlameDevelopment = mCombustionStateBuffer[pointIndex].MaxFlameDevelopment;
                }

                break;
            }

            case CombustionState::StateType::Extinguishing_Consumed:
            case CombustionState::StateType::Extinguishing_Smothered:
            {
                //
                // Un-develop
                //

                if (mCombustionStateBuffer[pointIndex].State == CombustionState::StateType::Extinguishing_Consumed)
                {
                    //
                    // f(n-1) - 0.0625*(1.01 - f(n-1)): when starting from 1, after 75 steps (1.5s) it's under 0.02
                    // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=f(n-1)%20-%200.0625*(1.01%20-%20f(n-1))&target=0&endTarget=80&range=true
                    //

                    mCombustionStateBuffer[pointIndex].FlameDevelopment -=
                        0.0625f
                        * (mCombustionStateBuffer[pointIndex].MaxFlameDevelopment - mCombustionStateBuffer[pointIndex].FlameDevelopment + 0.01f);
                }
                else
                {
                    //
                    // f(n-1) - 0.3*f(n-1): when starting from 1, after 10 steps (0.2s) it's under 0.02
                    // http://www.calcul.com/show/calculator/recursive?values=[{%22n%22:0,%22value%22:1,%22valid%22:true}]&expression=f(n-1)%20-%200.3*f(n-1)&target=0&endTarget=25&range=true
                    //

                    mCombustionStateBuffer[pointIndex].FlameDevelopment -=
                        0.3f * mCombustionStateBuffer[pointIndex].FlameDevelopment;
                }

                // Check whether we are done now
                if (mCombustionStateBuffer[pointIndex].FlameDevelopment <= 0.02f)
                {
                    //
                    // Stop burning
                    //

                    mCombustionStateBuffer[pointIndex].State = CombustionState::StateType::NotBurning;

                    // Remove point from set of burning points
                    auto pointIt = std::find(
                        mBurningPoints.cbegin(),
                        mBurningPoints.cend(),
                        pointIndex);
                    assert(pointIt != mBurningPoints.cend());
                    mBurningPoints.erase(pointIt);
                }

                break;
            }

            case CombustionState::StateType::Burning:
            case CombustionState::StateType::NotBurning:
            {
                // Nothing to do here
                break;
            }
        }
    }
}

void Points::ReorderBurningPointsForDepth()
{
    std::sort(
        mBurningPoints.begin(),
        mBurningPoints.end(),
        [this](auto p1, auto p2)
        {
            return this->mPlaneIdBuffer[p1] < this->mPlaneIdBuffer[p2];
        });
}

void Points::UpdateEphemeralParticles(
    float currentSimulationTime,
    GameParameters const & /*gameParameters*/)
{
    for (ElementIndex pointIndex : this->EphemeralPoints())
    {
        auto const ephemeralType = GetEphemeralType(pointIndex);
        if (EphemeralType::None != ephemeralType)
        {
            //
            // Run this particle's state machine
            //

            switch (ephemeralType)
            {
                case EphemeralType::AirBubble:
                {
                    // Do not advance air bubble if it's pinned
                    if (!mIsPinnedBuffer[pointIndex])
                    {
                        float const waterHeight = mParentWorld.GetOceanSurfaceHeightAt(GetPosition(pointIndex).x);
                        float const deltaY = waterHeight - GetPosition(pointIndex).y;

                        if (deltaY <= 0.0f)
                        {
                            // Got to the surface, expire
                            ExpireEphemeralParticle(pointIndex);
                        }
                        else
                        {
                            //
                            // Update progress based off y
                            //

                            mEphemeralStateBuffer[pointIndex].AirBubble.CurrentDeltaY = deltaY;

                            mEphemeralStateBuffer[pointIndex].AirBubble.Progress =
                                -1.0f
                                / (-1.0f + std::min(GetPosition(pointIndex).y, 0.0f));

                            //
                            // Update vortex
                            //

                            float const lifetime = currentSimulationTime - mEphemeralStartTimeBuffer[pointIndex];

                            float const vortexAmplitude =
                                mEphemeralStateBuffer[pointIndex].AirBubble.VortexAmplitude
                                + mEphemeralStateBuffer[pointIndex].AirBubble.Progress;

                            float vortexValue =
                                vortexAmplitude
                                * PrecalcLoFreqSin.GetNearestPeriodic(mEphemeralStateBuffer[pointIndex].AirBubble.NormalizedVortexAngularVelocity * lifetime);

                            // Update position
                            mPositionBuffer[pointIndex].x +=
                                vortexValue - mEphemeralStateBuffer[pointIndex].AirBubble.LastVortexValue;

                            mEphemeralStateBuffer[pointIndex].AirBubble.LastVortexValue = vortexValue;
                        }
                    }

                    break;
                }

                case EphemeralType::Debris:
                {
                    // Check if expired
                    auto const elapsedLifetime = currentSimulationTime - mEphemeralStartTimeBuffer[pointIndex];
                    if (elapsedLifetime >= mEphemeralMaxLifetimeBuffer[pointIndex])
                    {
                        ExpireEphemeralParticle(pointIndex);

                        // Remember that ephemeral points are now dirty
                        mAreEphemeralPointsDirty = true;
                    }
                    else
                    {
                        // Update alpha based off remaining time

                        float alpha = std::max(
                            1.0f - elapsedLifetime / mEphemeralMaxLifetimeBuffer[pointIndex],
                            0.0f);

                        mColorBuffer[pointIndex].w = alpha;
                    }

                    break;
                }

                case EphemeralType::Sparkle:
                {
                    // Check if expired
                    auto const elapsedLifetime = currentSimulationTime - mEphemeralStartTimeBuffer[pointIndex];
                    if (elapsedLifetime >= mEphemeralMaxLifetimeBuffer[pointIndex])
                    {
                        ExpireEphemeralParticle(pointIndex);
                    }
                    else
                    {
                        // Update progress based off remaining time

                        mEphemeralStateBuffer[pointIndex].Sparkle.Progress =
                            elapsedLifetime / mEphemeralMaxLifetimeBuffer[pointIndex];
                    }

                    break;
                }

                default:
                {
                    // Do nothing
                }
            }
        }
    }
}

void Points::Query(ElementIndex pointElementIndex) const
{
    LogMessage("PointIndex: ", pointElementIndex);
    LogMessage("P=", mPositionBuffer[pointElementIndex].toString(), " V=", mVelocityBuffer[pointElementIndex].toString());
    LogMessage("W=", mWaterBuffer[pointElementIndex], " T=", mTemperatureBuffer[pointElementIndex], " Decay=", mDecayBuffer[pointElementIndex]);
    LogMessage("Springs: ", mConnectedSpringsBuffer[pointElementIndex].ConnectedSprings.size(), " (factory: ", mFactoryConnectedSpringsBuffer[pointElementIndex].ConnectedSprings.size(), ")");
    LogMessage("PlaneID: ", mPlaneIdBuffer[pointElementIndex]);
    LogMessage("ConnectedComponentID: ", mConnectedComponentIdBuffer[pointElementIndex]);
}

void Points::UploadAttributes(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    // Upload immutable attributes, if we haven't uploaded them yet
    if (mIsTextureCoordinatesBufferDirty)
    {
        renderContext.UploadShipPointImmutableAttributes(
            shipId,
            mTextureCoordinatesBuffer.data());

        mIsTextureCoordinatesBufferDirty = false;
    }

    // Upload colors, if dirty
    if (mIsWholeColorBufferDirty)
    {
        renderContext.UploadShipPointColors(
            shipId,
            mColorBuffer.data(),
            0,
            mAllPointCount);

        mIsWholeColorBufferDirty = false;
    }
    else
    {
        // Only upload ephemeral particle portion
        renderContext.UploadShipPointColors(
            shipId,
            &(mColorBuffer.data()[mShipPointCount]),
            mShipPointCount,
            mEphemeralPointCount);
    }

    //
    // Upload mutable attributes
    //

    renderContext.UploadShipPointMutableAttributesStart(shipId);

    renderContext.UploadShipPointMutableAttributes(
        shipId,
        reinterpret_cast<void *>(mMutableAttributesBuffer.get()));

    if (mIsPlaneIdBufferNonEphemeralDirty)
    {
        if (mIsPlaneIdBufferEphemeralDirty)
        {
            // Whole

            renderContext.UploadShipPointMutableAttributesPlaneId(
                shipId,
                mPlaneIdFloatBuffer.data(),
                0,
                mAllPointCount);

            mIsPlaneIdBufferEphemeralDirty = false;
        }
        else
        {
            // Just non-ephemeral portion

            renderContext.UploadShipPointMutableAttributesPlaneId(
                shipId,
                mPlaneIdFloatBuffer.data(),
                0,
                mShipPointCount);
        }

        mIsPlaneIdBufferNonEphemeralDirty = false;
    }
    else if (mIsPlaneIdBufferEphemeralDirty)
    {
        // Just ephemeral portion

        renderContext.UploadShipPointMutableAttributesPlaneId(
            shipId,
            &(mPlaneIdFloatBuffer.data()[mShipPointCount]),
            mShipPointCount,
            mEphemeralPointCount);

        mIsPlaneIdBufferEphemeralDirty = false;
    }

    if (mIsDecayBufferDirty)
    {
        renderContext.UploadShipPointMutableAttributesDecay(
            shipId,
            mDecayBuffer.data(),
            0,
            mAllPointCount);

        mIsDecayBufferDirty = false;
    }

    if (renderContext.GetDrawHeatOverlay())
    {
        renderContext.UploadShipPointTemperature(
            shipId,
            mTemperatureBuffer.data(),
            0,
            mAllPointCount);
    }

    renderContext.UploadShipPointMutableAttributesEnd(shipId);
}

void Points::UploadNonEphemeralPointElements(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    bool doUploadAllPoints = (DebugShipRenderMode::Points == renderContext.GetDebugShipRenderMode());

    for (ElementIndex pointIndex : NonEphemeralPoints())
    {
        if (doUploadAllPoints
            || mConnectedSpringsBuffer[pointIndex].ConnectedSprings.empty()) // orphaned
        {
            renderContext.UploadShipElementPoint(
                shipId,
                pointIndex);
        }
    }
}

void Points::UploadFlames(
    ShipId shipId,
    float windSpeedMagnitude,
    Render::RenderContext & renderContext) const
{
    if (renderContext.GetShipFlameRenderMode() != ShipFlameRenderMode::NoDraw)
    {
        renderContext.UploadShipFlamesStart(shipId, mBurningPoints.size(), windSpeedMagnitude);

        // Upload flames, in order of plane ID
        for (auto const pointIndex : mBurningPoints)
        {
            renderContext.UploadShipFlame(
                shipId,
                GetPlaneId(pointIndex),
                GetPosition(pointIndex),
                mCombustionStateBuffer[pointIndex].FlameDevelopment,
                mCombustionStateBuffer[pointIndex].Personality,
                // IsOnChain: we use # of triangles as a heuristic for the point being on a chain,
                // and we use the *factory* ones to avoid sudden depth jumps when triangles are destroyed by fire
                mFactoryConnectedTrianglesBuffer[pointIndex].ConnectedTriangles.empty());
        }

        renderContext.UploadShipFlamesEnd(shipId);
    }
}

void Points::UploadVectors(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    static constexpr vec4f VectorColor(0.5f, 0.1f, 0.f, 1.0f);

    if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderMode::PointVelocity)
    {
        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mVelocityBuffer.data(),
            0.25f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderMode::PointForce)
    {
        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mForceRenderBuffer.data(),
            0.0005f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderMode::PointWaterVelocity)
    {
        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mWaterVelocityBuffer.data(),
            1.0f,
            VectorColor);
    }
    else if (renderContext.GetVectorFieldRenderMode() == VectorFieldRenderMode::PointWaterMomentum)
    {
        renderContext.UploadShipVectors(
            shipId,
            mElementCount,
            mPositionBuffer.data(),
            mPlaneIdFloatBuffer.data(),
            mWaterMomentumBuffer.data(),
            0.4f,
            VectorColor);
    }
}

void Points::UploadEphemeralParticles(
    ShipId shipId,
    Render::RenderContext & renderContext) const
{
    //
    // Upload points and/or textures
    //

    if (mAreEphemeralPointsDirty)
    {
        renderContext.UploadShipElementEphemeralPointsStart(shipId);
    }

    for (ElementIndex pointIndex : this->EphemeralPoints())
    {
        switch (GetEphemeralType(pointIndex))
        {
            case EphemeralType::AirBubble:
            {
                renderContext.UploadShipAirBubble(
                    shipId,
                    GetPlaneId(pointIndex),
                    TextureFrameId(TextureGroupType::AirBubble, mEphemeralStateBuffer[pointIndex].AirBubble.FrameIndex),
                    GetPosition(pointIndex),
                    mEphemeralStateBuffer[pointIndex].AirBubble.InitialSize, // Scale
                    std::min(1.0f, mEphemeralStateBuffer[pointIndex].AirBubble.CurrentDeltaY / 4.0f)); // Alpha

                break;
            }

            case EphemeralType::Debris:
            {
                // Don't upload point unless there's been a change
                if (mAreEphemeralPointsDirty)
                {
                    renderContext.UploadShipElementEphemeralPoint(
                        shipId,
                        pointIndex);
                }

                break;
            }

            case EphemeralType::Sparkle:
            {
                renderContext.UploadShipGenericTextureRenderSpecification(
                    shipId,
                    GetPlaneId(pointIndex),
                    TextureFrameId(TextureGroupType::SawSparkle, mEphemeralStateBuffer[pointIndex].Sparkle.FrameIndex),
                    GetPosition(pointIndex),
                    1.0f,
                    4.0f * mEphemeralStateBuffer[pointIndex].Sparkle.Progress,
                    1.0f - mEphemeralStateBuffer[pointIndex].Sparkle.Progress);

                break;
            }

            case EphemeralType::None:
            default:
            {
                // Ignore
                break;
            }
        }
    }

    if (mAreEphemeralPointsDirty)
    {
        renderContext.UploadShipElementEphemeralPointsEnd(shipId);

        mAreEphemeralPointsDirty = false;
    }
}

void Points::AugmentMaterialMass(
    ElementIndex pointElementIndex,
    float offset,
    Springs & springs)
{
    assert(pointElementIndex < mElementCount);

    mAugmentedMaterialMassBuffer[pointElementIndex] =
        GetStructuralMaterial(pointElementIndex).GetMass()
        + offset;

    // Notify all connected springs
    for (auto connectedSpring : mConnectedSpringsBuffer[pointElementIndex].ConnectedSprings)
    {
        springs.UpdateForMass(connectedSpring.SpringIndex, *this);
    }
}

void Points::UpdateMasses(GameParameters const & gameParameters)
{
    //
    // Update:
    //  - CurrentMass: augmented material mass + point's water mass
    //  - Integration factor: integration factor time coefficient / total mass
    //

    float const densityAdjustedWaterMass = GameParameters::WaterMass * gameParameters.WaterDensityAdjustment;

    for (ElementIndex i : *this)
    {
        float const mass =
            mAugmentedMaterialMassBuffer[i]
            + std::min(GetWater(i), GetMaterialWaterVolumeFill(i)) * densityAdjustedWaterMass;

        assert(mass > 0.0f);

        mMassBuffer[i] = mass;

        mIntegrationFactorBuffer[i] = vec2f(
            mIntegrationFactorTimeCoefficientBuffer[i] / mass,
            mIntegrationFactorTimeCoefficientBuffer[i] / mass);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////

ElementIndex Points::FindFreeEphemeralParticle(
    float currentSimulationTime,
    bool force)
{
    //
    // Search for the firt free ephemeral particle; if a free one is not found, reuse the
    // oldest particle
    //

    ElementIndex oldestParticle = NoneElementIndex;
    float oldestParticleLifetime = 0.0f;

    assert(mFreeEphemeralParticleSearchStartIndex >= mShipPointCount
        && mFreeEphemeralParticleSearchStartIndex < mAllPointCount);

    for (ElementIndex p = mFreeEphemeralParticleSearchStartIndex; ; /*incremented in loop*/)
    {
        if (EphemeralType::None == GetEphemeralType(p))
        {
            // Found!

            // Remember to start after this one next time
            mFreeEphemeralParticleSearchStartIndex = p + 1;
            if (mFreeEphemeralParticleSearchStartIndex >= mAllPointCount)
                mFreeEphemeralParticleSearchStartIndex = mShipPointCount;

            return p;
        }

        // Check whether it's the oldest
        auto lifetime = currentSimulationTime - mEphemeralStartTimeBuffer[p];
        if (lifetime >= oldestParticleLifetime)
        {
            oldestParticle = p;
            oldestParticleLifetime = lifetime;
        }

        // Advance
        ++p;
        if (p >= mAllPointCount)
            p = mShipPointCount;

        if (p == mFreeEphemeralParticleSearchStartIndex)
        {
            // Went around
            break;
        }
    }

    //
    // No luck
    //

    if (!force)
        return NoneElementIndex;


    //
    // Steal the oldest
    //

    assert(NoneElementIndex != oldestParticle);

    // Remember to start after this one next time
    mFreeEphemeralParticleSearchStartIndex = oldestParticle + 1;
    if (mFreeEphemeralParticleSearchStartIndex >= mAllPointCount)
        mFreeEphemeralParticleSearchStartIndex = mShipPointCount;

    return oldestParticle;
}

}