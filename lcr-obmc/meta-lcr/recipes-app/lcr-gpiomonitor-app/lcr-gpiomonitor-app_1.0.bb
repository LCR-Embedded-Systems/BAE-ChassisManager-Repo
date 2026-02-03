SUMMARY = "GPIO DBUS Monitor Driver"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/lcr_gpio_mon.cpp \
           file://include/lcr_gpio_mon.hpp \
           file://LCR_GPIO_Mon.service \
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
    ${WORKDIR}/src/lcr_gpio_mon.cpp \
    ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    -lgpiodcxx \
    -o lcrgpiomonitor
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 lcrgpiomonitor ${D}${bindir}/lcrgpiomonitor
    
    # Install the systemd service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/LCR_GPIO_Mon.service ${D}${systemd_system_unitdir}/LCR_GPIO_Mon.service
}

PROVIDES = "lcr_gpiomonitor"
RPROVIDES:${PN} = "lcr_gpiomonitor"

SYSTEMD_SERVICE:${PN} = "LCR_GPIO_Mon.service"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"