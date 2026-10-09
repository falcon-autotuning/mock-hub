vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO falcon-autotuning/falcon-comms
    REF v${VERSION}
    SHA512 0ebb130403c5fb022dc9f751779a314d9c56a76a0e08e8b79525ed7f077926cfae9afb2c74732bbb77e2176d5a8f250950d3bc1314efcf67919e9c010e10b2db
)

# Inject local workspace overrides if present
get_filename_component(WORKSPACE_ROOT "${CURRENT_PORT_DIR}/../../.." ABSOLUTE)
if(EXISTS "${WORKSPACE_ROOT}/falcon-comms/include/falcon-comms")
    file(GLOB COMMS_HEADERS "${WORKSPACE_ROOT}/falcon-comms/include/falcon-comms/*.hpp" "${WORKSPACE_ROOT}/falcon-comms/include/falcon-comms/*.h")
    file(COPY ${COMMS_HEADERS} DESTINATION "${SOURCE_PATH}/include/falcon-comms")
endif()

if(EXISTS "${WORKSPACE_ROOT}/falcon-comms/src")
    file(GLOB COMMS_SRCS "${WORKSPACE_ROOT}/falcon-comms/src/*.cpp")
    file(COPY ${COMMS_SRCS} DESTINATION "${SOURCE_PATH}/src")
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
