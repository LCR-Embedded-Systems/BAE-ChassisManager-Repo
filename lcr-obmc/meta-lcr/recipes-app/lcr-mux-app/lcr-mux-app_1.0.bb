SUMMARY = "MUX Command Line Application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/lcr_mux.cpp \
           file://include/lcr_mux.hpp \
           file://include/global.hpp"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json libgpiod"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libgpiodcxx` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/lcr_mux.cpp \
    -I ${WORKDIR}/include \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libgpiodcxx` \
    -o mux
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 mux ${D}${bindir}/mux
}

PROVIDES = "lcr_mux"
RPROVIDES:${PN} = "lcr_mux"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"