#include "game_session.h"
#include "model.h"   // здесь уже можно подключить полный Map
#include <cassert>
#include <cmath>
#include <iostream>

using namespace model;

GameSession::GameSession(std::shared_ptr<model::Map> map)
    : map_(std::move(map)) {
}

std::shared_ptr<model::Map> GameSession::GetMap() const {
    return map_;
}

Id GameSession::GetMapId() const {
    return map_->GetId();
}

model::Dog& GameSession::AddDog(std::shared_ptr<model::Dog> dog) {
    model::Dog* raw_ptr = dog.get();
    dogs_.push_back(std::move(dog));
    return *raw_ptr;
}

 const std::vector<std::shared_ptr<model::Dog>>& GameSession::GetDogs() const {
        std::cout << "GameSession::GetDogs() called, returning " << dogs_.size() << " dogs" << std::endl;
        return dogs_;
}

constexpr double ROAD_WIDTH = 0.8;
constexpr double ROAD_HALF_WIDTH = 0.4;
constexpr double EPS = 1e-9;

// Проверяет, находится ли точка в пределах дороги (с учетом ширины)
bool IsPointOnRoad(const model::Position& p, const Road& road) {
    if (road.IsHorizontal()) {
        double min_x = std::min(road.GetStart().x, road.GetEnd().x);
        double max_x = std::max(road.GetStart().x, road.GetEnd().x);
        double road_y = road.GetStart().y;
        
        bool within_x = (p.x >= min_x - ROAD_HALF_WIDTH - EPS) && 
                       (p.x <= max_x + ROAD_HALF_WIDTH + EPS);
        bool within_y = (p.y >= road_y - ROAD_HALF_WIDTH - EPS) && 
                       (p.y <= road_y + ROAD_HALF_WIDTH + EPS);
        return within_x && within_y;
    } else {
        double min_y = std::min(road.GetStart().y, road.GetEnd().y);
        double max_y = std::max(road.GetStart().y, road.GetEnd().y);
        double road_x = road.GetStart().x;
        
        bool within_x = (p.x >= road_x - ROAD_HALF_WIDTH - EPS) && 
                       (p.x <= road_x + ROAD_HALF_WIDTH + EPS);
        bool within_y = (p.y >= min_y - ROAD_HALF_WIDTH - EPS) && 
                       (p.y <= max_y + ROAD_HALF_WIDTH + EPS);
        return within_x && within_y;
    }
}

// Находит все дороги, на которых находится точка
std::vector<const Road*> FindRoadsAtPoint(const model::Position& pos, const std::vector<Road>& roads) {
    std::vector<const Road*> result;
    for (const auto& road : roads) {
        if (IsPointOnRoad(pos, road)) {
            result.push_back(&road);
        }
    }
    return result;
}

// Проверяет, можно ли двигаться с текущей дороги на другую в заданном направлении
bool CanTransition(const Road* from, const Road* to, const Speed& speed) {
    if (!from || !to) return false;
    
    // Если собака движется горизонтально, ищем вертикальную дорогу для перехода
    if (std::abs(speed.x) > EPS && std::abs(speed.y) < EPS) {
        // Горизонтальное движение: ищем вертикальную дорогу
        return !to->IsHorizontal();
    } 
    // Если собака движется вертикально, ищем горизонтальную дорогу для перехода
    else if (std::abs(speed.y) > EPS && std::abs(speed.x) < EPS) {
        // Вертикальное движение: ищем горизонтальную дорогу
        return to->IsHorizontal();
    }
    
    return false;
}

