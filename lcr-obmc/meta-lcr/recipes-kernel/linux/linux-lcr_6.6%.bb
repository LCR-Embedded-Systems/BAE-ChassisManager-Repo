# LINUX_VERSION ?= "5.15.19"
LINUX_VERSION ?= "6.6.40"
META_VERSION ?= "6.6"

# SRCREV_machine = "3701a4ff4efdb2a2d86ebac61340dfa42f1dbc2a"
SRCREV_machine = "2b7f6f70a62a52a467bed030a27c2ada879106e9"
# SRCREV_meta = "5f6249ab6fc0f2ac02d745c815adfbf2ad2f92fa"
SRCREV_meta = "3adceb388e86d1b704d8153af1d734877529085d"

require linux-lcr.inc

LIC_FILES_CHKSUM = "file://COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"

PACKAGE_ARCH = "${MACHINE_ARCH}"

# iptables mods
SRC_URI += "file://iptables.cfg"
# cfg mods
SRC_URI += "file://phylib.cfg"
