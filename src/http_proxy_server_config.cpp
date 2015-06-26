/*
 *    http_proxy_server_config.cpp:
 *
 *    Copyright (C) 2013-2015 limhiaoing <blog.poxiao.me> All Rights Reserved.
 *
 */

#include <memory>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#else
extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
}
#endif

#include "authentication.hpp"
#include "http_proxy_server_config.hpp"
#include "jsonxx/jsonxx.h"

namespace azure_proxy {

http_proxy_server_config::http_proxy_server_config()
{
}

bool http_proxy_server_config::load_config(const std::string& config_data)
{
    bool rollback = true;
    std::shared_ptr<bool> auto_rollback(&rollback, [this](bool* rollback) {
        if (*rollback) {
            this->config_map.clear();
            authentication::get_instance().remove_all_users();
        }
    });

    jsonxx::Object json_obj;
    if (!json_obj.parse(config_data)) {
        std::cerr << "Failed to parse config" << std::endl;
        return false;
    }
    if (json_obj.has<jsonxx::String>("bind_address")) {
        this->config_map["bind_address"] = std::string(json_obj.get<jsonxx::String>("bind_address"));
    }
    else {
        this->config_map["bind_address"] = std::string("0.0.0.0");
    }
    if (json_obj.has<jsonxx::Number>("listen_port")) {
        this->config_map["listen_port"] = static_cast<unsigned short>(json_obj.get<jsonxx::Number>("listen_port"));
    }
    else {
        this->config_map["listen_port"] = static_cast<unsigned short>(8090);
    }
    //if (!json_obj.has<jsonxx::String>("rsa_private_key")) {
        //std::cerr << "Could not find \"rsa_private_key\" in config or it's value is not a string" << std::endl;
        //return false;
    //}
    if (json_obj.has<jsonxx::Number>("timeout")) {
        int timeout = static_cast<int>(json_obj.get<jsonxx::Number>("timeout"));
        this->config_map["timeout"] = static_cast<unsigned int>(timeout < 30 ? 30 : timeout);
    }
    else {
        this->config_map["timeout"] = 240ul;
    }
    if (json_obj.has<jsonxx::Number>("workers")) {
        int threads = static_cast<int>(json_obj.get<jsonxx::Number>("workers"));
        this->config_map["workers"] = static_cast<unsigned int>(threads < 1 ? 1 : (threads > 16 ? 16 : threads));
    }
    else {
        this->config_map["workers"] = 4ul;
    }
    if (json_obj.has<jsonxx::Boolean>("auth")) {
        this->config_map["auth"] = json_obj.get<jsonxx::Boolean>("auth");
        if (!json_obj.has<jsonxx::Array>("users")) {
            std::cerr << "Could not find \"users\" in config or it's value is not a array" << std::endl;
            return false;
        }
        const jsonxx::Array& users_array = json_obj.get<jsonxx::Array>("users");
        for (size_t i = 0; i < users_array.size(); ++i) {
            if (!users_array.has<jsonxx::Object>(i) || !users_array.get<jsonxx::Object>(i).has<jsonxx::String>("username") || !users_array.get<jsonxx::Object>(i).has<jsonxx::String>("username")) {
                std::cerr << "The value of \"users\" contains unexpected element" << std::endl;
                return false;
            }
            authentication::get_instance().add_user(users_array.get<jsonxx::Object>(i).get<jsonxx::String>("username"),
                users_array.get<jsonxx::Object>(i).get<jsonxx::String>("password"));
        }
    }
    else {
        this->config_map["auth"] = false;
    }

    if (json_obj.has<jsonxx::Boolean>("filter_mode")) {
        this->config_map["filter_mode"] = json_obj.get<jsonxx::Boolean>("filter_mode");
    }
    else {
        this->config_map["filter_mode"] = false;
    }

    if (json_obj.has<jsonxx::Boolean>("request_bypass")) {
        this->config_map["request_bypass"] = json_obj.get<jsonxx::Boolean>("request_bypass");
    }
    else {
        this->config_map["request_bypass"] = false;
    }

    if (json_obj.has<jsonxx::Boolean>("response_filter")) {
        this->config_map["response_filter"] = json_obj.get<jsonxx::Boolean>("response_filter");
    }
    else {
        this->config_map["response_filter"] = false;
    }

    std::string jpeg_name = "p/jpeg_place_holder.jpg";
    std::ifstream jpeg_file(jpeg_name, std::ios::binary);
    if(jpeg_file.is_open())
    {
      std::ostringstream jpeg_ostr;
      jpeg_ostr << jpeg_file.rdbuf();
      jpeg_file.close();
      std::cout<<"jpeg file size: "<<jpeg_ostr.str().size()<<std::endl;
      this->config_map["jpeg_place_holder"] = jpeg_ostr.str();
    }
    else
      std::cout<<"Can not open jpeg place holder file!"<<std::endl;

    this->config_map["caffe_deploy_proto"] = std::string("nin/deploy.prototxt");
    this->config_map["caffe_model"] = std::string("nin/model_20150615_2_256_iter_20000.caffemodel");
    this->config_map["caffe_mean"] = std::string("nin/imagenet_mean.binaryproto");

    rollback = false;
    return true;
}

bool http_proxy_server_config::load_config()
{
    std::string config_data;
#ifdef _WIN32
    wchar_t path_buffer[MAX_PATH];
    if (GetModuleFileNameW(NULL, path_buffer, MAX_PATH) == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::cerr << "Failed to get retrieve the path of the executable file" << std::endl;
    }
    std::wstring config_file_path(path_buffer);
    config_file_path.resize(config_file_path.find_last_of(L'\\') + 1);
    config_file_path += L"server.json";
    std::shared_ptr<std::remove_pointer<HANDLE>::type> config_file_handle(
        CreateFileW(config_file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL),
        [](HANDLE native_handle) {
            if (native_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(native_handle);
            }
    });
    if (config_file_handle.get() == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open config file \"server.json\"" << std::endl;
        return false;
    }
    char ch;
    DWORD size_read = 0;
    BOOL read_result =  ReadFile(config_file_handle.get(), &ch, 1, &size_read, NULL);
    while (read_result != FALSE && size_read != 0) {
        config_data.push_back(ch);
        read_result =  ReadFile(config_file_handle.get(), &ch, 1, &size_read, NULL);
    }
    if (read_result == FALSE) {
        std::cerr << "Failed to read data from config file" << std::endl;
        return false;
    }
#else
    auto bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1) {
        bufsize = 16384;
    }
    std::unique_ptr<char[]> buf(new char[bufsize]);
    passwd pwd, *result = nullptr;
    getpwuid_r(getuid(), &pwd, buf.get(), bufsize, &result);
    if (result == nullptr) {
        return false;
    }
    std::string config_path = pwd.pw_dir;
    config_path += "/.ahps/server.json";
    std::ifstream ifile(config_path.c_str());
    if (!ifile.is_open()) {
        std::cerr << "Failed to open \"" << config_path << "\"" << std::endl;
        return false;
    }
    char ch;
    while (ifile.get(ch)) {
        config_data.push_back(ch);
    }
#endif
    return this->load_config(config_data);
}

