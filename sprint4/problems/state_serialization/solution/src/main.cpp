#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "app/application.h"
#include "request_handler/logging_request_handler.h"
#include "utils/command_parse.h"
#include "infrastructure/serializing_listener.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    Args arg;

    try {
        if (auto args = ParseCommandLine(argc, argv)) {
            arg = *args;
        } else {
            return EXIT_SUCCESS;
        }
    } catch (const std::exception& e) {
        std::cout << "Parse arguments failure. " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    logger::InitLogFilter();

    try {
        // Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // Объект Application содержит сценарии использования
        Application app{arg.config};

        // auto update
        if (arg.period > 0) {
            app.SetTickPeriod(std::chrono::milliseconds(arg.period));
        }

        app.SetRandomSpawn(arg.random);

        // Создаем SerializingListener если указан файл состояния
        std::unique_ptr<infrastructure::SerializingListener> serializing_listener;
        
        if (arg.state_file) {
            try {
                std::chrono::milliseconds save_period(0);
                if (arg.save_state_period) {
                    save_period = std::chrono::milliseconds(*arg.save_state_period);
                }
                
                serializing_listener = std::make_unique<infrastructure::SerializingListener>(
                    app.GetGame(), 
                    fs::path(*arg.state_file),
                    save_period
                );
                
                // Добавляем слушатель к приложению
                app.AddListener(*serializing_listener);
                
                BOOST_LOG_TRIVIAL(info) << boost::log::add_value(
                                           logger::additional_data,
                                           logger::json::value {
                                               {"state_file", *arg.state_file},
                                               {"save_period_ms", save_period.count()}
                                           }
                                       )
                                    << "serialization enabled";
                
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << boost::log::add_value(
                                           logger::additional_data,
                                           logger::json::value {
                                               {"error", "Failed to restore state"},
                                               {"details", e.what()}
                                           }
                                       )
                                    << "state restoration failed";
                return EXIT_FAILURE;
            }
        }

        // strand, используемый для доступа к API
        auto api_strand = net::make_strand(ioc);

        // Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &serializing_listener](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                // Останавливаем io_context для корректного завершения
                ioc.stop();
                
                // Сохраняем финальное состояние перед выходом
                if (serializing_listener) {
                    try {
                        serializing_listener->SaveFinalState();
                        BOOST_LOG_TRIVIAL(info) << "Final state saved successfully";
                    } catch (const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to save final state: " << e.what();
                    }
                }
            }   
        });

        // Создаём обработчик HTTP-запросов и связываем его с моделью игры        
        std::shared_ptr<http_handler::RequestHandler> handler = std::make_shared<http_handler::RequestHandler>(
            api_strand, app, arg.www_root.empty() ? std::nullopt : std::make_optional(arg.www_root)
        );
        
        // Если задан период для автоматического тика, запускаем его
        if (arg.period > 0) {
            handler->StartAutoTick(std::chrono::milliseconds(arg.period));
        }

        log_handler::LoggingRequestHandler logging_handler{*handler};

        // Запустить обработчик HTTP-запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send, auto&& end_point) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), std::forward<decltype(end_point)>(end_point)); 
        });

        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(
                                       logger::additional_data,
                                       logger::json::value {
                                           {"port", port},
                                           {"address", address.to_string()},
                                           {"config", arg.config},
                                           {"www_root", arg.www_root},
                                           {"tick_period", arg.period},
                                           {"random_spawn", arg.random},
                                           {"state_file", arg.state_file ? *arg.state_file : "not set"},
                                           {"save_state_period", arg.save_state_period ? *arg.save_state_period : 0}
                                       }
                                   )
                                << "server started";

        // Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        // После завершения всех асинхронных операций
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(
                                       logger::additional_data,
                                       logger::json::value {{"code", 0}}
                                   )
                                << "server exited";
        
        return EXIT_SUCCESS;
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << boost::log::add_value(
                                       logger::additional_data,
                                       logger::json::value {
                                           {"code", EXIT_FAILURE},
                                           {"exception", ex.what()},
                                       }
                                   )
                                << "server exited with exception";
        return EXIT_FAILURE;
    }
}