#include <thread>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <caffe/caffe.hpp>
#include "picture_classifier.hpp"
#include "misc.hpp"

namespace azure_proxy {
	using namespace caffe;

PictureClassifier::PictureClassifier(
    boost::asio::io_service &classification_service)
    // const string &label_file)
    : classification_service_(classification_service) {}

bool PictureClassifier::LoadModel(const std::string &model_file,
                                  const std::string &trained_file,
                                  const std::string &mean_file) {
  Caffe::set_mode(Caffe::CPU);
  /* Load the network. */
  net_.reset(new Net<float>(model_file, TEST)); //, TEST
  net_->CopyTrainedLayersFrom(trained_file);

  CHECK_EQ(net_->num_inputs(), 1) << "Network should have exactly one input.";
  CHECK_EQ(net_->num_outputs(), 1) << "Network should have exactly one output.";

  Blob<float> *input_layer = net_->input_blobs()[0];
  num_channels_ = input_layer->channels();
  CHECK(num_channels_ == 3 || num_channels_ == 1)
      << "Input layer should have 1 or 3 channels.";
  input_geometry_ = cv::Size(input_layer->width(), input_layer->height());

  /* Load the binaryproto mean file. */
  SetMean(mean_file);

  //[> Load labels. <]
  // std::ifstream labels(label_file.c_str());
  // CHECK(labels) << "Unable to open labels file " << label_file;
  // string line;
  // while (std::getline(labels, line))
  // labels_.push_back(string(line));

  Blob<float> *output_layer = net_->output_blobs()[0];
  // CHECK_EQ(labels_.size(), output_layer->channels())
  //<< "Number of labels is different from the output layer dimension.";
	return true;
}

static bool PairCompare(const std::pair<float, int> &lhs,
                        const std::pair<float, int> &rhs) {
  return lhs.first > rhs.first;
}

/* Return the indices of the top N values of vector v. */
static std::vector<int> Argmax(const std::vector<float> &v, int N) {
  std::vector<std::pair<float, int>> pairs;
  for (size_t i = 0; i < v.size(); ++i)
    pairs.push_back(std::make_pair(v[i], i));
  std::partial_sort(pairs.begin(), pairs.begin() + N, pairs.end(), PairCompare);

  std::vector<int> result;
  for (int i = 0; i < N; ++i)
    result.push_back(pairs[i].second);
  return result;
}

/* Return the top N predictions. */
// std::vector<Prediction> PictureClassifier::Classify(const cv::Mat& img, int
// N) {
// std::vector<float> output = Predict(img);

// std::vector<int> maxN = Argmax(output, N);

// std::vector<Prediction> predictions;
// for (int i = 0; i < N; ++i) {
// int idx = maxN[i];
// predictions.push_back(std::make_pair(labels_[idx], output[idx]));
//}

// return predictions;
//}

/* Load the mean file in binaryproto format. */
void PictureClassifier::SetMean(const string &mean_file) {
  BlobProto blob_proto;
  ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);

  /* Convert from BlobProto to Blob<float> */
  Blob<float> mean_blob;
  mean_blob.FromProto(blob_proto);
  CHECK_EQ(mean_blob.channels(), num_channels_)
      << "Number of channels of mean file doesn't match input layer.";

  /* The format of the mean file is planar 32-bit float BGR or grayscale. */
  std::vector<cv::Mat> channels;
  float *data = mean_blob.mutable_cpu_data();
  for (int i = 0; i < num_channels_; ++i) {
    /* Extract an individual channel. */
    cv::Mat channel(mean_blob.height(), mean_blob.width(), CV_32FC1, data);
    channels.push_back(channel);
    data += mean_blob.height() * mean_blob.width();
  }

  /* Merge the separate channels into a single image. */
  cv::Mat mean;
  cv::merge(channels, mean);

  /* Compute the global mean pixel value and create a mean image
   * filled with this value. */
  cv::Scalar channel_mean = cv::mean(mean);
  mean_ = cv::Mat(input_geometry_, mean.type(), channel_mean);
}