// Находит подходящую дорогу для движения в заданном направлении
const Road* FindRoadForMovement(const Position& pos, const Speed& speed, const std::vector<Road>& roads) {
    auto current_roads = FindRoadsAtPoint(pos, roads);
    
    // Если собака стоит на нескольких дорогах (перекресток)
    if (current_roads.size() > 1) {
        // Ищем дорогу, соответствующую направлению движения
        for (const auto* road : current_roads) {
            if (std::abs(speed.x) > EPS && road->IsHorizontal()) {
                return road;  // Горизонтальное движение -> горизонтальная дорога
            } else if (std::abs(speed.y) > EPS && !road->IsHorizontal()) {
                return road;  // Вертикальное движение -> вертикальная дорога
            }
        }
    }
    
    // Если на одной дороге или не нашли подходящую
    if (!current_roads.empty()) {
        return current_roads[0];
    }
    
    return nullptr;
}

void MoveDog(model::Position& pos, model::Speed& speed, double dt, const std::vector<Road>& roads) {
    // Находим текущую дорогу
    const Road* current_road = FindRoadForMovement(pos, speed, roads);
    
    if (!current_road) {
        // Если собака вне дорог, останавливаем
        speed = {0.0, 0.0};
        return;
    }
    
    // Рассчитываем целевую позицию
    Position target_pos = {
        pos.x + speed.x * dt,
        pos.y + speed.y * dt
    };
    
    // Проверяем, останется ли собака на текущей дороге
    if (IsPointOnRoad(target_pos, *current_road)) {
        // Двигаемся по текущей дороге
        pos = target_pos;
    } else {
        // Собака покидает текущую дорогу
        
        if (current_road->IsHorizontal()) {
            // Горизонтальная дорога
            double min_x = std::min(current_road->GetStart().x, current_road->GetEnd().x);
            double max_x = std::max(current_road->GetStart().x, current_road->GetEnd().x);
            double road_y = current_road->GetStart().y;
            
            // Проверяем движение по X
            if (std::abs(speed.x) > EPS) {
                // Достигли конца горизонтальной дороги
                if (speed.x > 0 && target_pos.x > max_x + ROAD_HALF_WIDTH) {
                    pos.x = max_x + ROAD_HALF_WIDTH;
                    speed.x = 0.0;
                } else if (speed.x < 0 && target_pos.x < min_x - ROAD_HALF_WIDTH) {
                    pos.x = min_x - ROAD_HALF_WIDTH;
                    speed.x = 0.0;
                }
            }
            
            // Проверяем движение по Y (попытка перейти на другую дорогу)
            if (std::abs(speed.y) > EPS) {
                // Пытаемся найти вертикальную дорогу для перехода
                const Road* vertical_road = nullptr;
                for (const auto& road : roads) {
                    if (!road.IsHorizontal()) {
                        double road_x = road.GetStart().x;
                        double min_y = std::min(road.GetStart().y, road.GetEnd().y);
                        double max_y = std::max(road.GetStart().y, road.GetEnd().y);
                        
                        // Проверяем, пересекается ли вертикальная дорога с текущей позицией по X
                        if (std::abs(pos.x - road_x) <= ROAD_HALF_WIDTH + EPS) {
                            // Проверяем, сможет ли собака перейти на вертикальную дорогу
                            if (target_pos.y >= min_y - ROAD_HALF_WIDTH - EPS && 
                                target_pos.y <= max_y + ROAD_HALF_WIDTH + EPS) {
                                vertical_road = &road;
                                break;
                            }
                        }
                    }
                }
                
                if (vertical_road) {
                    // Переходим на вертикальную дорогу
                    pos.x = vertical_road->GetStart().x;  // Центрируем по X
                    pos.y = target_pos.y;  // Двигаемся по Y
                    
                    // Проверяем границы вертикальной дороги
                    double min_y = std::min(vertical_road->GetStart().y, vertical_road->GetEnd().y);
                    double max_y = std::max(vertical_road->GetStart().y, vertical_road->GetEnd().y);
                    
                    if (pos.y < min_y - ROAD_HALF_WIDTH) {
                        pos.y = min_y - ROAD_HALF_WIDTH;
                        speed.y = 0.0;
                    } else if (pos.y > max_y + ROAD_HALF_WIDTH) {
                        pos.y = max_y + ROAD_HALF_WIDTH;
                        speed.y = 0.0;
                    }
                } else {
                    // Не нашли подходящую дорогу, останавливаемся у границы
                    if (speed.y > 0) {
                        pos.y = road_y + ROAD_HALF_WIDTH;
                    } else {
                        pos.y = road_y - ROAD_HALF_WIDTH;
                    }
                    speed.y = 0.0;
                }
            }
        } else {
            // Вертикальная дорога
            double min_y = std::min(current_road->GetStart().y, current_road->GetEnd().y);
            double max_y = std::max(current_road->GetStart().y, current_road->GetEnd().y);
            double road_x = current_road->GetStart().x;
            
            // Проверяем движение по Y
            if (std::abs(speed.y) > EPS) {
                // Достигли конца вертикальной дороги
                if (speed.y > 0 && target_pos.y > max_y + ROAD_HALF_WIDTH) {
                    pos.y = max_y + ROAD_HALF_WIDTH;
                    speed.y = 0.0;
                } else if (speed.y < 0 && target_pos.y < min_y - ROAD_HALF_WIDTH) {
                    pos.y = min_y - ROAD_HALF_WIDTH;
                    speed.y = 0.0;
                }
            }
            
            // Проверяем движение по X (попытка перейти на другую дорогу)
            if (std::abs(speed.x) > EPS) {
                // Пытаемся найти горизонтальную дорогу для перехода
                const Road* horizontal_road = nullptr;
                for (const auto& road : roads) {
                    if (road.IsHorizontal()) {
                        double road_y = road.GetStart().y;
                        double min_x = std::min(road.GetStart().x, road.GetEnd().x);
                        double max_x = std::max(road.GetStart().x, road.GetEnd().x);
                        
                        // Проверяем, пересекается ли горизонтальная дорога с текущей позицией по Y
                        if (std::abs(pos.y - road_y) <= ROAD_HALF_WIDTH + EPS) {
                            // Проверяем, сможет ли собака перейти на горизонтальную дорогу
                            if (target_pos.x >= min_x - ROAD_HALF_WIDTH - EPS && 
                                target_pos.x <= max_x + ROAD_HALF_WIDTH + EPS) {
                                horizontal_road = &road;
                                break;
                            }
                        }
                    }
                }
                
                if (horizontal_road) {
                    // Переходим на горизонтальную дорогу
                    pos.y = horizontal_road->GetStart().y;  // Центрируем по Y
                    pos.x = target_pos.x;  // Двигаемся по X
                    
                    // Проверяем границы горизонтальной дороги
                    double min_x = std::min(horizontal_road->GetStart().x, horizontal_road->GetEnd().x);
                    double max_x = std::max(horizontal_road->GetStart().x, horizontal_road->GetEnd().x);
                    
                    if (pos.x < min_x - ROAD_HALF_WIDTH) {
                        pos.x = min_x - ROAD_HALF_WIDTH;
                        speed.x = 0.0;
                    } else if (pos.x > max_x + ROAD_HALF_WIDTH) {
                        pos.x = max_x + ROAD_HALF_WIDTH;
                        speed.x = 0.0;
                    }
                } else {
                    // Не нашли подходящую дорогу, останавливаемся у границы
                    if (speed.x > 0) {
                        pos.x = road_x + ROAD_HALF_WIDTH;
                    } else {
                        pos.x = road_x - ROAD_HALF_WIDTH;
                    }
                    speed.x = 0.0;
                }
            }
        }
    }
}

void GameSession::Tick(double dt) {
    const auto& roads = map_->GetRoads();

    for (auto& dog : dogs_) {
        auto pos = dog->GetPosition();
        auto speed = dog->GetSpeed();

        if (std::abs(speed.x) < EPS && std::abs(speed.y) < EPS) continue;

        MoveDog(pos, speed, dt, roads);

        dog->SetPosition(pos);
        dog->SetSpeed(speed);
    }
}
