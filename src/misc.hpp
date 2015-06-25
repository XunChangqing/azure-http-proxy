#ifndef HEADER_MISC_MASA_FILTER
#define HEADER_MISC_MASA_FILTER
#include <string>
namespace azure_proxy{
unsigned char ToHex(unsigned char x);
unsigned char FromHex(unsigned char x);
std::string UrlEncode(const std::string& str);
std::string UrlDecode(const std::string& str);
}
#endif
