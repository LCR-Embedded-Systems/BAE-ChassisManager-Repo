SUMMARY = "MDIO Netlink Kernel Module"
LICENSE = "GPL-2.0-or-later"
LIC_FILES_CHKSUM = "file://${WORKDIR}/git/COPYING;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit module

SRC_URI = "git://github.com/wkz/mdio-tools.git;protocol=https;branch=master"
SRCREV = "f74eaf38dbda441df4fcaeb21ca4465957953a2f"

S = "${WORKDIR}/git"

EXTRA_OEMAKE = "-C kernel/ KDIR=${STAGING_KERNEL_DIR}"

MODULES_MODULE_SYMVERS_LOCATION = "kernel"
MODULES_INSTALL_TARGET = "install"

MODULE_TARBALL_LINK_NAME = ""

RPROVIDES:${PN} += "kernel-module-mdio-netlink"