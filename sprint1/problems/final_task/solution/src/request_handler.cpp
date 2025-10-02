#include "request_handler.h"

namespace http_handler {

json::object RequestHandler::MakeRoadJson(const model::Road& road) {
    json::object j;
    j["x0"] = road.GetStart().x;
    j["y0"] = road.GetStart().y;
    if (road.IsHorizontal()) j["x1"] = road.GetEnd().x;
    else j["y1"] = road.GetEnd().y;
    return j;
}

json::object RequestHandler::MakeBuildingJson(const model::Building& building) {
    auto b = building.GetBounds();
    return {
        {"x", b.position.x},
        {"y", b.position.y},
        {"w", b.size.width},
        {"h", b.size.height}
    };
}

json::object RequestHandler::MakeOfficeJson(const model::Office& o) {
    return {
        {"id", *o.GetId()},
        {"x", o.GetPosition().x},
        {"y", o.GetPosition().y},
        {"offsetX", o.GetOffset().dx},
        {"offsetY", o.GetOffset().dy}
    };
}

json::object RequestHandler::MakeMapJson(const model::Map& map) {
    json::object j;
    j["id"] = *map.GetId();
    j["name"] = map.GetName();

    j["roads"] = json::array();
    auto& roads = j["roads"].as_array();
    for (const auto& r : map.GetRoads()) {
        roads.push_back(MakeRoadJson(r));
    }

    j["buildings"] = json::array();
    auto& buildings = j["buildings"].as_array();
    for (const auto& b : map.GetBuildings()) {
        buildings.push_back(MakeBuildingJson(b));
    }

    j["offices"] = json::array();
    auto& offices = j["offices"].as_array();
    for (const auto& o : map.GetOffices()) {
        offices.push_back(MakeOfficeJson(o));
    }

    return j;
}

json::array RequestHandler::MakeMapsJson() {
    json::array arr;
    for (const auto& m : game_.GetMaps()) {
        json::object obj;
        obj["id"] = *m.GetId();
        obj["name"] = m.GetName();

        arr.push_back(obj);
    }

    return arr;
}

}  // namespace http_handler
