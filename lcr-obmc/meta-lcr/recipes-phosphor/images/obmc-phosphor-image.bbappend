OBMC_IMAGE_EXTRA_INSTALL:append = " openssh-sftp-server"
OBMC_IMAGE_EXTRA_INSTALL:append = " phosphor-ipmi-ipmb phosphor-ipmi-host"
OBMC_IMAGE_EXTRA_INSTALL:append = " python3-smbus"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_sensors"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_adc_sensors"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_interface"
OBMC_IMAGE_EXTRA_INSTALL:append = " LCR_fan_controller"
OBMC_IMAGE_EXTRA_INSTALL:append = " LCR_save_vars"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_mandatory_sensors"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_gpio"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_gpiomonitor"
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr_boot"
OBMC_IMAGE_EXTRA_INSTALL:append = " phytool"
OBMC_IMAGE_EXTRA_INSTALL:append = " LCR_KX_config"

# Replace ipmitool
OBMC_IMAGE_EXTRA_INSTALL:append = " lcr-ipmitool"
IMAGE_INSTALL:remove = "ipmitool"

# Open port 623 for RMCP
IMAGE_INSTALL:append = " iptables"


# Create a file with image version information

# Image Version
IMAGE_VERSION = "1.0.0"

# Guard against time change when doing post-processing
#do_rootfs[vardepsexclude] += " TIME DATE DATETIME"

ROOTFS_POSTPROCESS_COMMAND:append = " create_image_version_file; "

create_image_version_file() {
    timestamp=$(date -d @${BUILD_DATESTAMP} '+%Y-%m-%d %H:%M:%S' 2>/dev/null || date -r ${BUILD_DATESTAMP} '+%Y-%m-%d %H:%M:%S')
    echo "Build Timestamp: $timestamp" > ${IMAGE_ROOTFS}/etc/image-version
    chmod 644 ${IMAGE_ROOTFS}/etc/image-version
}

# Install a script to display the image version
ROOTFS_POSTPROCESS_COMMAND:append = " install_image_version_script; "

install_image_version_script() {
    echo '#!/bin/sh' > ${IMAGE_ROOTFS}/usr/bin/lcr-version
    echo 'cat /etc/image-version' >> ${IMAGE_ROOTFS}/usr/bin/lcr-version
    chmod +x ${IMAGE_ROOTFS}/usr/bin/lcr-version
}

FILES_${PN}:append = " /etc/image-version /usr/bin/lcr-version"
