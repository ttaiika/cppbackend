#include "infrastructure/serializing_listener.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace infrastructure {

SerializingListener::SerializingListener(model::Game& game,
                                       const std::filesystem::path& state_file_path,
                                       std::chrono::milliseconds save_period)
    : game_(game)
    , state_file_path_(state_file_path)
    , save_period_(save_period) {
    
    // Восстанавливаем состояние при запуске, если файл существует
    if (std::filesystem::exists(state_file_path_)) {
        LoadState();
    }
}

void SerializingListener::OnTick(std::chrono::milliseconds delta) {
    if (save_period_.count() == 0) return;
    
    time_since_last_save_ += delta;
    if (time_since_last_save_ >= save_period_) {
        SaveState();
        time_since_last_save_ = std::chrono::milliseconds(0);
    }
}

void SerializingListener::SaveFinalState() {
    SaveState();
}

void SerializingListener::SaveState() {
    // Безопасное сохранение через временный файл
    std::filesystem::path temp_path = state_file_path_;
    temp_path += ".tmp";
    
    // Сохраняем во временный файл
    {
        std::ofstream ofs(temp_path);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot open state file for writing");
        }
        
        boost::archive::text_oarchive oa(ofs);
        
        // Создаем представление состояния с использованием model_serialization
        serialization::GameStateRepr game_state_repr(game_, game_.GetPlayers());
        oa << game_state_repr;
    }
    
    // Атомарно заменяем старый файл новым
    std::filesystem::rename(temp_path, state_file_path_);
}

void SerializingListener::LoadState() {
    std::ifstream ifs(state_file_path_);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open state file for reading");
    }
    
    try {
        boost::archive::text_iarchive ia(ifs);
        
        // Загружаем представление состояния
        serialization::GameStateRepr game_state_repr;
        ia >> game_state_repr;
        
        // Восстанавливаем состояние игры
        game_state_repr.Restore(game_, game_.GetPlayers());
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to restore state: " + std::string(e.what()));
    }
}

} // namespace infrastructure