# mock-hub

Mock Instrument Hub service and FFI bindings for Falcon DSL tests.

## Overview

`mock-hub` provides a lightweight mock implementation of the Falcon Instrument Hub. It embeds an instance of `nats-server` and implements the NATS request/response protocol subjects used by `falcon-routine` and `std-lib/hub`:

- `INSTRUMENTHUB.STATE_REQUEST` -> `FALCON.STATE_RESPONSE`
- `INSTRUMENTHUB.DEVICE_CONFIG_REQUEST` -> `FALCON.DEVICE_CONFIG_RESPONSE`
- `INSTRUMENTHUB.PORT_REQUEST` -> `FALCON.PORT_PAYLOAD`
- `INSTRUMENTHUB.MEASURE_COMMAND` -> `FALCON.MEASURE_RESPONSE.<timestamp>` & JetStream `MEASUREMENTS.data`

This allows autotuner routines, standard library functions, and full DSL pipelines to be verified in unit tests and container integration tests without requiring physical hardware or running real instrument controllers.

## DSL Interface (`mockHub.fal`)

```fal
struct MockHub {
    routine New () -> (MockHub hub)
    routine Start (int port) -> (bool success)
    routine Stop () -> ()
    routine SetDeviceState (voltageStatesResponse::VoltageStatesResponse response) -> ()
    routine SetConfig (config::Config config) -> ()
    routine SetPortPayload (ports::Ports knobs, ports::Ports meters) -> ()
    routine SetMeasurementResponse (measurementResponse::MeasurementResponse response) -> ()
}
```

## Building & Testing

```bash
# Build C++ library and FFI wrapper
make build

# Run C++ unit tests
make test-cpp

# Run Falcon DSL tests
make test-fal

# Run all tests
make test

# Package release tarball
make archive
```

## License

MPL-2.0
