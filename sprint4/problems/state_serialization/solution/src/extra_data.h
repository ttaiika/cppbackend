#pragma once

#include <boost/json.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace extra_data {

class MapExtraData {
public:
    using LootTypes = std::vector<boost::json::value>;
    
    void SetLootTypes(const std::string& map_id, LootTypes loot_types) {
        loot_types_by_map_[map_id] = std::move(loot_types);
    }
    
    const LootTypes* GetLootTypes(const std::string& map_id) const {
        auto it = loot_types_by_map_.find(map_id);
        return it != loot_types_by_map_.end() ? &it->second : nullptr;
    }

    int GetLootValue(const std::string& map_id, size_t type_index) const {
        const auto* loot_types = GetLootTypes(map_id);
        if (!loot_types || type_index >= loot_types->size()) {
            // Возвращаем значение по умолчанию, если тип не найден
            return 0;
        }
        
        try {
            const auto& loot_json = (*loot_types)[type_index];
            if (!loot_json.is_object()) {
                return 0;
            }
            
            const auto& obj = loot_json.as_object();
            if (obj.contains("value")) {
                return static_cast<int>(obj.at("value").as_int64());
            }
        } catch (...) {
            // В случае ошибки парсинга возвращаем 0
        }
        
        return 0;
    }
    
private:
    std::unordered_map<std::string, LootTypes> loot_types_by_map_;
};

}  // namespace extra_data