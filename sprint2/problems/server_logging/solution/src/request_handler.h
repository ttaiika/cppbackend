#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <filesystem>
#include <optional>
#include <sstream>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::optional<fs::path> root = std::nullopt)
        : game_{game}
        , root_{std::move(root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    struct ContentType {
        ContentType() = delete;
        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    };

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::literals;

        std::string target = std::string(req.target());

        // Обработка REST API
        if (target.starts_with("/api/")) {
            if (target == "/api/v1/maps") {
                return HandleMaps(req, send);
            }

            constexpr std::string_view prefix = "/api/v1/maps/";
            if (target.starts_with(prefix)) {
                model::Map::Id id{target.substr(prefix.size())};
                return HandleMapById(req, send, id);
            }

            return SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        }

        // Раздача статических файлов
        if (!root_) {
            return SendTextResponse(req, send, http::status::not_found, "Static files not available");
        }   

        // URL-декодирование
        std::string decoded_uri = URLDecode(target);

        // Если URI пустой или оканчивается на '/', добавляем index.html
        if (decoded_uri.empty() || decoded_uri.back() == '/') {
            decoded_uri += "index.html";
        }

        // Убираем ведущий '/' — иначе filesystem воспримет путь как абсолютный
        if (!decoded_uri.empty() && decoded_uri.front() == '/') {
            decoded_uri.erase(0, 1);
        }

        // Абсолютный путь к корню и файлу
        fs::path root_path = fs::weakly_canonical(*root_);
        fs::path file_path;

        try {
            file_path = fs::weakly_canonical(root_path / decoded_uri);
        } catch (...) {
            return SendTextResponse(req, send, http::status::bad_request, "Bad request: invalid path");
        }

        // Проверка, что файл находится внутри root
        if (!IsSubPath(file_path, root_path)) {
            return SendTextResponse(req, send, http::status::bad_request, "Bad request: path outside root");
        }

        // Если файл не существует
        if (!fs::exists(file_path)) {
            return SendTextResponse(req, send, http::status::not_found, "File not found");
        }

        // Если путь ведёт к каталогу — ищем index.html
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
            if (!fs::exists(file_path)) {
                return SendTextResponse(req, send, http::status::not_found, "File not found");
            }
        }

        // Читаем файл
        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            return SendTextResponse(req, send, http::status::internal_server_error, "Failed to read file");
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string body = oss.str();

        std::string mime = GetMimeType(file_path.extension().string());

        // Отправляем ответ (GET или HEAD)
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.body() = (req.method() == http::verb::head ? "" : body);
        res.set(http::field::content_type, mime);
        res.set(http::field::content_length, std::to_string(body.size()));
        res.keep_alive(req.keep_alive());

        send(std::move(res));
    }

private:
    bool IsSubPath(fs::path path, fs::path base);
    std::string URLDecode(const std::string& str) const;

    template <typename Body, typename Send>
    void HandleMaps(http::request<Body> const& req, Send&& send) {
        SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapsJson()));
    }

    template <typename Body, typename Send>
    void HandleMapById(http::request<Body> const& req, Send&& send, model::Map::Id id) {
        // если id не было
        if ((*id).empty()) {
            // Создаем объект ответа с ошибкой
            return SendJsonResponse(req, send, http::status::bad_request, R"({"code":"badRequest","message":"Bad request"})");
        }

        const model::Map* map = game_.FindMap(id);

        //если существует карта с таким id
        if (map != nullptr) {
            return SendJsonResponse(req, send, http::status::ok, json::serialize(MakeMapJson(*map)));
        }
        return SendJsonResponse(req, send, http::status::not_found, R"({"code":"mapNotFound","message":"Map not found"})");
    }

    json::object MakeRoadJson(const model::Road& road);
    json::object MakeBuildingJson(const model::Building& b);
    json::object MakeOfficeJson(const model::Office& o);
    json::object MakeMapJson(const model::Map& map);
    json::array MakeMapsJson();

    template <typename Body, typename Send>
    void SendJsonResponse(http::request<Body> const& req, Send&& send, http::status status, const std::string& body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, ContentType::APPLICATION_JSON);
        res.keep_alive(req.keep_alive());
        res.body() = body;
        res.prepare_payload();
        send(std::move(res));
    }

    template <typename Body, typename Send>
    void SendTextResponse(http::request<Body> const& req, Send&& send, http::status status, const std::string& body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, ContentType::TEXT_PLAIN);
        res.keep_alive(req.keep_alive());
        res.body() = body;
        res.prepare_payload();
        send(std::move(res));
    }

    std::string GetMimeType(const std::string& ext) const {
        static const std::unordered_map<std::string, std::string> mime_map{
            {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"},
            {".txt", "text/plain"}, {".js", "text/javascript"}, {".json", "application/json"},
            {".xml", "application/xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"}, {".jpe", "image/jpeg"}, {".gif", "image/gif"},
            {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
            {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
            {".mp3", "audio/mpeg"}
        };
        std::string lower_ext;
        for (auto c : ext) lower_ext += std::tolower(c);

        auto it = mime_map.find(lower_ext);
        if (it != mime_map.end()) return it->second;
        return "application/octet-stream";
    }

    model::Game& game_;
    std::optional<fs::path> root_;
};

}  // namespace http_handler
