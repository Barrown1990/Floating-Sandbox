/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-02-13
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "RenderContext.h"

#include <Game/ImageFileTools.h>

#include <GameCore/GameException.h>
#include <GameCore/GameWallClock.h>
#include <GameCore/ImageTools.h>
#include <GameCore/Log.h>

#include <cstring>

namespace Render {

ImageSize constexpr ThumbnailSize(32, 32);

RenderContext::RenderContext(
    ResourceLoader & resourceLoader,
    ProgressCallback const & progressCallback)
    // Buffers
    : mStarVertexBuffer()
    , mStarVBO()
	, mLightningVertexBuffer()
	, mBackgroundLightningVertexCount(0)
	, mForegroundLightningVertexCount(0)
	, mLightningVBO()
    , mCloudVertexBuffer()
    , mCloudVBO()
    , mLandSegmentBuffer()
    , mLandSegmentBufferAllocatedSize(0u)
    , mLandVBO()
    , mOceanSegmentBuffer()
    , mOceanSegmentBufferAllocatedSize(0u)
    , mOceanVBO()
    , mCrossOfLightVertexBuffer()
    , mCrossOfLightVBO()
    , mHeatBlasterFlameVBO()
    , mFireExtinguisherSprayVBO()
	, mRainVBO()
    , mWorldBorderVertexBuffer()
    , mWorldBorderVBO()
    // VAOs
    , mStarVAO()
	, mLightningVAO()
    , mCloudVAO()
    , mLandVAO()
    , mOceanVAO()
    , mCrossOfLightVAO()
    , mHeatBlasterFlameVAO()
    , mFireExtinguisherSprayVAO()
	, mRainVAO()
    , mWorldBorderVAO()
    // Textures
    , mCloudTextureAtlasOpenGLHandle()
    , mCloudTextureAtlasMetadata()
    , mUploadedWorldTextureManager()
    , mOceanTextureFrameSpecifications()
    , mOceanTextureOpenGLHandle()
    , mLoadedOceanTextureIndex(std::numeric_limits<size_t>::max())
    , mLandTextureFrameSpecifications()
    , mLandTextureOpenGLHandle()
    , mLoadedLandTextureIndex(std::numeric_limits<size_t>::max())
    , mIsWorldBorderVisible(false)
    , mGenericLinearTextureAtlasOpenGLHandle()
    , mGenericLinearTextureAtlasMetadata()
    , mGenericMipMappedTextureAtlasOpenGLHandle()
    , mGenericMipMappedTextureAtlasMetadata()
    , mExplosionTextureAtlasOpenGLHandle()
    , mExplosionTextureAtlasMetadata()
    , mUploadedNoiseTexturesManager()
    // Misc Parameters
    , mCurrentStormAmbientDarkening(1.0f)
	, mCurrentRainDensity(0.0f)
    , mEffectiveAmbientLightIntensity(1.0f)
    // Ships
    , mShips()
    // HeatBlaster
    , mHeatBlasterFlameShaderToRender()
    // Fire extinguisher
    , mFireExtinguisherSprayShaderToRender()
    // Managers
    , mShaderManager()
    , mTextRenderContext()
    // Render parameters
    , mViewModel(1.0f, vec2f::zero(), 100, 100)
    , mFlatSkyColor(0x87, 0xce, 0xfa) // (cornflower blue)
    , mAmbientLightIntensity(1.0f)
    , mOceanTransparency(0.8125f)
    , mOceanDarkeningRate(0.356993f)
    , mShowShipThroughOcean(false)
    , mWaterContrast(0.71875f)
    , mWaterLevelOfDetail(0.6875f)
    , mShipRenderMode(ShipRenderMode::Texture)
    , mDebugShipRenderMode(DebugShipRenderMode::None)
    , mOceanRenderMode(OceanRenderMode::Texture)
    , mOceanAvailableThumbnails()
    , mSelectedOceanTextureIndex(0) // Wavy Clear Thin
    , mDepthOceanColorStart(0x4a, 0x84, 0x9f)
    , mDepthOceanColorEnd(0x00, 0x00, 0x00)
    , mFlatOceanColor(0x00, 0x3d, 0x99)
    , mLandRenderMode(LandRenderMode::Texture)
    , mLandAvailableThumbnails()
    , mSelectedLandTextureIndex(3) // Rock Coarse 3
    , mFlatLandColor(0x72, 0x46, 0x05)
    , mVectorFieldRenderMode(VectorFieldRenderMode::None)
    , mVectorFieldLengthMultiplier(1.0f)
    , mShowStressedSprings(false)
    , mDrawHeatOverlay(false)
    , mHeatOverlayTransparency(0.1875f)
    , mShipFlameRenderMode(ShipFlameRenderMode::Mode1)
    , mShipFlameSizeAdjustment(1.0f)
    // Statistics
    , mRenderStatistics()
{
    static constexpr float CloudAtlasProgressSteps = 10.0f;
    static constexpr float OceanProgressSteps = 10.0f;
    static constexpr float LandProgressSteps = 10.0f;
    static constexpr float GenericLinearTextureAtlasProgressSteps = 2.0f;
    static constexpr float GenericMipMappedTextureAtlasProgressSteps = 10.0f;
    static constexpr float ExplosionAtlasProgressSteps = 10.0f;

    static constexpr float TotalProgressSteps =
        1.0f // Shaders
        + 1.0f // TextRenderContext
        + CloudAtlasProgressSteps
        + OceanProgressSteps
        + LandProgressSteps
        + GenericLinearTextureAtlasProgressSteps
        + GenericMipMappedTextureAtlasProgressSteps
        + ExplosionAtlasProgressSteps
        + 2.0f; // Noise

    GLuint tmpGLuint;


    //
    // Load shader manager
    //

    progressCallback(0.0f, "Loading shaders...");

    mShaderManager = ShaderManager<ShaderManagerTraits>::CreateInstance(resourceLoader.GetRenderShadersRootPath());


    //
    // Initialize OpenGL
    //

    // Initialize the shared texture unit once and for all
    mShaderManager->ActivateTexture<ProgramParameterType::SharedTexture>();


    //
    // Initialize text render context
    //

    mTextRenderContext = std::make_shared<TextRenderContext>(
        resourceLoader,
        *(mShaderManager.get()),
        mViewModel.GetCanvasWidth(),
        mViewModel.GetCanvasHeight(),
        mAmbientLightIntensity,
        [&progressCallback](float progress, std::string const & message)
        {
            progressCallback((1.0f + progress) / TotalProgressSteps, message);
        });


    //
    // Initialize buffers
    //

    GLuint vbos[10];
    glGenBuffers(10, vbos);
    mStarVBO = vbos[0];
	mLightningVBO = vbos[1];
    mCloudVBO = vbos[2];
    mLandVBO = vbos[3];
    mOceanVBO = vbos[4];
    mCrossOfLightVBO = vbos[5];
    mHeatBlasterFlameVBO = vbos[6];
    mFireExtinguisherSprayVBO = vbos[7];
	mRainVBO = vbos[8];
    mWorldBorderVBO = vbos[9];


    //
    // Initialize Star VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mStarVAO = tmpGLuint;

    glBindVertexArray(*mStarVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mStarVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Star));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Star), 3, GL_FLOAT, GL_FALSE, sizeof(StarVertex), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


	//
	// Initialize Lightning VAO
	//

	glGenVertexArrays(1, &tmpGLuint);
	mLightningVAO = tmpGLuint;

	glBindVertexArray(*mLightningVAO);
	CheckOpenGLError();

	// Describe vertex attributes
	glBindBuffer(GL_ARRAY_BUFFER, *mLightningVBO);
	glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Lightning1));
	glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Lightning1), 4, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)0);
	glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Lightning2));
	glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Lightning2), 3, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)(4 * sizeof(float)));
	CheckOpenGLError();

	glBindVertexArray(0);


    //
    // Initialize Cloud VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mCloudVAO = tmpGLuint;

    glBindVertexArray(*mCloudVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mCloudVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Cloud1));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Cloud1), 4, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void *)0);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Cloud2));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Cloud2), 1, GL_FLOAT, GL_FALSE, sizeof(CloudVertex), (void *)(4 * sizeof(float)));
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize Land VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mLandVAO = tmpGLuint;

    glBindVertexArray(*mLandVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mLandVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Land));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Land), 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize Ocean VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mOceanVAO = tmpGLuint;

    glBindVertexArray(*mOceanVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mOceanVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Ocean));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Ocean), (2 + 1), GL_FLOAT, GL_FALSE, (2 + 1) * sizeof(float), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize CrossOfLight VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mCrossOfLightVAO = tmpGLuint;

    glBindVertexArray(*mCrossOfLightVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mCrossOfLightVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::CrossOfLight1));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::CrossOfLight1), 4, GL_FLOAT, GL_FALSE, sizeof(CrossOfLightVertex), (void*)0);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::CrossOfLight2));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::CrossOfLight2), 1, GL_FLOAT, GL_FALSE, sizeof(CrossOfLightVertex), (void*)(4 * sizeof(float)));
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize HeatBlaster flame VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mHeatBlasterFlameVAO = tmpGLuint;

    glBindVertexArray(*mHeatBlasterFlameVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mHeatBlasterFlameVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::HeatBlasterFlame));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::HeatBlasterFlame), 4, GL_FLOAT, GL_FALSE, sizeof(HeatBlasterFlameVertex), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize fire extinguisher spray VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mFireExtinguisherSprayVAO = tmpGLuint;

    glBindVertexArray(*mFireExtinguisherSprayVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mFireExtinguisherSprayVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::FireExtinguisherSpray));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::FireExtinguisherSpray), 4, GL_FLOAT, GL_FALSE, sizeof(FireExtinguisherSprayVertex), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


	//
	// Initialize Rain VAO
	//

	glGenVertexArrays(1, &tmpGLuint);
	mRainVAO = tmpGLuint;

	glBindVertexArray(*mRainVAO);
	CheckOpenGLError();

	// Describe vertex attributes
	glBindBuffer(GL_ARRAY_BUFFER, *mRainVBO);
	glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Rain));
	glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Rain), 2, GL_FLOAT, GL_FALSE, sizeof(RainVertex), (void*)0);
	CheckOpenGLError();

	// Upload quad
	{
		RainVertex rainVertices[6]{
			{-1.0, 1.0},
			{-1.0, -1.0},
			{1.0, 1.0},
			{-1.0, -1.0},
			{1.0, 1.0},
			{1.0, -1.0}
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(RainVertex) * 6, rainVertices, GL_STATIC_DRAW);
		CheckOpenGLError();
	}

	glBindVertexArray(0);


    //
    // Initialize WorldBorder VAO
    //

    glGenVertexArrays(1, &tmpGLuint);
    mWorldBorderVAO = tmpGLuint;

    glBindVertexArray(*mWorldBorderVAO);
    CheckOpenGLError();

    // Describe vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, *mWorldBorderVBO);
    glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::WorldBorder));
    glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::WorldBorder), 4, GL_FLOAT, GL_FALSE, sizeof(WorldBorderVertex), (void*)0);
    CheckOpenGLError();

    glBindVertexArray(0);


    //
    // Initialize cloud texture atlas
    //

    // Load texture database
    auto cloudTextureDatabase = TextureDatabase<Render::CloudTextureDatabaseTraits>::Load(
        resourceLoader.GetTexturesRootFolderPath());

    // Create atlas
    auto cloudTextureAtlas = TextureAtlasBuilder<CloudTextureGroups>::BuildAtlas(
        cloudTextureDatabase,
        AtlasOptions::None,
        [&progressCallback](float progress, std::string const &)
        {
            progressCallback((2.0f + progress * CloudAtlasProgressSteps) / TotalProgressSteps, "Loading cloud textures...");
        });

    LogMessage("Cloud texture atlas size: ", cloudTextureAtlas.AtlasData.Size.ToString());

    mShaderManager->ActivateTexture<ProgramParameterType::CloudsAtlasTexture>();

    // Create OpenGL handle
    glGenTextures(1, &tmpGLuint);
    mCloudTextureAtlasOpenGLHandle = tmpGLuint;

    // Bind texture atlas
    glBindTexture(GL_TEXTURE_2D, *mCloudTextureAtlasOpenGLHandle);
    CheckOpenGLError();

    // Upload atlas texture
    GameOpenGL::UploadTexture(std::move(cloudTextureAtlas.AtlasData));

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckOpenGLError();

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Store metadata
    mCloudTextureAtlasMetadata = std::make_unique<TextureAtlasMetadata<CloudTextureGroups>>(cloudTextureAtlas.Metadata);

    // Set texture in shader
    mShaderManager->ActivateProgram<ProgramType::Clouds>();
    mShaderManager->SetTextureParameters<ProgramType::Clouds>();


    //
    // Initialize world textures
    //

    // Load texture database
    auto worldTextureDatabase = TextureDatabase<Render::WorldTextureDatabaseTraits>::Load(
        resourceLoader.GetTexturesRootFolderPath());

    // Ocean

    mOceanTextureFrameSpecifications = worldTextureDatabase.GetGroup(WorldTextureGroups::Ocean).GetFrameSpecifications();

    // Create list of available textures for user
    for (size_t i = 0; i < mOceanTextureFrameSpecifications.size(); ++i)
    {
        auto const & tfs = mOceanTextureFrameSpecifications[i];

        auto textureThumbnail = ImageFileTools::LoadImageRgbaLowerLeftAndResize(
            tfs.FilePath,
            ThumbnailSize);

        assert(static_cast<size_t>(tfs.Metadata.FrameId.FrameIndex) == mOceanAvailableThumbnails.size());

        mOceanAvailableThumbnails.emplace_back(
            tfs.Metadata.FrameName,
            std::move(textureThumbnail));

        float progress = static_cast<float>(i + 1) / static_cast<float>(mOceanTextureFrameSpecifications.size());
        progressCallback((2.0f + CloudAtlasProgressSteps + progress * OceanProgressSteps) / TotalProgressSteps, "Loading world textures...");
    }

    // Land

    mLandTextureFrameSpecifications = worldTextureDatabase.GetGroup(WorldTextureGroups::Land).GetFrameSpecifications();

    // Create list of available textures for user
    for (size_t i = 0; i < mLandTextureFrameSpecifications.size(); ++i)
    {
        auto const & tfs = mLandTextureFrameSpecifications[i];

        auto textureThumbnail = ImageFileTools::LoadImageRgbaLowerLeftAndResize(
            tfs.FilePath,
            ThumbnailSize);

        assert(static_cast<size_t>(tfs.Metadata.FrameId.FrameIndex) == mLandAvailableThumbnails.size());

        mLandAvailableThumbnails.emplace_back(
            tfs.Metadata.FrameName,
            std::move(textureThumbnail));

        float progress = static_cast<float>(i + 1) / static_cast<float>(mLandTextureFrameSpecifications.size());
        progressCallback((2.0f + CloudAtlasProgressSteps + OceanProgressSteps + progress * LandProgressSteps) / TotalProgressSteps, "Loading world textures...");
    }


    //
    // Create generic linear texture atlas
    //

    // Load texture database
    auto genericLinearTextureDatabase = TextureDatabase<Render::GenericLinearTextureTextureDatabaseTraits>::Load(
        resourceLoader.GetTexturesRootFolderPath());

    // Create atlas
    auto genericLinearTextureAtlas = TextureAtlasBuilder<GenericLinearTextureGroups>::BuildAtlas(
        genericLinearTextureDatabase,
        AtlasOptions::None,
        [&progressCallback](float progress, std::string const & /*message*/)
        {
            progressCallback((2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + progress * GenericLinearTextureAtlasProgressSteps) / TotalProgressSteps, "Loading generic textures...");
        });

    LogMessage("Generic linear texture atlas size: ", genericLinearTextureAtlas.AtlasData.Size.ToString());

    // Activate texture
    mShaderManager->ActivateTexture<ProgramParameterType::GenericLinearTexturesAtlasTexture>();

    // Create texture OpenGL handle
    glGenTextures(1, &tmpGLuint);
    mGenericLinearTextureAtlasOpenGLHandle = tmpGLuint;

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, *mGenericLinearTextureAtlasOpenGLHandle);
    CheckOpenGLError();

    // Upload atlas texture
    GameOpenGL::UploadTexture(std::move(genericLinearTextureAtlas.AtlasData));

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckOpenGLError();

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Store metadata
    mGenericLinearTextureAtlasMetadata = std::make_unique<TextureAtlasMetadata<GenericLinearTextureGroups>>(
        genericLinearTextureAtlas.Metadata);

    // Set FlamesBackground1 shader parameters
    auto const & fireAtlasFrameMetadata = mGenericLinearTextureAtlasMetadata->GetFrameMetadata(GenericLinearTextureGroups::Fire, 0);
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesBackground1>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesBackground1>();
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesBackground1, ProgramParameterType::AtlasTile1Dx>(
        1.0f / static_cast<float>(fireAtlasFrameMetadata.FrameMetadata.Size.Width),
        1.0f / static_cast<float>(fireAtlasFrameMetadata.FrameMetadata.Size.Height));
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesBackground1, ProgramParameterType::AtlasTile1LeftBottomTextureCoordinates>(
        fireAtlasFrameMetadata.TextureCoordinatesBottomLeft.x,
        fireAtlasFrameMetadata.TextureCoordinatesBottomLeft.y);
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesBackground1, ProgramParameterType::AtlasTile1Size>(
        fireAtlasFrameMetadata.TextureSpaceWidth,
        fireAtlasFrameMetadata.TextureSpaceHeight);

    // Set FlamesForeground1 shader parameters
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesForeground1>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesForeground1>();
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesForeground1, ProgramParameterType::AtlasTile1Dx>(
        1.0f / static_cast<float>(fireAtlasFrameMetadata.FrameMetadata.Size.Width),
        1.0f / static_cast<float>(fireAtlasFrameMetadata.FrameMetadata.Size.Height));
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesForeground1, ProgramParameterType::AtlasTile1LeftBottomTextureCoordinates>(
        fireAtlasFrameMetadata.TextureCoordinatesBottomLeft.x,
        fireAtlasFrameMetadata.TextureCoordinatesBottomLeft.y);
    mShaderManager->SetProgramParameter<ProgramType::ShipFlamesForeground1, ProgramParameterType::AtlasTile1Size>(
        fireAtlasFrameMetadata.TextureSpaceWidth,
        fireAtlasFrameMetadata.TextureSpaceHeight);

    // Set WorldBorder shader parameters
    auto const & worldBorderAtlasFrameMetadata = mGenericLinearTextureAtlasMetadata->GetFrameMetadata(GenericLinearTextureGroups::WorldBorder, 0);
    mShaderManager->ActivateProgram<ProgramType::WorldBorder>();
    mShaderManager->SetTextureParameters<ProgramType::WorldBorder>();
    mShaderManager->SetProgramParameter<ProgramType::WorldBorder, ProgramParameterType::AtlasTile1Dx>(
        1.0f / static_cast<float>(worldBorderAtlasFrameMetadata.FrameMetadata.Size.Width),
        1.0f / static_cast<float>(worldBorderAtlasFrameMetadata.FrameMetadata.Size.Height));
    mShaderManager->SetProgramParameter<ProgramType::WorldBorder, ProgramParameterType::AtlasTile1LeftBottomTextureCoordinates>(
        worldBorderAtlasFrameMetadata.TextureCoordinatesBottomLeft.x,
        worldBorderAtlasFrameMetadata.TextureCoordinatesBottomLeft.y);
    mShaderManager->SetProgramParameter<ProgramType::WorldBorder, ProgramParameterType::AtlasTile1Size>(
        worldBorderAtlasFrameMetadata.TextureSpaceWidth,
        worldBorderAtlasFrameMetadata.TextureSpaceHeight);


    //
    // Create generic mipmapped texture atlas
    //

    // Load texture database
    auto genericMipMappedTextureDatabase = TextureDatabase<Render::GenericMipMappedTextureTextureDatabaseTraits>::Load(
        resourceLoader.GetTexturesRootFolderPath());

    // Create atlas
    auto genericMipMappedTextureAtlas = TextureAtlasBuilder<GenericMipMappedTextureGroups>::BuildAtlas(
        genericMipMappedTextureDatabase,
        AtlasOptions::None,
        [&progressCallback](float progress, std::string const & /*message*/)
        {
            progressCallback((2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + GenericLinearTextureAtlasProgressSteps + progress * GenericMipMappedTextureAtlasProgressSteps) / TotalProgressSteps, "Loading generic textures...");
        });

    LogMessage("Generic mipmapped texture atlas size: ", genericMipMappedTextureAtlas.AtlasData.Size.ToString());

    // Activate texture
    mShaderManager->ActivateTexture<ProgramParameterType::GenericMipMappedTexturesAtlasTexture>();

    // Create texture OpenGL handle
    glGenTextures(1, &tmpGLuint);
    mGenericMipMappedTextureAtlasOpenGLHandle = tmpGLuint;

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, *mGenericMipMappedTextureAtlasOpenGLHandle);
    CheckOpenGLError();

    // Upload atlas texture
    GameOpenGL::UploadMipmappedPowerOfTwoTexture(
        std::move(genericMipMappedTextureAtlas.AtlasData),
        genericMipMappedTextureAtlas.Metadata.GetMaxDimension());

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckOpenGLError();

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Store metadata
    mGenericMipMappedTextureAtlasMetadata = std::make_unique<TextureAtlasMetadata<GenericMipMappedTextureGroups>>(genericMipMappedTextureAtlas.Metadata);

    // Set texture in shaders
    mShaderManager->ActivateProgram<ProgramType::ShipGenericMipMappedTextures>();
    mShaderManager->SetTextureParameters<ProgramType::ShipGenericMipMappedTextures>();


    //
    // Initialize explosions texture atlas
    //

    progressCallback(
        (2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + GenericLinearTextureAtlasProgressSteps
            + GenericMipMappedTextureAtlasProgressSteps + 0.0f) / TotalProgressSteps, "Loading explosion textures...");

    // Load atlas
    TextureAtlas<ExplosionTextureGroups> explosionTextureAtlas = TextureAtlas<ExplosionTextureGroups>::Deserialize(
        ExplosionTextureDatabaseTraits::DatabaseName,
        resourceLoader.GetTexturesRootFolderPath());

    progressCallback(
        (2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + GenericLinearTextureAtlasProgressSteps
            + GenericMipMappedTextureAtlasProgressSteps + ExplosionAtlasProgressSteps) / TotalProgressSteps, "Loading explosion textures...");

    LogMessage("Explosion texture atlas size: ", explosionTextureAtlas.AtlasData.Size.ToString());

    // Activate texture
    mShaderManager->ActivateTexture<ProgramParameterType::ExplosionsAtlasTexture>();

    // Create OpenGL handle
    glGenTextures(1, &tmpGLuint);
    mExplosionTextureAtlasOpenGLHandle = tmpGLuint;

    // Bind texture atlas
    glBindTexture(GL_TEXTURE_2D, *mExplosionTextureAtlasOpenGLHandle);
    CheckOpenGLError();

    // Upload atlas texture
    GameOpenGL::UploadTexture(std::move(explosionTextureAtlas.AtlasData));

    // Set repeat mode - we want to clamp, to leverage the fact that
    // all frames are perfectly transparent at the edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckOpenGLError();

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Store metadata
    mExplosionTextureAtlasMetadata = std::make_unique<TextureAtlasMetadata<ExplosionTextureGroups>>(explosionTextureAtlas.Metadata);

    // Set texture in shaders
    mShaderManager->ActivateProgram<ProgramType::ShipExplosions>();
    mShaderManager->SetTextureParameters<ProgramType::ShipExplosions>();


    //
    // Initialize noise textures
    //

    // Load texture database
    auto noiseTextureDatabase = TextureDatabase<Render::NoiseTextureDatabaseTraits>::Load(
        resourceLoader.GetTexturesRootFolderPath());

    // Noise 1

    mShaderManager->ActivateTexture<ProgramParameterType::NoiseTexture1>();

    mUploadedNoiseTexturesManager.UploadNextFrame(
        noiseTextureDatabase.GetGroup(NoiseTextureGroups::Noise),
        0,
        GL_LINEAR);

    progressCallback(
        (2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + GenericLinearTextureAtlasProgressSteps
            + GenericMipMappedTextureAtlasProgressSteps + ExplosionAtlasProgressSteps + 1.0f) / TotalProgressSteps, "Loading noise textures...");

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, mUploadedNoiseTexturesManager.GetOpenGLHandle(NoiseTextureGroups::Noise, 0));
    CheckOpenGLError();

    // Set noise texture in shaders
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesBackground1>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesBackground1>();
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesBackground2>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesBackground2>();
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesBackground3>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesBackground3>();
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesForeground1>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesForeground1>();
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesForeground2>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesForeground2>();
    mShaderManager->ActivateProgram<ProgramType::ShipFlamesForeground3>();
    mShaderManager->SetTextureParameters<ProgramType::ShipFlamesForeground3>();

    // Noise 2

    mShaderManager->ActivateTexture<ProgramParameterType::NoiseTexture2>();

    mUploadedNoiseTexturesManager.UploadNextFrame(
        noiseTextureDatabase.GetGroup(NoiseTextureGroups::Noise),
        1,
        GL_LINEAR);

    progressCallback(
        (2.0f + CloudAtlasProgressSteps + OceanProgressSteps + LandProgressSteps + GenericLinearTextureAtlasProgressSteps
            + GenericMipMappedTextureAtlasProgressSteps + ExplosionAtlasProgressSteps + 2.0f) / TotalProgressSteps, "Loading noise textures...");

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, mUploadedNoiseTexturesManager.GetOpenGLHandle(NoiseTextureGroups::Noise, 1));
    CheckOpenGLError();

    // Set noise texture in shaders
    mShaderManager->ActivateProgram<ProgramType::HeatBlasterFlameCool>();
    mShaderManager->SetTextureParameters<ProgramType::HeatBlasterFlameCool>();
    mShaderManager->ActivateProgram<ProgramType::HeatBlasterFlameHeat>();
    mShaderManager->SetTextureParameters<ProgramType::HeatBlasterFlameHeat>();
    mShaderManager->ActivateProgram<ProgramType::FireExtinguisherSpray>();
    mShaderManager->SetTextureParameters<ProgramType::FireExtinguisherSpray>();
	mShaderManager->ActivateProgram<ProgramType::Lightning>();
	mShaderManager->SetTextureParameters<ProgramType::Lightning>();


    //
    // Initialize global settings
    //

    // Set anti-aliasing for lines
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Enable blend for alpha transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth test
    glDisable(GL_DEPTH_TEST);

    // Set depth test parameters for when we'll need them
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);


    //
    // Update parameters
    //

    OnViewModelUpdated();

    OnEffectiveAmbientLightIntensityUpdated();
	OnRainDensityUpdated();
    OnOceanTransparencyUpdated();
    OnOceanDarkeningRateUpdated();
    OnOceanRenderParametersUpdated();
    OnOceanTextureIndexUpdated();
    OnLandRenderParametersUpdated();
    OnLandTextureIndexUpdated();
    OnWaterContrastUpdated();
    OnWaterLevelOfDetailUpdated();
    OnShipRenderModeUpdated();
    OnDebugShipRenderModeUpdated();
    OnVectorFieldRenderModeUpdated();
    OnShowStressedSpringsUpdated();
    OnDrawHeatOverlayUpdated();
    OnHeatOverlayTransparencyUpdated();
    OnShipFlameRenderModeUpdated();
    OnShipFlameSizeAdjustmentUpdated();


    //
    // Flush all pending operations
    //

    glFinish();


    //
    // Notify progress
    //

    progressCallback(1.0f, "Loading textures...");
}

