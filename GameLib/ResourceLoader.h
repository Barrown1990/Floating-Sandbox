/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-02-18
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "Font.h"
#include "ImageData.h"
#include "MaterialDatabase.h"
#include "ProgressCallback.h"
#include "ShipDefinition.h"
#include "SysSpecifics.h"
#include "TextureDatabase.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ResourceLoader
{
public:

    ResourceLoader();

public:

    //
    // Ships
    //

    ShipDefinition LoadShipDefinition(std::filesystem::path const & filePath);

    std::filesystem::path GetDefaultShipDefinitionFilePath() const;


    //
    // Textures
    //

    Render::TextureDatabase LoadTextureDatabase(ProgressCallback const & progressCallback) const;


    //
    // Fonts
    //

    std::vector<Render::Font> LoadFonts(ProgressCallback const & progressCallback) const;


    //
    // Materials
    //

    MaterialDatabase LoadMaterialDatabase() const;


    //
    // Music
    //

    std::filesystem::path GetMusicFilepath(std::string const & musicName) const;


    //
    // Sounds
    //

    std::vector<std::string> GetSoundNames() const;

    std::filesystem::path GetSoundFilepath(std::string const & soundName) const;


    //
    // Resources
    //

    std::filesystem::path GetCursorFilepath(std::string const & cursorName) const;

    std::filesystem::path GetIconFilepath(std::string const & iconName) const;

    std::filesystem::path GetArtFilepath(std::string const & artName) const;


    //
    // Misc
    //

    std::filesystem::path GetOceanFloorBumpMapFilepath() const;


    //
    // Help
    //

    std::filesystem::path GetHelpFilepath() const;


    //
    // Shaders
    //

    std::filesystem::path GetShadersRootPath() const;


    //
    // Images
    //

    static ImageSize GetImageSize(std::filesystem::path const & filepath);

    static ImageData LoadImageRgbaUpperLeft(std::filesystem::path const & filepath);
    static ImageData LoadImageRgbaLowerLeft(std::filesystem::path const & filepath);
    static ImageData LoadImageRgbUpperLeft(std::filesystem::path const & filepath);
    static ImageData LoadImageRgbLowerLeft(std::filesystem::path const & filepath);

    static void SaveImage(
        std::filesystem::path filepath,
        ImageData const & image);

private:

    enum class ResizeType
    {
        // No resize
        None,

        // Resize up 8 using nearest filter
        ResizeUp8Nearest
    };

    static ImageData LoadImage(
        std::filesystem::path const & filepath,
        int targetFormat,
        int targetOrigin,
        ResizeType resizeType);
};
