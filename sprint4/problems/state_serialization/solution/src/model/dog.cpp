#include "model/dog.h"

namespace model {

bool Dog::TryAddToBag(size_t item_id, int type, int value) {
    if (IsBagFull()) {
        return false;
    }
    bag_.push_back({item_id, type, value});
    return true;
}
    
std::vector<BagItem> Dog::ClearBag() {
    std::vector<BagItem> returned = std::move(bag_);
    bag_.clear();
    return returned;
}
    
bool Dog::IsBagFull() const {
    return bag_.size() == bag_capacity_;
}
    
const std::vector<BagItem>& Dog::GetBagItems() const {
    return bag_;
}

} // end namespace model