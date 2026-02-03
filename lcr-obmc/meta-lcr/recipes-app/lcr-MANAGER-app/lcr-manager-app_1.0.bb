SUMMARY = "BOOT SEQUENCE AND MANAGER"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/MANAGER.cpp \
           file://include/MANAGER.hpp \
           file://include/global.hpp \
           file://LCR_MANAGER.service"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json libgpiod"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/MANAGER.cpp \
    -I ${WORKDIR}/include \
    ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    -lgpiodcxx \
    -o MANAGER
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 MANAGER ${D}${bindir}/MANAGER

    # Install the systemd service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/LCR_MANAGER.service ${D}${systemd_system_unitdir}/LCR_MANAGER.service
}

PROVIDES = "lcr_boot"
RPROVIDES:${PN} = "lcr_boot"

SYSTEMD_SERVICE:${PN} = "LCR_MANAGER.service"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"