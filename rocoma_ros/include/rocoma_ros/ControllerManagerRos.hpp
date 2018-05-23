#pragma once

// rocoma
#include "rocoma/ControllerManager.hpp"
#include "rocoma/controllers/ControllerAdapter.hpp"

// rocoma plugin
#include "rocoma_plugin/rocoma_plugin.hpp"

// rocoma msgs
#include "rocoma_msgs/GetAvailableControllers.h"
#include "rocoma_msgs/SwitchController.h"
#include "rocoma_msgs/GetActiveController.h"
#include "rocoma_msgs/ControllerManagerState.h"
#include "rocoma_msgs/ClearedEmergencyStopState.h"

// std msgs
#include "std_msgs/String.h"

// roco
#include "roco/controllers/adapters/ControllerAdapterInterface.hpp"

// message logger
#include "message_logger/message_logger.hpp"

// ros
#include <pluginlib/class_loader.h>
#include <ros/ros.h>
#include <ros/console.h>
#include <std_srvs/Trigger.h>

// stl
#include <string>
#include <vector>
#include <map>
#include <memory>


namespace rocoma_ros {

//! Struct to simplify the adding of a module (controller / shared module)
struct ManagedModuleOptions {

  ManagedModuleOptions():
      pluginName_(""),
      name_(""),
      parameterPath_(""),
      isRos_(false)
  {

  }

  /*! Constructor
   * @param pluginName          Name of the module plugin
   * @param name                Name of the module
   * @param parameterPath       Path from which module loads parameters
   * @param isRos               True if the module is ros dependent
   * @returns object of type    ManagedModuleOptions
   */
  ManagedModuleOptions(const std::string & pluginName,
                       const std::string & name,
                       const std::string & parameterPath,
                       const bool isRos):
      pluginName_(pluginName),
      name_(name),
      parameterPath_(parameterPath),
      isRos_(isRos)
  {

  }

  std::string pluginName_;
  std::string name_;
  std::string parameterPath_;
  bool isRos_;
};


//! Struct to simplify the adding of a controller pair
struct ManagedControllerOptions : public ManagedModuleOptions {

  ManagedControllerOptions():
    ManagedModuleOptions(),
    sharedModuleNames_{ }
  {

  }
  /*! Constructor
   * @param pluginName          Name of the controller plugin
   * @param name                Name of the controller
   * @param parameterPath       Path from which controller loads parameters
   * @param isRos               True if the controller is of type ControllerRosPlugin
   * @param sharedModuleNames   Plugin names of the shared modules to be added to this controller
   * @returns object of type ManagedControllerOptions
   */
  ManagedControllerOptions(const std::string & pluginName,
                           const std::string & name,
                           const std::string & parameterPath,
                           const bool isRos,
                           const std::vector<std::string> & sharedModuleNames = { }):
    ManagedModuleOptions(pluginName, name, parameterPath, isRos),
    sharedModuleNames_(sharedModuleNames)
  {

  }

  std::vector<std::string> sharedModuleNames_;
};

using ManagedControllerOptionsPair = std::pair<ManagedControllerOptions, ManagedControllerOptions>;


//! Options struct to initialize controller manager ros
struct ControllerManagerRosOptions : public rocoma::ControllerManagerOptions
{
  //! Default Constructor
  ControllerManagerRosOptions():
        rocoma::ControllerManagerOptions(),
        nodeHandle(ros::NodeHandle())
  {

  }

  //! Constructor
  ControllerManagerRosOptions(const double timeStep,
                              const bool isRealRobot,
                              const ros::NodeHandle& nodeHandle,
                              const rocoma::LoggerOptions loggerOptions = rocoma::LoggerOptions()):
        rocoma::ControllerManagerOptions(timeStep, isRealRobot, loggerOptions),
        nodeHandle(nodeHandle)
  {

  }

  //! Ros node handle
  ros::NodeHandle nodeHandle;
};

//! Extension of the Controller Manager to ROS
/*! Functionalities of the rocoma controller manager are wrapped as ros services.
 *  Controllers can be loaded using the ros pluginlib.
 *
 */
template<typename State_, typename Command_>
class ControllerManagerRos : public rocoma::ControllerManager {

 public:

  /*! Constructor
   * @param scopedStateName         Name of the robot state class, including namespaces  (e.g. "myStatePkg::State")
   * @param scopedCommandName       Name of the robot command class, including namespaces  (e.g. "myCommandPkg::Command")
   * @returns object of type ControllerManagerRos
   */
  ControllerManagerRos( const std::string & scopedStateName,
                        const std::string & scopedCommandName);