const std::string& http_proxy_server_config::get_bind_address() const
{
    return this->get_config_value<const std::string&>("bind_address");
}

unsigned short http_proxy_server_config::get_listen_port() const
{
    return this->get_config_value<unsigned short>("listen_port");
}

unsigned int http_proxy_server_config::get_timeout() const
{
    return this->get_config_value<unsigned int>("timeout");
}

unsigned int http_proxy_server_config::get_workers() const
{
    return this->get_config_value<unsigned int>("workers");
}

bool http_proxy_server_config::enable_auth() const
{
    return this->get_config_value<bool>("auth");
}

bool http_proxy_server_config::enable_filter_mode() const{
    return this->get_config_value<bool>("filter_mode");
}
bool http_proxy_server_config::enable_request_bypass() const{
    return this->get_config_value<bool>("request_bypass");
}
bool http_proxy_server_config::enable_response_filter() const{
    return this->get_config_value<bool>("response_filter");
}
const std::string& http_proxy_server_config::GetJpegPlaceHolder() const
{
    return this->get_config_value<const std::string&>("jpeg_place_holder");
}
const std::string& http_proxy_server_config::GetDeployProto() const{
    return this->get_config_value<const std::string&>("caffe_deploy_proto");
}
const std::string& http_proxy_server_config::GetModel() const{
    return this->get_config_value<const std::string&>("caffe_model");
}
const std::string& http_proxy_server_config::GetMean() const{
    return this->get_config_value<const std::string&>("caffe_mean");
}

http_proxy_server_config& http_proxy_server_config::get_instance()
{
    static http_proxy_server_config instance;
    return instance;
}

} // namespace azure_proxy
