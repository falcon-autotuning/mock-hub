#include "falcon-mock-hub/mock_hub.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>

namespace falcon {
namespace mock_hub {

namespace {

bool is_port_open(int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  bool open = (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
  close(sock);
  return open;
}

} // namespace

MockHubServer::MockHubServer() {
  falcon_core::instrument_interfaces::names::Ports empty_ports;
  knobs_json = empty_ports.to_json_string();
  meters_json = empty_ports.to_json_string();
  settings_json = empty_ports.to_json_string();
}

MockHubServer::~MockHubServer() {
  stop();
}

bool MockHubServer::start(int p) {
  if (is_running) return true;
  port = p;

  if (!is_port_open(port)) {
    pid_t pid = fork();
    if (pid == 0) {
      int devnull = open("/dev/null", O_RDWR);
      if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
      }
      execlp("nats-server", "nats-server", "-js", "-p", std::to_string(port).c_str(), (char *)NULL);
      _exit(1);
    } else if (pid > 0) {
      server_pid = pid;
      for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (is_port_open(port)) break;
      }
    }
  }

  std::string nats_url = "nats://127.0.0.1:" + std::to_string(port);
  setenv("NATS_URL", nats_url.c_str(), 1);

  auto &hub = falcon::comms::NatsManager::instance();
  hub.connect(nats_url);

  // Setup JetStream stream MEASUREMENTS
  natsConnection *raw_conn = nullptr;
  if (natsConnection_ConnectTo(&raw_conn, nats_url.c_str()) == NATS_OK) {
    jsCtx *js = nullptr;
    if (natsConnection_JetStream(&js, raw_conn, NULL) == NATS_OK) {
      jsStreamConfig stream_cfg;
      jsStreamConfig_Init(&stream_cfg);
      stream_cfg.Name = "MEASUREMENTS";
      const char *subjects[] = {"MEASUREMENTS", "MEASUREMENTS.>"};
      stream_cfg.Subjects = subjects;
      stream_cfg.SubjectsLen = 2;
      stream_cfg.Storage = js_MemoryStorage;
      jsStreamInfo *si = nullptr;
      js_AddStream(&si, js, &stream_cfg, NULL, NULL);
      if (si) jsStreamInfo_Destroy(si);

      jsCtx_Destroy(js);
    }
    natsConnection_Destroy(raw_conn);
  }

  // Subscribe to State Request
  hub.subscribe("INSTRUMENTHUB.STATE_REQUEST", [this](const std::string &msg) {
    try {
      auto req = StateRequest::from_json(nlohmann::json::parse(msg));
      StateResponse resp;
      resp.timestamp = req.timestamp;
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        resp.response = this->state_response_json;
      }
      falcon::comms::NatsManager::instance().publish("FALCON.STATE_RESPONSE", resp.to_json().dump());
    } catch (...) {}
  });

