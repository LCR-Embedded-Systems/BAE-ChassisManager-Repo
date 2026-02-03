DESCRIPTION = "Linux kernel from LCR BSP"
SECTION = "kernel"
LICENSE = "GPL-2.0-only"

KCONFIG_MODE="--alldefconfig"

require recipes-kernel/linux/linux-yocto.inc

KERNELURI ?= "git://github.com/Xilinx/linux-xlnx.git;name=machine;branch=xlnx_rebase_v6.6_LTS;protocol=https"
YOCTO_META ?= "git://git.yoctoproject.org/yocto-kernel-cache;type=kmeta;name=meta;branch=yocto-6.6;destsuffix=yocto-kmeta"
SRC_URI = "${KERNELURI} ${YOCTO_META}"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

#SRC_URI += "file://xilinx-gpio-dual-get.patch"

SRC_URI += "file://lcr_defconfig"

SRC_URI += "file://system-top.dts;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://system-conf.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://system-user.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://zc702.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://pl.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://pcw.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://zynq-7000.dtsi;subdir=git/arch/arm/boot/dts"
SRC_URI += "file://include;subdir=git/arch/arm/boot/dts"

PV = "${LINUX_VERSION}"
LINUX_VERSION_EXTENSION = "-xilinx"

# Workaround for kernel 6.6.40 modules_install issue
KERNEL_MODULE_AUTOLOAD = ""

do_install:append() {
    for moddir in ${D}/lib/modules/*; do
        rm -rf "$moddir/source" "$moddir/build"
    done
    rm -rf ${D}/usr/src/kernel
}