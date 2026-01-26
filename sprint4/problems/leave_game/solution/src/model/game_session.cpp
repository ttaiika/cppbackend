#include "game_session.h"

#include <random>

#include "../logger.h"
#include "collector.h"

namespace model {

namespace {

// [0, max]
size_t Rand(size_t max) {
  if (max == 0) return max;
  static std::default_random_engine generator;
  std::uniform_int_distribution<> distribution(0, max);
  return distribution(generator);
}

inline void BoundExpand(const Road& road, Point2d& min, Point2d& max) {
  // change min
  min.x = std::min(road.GetMinX(), min.x);
  min.y = std::min(road.GetMinY(), min.y);
  // change max
  max.x = std::max(road.GetMaxX(), max.x);
  max.y = std::max(road.GetMaxY(), max.y);
}

}  // namespace

GameSession::GameSession(/*GameSession::Id id,*/ const Map& map, LootGenerator gen)
    : m_id(0),
      m_map(map),
      m_random_spawn(false),
      m_loot_gen(std::move(gen)),
      m_retirement_time(0ms) {
}

GameSession::~GameSession() {
}

const Map& GameSession::GetMap() const noexcept {
  return m_map;
}

void GameSession::AddDog(DogPtr dog) {
  if (dog == nullptr) {
    throw std::invalid_argument("Try to add Dog as null");
  }

  if (dog->GetPosition() == Point2d()) {
    dog->SetPosition(m_map.GetRandPoint(m_random_spawn));
  }

  dog->SetDefaultSpeed(m_map.GetDogSpeed());
  dog->SetBagSize(m_map.GetBagCapacity());
  m_dogs.emplace_back(dog);
}

void GameSession::Update(std::chrono::milliseconds delta_time) {
  GenerateLoot(delta_time);

  DogsUpdate(delta_time);
}

void GameSession::SetDogRandomSpawn(bool enable) {
  m_random_spawn = enable;
}

void GameSession::SetRetirementTime(std::chrono::milliseconds downtime) {
  assert(downtime != 0ms);
  m_retirement_time = downtime;
}

std::chrono::milliseconds GameSession::GetRetirementTime() const noexcept {
  return m_retirement_time;
}

void GameSession::GenerateLoot(std::chrono::milliseconds delta_time) {
  const size_t max_loots = m_map.GetLootTypes().size();
  if (max_loots == 0 || m_loots.size() >= m_dogs.size()) return;

  const size_t count = m_loot_gen.Generate(delta_time, m_loots.size(), m_dogs.size());

  for (size_t i = 0; i < count; ++i) {
    const size_t loot_index = Rand(max_loots - 1);
    m_loots.emplace_back(m_map.GetLootTypes().at(loot_index), m_map.GetRandPoint(), m_loot_id++);
  }
}

void GameSession::DogsUpdate(std::chrono::milliseconds delta_time) {
  Collector collector(m_loots, m_map.GetOffices());

  std::erase_if(m_dogs, [](DogWeakPtr& w) {
    return w.expired();
  });

  for (auto it = m_dogs.begin(); it != m_dogs.end(); ++it) {
    assert(!it->expired());
    auto dog = it->lock();
    assert(dog != nullptr);

    dog->UpdateTimer(delta_time);

    const Point2d& pos = dog->GetPosition();
    const Point2d& speed = dog->GetSpeed();

    if (speed == Point2d()) {
      continue;
    }

    const double sec = std::chrono::duration<double>(delta_time).count();
    Point2d nextPos = pos + (speed * sec);

    // border define
    Point2d min_pos = pos;
    Point2d max_pos = pos;

    // border update
    for (auto road = m_map.GetRoads().cbegin(); road != m_map.GetRoads().cend(); ++road) {
      if (road->Contains(pos)) {
        BoundExpand(*road, min_pos, max_pos);
      }
    }

    if (nextPos.y < min_pos.y) {
      nextPos.y = min_pos.y;
      dog->Stop();
    } else if (nextPos.y > max_pos.y) {
      nextPos.y = max_pos.y;
      dog->Stop();
    } else if (nextPos.x < min_pos.x) {
      nextPos.x = min_pos.x;
      dog->Stop();
    } else if (nextPos.x > max_pos.x) {
      nextPos.x = max_pos.x;
      dog->Stop();
    }

    if (pos == nextPos) {
      continue;
    }

    collector.AddDogToMoveUpdate(dog, nextPos);
  }

  collector.Update();
}

}  // namespace model