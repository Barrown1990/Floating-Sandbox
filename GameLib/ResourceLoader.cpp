/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-02-18
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "ResourceLoader.h"

#include "GameException.h"
#include "Log.h"
#include "ShipDefinitionFile.h"
#include "Utils.h"

#include <IL/il.h>
#include <IL/ilu.h>

#include <algorithm>
#include <cstring>
#include <regex>

ResourceLoader::ResourceLoader()
{
    // Initialize DevIL
    ilInit();
    iluInit();
}

////////////////////////////////////////////////////////////////////////////////////////////
// Ships
////////////////////////////////////////////////////////////////////////////////////////////

ShipDefinition ResourceLoader::LoadShipDefinition(std::filesystem::path const & filepath)
{
    std::filesystem::path absoluteStructuralLayerImageFilePath;
    std::optional<ImageData> ropesLayerImage;
    std::optional<ImageData> electricalLayerImage;
    std::filesystem::path absoluteTextureLayerImageFilePath;
    ShipDefinition::TextureOriginType textureOrigin;
    std::optional<ShipMetadata> shipMetadata;

    if (ShipDefinitionFile::IsShipDefinitionFile(filepath))
    {
        //
        // Load full definition
        //

        picojson::value root = Utils::ParseJSONFile(filepath.string());
        if (!root.is<picojson::object>())
        {
            throw GameException("File \"" + filepath.string() + "\" does not contain a JSON object");
        }

        ShipDefinitionFile sdf = ShipDefinitionFile::Create(root.get<picojson::object>());

        //
        // Make paths absolute
        //

        std::filesystem::path basePath = filepath.parent_path();

        absoluteStructuralLayerImageFilePath =
            basePath /sdf.StructuralLayerImageFilePath;

        if (!!sdf.RopesLayerImageFilePath)
        {
            ropesLayerImage.emplace(
                ResourceLoader::LoadImage(
                    (basePath / *sdf.RopesLayerImageFilePath).string(),
                    IL_RGB,
                    IL_ORIGIN_UPPER_LEFT,
                    1));
        }

        if (!!sdf.ElectricalLayerImageFilePath)
        {
            electricalLayerImage.emplace(
                ResourceLoader::LoadImage(
                (basePath / *sdf.ElectricalLayerImageFilePath).string(),
                    IL_RGB,
                    IL_ORIGIN_UPPER_LEFT,
                    1));
        }

        if (!!sdf.TextureLayerImageFilePath)
        {
            // Texture image is as specified

            absoluteTextureLayerImageFilePath = basePath / *sdf.TextureLayerImageFilePath;

            textureOrigin = ShipDefinition::TextureOriginType::Texture;
        }
        else
        {
            // Texture image is the structural image

            absoluteTextureLayerImageFilePath = absoluteStructuralLayerImageFilePath;

            textureOrigin = ShipDefinition::TextureOriginType::StructuralImage;
        }


        //
        // Make metadata
        //

        std::string shipName = sdf.Metadata.ShipName.empty()
            ? std::filesystem::path(filepath).stem().string()
            : sdf.Metadata.ShipName;

        shipMetadata.emplace(
            shipName,
            sdf.Metadata.Author,
            sdf.Metadata.Offset);
    }
    else
    {
        //
        // Assume it's just a structural image
        //

        absoluteStructuralLayerImageFilePath = filepath;
        absoluteTextureLayerImageFilePath = absoluteStructuralLayerImageFilePath;
        textureOrigin = ShipDefinition::TextureOriginType::StructuralImage;

        shipMetadata.emplace(
            std::filesystem::path(filepath).stem().string(),
            std::nullopt,
            vec2f(0.0f, 0.0f));
    }

    assert(!!shipMetadata);

    //
    // Load structural image
    //

    ImageData structuralImage = ResourceLoader::LoadImage(
        absoluteStructuralLayerImageFilePath.string(),
        IL_RGB,
        IL_ORIGIN_UPPER_LEFT,
        1);

    //
    // Load texture image
    //

    std::optional<ImageData> textureImage;

    switch (textureOrigin)
    {
        case ShipDefinition::TextureOriginType::Texture:
        {
            // Just load as-is

            textureImage.emplace(
                ResourceLoader::LoadImage(
                    absoluteTextureLayerImageFilePath.string(),
                    IL_RGBA,
                    IL_ORIGIN_LOWER_LEFT,
                    1));

            break;
        }

        case ShipDefinition::TextureOriginType::StructuralImage:
        {
            // Resize it up - ideally by 8, but don't exceed 4096 in any dimension

            int maxDimension = std::max(structuralImage.Size.Width, structuralImage.Size.Height);
            int resize = 8;
            while (maxDimension * resize > 4096 && resize > 1)
                resize /= 2;

            textureImage.emplace(
                ResourceLoader::LoadImage(
                    absoluteTextureLayerImageFilePath.string(),
                    IL_RGBA,
                    IL_ORIGIN_LOWER_LEFT,
                    resize));

            break;
        }
    }

    assert(!!textureImage);

    return ShipDefinition(
        std::move(structuralImage),
        std::move(ropesLayerImage),
        std::move(electricalLayerImage),
        std::move(*textureImage),
        textureOrigin,
        *shipMetadata);
}

