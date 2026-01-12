#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <filesystem>
#include <iostream>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&t_c, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%F %T"); // %F = YYYY-MM-DD
        return oss.str();
    }

    // Для имени файла возьмем дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_r(&t_c, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y_%m_%d");
        return oss.str();
    }

    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Основной метод логирования
    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Проверяем дату — если изменилась, открываем новый файл
        const std::string current_file_date = GetFileTimeStamp();
        if (current_file_date != current_date_) {
            OpenLogFile(current_file_date);
        }

        if (!ofs_.is_open()) {
            std::cerr << "Logger: unable to open log file\n";
            return;
        }

        ofs_ << GetTimeStamp() << ": ";
        (ofs_ << ... << args);
        ofs_ << std::endl;
    }

    // Установка пользовательского времени
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
        // Проверим, нужно ли сменить файл при смене даты
        const std::string current_file_date = GetFileTimeStamp();
        if (current_file_date != current_date_) {
            OpenLogFile(current_file_date);
        }
    }

private:
    void OpenLogFile(const std::string& date_str) {
        // Формируем имя файла
        const std::string filename = "/var/log/sample_log_" + date_str + ".log";
        std::filesystem::create_directories("/var/log");

        ofs_.close();
        ofs_.open(filename, std::ios::app);
        current_date_ = date_str;
    }

    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream ofs_;
    std::string current_date_;
    std::mutex mutex_;
};