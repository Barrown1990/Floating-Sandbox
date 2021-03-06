/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-04-22
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include "GameParameters.h"
#include "MaterialDatabase.h"
#include "Physics.h"
#include "ShipDefinition.h"

#include <GameCore/FixedSizeVector.h>
#include <GameCore/GameTypes.h>
#include <GameCore/ImageSize.h>
#include <GameCore/TaskThreadPool.h>

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

/*
 * This class contains all the logic for building a ship out of a ShipDefinition.
 */
class ShipBuilder
{
public:

    static std::unique_ptr<Physics::Ship> Create(
        ShipId shipId,
        Physics::World & parentWorld,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        std::shared_ptr<TaskThreadPool> taskThreadPool,
        ShipDefinition const & shipDefinition,
        MaterialDatabase const & materialDatabase,
        GameParameters const & gameParameters);

private:

    struct PointInfo
    {
        vec2f Position;
        vec2f TextureCoordinates;
        vec4f RenderColor;
        StructuralMaterial const & StructuralMtl;
        bool IsRope;
        bool IsLeaking;

        ElectricalMaterial const * ElectricalMtl;
        ElectricalElementInstanceIndex ElectricalElementInstanceIndex;
        std::vector<ElementIndex> ConnectedSprings1;

        PointInfo(
            vec2f position,
            vec2f textureCoordinates,
            vec4f renderColor,
            StructuralMaterial const & structuralMtl,
            bool isRope)
            : Position(position)
            , TextureCoordinates(textureCoordinates)
            , RenderColor(renderColor)
            , StructuralMtl(structuralMtl)
            , IsRope(isRope)
            , IsLeaking(isRope ? true : false) // Ropes leak by default
            , ElectricalMtl(nullptr)
            , ElectricalElementInstanceIndex(NoneElectricalElementInstanceIndex)
            , ConnectedSprings1()
        {
        }

        bool ContainsConnectedSpring(ElementIndex springIndex1)
        {
            return std::find(
                ConnectedSprings1.cbegin(),
                ConnectedSprings1.cend(),
                springIndex1)
                != ConnectedSprings1.cend();
        }

        void AddConnectedSpring(ElementIndex springIndex1)
        {
            assert(!ContainsConnectedSpring(springIndex1));
            ConnectedSprings1.push_back(springIndex1);
        }
    };

    struct RopeSegment
    {
        ElementIndex PointAIndex1;
        ElementIndex PointBIndex1;

        MaterialDatabase::ColorKey RopeColorKey;

        RopeSegment()
            : PointAIndex1(NoneElementIndex)
            , PointBIndex1(NoneElementIndex)
            , RopeColorKey()
        {
        }

        bool SetEndpoint(
            ElementIndex pointIndex1,
            MaterialDatabase::ColorKey ropeColorKey)
        {
            if (NoneElementIndex == PointAIndex1)
            {
                PointAIndex1 = pointIndex1;
                RopeColorKey = ropeColorKey;
                return true;
            }
            else if (NoneElementIndex == PointBIndex1)
            {
                PointBIndex1 = pointIndex1;
                assert(RopeColorKey == ropeColorKey);
                return true;
            }
            else
            {
                // Too many
                return false;
            }
        }
    };

    struct SpringInfo
    {
        ElementIndex PointAIndex1;
        uint32_t PointAAngle;

        ElementIndex PointBIndex1;
        uint32_t PointBAngle;

        FixedSizeVector<ElementIndex, 2> SuperTriangles2;

        SpringInfo(
            ElementIndex pointAIndex1,
            uint32_t pointAAngle,
            ElementIndex pointBIndex1,
            uint32_t pointBAngle)
            : PointAIndex1(pointAIndex1)
            , PointAAngle(pointAAngle)
            , PointBIndex1(pointBIndex1)
            , PointBAngle(pointBAngle)
            , SuperTriangles2()
        {
        }
    };

    struct TriangleInfo
    {
        std::array<ElementIndex, 3> PointIndices1;

        FixedSizeVector<ElementIndex, 4> SubSprings2;

        TriangleInfo(
            std::array<ElementIndex, 3> const & pointIndices1)
            : PointIndices1(pointIndices1)
            , SubSprings2()
        {
        }
    };

private:

    /////////////////////////////////////////////////////////////////
    // Building helpers
    /////////////////////////////////////////////////////////////////

    struct Edge
    {
        ElementIndex Endpoint1Index;
        ElementIndex Endpoint2Index;

        Edge(
            ElementIndex endpoint1Index,
            ElementIndex endpoint2Index)
            : Endpoint1Index(std::min(endpoint1Index, endpoint2Index))
            , Endpoint2Index(std::max(endpoint1Index, endpoint2Index))
        {}

