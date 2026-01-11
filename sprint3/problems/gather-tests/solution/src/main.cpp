#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "app/application.h"
#include "request_handler/logging_request_handler.h"
#include "utils/command_parse.h"

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

        // strand, используемый для доступа к API
        auto api_strand = net::make_strand(ioc);

        // Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        // Подписываемся на сигналы и при их получении завершаем работу сервера
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }   
        });

        // Создаём обработчик HTTP-запросов и связываем его с моделью игры        
        std::shared_ptr<http_handler::RequestHandler> handler = std::make_shared<http_handler::RequestHandler>(api_strand, app, arg.www_root);
        // Если задан период для автоматического тика, запускаем его
        if (arg.period > 0) {
            handler->StartAutoTick(std::chrono::milliseconds(arg.period));
        }

        log_handler::LoggingRequestHandler logging_handler{*handler};

        // Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
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

        // Запускаем обработку асинхронных операций
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