std::vector<float> PictureClassifier::Predict(const cv::Mat &img) {
  Blob<float> *input_layer = net_->input_blobs()[0];

  input_layer->Reshape(1, num_channels_, input_geometry_.height,
                       input_geometry_.width);
  /* Forward dimension change to all layers. */
  net_->Reshape();

  std::vector<cv::Mat> input_channels;
  WrapInputLayer(&input_channels);

  Preprocess(img, &input_channels);

  net_->ForwardPrefilled();

  /* Copy the output layer to a std::vector */
  Blob<float> *output_layer = net_->output_blobs()[0];
  const float *begin = output_layer->cpu_data();
  const float *end = begin + output_layer->channels();
  return std::vector<float>(begin, end);
}

/* Wrap the input layer of the network in separate cv::Mat objects
 * (one per channel). This way we save one memcpy operation and we
 * don't need to rely on cudaMemcpy2D. The last preprocessing
 * operation will write the separate channels directly to the input
 * layer. */
void PictureClassifier::WrapInputLayer(std::vector<cv::Mat> *input_channels) {
  Blob<float> *input_layer = net_->input_blobs()[0];

  int width = input_layer->width();
  int height = input_layer->height();
  float *input_data = input_layer->mutable_cpu_data();
  for (int i = 0; i < input_layer->channels(); ++i) {
    cv::Mat channel(height, width, CV_32FC1, input_data);
    input_channels->push_back(channel);
    input_data += width * height;
  }
}

void PictureClassifier::Preprocess(const cv::Mat &img,
                                   std::vector<cv::Mat> *input_channels) {
  /* Convert the input image to the input image format of the network. */
  cv::Mat sample;
  if (img.channels() == 3 && num_channels_ == 1)
    cv::cvtColor(img, sample, CV_BGR2GRAY);
  else if (img.channels() == 4 && num_channels_ == 1)
    cv::cvtColor(img, sample, CV_BGRA2GRAY);
  else if (img.channels() == 4 && num_channels_ == 3)
    cv::cvtColor(img, sample, CV_BGRA2BGR);
  else if (img.channels() == 1 && num_channels_ == 3)
    cv::cvtColor(img, sample, CV_GRAY2BGR);
  else
    sample = img;

  cv::Mat sample_resized;
  if (sample.size() != input_geometry_)
    cv::resize(sample, sample_resized, input_geometry_);
  else
    sample_resized = sample;

  cv::Mat sample_float;
  if (num_channels_ == 3)
    sample_resized.convertTo(sample_float, CV_32FC3);
  else
    sample_resized.convertTo(sample_float, CV_32FC1);

  cv::Mat sample_normalized;
  cv::subtract(sample_float, mean_, sample_normalized);

  /* This operation will write the separate BGR planes directly to the
   * input layer of the network because it is wrapped by the cv::Mat
   * objects in input_channels. */
  cv::split(sample_normalized, *input_channels);

  CHECK(reinterpret_cast<float *>(input_channels->at(0).data) ==
        net_->input_blobs()[0]->cpu_data())
      << "Input channels are not wrapping the input layer of the network.";
}

void PictureClassifier::AsyncClassifyPicture(std::string format,
                                             std::string picture,
                                             std::function<void(std::string, int)> handler) {
  classification_service_.post(boost::bind(&PictureClassifier::ClassifyPicture,
                                           this, format, picture, handler));
}

