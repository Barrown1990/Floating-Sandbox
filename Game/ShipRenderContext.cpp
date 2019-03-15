/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-03-22
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ShipRenderContext.h"

#include "GameParameters.h"

#include <GameCore/GameException.h>
#include <GameCore/GameMath.h>
#include <GameCore/Log.h>

namespace Render {

ShipRenderContext::ShipRenderContext(
    ShipId shipId,
    size_t shipCount,
    size_t pointCount,
    RgbaImageData shipTexture,
    ShipDefinition::TextureOriginType /*textureOrigin*/,
    ShaderManager<ShaderManagerTraits> & shaderManager,
    GameOpenGLTexture & genericTextureAtlasOpenGLHandle,
    TextureAtlasMetadata const & genericTextureAtlasMetadata,
    RenderStatistics & renderStatistics,
    ViewModel const & viewModel,
    float ambientLightIntensity,
    float waterContrast,
    float waterLevelOfDetail,
    ShipRenderMode shipRenderMode,
    DebugShipRenderMode debugShipRenderMode,
    VectorFieldRenderMode vectorFieldRenderMode,
    bool showStressedSprings)
    : mShipId(shipId)
    , mShipCount(shipCount)
    , mPointCount(pointCount)
    , mMaxMaxPlaneId(0)
    // Buffers
    , mPointPositionVBO()
    , mPointLightVBO()
    , mPointWaterVBO()
    , mPointColorVBO()
    , mPointPlaneIdVBO()
    , mPointTextureCoordinatesVBO()
    //
    , mPointElementBuffer()
    , mPointElementVBO()
    , mSpringElementBuffer()
    , mSpringElementVBO()
    , mRopeElementBuffer()
    , mRopeElementVBO()
    , mTriangleElementBuffer()
    , mTriangleElementVBO()
    , mStressedSpringElementBuffer()
    , mStressedSpringElementVBO()
    , mEphemeralPointElementBuffer()
    , mEphemeralPointElementVBO()
    //
    , mGenericTexturePlaneVertexBuffers()
    , mGenericTextureMaxPlaneVertexBufferSize(0)
    , mGenericTextureVBO()
    , mGenericTextureVBOAllocatedSize()
    //
    , mVectorArrowVertexBuffer()
    , mVectorArrowVBO()
    , mVectorArrowColor()
    // VAOs
    , mShipVAO()
    , mGenericTextureVAO()
    , mVectorArrowVAO()
    // Textures
    , mShipTextureOpenGLHandle()
    , mStressedSpringTextureOpenGLHandle()
    , mGenericTextureAtlasOpenGLHandle(genericTextureAtlasOpenGLHandle)
    , mGenericTextureAtlasMetadata(genericTextureAtlasMetadata)
    // Managers
    , mShaderManager(shaderManager)
    // Parameters
    , mViewModel(viewModel)
    , mAmbientLightIntensity(ambientLightIntensity)
    , mWaterContrast(waterContrast)
    , mWaterLevelOfDetail(waterLevelOfDetail)
    , mShipRenderMode(shipRenderMode)
    , mDebugShipRenderMode(debugShipRenderMode)
    , mVectorFieldRenderMode(vectorFieldRenderMode)
    , mShowStressedSprings(showStressedSprings)
    // Statistics
    , mRenderStatistics(renderStatistics)
{
    GLuint tmpGLuint;

    // Clear errors
    glGetError();


    //
    // Initialize buffers
    //

    GLuint vbos[14];
    glGenBuffers(14, vbos);
    CheckOpenGLError();

    mPointPositionVBO = vbos[0];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointPositionVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec2f), nullptr, GL_STREAM_DRAW);

    mPointLightVBO = vbos[1];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointLightVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(float), nullptr, GL_STREAM_DRAW);

    mPointWaterVBO = vbos[2];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointWaterVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(float), nullptr, GL_STREAM_DRAW);

    mPointColorVBO = vbos[3];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec4f), nullptr, GL_STATIC_DRAW);

    mPointPlaneIdVBO = vbos[4];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointPlaneIdVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(PlaneId), nullptr, GL_STATIC_DRAW);

    mPointTextureCoordinatesVBO = vbos[5];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointTextureCoordinatesVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec2f), nullptr, GL_STATIC_DRAW);

    mPointElementVBO = vbos[6];
    mPointElementBuffer.reserve(pointCount);

    mSpringElementVBO = vbos[7];
    mSpringElementBuffer.reserve(pointCount * GameParameters::MaxSpringsPerPoint);

    mRopeElementVBO = vbos[8];
    mRopeElementBuffer.reserve(pointCount); // Arbitrary

    mTriangleElementVBO = vbos[9];
    mTriangleElementBuffer.reserve(pointCount * GameParameters::MaxTrianglesPerPoint);

    mStressedSpringElementVBO = vbos[10];
    mStressedSpringElementBuffer.reserve(1000); // Arbitrary

    mEphemeralPointElementVBO = vbos[11];
    mEphemeralPointElementBuffer.reserve(GameParameters::MaxEphemeralParticles);

    mGenericTextureVBO = vbos[12];

    mVectorArrowVBO = vbos[13];

    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Initialize Ship VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mShipVAO = tmpGLuint;

        glBindVertexArray(*mShipVAO);
        CheckOpenGLError();

        //
        // Describe vertex attributes
        //

        glBindBuffer(GL_ARRAY_BUFFER, *mPointPositionVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointPosition));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointPosition), 2, GL_FLOAT, GL_FALSE, sizeof(vec2f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointLightVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointLight));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointLight), 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointWaterVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointWater));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointWater), 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointColor));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointColor), 4, GL_FLOAT, GL_FALSE, sizeof(vec4f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointPlaneIdVBO);
        static_assert(sizeof(PlaneId) == sizeof(uint32_t)); // GL_UNSIGNED_INT
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointPlaneId));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointPlaneId), 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(PlaneId), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointTextureCoordinatesVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointTextureCoordinates));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointTextureCoordinates), 2, GL_FLOAT, GL_FALSE, sizeof(vec2f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        //
        // Associate element VBO
        //

        // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
        // in the VAO
        ////glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mPointElementVBO);
        ////CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize GenericTexture VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mGenericTextureVAO = tmpGLuint;

        glBindVertexArray(*mGenericTextureVAO);
        CheckOpenGLError();

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mGenericTextureVBO);
        static_assert(sizeof(GenericTextureVertex) == (4 + 4 + 3) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericTexture1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericTexture1), 4, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)0);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericTexture2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericTexture2), 4, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)((4) * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericTexture3));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericTexture3), 3, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)((4 + 4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize VectorArrow VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mVectorArrowVAO = tmpGLuint;

        glBindVertexArray(*mVectorArrowVAO);
        CheckOpenGLError();

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mVectorArrowVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::VectorArrow));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::VectorArrow), 3, GL_FLOAT, GL_FALSE, sizeof(vec3f), (void*)(0));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Ship texture
    //

    glGenTextures(1, &tmpGLuint);
    mShipTextureOpenGLHandle = tmpGLuint;

    // Bind texture
    mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
    glBindTexture(GL_TEXTURE_2D, *mShipTextureOpenGLHandle);
    CheckOpenGLError();

    // Upload texture
    GameOpenGL::UploadMipmappedTexture(std::move(shipTexture));

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CheckOpenGLError();

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Set texture parameter
    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetTextureParameters<ProgramType::ShipSpringsTexture>();
    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetTextureParameters<ProgramType::ShipTrianglesTexture>();

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);


    //
    // Initialize StressedSpring texture
    //

    glGenTextures(1, &tmpGLuint);
    mStressedSpringTextureOpenGLHandle = tmpGLuint;

    // Bind texture
    mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
    glBindTexture(GL_TEXTURE_2D, *mStressedSpringTextureOpenGLHandle);
    CheckOpenGLError();

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CheckOpenGLError();

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Make texture data
    unsigned char buf[] = {
        239, 16, 39, 255,       255, 253, 181,  255,    239, 16, 39, 255,
        255, 253, 181, 255,     239, 16, 39, 255,       255, 253, 181,  255,
        239, 16, 39, 255,       255, 253, 181,  255,    239, 16, 39, 255
    };

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    CheckOpenGLError();

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);


    //
    // Set parameters to initial values
    //

    OnViewModelUpdated();

    OnAmbientLightIntensityUpdated();
    OnWaterContrastUpdated();
    OnWaterLevelOfDetailUpdated();
}

