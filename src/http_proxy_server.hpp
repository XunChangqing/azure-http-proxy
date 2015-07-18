/*
 *    http_proxy_server.hpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#ifndef AZURE_HTTP_PROXY_SERVER_HPP
#define AZURE_HTTP_PROXY_SERVER_HPP

#include <set>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include "picture_classifier.hpp"
#include "sqlite3.h"

namespace azure_proxy {
using namespace boost;
using namespace boost::asio;
//using std::vector;
//using std::string;
//using std::set;

struct http_proxy_server_context{
  boost::mutex imgsrc_dict_mutex;
  //std::map<std::string, std::set<std::string> > imgsrc_dict;
  std::vector<std::pair<std::string, std::set<std::string>>> imgsrc_dict;
  sqlite3 *porn_db;
};

class http_proxy_server {
  io_service &network_io_service_;
  ip::tcp::acceptor acceptor_;
  io_service &classification_service_;
  PictureClassifier picture_classifier_;
  http_proxy_server_context server_context_;

public:
  http_proxy_server(io_service &network_io_service,
                    io_service &classification_service);
  void run();

private:
  void start_accept();
};

} // namespace azure_proxy

#endif
