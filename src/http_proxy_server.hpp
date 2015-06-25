/*
 *    http_proxy_server.hpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#ifndef AZURE_HTTP_PROXY_SERVER_HPP
#define AZURE_HTTP_PROXY_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/chrono.hpp>

#include "picture_classifier.hpp"

namespace azure_proxy {
using namespace boost;
using namespace boost::asio;

class http_proxy_server {
  io_service &network_io_service_;
  ip::tcp::acceptor acceptor_;
  io_service &classification_service_;
  PictureClassifier picture_classifier_;

public:
  http_proxy_server(io_service &network_io_service,
                    io_service &classification_service);
  void run();

private:
  void start_accept();
};

} // namespace azure_proxy

#endif
