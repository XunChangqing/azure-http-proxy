/*
 *    http_proxy_server_connection.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <cctype>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <thread>
#include <boost/regex.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/network/uri.hpp>
#include <boost/network/uri/uri_io.hpp>

#include "GLOG/logging.h"
#include "gumbo.h"

#include "authentication.hpp"
//#include "http_proxy_server_config.hpp"
#include "http_proxy_server_connection.hpp"
#include "misc.hpp"

#include "data/tld_tree.c"

static const std::size_t MAX_REQUEST_HEADER_LENGTH = 10240;
static const std::size_t MAX_RESPONSE_HEADER_LENGTH = 10240;

namespace azure_proxy {
  using namespace boost;
  using namespace boost::asio;

//http_proxy_server_connection::http_proxy_server_connection(boost::asio::ip::tcp::socket&& proxy_client_socket) :
 http_proxy_server_connection::http_proxy_server_connection(ip::tcp::socket&& proxy_client_socket, PictureClassifier& picture_classifier,
	 boost::asio::io_service &webaccess_service, http_proxy_server_context& server_context):
   picture_classifier_(picture_classifier),
   webaccess_service_(webaccess_service),
   url_database_(webaccess_service),
   server_context_(server_context),
    strand(proxy_client_socket.get_io_service()),
    proxy_client_socket(std::move(proxy_client_socket)),
    origin_server_socket(this->proxy_client_socket.get_io_service()),
    resolver(this->proxy_client_socket.get_io_service()),
    timer(this->proxy_client_socket.get_io_service())
{
	tree = init_tld_tree(tld_string);
}

http_proxy_server_connection::~http_proxy_server_connection()
{
	free_tld_tree(tree);
}

std::shared_ptr<http_proxy_server_connection> http_proxy_server_connection::create(boost::asio::ip::tcp::socket&& client_socket, PictureClassifier& picture_classifier,
	boost::asio::io_service& webaccess_service, http_proxy_server_context& server_context)
{
    return std::shared_ptr<http_proxy_server_connection>(new http_proxy_server_connection(std::move(client_socket), picture_classifier,
		webaccess_service, server_context));
}

void http_proxy_server_connection::start()
{
    //this->connection_context.connection_state = proxy_connection_state::read_cipher_data;
    //this->async_read_data_from_proxy_client(1, std::min<std::size_t>(this->rsa_pri.modulus_size(), BUFFER_LENGTH));
    this->connection_context.connection_state = proxy_connection_state::read_http_request_header;
    this->async_read_data_from_proxy_client();
}

void http_proxy_server_connection::async_read_data_from_proxy_client(std::size_t at_least_size, std::size_t at_most_size)
{
    assert(at_least_size <= at_most_size && at_most_size <= BUFFER_LENGTH);
    auto self(this->shared_from_this());
    this->set_timer();
    boost::asio::async_read(this->proxy_client_socket,
        boost::asio::buffer(&this->upgoing_buffer_read[0], at_most_size),
        boost::asio::transfer_at_least(at_least_size),
        this->strand.wrap([this, self](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (this->cancel_timer()) {
                if (!error) {
                    this->on_proxy_client_data_arrived(bytes_transferred);
                }
                else {
                    this->on_error(error);
                }
            }
        })
    );
}

void http_proxy_server_connection::async_read_data_from_origin_server(bool set_timer, std::size_t at_least_size, std::size_t at_most_size)
{
    auto self(this->shared_from_this());
    if (set_timer) {
        this->set_timer();
    }
    boost::asio::async_read(this->origin_server_socket,
        boost::asio::buffer(&this->downgoing_buffer_read[0], at_most_size),
        boost::asio::transfer_at_least(at_least_size),
        this->strand.wrap([this, self](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (this->cancel_timer()) {
                if (!error) {
                    this->on_origin_server_data_arrived(bytes_transferred);
                }
                else {
                    this->on_error(error);
                }
            }
        })
    );
}

void http_proxy_server_connection::async_connect_to_origin_server()
{
    this->connection_context.reconnect_on_error = false;
    if (this->origin_server_socket.is_open()) {
        if (this->request_header->method() == "CONNECT") {
            boost::system::error_code ec;
            this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            this->origin_server_socket.close(ec);
        }
    }

    if (this->origin_server_socket.is_open() &&
        this->request_header->host() == this->connection_context.origin_server_name &&
        this->request_header->port() == this->connection_context.origin_server_port) {
        this->connection_context.reconnect_on_error = true;
        this->on_connect();
    }
    else {
		this->connection_context.origin_server_name = this->request_header->host();
		this->connection_context.origin_server_port = this->request_header->port();

        boost::asio::ip::tcp::resolver::query query(this->connection_context.origin_server_name,
            std::to_string(this->connection_context.origin_server_port));
        auto self(this->shared_from_this());
        this->connection_context.connection_state = proxy_connection_state::resolve_origin_server_address;
        this->set_timer();
        this->resolver.async_resolve(query,
            this->strand.wrap([this, self](const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator iterator) {
                if (this->cancel_timer()) {
                    if (!error) {
                        this->on_resolved(iterator);
                    }
                    else {
                        this->on_error(error);
                    }
                }
            })
        );
    }
}

void http_proxy_server_connection::async_write_request_header_to_origin_server()
{
	auto request_content_begin = this->request_data.begin() + this->request_data.find("\r\n\r\n") + 4;
	this->modified_request_data = this->request_header->method();
	this->modified_request_data.push_back(' ');
	this->modified_request_data += this->request_header->path_and_query();
	this->modified_request_data += " HTTP/";
	this->modified_request_data += this->request_header->http_version();
	this->modified_request_data += "\r\n";

	this->request_header->erase_header("Proxy-Connection");
	this->request_header->erase_header("Proxy-Authorization");

	for (const auto& header : this->request_header->get_headers_map()) {
		this->modified_request_data += std::get<0>(header);
		this->modified_request_data += ": ";
		this->modified_request_data += std::get<1>(header);
		this->modified_request_data += "\r\n";
	}
	this->modified_request_data += "\r\n";
	this->modified_request_data.append(request_content_begin, this->request_data.end());
	this->connection_context.connection_state = proxy_connection_state::write_http_request_header;
	this->async_write_data_to_origin_server(this->modified_request_data.data(), 0, this->modified_request_data.size());
}

void http_proxy_server_connection::async_write_response_header_to_proxy_client()
{
    auto response_content_begin = this->response_data.begin() + this->response_data.find("\r\n\r\n") + 4;

    this->modified_response_data = "HTTP/";
    this->modified_response_data += this->response_header->http_version();
    this->modified_response_data.push_back(' ');
    this->modified_response_data += std::to_string(this->response_header->status_code());
    if (!this->response_header->status_description().empty()) {
        this->modified_response_data.push_back(' ');
        this->modified_response_data += this->response_header->status_description();
    }
    this->modified_response_data += "\r\n";

    for (const auto& header: this->response_header->get_headers_map()) {
        this->modified_response_data += std::get<0>(header);
        this->modified_response_data += ": ";
        this->modified_response_data += std::get<1>(header);
        this->modified_response_data += "\r\n";
    }
    this->modified_response_data += "\r\n";
    this->modified_response_data.append(response_content_begin, this->response_data.end());
    this->connection_context.connection_state = proxy_connection_state::write_http_response_header;
    this->async_write_data_to_proxy_client(this->modified_response_data.data(), 0, this->modified_response_data.size());
}

void http_proxy_server_connection::async_write_data_to_origin_server(const char* write_buffer, std::size_t offset, std::size_t size)
{
    auto self(this->shared_from_this());
    this->set_timer();
    this->origin_server_socket.async_write_some(boost::asio::buffer(write_buffer + offset, size),
        this->strand.wrap([this, self, write_buffer, offset, size](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (this->cancel_timer()) {
                if (!error) { 
                    this->connection_context.reconnect_on_error = false;
                    if (bytes_transferred < size) {
                        this->async_write_data_to_origin_server(write_buffer, offset + bytes_transferred, size - bytes_transferred);
                    }
                    else {
                        this->on_origin_server_data_written();
                    }
                }
                else {
                    this->on_error(error);
                }
            }
        })
    );
}

void http_proxy_server_connection::async_write_data_to_proxy_client(const char* write_buffer, std::size_t offset, std::size_t size)
{
    auto self(this->shared_from_this());
    this->set_timer();
    this->proxy_client_socket.async_write_some(boost::asio::buffer(write_buffer + offset, size),
        this->strand.wrap([this, self, write_buffer, offset, size](const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (this->cancel_timer()) {
                if (!error) {
                    if (bytes_transferred < size) {
                        this->async_write_data_to_proxy_client(write_buffer, offset + bytes_transferred, size - bytes_transferred);
                    }
                    else {
                        this->on_proxy_client_data_written();
                    }
                }
                else {
                    this->on_error(error);
                }
            }
        })
    );
}

void http_proxy_server_connection::start_tunnel_transfer()
{
    this->connection_context.connection_state = proxy_connection_state::tunnel_transfer;
    this->async_read_data_from_proxy_client();
    this->async_read_data_from_origin_server(false);
}

void http_proxy_server_connection::report_error(const std::string& status_code, const std::string& status_description, const std::string& error_message)
{
    this->modified_response_data.clear();
    this->modified_response_data += "HTTP/1.1 ";
    this->modified_response_data += status_code;
    if (!status_description.empty()) {
        this->modified_response_data.push_back(' ');
        this->modified_response_data += status_description;
    }
    this->modified_response_data += "\r\n";
    this->modified_response_data += "Content-Type: text/html\r\n";
    this->modified_response_data += "Server: AzureHttpProxy\r\n";
    this->modified_response_data += "Content-Length: ";

    std::string response_content;
    response_content = "<!DOCTYPE html><html><head><title>";
    response_content += status_code;
    response_content += ' ';
    response_content += status_description;
    response_content += "</title></head><body bgcolor=\"white\"><center><h1>";
    response_content += status_code;
    response_content += ' ';
    response_content += status_description;
    response_content += "</h1>";
    if (!error_message.empty()) {
        response_content += "<br/>";
        response_content += error_message;
        response_content += "</center>";
    }
    response_content += "<hr><center>";
    response_content += "masa server";
    response_content += "</center></body></html>";
    this->modified_response_data += std::to_string(response_content.size());
    this->modified_response_data += "\r\n";
    this->modified_response_data += "Proxy-Connection: close\r\n";
    this->modified_response_data += "\r\n";
    if (!this->request_header || this->request_header->method() != "HEAD") {
        this->modified_response_data += response_content;
    }
    
    this->connection_context.connection_state = proxy_connection_state::report_error;
    auto self(this->shared_from_this());
    this->async_write_data_to_proxy_client(this->modified_response_data.data(), 0 ,this->modified_response_data.size());
}

void http_proxy_server_connection::report_authentication_failed()
{
    std::string content = "<!DOCTYPE html><html><head><title>407 Proxy Authentication Required</title></head>";
    content += "<body bgcolor=\"white\"><center><h1>407 Proxy Authentication Required</h1></center><hr><center>azure http proxy server</center></body></html>";
    this->modified_response_data = "HTTP/1.1 407 Proxy Authentication Required\r\n";
    this->modified_response_data += "Server: AzureHttpProxy\r\n";
    this->modified_response_data += "Proxy-Authenticate: Basic realm=\"masa\"\r\n";
    this->modified_response_data += "Content-Type: text/html\r\n";
    this->modified_response_data += "Connection: Close\r\n";
    this->modified_response_data += "Content-Length: ";
    this->modified_response_data += std::to_string(content.size());
    this->modified_response_data += "\r\n\r\n";
    this->modified_response_data += content;
    //unsigned char temp_buffer[16];
    //for (std::size_t i = 0; i * 16 < this->modified_response_data.size(); ++i) {
        //std::size_t block_length = 16;
        //if (this->modified_response_data.size() - i * 16 < 16) {
            //block_length = this->modified_response_data.size() % 16;
        //}
        //this->encryptor->encrypt(reinterpret_cast<const unsigned char*>(&this->modified_response_data[i * 16]), temp_buffer, block_length);
        //std::copy(temp_buffer, temp_buffer + block_length, reinterpret_cast<unsigned char*>(&this->modified_response_data[i * 16]));
    //}
    this->connection_context.connection_state = proxy_connection_state::report_error;
    this->async_write_data_to_proxy_client(this->modified_response_data.data(), 0, this->modified_response_data.size());
}

void http_proxy_server_connection::set_timer()
{
    //if (this->timer.expires_from_now(std::chrono::seconds(http_proxy_server_config::get_instance().get_timeout())) != 0) {
    if (this->timer.expires_from_now(std::chrono::seconds(kTimeout)) != 0) {
        assert(false);
    }
    auto self(this->shared_from_this());
    this->timer.async_wait(this->strand.wrap([this, self](const boost::system::error_code& error) {
        if (error != boost::asio::error::operation_aborted) {
            this->on_timeout();
        }
    }));
}

bool http_proxy_server_connection::cancel_timer()
{
    std::size_t ret = this->timer.cancel();
    assert(ret <= 1);
    return ret == 1;
}

void http_proxy_server_connection::on_resolved(boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    if (this->origin_server_socket.is_open()) {
        for (auto iter = endpoint_iterator; iter != boost::asio::ip::tcp::resolver::iterator(); ++iter) {
            if (this->connection_context.origin_server_endpoint == iter->endpoint()) {
                this->connection_context.reconnect_on_error = true;
                this->on_connect();
                return;
            }
            boost::system::error_code ec;
            this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            this->origin_server_socket.close(ec);
        }
    }
    this->connection_context.origin_server_endpoint = endpoint_iterator->endpoint();
    auto self(this->shared_from_this());
    this->connection_context.connection_state = proxy_connection_state::connect_to_origin_server;
    this->set_timer();

    this->origin_server_socket.async_connect(endpoint_iterator->endpoint(),
        this->strand.wrap([this, self, endpoint_iterator](const boost::system::error_code& error) mutable {
            if (this->cancel_timer()) {
                if (!error) {
                    this->on_connect();
                }
                else {
                    boost::system::error_code ec;
                    this->origin_server_socket.close(ec);
                    if (++endpoint_iterator != boost::asio::ip::tcp::resolver::iterator()) {
                        this->on_resolved(endpoint_iterator);
                    }
                    else {
                        this->on_error(error);
                    }
                }
            }
        })
    );
}

void http_proxy_server_connection::on_connect()
{
    ////mythxcq
    //const time_t t = time(NULL);
    //struct tm* current_time = localtime(&t);
    //std::cout<<current_time->tm_hour<<":"
      //<<current_time->tm_min<<":"
      //<<current_time->tm_sec<<"@@"
      //<<(unsigned long)this<<"@@"
      //<<"Connected: "
      //<<this->request_header->host()
      ////<<this->request_header->port()<<","
      //<<this->request_header->path_and_query()<<","
      //<<this->request_header->method()
      //<<std::endl;
    if (this->request_header->method() == "CONNECT") {
        //const unsigned char response_message[] = "HTTP/1.1 200 Connection Established\r\nConnection: Close\r\n\r\n";
        //this->modified_response_data.resize(sizeof(response_message) - 1);
        //this->encryptor->encrypt(response_message, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(&this->modified_response_data[0])), this->modified_response_data.size());
        this->modified_response_data = "HTTP/1.1 200 Connection Established\r\nConnection: Close\r\n\r\n";
        this->connection_context.connection_state = proxy_connection_state::report_connection_established;
        this->async_write_data_to_proxy_client(&this->modified_response_data[0], 0, this->modified_response_data.size());
    }
    else {
        this->async_write_request_header_to_origin_server();
    }
}

void http_proxy_server_connection::on_proxy_client_data_arrived(std::size_t bytes_transferred)
{
    //this->decryptor->decrypt(reinterpret_cast<const unsigned char*>(&this->upgoing_buffer_read[0]), reinterpret_cast<unsigned char*>(&this->upgoing_buffer_write[0]), bytes_transferred);
    this->upgoing_buffer_write = this->upgoing_buffer_read;
    if (this->connection_context.connection_state == proxy_connection_state::read_http_request_header) {
        const auto& decripted_data_buffer = this->upgoing_buffer_write;
        this->request_data.append(decripted_data_buffer.begin(), decripted_data_buffer.begin() + bytes_transferred);
        auto double_crlf_pos = this->request_data.find("\r\n\r\n");
        if (double_crlf_pos == std::string::npos) {
            if (this->request_data.size() < MAX_REQUEST_HEADER_LENGTH) {
                this->async_read_data_from_proxy_client();
            }
            else {
                this->report_error("400", "Bad Request", "Request header too long");
            }
            return;
        }

        this->request_header = http_header_parser::parse_request_header(this->request_data.begin(), this->request_data.begin() + double_crlf_pos + 4);
        if (!this->request_header) {
            this->report_error("400", "Bad Request", "Failed to parse the http request header");
            return;
        }
        ////mythxcq
        //const time_t t = time(NULL);
        //struct tm* current_time = localtime(&t);
        //std::cout<<current_time->tm_hour<<":"
          //<<current_time->tm_min<<":"
          //<<current_time->tm_sec<<"@@"
          //<<(unsigned long)this<<"@@"
          //<<"Request: "
          //<<this->request_header->host()
          ////<<this->request_header->port()<<","
          //<<this->request_header->path_and_query()<<","
          //<<this->request_header->method()
          //<<std::endl;

        if (this->request_header->method() != "GET"
            // && this->request_header->method() != "OPTIONS"
            && this->request_header->method() != "HEAD"
            && this->request_header->method() != "POST"
            && this->request_header->method() != "PUT"
            && this->request_header->method() != "DELETE"
            // && this->request_header->method() != "TRACE"
            && this->request_header->method() != "CONNECT") {
            this->report_error("405", "Method Not Allowed", std::string());
            return;
        }
        if (this->request_header->http_version() != "1.1"
            && this->request_header->http_version() != "1.0") {
            this->report_error("505", "HTTP Version Not Supported", std::string());
            return;
        }

        //if (http_proxy_server_config::get_instance().enable_auth()) {
        //    auto proxy_authorization_value = this->request_header->get_header_value("Proxy-Authorization");
        //    bool auth_success = false;
        //    if (proxy_authorization_value) {
        //        if (authentication::get_instance().auth(*proxy_authorization_value) == auth_result::ok) {
        //            auth_success = true;
        //        }
        //    }
        //    if (!auth_success) {
        //        this->report_authentication_failed();
        //        return;
        //    }
        //}

		//init request url and scheme and host
		std::string url = this->request_header->scheme() + "://" + this->request_header->host();
		if (this->request_header->port() != 80)
			url += ":" + std::to_string(this->request_header->port());
		this->read_request_context.scheme_host = url;
		std::string url_val(url + this->request_header->path_and_query());
		this->read_request_context.request_url = url + this->request_header->path_and_query();
		//all lower case to insensitive
		std::transform(this->read_request_context.request_url.begin(), this->read_request_context.request_url.end(),
			this->read_request_context.request_url.begin(), std::tolower);

		//black list filter
		this->read_request_context.domain_name = GetDomainName(this->request_header->host());
		if (url_database_.GetIdTmpBlackList(this->read_request_context.domain_name) ||
			url_database_.GetIdBlackList(this->read_request_context.domain_name)){
			this->url_database_.InsertBlockedPage(this->read_request_context.request_url);
			this->report_error("400", "Host not allowed", std::string());
			return;
		}
		this->read_request_context.id_white_list = url_database_.GetIdWhiteList(this->read_request_context.domain_name);


        if (this->request_header->method() == "CONNECT") {
            this->async_connect_to_origin_server();
            return;
        }
        else {
            if (this->request_header->scheme() != "http") {
                this->report_error("400", "Bad Request", "Unsupported scheme");
                return;
            }
            auto proxy_connection_value = this->request_header->get_header_value("Proxy-Connection");
            auto connection_value = this->request_header->get_header_value("Connection");
            auto string_to_lower_case = [](std::string& str) {
                for (auto iter = str.begin(); iter != str.end(); ++iter) {
                    *iter = std::tolower(static_cast<unsigned char>(*iter));
                }
            };
            if (proxy_connection_value) {
                string_to_lower_case(*proxy_connection_value);
                if (this->request_header->http_version() == "1.1") {
                    this->read_request_context.is_proxy_client_keep_alive = true;
                    if (*proxy_connection_value == "close") {
                        this->read_request_context.is_proxy_client_keep_alive = false;
                    }
                }
                else {
                    assert(this->request_header->http_version() == "1.0");
                    this->read_request_context.is_proxy_client_keep_alive = false;
                    if (*proxy_connection_value == "keep-alive") {
                        this->read_request_context.is_proxy_client_keep_alive = true;
                    }
                }
            }
            else {
                if (this->request_header->http_version() == "1.1") {
                    this->read_request_context.is_proxy_client_keep_alive = true;
                }
                else {
                    this->read_request_context.is_proxy_client_keep_alive = false;
                }
                if (connection_value) {
                    string_to_lower_case(*connection_value);
                    if (this->request_header->http_version() == "1.1" && *connection_value == "close") {
                        this->read_request_context.is_proxy_client_keep_alive = false;
                    }
                    else if (this->request_header->http_version() == "1.0" && *connection_value == "keep-alive") {
                        this->read_request_context.is_proxy_client_keep_alive = true;
                    }
                }
            }

            this->read_request_context.content_length = boost::optional<std::uint64_t>();
            this->read_request_context.content_length_has_read = 0;
            this->read_request_context.chunk_checker = boost::optional<http_chunk_checker>();

            if (this->request_header->method() == "GET" || this->request_header->method() == "HEAD" || this->request_header->method() == "DELETE") {
                this->read_request_context.content_length = boost::optional<std::uint64_t>(0);
            }
            else if (this->request_header->method() == "POST" || this->request_header->method() == "PUT") {
                auto content_length_value = this->request_header->get_header_value("Content-Length");
                auto transfer_encoding_value = this->request_header->get_header_value("Transfer-Encoding");
                if (content_length_value) {
                    try {
                        this->read_request_context.content_length = boost::optional<std::uint64_t>(std::stoull(*content_length_value));
                    }
                    catch (const std::exception&) {
                        this->report_error("400", "Bad Request", "Invalid Content-Length in request");
                        return;
                    }
                    this->read_request_context.content_length_has_read = this->request_data.size() - (double_crlf_pos + 4);
                }
                else if (transfer_encoding_value) {
                    string_to_lower_case(*transfer_encoding_value);
                    if (*transfer_encoding_value == "chunked") {
                        if (!this->read_request_context.chunk_checker->check(this->request_data.begin() + double_crlf_pos + 4, this->request_data.end())) {
                            this->report_error("400", "Bad Request", "Failed to check chunked response");
                            return;
                        }
                        return;
                    }
                    else {
                        this->report_error("400", "Bad Request", "Unsupported Transfer-Encoding");
                        return;
                    }
                }
                else {
                    this->report_error("411", "Length Required", std::string());
                    return;
                }
            }
            else {
                assert(false);
                return;
            }
        }
        this->async_connect_to_origin_server();
    }
    else if (this->connection_context.connection_state == proxy_connection_state::read_http_request_content) {
        if (this->read_request_context.content_length) {
            this->read_request_context.content_length_has_read += bytes_transferred;
        }
        else {
            assert(this->read_request_context.chunk_checker);
            if (!this->read_request_context.chunk_checker->check(this->upgoing_buffer_write.begin(), this->upgoing_buffer_write.begin() + bytes_transferred)) {
                return;
            }
        }
        this->connection_context.connection_state = proxy_connection_state::write_http_request_content;
        this->async_write_data_to_origin_server(this->upgoing_buffer_write.data(), 0, bytes_transferred);
    }
    else if (this->connection_context.connection_state == proxy_connection_state::tunnel_transfer) {
        this->async_write_data_to_origin_server(this->upgoing_buffer_write.data(), 0, bytes_transferred);
    }
}

void http_proxy_server_connection::on_origin_server_data_arrived(std::size_t bytes_transferred)
{
	auto string_to_lower_case = [](std::string& str) {
		for (auto iter = str.begin(); iter != str.end(); ++iter) {
			*iter = std::tolower(static_cast<unsigned char>(*iter));
		}
	};
	//std::cout<<"origin_server_data_arrived: "<<bytes_transferred<<std::endl;
	if (this->connection_context.connection_state == proxy_connection_state::read_http_response_header) {
		//std::cout<<"origin_server_data_arrived read_http_response_header"<<std::endl;
		this->response_data.append(this->downgoing_buffer_read.begin(), this->downgoing_buffer_read.begin() + bytes_transferred);
		auto double_crlf_pos = this->response_data.find("\r\n\r\n");
		if (double_crlf_pos == std::string::npos) {
			if (this->response_data.size() < MAX_RESPONSE_HEADER_LENGTH) {
				this->async_read_data_from_origin_server();
			}
			else {
				this->report_error("502", "Bad Gateway", "Response header too long");
			}
			return;
		}

		//std::cout<<"header end pos: "<<double_crlf_pos<<std::endl;
		this->response_header = http_header_parser::parse_response_header(this->response_data.begin(), this->response_data.begin() + double_crlf_pos + 4);
		if (!this->response_header) {
			this->report_error("502", "Bad Gateway", "Failed to parse response header");
			return;
		}
		if (this->response_header->http_version() != "1.1" && this->response_header->http_version() != "1.0") {
			this->report_error("502", "Bad Gateway", "HTTP version not supported");
			return;
		}
		if (this->response_header->status_code() < 100 || this->response_header->status_code() > 700) {
			this->report_error("502", "Bad Gateway", "Unexpected status code");
			return;
		}
		this->read_response_context.content_length = boost::optional<std::uint64_t>();
		this->read_response_context.content_length_has_read = 0;
		this->read_response_context.is_origin_server_keep_alive = false;
		this->read_response_context.chunk_checker = boost::optional<http_chunk_checker>();
		this->read_response_context.content_type = boost::optional<std::string>();
		this->read_response_context.content_encoding = boost::optional<std::string>();

		auto connection_value = this->response_header->get_header_value("Connection");

		if (this->response_header->http_version() == "1.1") {
			this->read_response_context.is_origin_server_keep_alive = true;
		}
		else {
			this->read_response_context.is_origin_server_keep_alive = false;
		}
		if (connection_value) {
			string_to_lower_case(*connection_value);
			if (*connection_value == "close") {
				this->read_response_context.is_origin_server_keep_alive = false;
			}
			else if (*connection_value == "keep-alive") {
				this->read_response_context.is_origin_server_keep_alive = true;
			}
			else {
				this->report_error("502", "Bad Gateway", std::string());
				return;
			}
		}

		if (this->request_header->method() == "HEAD") {
			this->read_response_context.content_length = boost::optional<std::uint64_t>(0);
		}
		else if (this->response_header->status_code() == 204 || this->response_header->status_code() == 304) {
			// 204 No Content
			// 304 Not Modified
			this->read_response_context.content_length = boost::optional<std::uint64_t>(0);
		}
		else {
			auto content_length_value = this->response_header->get_header_value("Content-Length");
			auto transfer_encoding_value = this->response_header->get_header_value("Transfer-Encoding");
			if (content_length_value) {
				try {
					this->read_response_context.content_length = boost::optional<std::uint64_t>(std::stoull(*content_length_value));
				}
				catch (const std::exception&) {
					this->report_error("502", "Bad Gateway", "Invalid Content-Length in response");
					return;
				}
				this->read_response_context.content_length_has_read = this->response_data.size() - (double_crlf_pos + 4);
			}
			else if (transfer_encoding_value) {
				string_to_lower_case(*transfer_encoding_value);
				if (*transfer_encoding_value == "chunked") {
					this->read_response_context.chunk_checker = boost::optional<http_chunk_checker>(http_chunk_checker());
					if (!this->read_response_context.chunk_checker->check(this->response_data.begin() + double_crlf_pos + 4, this->response_data.end())) {
						this->report_error("502", "Bad Gateway", "Failed to check chunked response");
						return;
					}
				}
				else {
					this->report_error("502", "Bad Gateway", "Unsupported Transfer-Encoding");
					return;
				}
			}
			else if (this->read_response_context.is_origin_server_keep_alive) {
				this->report_error("502", "Bad Gateway", "Miss response length info");
				return;
			}
		}

		//     if(this->response_header->get_header_value("Content-Encoding") ){
		//std::cout << *this->response_header->get_header_value("Content-Encoding")<<": ";
		//std::cout << this->request_header->host() << this->request_header->path_and_query() << std::endl;
		//     }

		//white list, just write response to proxy client

		if (this->response_header->get_header_value("Content-Type")){
			this->read_response_context.content_type = this->response_header->get_header_value("Content-Type");
			std::string content_type = *this->read_response_context.content_type;
			//to ignore case
			//string_to_lower_case(content_type);
			std::transform(content_type.begin(), content_type.end(), content_type.begin(), std::tolower);
			if (content_type.find(kJpegType) != std::string::npos){
				this->read_response_context.content_type = boost::optional<std::string>(kJpegType);
			}
			else if (content_type.find(kPngType) != std::string::npos){
				this->read_response_context.content_type = boost::optional<std::string>(kPngType);
			}
			else if (content_type.find(kHtmlType) != std::string::npos){
				this->read_response_context.content_type = boost::optional<std::string>(kHtmlType);
			}
		}

		if (this->response_header->get_header_value("Content-Encoding")){
			this->read_response_context.content_encoding = this->response_header->get_header_value("Content-Encoding");
			std::string content_encoding = *this->read_response_context.content_encoding;
			//to ignore case
			//string_to_lower_case(content_encoding);
			std::transform(content_encoding.begin(), content_encoding.end(), content_encoding.begin(), std::tolower);
			if (content_encoding.find(kGzipEncoding) != std::string::npos){
				this->read_response_context.content_encoding = boost::optional<std::string>(kGzipEncoding);
			}
		}


		//for images
		//set max-age to zero to force browser to send request for this pic again
		//bool is_image = false;
		//if ( this->read_response_context.content_type &&
		//	( (*this->read_response_context.content_type) == kJpegType
		//	|| (*this->read_response_context.content_type) == kPngType )
		//){
		//	is_image = true;
		//	this->response_header->SetHeaderValue("Cache-Control", "max-age=0");
		//}

		if ( //content encoding
			(!this->read_response_context.content_encoding
			|| (*this->read_response_context.content_encoding == kGzipEncoding))
			//content type
			&& this->read_response_context.content_type
			//&& (this->read_response_context.is_origin_server_keep_alive)
			//not redirect
			//&& (!this->response_header->get_header_value("Location"))
			//satus code need to be 200, it is 3xx when redirect
			&& this->response_header->status_code() == 200
			&& this->request_header->method() == "GET"
			&&
			(
			(
			((*this->read_response_context.content_type) == kJpegType
			|| (*this->read_response_context.content_type) == kPngType
			) &&
			(this->read_response_context.content_length && *this->read_response_context.content_length > 0) &&
			(
			kFilterMode == FILTER_ALL || !this->read_request_context.id_white_list
			)
			)
			//transfer type
			//only handle html with chunked, because it is difficult to reconsctuct the message body for iamge.
			||
			(
			(*this->read_response_context.content_type) == kHtmlType &&
			!this->read_request_context.id_white_list &&
			((this->read_response_context.content_length && *this->read_response_context.content_length > 0)
			|| this->read_response_context.chunk_checker)
			)
			)
			)
		{
			BOOST_LOG_TRIVIAL(debug) << "Need to process: " << this->request_header->host() << this->request_header->path_and_query();
			//if the body if very short, and all data has been received now
			if (IsAllRecved()){
				HandleCompleteResponse();
			}
			else{
				this->connection_context.connection_state = proxy_connection_state::wait_for_total_http_response_content;
				this->async_read_data_from_origin_server();
			}
		}
		else
			this->async_write_response_header_to_proxy_client();
	}
	else if (this->connection_context.connection_state == proxy_connection_state::wait_for_total_http_response_content){
		if (this->read_response_context.content_length) {
			this->read_response_context.content_length_has_read += bytes_transferred;
		}
		else if (this->read_response_context.chunk_checker) {
			if (!this->read_response_context.chunk_checker->check(this->downgoing_buffer_read.begin(), this->downgoing_buffer_read.begin() + bytes_transferred)) {
				return;
			}
		}
		this->response_data.append(this->downgoing_buffer_read.begin(), this->downgoing_buffer_read.begin() + bytes_transferred);
		//all is read
		//if (this->read_response_context.content_length_has_read >= *this->read_response_context.content_length){
		if (IsAllRecved()){
			HandleCompleteResponse();
		}
		else{
			this->async_read_data_from_origin_server();
		}
	}
	else if (this->connection_context.connection_state == proxy_connection_state::read_http_response_content) {
		//std::cout<<"origin_server_data_arrived read_http_response_content"<<std::endl;
		if (this->read_response_context.content_length) {
			this->read_response_context.content_length_has_read += bytes_transferred;
		}
		else if (this->read_response_context.chunk_checker) {
			if (!this->read_response_context.chunk_checker->check(this->downgoing_buffer_read.begin(), this->downgoing_buffer_read.begin() + bytes_transferred)) {
				return;
			}
		}
		this->connection_context.connection_state = proxy_connection_state::write_http_response_content;
		this->downgoing_buffer_write = this->downgoing_buffer_read;
		this->async_write_data_to_proxy_client(this->downgoing_buffer_write.data(), 0, bytes_transferred);
	}
	else if (this->connection_context.connection_state == proxy_connection_state::tunnel_transfer) {
		this->downgoing_buffer_write = this->downgoing_buffer_read;
		this->async_write_data_to_proxy_client(this->downgoing_buffer_write.data(), 0, bytes_transferred);
	}
}

void http_proxy_server_connection::on_proxy_client_data_written()
{
	if (this->connection_context.connection_state == proxy_connection_state::tunnel_transfer) {
		this->async_read_data_from_origin_server();
	}
	else if (this->connection_context.connection_state == proxy_connection_state::write_http_response_header
		|| this->connection_context.connection_state == proxy_connection_state::write_http_response_content) {
		if ((this->read_response_context.content_length && this->read_response_context.content_length_has_read >= *this->read_response_context.content_length)
			|| (this->read_response_context.chunk_checker && this->read_response_context.chunk_checker->is_complete())) {
			boost::system::error_code ec;
			if (!this->read_response_context.is_origin_server_keep_alive) {
				this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
				this->origin_server_socket.close(ec);
			}
			if (this->read_request_context.is_proxy_client_keep_alive) {
				this->request_data.clear();
				this->response_data.clear();
				this->request_header = boost::optional<http_request_header>();
				this->response_header = boost::optional<http_response_header>();
				this->read_request_context.content_length = boost::optional<std::uint64_t>();
				this->read_request_context.chunk_checker = boost::optional<http_chunk_checker>();
				this->read_response_context.content_length = boost::optional<std::uint64_t>();
				this->read_response_context.chunk_checker = boost::optional<http_chunk_checker>();
				this->connection_context.connection_state = proxy_connection_state::read_http_request_header;
				this->async_read_data_from_proxy_client();
			}
			else {
				this->proxy_client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
				this->proxy_client_socket.close(ec);
			}
		}
		else {
			this->connection_context.connection_state = proxy_connection_state::read_http_response_content;
			this->async_read_data_from_origin_server();
		}
	}
	else if (this->connection_context.connection_state == proxy_connection_state::report_connection_established) {
		this->start_tunnel_transfer();
	}
	else if (this->connection_context.connection_state == proxy_connection_state::report_error) {
		boost::system::error_code ec;
		if (this->origin_server_socket.is_open()) {
			this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			this->origin_server_socket.close(ec);
		}
		if (this->proxy_client_socket.is_open()) {
			this->proxy_client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			this->proxy_client_socket.close(ec);
		}
	}
}

void http_proxy_server_connection::on_origin_server_data_written()
{
	if (this->connection_context.connection_state == proxy_connection_state::tunnel_transfer) {
		this->async_read_data_from_proxy_client();
	}
	else if (this->connection_context.connection_state == proxy_connection_state::write_http_request_header
		|| this->connection_context.connection_state == proxy_connection_state::write_http_request_content) {
		if (this->read_request_context.content_length) {
			if (this->read_request_context.content_length_has_read < *this->read_request_context.content_length) {
				this->connection_context.connection_state = proxy_connection_state::read_http_request_content;
				this->async_read_data_from_proxy_client();
			}
			else {
				this->connection_context.connection_state = proxy_connection_state::read_http_response_header;
				this->async_read_data_from_origin_server();
			}
		}
		else {
			assert(this->read_request_context.chunk_checker);
			if (!this->read_request_context.chunk_checker->is_complete()) {
				this->connection_context.connection_state = proxy_connection_state::read_http_request_content;
				this->async_read_data_from_proxy_client();
			}
			else {
				this->connection_context.connection_state = proxy_connection_state::read_http_response_header;
				this->async_read_data_from_origin_server();
			}
		}
	}
}

void http_proxy_server_connection::on_error(const boost::system::error_code& error)
{
	if (this->connection_context.connection_state == proxy_connection_state::resolve_origin_server_address) {
		this->report_error("504", "Gateway Timeout", "Failed to resolve the hostname");
	}
	else if (this->connection_context.connection_state == proxy_connection_state::connect_to_origin_server) {
		this->report_error("502", "Bad Gateway", "Failed to connect to origin server");
	}
	else if (this->connection_context.connection_state == proxy_connection_state::write_http_request_header && this->connection_context.reconnect_on_error) {
		boost::system::error_code ec;
		this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		this->origin_server_socket.close(ec);
		this->async_connect_to_origin_server();
	}
	else {
		boost::system::error_code ec;
		if (this->origin_server_socket.is_open()) {
			this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			this->origin_server_socket.close(ec);
		}
		if (this->proxy_client_socket.is_open()) {
			this->proxy_client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			this->proxy_client_socket.close(ec);
		}
	}
}

void http_proxy_server_connection::on_timeout()
{
	boost::system::error_code ec;
	if (this->origin_server_socket.is_open()) {
		this->origin_server_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		this->origin_server_socket.close(ec);
	}
	if (this->proxy_client_socket.is_open()) {
		this->proxy_client_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		this->proxy_client_socket.close(ec);
	}
}


//void http_proxy_server_connection::OnClassify(int mode)
void http_proxy_server_connection::OnClassify(std::string pic, int type)
{
	//BOOST_LOG_TRIVIAL(debug) << "Classify pic: " << BuildRequestUrl() << std::endl;
	if (type>=2){
		//BOOST_LOG_TRIVIAL(info) << "Sexy or Porn pic: " << this->read_request_context.request_url << std::endl;
		//insert to porn_pics database
		url_database_.InsertPornPic(this->read_request_context.request_url, type);
		//store the image;
		std::ofstream of(std::string(kImageCacheDir) + std::string("/") + UrlEncode(this->read_request_context.request_url), std::fstream::binary);
		of << this->read_response_context.decompressed_origin_data; of.close();
		//detect porn host according to porn pic only
		if (type == 3){
			BOOST_LOG_TRIVIAL(info) << "Porn pic: " << this->read_request_context.request_url;
			server_context_.imgsrc_dict_mutex.lock();
			for (auto eachset : server_context_.imgsrc_dict){
				if (eachset.second.find(this->read_request_context.request_url) != eachset.second.end()){
					boost::network::uri::uri porn_page_uri(eachset.first);
					BOOST_LOG_TRIVIAL(info) << "\tHit: " << porn_page_uri.host() << porn_page_uri.path();
					//write to porn detect table of database
					//domain name, html url, pic url, detect time
					std::string domain_name(GetDomainName(porn_page_uri.host()));
					url_database_.InsertPornPage(domain_name, eachset.first, this->read_request_context.request_url);
					//check number of porn pics this domain_name contains
					int num = url_database_.GetPornPicNumOfDomainName(domain_name);
					BOOST_LOG_TRIVIAL(debug) << "Number of porn pics in: " << domain_name << " is: " << num;
					//add to temp black list, if contains too many porn pics
					//if (num > kPornPicNumThd){//&& !url_database_.GetCountWhiteList(domain_name)){
					if (num >= kPornPicNumThd && this->read_request_context.id_white_list == 0){
						if (url_database_.GetIdTmpBlackList(domain_name) == 0)
						{
							BOOST_LOG_TRIVIAL(info) << "Add " << domain_name << " to temp black list";
							url_database_.InsertIntoTmpBlackList(domain_name);
						}
						//add this domain name to web server
					}
				}
			}
			server_context_.imgsrc_dict_mutex.unlock();
		}
	}
	//insert into processed_pics

	auto response_content_begin = this->response_data.begin() + this->response_data.find("\r\n\r\n") + 4;

	this->modified_response_data = "HTTP/";
	this->modified_response_data += this->response_header->http_version();
	this->modified_response_data.push_back(' ');
	this->modified_response_data += std::to_string(this->response_header->status_code());
	if (!this->response_header->status_description().empty()) {
		this->modified_response_data.push_back(' ');
		this->modified_response_data += this->response_header->status_description();
	}
	this->modified_response_data += "\r\n";

	//const std::string& jpeg_place_holder = http_proxy_server_config::get_instance().GetJpegPlaceHolder();
	//if(mode >=0)
	//this->response_header->SetHeaderValue("Content-Length", std::to_string(jpeg_place_holder.size()));
	//if(mode == 1){
	pic = this->Compress(pic);
	this->response_header->SetHeaderValue("Content-Length", std::to_string(pic.size()));
	//}
	for (const auto& header : this->response_header->get_headers_map()) {
		this->modified_response_data += std::get<0>(header);
		this->modified_response_data += ": ";
		this->modified_response_data += std::get<1>(header);
		this->modified_response_data += "\r\n";
	}
	this->modified_response_data += "\r\n";
	//if(mode ==1)
	//  this->modified_response_data.append(jpeg_place_holder);
	//else
	//  this->modified_response_data.append(response_content_begin, this->response_data.end());
	this->modified_response_data.append(pic);
	this->connection_context.connection_state = proxy_connection_state::write_http_response_header;
	this->async_write_data_to_proxy_client(this->modified_response_data.data(), 0, this->modified_response_data.size());
	//this->async_write_response_header_to_proxy_client();
}

bool http_proxy_server_connection::IsAllRecved(){
	if (this->read_response_context.content_length &&
		this->read_response_context.content_length_has_read >= *this->read_response_context.content_length){
		return true;
	}
	else if (this->read_response_context.chunk_checker &&
		this->read_response_context.chunk_checker->is_complete()){
		return true;
	}
	else
		return false;
}

static void search_for_links(GumboNode* node, std::string scheme_host, std::set<std::string> &srclist) {
	if (node->type != GUMBO_NODE_ELEMENT) {
		return;
	}
	GumboAttribute* src;
	if (node->v.element.tag == GUMBO_TAG_IMG &&
		(src = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
		//srclist.insert(std::string(src->value));
		std::string src_val(src->value);
		//transform src to lower case
		if (src_val.size() > 0 && src_val[0] == '/')
			src_val = scheme_host + src_val;
		std::transform(src_val.begin(), src_val.end(), src_val.begin(), std::tolower);
		srclist.insert(src_val);
		//std::cout << src->value << std::endl;
	}

	GumboVector* children = &node->v.element.children;
	for (unsigned int i = 0; i < children->length; ++i) {
		search_for_links(static_cast<GumboNode*>(children->data[i]), scheme_host, srclist);
	}
}

void http_proxy_server_connection::HandleCompleteResponse(){
	//std::cout << "All Recved: " << this->request_header->host() << this->request_header->path_and_query() << std::endl;
	//filter the response
	auto response_content_begin = this->response_data.begin() + this->response_data.find("\r\n\r\n") + 4;
	std::string response_content_data(response_content_begin, this->response_data.end());

	//extract the data from chunk, it only happens for html
	if (this->read_response_context.chunk_checker)
		response_content_data = this->read_response_context.chunk_checker->GetData();

	//if content-encoding == gzip, decompress
	std::string decompressed_response_content_data = Decompress(response_content_data);
	this->read_response_context.decompressed_origin_data = decompressed_response_content_data;

	if (*this->read_response_context.content_type == kHtmlType){
		//std::cout << this->request_header->host() << std::endl;
		//if (server_context_.imgsrc_dict.find(BuildRequestUrl()) == server_context_.imgsrc_dict.end()){
		//server_context_.imgsrc_dict[BuildRequestUrl()] = std::set<std::string>();
		//}
		//use whole host+path&query as key, ignore port
		//std::set<std::string>& srcset = server_context_.imgsrc_dict[];
		std::set<std::string> srcset;
		GumboOutput* output = gumbo_parse(decompressed_response_content_data.c_str());
		//search for links and insert to the set
		search_for_links(output->root, this->read_request_context.scheme_host, srcset);
		gumbo_destroy_output(&kGumboDefaultOptions, output);
		//add to map
		if (srcset.size() > 0){
			BOOST_LOG_TRIVIAL(info) << this->read_request_context.request_url << ":";
			for (auto src : srcset){
				BOOST_LOG_TRIVIAL(info) << "\t" << src;
			}
			server_context_.imgsrc_dict_mutex.lock();
			bool exist = false;
			for (auto eachset : server_context_.imgsrc_dict){
				//if exist, replace it
				if (eachset.first == this->read_request_context.request_url){
					eachset.second = std::move(srcset);
					exist = true;
					break;
				}
			}
			//else add new, and remove the oldest
			if (!exist){
				server_context_.imgsrc_dict.push_back(std::make_pair(this->read_request_context.request_url, std::move(srcset)));
				if (server_context_.imgsrc_dict.size() > kMaxHtmlBufferSize){
					server_context_.imgsrc_dict.erase(server_context_.imgsrc_dict.begin());
				}
			}
			//server_context_.imgsrc_dict[BuildRequestUrl()] = std::move(srcset);
			server_context_.imgsrc_dict_mutex.unlock();
			//check if the pic is porn according database, otherwise the cached pic will not be classified again
			//then we cannot identify the cached pic
			//....
		}
		//then send back
		this->async_write_response_header_to_proxy_client();
	}
	else if (*this->read_response_context.content_type == kJpegType
		|| *this->read_response_context.content_type == kPngType){
		std::shared_ptr<http_proxy_server_connection> self(this->shared_from_this());
		picture_classifier_.AsyncClassifyPicture(*this->read_response_context.content_type, decompressed_response_content_data,
			this->strand.wrap([this, self](std::string oimg, int type) {
			this->OnClassify(oimg, type);
		})
			);
	}
}

std::string http_proxy_server_connection::Decompress(std::string in){
	if (this->read_response_context.content_encoding){
		std::string decompressed;
		boost::iostreams::filtering_ostream os;
		if (*this->read_response_context.content_encoding == kGzipEncoding){
			os.push(boost::iostreams::gzip_decompressor());
		}
		//else if (*this->response_header->get_header_value("Content-Encoding") == "deflate"){
		//	os.push(boost::iostreams::zlib_decompressor());
		//}
		os.push(boost::iostreams::back_inserter(decompressed));

		boost::iostreams::write(os, in.data(), in.size());
		os.reset();
		return decompressed;
	}
	else{
		return in;
	}
}

std::string http_proxy_server_connection::Compress(std::string in){
	if (this->read_response_context.content_encoding){
		std::string compressed;
		boost::iostreams::filtering_ostream os;
		if (*this->read_response_context.content_encoding == kGzipEncoding){
			os.push(boost::iostreams::gzip_compressor());
		}
		//else if (*this->response_header->get_header_value("Content-Encoding") == "deflate"){
		//	os.push(boost::iostreams::zlib_compressor());
		//}
		os.push(boost::iostreams::back_inserter(compressed));

		boost::iostreams::write(os, in.data(), in.size());
		os.reset();
		return compressed;
	}
	else{
		return in;
	}
}

std::string http_proxy_server_connection::BuildRequestUrl(){
	std::string url = this->request_header->scheme() + "://" + this->request_header->host();
	if (this->request_header->port() != 80)
		url += ":" + std::to_string(this->request_header->port());
	std::string url_val(url + this->request_header->path_and_query());
	//std::transform(url_val.begin(), url_val.end(), url_val.begin(), std::tolower);
	return url_val;
}

std::string http_proxy_server_connection::GetDomainName(std::string host)
{
	http_result_t     result;
	string_t         *domain = &result.domain;

	memset(&result, 0, sizeof(http_result_t));
	std::string indomain(host);
	std::transform(indomain.begin(), indomain.end(), indomain.begin(), std::tolower);

	domain->data = &indomain[0];
	domain->len = indomain.size();

	//if cannot parse, then return host directly
	if (parse_domain(tree, &result, domain) != 0) {
		//BOOST_THROW_EXCEPTION(FatalException()<<MessageInfo("Cannt parse the domain: ")<<MessageInfo(host));
		BOOST_LOG_TRIVIAL(warning) << "Cannot parse the host: " << host;
		return host;
	}

	public_suffix_t *ps = &result.complex_domain;

	return std::string(ps->domain.data, ps->domain.len);
}


} // namespace azure_proxy
