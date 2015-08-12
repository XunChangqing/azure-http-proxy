/*
 *    http_proxy_server_main.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

//#include "http_proxy_server_config.hpp"
#include "http_proxy_server.hpp"
#include "glog/logging.h"
#include "misc.hpp"
#include "url_database.h"
#include "curl.h"

#include "picture_classifier.hpp"

int main(int argc, char **argv) {
  using namespace azure_proxy;
  namespace fs = boost::filesystem;
  namespace po = boost::program_options;
  //try{
	 // curl_global_init(CURL_GLOBAL_ALL);
	 // InitLogging(kProxyLogDir);
	 // UrlDatabase::InitAndCreate();
	 // //PictureClassifier::Test("test_images");
	 // UrlDatabase::Test();
	 // curl_global_cleanup();
  //}
  //catch (const boost::exception &e){
	 // BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
  //}
  //catch (const std::exception &e) {
	 // BOOST_LOG_TRIVIAL(fatal) << e.what();
  //}

  try {
	  po::options_description desc("Allowed options");
	  desc.add_options()
		  ("help", "produce help message")
		  ("workdir", po::value<std::string>(), "set work directory")
		  ;

	  po::variables_map vm;
	  po::store(po::parse_command_line(argc, argv, desc), vm);
	  po::notify(vm);

	  if (vm.count("help")) {
		  std::cout << desc << "\n";
		  return 1;
	  }

	  if (vm.count("workdir")) {
		  //BOOST_LOG_TRIVIAL(info) << "Work dir is set to: "
			 // << vm["workdir"].as<std::string>();
		  std::string workdir = vm["workdir"].as<std::string>();
		  GlobalConfig::LoadConfig(workdir);
	  }
	  else
	  {
		  BOOST_LOG_TRIVIAL(fatal) << "No workdir param!";
		  return 1;
	  }

	  curl_global_init(CURL_GLOBAL_ALL);
	  ::google::InitGoogleLogging(argv[0]);
	  InitLogging(GlobalConfig::GetInstance()->GetLogsDir());
	  BOOST_LOG_TRIVIAL(info) << "Start!";
	  UrlDatabase::InitAndCreate();
	  fs::path image_cache_path(GlobalConfig::GetInstance()->GetImageCacheDir());
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
	  boost::asio::io_service webaccess_service;
	  http_proxy_server server(io_service, classification_service, webaccess_service);
	  //handle ctrl c signal to exit cleanly
	  boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
	  signals.async_wait(boost::bind(&boost::asio::io_service::stop, &io_service));
	  server.run();
  }
  catch (const boost::exception &e){
	  BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
	  //exit(EXIT_FAILURE);
  }
  catch (const std::exception &e) {
	  BOOST_LOG_TRIVIAL(fatal) << e.what();
	  //exit(EXIT_FAILURE);
  }
  curl_global_cleanup();
  return 0;
}
