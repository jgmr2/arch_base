#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <memory>

namespace net = boost::asio;
namespace http = boost::beast::http;
using stream_protocol = net::local::stream_protocol;

void handle_session(stream_protocol::socket socket) {
    auto self = std::make_shared<stream_protocol::socket>(std::move(socket));
    auto buffer = std::make_shared<boost::beast::flat_buffer>();
    auto req = std::make_shared<http::request<http::string_body>>();

    // Lectura asíncrona: No bloquea el hilo
    http::async_read(*self, *buffer, *req, [self, buffer, req](boost::beast::error_code ec, std::size_t) {
        if (ec) return;

        auto res = std::make_shared<http::response<http::string_body>>(http::status::ok, req->version());
        res->set(http::field::content_type, "application/json");
        res->set(http::field::server, "Nexus-C++");
        res->body() = "{\"status\":\"NEXUS_MAX\"}";
        res->prepare_payload();
        res->keep_alive(req->keep_alive());

        // Escritura asíncrona
        http::async_write(*self, *res, [self, res](boost::beast::error_code ec, std::size_t) {
            if (!ec && res->keep_alive()) {
                // Aquí podrías implementar recursión para persistir la conexión
            }
            boost::beast::error_code ignored_ec;
            self->shutdown(stream_protocol::socket::shutdown_both, ignored_ec);
        });
    });
}

void accept_loop(stream_protocol::acceptor& acceptor, net::io_context& ioc) {
    acceptor.async_accept([&acceptor, &ioc](boost::beast::error_code ec, stream_protocol::socket socket) {
        if (!ec) std::thread(handle_session, std::move(socket)).detach();
        accept_loop(acceptor, ioc);
    });
}

int main() {
    const std::string path = "/var/run/nexus/messenger.sock";
    std::filesystem::create_directories("/var/run/nexus");
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    net::io_context ioc;
    stream_protocol::endpoint ep(path);
    stream_protocol::acceptor acceptor(ioc, ep);
    
    // Optimización de Kernel: Aumentar la cola de espera (backlog)
    listen(acceptor.native_handle(), 4096); 
    
    std::filesystem::permissions(path, std::filesystem::perms::all);

    accept_loop(acceptor, ioc);

    // Crear un Pool de hilos igual al número de núcleos físicos
    std::vector<std::thread> threads;
    for (auto i = 0u; i < std::thread::hardware_concurrency(); ++i) {
        threads.emplace_back([&ioc] { ioc.run(); });
    }

    std::cout << "🚀 NEXUS Engine en Scratch con " << threads.size() << " hilos de ejecución." << std::endl;
    for (auto& t : threads) t.join();

    return 0;
}