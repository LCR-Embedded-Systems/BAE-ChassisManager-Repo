SUMMARY = "basekx script"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://configurekx.sh \
"

S = "${WORKDIR}"

inherit pkgconfig systemd

RDEPENDS:${PN} += "bash"

do_install() {
    install -d ${D}${bindir}
    install -m 0777 configurekx.sh ${D}${bindir}/configurekx.sh
}

PROVIDES = "LCR_KX_config"
RPROVIDES:${PN} = "LCR_KX_config"
