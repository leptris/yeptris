# yeptris vcpkg port (TODO.impl/20) — the leptris convention.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO leptris/yeptris
    REF "v${VERSION}"
    # SHA512 of the GitHub tag tarball — recompute per release:
    # curl -sL .../archive/refs/tags/v${VERSION}.tar.gz | shasum -a 512
    SHA512 f8bff86342bd6961e6fd55968059100b9cdb86d8c7255f760a423fdd307fdd83998732afa5315f0e5778a1037fb442d0a8195996980f91aedf2dc9bea1d4986d
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DYEPTRIS_BUILD_CLI=OFF
        -DYEPTRIS_BUILD_BENCHMARKS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/yeptris)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
