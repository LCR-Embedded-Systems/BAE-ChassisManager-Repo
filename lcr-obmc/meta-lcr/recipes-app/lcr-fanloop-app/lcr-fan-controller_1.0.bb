SUMMARY = "Fan Controller"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/fan_manipulation.cpp \
           file://src/pid.cpp \
           file://include/fan_manipulation.hpp \
           file://include/global.hpp \
           file://include/pid.hpp \
           file://LCR_fan_controller.service \
           file://include/pid.json"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/fan_manipulation.cpp \
    ${WORKDIR}/src/pid.cpp \
    -I ${WORKDIR}/include \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    -o lcr_fan_controller
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 lcr_fan_controller ${D}${bindir}/lcr_fan_controller

    # Install the systemd service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/LCR_fan_controller.service ${D}${systemd_system_unitdir}/LCR_fan_controller.service

    # Install the pid.json file
    install -d ${D}${prefix}/share/pid
    install -m 0644 ${WORKDIR}/include/pid.json ${D}${prefix}/share/pid/pid.json
}

# Include installed files in the main package
FILES:${PN} += "${prefix}/share/pid ${prefix}/share/pid/pid.json"

PROVIDES = "LCR_fan_controller"
RPROVIDES:${PN} = "LCR_fan_controller"

SYSTEMD_SERVICE:${PN} = "LCR_fan_controller.service"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"