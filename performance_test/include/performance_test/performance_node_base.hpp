/* Software License Agreement (BSD License)
 *
 *  Copyright (c) 2019, iRobot ROS
 *  All rights reserved.
 *
 *  This file is part of ros2-performance, which is released under BSD-3-Clause.
 *  You may use, distribute and modify this code under the BSD-3-Clause license.
 */

#ifndef PERFORMANCE_TEST__PERFORMANCE_NODE_BASE_HPP_
#define PERFORMANCE_TEST__PERFORMANCE_NODE_BASE_HPP_

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/qos.hpp"

#include "performance_test/communication.hpp"
#include "performance_test/events_logger.hpp"
#include "performance_test/tracker.hpp"

namespace performance_test
{

struct NodeInterfaces
{
  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr base;
  rclcpp::node_interfaces::NodeClockInterface::SharedPtr clock;
  rclcpp::node_interfaces::NodeGraphInterface::SharedPtr graph;
  rclcpp::node_interfaces::NodeLoggingInterface::SharedPtr logging;
  rclcpp::node_interfaces::NodeParametersInterface::SharedPtr parameters;
  rclcpp::node_interfaces::NodeServicesInterface::SharedPtr services;
  rclcpp::node_interfaces::NodeTimersInterface::SharedPtr timers;
  rclcpp::node_interfaces::NodeTopicsInterface::SharedPtr topics;
};

class PerformanceNodeBase
{
public:
  using SharedPtr = std::shared_ptr<PerformanceNodeBase>;

  explicit PerformanceNodeBase(const NodeInterfaces & node_interfaces);

  virtual ~PerformanceNodeBase() = default;

  rclcpp::node_interfaces::NodeBaseInterface::SharedPtr
  get_node_base();

  rclcpp::node_interfaces::NodeGraphInterface::SharedPtr
  get_node_graph();

  rclcpp::Logger
  get_node_logger();

  const char *
  get_node_name();

  template<typename Msg>
  void add_subscriber(
    const std::string & topic_name,
    msg_pass_by_t msg_pass_by,
    Tracker::TrackingOptions tracking_options = Tracker::TrackingOptions(),
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default);

  template<typename Msg>
  void add_periodic_publisher(
    const std::string & topic_name,
    std::chrono::microseconds period,
    msg_pass_by_t msg_pass_by,
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default,
    size_t size = 0);

  template<typename Msg>
  void add_publisher(
    const std::string & topic_name,
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default);

  template<typename Srv>
  void add_server(
    const std::string & service_name,
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default);

  template<typename Srv>
  void add_periodic_client(
    const std::string & service_name,
    std::chrono::microseconds period,
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default,
    size_t size = 0);

  template<typename Srv>
  void add_client(
    const std::string & service_name,
    const rmw_qos_profile_t & qos_profile = rmw_qos_profile_default);

  void add_timer(std::chrono::microseconds period, std::function<void()> callback);

  using Trackers = std::vector<std::pair<std::string, Tracker>>;

  std::shared_ptr<Trackers> sub_and_client_trackers();

  std::shared_ptr<Trackers> pub_trackers();

  void set_events_logger(std::shared_ptr<EventsLogger> ev);

  int get_executor_id();

  std::vector<std::string> get_published_topics();

private:
  void store_subscription(
    rclcpp::SubscriptionBase::SharedPtr sub,
    const std::string & topic_name,
    const Tracker::TrackingOptions & tracking_options);

  void store_publisher(
    rclcpp::PublisherBase::SharedPtr pub,
    const std::string & topic_name,
    const Tracker::TrackingOptions & tracking_options);

  void store_client(
    rclcpp::ClientBase::SharedPtr client,
    const std::string & service_name,
    const Tracker::TrackingOptions & tracking_options);

  void store_server(
    rclcpp::ServiceBase::SharedPtr server,
    const std::string & service_name,
    const Tracker::TrackingOptions & tracking_options);

  performance_test_msgs::msg::PerformanceHeader create_msg_header(
    rclcpp::Time publish_time,
    float pub_frequency,
    Tracker::TrackingNumber tracking_number,
    size_t msg_size);

  template<typename Msg>
  void _publish(
    const std::string & name,
    msg_pass_by_t msg_pass_by,
    size_t size,
    std::chrono::microseconds period);

  template<typename DataT>
  typename std::enable_if<
    (!std::is_same<DataT, std::vector<uint8_t>>::value), size_t>::type
  resize_msg(DataT & data, size_t size);

  template<typename DataT>
  typename std::enable_if<
    (std::is_same<DataT, std::vector<uint8_t>>::value), size_t>::type
  resize_msg(DataT & data, size_t size);

  template<typename MsgType>
  void _topic_callback(const std::string & name, MsgType msg);

  void _handle_sub_received_msg(
    const std::string & topic_name,
    const performance_test_msgs::msg::PerformanceHeader & msg_header);

  template<typename Srv>
  void _request(const std::string & name, size_t size);

  template<typename Srv>
  void _response_received_callback(
    const std::string & name,
    std::shared_ptr<typename Srv::Request> request,
    typename rclcpp::Client<Srv>::SharedFuture result_future);

  void _handle_client_received_response(
    const std::string & service_name,
    const performance_test_msgs::msg::PerformanceHeader & request_header,
    const performance_test_msgs::msg::PerformanceHeader & response_header);

  template<typename Srv>
  void _service_callback(
    const std::string & name,
    const std::shared_ptr<rmw_request_id_t> request_header,
    const std::shared_ptr<typename Srv::Request> request,
    const std::shared_ptr<typename Srv::Response> response);

  performance_test_msgs::msg::PerformanceHeader
  _handle_server_received_request(
    const std::string & service_name,
    const performance_test_msgs::msg::PerformanceHeader & request_header);

  // Client blocking call does not work with timers
  // Use a lock variable to avoid calling when you are already waiting
  bool _client_lock = false;

  NodeInterfaces m_node_interfaces;

  // A topic-name indexed map to store the publisher pointers with their
  // trackers.
  std::map<std::string, std::pair<rclcpp::PublisherBase::SharedPtr, Tracker>> _pubs;

  // A topic-name indexed map to store the subscriber pointers with their
  // trackers.
  std::map<std::string, std::pair<rclcpp::SubscriptionBase::SharedPtr, Tracker>> _subs;

  using ClientsTuple = std::tuple<rclcpp::ClientBase::SharedPtr, Tracker, Tracker::TrackingNumber>;
  // A service-name indexed map to store the client pointers with their
  // trackers.
  std::map<std::string, ClientsTuple> _clients;

  // A service-name indexed map to store the server pointers with their
  // trackers.
  std::map<std::string, std::pair<rclcpp::ServiceBase::SharedPtr, Tracker>> _servers;

  std::vector<rclcpp::TimerBase::SharedPtr> _timers;

  std::shared_ptr<EventsLogger> _events_logger;

  int m_executor_id = 0;
};

}  // namespace performance_test

#ifndef PERFORMANCE_TEST__PERFORMANCE_NODE_BASE_IMPL_HPP_
// Template implementations
#include "performance_node_base_impl.hpp"
#endif

#endif  // PERFORMANCE_TEST__PERFORMANCE_NODE_BASE_HPP_
