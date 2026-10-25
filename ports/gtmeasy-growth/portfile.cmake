# SHA512 is a placeholder filled at release.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO gtmeasy/gtm-easy-cpp-sdk
    REF v0.2.0
    SHA512 7da2de263fdc0691711cfb94a1920107d5c9528a697f3c763c092fbfedb775448a36d0988260bc71ff4233f0d4f570b0ff1f209a957c5b07cdd5b577a8079940
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DGTMEASY_BUILD_TESTS=OFF
        -DGTMEASY_BUILD_EXAMPLES=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME GtmEasyGrowth CONFIG_PATH lib/cmake/GtmEasyGrowth)
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
