FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://60-phosphor-networkd-default.network"
SRC_URI += "file://60-phosphor-networkd-end1.network"

do_install:append() {
    install -d ${D}${systemd_unitdir}/network
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-default.network ${D}${systemd_unitdir}/network/60-phosphor-networkd-default.network
    install -m 0644 ${WORKDIR}/60-phosphor-networkd-end1.network ${D}${systemd_unitdir}/network/60-phosphor-networkd-end1.network
}

FILES:${PN} += "${systemd_unitdir}/network/60-phosphor-networkd-default.network"
FILES:${PN} += "${systemd_unitdir}/network/60-phosphor-networkd-end1.network"
