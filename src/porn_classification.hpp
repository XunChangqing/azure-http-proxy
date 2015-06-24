#ifndef HEADER_PORN_CLASSIFICATION
#define HEADER_PORN_CLASSIFICATION
#include <thread>
#include "http_proxy_server_connection.hpp"
namespace azure_proxy {
/// @brief ODBC blocking operation.
///
/// @brief data Data to use for query.
/// @brief handler Handler to invoke upon completion of operation.
// template <typename Handler>
// void query_odbc(unsigned int data, Handler handler) {
// std::cout << "in background service, start querying odbc\n";
// std::cout.flush();
//// Mimic busy work.
// boost::this_thread::sleep_for(boost::chrono::seconds(5));

// std::cout << "in background service, posting odbc result to main service\n";
// std::cout.flush();
// io_service.post(boost::bind(handler, data * 2));
//}
// class PornClassifier {

  using namespace boost;
  using namespace boost::asio;

template <typename Handler>
void PornClassify(std::string format, std::string pic, io_service::strand &str, http_proxy_server_connection *conn, std::shared_ptr<http_proxy_server_connection> self, Handler handler) {
  std::cout << "porn classify: "<<std::this_thread::get_id()<<std::endl;
  std::cout.flush();
  str.post(bind(handler, conn, 0, self));
}
//};

/// @brief Functions as a continuation for handle_read, that will be
///        invoked with results from ODBC.
// void handle_read_odbc(unsigned int result) {
// std::stringstream stream;
// stream << "in main service, got " << result << " from odbc.\n";
// std::cout << stream.str();
// std::cout.flush();

//// Allow io_service to stop in this example.
// work = boost::none;
//}

// template <typename Handler>
// void porn_classify(std::string format, std::string pic, Handler handler){
// io_service.post(boost::bind(handler, data * 2));

// void handle_porn_classification(std::string format, std::string pic)

///// @brief Mocked up read handler that will post work into a background
/////        service.
// void handle_read(const boost::system::error_code &error,
// std::size_t bytes_transferred) {
// typedef void (*handler_type)(unsigned int);
// background_service.post(boost::bind(&query_odbc<handler_type>,
// 21,                // data
//&handle_read_odbc) // handler
//);

// Keep io_service event loop running in this example.
}
#endif
