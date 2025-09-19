#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": " << ec.message() << std::endl;
}

void SessionBase::Run() {
    // �������� ����� Read, ��������� executor ������� stream_.
    // ����� ������� ��� ������ �� stream_ ����� �����������, ��������� ��� executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write"sv);
    }

    if (close) {
        // ��������� ������ ������� ������� ����������
        return Close();
    }

    // ��������� ��������� ������
    Read();
}

void SessionBase::Read() {
    using namespace std::literals;
    // ������� ������ �� �������� �������� (����� Read ����� ���� ������ ��������� ���)
    request_ = {};
    stream_.expires_after(30s);
    // ��������� request_ �� stream_, ��������� buffer_ ��� �������� ��������� ������
    http::async_read(stream_, buffer_, request_,
        // �� ��������� �������� ����� ������ ����� OnRead
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    using namespace std::literals;
    if (ec == http::error::end_of_stream) {
        // ���������� �������� - ������ ������ ����������
        return Close();
    }
    if (ec) {
        return ReportError(ec, "read"sv);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace http_server