RenderContext::~RenderContext()
{
    glUseProgram(0u);
}

//////////////////////////////////////////////////////////////////////////////////

void RenderContext::Reset()
{
    // Clear ships
    mShips.clear();
}

void RenderContext::ValidateShip(
    ShipDefinition const & shipDefinition) const
{
    // Check texture against max texture size
    if (shipDefinition.TextureLayerImage.Size.Width > GameOpenGL::MaxTextureSize
        || shipDefinition.TextureLayerImage.Size.Height > GameOpenGL::MaxTextureSize)
    {
        throw GameException("We are sorry, but this ship's texture image is too large for your graphics driver");
    }
}

void RenderContext::AddShip(
    ShipId shipId,
    size_t pointCount,
    RgbaImageData texture,
    ShipDefinition::TextureOriginType textureOrigin)
{
    assert(shipId == mShips.size());

    size_t const newShipCount = mShips.size() + 1;

    // Tell all ships that there's a new ship
    for (auto & ship : mShips)
    {
        ship->SetShipCount(newShipCount);
    }

    // Add the ship
    mShips.emplace_back(
        new ShipRenderContext(
            shipId,
            newShipCount,
            pointCount,
            std::move(texture),
            textureOrigin,
            *mShaderManager,
            mExplosionTextureAtlasOpenGLHandle,
            *mExplosionTextureAtlasMetadata,
            mGenericLinearTextureAtlasOpenGLHandle,
            *mGenericLinearTextureAtlasMetadata,
            mGenericMipMappedTextureAtlasOpenGLHandle,
            *mGenericMipMappedTextureAtlasMetadata,
            mRenderStatistics,
            mViewModel,
            mEffectiveAmbientLightIntensity,
            CalculateWaterColor(),
            mWaterContrast,
            mWaterLevelOfDetail,
            mShipRenderMode,
            mDebugShipRenderMode,
            mVectorFieldRenderMode,
            mShowStressedSprings,
            mDrawHeatOverlay,
            mHeatOverlayTransparency,
            mShipFlameRenderMode,
            mShipFlameSizeAdjustment));
}

