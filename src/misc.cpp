#include "misc.hpp"

#include <fstream>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/utility/empty_deleter.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <cassert>


namespace azure_proxy{

const char* kJpegType = "image/jpeg";
const char* kPngType = "image/png";
const char* kHtmlType = "text/html";
const char* kGzipEncoding = "gzip";
const int kMaxHtmlBufferSize = 50;
const int kPornPicNumThd = 50;
const enum FilterMode kFilterMode = FILTER_PORN_SITE;

const char *kBindAddress = "127.0.0.1";
const int kBindPort = 8090;
const char *kDeployProto = "nin/deploy.prototxt";
const char *kModelName = "nin/20150726/model_256__iter_50000.caffemodel";
const char *kMeanName = "nin/imagenet_mean.binaryproto";
const int kTimeout = 30;
const int kWorkers = 2;
const char* kProxyLogDir = "Logs";
const float kPornThd = 0.5f;
const int kMinWidth = 200;
const int kMinHeigth = 180;
const char* kImageCacheDir = "images";

unsigned char ToHex(unsigned char x) 
{ 
  return  x > 9 ? x + 55 : x + 48; 
}

unsigned char FromHex(unsigned char x) 
{ 
  unsigned char y;
  if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
  else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
  else if (x >= '0' && x <= '9') y = x - '0';
  else assert(0);
  return y;
}

std::string UrlEncode(const std::string& str)
{
  std::string strTemp = "";
  size_t length = str.length();
  for (size_t i = 0; i < length; i++)
  {
    if (isalnum((unsigned char)str[i]) || 
        (str[i] == '-') ||
        (str[i] == '_') || 
        (str[i] == '.') || 
        (str[i] == '~'))
      strTemp += str[i];
    else if (str[i] == ' ')
      strTemp += "+";
    else
    {
      strTemp += '%';
      strTemp += ToHex((unsigned char)str[i] >> 4);
      strTemp += ToHex((unsigned char)str[i] % 16);
    }
  }
  return strTemp;
}

std::string UrlDecode(const std::string& str)
{
  std::string strTemp = "";
  size_t length = str.length();
  for (size_t i = 0; i < length; i++)
  {
    if (str[i] == '+') strTemp += ' ';
    else if (str[i] == '%')
    {
      assert(i + 2 < length);
      unsigned char high = FromHex((unsigned char)str[++i]);
      unsigned char low = FromHex((unsigned char)str[++i]);
      strTemp += high*16 + low;
    }
    else strTemp += str[i];
  }
  return strTemp;
}

//boost log

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

////[ example_tutorial_file_advanced
void InitLogging(std::string collector_target)
{
	boost::shared_ptr< logging::core > core = logging::core::get();

	boost::shared_ptr< sinks::text_file_backend > file_backend =
		boost::make_shared< sinks::text_file_backend >(
		keywords::file_name = "LOG_%Y-%m-%d_%H-%M-%S.%N.log",
		keywords::rotation_size = 5 * 1024 * 1024,
		keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
		keywords::auto_flush = true
		);

	// Wrap it into the frontend and register in the core.
	// The backend requires synchronization in the frontend.
	typedef sinks::synchronous_sink< sinks::text_file_backend > sink_t;
	boost::shared_ptr< sink_t > file_sink(new sink_t(file_backend));
	file_sink->set_formatter(
		expr::stream
		<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "[%Y-%m-%d %H:%M:%S]@")
		<< "[" << expr::attr<logging::attributes::current_thread_id::value_type>("ThreadID")
		<< "]: <" << logging::trivial::severity
		<< "> " << expr::smessage
		);

	file_sink->locked_backend()->set_file_collector(sinks::file::make_collector(
		keywords::target = collector_target,
		keywords::max_size = 16 * 1024 * 1024,
		keywords::min_free_space = 100 * 1024 * 1024
		));
	//scan for old files in logs
	file_sink->locked_backend()->scan_for_files();

	file_sink->set_filter(
		logging::trivial::severity >= logging::trivial::info
		);
	core->add_sink(file_sink);

	 //Construct the console sink
	typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
	boost::shared_ptr< text_sink > console_sink = boost::make_shared< text_sink >();

	boost::shared_ptr< std::ostream > clog_stream(&std::clog, boost::empty_deleter());
	console_sink->locked_backend()->add_stream(clog_stream);
	console_sink->set_formatter(
		expr::stream
		<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "[%H:%M:%S]@")
		<< "[" << expr::attr<logging::attributes::current_thread_id::value_type>("ThreadID")
		<< "]: <" << logging::trivial::severity
		<< "> " << expr::smessage
		);
	// Register the sink in the logging core
	core->add_sink(console_sink);
	logging::add_common_attributes();
}
}