        bool operator==(Edge const & other) const
        {
            return this->Endpoint1Index == other.Endpoint1Index
                && this->Endpoint2Index == other.Endpoint2Index;
        }

        struct Hasher
        {
            size_t operator()(Edge const & edge) const
            {
                return edge.Endpoint1Index * 23
                    + edge.Endpoint2Index;
            }
        };
    };

    static bool IsConnectedToNonRopePoints(
        ElementIndex pointIndex,
        Physics::Points const & points,
        std::vector<ElementIndex> const & pointIndexRemap,
        std::vector<SpringInfo> const & springInfos)
    {
        for (auto cs : points.GetConnectedSprings(pointIndex).ConnectedSprings)
        {
            if (!points.IsRope(pointIndexRemap[springInfos[cs.SpringIndex].PointAIndex1])
                || !points.IsRope(pointIndexRemap[springInfos[cs.SpringIndex].PointBIndex1]))
            {
                return true;
            }
        }

        return false;
    }

    template <typename CoordType>
    static vec2f MakeTextureCoordinates(
        CoordType x,
        CoordType y,
        ImageSize const & imageSize)
    {
        float const textureDx = 0.5f / static_cast<float>(imageSize.Width);
        float const textureDy = 0.5f / static_cast<float>(imageSize.Height);

        return vec2f(
            textureDx + static_cast<float>(x) / static_cast<float>(imageSize.Width),
            textureDy + static_cast<float>(y) / static_cast<float>(imageSize.Height));
    }

    static void AppendRopeEndpoints(
        RgbImageData const & ropeLayerImage,
        std::map<MaterialDatabase::ColorKey, RopeSegment> & ropeSegments,
        std::vector<PointInfo> & pointInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> & pointIndexMatrix,
        MaterialDatabase const & materialDatabase,
        vec2f const & shipOffset);

    static void DecoratePointsWithElectricalMaterials(
        RgbImageData const & layerImage,
        std::vector<PointInfo> & pointInfos1,
        bool isDedicatedElectricalLayer,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        MaterialDatabase const & materialDatabase);

    static void AppendRopes(
        std::map<MaterialDatabase::ColorKey, RopeSegment> const & ropeSegments,
        ImageSize const & structureImageSize,
        StructuralMaterial const & ropeMaterial,
        std::vector<PointInfo> & pointInfos1,
        std::vector<SpringInfo> & springInfos1);

    static void CreateShipElementInfos(
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize,
        std::vector<PointInfo> & pointInfos1,
        std::vector<SpringInfo> & springInfos1,
        std::vector<TriangleInfo> & triangleInfos1,
        size_t & leakingPointsCount);

    static Physics::Points CreatePoints(
        std::vector<PointInfo> const & pointInfos2,
        Physics::World & parentWorld,
        MaterialDatabase const & materialDatabase,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        GameParameters const & gameParameters,
        std::vector<ElectricalElementInstanceIndex> & electricalElementInstanceIndices);

    static std::vector<TriangleInfo> FilterOutRedundantTriangles(
        std::vector<TriangleInfo> const & triangleInfos2,
        Physics::Points const & points,
        std::vector<ElementIndex> const & pointIndexRemap,
        std::vector<SpringInfo> const & springInfos2);

    static void ConnectSpringsAndTriangles(
        std::vector<SpringInfo> & springInfos2,
        std::vector<TriangleInfo> & triangleInfos2);

    static Physics::Springs CreateSprings(
        std::vector<SpringInfo> const & springInfos2,
        Physics::Points & points,
        std::vector<ElementIndex> const & pointIndexRemap,
        Physics::World & parentWorld,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        GameParameters const & gameParameters);

    static Physics::Triangles CreateTriangles(
        std::vector<TriangleInfo> const & triangleInfos2,
        Physics::Points & points,
        std::vector<ElementIndex> const & pointIndexRemap);

    static Physics::ElectricalElements CreateElectricalElements(
        Physics::Points const & points,
        std::vector<ElectricalElementInstanceIndex> const & electricalElementInstanceIndices,
        std::map<ElectricalElementInstanceIndex, ElectricalPanelElementMetadata> const & panelMetadata,
        ShipId shipId,
        Physics::World & parentWorld,
        std::shared_ptr<GameEventDispatcher> gameEventDispatcher,
        GameParameters const & gameParameters);

private:

    using ReorderingResults = std::tuple<std::vector<PointInfo>, std::vector<ElementIndex>, std::vector<SpringInfo>>;

    //
    // Reordering
    //

