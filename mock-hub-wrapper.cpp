#include <falcon-typing/FFIHelpers.hpp>
#include <falcon-comms/natsManager.hpp>
#include <falcon-comms/commands_definitions.hpp>
#include <falcon-core/communications/messages/VoltageStatesResponse.hpp>
#include <falcon-core/communications/messages/MeasurementResponse.hpp>
#include <falcon-core/physics/config/core/Config.hpp>
#include <falcon-core/instrument_interfaces/names/Ports.hpp>
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
#include <mutex>
#include <iostream>
#include <memory>
#include <string>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

namespace {

template <typename T>
std::shared_ptr<T> extract_opaque_handle(const FalconParamEntry *entries, int32_t count, const char *key, const char *expected_type) {
  for (int32_t i = 0; i < count; ++i) {
    if (std::strcmp(entries[i].key, key) != 0) continue;
    const FalconParamEntry &e = entries[i];
    if (e.tag != FALCON_TYPE_OPAQUE) {
      throw std::runtime_error(std::string("parameter '") + key + "' is not OPAQUE");
    }
    std::string tn = e.value.opaque.type_name ? e.value.opaque.type_name : "";
    if (tn == expected_type) {
      return *static_cast<std::shared_ptr<T> *>(e.value.opaque.ptr);
    }
    auto raw_ptr = *static_cast<std::shared_ptr<void> *>(e.value.opaque.ptr);
    return std::static_pointer_cast<T>(raw_ptr);
  }
  throw std::runtime_error(std::string("parameter '") + key + "' not found");
}

template <typename T>
void pack_opaque(std::shared_ptr<T> obj, FalconResultSlot *slot, const char *type_name) {
  slot->tag = FALCON_TYPE_OPAQUE;
  slot->value.opaque.type_name = type_name;
  slot->value.opaque.ptr = new std::shared_ptr<T>(std::move(obj));
  slot->value.opaque.deleter = [](void *p) {
    delete static_cast<std::shared_ptr<T> *>(p);
  };
}

int32_t extract_int_param(const FalconParamEntry *entries, int32_t count, const char *key) {
  for (int32_t i = 0; i < count; ++i) {
    if (std::strcmp(entries[i].key, key) == 0) {
      if (entries[i].tag == FALCON_TYPE_INT) {
        return static_cast<int32_t>(entries[i].value.int_val);
      }
      throw std::runtime_error(std::string("parameter '") + key + "' is not INT");
    }
  }
  throw std::runtime_error(std::string("parameter '") + key + "' not found");
}

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

struct MockHubServer {
  std::mutex mutex;
  std::string state_response_json;
  std::string config_response_json;
  std::string knobs_json;
  std::string meters_json;
  std::string measure_response_json;
  pid_t server_pid = 0;
  bool is_running = false;
  int port = 4222;

  ~MockHubServer() {
    stop();
  }

  bool start(int p) {
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
        const char *subjects[] = {"MEASUREMENTS.>"};
        stream_cfg.Subjects = subjects;
        stream_cfg.SubjectsLen = 1;
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
        }
        falcon::comms::NatsManager::instance().publish("FALCON.PORT_PAYLOAD", payload.to_json().dump());
      } catch (...) {}
    });

    // Subscribe to Measure Command
    hub.subscribe("INSTRUMENTHUB.MEASURE_COMMAND", [this](const std::string &msg) {
      try {
        auto cmd = MeasureCommand::from_json(nlohmann::json::parse(msg));
        MeasureResponse resp;
        resp.timestamp = cmd.timestamp;
        resp.stream = "MEASUREMENTS";
        resp.channel = "measurement_channel";
        falcon::comms::NatsManager::instance().publish(
            "FALCON.MEASURE_RESPONSE." + std::to_string(cmd.timestamp), resp.to_json().dump());

        std::string data;
        {
          std::lock_guard<std::mutex> lock(this->mutex);
          data = this->measure_response_json;
        }
        natsConnection *raw_conn = nullptr;
        std::string nats_url = std::getenv("NATS_URL") ? std::getenv("NATS_URL") : "nats://127.0.0.1:4222";
        if (natsConnection_ConnectTo(&raw_conn, nats_url.c_str()) == NATS_OK) {
          jsCtx *js = nullptr;
          if (natsConnection_JetStream(&js, raw_conn, NULL) == NATS_OK) {
            jsPubAck *ack = nullptr;
            jsErrCode errCode;
            js_Publish(&ack, js, "MEASUREMENTS.data", data.c_str(), data.length(), nullptr, &errCode);
            if (ack) jsPubAck_Destroy(ack);
            jsCtx_Destroy(js);
          }
          natsConnection_Destroy(raw_conn);
        }
      } catch (...) {}
    });

    is_running = true;
    return true;
  }

  void stop() {
    if (!is_running) return;
    try {
      auto &hub = falcon::comms::NatsManager::instance();
      hub.unsubscribe("INSTRUMENTHUB.STATE_REQUEST");
      hub.unsubscribe("INSTRUMENTHUB.DEVICE_CONFIG_REQUEST");
      hub.unsubscribe("INSTRUMENTHUB.PORT_REQUEST");
      hub.unsubscribe("INSTRUMENTHUB.MEASURE_COMMAND");
      hub.disconnect();
    } catch (...) {}

    if (server_pid > 0) {
      kill(server_pid, SIGTERM);
      waitpid(server_pid, NULL, 0);
      server_pid = 0;
    }
    is_running = false;
  }
};

} // namespace

