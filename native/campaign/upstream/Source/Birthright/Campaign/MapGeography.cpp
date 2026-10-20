#include "MapGeography.h"
#include "WorldTraversal.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <set>

namespace Birthright::Campaign
{
namespace
{
Result Ok() { return {true, "accepted", ""}; }
Result Bad(const std::string& detail) { return {false, "geography_invalid", detail}; }
bool Text(const std::string& value) { return !value.empty() && value.size() <= 1024; }
bool Bounded(double value, double low, double high) { return std::isfinite(value) && value >= low && value <= high; }
double Cross(MapPoint a, MapPoint b, MapPoint c) { return (b.X-a.X)*(c.Y-a.Y)-(b.Y-a.Y)*(c.X-a.X); }
double PolygonArea(const std::vector<MapPoint>& polygon)
{
    double sum = 0;
    for (std::size_t n = 0; n < polygon.size(); ++n)
    {
        const auto& a = polygon[n]; const auto& b = polygon[(n+1)%polygon.size()];
        sum += a.X*b.Y-b.X*a.Y;
    }
    return std::abs(sum)*0.5;
}
double Median(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    return values.empty() ? 1 : values[values.size()/2];
}
const char* TerrainColor(std::uint32_t biome)
{
    static const char* colors[] = {"#315c79","#d8c188","#c4b891","#aeb574","#98b777","#507e55","#688f59",
        "#386b56","#517967","#6c876b","#a3a893","#dce4e4","#6a8f89"};
    return colors[biome];
}
std::string Xml(const std::string& value)
{
    std::string out;
    for (const auto c : value)
    {
        if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;"; else if (c == '"') out += "&quot;"; else out += c;
    }
    return out;
}
}
bool PointInPolygon(const std::vector<MapPoint>& polygon, const MapPoint& point)
{
    if (polygon.size() < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size()-1; i < polygon.size(); j=i++)
    {
        const auto a = polygon[j], b = polygon[i];
        if (std::abs(Cross(a,b,point)) <= 0.00000001 &&
            point.X >= std::min(a.X,b.X)-0.00000001 && point.X <= std::max(a.X,b.X)+0.00000001 &&
            point.Y >= std::min(a.Y,b.Y)-0.00000001 && point.Y <= std::max(a.Y,b.Y)+0.00000001) return true;
        if ((a.Y > point.Y) != (b.Y > point.Y) &&
            point.X < (b.X-a.X)*(point.Y-a.Y)/(b.Y-a.Y)+a.X) inside = !inside;
    }
    return inside;
}
Result ValidateGeometry(const MapGeometry& geometry)
{
    if (geometry.Version != 1) return Bad("Unknown map geometry version.");
    if (geometry.Cells.empty()) return Ok(); // Existing grid maps have no polygon backdrop.
    if (!Text(geometry.Name) || !Text(geometry.SourceVersion) || !Text(geometry.SourceSeed) || !Text(geometry.DistanceUnit) ||
        !Bounded(geometry.Width, 1, 1000000) || !Bounded(geometry.Height, 1, 1000000) ||
        !Bounded(geometry.DistanceScale, 0.000001, 1000000) || geometry.Cells.size() > 20000)
        return Bad("Invalid dimensions, metadata or geometry count (maximum 20000).");
    for (const auto& item : geometry.Cells)
    {
        const auto& cell = item.second;
        if (!cell.Key || cell.Key != item.first || cell.Height > 100 || cell.Biome > 12 ||
            cell.Polygon.size() < 3 || cell.Polygon.size() > 64 ||
            !Bounded(cell.Center.X, 0, geometry.Width) || !Bounded(cell.Center.Y, 0, geometry.Height))
            return Bad("Invalid cell metadata or polygon: " + std::to_string(item.first));
        double direction = 0;
        for (std::size_t n = 0; n < cell.Polygon.size(); ++n)
        {
            const auto a = cell.Polygon[n], b = cell.Polygon[(n+1)%cell.Polygon.size()], c = cell.Polygon[(n+2)%cell.Polygon.size()];
            if (!Bounded(a.X, 0, geometry.Width) || !Bounded(a.Y, 0, geometry.Height) ||
                std::hypot(a.X-b.X,a.Y-b.Y) < 0.000000001) return Bad("Invalid vertex: " + std::to_string(item.first));
            const double cross = Cross(a,b,c);
            if (std::abs(cross) > 0.00000001)
            {
                if (direction && direction*cross < 0) return Bad("Polygon must be convex: " + std::to_string(item.first));
                direction = cross;
            }
        }
        if (!direction || PolygonArea(cell.Polygon) < 0.00000001 || !PointInPolygon(cell.Polygon, cell.Center))
            return Bad("Degenerate polygon or center outside cell: " + std::to_string(item.first));
    }
    return Ok();
}
void MaterializeWaterAreas(Snapshot& state)
{
    for (const auto& item : state.Geometry.Cells)
    {
        if (item.second.Height >= 20 || state.Areas.count(item.first)) continue;
        Area area; area.Key = item.first; area.IsWater = true;
        area.X = static_cast<std::int32_t>(std::round(item.second.Center.X));
        area.Y = static_cast<std::int32_t>(std::round(item.second.Center.Y));
        area.SettlementSuitability = 0; area.PopulationCapacity = 0;
        area.Prosperity = 0; area.Unrest = 0;
        state.Areas.emplace(item.first,std::move(area));
    }
}
Id AreaAtMapPoint(const Snapshot& state, const MapPoint& point)
{
    if (!std::isfinite(point.X) || !std::isfinite(point.Y)) return 0;
    // Stable shared-edge tie: smaller area ID wins. Water areas are excluded from walking selections.
    for (const auto& item : state.Areas)
        if (!item.second.IsWater && state.Geometry.Cells.count(item.first) && PointInPolygon(state.Geometry.Cells.at(item.first).Polygon, point)) return item.first;
    return 0;
}
Result WriteGeometry(const MapGeometry& geometry, std::ostream& output)
{
    const auto valid = ValidateGeometry(geometry); if (!valid) return valid;
    output << std::setprecision(17) << "MAP_GEOMETRY " << geometry.Version << ' ' << std::quoted(geometry.Name) << ' '
        << std::quoted(geometry.SourceVersion) << ' ' << std::quoted(geometry.SourceSeed) << ' ' << std::quoted(geometry.DistanceUnit)
        << ' ' << geometry.Width << ' ' << geometry.Height << ' ' << geometry.DistanceScale << ' ' << geometry.Cells.size() << '\n';
    for (const auto& item : geometry.Cells)
    {
        const auto& cell = item.second;
        output << cell.Key << ' ' << cell.Center.X << ' ' << cell.Center.Y << ' ' << cell.Height << ' ' << cell.Biome << ' ' << cell.Polygon.size();
        for (const auto& point : cell.Polygon) output << ' ' << point.X << ' ' << point.Y;
        output << '\n';
    }
    return output ? Ok() : Bad("Geometry output stream failed.");
}
Result ReadGeometry(std::istream& input, MapGeometry& output)
{
    MapGeometry next; std::string magic; std::size_t count = 0;
    if (!(input >> magic >> next.Version >> std::quoted(next.Name) >> std::quoted(next.SourceVersion) >> std::quoted(next.SourceSeed) >>
        std::quoted(next.DistanceUnit) >> next.Width >> next.Height >> next.DistanceScale >> count) ||
        magic != "MAP_GEOMETRY" || next.Version != 1 || count > 20000) return Bad("Truncated/unsupported geometry header.");
    for (std::size_t n = 0; n < count; ++n)
    {
        MapCell cell; std::size_t vertices = 0;
        if (!(input >> cell.Key >> cell.Center.X >> cell.Center.Y >> cell.Height >> cell.Biome >> vertices) || vertices < 3 || vertices > 64)
            return Bad("Malformed polygon record.");
        for (std::size_t v = 0; v < vertices; ++v)
        {
            MapPoint point; if (!(input >> point.X >> point.Y)) return Bad("Truncated vertices.");
            cell.Polygon.push_back(point);
        }
        if (!next.Cells.emplace(cell.Key,std::move(cell)).second) return Bad("Duplicate geometry ID.");
    }
    const auto result = ValidateGeometry(next); if (result) output = std::move(next); return result;
}
Result LoadGeography(std::istream& input, GeographyDocument& output)
{
    input.imbue(std::locale::classic());
    std::string magic; unsigned version = 0; GeographyDocument next;
    if (!(input >> magic >> version) || magic != "BIRTHRIGHT_GEOGRAPHY" || version != 1) return Bad("Expected BIRTHRIGHT_GEOGRAPHY 1.");
    auto result = ReadGeometry(input,next.Geometry); if (!result) return result;
    std::size_t count = 0;
    if (next.Geometry.Cells.empty() || !(input >> count) || count < 1 || count > 10000) return Bad("Use 1..10000 land cells.");
    std::set<Id> seen; std::vector<double> sizes;
    for (const auto& cell : next.Geometry.Cells) if (cell.second.Height >= 20) sizes.push_back(PolygonArea(cell.second.Polygon));
    const auto typicalArea = Median(sizes);
    for (std::size_t n = 0; n < count; ++n)
    {
        GeographicArea area; std::size_t neighbors = 0;
        if (!(input >> area.Key >> neighbors) || neighbors > 64 || !next.Geometry.Cells.count(area.Key) || !seen.insert(area.Key).second)
            return Bad("Invalid land/neighbor record.");
        const auto& shape = next.Geometry.Cells.at(area.Key);
        if (shape.Height < 20 || area.Key > 1000000) return Bad("The inhabited generation graph must contain only land.");
        area.X = static_cast<std::int32_t>(std::round(shape.Center.X));
        area.Y = static_cast<std::int32_t>(std::round(shape.Center.Y)); // Approximate metadata only; geometry and links are authoritative.
        static constexpr double suitability[] = {0,0.1,0.2,0.55,0.9,0.75,1,0.85,0.9,0.4,0.2,0.02,0.3};
        static constexpr double difficulty[] = {1,2,2,1.2,1,3,3,4,4,3,2,30,5};
        area.DiffusionCost = shape.Height >= 71 ? 30 : (shape.Height >= 50 ? std::max(3.0,difficulty[shape.Biome]) : difficulty[shape.Biome]);
        area.SettlementSuitability = suitability[shape.Biome] * (shape.Height >= 71 ? 0.2 : 1);
        area.PopulationCapacity = static_cast<std::int64_t>(std::clamp(std::floor(10000*PolygonArea(shape.Polygon)/typicalArea),1.0,1000000000.0));
        std::set<Id> links;
        for (std::size_t k = 0; k < neighbors; ++k)
        {
            Id neighbor = 0;
            if (!(input >> neighbor) || neighbor == area.Key || !links.insert(neighbor).second ||
                !next.Geometry.Cells.count(neighbor) || next.Geometry.Cells.at(neighbor).Height < 20)
                return Bad("Invalid land adjacency.");
            area.Neighbors.push_back(neighbor);
        }
        std::sort(area.Neighbors.begin(),area.Neighbors.end()); next.Areas.push_back(std::move(area));
    }
    if (seen.size() != sizes.size()) return Bad("Every land polygon needs exactly one area.");
    std::map<Id,const GeographicArea*> graph;
    for (const auto& area : next.Areas) graph[area.Key] = &area;
    std::vector<double> lengths;
    for (const auto& area : next.Areas)
        for (const auto neighbor : area.Neighbors)
        {
            const auto& reverse = graph.at(neighbor)->Neighbors;
            if (std::find(reverse.begin(),reverse.end(),area.Key) == reverse.end()) return Bad("Asymmetric land adjacency.");
            if (neighbor <= area.Key) continue;
            const auto a = next.Geometry.Cells.at(area.Key).Center, b = next.Geometry.Cells.at(neighbor).Center;
            lengths.push_back(std::hypot(a.X-b.X,a.Y-b.Y));
        }
    const auto typicalLength = Median(lengths);
    std::map<std::pair<Id,Id>,double> roads;
    if (!(input >> count) || count > 1000000) return Bad("Invalid road count.");
    for (std::size_t n = 0; n < count; ++n)
    {
        Id a = 0,b = 0; double quality = 0;
        if (!(input >> a >> b >> quality) || a >= b || !graph.count(a) || !graph.count(b) || !Bounded(quality,0,1) ||
            !roads.emplace(std::make_pair(a,b),quality).second ||
            std::find(graph.at(a)->Neighbors.begin(),graph.at(a)->Neighbors.end(),b) == graph.at(a)->Neighbors.end()) return Bad("Invalid road override.");
    }
    for (const auto& area : next.Areas)
        for (const auto neighbor : area.Neighbors)
        {
            if (neighbor <= area.Key) continue;
            const auto a = next.Geometry.Cells.at(area.Key).Center, b = next.Geometry.Cells.at(neighbor).Center;
            const auto link = roads.find({area.Key,neighbor});
            next.Connections.push_back({area.Key,neighbor,link == roads.end() ? 0 : link->second,
                std::clamp(std::hypot(a.X-b.X,a.Y-b.Y)/typicalLength,0.01,1000.0)});
        }
    input >> std::ws; if (!input.eof()) return Bad("Unexpected trailing geography data.");
    output = std::move(next); return Ok();
}
Result GenerateImportedWorld(const GeographyDocument& document, EmpireGeneration options, const Rules& rules, Snapshot& output)
{
    const auto valid = ValidateGeometry(document.Geometry); if (!valid) return valid;
    options.Geometry = document.Geometry; options.Connections = document.Connections; options.AllowDisconnected = true;
    return GenerateEmpires(document.Areas,options,rules,output);
}
Result ExportGeographySvg(const Snapshot& state, std::ostream& output)
{
    const auto valid = Validate(state); if (!valid) return valid;
    if (state.Geometry.Cells.empty()) return Bad("No imported polygon geometry.");
    output << std::setprecision(12) << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << state.Geometry.Width << ' ' << state.Geometry.Height
        << "\"><rect width=\"100%\" height=\"100%\" fill=\"#315c79\"/><title>" << Xml(state.Geometry.Name) << "</title>";
    for (const auto& item : state.Geometry.Cells)
    {
        const auto& shape = item.second;
        output << "<polygon points=\"";
        for (const auto& point : shape.Polygon) output << point.X << ',' << point.Y << ' ';
        output << "\" fill=\"" << TerrainColor(shape.Height < 20 ? 0 : shape.Biome) << "\" stroke=\"#243b35\" stroke-width=\"0.25\"><title>Cell " << item.first
            << "; elevation " << shape.Height << "; biome " << shape.Biome;
        if (state.Areas.count(item.first))
        {
            const auto& area = state.Areas.at(item.first);
            std::int64_t population = 0; for (const auto& cohort : area.Cohorts) population += cohort.Population;
            output << "; region " << area.RegionId << "; population " << population << "; terrain effort " << area.DiffusionCost;
        }
        output << "</title></polygon>";
    }
    for (const auto& item : state.Connections)
        if (item.second.RoadQuality > 0)
        {
            const auto a = state.Geometry.Cells.at(item.first.first).Center, b = state.Geometry.Cells.at(item.first.second).Center;
            output << "<path d=\"M" << a.X << ',' << a.Y << "L" << b.X << ',' << b.Y << "\" stroke=\"#c4b39a\" stroke-width=\""
                << (item.second.RoadQuality >= 0.8 ? 1.2 : 0.5) << "\" opacity=\"0.6\"/>";
        }
    output << "</svg>";
    return output ? Ok() : Bad("SVG output stream failed.");
}
}
