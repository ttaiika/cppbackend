#pragma once

#include "app/application_listener.h"
#include "serialization/model_serialization.h"
#include <filesystem>
#include <chrono>

namespace infrastructure {

class SerializingListener : public ApplicationListener {
public:
    SerializingListener(model::Game& game, 
                       const std::filesystem::path& state_file_path,
                       std::chrono::milliseconds save_period = std::chrono::milliseconds(0));
    
    ~SerializingListener() = default;
    
    // Реализация интерфейса ApplicationListener
    void OnTick(std::chrono::milliseconds delta) override;
    
    // Сохранение при завершении работы
    void SaveFinalState();
    
private:
    void SaveState();
    void LoadState();
    
    model::Game& game_;
    std::filesystem::path state_file_path_;
    std::chrono::milliseconds save_period_;
    std::chrono::milliseconds time_since_last_save_{0};
};

} // namespace infrastructure