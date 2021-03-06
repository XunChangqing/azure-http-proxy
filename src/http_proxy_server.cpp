﻿/*
 *    http_proxy_server.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "http_proxy_server.hpp"
#include "http_proxy_server_config.hpp"
#include "http_proxy_server_connection.hpp"

namespace azure_proxy {

http_proxy_server::http_proxy_server(boost::asio::io_service& io_service) :
    io_service(io_service),
    acceptor(io_service)
{
}

void http_proxy_server::run()
{
    const auto& config = http_proxy_server_config::get_instance();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(config.get_bind_address()), config.get_listen_port());
    this->acceptor.open(endpoint.protocol());
    this->acceptor.bind(endpoint);
    this->acceptor.listen(boost::asio::socket_base::max_connections);
    this->start_accept();

    std::vector<std::thread> td_vec;

    for (auto i = 0u; i < config.get_workers(); ++i) {
        td_vec.emplace_back([this]() {
            try {
                this->io_service.run();
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        });
    }

    for (auto& td : td_vec) {
        td.join();
    }
}

void http_proxy_server::start_accept()
{
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(this->acceptor.get_io_service());
    this->acceptor.async_accept(*socket, [socket, this](const boost::system::error_code& error) {
        if (!error) {
            //std::cout<< "new connection!\n";
            auto connection = http_proxy_server_connection::create(std::move(*socket));
            connection->start();
            this->start_accept();
        }
    });
}

} //namespace azure_proxy
