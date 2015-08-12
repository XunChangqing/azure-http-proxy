#ifndef HEADER_MISC_MASA_FILTER
#define HEADER_MISC_MASA_FILTER
#include <string>
#include <boost/exception/all.hpp>
#include <boost/throw_exception.hpp>
#include <boost/log/trivial.hpp>

namespace azure_proxy{
enum FilterMode{FILTER_PORN_SITE, FILTER_ALL};
//temp constants
extern const char* kJpegType;
extern const char* kPngType;
extern const char* kHtmlType;
extern const char* kGzipEncoding;
extern const int kMaxHtmlBufferSize;
extern const enum FilterMode kFilterMode;

//configurations
extern const int kPornPicNumThd;
extern const char *kBindAddress;
extern const int kBindPort;
extern const char *kDeployProto;
extern const char *kModelName;
extern const char *kMeanName;
extern const int kTimeout;
extern const int kWorkers;
//extern const char* kProxyLogDir;
extern const float kPornThd;
extern const int kMinWidth;
extern const int kMinHeigth;
//extern const char* kImageCacheDir;
extern const char* kCreateTmpDomainNameUrl;

//config
class GlobalConfig{
public:
	static GlobalConfig* GetInstance();
	static void LoadConfig(std::string work_dir);
	std::string GetWorkDir(){ return work_dir_; }
	std::string GetImageCacheDir(){ return images_cache_dir_; }
	std::string GetPornDBPath(){ return porn_db_path_; }
	std::string GetLogsDir(){ return logs_dir_; }
private:
	static GlobalConfig *kSingleInstance;
	GlobalConfig(std::string work_dir);

	std::string work_dir_;
	std::string images_cache_dir_;
	std::string porn_db_path_;
	std::string logs_dir_;
};

//url helppers
unsigned char ToHex(unsigned char x);
unsigned char FromHex(unsigned char x);
std::string UrlEncode(const std::string& str);
std::string UrlDecode(const std::string& str);

//boost exceptions
typedef boost::error_info<struct tag_message_info, const std::string> MessageInfo;
struct FatalException : virtual boost::exception, virtual std::exception { }; 

//boost log
void InitLogging(std::string collector_target);

}
#endif
