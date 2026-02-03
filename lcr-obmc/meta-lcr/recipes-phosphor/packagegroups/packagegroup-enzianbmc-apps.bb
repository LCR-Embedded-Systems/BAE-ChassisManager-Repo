SUMMARY = "OpenBMC for LCR-OBMC - Applications"
PR = "r1"

inherit packagegroup

PROVIDES = "${PACKAGES}"
PACKAGES = " \
        ${PN}-chassis \
        ${PN}-fans \
        ${PN}-flash \
        ${PN}-system \
        "

PROVIDES += "virtual/obmc-chassis-mgmt"
PROVIDES += "virtual/obmc-fan-mgmt"
PROVIDES += "virtual/obmc-flash-mgmt"
PROVIDES += "virtual/obmc-system-mgmt"

RPROVIDES:${PN}-chassis += "virtual-obmc-chassis-mgmt"
RPROVIDES:${PN}-fans += "virtual-obmc-fan-mgmt"
RPROVIDES:${PN}-flash += "virtual-obmc-flash-mgmt"
RPROVIDES:${PN}-system += "virtual-obmc-system-mgmt"

SUMMARY:${PN}-chassis = "LCR-OBMC Chassis"
RDEPENDS:${PN}-chassis = " \
        "

SUMMARY:${PN}-fans = "LCR-OBMC Fans"
RDEPENDS:${PN}-fans = " \
        "

SUMMARY:${PN}-flash = "LCR-OBMC Flash"

RDEPENDS:${PN}-flash = " \
        "

SUMMARY:${PN}-system = "LCR-OBMC System"
RDEPENDS:${PN}-system = " \
        gdb \
        entity-manager \
        ethtool \
        e2fsprogs-resize2fs \
        git \
        webui-vue \
        "
