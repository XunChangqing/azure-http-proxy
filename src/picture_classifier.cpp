#include <thread>
#include <boost/bind.hpp>

#include "picture_classifier.hpp"
namespace azure_proxy{

PictureClassifier::PictureClassifier(
    boost::asio::io_service &classification_service)
    : classification_service_(classification_service) {}

void PictureClassifier::AsyncClassifyPicture(std::string format,
                                             std::string picture,
                                             std::function<void(int)> handler) {
  classification_service_.post(boost::bind(&PictureClassifier::ClassifyPicture,
        this, format, picture, handler));
}

void PictureClassifier::ClassifyPicture(std::string format, std::string picture,
                                        std::function<void(int)> handler) {
  std::cout << "porn classify: "<<std::this_thread::get_id()<<std::endl;
  handler(0);
}

}
