#include <rocoma_ros/ControllerManagerRos.hpp>
#include <ros/package.h>

namespace rocoma_ros {

template<typename State_, typename Command_>
ControllerManagerRos<State_,Command_>::ControllerManagerRos(const std::string & scopedStateName,
                                                            const std::string & scopedCommandName,
                                                            const double timeStep,
                                                            ros::NodeHandle nodeHandle):
                                                            rocoma::ControllerManager(timeStep),
                                                            nodeHandle_(nodeHandle),
                                                            failproofControllerLoader_("rocoma_plugin", "rocoma_plugin::FailproofControllerPluginInterface<" + scopedStateName + ", " + scopedCommandName + ">"),
                                                            emergencyControllerLoader_("rocoma_plugin", "rocoma_plugin::EmergencyControllerPluginInterface<" + scopedStateName + ", " + scopedCommandName + ">"),
                                                            emergencyControllerRosLoader_("rocoma_plugin", "rocoma_plugin::EmergencyControllerRosPluginInterface<" + scopedStateName + ", " + scopedCommandName + ">"),
                                                            controllerLoader_("rocoma_plugin", "rocoma_plugin::ControllerPluginInterface<" + scopedStateName + ", " + scopedCommandName + ">"),
                                                            controllerRosLoader_("rocoma_plugin", "rocoma_plugin::ControllerRosPluginInterface<" + scopedStateName + ", " + scopedCommandName + ">")

{
  // initialize services
  switchControllerService_ = nodeHandle.advertiseService("controller_manager/switch_controller", &ControllerManagerRos::switchController, this);
  getAvailableControllersService_ = nodeHandle.advertiseService("controller_manager/get_available_controllers", &ControllerManagerRos::getAvailableControllers, this);
  getActiveControllerService_ = nodeHandle.advertiseService("controller_manager/get_active_controller", &ControllerManagerRos::getActiveController, this);
  emergencyStopService_ = nodeHandle.advertiseService("controller_manager/emergency_stop", &ControllerManagerRos::emergencyStop, this);

  // initialize publishers
  emergencyStopStatePublisher_.shutdown();
  emergencyStopStatePublisher_ = nodeHandle.advertise<any_msgs::State>("notify_emergency_stop", 1, true);
  publishEmergencyState(true);
}

template<typename State_, typename Command_>
ControllerManagerRos<State_,Command_>::~ControllerManagerRos() {

}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::setupControllerPair(const ControllerOptionsPair & options,
                                                                std::shared_ptr<State_> state,
                                                                std::shared_ptr<Command_> command,
                                                                std::shared_ptr<boost::shared_mutex> mutexState,
                                                                std::shared_ptr<boost::shared_mutex> mutexCommand) {

  //--- Add controller
  rocoma_plugin::ControllerPluginInterface<State_, Command_> * controller;

  try
  {
    if(options.first.isRos_) {
      // Instantiate controller
      rocoma_plugin::ControllerRosPluginInterface<State_, Command_> * rosController = controllerRosLoader_.createUnmanagedInstance(options.first.name_);
      // Set node handle
      rosController->setNodeHandle(nodeHandle_);
      controller = rosController;
    }
    else {
      controller = controllerLoader_.createUnmanagedInstance(options.first.name_);
    }

    // Set state and command
    controller->setStateAndCommand(state, mutexState, command, mutexCommand);
    controller->setParameterPath(options.first.parameterPath_);

  }
  catch(pluginlib::PluginlibException& ex)
  {
    //handle the class failing to load
    MELO_ERROR("The plugin failed to load for some reason. Error: %s", ex.what());
    MELO_WARN_STREAM("Could not setup controller: " << options.first.name_ << "!");

    return false;
  }


  //--- Add emergency controller
  rocoma_plugin::EmergencyControllerPluginInterface<State_, Command_> * emgcyController = nullptr;

  if(!options.second.name_.empty())
  {
    try
    {
      if(options.second.isRos_) {
        // Instantiate controller
        rocoma_plugin::EmergencyControllerRosPluginInterface<State_, Command_> * rosEmergencyController =
            emergencyControllerRosLoader_.createUnmanagedInstance(options.second.name_);
        // Set node handle
        rosEmergencyController->setNodeHandle(nodeHandle_);
        emgcyController = rosEmergencyController;
      }
      else {
        emgcyController = emergencyControllerLoader_.createUnmanagedInstance(options.second.name_);
      }

      // Set state and command
      emgcyController->setStateAndCommand(state, mutexState, command, mutexCommand);
      emgcyController->setParameterPath(options.second.parameterPath_);
    }
    catch(pluginlib::PluginlibException& ex)
    {
      //handle the class failing to load
      MELO_WARN("The plugin failed to load for some reason. Error: %s", ex.what());
      MELO_WARN_STREAM("Could not setup emergency controller: " << options.second.name_ << "! Using failproof controller instead");
      emgcyController = nullptr;
    }
  } // endif

  // Set name to failproof if nullptr
  std::string emergencyControllerName = (emgcyController == nullptr)? "FailproofController" : options.second.name_;

  // Add controller to the manager
  if(!this->addControllerPair(std::unique_ptr< roco::ControllerAdapterInterface >(controller),
                              std::unique_ptr< roco::EmergencyControllerAdapterInterface >(emgcyController)))
  {
    MELO_WARN_STREAM("Could not add controller pair ( " << options.first.name_ << " / "
                     << emergencyControllerName << " ) to controller manager!");
    return false;
  }

  // Inform user
  MELO_INFO_STREAM("Successfully added controller pair ( " << options.first.name_ << " / "
                   << emergencyControllerName << " ) to controller manager!");

}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::setupFailproofController(const std::string & controllerPluginName,
                                                                     std::shared_ptr<State_> state,
                                                                     std::shared_ptr<Command_> command,
                                                                     std::shared_ptr<boost::shared_mutex> mutexState,
                                                                     std::shared_ptr<boost::shared_mutex> mutexCommand) {
  try
  {
    // Instantiate controller
    rocoma_plugin::FailproofControllerPluginInterface<State_, Command_> * controller =
        failproofControllerLoader_.createUnmanagedInstance(controllerPluginName);

    // Set state and command
    controller->setStateAndCommand(state, mutexState, command, mutexCommand);

    // Add controller to the manager
    if(!this->setFailproofController(std::unique_ptr< rocoma_plugin::FailproofControllerPluginInterface<State_,Command_> >(controller))) {
      MELO_WARN_STREAM("Could not add failproof controller: " << controllerPluginName << " to controller manager!");
      return false;
    }

    // Inform user
    MELO_INFO_STREAM("Succesfully setup failproof controller: " << controllerPluginName << "!");

  }
  catch(pluginlib::PluginlibException& ex)
  {
    //handle the class failing to load
    MELO_ERROR("The plugin failed to load for some reason. Error: %s", ex.what());
    MELO_WARN_STREAM("Could not setup failproof controller: " << controllerPluginName << "!");
    return false;
  }

  return true;

}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::setupControllers(const std::string & failproofControllerName,
                                                             const std::vector< ControllerOptionsPair > & controllerNameMap,
                                                             std::shared_ptr<State_> state,
                                                             std::shared_ptr<Command_> command,
                                                             std::shared_ptr<boost::shared_mutex> mutexState,
                                                             std::shared_ptr<boost::shared_mutex> mutexCommand) {
  // add failproof controller to manager
  bool success = setupFailproofController(failproofControllerName, state, command, mutexState, mutexCommand);

  // if failproof controller can not be added abort immediately
  if( !success )
  {
    MELO_FATAL("Failproof controller could not be added! ABORT!");
    exit(-1);
  }

  // add emergency controllers to manager
  for(auto& controllerPair : controllerNameMap)
  {
    success = setupControllerPair(controllerPair, state, command, mutexState, mutexCommand) && success;
  }

  return success;

}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::setupControllersFromParameterServer(std::shared_ptr<State_> state,
                                                                                std::shared_ptr<Command_> command,
                                                                                std::shared_ptr<boost::shared_mutex> mutexState,
                                                                                std::shared_ptr<boost::shared_mutex> mutexCommand) {
  // Parse failproof controller name
  std::string failproofControllerName;
  if(!nodeHandle_.getParam("controller_manager/failproof_controller", failproofControllerName)) {
    MELO_ERROR("Could not load parameter 'controller_manager/failproof_controller' from parameter server. Abort.");
    exit(-1);
    return false;
  }

  // Parse controller list
  std::vector<ControllerOptionsPair> controller_option_pairs;
  ControllerOptionsPair controller_option_pair;
  XmlRpc::XmlRpcValue controller_pair_list;
  XmlRpc::XmlRpcValue controller;

  if(!nodeHandle_.getParam("controller_manager/controller_pairs", controller_pair_list)) {
    MELO_WARN("Could not load parameter 'controller_manager/controller_pairs'. Add only failproof controller.");
  }
  else {
    if(controller_pair_list.getType() == XmlRpc::XmlRpcValue::TypeArray)
    {
      for (unsigned int i = 0; i < controller_pair_list.size(); ++i)
      {
        // Check that controller_pair exists
        if(controller_pair_list[i].getType() != XmlRpc::XmlRpcValue::TypeStruct ||
           !controller_pair_list[i].hasMember("controller_pair") ||
           controller_pair_list[i]["controller_pair"].getType() != XmlRpc::XmlRpcValue::TypeStruct )
        {
          MELO_WARN("Controllerpair nr %d can not be obtained. Skip controller pair.", i);
          continue;
        }

        // Check if controller exists
        if(controller_pair_list[i]["controller_pair"].hasMember("controller") &&
            controller_pair_list[i]["controller_pair"]["controller"].getType() == XmlRpc::XmlRpcValue::TypeStruct)
        {
          controller = controller_pair_list[i]["controller_pair"]["controller"];
        }
        else
        {
          MELO_WARN("Controllerpair nr %d has no or wrong-typed member controller. Skip controller pair.", i);
          continue;
        }

        // Check for data members
        if(controller.hasMember("name") &&
            controller["name"].getType() == XmlRpc::XmlRpcValue::TypeString &&
            controller.hasMember("is_ros") &&
            controller["is_ros"].getType() == XmlRpc::XmlRpcValue::TypeBoolean &&
            controller.hasMember("parameter_package") &&
            controller["parameter_package"].getType() == XmlRpc::XmlRpcValue::TypeString &&
            controller.hasMember("parameter_path") &&
            controller["parameter_path"].getType() == XmlRpc::XmlRpcValue::TypeString)
        {
          controller_option_pair.first.name_ = static_cast<std::string>(controller["name"]);
          controller_option_pair.first.isRos_ = static_cast<bool>(controller["is_ros"]);
          controller_option_pair.first.parameterPath_ = ros::package::getPath( static_cast<std::string>(controller["parameter_package"]) ) + "/" +
              static_cast<std::string>(controller["parameter_path"]);
          MELO_INFO("Got controller %s successfully from the parameter server. (is_ros: %s, complete parameter_path: %s!",
                    controller_option_pair.first.name_.c_str(), controller_option_pair.first.isRos_?"true":"false", controller_option_pair.first.parameterPath_.c_str());
        }
        else
        {
          MELO_WARN("Subentry 'controller' of controllerpair nr %d has missing or wrong-type entries. Skip controller.", i);
          continue;
        }

        // Parse emergency stop controller
        if(controller_pair_list[i]["controller_pair"].hasMember("emergency_controller") &&
            controller_pair_list[i]["controller_pair"]["emergency_controller"].getType() == XmlRpc::XmlRpcValue::TypeStruct)
        {
          controller = controller_pair_list[i]["controller_pair"]["emergency_controller"];
        }
        else
        {
          MELO_WARN("Controllerpair nr %d has no member emergency_controller. Add failproof controller instead.", i);
          controller_option_pair.second = ControllerOptions();
          controller_option_pairs.push_back(controller_option_pair);
          continue;
        }

        if( controller.hasMember("name") &&
            controller["name"].getType() == XmlRpc::XmlRpcValue::TypeString &&
            controller.hasMember("is_ros") &&
            controller["is_ros"].getType() == XmlRpc::XmlRpcValue::TypeBoolean &&
            controller.hasMember("parameter_package") &&
            controller["parameter_package"].getType() == XmlRpc::XmlRpcValue::TypeString &&
            controller.hasMember("parameter_path") &&
            controller["parameter_path"].getType() == XmlRpc::XmlRpcValue::TypeString)
        {
          controller_option_pair.second.name_ = static_cast<std::string>(controller["name"]);
          controller_option_pair.second.isRos_ = static_cast<bool>(controller["is_ros"]);
          controller_option_pair.second.parameterPath_ = ros::package::getPath( static_cast<std::string>(controller["parameter_package"]) ) + "/" +
              static_cast<std::string>(controller["parameter_path"]);
          MELO_INFO("Got controller %s successfully from the parameter server. (is_ros: %s, complete parameter_path: %s!",
                    controller_option_pair.first.name_.c_str(), controller_option_pair.first.isRos_?"true":"false", controller_option_pair.first.parameterPath_.c_str());
        }
        else
        {
          MELO_WARN("Subentry 'emergency_controller' of controllerpair nr %d has missing or wrong-type entries. Add failproof controller instead.", i);
          controller_option_pair.second = ControllerOptions();
        }

        controller_option_pairs.push_back(controller_option_pair);
      }
    }
    else {
      MELO_WARN("Parameter 'controller_manager/controller_pairs' is not of array type. Add only failproof controller.");
      controller_option_pairs.clear();
    }
  }


  return setupControllers(failproofControllerName,
                          controller_option_pairs,
                          state,
                          command,
                          mutexState,
                          mutexCommand);

}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::emergencyStop(rocoma_msgs::EmergencyStop::Request& req,
                                                          rocoma_msgs::EmergencyStop::Response& res) {
  return rocoma::ControllerManager::emergencyStop();
}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::switchController(rocoma_msgs::SwitchController::Request& req,
                                                             rocoma_msgs::SwitchController::Response& res) {

  switch(rocoma::ControllerManager::switchController(req.name)) {
    case rocoma::ControllerManager::SwitchResponse::ERROR:
      res.status = res.STATUS_ERROR;
      break;
    case rocoma::ControllerManager::SwitchResponse::NOTFOUND:
      res.status = res.STATUS_NOTFOUND;
      break;
    case rocoma::ControllerManager::SwitchResponse::RUNNING:
      res.status = res.STATUS_RUNNING;
      break;
    case rocoma::ControllerManager::SwitchResponse::SWITCHING:
      res.status = res.STATUS_SWITCHED;
      break;
  }

  return true;
}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::getAvailableControllers( rocoma_msgs::GetAvailableControllers::Request& req,
                                                                     rocoma_msgs::GetAvailableControllers::Response& res) {
  res.available_controllers = this->getAvailableControllerNames();
  return true;
}

template<typename State_, typename Command_>
bool ControllerManagerRos<State_,Command_>::getActiveController(rocoma_msgs::GetActiveController::Request& req,
                                                                rocoma_msgs::GetActiveController::Response& res) {
  res.active_controller = this->getActiveControllerName();
  return true;
}

template<typename State_, typename Command_>
void ControllerManagerRos<State_,Command_>::notifyEmergencyStop(rocoma::ControllerManager::EmergencyStopType type) {
  // TODO react differently depending on the emergency stop type
  publishEmergencyState(false);
  publishEmergencyState(true);
}

template<typename State_, typename Command_>
void ControllerManagerRos<State_,Command_>::publishEmergencyState(bool isOk) {
  // Fill msg
  emergencyStopStateMsg_.stamp = ros::Time::now();
  emergencyStopStateMsg_.is_ok = isOk;

  // Publish message
  any_msgs::StatePtr stateMsg( new any_msgs::State(emergencyStopStateMsg_) );
  emergencyStopStatePublisher_.publish( any_msgs::StateConstPtr(stateMsg) );
}

}
