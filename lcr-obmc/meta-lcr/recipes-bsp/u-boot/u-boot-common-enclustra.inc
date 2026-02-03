SECTION = "bootloaders"
DEPENDS += "flex-native bison-native bc-native dtc-native xxd-native"

LICENSE = "GPL-2.0-or-later"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=5a7450c57ffe5ae63fd732446b988025"

UBRANCH = "master"
SRC_URI = "git://github.com/enclustra-bsp/xilinx-uboot.git;branch=${UBRANCH};protocol=https"
SRCREV = "7b4e587ab5a3180434836f57eeb6ed209b5e4a3c"

S = "${WORKDIR}/git"
B = "${WORKDIR}/build"
do_configure[cleandirs] = "${B}"

PV .= "+${UBRANCH}+"
