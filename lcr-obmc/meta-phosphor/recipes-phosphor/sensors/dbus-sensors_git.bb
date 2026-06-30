SUMMARY = "dbus-sensors"
DESCRIPTION = "Dbus Sensor Services Configured from D-Bus"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"
DEPENDS = " \
    boost \
    i2c-tools \
    libgpiod \
    liburing \
    nlohmann-json \
    phosphor-logging \
    sdbusplus \
    "
SRCREV = "c361e222997b94f08a38975826a25ba6ab8ffa86"
PACKAGECONFIG ??= " \
    fansensor \
    intrusionsensor \
    ipmbsensor \
    "

PACKAGECONFIG[fansensor] = "-Dfan=enabled, -Dfan=disabled"
PACKAGECONFIG[intrusionsensor] = "-Dintrusion=enabled, -Dintrusion=disabled"
PACKAGECONFIG[ipmbsensor] = "-Dipmb=enabled, -Dipmb=disabled"
PACKAGECONFIG[nvmesensor] = "-Dnvme=enabled, -Dnvme=disabled"
PV = "0.1+git${SRCPV}"

SRC_URI = "git://github.com/openbmc/dbus-sensors.git;branch=master;protocol=https \
           file://0001-small-mods.patch \
           "

SYSTEMD_SERVICE:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'fansensor', \
                                               'xyz.openbmc_project.fansensor.service', \
                                               '', d)}"
SYSTEMD_SERVICE:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'intrusionsensor', \
                                               'xyz.openbmc_project.intrusionsensor.service', \
                                               '', d)}"
SYSTEMD_SERVICE:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'ipmbsensor', \
                                               'xyz.openbmc_project.ipmbsensor.service', \
                                               '', d)}"
SYSTEMD_SERVICE:${PN} += "${@bb.utils.contains('PACKAGECONFIG', 'nvmesensor', \
                                               'xyz.openbmc_project.nvmesensor.service', \
                                               '', d)}"
S = "${WORKDIR}/git"

inherit pkgconfig meson systemd

EXTRA_OEMESON:append = " -Dtests=disabled"
