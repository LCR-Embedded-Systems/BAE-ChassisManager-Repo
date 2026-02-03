FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://bsp.cfg"
SRC_URI:append = " file://user_2025-05-13-16-56-00.cfg"
KERNEL_FEATURES:append = " bsp.cfg"
KERNEL_FEATURES:append = " user_2025-05-13-16-56-00.cfg"

