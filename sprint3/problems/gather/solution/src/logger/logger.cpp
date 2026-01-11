#include "logger/logger.h"

namespace logger {

namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

void JsonFormatter(const logging::record_view& rec, logging::formatting_ostream& strm) {
    json::object log;

    if (auto ts = rec[timestamp]; ts) {
        log["timestamp"] = to_iso_extended_string(*ts);
    }

    if (auto data = rec[additional_data]; data) {
        log["data"] = *data;
    }

    if (auto message = rec[expr::smessage]; message) {
        log["message"] = *message;
    }

    strm << json::serialize(log);
}

void InitLogFilter() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        logging::keywords::format = &JsonFormatter,
        logging::keywords::auto_flush = true
    );
}

} // namespace logger