RgbImageData RenderContext::TakeScreenshot()
{
    //
    // Flush draw calls
    //

    glFinish();

    //
    // Allocate buffer
    //

    int const canvasWidth = mViewModel.GetCanvasWidth();
    int const canvasHeight = mViewModel.GetCanvasHeight();

    auto pixelBuffer = std::make_unique<rgbColor[]>(canvasWidth * canvasHeight);

    //
    // Read pixels
    //

    // Alignment is byte
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    CheckOpenGLError();

    // Read the front buffer
    glReadBuffer(GL_FRONT);
    CheckOpenGLError();

    // Read
    glReadPixels(0, 0, canvasWidth, canvasHeight, GL_RGB, GL_UNSIGNED_BYTE, pixelBuffer.get());
    CheckOpenGLError();

    return RgbImageData(
        ImageSize(canvasWidth, canvasHeight),
        std::move(pixelBuffer));
}

//////////////////////////////////////////////////////////////////////////////////

void RenderContext::RenderStart()
{
    // Set polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Clear canvas - and depth buffer
    vec3f const clearColor = mFlatSkyColor.toVec3f() * mEffectiveAmbientLightIntensity;
    glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Reset crosses of light, they are uploaded as needed
    mCrossOfLightVertexBuffer.clear();

    // Reset HeatBlaster flame, it's uploaded as needed
    mHeatBlasterFlameShaderToRender.reset();

    // Reset fire extinguisher spray, it's uploaded as needed
    mFireExtinguisherSprayShaderToRender.reset();

    // Reset stats
    mRenderStatistics.Reset();
}