extern "C" {

void STRUCTMockHubNew(const FalconParamEntry *param_entries, int32_t param_count,
                      FalconResultSlot *out_slots, int32_t *out_count) {
  (void)param_entries;
  (void)param_count;
  auto mock = std::make_shared<MockHubServer>();
  pack_opaque(mock, &out_slots[0], "MockHub");
  *out_count = 1;
}

void STRUCTMockHubStart(const FalconParamEntry *param_entries, int32_t param_count,
                        FalconResultSlot *out_slots, int32_t *out_count) {
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  int32_t port = extract_int_param(param_entries, param_count, "port");
  bool ok = self->start(port);
  out_slots[0].tag = FALCON_TYPE_BOOL;
  out_slots[0].value.bool_val = ok ? 1 : 0;
  *out_count = 1;
}

void STRUCTMockHubStop(const FalconParamEntry *param_entries, int32_t param_count,
                       FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  self->stop();
  *out_count = 0;
}

void STRUCTMockHubSetDeviceState(const FalconParamEntry *param_entries, int32_t param_count,
                                 FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto resp = extract_opaque_handle<falcon_core::communications::messages::VoltageStatesResponse>(
      param_entries, param_count, "response", "VoltageStatesResponse");
  {
    std::lock_guard<std::mutex> lock(self->mutex);
    self->state_response_json = resp->to_json_string();
  }
  *out_count = 0;
}

void STRUCTMockHubSetConfig(const FalconParamEntry *param_entries, int32_t param_count,
                            FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto cfg = extract_opaque_handle<falcon_core::physics::config::core::Config>(
      param_entries, param_count, "config", "Config");
  {
    std::lock_guard<std::mutex> lock(self->mutex);
    self->config_response_json = cfg->to_json_string();
  }
  *out_count = 0;
}

void STRUCTMockHubSetPortPayload(const FalconParamEntry *param_entries, int32_t param_count,
                                 FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto knobs = extract_opaque_handle<falcon_core::instrument_interfaces::names::Ports>(
      param_entries, param_count, "knobs", "Ports");
  auto meters = extract_opaque_handle<falcon_core::instrument_interfaces::names::Ports>(
      param_entries, param_count, "meters", "Ports");
  {
    std::lock_guard<std::mutex> lock(self->mutex);
    self->knobs_json = knobs->to_json_string();
    self->meters_json = meters->to_json_string();
  }
  *out_count = 0;
}

void STRUCTMockHubSetMeasurementResponse(const FalconParamEntry *param_entries, int32_t param_count,
                                         FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto resp = extract_opaque_handle<falcon_core::communications::messages::MeasurementResponse>(
      param_entries, param_count, "response", "MeasurementResponse");
  {
    std::lock_guard<std::mutex> lock(self->mutex);
    self->measure_response_json = resp->to_json_string();
  }
  *out_count = 0;
}

} // extern "C"