  /*! Constructor
   * @param scopedStateName         Name of the robot state class, including namespaces  (e.g. "myStatePkg::State")
   * @param scopedCommandName       Name of the robot command class, including namespaces  (e.g. "myCommandPkg::Command")
   * @param timestep                Controller timestep
   * @param isRealRobot             Simulation flag
   * @param nodeHandle              Ros nodehandle
   * @param loggerOptions           Logger options
   * @returns object of type ControllerManagerRos
   */
  ControllerManagerRos(const std::string & scopedStateName,
                       const std::string & scopedCommandName,
                       const double timeStep,
                       const bool isRealRobot,
                       const ros::NodeHandle& nodeHandle,
                       const rocoma::LoggerOptions loggerOptions = rocoma::LoggerOptions());

  /*! Constructor
   * @param options    Controller manager ros options
   * @param scopedStateName         Name of the robot state class, including namespaces  (e.g. "myStatePkg::State")
   * @param scopedCommandName       Name of the robot command class, including namespaces  (e.g. "myCommandPkg::Command")
   * @returns object of type ControllerManagerRos
   */
  ControllerManagerRos( const std::string & scopedStateName,
                        const std::string & scopedCommandName,
                        const ControllerManagerRosOptions & options);

  //! Default destructor
  ~ControllerManagerRos() override = default;

  //! Init Manager and ROS publishers and services.
  virtual void init(const ControllerManagerRosOptions & options);

  //! Shuts down ROS publishers and services.
  void shutdown();

  /*! Add a single controller pair to the manager
   * @param options         options containing names and ros flags
   * @param state           robot state pointer
   * @param command         robot command pointer
   * @param mutexState      mutex protecting robot state pointer
   * @param mutexCommand    mutex protecting robot command pointer
   * @returns true iff added controller pair successfully
   */
  bool setupControllerPair(const ManagedControllerOptionsPair & options,
                           std::shared_ptr<State_> state,
                           std::shared_ptr<Command_> command,
                           std::shared_ptr<boost::shared_mutex> mutexState,
                           std::shared_ptr<boost::shared_mutex> mutexCommand);

  /*! Add the failproof controller to the controller manager
   * @param controllerPluginName  name of the failproof controller
   * @param state                 robot state pointer
   * @param command               robot command pointer
   * @param mutexState            mutex protecting robot state pointer
   * @param mutexCommand          mutex protecting robot command pointer
   * @returns true iff the failproof controller was added successfully
   */
  bool setupFailproofController(const std::string & controllerPluginName,
                                std::shared_ptr<State_> state,
                                std::shared_ptr<Command_> command,
                                std::shared_ptr<boost::shared_mutex> mutexState,
                                std::shared_ptr<boost::shared_mutex> mutexCommand);

  /*! Add a vector of controller pairs and a failproof controller to the manager
   * @param failproofControllerName name of the failproof controller
   * @param controllerOptions       options containing names and ros flags of all controller pairs
   * @param state                   robot state pointer
   * @param command                 robot command pointer
   * @param mutexState              mutex protecting robot state pointer
   * @param mutexCommand            mutex protecting robot command pointer
   * @returns true iff all controllers were added successfully
   */
  bool setupControllers(const std::string & failproofControllerName,
                        const std::vector<ManagedControllerOptionsPair> & controllerOptions,
                        std::shared_ptr<State_> state,
                        std::shared_ptr<Command_> command,
                        std::shared_ptr<boost::shared_mutex> mutexState,
                        std::shared_ptr<boost::shared_mutex> mutexCommand);

  /*! Add a vector of shared module pairs to the manager
   * @param sharedModuleOptions vector of shared module options
   * @returns true iff all shared modules were added successfully
   */
  bool setupSharedModules(const std::vector<ManagedModuleOptions> & sharedModuleOptions);

  /*! Loads the names of the controllers from the ros parameter server.
   * Pattern in yaml:
   * controller_manager:
   *  failproof_controller: "MyFailProofController"
   *    controller_pairs:
   *      - controller_pair:
   *          controller:
   *            plugin_name:              "MyControllerPlugin"
   *            name:                     "MyController"
   *            is_ros:                   true
   *            parameter_package:        "my_controller_package"
   *            parameter_path:           "my_param_folder/my_param_file.xml"
   *          emergency_controller:
   *            plugin_name:              "MyEmergencyControllerPlugin"
   *            name:                     "MyEmergencyController "
   *            is_ros:                   false
   *            package:                  "my_emergency_controller_package"
   *            parameter_path:           "my_emergency_param_folder/my_emergency_param_file.xml"
   *      - controller_pair:
   *          controller:
   *           .... and so on
   *
   *
   * @param state                   robot state pointer
   * @param command                 robot command pointer
   * @param mutexState              mutex protecting robot state pointer
   * @param mutexCommand            mutex protecting robot command pointer
   * @returns true iff all controllers were added successfully
   */
  bool setupControllersFromParameterServer(std::shared_ptr<State_> state,
                                           std::shared_ptr<Command_> command,
                                           std::shared_ptr<boost::shared_mutex> mutexState,
                                           std::shared_ptr<boost::shared_mutex> mutexCommand);

  /*! Emergency stop service callback, triggers an emergency stop of the current controller
   * @param req   empty request
   * @param res   empty response
   * @return true iff successful
   */
  bool emergencyStopService(std_srvs::Trigger::Request  &req,
                            std_srvs::Trigger::Response &res);