void RenderContext::UploadStarsStart(size_t starCount)
{
    //
    // Prepare star vertex buffer
    //

    if (starCount != mStarVertexBuffer.max_size())
    {
        // Reallocate GPU buffer
        glBindBuffer(GL_ARRAY_BUFFER, *mStarVBO);
        glBufferData(GL_ARRAY_BUFFER, starCount * sizeof(StarVertex), nullptr, GL_STATIC_DRAW);
        CheckOpenGLError();
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Reallocate CPU buffer
        mStarVertexBuffer.reset(starCount);
    }
    else
    {
        mStarVertexBuffer.clear();
    }
}

void RenderContext::UploadStarsEnd()
{
    //
    // Upload star vertex buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mStarVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mStarVertexBuffer.size() * sizeof(StarVertex), mStarVertexBuffer.data());
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderContext::RenderStars()
{
    if (!mStarVertexBuffer.empty())
    {
        glBindVertexArray(*mStarVAO);

        mShaderManager->ActivateProgram<ProgramType::Stars>();

        glPointSize(0.5f);

        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(mStarVertexBuffer.size()));
        CheckOpenGLError();
    }
}

void RenderContext::RenderCloudsStart()
{

}

void RenderContext::UploadLightningsStart(size_t lightningCount)
{
	//
	// Prepare buffer
	//

	if (lightningCount > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, *mLightningVBO);

		auto const nVertices = 6 * lightningCount;
		if (nVertices > mLightningVertexBuffer.max_size())
		{
			glBufferData(GL_ARRAY_BUFFER, nVertices * sizeof(LightningVertex), nullptr, GL_STREAM_DRAW);
			CheckOpenGLError();
		}

		mLightningVertexBuffer.map_and_fill(nVertices);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	else
	{
		mLightningVertexBuffer.reset();
	}

	mBackgroundLightningVertexCount = 0;
	mForegroundLightningVertexCount = 0;
}

