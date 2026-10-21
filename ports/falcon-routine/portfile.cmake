vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO falcon-autotuning/falcon-routine
    REF v${VERSION}
    SHA512 29df8b94a845dd26fe7c91046b421895c1a83aef29d2b5d3400db3d2a188fd9f6916f53f428fa223835120f29e8f6b8fc48f3d0824a765bfe572d4eb31b55a0d
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
