
#include "depthai_bridge/DisparityConverter.hpp"

#include "depthai_bridge/depthaiUtility.hpp"

namespace dai {

namespace ros {

DisparityConverter::DisparityConverter(
    const std::string frameName, float focalLength, float baseline, float minDepth, float maxDepth, bool getBaseDeviceTimestamp)
    : _frameName(frameName),
      _focalLength(focalLength),
      _baseline(baseline / 100.0),
      _minDepth(minDepth / 100.0),
      _maxDepth(maxDepth / 100.0),
      _steadyBaseTime(std::chrono::steady_clock::now()),
      _getBaseDeviceTimestamp(getBaseDeviceTimestamp) {
    _rosBaseTime = ::ros::Time::now();
}

void DisparityConverter::updateRosBaseTime()
{
    ::ros::Time currentRosTime = ::ros::Time::now();
    std::chrono::time_point<std::chrono::steady_clock> currentSteadyTime =
        std::chrono::steady_clock::now();
    // In nanoseconds
    auto expectedOffset = std::chrono::duration_cast<std::chrono::nanoseconds>(currentSteadyTime - _steadyBaseTime).count();
    uint64_t previousBaseTimeNs = _rosBaseTime.toNSec();
    _rosBaseTime = _rosBaseTime.fromNSec(currentRosTime.toNSec() - expectedOffset);
    uint64_t newBaseTimeNs = _rosBaseTime.toNSec();
    int64_t diff = static_cast<int64_t>(newBaseTimeNs - previousBaseTimeNs);
    _totalNsChange += diff;
    if(::abs(diff) > ZERO_TIME_DELTA_NS)
    {
        // Has been updated
        DEPTHAI_ROS_DEBUG_STREAM("ROS BASE TIME CHANGE: ", "ROS base time changed by " <<
            std::to_string(diff) << " ns. Total change: " << std::to_string(_totalNsChange) << " ns. New time: " <<
            std::to_string(_rosBaseTime.toNSec()) << " ns.");
    }

}

void DisparityConverter::toRosMsg(std::shared_ptr<dai::ImgFrame> inData, std::deque<DisparityMsgs::DisparityImage>& outDispImageMsgs) {
    std::chrono::_V2::steady_clock::time_point tstamp;
    if(_getBaseDeviceTimestamp)
        tstamp = inData->getTimestampDevice();
    else
        tstamp = inData->getTimestamp();
    DisparityMsgs::DisparityImage outDispImageMsg;
    outDispImageMsg.header.frame_id = _frameName;
    outDispImageMsg.f = _focalLength;
    outDispImageMsg.min_disparity = _focalLength * _baseline / _maxDepth;
    outDispImageMsg.max_disparity = _focalLength * _baseline / _minDepth;

    outDispImageMsg.T = _baseline / 100.0;  // converting cm to meters

    ImageMsgs::Image& outImageMsg = outDispImageMsg.image;
    outDispImageMsg.header.stamp = getFrameTime(_rosBaseTime, _steadyBaseTime, tstamp);

    outImageMsg.encoding = sensor_msgs::image_encodings::TYPE_32FC1;
    outImageMsg.header = outDispImageMsg.header;
    if(inData->getType() == dai::RawImgFrame::Type::RAW8) {
        outDispImageMsg.delta_d = 1.0;
        size_t size = inData->getData().size() * sizeof(float);
        outImageMsg.data.resize(size);
        outImageMsg.height = inData->getHeight();
        outImageMsg.width = inData->getWidth();
        outImageMsg.step = size / inData->getHeight();
        outImageMsg.is_bigendian = true;

        std::vector<float> convertedData(inData->getData().begin(), inData->getData().end());
        unsigned char* imageMsgDataPtr = reinterpret_cast<unsigned char*>(outImageMsg.data.data());

        unsigned char* daiImgData = reinterpret_cast<unsigned char*>(convertedData.data());

        // TODO(Sachin): Try using assign since it is a vector
        // img->data.assign(packet.data->cbegin(), packet.data->cend());
        memcpy(imageMsgDataPtr, daiImgData, size);

    } else {
        outDispImageMsg.delta_d = 1.0 / 32.0;
        size_t size = inData->getHeight() * inData->getWidth() * sizeof(float);
        outImageMsg.data.resize(size);
        outImageMsg.height = inData->getHeight();
        outImageMsg.width = inData->getWidth();
        outImageMsg.step = size / inData->getHeight();
        outImageMsg.is_bigendian = true;
        unsigned char* daiImgData = reinterpret_cast<unsigned char*>(inData->getData().data());

        std::vector<int16_t> raw16Data(inData->getHeight() * inData->getWidth());
        unsigned char* raw16DataPtr = reinterpret_cast<unsigned char*>(raw16Data.data());
        memcpy(raw16DataPtr, daiImgData, inData->getData().size());
        std::vector<float> convertedData;
        std::transform(
            raw16Data.begin(), raw16Data.end(), std::back_inserter(convertedData), [](int16_t disp) -> std::size_t { return static_cast<float>(disp) / 32.0; });

        unsigned char* imageMsgDataPtr = reinterpret_cast<unsigned char*>(outImageMsg.data.data());
        unsigned char* convertedDataPtr = reinterpret_cast<unsigned char*>(convertedData.data());
        memcpy(imageMsgDataPtr, convertedDataPtr, size);
    }
    outDispImageMsgs.push_back(outDispImageMsg);
    return;
}

DisparityImagePtr DisparityConverter::toRosMsgPtr(std::shared_ptr<dai::ImgFrame> inData) {
    std::deque<DisparityMsgs::DisparityImage> msgQueue;
    toRosMsg(inData, msgQueue);
    auto msg = msgQueue.front();
    DisparityImagePtr ptr = boost::make_shared<DisparityMsgs::DisparityImage>(msg);
    return ptr;
}

}  // namespace ros
}  // namespace dai