void RenderContext::UploadLightningsEnd()
{
	if (mLightningVertexBuffer.size() > 0)
	{
		//
		// Upload lightning vertex buffer
		//

		glBindBuffer(GL_ARRAY_BUFFER, *mLightningVBO);

		mLightningVertexBuffer.unmap();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void RenderContext::UploadCloudsStart(size_t cloudCount)
{
    //
    // Prepare cloud quad buffer
    //

    if (cloudCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, *mCloudVBO);

        if (mCloudVertexBuffer.size() != 6 * cloudCount)
        {
            glBufferData(GL_ARRAY_BUFFER, 6 * cloudCount * sizeof(CloudVertex), nullptr, GL_STREAM_DRAW);
            CheckOpenGLError();
        }

        mCloudVertexBuffer.map(6 * cloudCount);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else
    {
        mCloudVertexBuffer.reset();
    }
}

void RenderContext::UploadCloudsEnd()
{
    if (mCloudVertexBuffer.size() > 0)
    {
        //
        // Upload cloud vertex buffer
        //

        glBindBuffer(GL_ARRAY_BUFFER, *mCloudVBO);

        mCloudVertexBuffer.unmap();

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void RenderContext::RenderCloudsEnd()
{
	////////////////////////////////////////////////////
	// Draw background clouds, iff there are background lightnings
	////////////////////////////////////////////////////

	// The number of clouds we want to draw *over* background
	// lightnings
	size_t constexpr CloudsOverLightnings = 5;
	GLsizei cloudsOverLightningVertexStart = 0;

	if (mBackgroundLightningVertexCount > 0
		&& mCloudVertexBuffer.size() > 6 * CloudsOverLightnings)
	{
		glBindVertexArray(*mCloudVAO);

		mShaderManager->ActivateProgram<ProgramType::Clouds>();

		if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
			glLineWidth(0.1f);

		cloudsOverLightningVertexStart = static_cast<GLsizei>(mCloudVertexBuffer.size()) - (6 * CloudsOverLightnings);
		glDrawArrays(GL_TRIANGLES, 0, cloudsOverLightningVertexStart);
		CheckOpenGLError();
	}

	////////////////////////////////////////////////////
	// Draw background lightnings
	////////////////////////////////////////////////////

	if (mBackgroundLightningVertexCount > 0)
	{
		glBindVertexArray(*mLightningVAO);

		mShaderManager->ActivateProgram<ProgramType::Lightning>();

		glDrawArrays(GL_TRIANGLES,
			0,
			static_cast<GLsizei>(mBackgroundLightningVertexCount));
		CheckOpenGLError();
	}

    ////////////////////////////////////////////////////
    // Draw clouds
    ////////////////////////////////////////////////////

    if (mCloudVertexBuffer.size() > 0)
    {
        glBindVertexArray(*mCloudVAO);

        mShaderManager->ActivateProgram<ProgramType::Clouds>();

        if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
            glLineWidth(0.1f);

        glDrawArrays(GL_TRIANGLES, cloudsOverLightningVertexStart, static_cast<GLsizei>(mCloudVertexBuffer.size()) - cloudsOverLightningVertexStart);
        CheckOpenGLError();
    }

    ////////////////////////////////////////////////////

    glBindVertexArray(0);
}

void RenderContext::UploadLandStart(size_t slices)
{
    //
    // Prepare land segment buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mLandVBO);

    if (slices + 1 != mLandSegmentBufferAllocatedSize)
    {
        glBufferData(GL_ARRAY_BUFFER, (slices + 1) * sizeof(LandSegment), nullptr, GL_STREAM_DRAW);
        CheckOpenGLError();

        mLandSegmentBufferAllocatedSize = slices + 1;
    }

    mLandSegmentBuffer.map(slices + 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderContext::UploadLandEnd()
{
    //
    // Upload land segment buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mLandVBO);

    mLandSegmentBuffer.unmap();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderContext::RenderLand()
{
    glBindVertexArray(*mLandVAO);

    switch (mLandRenderMode)
    {
        case LandRenderMode::Flat:
        {
            mShaderManager->ActivateProgram<ProgramType::LandFlat>();
            break;
        }

        case LandRenderMode::Texture:
        {
            mShaderManager->ActivateProgram<ProgramType::LandTexture>();
            break;
        }
    }

    if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
        glLineWidth(0.1f);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(2 * mLandSegmentBuffer.size()));

    glBindVertexArray(0);
}

void RenderContext::UploadOceanStart(size_t slices)
{
    //
    // Prepare ocean segment buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mOceanVBO);

    if (slices + 1 != mOceanSegmentBufferAllocatedSize)
    {
        glBufferData(GL_ARRAY_BUFFER, (slices + 1) * sizeof(OceanSegment), nullptr, GL_STREAM_DRAW);
        CheckOpenGLError();

        mOceanSegmentBufferAllocatedSize = slices + 1;
    }

    mOceanSegmentBuffer.map(slices + 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderContext::UploadOceanEnd()
{
    //
    // Upload ocean segment buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mOceanVBO);

    mOceanSegmentBuffer.unmap();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderContext::RenderOcean(bool opaquely)
{
    float const transparency = opaquely ? 0.0f : mOceanTransparency;

    glBindVertexArray(*mOceanVAO);

    switch (mOceanRenderMode)
    {
        case OceanRenderMode::Depth:
        {
            mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
            mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OceanTransparency>(
                transparency);

            break;
        }

        case OceanRenderMode::Flat:
        {
            mShaderManager->ActivateProgram<ProgramType::OceanFlat>();
            mShaderManager->SetProgramParameter<ProgramType::OceanFlat, ProgramParameterType::OceanTransparency>(
                transparency);

            break;
        }

        case OceanRenderMode::Texture:
        {
            mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
            mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::OceanTransparency>(
                transparency);

            break;
        }
    }

    if (mDebugShipRenderMode == DebugShipRenderMode::Wireframe)
        glLineWidth(0.1f);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(2 * mOceanSegmentBuffer.size()));

    glBindVertexArray(0);
}

void RenderContext::RenderShipsStart()
{
    // Enable depth test, required by ships
    glEnable(GL_DEPTH_TEST);
}

void RenderContext::RenderShipsEnd()
{
    // Disable depth test
    glDisable(GL_DEPTH_TEST);
}

void RenderContext::RenderEnd()
{
    // Render crosses of light
    if (!mCrossOfLightVertexBuffer.empty())
    {
        RenderCrossesOfLight();
    }

    // Render HeatBlaster flames
    if (!!mHeatBlasterFlameShaderToRender)
    {
        RenderHeatBlasterFlame();
    }

    // Render fire extinguisher spray
    if (!!mFireExtinguisherSprayShaderToRender)
    {
        RenderFireExtinguisherSpray();
    }

	// Render foreground lightnings
	if (mForegroundLightningVertexCount > 0)
	{
		RenderForegroundLightnings();
	}

	// Render rain
	if (mCurrentRainDensity != 0.0f)
	{
		RenderRain();
	}

    // Render world end
    RenderWorldBorder();

    // Render text
    mTextRenderContext->Render();

    // Flush all pending commands (but not the GPU buffer)
    GameOpenGL::Flush();
}

////////////////////////////////////////////////////////////////////////////////////

void RenderContext::RenderCrossesOfLight()
{
    //
    // Upload buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mCrossOfLightVBO);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(CrossOfLightVertex) * mCrossOfLightVertexBuffer.size(),
        mCrossOfLightVertexBuffer.data(),
        GL_DYNAMIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Render
    //

    glBindVertexArray(*mCrossOfLightVAO);

    mShaderManager->ActivateProgram<ProgramType::CrossOfLight>();

    assert((mCrossOfLightVertexBuffer.size() % 6) == 0);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mCrossOfLightVertexBuffer.size()));

    glBindVertexArray(0);
}

void RenderContext::RenderHeatBlasterFlame()
{
    //
    // Upload buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mHeatBlasterFlameVBO);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(HeatBlasterFlameVertex) * mHeatBlasterFlameVertexBuffer.size(),
        mHeatBlasterFlameVertexBuffer.data(),
        GL_DYNAMIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Render
    //

    glBindVertexArray(*mHeatBlasterFlameVAO);

    assert(!!mHeatBlasterFlameShaderToRender);

    mShaderManager->ActivateProgram(*mHeatBlasterFlameShaderToRender);

    // Set time parameter
    mShaderManager->SetProgramParameter<ProgramParameterType::Time>(
        *mHeatBlasterFlameShaderToRender,
        GameWallClock::GetInstance().NowAsFloat());

    assert((mHeatBlasterFlameVertexBuffer.size() % 6) == 0);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mHeatBlasterFlameVertexBuffer.size()));

    glBindVertexArray(0);
}

void RenderContext::RenderFireExtinguisherSpray()
{
    //
    // Upload buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mFireExtinguisherSprayVBO);
    glBufferData(GL_ARRAY_BUFFER,
        sizeof(FireExtinguisherSprayVertex) * mFireExtinguisherSprayVertexBuffer.size(),
        mFireExtinguisherSprayVertexBuffer.data(),
        GL_DYNAMIC_DRAW);
    CheckOpenGLError();
    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Render
    //

    glBindVertexArray(*mFireExtinguisherSprayVAO);

    assert(!!mFireExtinguisherSprayShaderToRender);

    mShaderManager->ActivateProgram(*mFireExtinguisherSprayShaderToRender);

    // Set time parameter
    mShaderManager->SetProgramParameter<ProgramParameterType::Time>(
        *mFireExtinguisherSprayShaderToRender,
        GameWallClock::GetInstance().NowAsFloat());

	// Draw
    assert((mFireExtinguisherSprayVertexBuffer.size() % 6) == 0);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mFireExtinguisherSprayVertexBuffer.size()));

    glBindVertexArray(0);
}

void RenderContext::RenderForegroundLightnings()
{
	assert(mForegroundLightningVertexCount > 0);

	glBindVertexArray(*mLightningVAO);

	mShaderManager->ActivateProgram<ProgramType::Lightning>();

	glDrawArrays(GL_TRIANGLES,
		static_cast<GLsizei>(mLightningVertexBuffer.max_size() - mForegroundLightningVertexCount),
		static_cast<GLsizei>(mForegroundLightningVertexCount));
	CheckOpenGLError();
}

void RenderContext::RenderRain()
{
	glBindVertexArray(*mRainVAO);

	mShaderManager->ActivateProgram(ProgramType::Rain);

	// Set time parameter
	mShaderManager->SetProgramParameter<ProgramParameterType::Time>(
		ProgramType::Rain,
		GameWallClock::GetInstance().NowAsFloat());

	// Draw
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
}

void RenderContext::RenderWorldBorder()
{
    if (mIsWorldBorderVisible)
    {
        //
        // Render
        //

        glBindVertexArray(*mWorldBorderVAO);

        mShaderManager->ActivateProgram<ProgramType::WorldBorder>();

        assert((mWorldBorderVertexBuffer.size() % 6) == 0);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mWorldBorderVertexBuffer.size()));

        glBindVertexArray(0);
    }
}

