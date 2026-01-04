#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "logging_request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

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
    logger::InitLogFilter();

    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        // 1. Загружаем карту из файла и строим модель игры
        model::Game game = json_loader::LoadGame(argv[1]);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        // Подписываемся на сигналы и при их получении завершаем работу сервера
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }   
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        std::optional<fs::path> static_root;
        if (argc == 3) {
            static_root = argv[2];
        }
        
        http_handler::RequestHandler handler{game, ioc, static_root};
        log_handler::LoggingRequestHandler logging_handler{handler};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
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
                                       }
                                   )
                                << "server started";

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(
                                       logger::additional_data,
                                       logger::json::value {{"code", 0}}
                                   )
                                << "server exited";
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(
                                       logger::additional_data,
                                       logger::json::value {
                                           {"code", EXIT_FAILURE},
                                           {"exception", ex.what()},
                                       }
                                   )
                                << "server exited";
        return EXIT_FAILURE;
    }
}