SUMMARY = "LCR Fan Control CLI utility (fanctl)"
DESCRIPTION = "Custom fanctl tool for LCR_fan_controller.service"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit meson pkgconfig

DEPENDS = " \
    cli11 \
    nlohmann-json \
    phosphor-logging \
    sdbusplus \
"

SRC_URI = " \
    file://fanctl.cpp \
    file://meson.build \
    file://sdbusplus.hpp \
"

S = "${WORKDIR}"

do_install:append() {
    # fanctl is already installed by meson, but we make sure the name is correct
    :
}