////////////////////////////////////////////////////////////////////////////////////

void RenderContext::OnViewModelUpdated()
{
    //
    // Update ortho matrix
    //

    constexpr float ZFar = 1000.0f;
    constexpr float ZNear = 1.0f;

    ViewModel::ProjectionMatrix globalOrthoMatrix;
    mViewModel.CalculateGlobalOrthoMatrix(ZFar, ZNear, globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::LandFlat>();
    mShaderManager->SetProgramParameter<ProgramType::LandFlat, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::LandTexture>();
    mShaderManager->SetProgramParameter<ProgramType::LandTexture, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::OceanFlat>();
    mShaderManager->SetProgramParameter<ProgramType::OceanFlat, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
    mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::CrossOfLight>();
    mShaderManager->SetProgramParameter<ProgramType::CrossOfLight, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::HeatBlasterFlameCool>();
    mShaderManager->SetProgramParameter<ProgramType::HeatBlasterFlameCool, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::HeatBlasterFlameHeat>();
    mShaderManager->SetProgramParameter<ProgramType::HeatBlasterFlameHeat, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::FireExtinguisherSpray>();
    mShaderManager->SetProgramParameter<ProgramType::FireExtinguisherSpray, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    mShaderManager->ActivateProgram<ProgramType::WorldBorder>();
    mShaderManager->SetProgramParameter<ProgramType::WorldBorder, ProgramParameterType::OrthoMatrix>(
        globalOrthoMatrix);

    //
    // Update canvas size
    //

    mShaderManager->ActivateProgram<ProgramType::CrossOfLight>();
    mShaderManager->SetProgramParameter<ProgramType::CrossOfLight, ProgramParameterType::ViewportSize>(
        static_cast<float>(mViewModel.GetCanvasWidth()),
        static_cast<float>(mViewModel.GetCanvasHeight()));

    //
    // Update all ships
    //

    for (auto & ship : mShips)
    {
        ship->OnViewModelUpdated();
    }

    //
    // Update world border
    //

    UpdateWorldBorder();
}

void RenderContext::OnEffectiveAmbientLightIntensityUpdated()
{
    // Calculate effective ambient light intensity
    mEffectiveAmbientLightIntensity = mAmbientLightIntensity * mCurrentStormAmbientDarkening;

    // Set parameters in all programs

    mShaderManager->ActivateProgram<ProgramType::Stars>();
    mShaderManager->SetProgramParameter<ProgramType::Stars, ProgramParameterType::StarTransparency>(
        pow(std::max(0.0f, 1.0f - mEffectiveAmbientLightIntensity), 3.0f));

    mShaderManager->ActivateProgram<ProgramType::Clouds>();
    mShaderManager->SetProgramParameter<ProgramType::Clouds, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

	mShaderManager->ActivateProgram<ProgramType::Lightning>();
	mShaderManager->SetProgramParameter<ProgramType::Lightning, ProgramParameterType::EffectiveAmbientLightIntensity>(
		mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::LandFlat>();
    mShaderManager->SetProgramParameter<ProgramType::LandFlat, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::LandTexture>();
    mShaderManager->SetProgramParameter<ProgramType::LandTexture, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::OceanFlat>();
    mShaderManager->SetProgramParameter<ProgramType::OceanFlat, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
    mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

	mShaderManager->ActivateProgram<ProgramType::Rain>();
	mShaderManager->SetProgramParameter<ProgramType::Rain, ProgramParameterType::EffectiveAmbientLightIntensity>(
		mEffectiveAmbientLightIntensity);

    mShaderManager->ActivateProgram<ProgramType::WorldBorder>();
    mShaderManager->SetProgramParameter<ProgramType::WorldBorder, ProgramParameterType::EffectiveAmbientLightIntensity>(
        mEffectiveAmbientLightIntensity);

    // Update all ships
    for (auto & ship : mShips)
    {
        ship->SetEffectiveAmbientLightIntensity(mEffectiveAmbientLightIntensity);
    }

    // Update text context
    mTextRenderContext->UpdateEffectiveAmbientLightIntensity(mEffectiveAmbientLightIntensity);
}

void RenderContext::OnRainDensityUpdated()
{
	// Set parameter in all programs

	mShaderManager->ActivateProgram<ProgramType::Rain>();
	mShaderManager->SetProgramParameter<ProgramType::Rain, ProgramParameterType::RainDensity>(
		mCurrentRainDensity);
}

void RenderContext::OnOceanTransparencyUpdated()
{
    // Set parameter in all programs

    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OceanTransparency>(
        mOceanTransparency);

    mShaderManager->ActivateProgram<ProgramType::OceanFlat>();
    mShaderManager->SetProgramParameter<ProgramType::OceanFlat, ProgramParameterType::OceanTransparency>(
        mOceanTransparency);

    mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
    mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::OceanTransparency>(
        mOceanTransparency);
}

void RenderContext::OnOceanDarkeningRateUpdated()
{
    // Set parameter in all programs

    mShaderManager->ActivateProgram<ProgramType::LandTexture>();
    mShaderManager->SetProgramParameter<ProgramType::LandTexture, ProgramParameterType::OceanDarkeningRate>(
        mOceanDarkeningRate / 50.0f);

    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OceanDarkeningRate>(
        mOceanDarkeningRate / 50.0f);

    mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
    mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::OceanDarkeningRate>(
        mOceanDarkeningRate / 50.0f);
}

void RenderContext::OnOceanRenderParametersUpdated()
{
    // Set ocean parameters in all water programs

    auto depthColorStart = mDepthOceanColorStart.toVec3f();
    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OceanDepthColorStart>(
        depthColorStart.x,
        depthColorStart.y,
        depthColorStart.z);

    auto depthColorEnd = mDepthOceanColorEnd.toVec3f();
    mShaderManager->ActivateProgram<ProgramType::OceanDepth>();
    mShaderManager->SetProgramParameter<ProgramType::OceanDepth, ProgramParameterType::OceanDepthColorEnd>(
        depthColorEnd.x,
        depthColorEnd.y,
        depthColorEnd.z);

    auto flatColor = mFlatOceanColor.toVec3f();
    mShaderManager->ActivateProgram<ProgramType::OceanFlat>();
    mShaderManager->SetProgramParameter<ProgramType::OceanFlat, ProgramParameterType::OceanFlatColor>(
        flatColor.x,
        flatColor.y,
        flatColor.z);

    //
    // Tell ships about the water color
    //

    auto const waterColor = CalculateWaterColor();
    for (auto & s : mShips)
    {
        s->SetWaterColor(waterColor);
    }
}

void RenderContext::OnOceanTextureIndexUpdated()
{
    if (mSelectedOceanTextureIndex != mLoadedOceanTextureIndex)
    {
        //
        // Reload the ocean texture
        //

        // Destroy previous texture
        mOceanTextureOpenGLHandle.reset();

        // Clamp the texture index
        mLoadedOceanTextureIndex = std::min(mSelectedOceanTextureIndex, mOceanTextureFrameSpecifications.size() - 1);

        // Load texture image
        auto oceanTextureFrame = mOceanTextureFrameSpecifications[mLoadedOceanTextureIndex].LoadFrame();

        // Activate texture
        mShaderManager->ActivateTexture<ProgramParameterType::OceanTexture>();

        // Create texture
        GLuint tmpGLuint;
        glGenTextures(1, &tmpGLuint);
        mOceanTextureOpenGLHandle = tmpGLuint;

        // Bind texture
        glBindTexture(GL_TEXTURE_2D, *mOceanTextureOpenGLHandle);
        CheckOpenGLError();

        // Upload texture
        GameOpenGL::UploadMipmappedTexture(std::move(oceanTextureFrame.TextureData));

        // Set repeat mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        CheckOpenGLError();

        // Set filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        CheckOpenGLError();

        // Set texture and texture parameters in shader
        mShaderManager->ActivateProgram<ProgramType::OceanTexture>();
        mShaderManager->SetProgramParameter<ProgramType::OceanTexture, ProgramParameterType::TextureScaling>(
            1.0f / oceanTextureFrame.Metadata.WorldWidth,
            1.0f / oceanTextureFrame.Metadata.WorldHeight);
        mShaderManager->SetTextureParameters<ProgramType::OceanTexture>();
    }
}

void RenderContext::OnLandRenderParametersUpdated()
{
    // Set land parameters in all water programs

    auto flatColor = mFlatLandColor.toVec3f();
    mShaderManager->ActivateProgram<ProgramType::LandFlat>();
    mShaderManager->SetProgramParameter<ProgramType::LandFlat, ProgramParameterType::LandFlatColor>(
        flatColor.x,
        flatColor.y,
        flatColor.z);
}

void RenderContext::OnLandTextureIndexUpdated()
{
    if (mSelectedLandTextureIndex != mLoadedLandTextureIndex)
    {
        //
        // Reload the land texture
        //

        // Destroy previous texture
        mLandTextureOpenGLHandle.reset();

        // Clamp the texture index
        mLoadedLandTextureIndex = std::min(mSelectedLandTextureIndex, mLandTextureFrameSpecifications.size() - 1);

        // Load texture image
        auto landTextureFrame = mLandTextureFrameSpecifications[mLoadedLandTextureIndex].LoadFrame();

        // Activate texture
        mShaderManager->ActivateTexture<ProgramParameterType::LandTexture>();

        // Create texture
        GLuint tmpGLuint;
        glGenTextures(1, &tmpGLuint);
        mLandTextureOpenGLHandle = tmpGLuint;

        // Bind texture
        glBindTexture(GL_TEXTURE_2D, *mLandTextureOpenGLHandle);
        CheckOpenGLError();

        // Upload texture
        GameOpenGL::UploadMipmappedTexture(std::move(landTextureFrame.TextureData));

        // Set repeat mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        CheckOpenGLError();

        // Set filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        CheckOpenGLError();

        // Set texture and texture parameters in shader
        mShaderManager->ActivateProgram<ProgramType::LandTexture>();
        mShaderManager->SetProgramParameter<ProgramType::LandTexture, ProgramParameterType::TextureScaling>(
            1.0f / landTextureFrame.Metadata.WorldWidth,
            1.0f / landTextureFrame.Metadata.WorldHeight);
        mShaderManager->SetTextureParameters<ProgramType::LandTexture>();
    }
}

void RenderContext::OnWaterContrastUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetWaterContrast(mWaterContrast);
    }
}

void RenderContext::OnWaterLevelOfDetailUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetWaterLevelThreshold(mWaterLevelOfDetail);
    }
}

void RenderContext::OnShipRenderModeUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetShipRenderMode(mShipRenderMode);
    }
}

void RenderContext::OnDebugShipRenderModeUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetDebugShipRenderMode(mDebugShipRenderMode);
    }
}

void RenderContext::OnVectorFieldRenderModeUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetVectorFieldRenderMode(mVectorFieldRenderMode);
    }
}

void RenderContext::OnShowStressedSpringsUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetShowStressedSprings(mShowStressedSprings);
    }
}

void RenderContext::OnDrawHeatOverlayUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetDrawHeatOverlay(mDrawHeatOverlay);
    }
}

void RenderContext::OnHeatOverlayTransparencyUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetHeatOverlayTransparency(mHeatOverlayTransparency);
    }
}

void RenderContext::OnShipFlameRenderModeUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetShipFlameRenderMode(mShipFlameRenderMode);
    }
}

void RenderContext::OnShipFlameSizeAdjustmentUpdated()
{
    // Set parameter in all ships

    for (auto & s : mShips)
    {
        s->SetShipFlameSizeAdjustment(mShipFlameSizeAdjustment);
    }
}

template <typename TVertexBuffer>
static void MakeQuad(
    float x1,
    float y1,
    float tx1,
    float ty1,
    float x2,
    float y2,
    float tx2,
    float ty2,
    TVertexBuffer & buffer)
{
    buffer.emplace_back(x1, y1, tx1, ty1);
    buffer.emplace_back(x1, y2, tx1, ty2);
    buffer.emplace_back(x2, y1, tx2, ty1);
    buffer.emplace_back(x1, y2, tx1, ty2);
    buffer.emplace_back(x2, y1, tx2, ty1);
    buffer.emplace_back(x2, y2, tx2, ty2);
}

