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
extern const int kPornPicNumThd;
extern const enum FilterMode kFilterMode;

//url helppers
unsigned char ToHex(unsigned char x);
unsigned char FromHex(unsigned char x);
std::string UrlEncode(const std::string& str);
std::string UrlDecode(const std::string& str);

//boost exceptions
typedef boost::error_info<struct tag_message_info, const std::string> MessageInfo;
struct FatalException : virtual boost::exception, virtual std::exception { }; 

//boost log
void InitLogging();

}
#endif
