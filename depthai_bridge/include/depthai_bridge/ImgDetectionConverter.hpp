#pragma once

#include <deque>
#include <memory>
#include <string>

#include "depthai/pipeline/datatype/ImgDetections.hpp"
#include "ros/time.h"
#include "vision_msgs/Detection2DArray.h"

namespace dai {

namespace ros {

namespace VisionMsgs = vision_msgs;
using Detection2DArrayPtr = VisionMsgs::Detection2DArray::Ptr;
class ImgDetectionConverter {
   public:
    // DetectionConverter() = default;
    ImgDetectionConverter(std::string frameName, int width, int height, bool normalized = false, bool getBaseDeviceTimestamp = false);

    /**
     * @brief Handles cases in which the ROS time shifts forward or backward
     *  Should be called at regular intervals or on-change of ROS time, depending
     *  on monitoring.
     * 
     */
    void updateRosBaseTime();

    void toRosMsg(std::shared_ptr<dai::ImgDetections> inNetData, std::deque<VisionMsgs::Detection2DArray>& opDetectionMsgs);

    Detection2DArrayPtr toRosMsgPtr(std::shared_ptr<dai::ImgDetections> inNetData);

   private:
    int _width, _height;
    const std::string _frameName;
    bool _normalized;
    std::chrono::time_point<std::chrono::steady_clock> _steadyBaseTime;
    ::ros::Time _rosBaseTime;
    bool _getBaseDeviceTimestamp;
    // For handling ROS time shifts and debugging
    int64_t _totalNsChange{ 0 };
    static const int64_t ZERO_TIME_DELTA_NS { 100 };
};

/** TODO(sachin): Do we need to have ros msg -> dai bounding box ?
 * is there any situation where we would need to have xlinkin to take bounding
 * box as input. One scenario would to take this as input and use ImageManip
 * node to crop the roi of the image. Since it is not available yet. Leaving
 * it out for now to speed up on other tasks feel free to raise a issue if you
 * feel that feature is good to have...
 */

}  // namespace ros

namespace rosBridge = ros;

}  // namespace dai