ShipRenderContext::~ShipRenderContext()
{
}

void ShipRenderContext::UpdateOrthoMatrices()
{
    //
    // Each plane Z segment is divided into 7 layers, one for each type of rendering we do for a ship:
    //      - 0: Ropes (always behind)
    //      - 1: Springs
    //      - 2: Triangles
    //          - Triangles are always drawn temporally before ropes and springs though, to avoid anti-aliasing issues
    //      - 3: Stressed springs
    //      - 4: Points
    //      - 5: Generic textures
    //      - 6: Vectors
    //

    constexpr float ShipRegionZStart = 1.0f;
    constexpr float ShipRegionZWidth = -2.0f;

    constexpr int NLayers = 7;

    ViewModel::ProjectionMatrix shipOrthoMatrix;

    //
    // Layer 0: Ropes
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        0,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);


    //
    // Layer 1: Springs
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        1,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 2: Triangles
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        2,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 3: Stressed Springs
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        3,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipStressedSprings>();
    mShaderManager.SetProgramParameter<ProgramType::ShipStressedSprings, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 4: Points
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        4,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 5: Generic Textures
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        5,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipGenericTextures>();
    mShaderManager.SetProgramParameter<ProgramType::ShipGenericTextures, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 6: Vectors
    //

    mViewModel.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        6,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipVectors>();
    mShaderManager.SetProgramParameter<ProgramType::ShipVectors, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);
}

