#pragma once
#include "http_server.h"
#include "content_type.h"
#include <filesystem>
#include <optional>
#include <sstream>
#include <fstream>
#include <iostream>

#include <boost/json.hpp>

namespace fs = std::filesystem;

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class StaticHandler {
public:
    explicit StaticHandler(std::optional<fs::path> root = std::nullopt)
        : root_(std::move(root)) {}

    template <typename Body, typename Allocator, typename Send>
    bool Handle(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (!root_) {
            SendTextResponse(req, send, http::status::not_found, "Static files not available");
            return true;
        }

        std::string target = std::string(req.target());

        std::string decoded = URLDecode(target);

        // Если URI пустой или оканчивается на '/', добавляем index.html
        if (decoded.empty() || decoded.back() == '/')
            decoded += "index.html";

        // Убираем ведущий '/' — иначе filesystem воспримет путь как абсолютный
        if (!decoded.empty() && decoded.front() == '/')
            decoded.erase(0, 1);

        fs::path root_path;
        fs::path file_path;

        try {
            root_path = fs::weakly_canonical(*root_);
            file_path = fs::weakly_canonical(root_path / decoded);
        } catch (...) {
            SendTextResponse(req, send, http::status::bad_request, "Bad request: invalid path");
            return true;
        }

        // Проверка, что файл находится внутри root
        if (!IsSubPath(file_path, root_path)) {
            SendTextResponse(req, send, http::status::bad_request, "Bad request: path outside root");
            return true;
        }

        // Если файл не существует
        if (!fs::exists(file_path)) {
            SendTextResponse(req, send, http::status::not_found, "File not found");
            return true;
        }

        // Если путь ведёт к каталогу — ищем index.html
        if (fs::is_directory(file_path)) {
            file_path /= "index.html";
            if (!fs::exists(file_path)) {
                SendTextResponse(req, send, http::status::not_found, "File not found");
                return true;
            }
        }

        // Читаем файл
        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            SendTextResponse(req, send, http::status::internal_server_error, "Failed to read file");
            return true;
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string body = oss.str();

        std::string ext = file_path.extension().string();
        std::string mime = GetMimeType(ext);

        // Отправляем ответ (GET или HEAD)
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.body() = (req.method() == http::verb::head ? "" : body);
        res.set(http::field::content_type, mime);
        res.set(http::field::content_length, std::to_string(body.size()));
        res.keep_alive(req.keep_alive());

        send(std::move(res));
        return true;
    }

private:
    std::optional<fs::path> root_;

    bool IsSubPath(fs::path path, fs::path base);

    std::string URLDecode(const std::string& str) const;
    
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
};