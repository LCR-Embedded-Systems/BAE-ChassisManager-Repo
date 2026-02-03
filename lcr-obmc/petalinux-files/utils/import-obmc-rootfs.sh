#!/bin/bash

# Copy obmc rootfs files
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.cpio rootfs.cpio || exit
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.cpio.gz rootfs.cpio.gz || exit
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.cpio.gz.u-boot rootfs.cpio.gz.u-boot || exit
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.ext4 rootfs.ext4 || exit
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.manifest rootfs.manifest || exit
cp ~/tmp-obmc/deploy/images/zc702-zynq7/obmc-phosphor-image-zc702-zynq7.tar.gz rootfs.tar.gz || exit

# Copy tailored boot script
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/boot.scr .

# These are no longer accurate, so delete
rm rootfs.qemuboot.conf
rm rootfs.spdx.tar.zst
rm rootfs.testdata.json
sync

