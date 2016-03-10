/*
 * Copyright (c) 2014, Christian Gehring
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Autonomous Systems Lab, ETH Zurich nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL  Christian Gehring BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/
/*!
 * @file    ControllerManager.hpp
 * @author  Christian Gehring, Dario Bellicoso
 * @date    Oct, 2014
 */

#ifndef LOCOMOTION_CONTROLLER_CONTROLLERMANAGER_HPP_
#define LOCOMOTION_CONTROLLER_CONTROLLERMANAGER_HPP_

#include <ros/ros.h>
#include <locomotion_controller_msgs/SwitchController.h>
#include <locomotion_controller_msgs/EmergencyStop.h>
#include <locomotion_controller_msgs/GetAvailableControllers.h>
#include <locomotion_controller_msgs/GetActiveController.h>

#include <locomotion_controller/Model.hpp>

#include "roco_freeze/RocoFreeze.hpp"
#include <roco/controllers/LocomotionControllerInterface.hpp>
#include <quadruped_model/common/State.hpp>
#include <quadruped_model/common/Command.hpp>

#include <any_msgs/State.h>

#include <boost/ptr_container/ptr_vector.hpp>

#include <mutex>

#include <roscpp_nodewrap/worker/Worker.h>

namespace locomotion_controller {

class ControllerManager;
class LocomotionController;

void add_locomotion_controllers(
    locomotion_controller::ControllerManager* manager,
    quadruped_model::State& state,
    boost::shared_mutex& mutexState,
    quadruped_model::Command& command,
    boost::shared_mutex& mutexCommand,
    ros::NodeHandle& nodeHandle);

class ControllerManager
{
 public:
  typedef roco::controllers::LocomotionControllerInterface Controller;
  typedef roco::controllers::LocomotionControllerInterface* ControllerPtr;
 public:
  explicit ControllerManager(locomotion_controller::LocomotionController* locomotionController);
  virtual ~ControllerManager();

  void updateController();
  void setupControllers(double dt,
                        quadruped_model::State& state,
                        quadruped_model::Command& command,
                        boost::shared_mutex& mutexState,
                        boost::shared_mutex& mutexCommand,
                        ros::NodeHandle& nodeHandle);
  void addController(ControllerPtr controller);
  bool switchController(locomotion_controller_msgs::SwitchController::Request  &req,
                        locomotion_controller_msgs::SwitchController::Response &res);

  bool getAvailableControllers(locomotion_controller_msgs::GetAvailableControllers::Request &req,
                               locomotion_controller_msgs::GetAvailableControllers::Response &res);

  bool getActiveController(locomotion_controller_msgs::GetActiveController::Request &req,
                           locomotion_controller_msgs::GetActiveController::Response &res);

  bool emergencyStop();
  bool switchControllerAfterEmergencyStop();
  bool isRealRobot() const;
  void setIsRealRobot(bool isRealRobot);

  locomotion_controller::LocomotionController* getLocomotionController();
  void notifyEmergencyState();
  void cleanup();
 protected:
  void switchToEmergencyTask();

  double timeStep_;
  bool isInitializingTask_;
  boost::ptr_vector<Controller> controllers_;
  ControllerPtr activeController_;
  bool isRealRobot_;

  locomotion_controller::LocomotionController* locomotionController_;
  std::recursive_mutex activeControllerMutex_;

  //--- Notify an emergency stop
  void publishEmergencyState(bool isOk);
  ros::Publisher emergencyStopStatePublisher_;
  any_msgs::State emergencyStopStateMsg_;
  //---

};

} /* namespace locomotion_controller */

#endif /* LOCOMOTION_CONTROLLER_CONTROLLERMANAGER_HPP_ */
