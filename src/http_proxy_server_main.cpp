/*
 *    http_proxy_server_main.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <iostream>
#include <boost/filesystem.hpp>

//#include "http_proxy_server_config.hpp"
#include "http_proxy_server.hpp"
#include "glog/logging.h"
#include "misc.hpp"
#include "url_database.h"

#include "picture_classifier.hpp"

int main(int argc, char **argv) {
  using namespace azure_proxy;
  namespace fs = boost::filesystem;
  //try{
	 // InitLogging(kProxyLogDir);
	 // UrlDatabase::InitAndCreate();
	 // //PictureClassifier::Test("test_images");
	 // UrlDatabase::Test();
  //}
  //catch (const boost::exception &e){
	 // BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
  //}
  //catch (const std::exception &e) {
	 // BOOST_LOG_TRIVIAL(fatal) << e.what();
  //}

  try {
	  ::google::InitGoogleLogging(argv[0]);
	  InitLogging(kProxyLogDir);
	  UrlDatabase::InitAndCreate();
	  fs::path image_cache_path(kImageCacheDir);
	  if (!fs::exists(image_cache_path))
		  fs::create_directories(image_cache_path);
	  //auto &config = http_proxy_server_config::get_instance();
	  //if (config.load_config()) {
	  std::cout << "Azure Http Proxy Server" << std::endl;
	  //std::cout << "bind address: " << config.get_bind_address() << ':'
	  //          << config.get_listen_port() << std::endl;
	  std::cout << "bind address: " << kBindAddress << ':'
		  << kBindPort << std::endl;
	  boost::asio::io_service io_service;
	  boost::asio::io_service classification_service;
	  http_proxy_server server(io_service, classification_service);
	  server.run();
	  //}
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
