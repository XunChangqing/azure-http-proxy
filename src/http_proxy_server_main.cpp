/*
 *    http_proxy_server_main.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <iostream>

#include "http_proxy_server_config.hpp"
#include "http_proxy_server.hpp"
#include "glog/logging.h"
#include "misc.hpp"
#include "url_database.h"

int main(int argc, char **argv) {
  using namespace azure_proxy;
  try {
    ::google::InitGoogleLogging(argv[0]);
	InitLogging();
	UrlDatabase::InitAndCreate();
    auto &config = http_proxy_server_config::get_instance();
    if (config.load_config()) {
      std::cout << "Azure Http Proxy Server" << std::endl;
      std::cout << "bind address: " << config.get_bind_address() << ':'
                << config.get_listen_port() << std::endl;
      boost::asio::io_service io_service;
      boost::asio::io_service classification_service;
      http_proxy_server server(io_service, classification_service);
      server.run();
    }
  } 
  catch (const boost::exception &e){
	  BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
	  //exit(EXIT_FAILURE);
  }
  catch (const std::exception &e) {
	  BOOST_LOG_TRIVIAL(fatal) << e.what();
	  //exit(EXIT_FAILURE);
  }
  return 0;
}