  /*! Clear emergency stop service callback
   * @param req   empty request
   * @param res   empty response
   * @return true iff successful
   */
  bool clearEmergencyStopService(std_srvs::Trigger::Request  &req,
                                 std_srvs::Trigger::Response &res);



  /*! Switch controller service callback, switches to a new controller
   * @param req   contains name of the new controller
   * @param res   contains the result of the switching
   * @return true iff successful
   */
  bool switchControllerService(rocoma_msgs::SwitchController::Request  &req,
                               rocoma_msgs::SwitchController::Response &res);

  /*! Get available controllers service callback, returns a list of all available controllers
   * @param req   empty request
   * @param res   contains vector of strings with the available controller names
   * @return true iff successful
   */
  bool getAvailableControllersService(rocoma_msgs::GetAvailableControllers::Request &req,
                                      rocoma_msgs::GetAvailableControllers::Response &res);

  /*! Get active controller service callback, returns the name of the currently active controller
   * @param req   empty request
   * @param res   contains name of the currently active controller
   * @return true iff successful
   */
  bool getActiveControllerService(rocoma_msgs::GetActiveController::Request &req,
                                  rocoma_msgs::GetActiveController::Response &res);

  /*! Inform other nodes (via message) when an emergency stop was triggered
   * @param type   type of the emergency stop
   */
  void notifyEmergencyStop(rocoma::ControllerManager::EmergencyStopType type) override;

  /**
   * @brief notify others of a controller state change (default: do nothing)
   * @param state     New state of the emergency stop
   */
  void notifyControllerManagerStateChanged(State state) override;

  /*! Inform other nodes (via message) that the controller was switched
   * @param newControllerName   name of the "new" controller
   */
  void notifyControllerChanged(const std::string & newControllerName) override;

  /**
   * @brief Cleanup all controllers and ROS services and publishers.
   * @return true, if successful emergency stop and all controllers are cleaned up
   */
  virtual bool cleanup();

  /**
   * @brief Clears the emergency stop, controller switches are now allowed
   */
  void clearEmergencyStop() override;

 private:
  /*! Publish the active controller.
   * @param activeController   active controller name
   */
  void publishActiveController(std::string activeController);

  /*! Publish the current controller manager state.
   */
  void publishControllerManagerState();

  /*! Publish on the cleared emergency stop topic
   * @param cleared   publish cleared value on topic
   */
  void publishClearedEmergencyState(bool cleared);

 private:
  //! Init flag
  std::atomic_bool isInitializedRos_;
  //! Ros node handle
  ros::NodeHandle nodeHandle_;
  //! Switch controller service
  ros::ServiceServer switchControllerService_;
  //! Emergency stop service
  ros::ServiceServer emergencyStopService_;
  //! Clear emergency stop service
  ros::ServiceServer clearEmergencyStopService_;
  //! Get available controllers service
  ros::ServiceServer getAvailableControllersService_;
  //! Get active controller service
  ros::ServiceServer getActiveControllerService_;

  //! Active controller publisher
  ros::Publisher activeControllerPublisher_;
  //! Active controller message
  std_msgs::String activeControllerMsg_;

  //! Controller manager state publisher
  ros::Publisher controllerManagerStatePublisher_;
  //! Controller manager state message
  rocoma_msgs::ControllerManagerState controllerManagerStateMsg_;

  //! Cleared emergency state publisher
  ros::Publisher clearedEmergencyStopStatePublisher_;
  //! Cleared emergency state message
  rocoma_msgs::ClearedEmergencyStopState clearedEmergencyStopStateMsg_;

  //! Failproof controller class loader
  pluginlib::ClassLoader< rocoma_plugin::FailproofControllerPluginInterface<State_, Command_> > failproofControllerLoader_;
  //! Emergency controller class loader
  pluginlib::ClassLoader< rocoma_plugin::EmergencyControllerPluginInterface<State_, Command_> > emergencyControllerLoader_;
  //! Emergency controller ROS class loader
  pluginlib::ClassLoader< rocoma_plugin::EmergencyControllerRosPluginInterface<State_, Command_> > emergencyControllerRosLoader_;
  //! Controller class loader
  pluginlib::ClassLoader< rocoma_plugin::ControllerPluginInterface<State_, Command_> > controllerLoader_;
  //! Controller ROS class loader
  pluginlib::ClassLoader< rocoma_plugin::ControllerRosPluginInterface<State_, Command_> > controllerRosLoader_;
  //! Shared module class loader
  pluginlib::ClassLoader< rocoma_plugin::SharedModulePluginInterface > sharedModuleLoader_;
  //! Shared module ROS class loader
  pluginlib::ClassLoader< rocoma_plugin::SharedModuleRosPluginInterface > sharedModuleRosLoader_;
};

}

#include <rocoma_ros/ControllerManagerRos.tpp>
