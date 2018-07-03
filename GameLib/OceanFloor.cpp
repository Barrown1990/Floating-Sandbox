﻿/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-04-14
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "Physics.h"

namespace Physics {

OceanFloor::OceanFloor()
    : mSamples(new float[SamplesCount + 1])
    , mCurrentSeaDepth(std::numeric_limits<float>::lowest())
{
}

void OceanFloor::Update(GameParameters const & gameParameters)
{
    if (gameParameters.SeaDepth != mCurrentSeaDepth)
    {
        // Fill-in an extra sample, so we can avoid having to wrap around
        float x = 0;
        for (int64_t i = 0; i < SamplesCount + 1; i++, x += Dx)
        {
            float const c1 = sinf(x * Frequency1) * 10.f;
            float const c2 = sinf(x * Frequency2) * 6.f;
            float const c3 = sinf(x * Frequency3) * 45.f;

            mSamples[i] = (c1 + c2 - c3) - gameParameters.SeaDepth;
        }

        // Remember current sea depth
        mCurrentSeaDepth = gameParameters.SeaDepth;
    }
}

}