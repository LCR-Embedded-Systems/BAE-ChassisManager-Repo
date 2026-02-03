FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://lcr-iptables.rules"

do_install:append() {
    cat ${WORKDIR}/lcr-iptables.rules >> ${D}${sysconfdir}/iptables/iptables.rules
}

