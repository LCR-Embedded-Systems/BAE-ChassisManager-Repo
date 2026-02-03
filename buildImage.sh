#!/bin/bash

# Original build logic starts here

echo "Building on branch: " && git branch --show-current

BRANCH=$(git branch --show-current)

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc

source oe-init-build-env .

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc

. ./setup zc702-zynq7

if [ $? -ne 0 ]; then
        echo "Error: bitbake build failed."
        exit 1
fi

bitbake obmc-phosphor-image

if [ $? -ne 0 ]; then
        echo "Error: bitbake build failed."
        exit 1
fi

cd ~/projects/petalinux-2024.2
. ./settings.sh
cd ..

petalinux-create project -n peta-bsp --template zynq --tmpdir ~/tmp-peta

cd peta-bsp
# cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_top_wrapper_KX.xsa .
# cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_top_wrapper_IOPLL.xsa .
# cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_top_wrapper_rv_c.xsa .
cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_top_wrapper_rv_c4.xsa .

# cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_FPGA_R4_3.xsa .
echo "running petalinux config now"
# petalinux-config --get-hw-description=./BCM_top_wrapper_KX.xsa --silentconfig
# petalinux-config --get-hw-description=./BCM_top_wrapper_IOPLL.xsa --silentconfig
# petalinux-config --get-hw-description=./BCM_top_wrapper_rv_c.xsa --silentconfig
petalinux-config --get-hw-description=./BCM_top_wrapper_rv_c4.xsa --silentconfig
# petalinux-config -c kernel --silentconfig
# petalinux-config --get-hw-description=./BCM_FPGA_R4_3.xsa --silentconfig
cp -a ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/project-spec .
grep -rl @@@HOME@@@ * | xargs sed -i "s|@@@HOME@@@|$HOME|g"
petalinux-build

if [ $? -ne 0 ]; then
        echo "Error: petalinux build failed."
        exit 1
fi

cd ./images/linux

cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/import-obmc-rootfs.sh .
cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/lcr-image.its .
echo "running import obmc rootfs now"
./import-obmc-rootfs.sh
mkimage -f lcr-image.its image.ub
petalinux-package boot --fsbl ./zynq_fsbl.elf --u-boot ./u-boot.elf --fpga ./system.bit --force

# Generate unique names and copy files
unique_suffix="${BRANCH}_$(date +%Y%m%d_%H%M%S)"
cp BOOT.BIN "BOOT_${unique_suffix}.BIN"
cp boot.scr "boot_${unique_suffix}.scr"
cp image.ub "image_${unique_suffix}.ub"

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "Finished building on branch: $BRANCH"

echo "-----------------------------------------------------------------------------------------------------------------------------"
# Print the scp command for the user
echo "Build completed successfully."
echo "To copy the files to your local machine, run the following command from the computer you intend to flash on:"
echo "scp lcr@192.168.0.35:./projects/peta-bsp/images/linux/BOOT_${unique_suffix}.BIN lcr@192.168.0.35:./projects/peta-bsp/images/linux/boot_${unique_suffix}.scr lcr@192.168.0.35:./projects/peta-bsp/images/linux/image_${unique_suffix}.ub ."

echo "-----------------------------------------------------------------------------------------------------------------------------"
# Print the flash command for the user
echo "Run this command to flash:"
echo "connect; targets; target 2; program_flash -f c:/users/tc/lcr/BOOT_${unique_suffix}.BIN -fsbl c:/users/tc/lcr/zynq_fsbl.elf -offset 0x0 -flash_type qspi-x4-single; program_flash -f c:/users/tc/lcr/boot_${unique_suffix}.scr -fsbl c:/users/tc/lcr/zynq_fsbl.elf -offset 0x800000 -flash_type qspi-x4-single; program_flash -f c:/users/tc/lcr/image_${unique_suffix}.ub -fsbl c:/users/tc/lcr/zynq_fsbl.elf -offset 0x840000 -flash_type qspi-x4-single"

# Original build logic ends here

