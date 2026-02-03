SUMMARY = "LCR specific ipmitool"
DESCRIPTION = "IPMITOOL modified for LCR VITA 46.11"

SECTION = "kernel/userland"

LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://COPYING;md5=9aa91e13d644326bf281924212862184"

DEPENDS = "openssl readline ncurses systemd"
SRC_URI = "                           \
           file://AUTHORS             \
           file://README              \
           file://bootstrap           \
           file://ChangeLog           \
           file://configure.ac        \
           file://contrib             \
           file://control             \
           file://COPYING             \
           file://csv-revision        \
           file://doc                 \
           file://include             \
           file://INSTALL             \
           file://lib                 \
           file://Makefile.am         \
           file://src                 \
           ${IANA_ENTERPRISE_NUMBERS} \
           file://0001-csv-revision-Drop-the-git-revision-info.patch \
           "

UPSTREAM_CHECK_GITTAGREGEX = "IPMITOOL_(?P<pver>\d+(_\d+)+)"

IANA_ENTERPRISE_NUMBERS ?= ""

# Add these via bbappend if this database is needed by the system
#IANA_ENTERPRISE_NUMBERS = "http://www.iana.org/assignments/enterprise-numbers.txt;name=iana-enterprise-numbers;downloadfilename=iana-enterprise-numbers"
#SRC_URI[iana-enterprise-numbers.sha256sum] = "cdd97fc08325667434b805eb589104ae63f7a9eb720ecea73cb55110b383934c"

S = "${WORKDIR}"

inherit autotools pkgconfig

do_install:append() {
        if [ -e ${UNPACKDIR}/iana-enterprise-numbers ]; then
                install -Dm 0755 ${UNPACKDIR}/iana-enterprise-numbers ${D}${datadir}/misc/enterprise-numbers
        fi

        # Rename this binary to lcr-ipmitool
        mv ${D}${bindir}/ipmitool ${D}${bindir}/lcr-ipmitool
}



PACKAGES =+ "${PN}-ipmievd"
FILES:${PN}-ipmievd += "${sbindir}/ipmievd"
FILES:${PN} += "${datadir}/misc"

FILES:${PN} += "${datadir}/ipmitool"
FILES:${PN} += "${datadir}/ipmitool/oem_ibm_sel_map"
FILES:${PN} += "${bindir}/lcr-ipmitool"

# --disable-dependency-tracking speeds up the build
# --enable-file-security adds some security checks
# --disable-intf-free disables FreeIPMI support - we don't want to depend on
#   FreeIPMI libraries, FreeIPMI has its own ipmitoool-like utility.
# --disable-registry-download prevents the IANA numbers from being fetched
#   at build time, as it is not repeatable.
#
EXTRA_OECONF = "--disable-dependency-tracking --enable-file-security --disable-intf-free --enable-intf-dbus \
                --disable-registry-download \
                "

# http://errors.yoctoproject.org/Errors/Details/766896/
# git/lib/ipmi_fru.c:1556:41: error: initialization of 'struct fru_multirec_mgmt *' from incompatible pointer type 'struct fru_multirect_mgmt *' [-Wincompatible-pointer-types]
CFLAGS += "-Wno-error=incompatible-pointer-types"





