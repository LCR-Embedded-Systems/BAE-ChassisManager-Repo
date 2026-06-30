# SEL Logger has the ability to monitor and automatically log SEL records for
# various types of events, but this is disabled by default.  The following
# flags can be set in a .bbappend to enable specific types of event
# monitoring:
#
#   SEL_LOGGER_MONITOR_THRESHOLD_EVENTS:
#      Monitors and logs SEL records for threshold sensor events
SUMMARY = "Journal IPMI SEL Logger"
DESCRIPTION = "Utility to write IPMI SEL records to the journal"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"
DEPENDS += " \
  boost \
  sdbusplus \
  systemd \
  phosphor-dbus-interfaces \
  "
SRCREV = "9b2f8db0c841b7f2d1ee6e0b359f70281b849476"
PACKAGECONFIG ??= ""
PACKAGECONFIG[log-threshold] = "-Dlog-threshold=true,-Dlog-threshold=false,"
PACKAGECONFIG[log-pulse] = "-Dlog-pulse=true,-Dlog-pulse=false,"
PACKAGECONFIG[log-watchdog] = "-Dlog-watchdog=true,-Dlog-watchdog=false,"
PACKAGECONFIG[log-alarm] = "-Dlog-alarm=true,-Dlog-alarm=false,"
PACKAGECONFIG[log-host] = "-Dlog-host=true,-Dlog-host=false,"
PACKAGECONFIG[send-to-logger] = "-Dsend-to-logger=true,-Dsend-to-logger=false,phosphor-logging"
PACKAGECONFIG[sel-delete] = "-Dsel-delete=true,-Dsel-delete=false,"
PV = "0.1+git${SRCPV}"



SRC_URI = "git://github.com/openbmc/phosphor-sel-logger.git;protocol=https;branch=master"



S = "${WORKDIR}/git"
SYSTEMD_SERVICE:${PN} += "xyz.openbmc_project.Logging.IPMI.service"

inherit pkgconfig meson systemd

DEPENDS:remove = "meson-native"

meson_do_configure:prepend() {
    bbnote "=== Using newer host Meson from pipx ==="
    export PATH="$HOME/.local/bin:$PATH"

    # Wipe any old Meson 1.0.1 cache (critical after version upgrade)
    if [ -d "${B}/meson-private" ]; then
        bbnote "Wiping stale Meson configuration (coredata.dat etc.)"
        rm -rf "${B}/meson-private" \
               "${B}/build.ninja" \
               "${B}/meson-logs" \
               "${B}/meson-info" \
               "${B}"/*.dat \
               "${B}"/*.ninja
    fi
}

do_compile:prepend() {
    bbnote "=== Using newer host Meson from pipx (compile) ==="
    export PATH="$HOME/.local/bin:$PATH"
}

# Also cover install step (just in case)
do_install:prepend() {
    bbnote "=== Using newer host Meson from pipx (install) ==="
    export PATH="$HOME/.local/bin:$PATH"
}

# Force full clean of build dir on every configure (safe with externalsrc)
do_configure[cleandirs] += "${B}"

PACKAGECONFIG:append = " sel-delete"