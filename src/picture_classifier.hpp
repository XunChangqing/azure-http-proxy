#ifndef HEADER_PICTURE_CLASSIFIER
#define HEADER_PICTURE_CLASSIFIER

#include <boost/asio.hpp>
#include <boost/optional.hpp>

namespace azure_proxy{
class PictureClassifier{
  boost::asio::io_service& classification_service_;
  public:
  PictureClassifier(boost::asio::io_service& classification_service);
  void AsyncClassifyPicture(std::string format, std::string picture, std::function<void (int)> handler);
  private:
  void ClassifyPicture(std::string format, std::string picture, std::function<void (int)> handler);
};

}
#endif
