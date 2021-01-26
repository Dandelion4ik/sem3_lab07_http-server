// Copyright 2020 Kavykin Andrey Kaviandr@yandex.ru

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "json_rep.hpp"
#include "suggestion_collect.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
std::string make_json(const json& data) {
  std::stringstream ss;
  if (data.is_null())
    ss << "No suggestions";
  else
    ss << std::setw(4) << data;
  return ss.str();
}

template <class body_, class allocator_, class send_>
void handle_request(http::request<body_, http::basic_fields<allocator_>>&& req,
                    send_&& send, const std::shared_ptr<std::timed_mutex>& mutex,
                    const std::shared_ptr<suggestion_collect>& collection) {


  auto const bad_request = [&req](beast::string_view why) {
    http::response<http::string_body> res{http::status::bad_request,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };


  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{http::status::not_found,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

  if (req.method() == http::verb::get) {
    return send(bad_request("Hello!"));
  }

  if (req.method() != http::verb::post && req.method() != http::verb::head) {
    return send(bad_request("Unknown HTTP-method"));
  }

  if (req.target() != "/v1/api/suggest") {
    return send(not_found(req.target()));
  }

  json input_body;
  try {
    input_body = json::parse(req.body());
  } catch (std::exception& e) {
    return send(bad_request(e.what()));
  }
  boost::optional<std::string> input;
  try {
    input = input_body.at("input").get<std::string>();
  } catch (std::exception& e) {
    return send(bad_request(R"(JSON format: {"input": "<user_input>"}!)"));
  }
  if (!input.has_value()) {
    return send(bad_request(R"(JSON format: {"input": "<user_input>"}!)"));
  }

  mutex->lock();
  auto result = collection->suggest(input.value());
  mutex->unlock();
  http::string_body::value_type body = make_json(result);
  auto const size = body.size();

  if (req.method() == http::verb::head) {
    http::response<http::empty_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
  }

  http::response<http::string_body> res{
      std::piecewise_construct, std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "application/json");
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}

void fail(beast::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

template <class Stream>
struct send_lambda {
  Stream& stream_;
  [[maybe_unused]] bool& close_;
  beast::error_code& ec_;

  explicit send_lambda(Stream& stream, bool& close, beast::error_code& ec)
      : stream_(stream), close_(close), ec_(ec) {}

  template <bool is_request, class body, class fields>
  void operator()(http::message<is_request, body, fields>&& msg) const {
    close_ = msg.need_eof();
    http::serializer<is_request, body, fields> sr{msg};
    http::write(stream_, sr, ec_);
  }
};

void do_session(net::ip::tcp::socket& socket,
                const std::shared_ptr<suggestion_collect>& collection,
                const std::shared_ptr<std::timed_mutex>& mutex) {
  bool close = false;
  beast::error_code ec;

  beast::flat_buffer buffer;

  send_lambda<tcp::socket> lambda{socket, close, ec};
  for (;;) {
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec == http::error::end_of_stream) break;
    if (ec) return fail(ec, "read");
    handle_request(std::move(req), lambda, mutex, collection);
    if (ec) return fail(ec, "write");
  }

  socket.shutdown(tcp::socket::shutdown_send, ec);
}

void suggestion_updater(
    const std::shared_ptr<json_rep>& storage,
    const std::shared_ptr<suggestion_collect>& suggestions,
    const std::shared_ptr<std::timed_mutex>& mutex) {
  using std::chrono_literals::operator""min;
  for (;;) {
    mutex->lock();
    storage->load();
    suggestions->update(storage->get_rep());
    mutex->unlock();
    std::cout << "Suggestions updated" << std::endl;
    std::this_thread::sleep_for(15min);
  }
}
int run_server(int argc, char** argv) {
  std::shared_ptr<std::timed_mutex> mutex =
      std::make_shared<std::timed_mutex>();
  std::shared_ptr<json_rep> storage = std::make_shared<json_rep>(
      "C::\\Users/Kavia/CLionProjects/lab-07-http-server/suggestions.json");
  std::shared_ptr<suggestion_collect> suggestions =
      std::make_shared<suggestion_collect>();
  try {
    if (argc != 3) {
      std::cerr << "Usage: suggestion_server <address> <port>\n"
                << "Example:\n"
                << "    http-server-sync 0.0.0.0 8080\n";
      return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<uint16_t>(std::atoi(argv[2]));
    net::io_context ioc{1};


    tcp::acceptor acceptor{ioc, {address, port}};


    std::thread{suggestion_updater, storage, suggestions, mutex}.detach();
    for (;;) {
      tcp::socket socket{ioc};
      acceptor.accept(socket);
      std::thread{std::bind(&do_session, std::move(socket), suggestions, mutex)}
          .detach();
    }
  } catch (std::exception& e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
// Using: ./cmake-build-debug/tests 0.0.0.0 8080
int main(int argc, char* argv[]) {
 return Run_server(argc, argv);
}