void PictureClassifier::ClassifyPicture(std::string format, std::string picture,
                                        std::function<void(std::string, int)> handler) {
  //std::cout << "porn classify: " << std::this_thread::get_id() << std::endl;
  // cv::Mat img = cv::imread(file, -1);
  std::vector<char> picdata(picture.begin(), picture.end());
  cv::Mat img = cv::imdecode(cv::Mat(picdata), -1);
  if (img.empty() ||
	  img.cols < kMinWidth ||
	  img.rows < kMinHeigth) {
	  //std::cout << "Unable to decode image or too small!" << std::endl;
	  LOG(INFO) << "Unable to decode image or too small!";
	  handler(picture, false);
	  return;
  }
  // CHECK(!img.empty()) << "Unable to decode image " << picture;
  // std::vector<Prediction> predictions = classifier.Classify(img, 2);
  std::vector<float> output = Predict(img);
  if (output.size()<4)
	  BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Output of classifier is not 4 classes!"));
  //BOOST_LOG_TRIVIAL(debug) << "Prob of Types: " << output[0] << "  " << output[1] << "  " << output[2] << "  " << output[3];

  if (output[3] > kPornThd){
	  //draw tag on the picture
	  //cv::blur(img, img, cv::Size(5, 5));
	  cv::rectangle(img, cv::Rect(0, 0, img.cols, img.rows), cv::Scalar(0, 0, 255), (float)img.cols*0.1f);
	  //encode to jpeg
	  size_t pos;
	  std::string postfix = ".jpg";
	  if ((pos = format.rfind('/')) != std::string::npos){
		  postfix.replace(1, std::string::npos, format.substr(pos + 1, std::string::npos));
	  }
	  std::vector<uchar> obuf;
	  cv::imencode(postfix, img, obuf);
	  std::string oimg(obuf.begin(), obuf.end());
	  handler(oimg, 3);
  }
  else if (output[2] > 0.25f){
	  //draw tag on the picture
	  //cv::blur(img, img, cv::Size(5, 5));
	  cv::rectangle(img, cv::Rect(0, 0, img.cols, img.rows), cv::Scalar(0, 255, 255), (float)img.cols*0.1f);
	  //encode to jpeg
	  size_t pos;
	  std::string postfix = ".jpg";
	  if ((pos = format.rfind('/')) != std::string::npos){
		  postfix.replace(1, std::string::npos, format.substr(pos + 1, std::string::npos));
	  }
	  std::vector<uchar> obuf;
	  cv::imencode(postfix, img, obuf);
	  std::string oimg(obuf.begin(), obuf.end());
	  handler(oimg, 2);
  }
  else if (output[1] > 0.25f){
	  //rettype = 1;
	  handler(picture, 1);
  }
  else{
	  //rettype = 0;
	  handler(picture, 0);
  }
  //std::vector<int> maxN = Argmax(output, 1);
  //std::cout << "Porn classify result: " << maxN[0] << std::endl;
  //handler(maxN[0]);
  //if (maxN[0] == 1){//porn
  //}
  //else
	 // handler(picture, false);
}

void PictureClassifier::Test(std::string imagesdir){
	namespace fs = boost::filesystem;
	fs::path p(imagesdir);
	fs::path op("output_images");
	fs::create_directories(op);

	boost::asio::io_service tmp_classification_service;
	PictureClassifier classifier(tmp_classification_service);
	if(!classifier.LoadModel(kDeployProto, kModelName, kMeanName))
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Faied load caffe model"));
	//auto handler = [std::string filename](std::string image, int type){
	//	BOOST_LOG_TRIVIAL(info) << 
	//}
	if (exists(p) && is_directory(p)){
		BOOST_LOG_TRIVIAL(info) << "Testing in: " << imagesdir;
		//std::copy(fs::directory_iterator(p), fs::directory_iterator(), std::ostream_iterator<fs::directory_entry>(std::cout, "\n"));
		for (auto iter = fs::directory_iterator(p);iter!=fs::directory_iterator() ;++iter){
			if (fs::is_regular_file(iter->path())){
				fs::path np(op);
				np /= iter->path().filename();
				std::string ofilename = np.string();
				//BOOST_LOG_TRIVIAL(info) << np;
				BOOST_LOG_TRIVIAL(info) << iter->path().filename().string();
				//BOOST_LOG_TRIVIAL(info) << filename;
				std::ostringstream ostr;
				std::ifstream infs(iter->path().string(), std::fstream::binary);
				ostr << infs.rdbuf();
				infs.close();
				//std::string input_img = ostr.str();
				classifier.ClassifyPicture("jpeg", ostr.str(), [ofilename](std::string img, int type){
					std::ofstream of(ofilename, std::fstream::binary | std::fstream::out);
					of << img;
					of.close();
				});
			}
		}
		//classifier.
	}

}
}
