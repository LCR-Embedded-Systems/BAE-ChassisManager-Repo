SUMMARY = "Fan Controller"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://src/main.cpp \
           file://src/memory.cpp \
           file://include/memory.hpp \
           file://include/global.hpp \
           file://include/thresholds_default.json \
           file://include/thresholds.json"

S = "${WORKDIR}"

inherit pkgconfig systemd

DEPENDS += "sdbusplus phosphor-dbus-interfaces systemd phosphor-logging fmt nlohmann-json"

do_compile() {
    ${CXX} ${CXXFLAGS} -std=c++20 ${LDFLAGS} \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --cflags nlohmann-json` \
    ${WORKDIR}/src/main.cpp \
    ${WORKDIR}/src/memory.cpp \
    -I ${WORKDIR}/include \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs sdbusplus` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs phosphor-dbus-interfaces phosphor-logging` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs libsystemd` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs fmt` \
    `${STAGING_BINDIR_NATIVE}/pkg-config --libs nlohmann-json` \
    -o save
}

do_install() {
    install -d ${D}${bindir}
    install -m 0777 save ${D}${bindir}/save

    # Install the thresholds.json file
    install -d ${D}${prefix}/share/thresholds
    install -m 0777 ${WORKDIR}/include/thresholds.json ${D}${prefix}/share/thresholds/thresholds.json
    install -m 0777 ${WORKDIR}/include/thresholds_default.json ${D}${prefix}/share/thresholds/thresholds_default.json
}

# Include installed files in the main package
FILES:${PN} += "${prefix}/share/thresholds ${prefix}/share/thresholds/thresholds.json"
FILES:${PN} += "${prefix}/share/thresholds ${prefix}/share/thresholds/thresholds_default.json"

PROVIDES = "LCR_save_vars"
RPROVIDES:${PN} = "LCR_save_vars"
