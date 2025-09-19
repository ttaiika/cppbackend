#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        try {
            // Получаем уникальные id сразу, атомарно
            int bread_id = next_id_.fetch_add(1, std::memory_order_relaxed);
            int sausage_id = next_id_.fetch_add(1, std::memory_order_relaxed);
            int hotdog_id = next_id_.fetch_add(1, std::memory_order_relaxed);

            // Создаём ингредиенты напрямую с id, минуя внутренний счётчик Store
            auto bread = std::make_shared<Bread>(bread_id);
            auto sausage = std::make_shared<Sausage>(sausage_id);

            // Состояние готовки ингредиентов
            auto bread_done = std::make_shared<std::atomic_bool>(false);
            auto sausage_done = std::make_shared<std::atomic_bool>(false);

            auto handler_ptr = std::make_shared<HotDogHandler>(std::move(handler));
            auto check_completion = std::make_shared<std::function<void()>>();

            *check_completion = [bread, sausage, bread_done, sausage_done, handler_ptr, hotdog_id, check_completion]() {
                if (*bread_done && *sausage_done) {
                    try {
                        auto bread_time = bread->GetBakingDuration();
                        auto sausage_time = sausage->GetCookDuration();

                        if (bread_time < HotDog::MIN_BREAD_COOK_DURATION ||
                            bread_time > HotDog::MAX_BREAD_COOK_DURATION) {
                            (*handler_ptr)(Result<HotDog>(
                                std::make_exception_ptr(std::runtime_error("Bread cook duration out of range"))));
                            return;
                        }

                        if (sausage_time < HotDog::MIN_SAUSAGE_COOK_DURATION ||
                            sausage_time > HotDog::MAX_SAUSAGE_COOK_DURATION) {
                            (*handler_ptr)(Result<HotDog>(
                                std::make_exception_ptr(std::runtime_error("Sausage cook duration out of range"))));
                            return;
                        }

                        auto hotdog = std::make_shared<HotDog>(hotdog_id, sausage, bread);
                        (*handler_ptr)(Result<HotDog>(*hotdog));
                    }
                    catch (const std::exception& e) {
                        (*handler_ptr)(Result<HotDog>(std::make_exception_ptr(e)));
                    }
                }
                };

            // Асинхронная готовка булки
            bread->StartBake(*gas_cooker_, [this, bread, bread_done, check_completion]() {
                auto timer = std::make_shared<boost::asio::steady_timer>(io_);
                timer->expires_after(HotDog::MIN_BREAD_COOK_DURATION);
                timer->async_wait([bread, bread_done, check_completion, timer](const boost::system::error_code& ec) {
                    if (ec) return;
                    bread->StopBaking();
                    *bread_done = true;
                    (*check_completion)();
                    });
                });

            // Асинхронная готовка сосиски
            sausage->StartFry(*gas_cooker_, [this, sausage, sausage_done, check_completion]() {
                auto timer = std::make_shared<boost::asio::steady_timer>(io_);
                timer->expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
                timer->async_wait([sausage, sausage_done, check_completion, timer](const boost::system::error_code& ec) {
                    if (ec) return;
                    sausage->StopFry();
                    *sausage_done = true;
                    (*check_completion)();
                    });
                });

        }
        catch (const std::exception& e) {
            handler(Result<HotDog>(std::make_exception_ptr(e)));
        }
    }


private:
    net::io_context& io_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    std::atomic_int next_id_{ 0 }; // счётчик хот-догов
};
