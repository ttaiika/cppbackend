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
    
private:
    std::unordered_map<std::string, LootTypes> loot_types_by_map_;
};

}  // namespace extra_data