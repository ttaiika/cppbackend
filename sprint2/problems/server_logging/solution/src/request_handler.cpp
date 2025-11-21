#include "request_handler.h"

#include <cctype>  // для isxdigit

namespace http_handler {

// Возвращает true, если каталог p содержится внутри base_path.
bool RequestHandler::IsSubPath(fs::path path, fs::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

// Функция для перевода hex-пары в символ
char FromHex(const std::string& hex) {
    unsigned int value;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> value;
    return static_cast<char>(value);
}

std::string RequestHandler::URLDecode(const std::string& str) const {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i+2 < str.size() && std::isxdigit(str[i+1]) && std::isxdigit(str[i+2])) {
                std::string hex = str.substr(i+1, 2);
                result += FromHex(hex);
                i+=2;
            } else {
                result+= '%';
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

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
