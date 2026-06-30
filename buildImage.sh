#!/bin/bash

CURRENT_XSA="BCM_top_wrapper_BAE_052026_rv1"

echo "Building on branch: " && git branch --show-current
# cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_top_wrapper_BAE_051526.xsa ~/projects/peta-bsp
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/$CURRENT_XSA.xsa ~/projects/peta-bsp
cp -a ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/project-spec ~/projects/peta-bsp
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/import-obmc-rootfs.sh ~/projects/peta-bsp/images/linux
cp ~/projects/LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/lcr-image.its ~/projects/peta-bsp/images/linux
BRANCH=$(git branch --show-current)

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc

source oe-init-build-env .

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc

. ./setup zc702-zynq7

bitbake -c clean lcr-ipmitool

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
echo "LCR INFO running petalinux config now"
petalinux-config --get-hw-description=./$CURRENT_XSA.xsa --silentconfig
# petalinux-config --get-hw-description=./BCM_top_wrapper_BAE_051526.xsa --silentconfig
echo "LCR INFO running grep and move now"
grep -rl @@@HOME@@@ ./project-spec/ | xargs sed -i "s|@@@HOME@@@|$HOME|g"
echo "LCR INFO running petalinux build now"
petalinux-build

if [ $? -ne 0 ]; then
        echo "Error: petalinux build failed."
        exit 1
fi

cd ./images/linux
echo "LCR INFO running import obmc rootfs now"
./import-obmc-rootfs.sh
mkimage -f lcr-image.its image.ub
petalinux-package boot --fsbl ./zynq_fsbl.elf --u-boot ./u-boot.elf --fpga ./system.bit --force

# Generate unique names and copy files
unique_suffix="${BRANCH}_$(date +%Y%m%d_%H%M%S)"
cp BOOT.BIN "BOOT_${unique_suffix}.BIN"
cp boot.scr "boot_${unique_suffix}.scr"
cp image.ub "image_${unique_suffix}.ub"

find . -type f \( -name "image_*" -o -name "boot_*" -o -name "BOOT_*" \) -mtime +7 -delete

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "Finished building on branch: $BRANCH"

echo "-----------------------------------------------------------------------------------------------------------------------------"
# Print the scp command for the user
CURRENT_ELF="zynq_fsbl"
echo "Build completed successfully."

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "IF DOING A FIRST TIME FLASH OR REBUILDING THE XSA, DO THE FOLLOWING."

echo "To copy the files to your local machine, ensure it is on the same LAN network as the build machine and run the following command from the computer you intend to flash on:"
echo "scp lcr@192.168.0.35:./projects/peta-bsp/images/linux/$CURRENT_ELF.elf lcr@192.168.0.35:./projects/peta-bsp/images/linux/BOOT_${unique_suffix}.BIN lcr@192.168.0.35:./projects/peta-bsp/images/linux/boot_${unique_suffix}.scr lcr@192.168.0.35:./projects/peta-bsp/images/linux/image_${unique_suffix}.ub ."

echo "-----------------------------------------------------------------------------------------------------------------------------"
# Print the flash command for the user
echo "Run this command to flash:"
echo "connect; targets; target 2; program_flash -f c:/users/tc/lcr/BOOT_${unique_suffix}.BIN -fsbl c:/users/tc/lcr/$CURRENT_ELF.elf -offset 0x0 -flash_type qspi-x2-single; program_flash -f c:/users/tc/lcr/boot_${unique_suffix}.scr -fsbl c:/users/tc/lcr/$CURRENT_ELF.elf -offset 0x800000 -flash_type qspi-x2-single; program_flash -f c:/users/tc/lcr/image_${unique_suffix}.ub -fsbl c:/users/tc/lcr/$CURRENT_ELF.elf -offset 0x840000 -flash_type qspi-x2-single"

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "IF YOU ARE FLASHING NEW FIRMWARE TO A UNIT THAT ALREADY HAS A WORKING KERNEL FLASH, DO THE FOLLOWING."

echo "To copy the files to your unit, ensure it is on the same LAN network as the build machine and run the following command from the unit:"
echo "scp lcr@192.168.0.35:./projects/peta-bsp/images/linux/BOOT_${unique_suffix}.BIN lcr@192.168.0.35:./projects/peta-bsp/images/linux/boot_${unique_suffix}.scr lcr@192.168.0.35:./projects/peta-bsp/images/linux/image_${unique_suffix}.ub ."

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "Run this command to flash:"
echo "flashcp -v BOOT_${unique_suffix}.BIN /dev/mtd0 && flashcp -v boot_${unique_suffix}.scr /dev/mtd1 && flashcp -v image_${unique_suffix}.ub /dev/mtd2"

echo "-----------------------------------------------------------------------------------------------------------------------------"
echo "Once flashed, reboot for the new firmware to take effect."