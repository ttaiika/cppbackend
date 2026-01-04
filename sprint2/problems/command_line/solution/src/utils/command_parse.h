#pragma once

#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

using namespace std::literals;

struct Args {
    unsigned int period = 0;
    std::string config;
    std::string www_root;
    bool random;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{"Allowed options"s};
    Args args;

    // Allowed options:
    // -h [ --help ]                     produce help message
    // -t [ --tick-period ] milliseconds set tick period
    // -c [ --config-file ] file         set config file path
    // -w [ --www-root ] dir             set static files root
    // --randomize-spawn-points          spawn dogs at random positions

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.period)->value_name("milliseconds"s),"set tick period")
        ("config-file,c", po::value(&args.config)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.www_root)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", po::value<bool>(&args.random), "spawn dogs at random positions");

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    // Проверяем наличие опций src и dst
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("The config file was not specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("The root of static files is not specified"s);
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;    
}
