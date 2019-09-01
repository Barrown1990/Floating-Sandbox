/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-10-08
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "RenderCore.h"

#include <GameCore/GameException.h>
#include <GameCore/Utils.h>

#include <cassert>

namespace Render {

ProgramType ShaderFilenameToProgramType(std::string const & str)
{
    std::string lstr = Utils::ToLower(str);
    if (lstr == "clouds")
        return ProgramType::Clouds;
    else if (lstr == "cross_of_light")
        return ProgramType::CrossOfLight;
    else if (lstr == "fire_extinguisher_spray")
        return ProgramType::FireExtinguisherSpray;
    else if (lstr == "heat_blaster_flame_cool")
        return ProgramType::HeatBlasterFlameCool;
    else if (lstr == "heat_blaster_flame_heat")
        return ProgramType::HeatBlasterFlameHeat;
    else if (lstr == "land_flat")
        return ProgramType::LandFlat;
    else if (lstr == "land_texture")
        return ProgramType::LandTexture;
    else if (lstr == "ocean_depth")
        return ProgramType::OceanDepth;
    else if (lstr == "ocean_flat")
        return ProgramType::OceanFlat;
    else if (lstr == "ocean_texture")
        return ProgramType::OceanTexture;
    else if (lstr == "ship_flames_background_1")
        return ProgramType::ShipFlamesBackground1;
    else if (lstr == "ship_flames_background_2")
        return ProgramType::ShipFlamesBackground2;
    else if (lstr == "ship_flames_foreground_1")
        return ProgramType::ShipFlamesForeground1;
    else if (lstr == "ship_flames_foreground_2")
        return ProgramType::ShipFlamesForeground2;
    else if (lstr == "ship_generic_textures")
        return ProgramType::ShipGenericTextures;
    else if (lstr == "ship_points_color")
        return ProgramType::ShipPointsColor;
    else if (lstr == "ship_points_color_with_temperature")
        return ProgramType::ShipPointsColorWithTemperature;
    else if (lstr == "ship_ropes")
        return ProgramType::ShipRopes;
    else if (lstr == "ship_ropes_with_temperature")
        return ProgramType::ShipRopesWithTemperature;
    else if (lstr == "ship_springs_color")
        return ProgramType::ShipSpringsColor;
    else if (lstr == "ship_springs_color_with_temperature")
        return ProgramType::ShipSpringsColorWithTemperature;
    else if (lstr == "ship_springs_texture")
        return ProgramType::ShipSpringsTexture;
    else if (lstr == "ship_springs_texture_with_temperature")
        return ProgramType::ShipSpringsTextureWithTemperature;
    else if (lstr == "ship_stressed_springs")
        return ProgramType::ShipStressedSprings;
    else if (lstr == "ship_triangles_color")
        return ProgramType::ShipTrianglesColor;
    else if (lstr == "ship_triangles_color_with_temperature")
        return ProgramType::ShipTrianglesColorWithTemperature;
    else if (lstr == "ship_triangles_decay")
        return ProgramType::ShipTrianglesDecay;
    else if (lstr == "ship_triangles_texture")
        return ProgramType::ShipTrianglesTexture;
    else if (lstr == "ship_triangles_texture_with_temperature")
        return ProgramType::ShipTrianglesTextureWithTemperature;
    else if (lstr == "ship_vectors")
        return ProgramType::ShipVectors;
    else if (lstr == "stars")
        return ProgramType::Stars;
    else if (lstr == "text_ndc")
        return ProgramType::TextNDC;
    else if (lstr == "world_border")
        return ProgramType::WorldBorder;
    else
        throw GameException("Unrecognized program \"" + str + "\"");
}

std::string ProgramTypeToStr(ProgramType program)
{
    switch (program)
    {
    case ProgramType::Clouds:
        return "Clouds";
    case ProgramType::CrossOfLight:
        return "CrossOfLight";
    case ProgramType::FireExtinguisherSpray:
        return "FireExtinguisherSpray";
    case ProgramType::HeatBlasterFlameCool:
        return "HeatBlasterFlameCool";
    case ProgramType::HeatBlasterFlameHeat:
        return "HeatBlasterFlameHeat";
    case ProgramType::LandFlat:
        return "LandFlat";
    case ProgramType::LandTexture:
        return "LandTexture";
    case ProgramType::OceanDepth:
        return "OceanDepth";
    case ProgramType::OceanFlat:
        return "OceanFlat";
    case ProgramType::OceanTexture:
        return "OceanTexture";
    case ProgramType::ShipFlamesBackground1:
        return "ShipFlamesBackground1";
    case ProgramType::ShipFlamesBackground2:
        return "ShipFlamesBackground2";
    case ProgramType::ShipFlamesForeground1:
        return "ShipFlamesForeground1";
    case ProgramType::ShipFlamesForeground2:
        return "ShipFlamesForeground2";
    case ProgramType::ShipGenericTextures:
        return "ShipGenericTextures";
    case ProgramType::ShipPointsColor:
        return "ShipPointsColor";
    case ProgramType::ShipPointsColorWithTemperature:
        return "ShipPointsColorWithTemperature";
    case ProgramType::ShipRopes:
        return "ShipRopes";
    case ProgramType::ShipRopesWithTemperature:
        return "ShipRopesWithTemperature";
    case ProgramType::ShipSpringsColor:
        return "ShipSpringsColor";
    case ProgramType::ShipSpringsColorWithTemperature:
        return "ShipSpringsColorWithTemperature";
    case ProgramType::ShipSpringsTexture:
        return "ShipSpringsTexture";
    case ProgramType::ShipSpringsTextureWithTemperature:
        return "ShipSpringsTextureWithTemperature";
    case ProgramType::ShipStressedSprings:
        return "ShipStressedSprings";
    case ProgramType::ShipTrianglesColor:
        return "ShipTrianglesColor";
    case ProgramType::ShipTrianglesColorWithTemperature:
        return "ShipTrianglesColorWithTemperature";
    case ProgramType::ShipTrianglesDecay:
        return "ShipTrianglesDecay";
    case ProgramType::ShipTrianglesTexture:
        return "ShipTrianglesTexture";
    case ProgramType::ShipTrianglesTextureWithTemperature:
        return "ShipTrianglesTextureWithTemperature";
    case ProgramType::ShipVectors:
        return "ShipVectors";
    case ProgramType::Stars:
        return "Stars";
    case ProgramType::TextNDC:
        return "TextNDC";
    case ProgramType::WorldBorder:
        return "WorldBorder";
    default:
        assert(false);
        throw GameException("Unsupported ProgramType");
    }
}

ProgramParameterType StrToProgramParameterType(std::string const & str)
{
    if (str == "AmbientLightIntensity")
        return ProgramParameterType::AmbientLightIntensity;
    else if (str == "FlameSpeed")
        return ProgramParameterType::FlameSpeed;
    else if (str == "FlameWindRotationAngle")
        return ProgramParameterType::FlameWindRotationAngle;
    else if (str == "HeatOverlayTransparency")
        return ProgramParameterType::HeatOverlayTransparency;
    else if (str == "LandFlatColor")
        return ProgramParameterType::LandFlatColor;
    else if (str == "MatteColor")
        return ProgramParameterType::MatteColor;
    else if (str == "OceanTransparency")
        return ProgramParameterType::OceanTransparency;
    else if (str == "OceanDarkeningRate")
        return ProgramParameterType::OceanDarkeningRate;
    else if (str == "OceanDepthColorStart")
        return ProgramParameterType::OceanDepthColorStart;
    else if (str == "OceanDepthColorEnd")
        return ProgramParameterType::OceanDepthColorEnd;
    else if (str == "OceanFlatColor")
        return ProgramParameterType::OceanFlatColor;
    else if (str == "OrthoMatrix")
        return ProgramParameterType::OrthoMatrix;
    else if (str == "StarTransparency")
        return ProgramParameterType::StarTransparency;
    else if (str == "TextureScaling")
        return ProgramParameterType::TextureScaling;
    else if (str == "Time")
        return ProgramParameterType::Time;
    else if (str == "ViewportSize")
        return ProgramParameterType::ViewportSize;
    else if (str == "WaterColor")
        return ProgramParameterType::WaterColor;
    else if (str == "WaterContrast")
        return ProgramParameterType::WaterContrast;
    else if (str == "WaterLevelThreshold")
        return ProgramParameterType::WaterLevelThreshold;
    // Textures
    else if (str == "SharedTexture")
        return ProgramParameterType::SharedTexture;
    else if (str == "CloudTexture")
        return ProgramParameterType::CloudTexture;
    else if (str == "GenericTexturesAtlasTexture")
        return ProgramParameterType::GenericTexturesAtlasTexture;
    else if (str == "LandTexture")
        return ProgramParameterType::LandTexture;
    else if (str == "NoiseTexture1")
        return ProgramParameterType::NoiseTexture1;
    else if (str == "NoiseTexture2")
        return ProgramParameterType::NoiseTexture2;
    else if (str == "OceanTexture")
        return ProgramParameterType::OceanTexture;
    else if (str == "WorldBorderTexture")
        return ProgramParameterType::WorldBorderTexture;
    else
        throw GameException("Unrecognized program parameter \"" + str + "\"");
}

std::string ProgramParameterTypeToStr(ProgramParameterType programParameter)
{
    switch (programParameter)
    {
    case ProgramParameterType::AmbientLightIntensity:
        return "AmbientLightIntensity";
    case ProgramParameterType::FlameSpeed:
        return "FlameSpeed";
    case ProgramParameterType::FlameWindRotationAngle:
        return "FlameWindRotationAngle";
    case ProgramParameterType::HeatOverlayTransparency:
        return "HeatOverlayTransparency";
    case ProgramParameterType::LandFlatColor:
        return "LandFlatColor";
    case ProgramParameterType::MatteColor:
        return "MatteColor";
    case ProgramParameterType::OceanTransparency:
        return "OceanTransparency";
    case ProgramParameterType::OceanDarkeningRate:
        return "OceanDarkeningRate";
    case ProgramParameterType::OceanDepthColorStart:
        return "OceanDepthColorStart";
    case ProgramParameterType::OceanDepthColorEnd:
        return "OceanDepthColorEnd";
    case ProgramParameterType::OceanFlatColor:
        return "OceanFlatColor";
    case ProgramParameterType::OrthoMatrix:
        return "OrthoMatrix";
    case ProgramParameterType::StarTransparency:
        return "StarTransparency";
    case ProgramParameterType::TextureScaling:
        return "TextureScaling";
    case ProgramParameterType::Time:
        return "Time";
    case ProgramParameterType::ViewportSize:
        return "ViewportSize";
    case ProgramParameterType::WaterColor:
        return "WaterColor";
    case ProgramParameterType::WaterContrast:
        return "WaterContrast";
    case ProgramParameterType::WaterLevelThreshold:
        return "WaterLevelThreshold";
    // Textures
    case ProgramParameterType::SharedTexture:
        return "SharedTexture";
    case ProgramParameterType::CloudTexture:
        return "CloudTexture";
    case ProgramParameterType::GenericTexturesAtlasTexture:
        return "GenericTexturesAtlasTexture";
    case ProgramParameterType::LandTexture:
        return "LandTexture";
    case ProgramParameterType::NoiseTexture1:
        return "NoiseTexture1";
    case ProgramParameterType::NoiseTexture2:
        return "NoiseTexture2";
    case ProgramParameterType::OceanTexture:
        return "OceanTexture";
    case ProgramParameterType::WorldBorderTexture:
        return "WorldBorderTexture";
    default:
        assert(false);
        throw GameException("Unsupported ProgramParameterType");
    }
}

VertexAttributeType StrToVertexAttributeType(std::string const & str)
{
    // World
    if (Utils::CaseInsensitiveEquals(str, "Star"))
        return VertexAttributeType::Star;
    else if (Utils::CaseInsensitiveEquals(str, "Cloud"))
        return VertexAttributeType::Cloud;
    else if (Utils::CaseInsensitiveEquals(str, "Land"))
        return VertexAttributeType::Land;
    else if (Utils::CaseInsensitiveEquals(str, "Ocean"))
        return VertexAttributeType::Ocean;
    else if (Utils::CaseInsensitiveEquals(str, "CrossOfLight1"))
        return VertexAttributeType::CrossOfLight1;
    else if (Utils::CaseInsensitiveEquals(str, "CrossOfLight2"))
        return VertexAttributeType::CrossOfLight2;
    else if (Utils::CaseInsensitiveEquals(str, "FireExtinguisherSpray"))
        return VertexAttributeType::FireExtinguisherSpray;
    else if (Utils::CaseInsensitiveEquals(str, "HeatBlasterFlame"))
        return VertexAttributeType::HeatBlasterFlame;
    else if (Utils::CaseInsensitiveEquals(str, "WorldBorder"))
        return VertexAttributeType::WorldBorder;
    // Ship
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointTextureCoordinates"))
        return VertexAttributeType::ShipPointTextureCoordinates;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointColor"))
        return VertexAttributeType::ShipPointColor;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointPosition"))
        return VertexAttributeType::ShipPointPosition;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointWater"))
        return VertexAttributeType::ShipPointWater;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointLight"))
        return VertexAttributeType::ShipPointLight;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointPlaneId"))
        return VertexAttributeType::ShipPointPlaneId;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointDecay"))
        return VertexAttributeType::ShipPointDecay;
    else if (Utils::CaseInsensitiveEquals(str, "ShipPointTemperature"))
        return VertexAttributeType::ShipPointTemperature;
    else if (Utils::CaseInsensitiveEquals(str, "GenericTexture1"))
        return VertexAttributeType::GenericTexture1;
    else if (Utils::CaseInsensitiveEquals(str, "GenericTexture2"))
        return VertexAttributeType::GenericTexture2;
    else if (Utils::CaseInsensitiveEquals(str, "GenericTexture3"))
        return VertexAttributeType::GenericTexture3;
    else if (Utils::CaseInsensitiveEquals(str, "Flame1"))
        return VertexAttributeType::Flame1;
    else if (Utils::CaseInsensitiveEquals(str, "Flame2"))
        return VertexAttributeType::Flame2;
    else if (Utils::CaseInsensitiveEquals(str, "VectorArrow"))
        return VertexAttributeType::VectorArrow;
    // Text
    else if (Utils::CaseInsensitiveEquals(str, "Text1"))
        return VertexAttributeType::Text1;
    else if (Utils::CaseInsensitiveEquals(str, "Text2"))
        return VertexAttributeType::Text2;
    else
        throw GameException("Unrecognized vertex attribute \"" + str + "\"");
}

}