#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <filesystem>

namespace net = boost::asio;
namespace http = boost::beast::http;
using stream_protocol = net::local::stream_protocol;

int main() {
    std::string const path = "/var/run/nexus/messenger.sock";
    
    if (std::filesystem::exists(path)) std::filesystem::remove(path);

    net::io_context ioc;
    stream_protocol::endpoint ep(path);
    stream_protocol::acceptor acceptor(ioc, ep);

    // Permisos para que Nginx pueda leer/escribir en el socket
    std::filesystem::permissions(path, std::filesystem::perms::all);

    std::cout << "Beast escuchando en: " << path << std::endl;

    for (;;) {
        stream_protocol::socket socket(ioc);
        acceptor.accept(socket);

        // Bloque de sesión: Mientras Nginx mantenga el socket abierto
        boost::beast::error_code ec;
        boost::beast::flat_buffer buffer;

        for (;;) {
            http::request<http::string_body> req;
            http::read(socket, buffer, req, ec);

            if (ec == http::error::end_of_stream) break; // Nginx cerró la conexión
            if (ec) break; // Error de lectura

            // Respuesta
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, "application/json");
            res.keep_alive(req.keep_alive());
            res.body() = "{\"status\":\"ok, todo funciono\"}";
            res.prepare_payload();

            http::write(socket, res, ec);
            if (ec) break;

            // Si Nginx no pidió keep-alive, cerramos nosotros
            if (!req.keep_alive()) break;
        }

        // Cerramos socket al salir del bucle de sesión
        socket.shutdown(stream_protocol::socket::shutdown_both, ec);
    }
    return 0;
}