#pragma once

#include "EmpireGeneration.h"

namespace Birthright::Campaign
{
struct GeographyDocument
{
    MapGeometry Geometry;
    std::vector<GeographicArea> Areas;
    std::vector<TraversalConnection> Connections;
};
// Adds only missing water records; land must already be provided by generation or a save.
void MaterializeWaterAreas(Snapshot& state);
Result ValidateGeometry(const MapGeometry& geometry);
bool PointInPolygon(const std::vector<MapPoint>& polygon, const MapPoint& point);
Id AreaAtMapPoint(const Snapshot& state, const MapPoint& point);
Result ReadGeometry(std::istream& input, MapGeometry& output);
Result WriteGeometry(const MapGeometry& geometry, std::ostream& output);
Result LoadGeography(std::istream& input, GeographyDocument& output);
Result GenerateImportedWorld(const GeographyDocument& document, EmpireGeneration options, const Rules& rules, Snapshot& output);
Result ExportGeographySvg(const Snapshot& state, std::ostream& output);
}