  // Subscribe to Device Config Request
  hub.subscribe("INSTRUMENTHUB.DEVICE_CONFIG_REQUEST", [this](const std::string &msg) {
    try {
      auto req = DeviceConfigRequest::from_json(nlohmann::json::parse(msg));
      DeviceConfigResponse resp;
      resp.timestamp = req.timestamp;
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        resp.response = this->config_response_json;
      }
      falcon::comms::NatsManager::instance().publish("FALCON.DEVICE_CONFIG_RESPONSE", resp.to_json().dump());
    } catch (...) {}
  });

  // Subscribe to Port Request
  hub.subscribe("INSTRUMENTHUB.PORT_REQUEST", [this](const std::string &msg) {
    try {
      auto req = PortRequest::from_json(nlohmann::json::parse(msg));
      PortPayload payload;
      payload.timestamp = req.timestamp;
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        payload.knobs = this->knobs_json;
        payload.meters = this->meters_json;
        payload.settings = this->settings_json;
      }
      falcon::comms::NatsManager::instance().publish("FALCON.PORT_PAYLOAD", payload.to_json().dump());
    } catch (...) {}
  });

  // Subscribe to Setting Command
  hub.subscribe("INSTRUMENTHUB.SETTING_COMMAND", [this](const std::string &msg) {
    try {
      auto j = nlohmann::json::parse(msg);
      long long ts = 0;
      if (j.contains("timestamp")) {
        ts = j["timestamp"].get<long long>();
      }
      std::string resp_data;
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        resp_data = this->setting_response_json;
      }
      nlohmann::json resp;
      resp["timestamp"] = ts;
      resp["response"] = resp_data;
      if (ts != 0) {
        falcon::comms::NatsManager::instance().publish(
            "FALCON.SETTING_RESPONSE." + std::to_string(ts), resp.dump());
      }
      falcon::comms::NatsManager::instance().publish(
          "FALCON.SETTING_RESPONSE", resp.dump());
    } catch (...) {}
  });

  // Subscribe to Measure Command
  hub.subscribe("INSTRUMENTHUB.MEASURE_COMMAND", [this](const std::string &msg) {
    try {
      auto cmd = MeasureCommand::from_json(nlohmann::json::parse(msg));
      std::string data;
      {
        std::lock_guard<std::mutex> lock(this->mutex);
        data = this->measure_response_json;
      }
      natsConnection *raw_conn = nullptr;
      std::string nats_url = std::getenv("NATS_URL") ? std::getenv("NATS_URL") : "nats://127.0.0.1:4222";
      natsStatus conn_st = natsConnection_ConnectTo(&raw_conn, nats_url.c_str());
      if (conn_st == NATS_OK) {
        jsCtx *js = nullptr;
        natsStatus js_st = natsConnection_JetStream(&js, raw_conn, NULL);
        if (js_st == NATS_OK) {
          jsPubAck *ack = nullptr;
          jsErrCode errCode;
          js_Publish(&ack, js, "MEASUREMENTS.data", data.c_str(), data.length(), nullptr, &errCode);
          if (ack) jsPubAck_Destroy(ack);
          ack = nullptr;
          js_Publish(&ack, js, "MEASUREMENTS", data.c_str(), data.length(), nullptr, &errCode);
          if (ack) jsPubAck_Destroy(ack);
          jsCtx_Destroy(js);
        }
        natsConnection_Destroy(raw_conn);
      }

      MeasureResponse resp;
      resp.timestamp = cmd.timestamp;
      resp.stream = "MEASUREMENTS";
      resp.channel = "measurement_channel";
      falcon::comms::NatsManager::instance().publish(
          "FALCON.MEASURE_RESPONSE." + std::to_string(cmd.timestamp), resp.to_json().dump());
      falcon::comms::NatsManager::instance().publish(
          "FALCON.MEASURE_RESPONSE", resp.to_json().dump());
    } catch (...) {}
  });

  is_running = true;
  return true;
}

void MockHubServer::stop() {
  if (!is_running) return;
  try {
    auto &hub = falcon::comms::NatsManager::instance();
    hub.unsubscribe("INSTRUMENTHUB.STATE_REQUEST");
    hub.unsubscribe("INSTRUMENTHUB.DEVICE_CONFIG_REQUEST");
    hub.unsubscribe("INSTRUMENTHUB.PORT_REQUEST");
    hub.unsubscribe("INSTRUMENTHUB.MEASURE_COMMAND");
    hub.unsubscribe("INSTRUMENTHUB.SETTING_COMMAND");
    hub.disconnect();
  } catch (...) {}

  if (server_pid > 0) {
    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, 0);
    server_pid = 0;
  }
  is_running = false;
}

void MockHubServer::set_device_state(const falcon_core::communications::messages::VoltageStatesResponse &response) {
  std::lock_guard<std::mutex> lock(mutex);
  state_response_json = response.to_json_string();
}

void MockHubServer::set_config(const falcon_core::physics::config::core::Config &config) {
  std::lock_guard<std::mutex> lock(mutex);
  config_response_json = config.to_json_string();
}

void MockHubServer::set_port_payload(const falcon_core::instrument_interfaces::names::Ports &knobs,
                                    const falcon_core::instrument_interfaces::names::Ports &meters) {
  falcon_core::instrument_interfaces::names::Ports empty_settings;
  set_port_payload(knobs, meters, empty_settings);
}

void MockHubServer::set_port_payload(const falcon_core::instrument_interfaces::names::Ports &knobs,
                                    const falcon_core::instrument_interfaces::names::Ports &meters,
                                    const falcon_core::instrument_interfaces::names::Ports &settings) {
  std::lock_guard<std::mutex> lock(mutex);
  knobs_json = knobs.to_json_string();
  meters_json = meters.to_json_string();
  settings_json = settings.to_json_string();
}

void MockHubServer::set_measurement_response(const falcon_core::communications::messages::MeasurementResponse &response) {
  std::lock_guard<std::mutex> lock(mutex);
  measure_response_json = response.to_json_string();
}

void MockHubServer::set_setting_response(const falcon_core::communications::messages::SettingResponse &response) {
  std::lock_guard<std::mutex> lock(mutex);
  setting_response_json = response.to_json_string();
}

} // namespace mock_hub
} // namespace falcon