    template <int StripeLength>
    static ReorderingResults ReorderPointsAndSpringsOptimally_Stripes(
        std::vector<PointInfo> const & pointInfos1,
        std::vector<SpringInfo> const & springInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize);

    template <int StripeLength>
    static void ReorderPointsAndSpringsOptimally_Stripes_Stripe(
        int y,
        std::vector<PointInfo> const & pointInfos1,
        std::vector<bool> & reorderedPointInfos1,
        std::vector<SpringInfo> const & springInfos1,
        std::vector<bool> & reorderedSpringInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize,
        std::unordered_map<Edge, ElementIndex, Edge::Hasher> const & edgeToSpringIndex1Map,
        std::vector<PointInfo> & pointInfos2,
        std::vector<ElementIndex> & pointIndexRemap,
        std::vector<SpringInfo> & springInfos2);

    static ReorderingResults ReorderPointsAndSpringsOptimally_Blocks(
        std::vector<PointInfo> const & pointInfos1,
        std::vector<SpringInfo> const & springInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize);

    static void ReorderPointsAndSpringsOptimally_Blocks_Row(
        int y,
        std::vector<PointInfo> const & pointInfos1,
        std::vector<bool> & reorderedPointInfos1,
        std::vector<SpringInfo> const & springInfos1,
        std::vector<bool> & reorderedSpringInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize,
        std::unordered_map<Edge, ElementIndex, Edge::Hasher> const & edgeToSpringIndex1Map,
        std::vector<PointInfo> & pointInfos2,
        std::vector<ElementIndex> & pointIndexRemap,
        std::vector<SpringInfo> & springInfos2);

    template <int BlockSize>
    static ReorderingResults ReorderPointsAndSpringsOptimally_Tiling(
        std::vector<PointInfo> const & pointInfos1,
        std::vector<SpringInfo> const & springInfos1,
        std::unique_ptr<std::unique_ptr<std::optional<ElementIndex>[]>[]> const & pointIndexMatrix,
        ImageSize const & structureImageSize);

    static std::vector<SpringInfo> ReorderSpringsOptimally_TomForsyth(
        std::vector<SpringInfo> const & springInfos1,
        size_t pointCount);

    static std::vector<TriangleInfo> ReorderTrianglesOptimally_ReuseOptimization(
        std::vector<TriangleInfo> const & triangleInfos1,
        size_t pointCount);

    static std::vector<TriangleInfo> ReorderTrianglesOptimally_TomForsyth(
        std::vector<TriangleInfo> const & triangleInfos1,
        size_t pointCount);

    static float CalculateACMR(std::vector<SpringInfo> const & springInfos);

    static float CalculateACMR(std::vector<TriangleInfo> const & triangleInfos);

    static float CalculateVertexMissRatio(std::vector<TriangleInfo> const & triangleInfos);

private:

    /////////////////////////////////////////////////////////////////
    // Vertex cache optimization
    /////////////////////////////////////////////////////////////////

    // See Tom Forsyth's comments: using 32 is good enough; apparently 64 does not yield significant differences
    static constexpr size_t VertexCacheSize = 32;

    using ModelLRUVertexCache = std::list<size_t>;

    struct VertexData
    {
        int32_t CachePosition;                          // Position in cache; -1 if not in cache
        float CurrentScore;                             // Current score of the vertex
        std::vector<size_t> RemainingElementIndices;    // Indices of not yet drawn elements that use this vertex

        VertexData()
            : CachePosition(-1)
            , CurrentScore(0.0f)
            , RemainingElementIndices()
        {
        }
    };

    struct ElementData
    {
        bool HasBeenDrawn;                  // Set to true when the element has been drawn already
        float CurrentScore;                 // Current score of the element - sum of its vertices' scores
        std::vector<size_t> VertexIndices;  // Indices of vertices in this element

        ElementData()
            : HasBeenDrawn(false)
            , CurrentScore(0.0f)
            , VertexIndices()
        {
        }
    };

    template <size_t VerticesInElement>
    static std::vector<size_t> ReorderOptimally(
        std::vector<VertexData> & vertexData,
        std::vector<ElementData> & elementData);


    static void AddVertexToCache(
        size_t vertexIndex,
        ModelLRUVertexCache & cache);

    template <size_t VerticesInElement>
    static float CalculateVertexScore(VertexData const & vertexData);

    template <size_t Size>
    class TestLRUVertexCache
    {
    public:

        bool UseVertex(size_t vertexIndex);

        std::optional<size_t> GetCachePosition(size_t vertexIndex);

    private:

        std::list<size_t> mEntries;
    };
};
