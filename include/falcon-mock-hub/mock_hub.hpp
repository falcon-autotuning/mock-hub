#pragma once
#include "falcon-mock-hub/export.h"
#include <falcon-comms/natsManager.hpp>
#include <falcon-comms/commands_definitions.hpp>
#include <falcon-core/communications/messages/VoltageStatesResponse.hpp>
#include <falcon-core/communications/messages/MeasurementResponse.hpp>
#include <falcon-core/physics/config/core/Config.hpp>
#include <falcon-core/instrument_interfaces/names/Ports.hpp>
#include <mutex>
#include <string>
#include <memory>
#include <sys/types.h>

namespace falcon {
namespace mock_hub {

class FALCON_MOCK_HUB_API MockHubServer {
public:
  MockHubServer();
  ~MockHubServer();

  bool start(int port = 4222);
  void stop();

  void set_device_state(const falcon_core::communications::messages::VoltageStatesResponse &response);
  void set_config(const falcon_core::physics::config::core::Config &config);
  void set_port_payload(const falcon_core::instrument_interfaces::names::Ports &knobs,
                        const falcon_core::instrument_interfaces::names::Ports &meters);
  void set_measurement_response(const falcon_core::communications::messages::MeasurementResponse &response);

  bool is_server_running() const { return is_running; }
  int get_port() const { return port; }

private:
  std::mutex mutex;
  std::string state_response_json;
  std::string config_response_json;
  std::string knobs_json;
  std::string meters_json;
  std::string measure_response_json;
  pid_t server_pid = 0;
  bool is_running = false;
  int port = 4222;
};

} // namespace mock_hub
} // namespace falcon