std::filesystem::path ResourceLoader::GetDefaultShipDefinitionFilePath() const
{
    std::filesystem::path defaultShipDefinitionFilePath = std::filesystem::path("Ships") / "default_ship.shp";
    if (!std::filesystem::exists(defaultShipDefinitionFilePath))
    {
        defaultShipDefinitionFilePath = std::filesystem::path("Ships") / "default_ship.png";
    }

    return defaultShipDefinitionFilePath;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Textures
////////////////////////////////////////////////////////////////////////////////////////////

Render::TextureDatabase ResourceLoader::LoadTextureDatabase(ProgressCallback const & progressCallback) const
{
    return Render::TextureDatabase::Load(
        std::filesystem::path("Data") / "Textures",
        progressCallback);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Fonts
////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Render::Font> ResourceLoader::LoadFonts(ProgressCallback const & progressCallback) const
{
    //
    // Get all font file paths
    //

    std::vector<std::filesystem::path> filepaths;

    for (auto const & entryIt : std::filesystem::directory_iterator(std::filesystem::path("Data") / "Fonts"))
    {
        if (std::filesystem::is_regular_file(entryIt.path())
            && entryIt.path().extension().string() == ".bff")
        {
            filepaths.push_back(entryIt.path());
        }
    }


    //
    // Sort paths
    //

    std::sort(
        filepaths.begin(),
        filepaths.end(),
        [](auto const & fp1, auto const & fp2)
        {
            return fp1 < fp2;
        });


    //
    // Load fonts
    //

    std::vector<Render::Font> fonts;

    for (auto const & filepath : filepaths)
    {
        fonts.emplace_back(
            std::move(
                Render::Font::Load(filepath)));

        progressCallback(
            static_cast<float>(fonts.size()) / static_cast<float>(filepaths.size()),
            "Loading fonts...");
    }

    return fonts;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Materials
////////////////////////////////////////////////////////////////////////////////////////////

MaterialDatabase ResourceLoader::LoadMaterialDatabase() const
{
    return LoadMaterialDatabase(std::filesystem::path("Data"));
}

MaterialDatabase ResourceLoader::LoadMaterialDatabase(std::filesystem::path materialsDirectory)
{
    std::filesystem::path const structuralMaterialsFilepath = materialsDirectory / "materials_structural.json";
    picojson::value structuralMaterialsRoot = Utils::ParseJSONFile(structuralMaterialsFilepath.string());

    std::filesystem::path const electricalMaterialsFilepath = materialsDirectory / "materials_electrical.json";
    picojson::value electricalMaterialsRoot = Utils::ParseJSONFile(electricalMaterialsFilepath.string());

    return MaterialDatabase::Create(
        structuralMaterialsRoot,
        electricalMaterialsRoot);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Music
////////////////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ResourceLoader::GetMusicFilepath(std::string const & musicName) const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Music" / (musicName + ".flac");
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Sounds
////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ResourceLoader::GetSoundNames() const
{
    std::vector<std::string> filenames;
    for (auto const & entryIt : std::filesystem::directory_iterator(std::filesystem::path("Data") / "Sounds"))
    {
        if (std::filesystem::is_regular_file(entryIt.path())
            && entryIt.path().extension().string() == ".flac")
        {
            filenames.push_back(entryIt.path().stem().string());
        }
    }

    return filenames;
}

std::filesystem::path ResourceLoader::GetSoundFilepath(std::string const & soundName) const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Sounds" / (soundName + ".flac");
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Resources
////////////////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ResourceLoader::GetCursorFilepath(std::string const & cursorName) const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Resources" / (cursorName + ".png");
    return std::filesystem::absolute(localPath);
}

std::filesystem::path ResourceLoader::GetIconFilepath(std::string const & iconName) const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Resources" / (iconName + ".png");
    return std::filesystem::absolute(localPath);
}

std::filesystem::path ResourceLoader::GetArtFilepath(std::string const & artName) const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Resources" / (artName + ".png");
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Misc
////////////////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ResourceLoader::GetOceanFloorBumpMapFilepath() const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Misc" / "ocean_floor_bumpmap.png";
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Help
////////////////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ResourceLoader::GetHelpFilepath() const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Help" / "index.html";
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Shaders
////////////////////////////////////////////////////////////////////////////////////////////

std::filesystem::path ResourceLoader::GetShadersRootPath() const
{
    std::filesystem::path localPath = std::filesystem::path("Data") / "Shaders";
    return std::filesystem::absolute(localPath);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Images
////////////////////////////////////////////////////////////////////////////////////////////

ImageSize ResourceLoader::GetImageSize(std::filesystem::path const & filepath)
{
    ILuint imghandle;
    ilGenImages(1, &imghandle);
    ilBindImage(imghandle);

    //
    // Load image
    //

    std::string filepathStr = filepath.string();
    ILconst_string ilFilename(filepathStr.c_str());
    if (!ilLoadImage(ilFilename))
    {
        ILint devilError = ilGetError();
        std::string devilErrorMessage(iluErrorString(devilError));
        throw GameException("Could not load image \"" + filepathStr + "\": " + devilErrorMessage);
    }

    //
    // Get size
    //

    int const width = ilGetInteger(IL_IMAGE_WIDTH);
    int const height = ilGetInteger(IL_IMAGE_HEIGHT);


    //
    // Delete image
    //

    ilDeleteImage(imghandle);

    return ImageSize(width, height);
}

ImageData ResourceLoader::LoadImageRgbaUpperLeft(std::filesystem::path const & filepath)
{
    return LoadImage(
        filepath,
        IL_RGBA,
        IL_ORIGIN_UPPER_LEFT,
        1);
}

ImageData ResourceLoader::LoadImageRgbaLowerLeft(std::filesystem::path const & filepath)
{
    return LoadImage(
        filepath,
        IL_RGBA,
        IL_ORIGIN_LOWER_LEFT,
        1);
}

ImageData ResourceLoader::LoadImageRgbUpperLeft(std::filesystem::path const & filepath)
{
    return LoadImage(
        filepath,
        IL_RGB,
        IL_ORIGIN_UPPER_LEFT,
        1);
}

ImageData ResourceLoader::LoadImageRgbLowerLeft(std::filesystem::path const & filepath)
{
    return LoadImage(
        filepath,
        IL_RGB,
        IL_ORIGIN_LOWER_LEFT,
        1);
}

ImageData ResourceLoader::LoadImage(
    std::filesystem::path const & filepath,
    int targetFormat,
    int targetOrigin,
    int resize)
{
    //
    // Load image
    //

    ILuint imghandle;
    ilGenImages(1, &imghandle);
    ilBindImage(imghandle);

    std::string filepathStr = filepath.string();
    ILconst_string ilFilename(filepathStr.c_str());
    if (!ilLoadImage(ilFilename))
    {
        ILint devilError = ilGetError();
        std::string devilErrorMessage(iluErrorString(devilError));
        throw GameException("Could not load image \"" + filepathStr + "\": " + devilErrorMessage);
    }

    //
    // Check if we need to convert it
    //

    int imageFormat = ilGetInteger(IL_IMAGE_FORMAT);
    int imageType = ilGetInteger(IL_IMAGE_TYPE);
    if (targetFormat != imageFormat || IL_UNSIGNED_BYTE != imageType)
    {
        if (!ilConvertImage(targetFormat, IL_UNSIGNED_BYTE))
        {
            ILint devilError = ilGetError();
            std::string devilErrorMessage(iluErrorString(devilError));
            throw GameException("Could not convert image \"" + filepathStr + "\": " + devilErrorMessage);
        }
    }

    int imageOrigin = ilGetInteger(IL_IMAGE_ORIGIN);
    if (targetOrigin != imageOrigin)
    {
        iluFlipImage();
    }


    //
    // Get metadata
    //

    int width = ilGetInteger(IL_IMAGE_WIDTH);
    int height = ilGetInteger(IL_IMAGE_HEIGHT);
    int const depth = ilGetInteger(IL_IMAGE_DEPTH);
    int const bpp = ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL);



    //
    // Resize it
    //

    if (resize != 1)
    {
        //
        // Resize with nearest
        //

        iluImageParameter(ILU_FILTER, ILU_NEAREST);

        if (!iluScale(width * resize, height * resize, depth))
        {
            ILint devilError = ilGetError();
            std::string devilErrorMessage(iluErrorString(devilError));
            throw GameException("Could not resize image: " + devilErrorMessage);
        }

        width *= resize;
        height *= resize;
    }


    //
    // Create data
    //

    ILubyte const * imageData = ilGetData();
    auto data = std::make_unique<unsigned char []>(width * height * bpp);
    std::memcpy(data.get(), imageData, width * height * bpp);


    //
    // Delete image
    //

    ilDeleteImage(imghandle);

    return ImageData(
        width,
        height,
        std::move(data));
}

void ResourceLoader::SaveImage(
    std::filesystem::path filepath,
    ImageData const & image)
{
    ILuint imghandle;
    ilGenImages(1, &imghandle);
    ilBindImage(imghandle);

    ilTexImage(
        image.Size.Width,
        image.Size.Height,
        1,
        4,
        IL_RGBA,
        IL_UNSIGNED_BYTE,
        reinterpret_cast<void *>(const_cast<unsigned char *>(image.Data.get())));

    ilEnable(IL_FILE_OVERWRITE);
    ilSave(IL_PNG, filepath.string().c_str());

    ilDeleteImage(imghandle);
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////