void ShipRenderContext::OnAmbientLightIntensityUpdated()
{
    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipGenericTextures>();
    mShaderManager.SetProgramParameter<ProgramType::ShipGenericTextures, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipVectors>();
    mShaderManager.SetProgramParameter<ProgramType::ShipVectors, ProgramParameterType::AmbientLightIntensity>(
        mAmbientLightIntensity);
}

void ShipRenderContext::OnWaterContrastUpdated()
{
    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::WaterContrast>(
        mWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::WaterContrast>(
        mWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::WaterContrast>(
        mWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::WaterContrast>(
        mWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::WaterContrast>(
        mWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::WaterContrast>(
        mWaterContrast);
}

void ShipRenderContext::OnWaterLevelOfDetailUpdated()
{
    // Transform: 0->1 == 2.0->0.01
    float waterLevelThreshold = 2.0f + mWaterLevelOfDetail * (-2.0f + 0.01f);

    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);
}

//////////////////////////////////////////////////////////////////////////////////

void ShipRenderContext::RenderStart(PlaneId maxMaxPlaneId)
{
    //
    // Reset generic textures
    //

    mGenericTexturePlaneVertexBuffers.clear();
    mGenericTexturePlaneVertexBuffers.resize(maxMaxPlaneId + 1);
    mGenericTextureMaxPlaneVertexBufferSize = 0;


    //
    // Check if the max ever plane ID has changed
    //

    if (maxMaxPlaneId != mMaxMaxPlaneId)
    {
        // Update value
        mMaxMaxPlaneId = maxMaxPlaneId;

        // Recalculate view model parameters
        OnViewModelUpdated();
    }
}

void ShipRenderContext::UploadPointImmutableAttributes(vec2f const * textureCoordinates)
{
    // Upload texture coordinates
    glBindBuffer(GL_ARRAY_BUFFER, *mPointTextureCoordinatesVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(vec2f), textureCoordinates);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadPointMutableAttributes(
    vec2f const * position,
    float const * light,
    float const * water)
{
    // Upload positions
    glBindBuffer(GL_ARRAY_BUFFER, *mPointPositionVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(vec2f), position);
    CheckOpenGLError();

    // Upload light
    glBindBuffer(GL_ARRAY_BUFFER, *mPointLightVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(float), light);
    CheckOpenGLError();

    // Upload water
    glBindBuffer(GL_ARRAY_BUFFER, *mPointWaterVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(float), water);
    CheckOpenGLError();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadPointColors(
    vec4f const * color,
    size_t startDst,
    size_t count)
{
    assert(startDst + count <= mPointCount);

    // Uplaod color range
    glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);
    glBufferSubData(GL_ARRAY_BUFFER, startDst * sizeof(vec4f), count * sizeof(vec4f), color);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadPointPlaneIds(
    PlaneId const * planeId,
    size_t startDst,
    size_t count)
{
    assert(startDst + count <= mPointCount);

    // Upload plane IDs
    glBindBuffer(GL_ARRAY_BUFFER, *mPointPlaneIdVBO);
    glBufferSubData(GL_ARRAY_BUFFER, startDst * sizeof(PlaneId), count * sizeof(PlaneId), planeId);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadElementTrianglesStart(size_t trianglesCount)
{
    // No need to clear, we'll repopulate everything
    mTriangleElementBuffer.resize(trianglesCount);
}

void ShipRenderContext::UploadElementTrianglesEnd()
{
    // Upload
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mTriangleElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mTriangleElementBuffer.size() * sizeof(TriangleElement), mTriangleElementBuffer.data(), GL_STATIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadElementsStart()
{
    // Empty all buffers, as they will be completely re-populated soon
    // (with a yet-unknown quantity of elements)
    mPointElementBuffer.clear();
    mSpringElementBuffer.clear();
    mRopeElementBuffer.clear();
    mStressedSpringElementBuffer.clear();
}

void ShipRenderContext::UploadElementsEnd()
{
    //
    // Upload all elements, except for stressed springs
    //

    // Points
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mPointElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mPointElementBuffer.size() * sizeof(PointElement), mPointElementBuffer.data(), GL_STATIC_DRAW);
    CheckOpenGLError();

    // Springs
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mSpringElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSpringElementBuffer.size() * sizeof(LineElement), mSpringElementBuffer.data(), GL_STATIC_DRAW);
    CheckOpenGLError();

    // Ropes
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mRopeElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mRopeElementBuffer.size() * sizeof(LineElement), mRopeElementBuffer.data(), GL_STATIC_DRAW);
    CheckOpenGLError();

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadElementStressedSpringsStart()
{
    // Empty buffer
    mStressedSpringElementBuffer.clear();
}

void ShipRenderContext::UploadElementStressedSpringsEnd()
{
    //
    // Upload stressed spring elements
    //

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mStressedSpringElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mStressedSpringElementBuffer.size() * sizeof(LineElement), mStressedSpringElementBuffer.data(), GL_DYNAMIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadElementEphemeralPointsStart()
{
    mEphemeralPointElementBuffer.clear();
}

void ShipRenderContext::UploadElementEphemeralPointsEnd()
{
    //
    // Upload ephemeral point elements
    //

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mEphemeralPointElementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mEphemeralPointElementBuffer.size() * sizeof(PointElement), mEphemeralPointElementBuffer.data(), GL_STATIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadVectors(
    size_t count,
    vec2f const * position,
    PlaneId const * planeId,
    vec2f const * vector,
    float lengthAdjustment,
    vec4f const & color)
{
    static float const CosAlphaLeftRight = cos(-2.f * Pi<float> / 8.f);
    static float const SinAlphaLeft = sin(-2.f * Pi<float> / 8.f);
    static float const SinAlphaRight = -SinAlphaLeft;

    static vec2f const XMatrixLeft = vec2f(CosAlphaLeftRight, SinAlphaLeft);
    static vec2f const YMatrixLeft = vec2f(-SinAlphaLeft, CosAlphaLeftRight);
    static vec2f const XMatrixRight = vec2f(CosAlphaLeftRight, SinAlphaRight);
    static vec2f const YMatrixRight = vec2f(-SinAlphaRight, CosAlphaLeftRight);

    //
    // Create buffer with endpoint positions of each segment of each arrow
    //

    mVectorArrowVertexBuffer.clear();
    mVectorArrowVertexBuffer.reserve(count * 3 * 2);

    for (size_t i = 0; i < count; ++i)
    {
        float planeIdf = static_cast<float>(planeId[i]);

        // Stem
        vec2f stemEndpoint = position[i] + vector[i] * lengthAdjustment;
        mVectorArrowVertexBuffer.emplace_back(position[i], planeIdf);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeIdf);

        // Left
        vec2f leftDir = vec2f(-vector[i].dot(XMatrixLeft), -vector[i].dot(YMatrixLeft)).normalise();
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeIdf);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint + leftDir * 0.2f, planeIdf);

        // Right
        vec2f rightDir = vec2f(-vector[i].dot(XMatrixRight), -vector[i].dot(YMatrixRight)).normalise();
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeIdf);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint + rightDir * 0.2f, planeIdf);
    }


    //
    // Upload buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mVectorArrowVBO);
    glBufferData(GL_ARRAY_BUFFER, mVectorArrowVertexBuffer.size() * sizeof(vec3f), mVectorArrowVertexBuffer.data(), GL_DYNAMIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Manage color
    //

    if (mVectorArrowColor != color)
    {
        mShaderManager.ActivateProgram<ProgramType::ShipVectors>();
        mShaderManager.SetProgramParameter<ProgramType::ShipVectors, ProgramParameterType::MatteColor>(
            color.x,
            color.y,
            color.z,
            color.w);

        mVectorArrowColor = color;
    }
}

void ShipRenderContext::RenderEnd()
{
    //
    // Draw ship elements
    //

    glBindVertexArray(*mShipVAO);

    {
        //
        // Bind ship texture
        //

        assert(!!mShipTextureOpenGLHandle);

        mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
        glBindTexture(GL_TEXTURE_2D, *mShipTextureOpenGLHandle);



        // TODO: this will go with orphaned points rearc, will become a single Render invoked right after triangles
        //
        // Draw points
        //

        if (mDebugShipRenderMode == DebugShipRenderMode::Points)
        {
            mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();

            glPointSize(0.2f * 2.0f * mViewModel.GetCanvasToVisibleWorldHeightRatio());

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mPointElementVBO);

            glDrawElements(GL_POINTS, static_cast<GLsizei>(1 * mPointElementBuffer.size()), GL_UNSIGNED_INT, 0);
        }



        //
        // Draw triangles
        //
        // Best to draw triangles (temporally) before springs and ropes, otherwise
        // the latter, which use anti-aliasing, would end up being contoured with background
        // when drawn Z-ally over triangles
        //
        // Also, edge springs might just contain transparent pixels (when textured), which
        // would result in the same artifact
        //

        if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe
            || (mDebugShipRenderMode == DebugShipRenderMode::None
                && (mShipRenderMode == ShipRenderMode::Structure || mShipRenderMode == ShipRenderMode::Texture)))
        {
            if (mShipRenderMode == ShipRenderMode::Texture)
            {
                // Use texture program
                mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
            }
            else
            {
                // Use color program
                mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
            }

            if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
                glLineWidth(0.1f);

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mTriangleElementVBO);

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(3 * mTriangleElementBuffer.size()), GL_UNSIGNED_INT, 0);

            // Update stats
            mRenderStatistics.LastRenderedShipTriangles += mTriangleElementBuffer.size();
        }



        //
        // Set line width, for ropes and springs
        //

        glLineWidth(0.1f * 2.0f * mViewModel.GetCanvasToVisibleWorldHeightRatio());



        //
        // Draw ropes, unless it's a debug mode
        //
        // Note: in springs or edge springs debug mode, all ropes are uploaded as springs
        //

        if (mDebugShipRenderMode == DebugShipRenderMode::None)
        {
            mShaderManager.ActivateProgram<ProgramType::ShipRopes>();

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mRopeElementVBO);

            glDrawElements(GL_LINES, static_cast<GLsizei>(2 * mRopeElementBuffer.size()), GL_UNSIGNED_INT, 0);

            // Update stats
            mRenderStatistics.LastRenderedShipRopes += mRopeElementBuffer.size();
        }


        //
        // Draw springs
        //
        // We draw springs when:
        // - DebugRenderMode is springs|edgeSprings, in which case we use colors - so to show
        //   structural springs -, or
        // - RenderMode is structure (so to draw 1D chains), in which case we use colors, or
        // - RenderMode is texture (so to draw 1D chains), in which case we use texture iff it is present
        //
        // Note: when DebugRenderMode is springs|edgeSprings, ropes would all be here.
        //

        if (mDebugShipRenderMode == DebugShipRenderMode::Springs
            || mDebugShipRenderMode == DebugShipRenderMode::EdgeSprings
            || (mDebugShipRenderMode == DebugShipRenderMode::None
                && (mShipRenderMode == ShipRenderMode::Structure || mShipRenderMode == ShipRenderMode::Texture)))
        {
            if (mDebugShipRenderMode == DebugShipRenderMode::None && mShipRenderMode == ShipRenderMode::Texture)
            {
                // Use texture program
                mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
            }
            else
            {
                // Use color program
                mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
            }

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mSpringElementVBO);

            glDrawElements(GL_LINES, static_cast<GLsizei>(2 * mSpringElementBuffer.size()), GL_UNSIGNED_INT, 0);

            // Update stats
            mRenderStatistics.LastRenderedShipSprings += mSpringElementBuffer.size();
        }



        //
        // Draw stressed springs
        //

        if (mShowStressedSprings
            && !mStressedSpringElementBuffer.empty())
        {
            mShaderManager.ActivateProgram<ProgramType::ShipStressedSprings>();

            // Bind stressed spring texture
            mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
            glBindTexture(GL_TEXTURE_2D, *mStressedSpringTextureOpenGLHandle);
            CheckOpenGLError();

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mStressedSpringElementVBO);

            glDrawElements(GL_LINES, static_cast<GLsizei>(2 * mStressedSpringElementBuffer.size()), GL_UNSIGNED_INT, 0);
        }



        //
        // Draw ephemeral points
        //

        if (!mEphemeralPointElementBuffer.empty())
        {
            mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();

            glPointSize(0.3f * mViewModel.GetCanvasToVisibleWorldHeightRatio());

            // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
            // in the VAO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mEphemeralPointElementVBO);

            glDrawElements(GL_POINTS, static_cast<GLsizei>(mEphemeralPointElementBuffer.size()), GL_UNSIGNED_INT, 0);

            // Update stats
            mRenderStatistics.LastRenderedShipEphemeralPoints += mEphemeralPointElementBuffer.size();
        }

        // We are done with the ship VAO
        glBindVertexArray(0);
    }



    //
    // Render generic textures
    //

    RenderGenericTextures();



    //
    // Render vectors, if we're asked to
    //

    if (mVectorFieldRenderMode != VectorFieldRenderMode::None)
    {
        RenderVectorArrows();
    }



    //
    // Update stats
    //

    mRenderStatistics.LastRenderedShipPlanes += mMaxMaxPlaneId + 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////

