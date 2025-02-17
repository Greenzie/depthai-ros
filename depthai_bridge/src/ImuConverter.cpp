
#include "depthai_bridge/ImuConverter.hpp"

#include "depthai_bridge/depthaiUtility.hpp"

namespace dai {

namespace ros {

ImuConverter::ImuConverter(const std::string& frameName,
                           ImuSyncMethod syncMode,
                           double linear_accel_cov,
                           double angular_velocity_cov,
                           double rotation_cov,
                           double magnetic_field_cov,
                           bool enable_rotation)
    : _frameName(frameName),
      _syncMode(syncMode),
      _linear_accel_cov(linear_accel_cov),
      _angular_velocity_cov(angular_velocity_cov),
      _rotation_cov(rotation_cov),
      _magnetic_field_cov(magnetic_field_cov),
      _enable_rotation(enable_rotation),
      _sequenceNum(0),
      _steadyBaseTime(std::chrono::steady_clock::now()) {
    _rosBaseTime = ::ros::Time::now();
}

ImuConverter::~ImuConverter() = default;

void ImuConverter::updateRosBaseTime()
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

void ImuConverter::fillImuMsg(dai::IMUReportAccelerometer report, ImuMsgs::Imu& msg) {
    msg.linear_acceleration.x = report.x;
    msg.linear_acceleration.y = report.y;
    msg.linear_acceleration.z = report.z;
    msg.linear_acceleration_covariance = {_linear_accel_cov, 0.0, 0.0, 0.0, _linear_accel_cov, 0.0, 0.0, 0.0, _linear_accel_cov};
}

void ImuConverter::fillImuMsg(dai::IMUReportGyroscope report, ImuMsgs::Imu& msg) {
    msg.angular_velocity.x = report.x;
    msg.angular_velocity.y = report.y;
    msg.angular_velocity.z = report.z;
    msg.angular_velocity_covariance = {_angular_velocity_cov, 0.0, 0.0, 0.0, _angular_velocity_cov, 0.0, 0.0, 0.0, _angular_velocity_cov};
}

void ImuConverter::fillImuMsg(dai::IMUReportRotationVectorWAcc report, ImuMsgs::Imu& msg) {
    if(_enable_rotation) {
        msg.orientation.x = report.i;
        msg.orientation.y = report.j;
        msg.orientation.z = report.k;
        msg.orientation.w = report.real;
        msg.orientation_covariance = {_rotation_cov, 0.0, 0.0, 0.0, _rotation_cov, 0.0, 0.0, 0.0, _rotation_cov};
    } else {
        msg.orientation.x = 0.0;
        msg.orientation.y = 0.0;
        msg.orientation.z = 0.0;
        msg.orientation.w = 1.0;
        msg.orientation_covariance = {-1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    }
}

void ImuConverter::fillImuMsg(dai::IMUReportMagneticField report, ImuMsgs::Imu& msg) {
    return;
}

void ImuConverter::fillImuMsg(dai::IMUReportAccelerometer report, depthai_ros_msgs::ImuWithMagneticField& msg) {
    fillImuMsg(report, msg.imu);
}

void ImuConverter::fillImuMsg(dai::IMUReportGyroscope report, depthai_ros_msgs::ImuWithMagneticField& msg) {
    fillImuMsg(report, msg.imu);
}

void ImuConverter::fillImuMsg(dai::IMUReportRotationVectorWAcc report, depthai_ros_msgs::ImuWithMagneticField& msg) {
    fillImuMsg(report, msg.imu);
}

void ImuConverter::fillImuMsg(dai::IMUReportMagneticField report, depthai_ros_msgs::ImuWithMagneticField& msg) {
    msg.field.magnetic_field.x = report.x;
    msg.field.magnetic_field.y = report.y;
    msg.field.magnetic_field.z = report.z;
    msg.field.magnetic_field_covariance = {_magnetic_field_cov, 0.0, 0.0, 0.0, _magnetic_field_cov, 0.0, 0.0, 0.0, _magnetic_field_cov};
}

void ImuConverter::toRosMsg(std::shared_ptr<dai::IMUData> inData, std::deque<ImuMsgs::Imu>& outImuMsgs) {
    if(_syncMode != ImuSyncMethod::COPY) {
        FillImuData_LinearInterpolation(inData->packets, outImuMsgs);
    } else {
        for(int i = 0; i < inData->packets.size(); ++i) {
            auto accel = inData->packets[i].acceleroMeter;
            auto gyro = inData->packets[i].gyroscope;
            auto rot = inData->packets[i].rotationVector;
            auto magn = inData->packets[i].magneticField;
            ImuMsgs::Imu msg;
            CreateUnitMessage(accel, gyro, rot, magn, msg, accel.timestamp);
            outImuMsgs.push_back(msg);
        }
    }
}

void ImuConverter::toRosDaiMsg(std::shared_ptr<dai::IMUData> inData, std::deque<depthai_ros_msgs::ImuWithMagneticField>& outImuMsgs) {
    if(_syncMode != ImuSyncMethod::COPY) {
        FillImuData_LinearInterpolation(inData->packets, outImuMsgs);
    } else {
        for(int i = 0; i < inData->packets.size(); ++i) {
            auto accel = inData->packets[i].acceleroMeter;
            auto gyro = inData->packets[i].gyroscope;
            auto rot = inData->packets[i].rotationVector;
            auto magn = inData->packets[i].magneticField;
            depthai_ros_msgs::ImuWithMagneticField msg;
            CreateUnitMessage(accel, gyro, rot, magn, msg, accel.timestamp);
            outImuMsgs.push_back(msg);
        }
    }
}

}  // namespace ros
}  // namespace dai