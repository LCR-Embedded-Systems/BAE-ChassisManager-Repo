SUMMARY = "BOOT SEQUENCE AND MANAGER"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/utils.cpp \
           file://src/payloadtest.cpp \
           file://include/payloadtest.hpp \
           file://include/global.hpp \
           file://LCR_Payload_test.service"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json libgpiod mtd-utils boost"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/utils.cpp \
    ${WORKDIR}/src/payloadtest.cpp \
    -I ${WORKDIR}/include \
    ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    -lgpiodcxx \
    -o payloadtest
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 payloadtest ${D}${bindir}/payloadtest

    install -d ${D}${prefix}/payload_test

    # Install the systemd service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/LCR_Payload_test.service ${D}${systemd_system_unitdir}/LCR_Payload_test.service
}

FILES:${PN} += "${prefix}/payload_test"

PROVIDES = "lcr_payload"
RPROVIDES:${PN} = "lcr_payload"

SYSTEMD_SERVICE:${PN} = "LCR_Payload_test.service"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"