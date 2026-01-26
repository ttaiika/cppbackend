#pragma once

#include "../tagged.h"

namespace boost::serialization {

template <typename Archive, typename Value, typename Tag>
void save_construct_data (Archive& ar, const util::Tagged<Value, Tag> * tag, const unsigned version) {
  ar << (**tag);
}

template <typename Archive, typename Value, typename Tag>
void load_construct_data (Archive& ar, util::Tagged<Value, Tag> * tag, const unsigned version) {
  Value val;
  ar >> val;
  ::new(tag) util::Tagged<Value, Tag>(val);
}

template <typename Archive, typename Value, typename Tag>
void serialize(Archive& ar, util::Tagged<Value, Tag> & tag, const unsigned version) {
  // ** note that this is empty **
}

}  // namespace boost::serialization