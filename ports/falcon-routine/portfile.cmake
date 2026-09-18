vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO falcon-autotuning/falcon-routine
    REF v${VERSION}
    SHA512 fb8325fb774fbf225446d02f79e3650aaf09ef587f3abaed43c33a151327e0daa9a1601c801c09fd49f0276e2ef8fd8d50fc5f44b624ac9f34d6eb754a448d87
)

# Inject local workspace overrides if present
get_filename_component(WORKSPACE_ROOT "${CURRENT_PORT_DIR}/../../.." ABSOLUTE)
set(FALCON_ROUTINE_HUB_OVERRIDE "${WORKSPACE_ROOT}/falcon/comms/src/hub_override/hub.cpp")
if(EXISTS "${FALCON_ROUTINE_HUB_OVERRIDE}")
    file(COPY "${FALCON_ROUTINE_HUB_OVERRIDE}" DESTINATION "${SOURCE_PATH}/src")
endif()

if(EXISTS "${WORKSPACE_ROOT}/falcon-routine/include/falcon-routine")
    file(GLOB ROUTINE_HEADERS "${WORKSPACE_ROOT}/falcon-routine/include/falcon-routine/*.hpp" "${WORKSPACE_ROOT}/falcon-routine/include/falcon-routine/*.h")
    file(COPY ${ROUTINE_HEADERS} DESTINATION "${SOURCE_PATH}/include/falcon-routine")
endif()

if(EXISTS "${WORKSPACE_ROOT}/falcon-routine/src")
    file(GLOB ROUTINE_SRCS "${WORKSPACE_ROOT}/falcon-routine/src/*.cpp")
    file(COPY ${ROUTINE_SRCS} DESTINATION "${SOURCE_PATH}/src")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(INSTALL "${SOURCE_PATH}/LICENSE"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)

vcpkg_copy_pdbs()
