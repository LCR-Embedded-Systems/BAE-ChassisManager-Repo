FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://0001-lcr-ipmid-mods.patch \
            file://0001-additional-patch-for-vita4611cm.cpp.patch \
            "


PACKAGECONFIG:append = " dynamic-sensors"
PACKAGECONFIG:append = " sel-logger-clears-sel"

# EXTRA_OEMESON:append = " -Ddynamic-sensors=enabled"


