/*
 *    http_proxy_server.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <boost/ref.hpp>
#include "misc.hpp"

#include "http_proxy_server.hpp"
#include "http_proxy_server_config.hpp"
#include "http_proxy_server_connection.hpp"

namespace azure_proxy {
using namespace boost;
using namespace boost::asio;

http_proxy_server::http_proxy_server(io_service &network_io_service,
                                     io_service &classification_service)
    : network_io_service_(network_io_service), acceptor_(network_io_service),
      classification_service_(classification_service), picture_classifier_(classification_service) {}

void http_proxy_server::run() {
  const auto &config = http_proxy_server_config::get_instance();

  picture_classifier_.LoadModel(config.GetDeployProto(), config.GetModel(), config.GetMean());
  
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string(config.get_bind_address()),
      config.get_listen_port());
  acceptor_.open(endpoint.protocol());
  acceptor_.bind(endpoint);
  acceptor_.listen(socket_base::max_connections);
  start_accept();

  std::vector<std::thread> td_vec;

  for (auto i = 0u; i < config.get_workers(); ++i) {
  //for (auto i = 0u; i < 1; ++i) {
    td_vec.emplace_back([this]() {
      try {
        this->network_io_service_.run();
      } 
	  catch (const boost::exception &e){
		  BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
		  exit(EXIT_FAILURE);
	  }
	  catch (const std::exception &e) {
		  BOOST_LOG_TRIVIAL(fatal) << e.what();
		  exit(EXIT_FAILURE);
	  }
	});
  }

  optional<io_service::work> classification_work(
	  in_place(ref(classification_service_)));

  std::vector<std::thread> back_td_vec;

  for (auto i = 0u; i < 1; ++i) {
	  back_td_vec.emplace_back([this]() {
		  try {
			  this->classification_service_.run();
		  }
		  catch (const boost::exception &e){
			  BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
			  exit(EXIT_FAILURE);
		  }
		  catch (const std::exception &e) {
			  BOOST_LOG_TRIVIAL(fatal) << e.what();
			  exit(EXIT_FAILURE);
		  }
	  });
  }

  for (auto &td : td_vec) {
	  td.join();
  }

  classification_work = boost::none;
  for (auto &td : back_td_vec) {
	  td.join();
  }

}

void http_proxy_server::start_accept() {
	auto socket = std::make_shared<ip::tcp::socket>(acceptor_.get_io_service());
	acceptor_.async_accept(
		*socket, [socket, this](const system::error_code &error) {
		if (!error) {
			//std::cout<< "new connection!\n";
			auto connection = http_proxy_server_connection::create(
				std::move(*socket), picture_classifier_, server_context_);
			connection->start();
			this->start_accept();
		}
	});
}

} // namespace azure_proxy
