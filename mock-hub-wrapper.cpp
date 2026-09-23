#include <falcon-typing/FFIHelpers.hpp>
#include "falcon-mock-hub/mock_hub.hpp"
#include <cstring>
#include <memory>
#include <string>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;
using namespace falcon::mock_hub;

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
  self->set_device_state(*resp);
  *out_count = 0;
}

void STRUCTMockHubSetConfig(const FalconParamEntry *param_entries, int32_t param_count,
                            FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto cfg = extract_opaque_handle<falcon_core::physics::config::core::Config>(
      param_entries, param_count, "config", "Config");
  self->set_config(*cfg);
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
  self->set_port_payload(*knobs, *meters);
  *out_count = 0;
}

void STRUCTMockHubSetMeasurementResponse(const FalconParamEntry *param_entries, int32_t param_count,
                                         FalconResultSlot *out_slots, int32_t *out_count) {
  (void)out_slots;
  auto self = extract_opaque_handle<MockHubServer>(param_entries, param_count, "this", "MockHub");
  auto resp = extract_opaque_handle<falcon_core::communications::messages::MeasurementResponse>(
      param_entries, param_count, "response", "MeasurementResponse");
  self->set_measurement_response(*resp);
  *out_count = 0;
}

} // extern "C"
