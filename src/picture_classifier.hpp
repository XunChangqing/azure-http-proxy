#ifndef HEADER_PICTURE_CLASSIFIER
#define HEADER_PICTURE_CLASSIFIER

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//#include <caffe/caffe.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <boost/asio.hpp>
#include <boost/optional.hpp>
namespace caffe { template <typename Dtype> class Net; }
namespace azure_proxy {
	
  using std::string;
class PictureClassifier {
  boost::asio::io_service &classification_service_;

public:
  PictureClassifier(boost::asio::io_service &classification_service);
  bool LoadModel(const std::string &model_file, const std::string &trained_file,
                 const std::string &mean_file);
  void AsyncClassifyPicture(std::string format, std::string picture,
                            std::function<void(std::string, bool)> handler);

private:
  void ClassifyPicture(std::string format, std::string picture,
                       std::function<void(std::string, bool)> handler);
  // std::vector<Prediction> Classify(const cv::Mat &img, int N);
  void SetMean(const std::string &mean_file);
  std::vector<float> Predict(const cv::Mat &img);
  void WrapInputLayer(std::vector<cv::Mat> *input_channels);
  void Preprocess(const cv::Mat &img, std::vector<cv::Mat> *input_channels);

private:
  std::shared_ptr<caffe::Net<float>> net_;
  cv::Size input_geometry_;
  int num_channels_;
  cv::Mat mean_;
  std::vector<std::string> labels_;
};
}
#endif