void RenderContext::UpdateWorldBorder()
{
    ImageSize const & worldBorderTextureSize =
        mGenericLinearTextureAtlasMetadata->GetFrameMetadata(GenericLinearTextureGroups::WorldBorder, 0)
        .FrameMetadata.Size;

    // Calculate width and height, in world coordinates, of the world border, under the constraint
    // that we want to ensure that the texture is rendered with half of its original pixel size
    float const worldBorderWorldWidth = mViewModel.PixelWidthToWorldWidth(static_cast<float>(worldBorderTextureSize.Width)) / 2.0f;
    float const worldBorderWorldHeight = mViewModel.PixelHeightToWorldHeight(static_cast<float>(worldBorderTextureSize.Height)) / 2.0f;

    // Max coordinates in texture space (e.g. 3.0 means three frames); note that the texture bottom-left origin
    // already starts at a dead pixel (0.5/size)
    float const textureSpaceWidth =
        GameParameters::MaxWorldWidth / worldBorderWorldWidth
        - 1.0f / static_cast<float>(worldBorderTextureSize.Width);
    float const textureSpaceHeight = GameParameters::MaxWorldHeight / worldBorderWorldHeight
        - 1.0f / static_cast<float>(worldBorderTextureSize.Height);


    //
    // Check which sides of the border we need to draw
    //

    mWorldBorderVertexBuffer.clear();

    // Left
    if (-GameParameters::HalfMaxWorldWidth + worldBorderWorldWidth >= mViewModel.GetVisibleWorldTopLeft().x)
    {
        MakeQuad(
            // Top-left
            -GameParameters::HalfMaxWorldWidth,
            GameParameters::HalfMaxWorldHeight,
            0.0f,
            textureSpaceHeight,
            // Bottom-right
            -GameParameters::HalfMaxWorldWidth + worldBorderWorldWidth,
            -GameParameters::HalfMaxWorldHeight,
            1.0f,
            0.0f,
            mWorldBorderVertexBuffer);
    }

    // Right
    if (GameParameters::HalfMaxWorldWidth - worldBorderWorldWidth <= mViewModel.GetVisibleWorldBottomRight().x)
    {
        MakeQuad(
            // Top-left
            GameParameters::HalfMaxWorldWidth - worldBorderWorldWidth,
            GameParameters::HalfMaxWorldHeight,
            0.0f,
            textureSpaceHeight,
            // Bottom-right
            GameParameters::HalfMaxWorldWidth,
            -GameParameters::HalfMaxWorldHeight,
            1.0f,
            0.0f,
            mWorldBorderVertexBuffer);
    }

    // Top
    if (GameParameters::HalfMaxWorldHeight - worldBorderWorldHeight <= mViewModel.GetVisibleWorldTopLeft().y)
    {
        MakeQuad(
            // Top-left
            -GameParameters::HalfMaxWorldWidth,
            GameParameters::HalfMaxWorldHeight,
            0.0f,
            1.0f,
            // Bottom-right
            GameParameters::HalfMaxWorldWidth,
            GameParameters::HalfMaxWorldHeight - worldBorderWorldHeight,
            textureSpaceWidth,
            0.0f,
            mWorldBorderVertexBuffer);
    }

    // Bottom
    if (-GameParameters::HalfMaxWorldHeight + worldBorderWorldHeight >= mViewModel.GetVisibleWorldBottomRight().y)
    {
        MakeQuad(
            // Top-left
            -GameParameters::HalfMaxWorldWidth,
            -GameParameters::HalfMaxWorldHeight + worldBorderWorldHeight,
            0.0f,
            1.0f,
            // Bottom-right
            GameParameters::HalfMaxWorldWidth,
            -GameParameters::HalfMaxWorldHeight,
            textureSpaceWidth,
            0.0f,
            mWorldBorderVertexBuffer);
    }

    if (!mWorldBorderVertexBuffer.empty())
    {
        //
        // Upload buffer
        //

        glBindBuffer(GL_ARRAY_BUFFER, *mWorldBorderVBO);

        glBufferData(GL_ARRAY_BUFFER,
            sizeof(WorldBorderVertex) * mWorldBorderVertexBuffer.size(),
            mWorldBorderVertexBuffer.data(),
            GL_STATIC_DRAW);
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Remember we have to draw the world border
        mIsWorldBorderVisible = true;
    }
    else
    {
        // No need to draw the world border
        mIsWorldBorderVisible = false;
    }
}

vec4f RenderContext::CalculateWaterColor() const
{
    switch (mOceanRenderMode)
    {
        case OceanRenderMode::Depth:
        {
            return
                (mDepthOceanColorStart.toVec4f(1.0f) + mDepthOceanColorEnd.toVec4f(1.0f))
                / 2.0f;
        }

        case OceanRenderMode::Flat:
        {
            return mFlatOceanColor.toVec4f(1.0f);
        }

        default:
        {
            assert(mOceanRenderMode == OceanRenderMode::Texture); // Darn VS - warns

            return vec4f(0.0f, 0.0f, 0.8f, 1.0f);
        }
    }
}

}