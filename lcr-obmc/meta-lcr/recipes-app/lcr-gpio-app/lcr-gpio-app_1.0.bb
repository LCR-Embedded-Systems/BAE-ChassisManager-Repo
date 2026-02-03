SUMMARY = "GPIO CLI Tool"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/lcr_gpio.cpp \
           file://include/lcr_gpio.hpp \
           file://include/global.hpp"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json libgpiod"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    -I ${WORKDIR}/include \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/lcr_gpio.cpp \
    ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    -lgpiodcxx \
    -o lcrpin
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 lcrpin ${D}${bindir}/lcrpin
}

PROVIDES = "lcr_gpio"
RPROVIDES:${PN} = "lcr_gpio"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"