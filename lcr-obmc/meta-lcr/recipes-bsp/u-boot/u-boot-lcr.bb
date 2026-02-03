require u-boot-common-enclustra.inc
#require recipes-bsp/u-boot/u-boot.inc

PROVIDES += "u-boot"

DEPENDS += "enzianbmc-fpga"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
PACKAGE_ARCH = "${MACHINE_ARCH}"

SRC_URI += "file://fix-zynqmp-dram-ecc-initialization.patch"
SRC_URI += "file://remove-hardcoded-enclustra-config.patch"

SRC_URI += "file://lcr_bootloader_defconfig;subdir=${S}/configs"

#SRC_URI += "file://enzianbmc-zx5.dts;subdir=${S}/arch/arm/dts"
#SRC_URI += "file://dt-makefile.patch"

SRC_URI += "file://enzianbmc.its"
SRC_URI += "file://u-boot-env.txt"

SRC_URI+= "file://boot.cmd"

# Patch only needed for ZX5
#SRC_URI:append:zc702-zynq7 = "
#    file://initialize-UART-at-least-once.patch
#"

SYSROOT_DIRS += "/boot"