void ShipRenderContext::RenderGenericTextures()
{
    if (mGenericTextureMaxPlaneVertexBufferSize > 0)
    {
        glBindVertexArray(*mGenericTextureVAO);

        mShaderManager.ActivateProgram<ProgramType::ShipGenericTextures>();

        if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
            glLineWidth(0.1f);

        // Bind VBO once and for all
        glBindBuffer(GL_ARRAY_BUFFER, *mGenericTextureVBO);

        // (Re-)Allocate vertex buffer, if needed
        if (mGenericTextureVBOAllocatedSize != mGenericTextureMaxPlaneVertexBufferSize)
        {
            glBufferData(GL_ARRAY_BUFFER, mGenericTextureMaxPlaneVertexBufferSize * sizeof(GenericTextureVertex), nullptr, GL_DYNAMIC_DRAW);
            CheckOpenGLError();

            mGenericTextureVBOAllocatedSize = mGenericTextureMaxPlaneVertexBufferSize;
        }

        //
        // Draw, separately for each plane
        //

        for (auto const & plane : mGenericTexturePlaneVertexBuffers)
        {
            if (!plane.vertexBuffer.empty())
            {
                //
                // Upload vertex buffer
                //

                assert(plane.vertexBuffer.size() <= mGenericTextureVBOAllocatedSize);

                glBufferSubData(
                    GL_ARRAY_BUFFER,
                    0,
                    plane.vertexBuffer.size() * sizeof(GenericTextureVertex),
                    plane.vertexBuffer.data());

                CheckOpenGLError();


                //
                // Render
                //

                assert((plane.vertexBuffer.size() % 6) == 0);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(plane.vertexBuffer.size()));


                //
                // Update stats
                //

                mRenderStatistics.LastRenderedShipGenericTextures += plane.vertexBuffer.size() / 6;
            }
        }

        glBindVertexArray(0);
    }
}

void ShipRenderContext::RenderVectorArrows()
{
    glBindVertexArray(*mVectorArrowVAO);

    mShaderManager.ActivateProgram<ProgramType::ShipVectors>();

    glLineWidth(0.5f);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(mVectorArrowVertexBuffer.size()));

    glBindVertexArray(0);
}

}