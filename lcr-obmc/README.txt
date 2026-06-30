#########################
# Requirements
#########################

1) Ubuntu server 20.04 or later.  (Tested on 20.04LTS)
2) Memory 2G+
3) Hard Disk  100G Free
4) Non-root login

#  On new machine, run:
$ sudo apt update -y && sudo apt upgrade -y
$ sudo apt install -y autoconf build-essential chrpath cmake cpio ca-certificates curl debianutils \
        diffstat doxygen fop gcc-multilib gawk git iputils-ping libboost-dev libgl1 libglx-mesa0 \
        liblz4-tool libncurses-dev libsdl1.2-dev libsystemd-dev libtool libudev-dev lz4 lzop \
        mesa-common-dev net-tools pylint python3 python3-git python3-jinja2 python3-pexpect \
        python3-pip python3-subunit python-is-python3 qtbase5-dev rsync socat texinfo tree unzip \
        valgrind wget xsltproc xterm xz-utils zstd


NOTE: Depending on Ubuntu version, some of these packages may change.


#########################
# CREATE AND POPULATE
# PROJECT DIRECTORY
#########################

# Create project dir
$ cd        

# Install credentials for repo
# NOTE: on my machine, this simply entailed adding github keys

$ mkdir projects && cd projects
$ sudo mkdir /tftpboot
$ sudo chmod 777 /tftpboot

# Fetch source repositories
$ git clone git@github.com:jeff-triple-crown/LCR-ChassisManager-Repo.git

#  download petalinux 2024.2 from site:
#     https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/embedded-design-tools.html
#  and place petalinux-v2024.2-11062026-installer.run in projects dir.
#  To verify petalinux-v2024.2-11062026-installer.run integrity, run md5sum on it.  That should yield:
# 1c88abbb1ba23370692c0aa9d27c0e74  ../petalinux-v2024.2-11062026-installer.run

# INSTALL PETALINUX
$ chmod +x ./petalinux-v2024.2-11062026-installer.run
$ ./petalinux-v2024.2-11062026-installer.run

# projects directory should now look like this.
├── LCR-ChassisManager-Repo
├── petalinux-2024.2
└── petalinux-v2024.2-11062026-installer.run


#########################
# CREATE OBMC ROOTFS
#########################

$ cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc
$ . ./setup zc702-zynq7
$ bitbake obmc-phosphor-image

cd ~/projects/LCR-ChassisManager-Repo/lcr-obmc
. ./setup zc702-zynq7
bitbake obmc-phosphor-image


#########################
# CREATE PETALINUX IMAGE
#########################
# NOTE: Petalinux gets confused if yocto setup has
# been run on a different project beforehand.
# So, it's best to log out and log back in for a new
# session before proceeding. Trying to run both petalinux
# and regular yocto in the same ssh session usually caused
# petalinux lockups.


# Install Petalinux build system
cd ~/projects/petalinux-2024.2
. ./settings.sh
cd ..

# Create new project, redirecting TMPDIR if necessary (if you have an NFS mount, for instance)
petalinux-create project -n peta-bsp --template zynq --tmpdir ~/tmp-peta

# projects directory should look like this
├── LCR-ChassisManager-Repo
├── peta-bsp
├── petalinux-2024.2
└── petalinux-v2024.2-11062026-installer.run

# Copy petalinux hardware file and configure baseline project
cd peta-bsp
cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_LCR.xsa .
petalinux-config --get-hw-description=./BCM_LCR.xsa
# NOTE: Save config without edits then exit from menuconfig

# Now overlay lcr configuration files
# And sync them to this build machine
cp -a ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/project-spec .
grep -rl @@@HOME@@@ * | xargs sed -i "s|@@@HOME@@@|$HOME|g"

# Build petalinux image
petalinux-build

cd peta-bsp
cp ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_LCR.xsa .
petalinux-config --get-hw-description=./BCM_LCR.xsa
cp -a ../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/project-spec .
grep -rl @@@HOME@@@ * | xargs sed -i "s|@@@HOME@@@|$HOME|g"
petalinux-build

#########################
# CREATE OBMC IMAGE
#########################

cd ./images/linux

# Modify petalinux image with obmc rootfs
# Copy import-obmc-files.sh and lcr-image.its from the peta-utils directory
cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/import-obmc-rootfs.sh .
cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/lcr-image.its .

# Overwrite rootfs files with OBMC rootfs files
./import-obmc-rootfs.sh

#Create merged image
mkimage -f lcr-image.its image.ub

# image.ub now has lcr-obmc rootfs and petalinux kernel combined

# Now, we package the bootloader with the current bitstream, u-boot, etc.
petalinux-package boot --fsbl ./zynq_fsbl.elf --u-boot ./u-boot.elf --fpga ./system.bit --force
# ./BOOT.BIN has now been produced

cd ./images/linux
cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/import-obmc-rootfs.sh .
cp ../../../LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/utils/lcr-image.its .
./import-obmc-rootfs.sh
mkimage -f lcr-image.its image.ub
petalinux-package boot --fsbl ./zynq_fsbl.elf --u-boot ./u-boot.elf --fpga ./system.bit --force


#########################
# FLASH IMAGES TO BOARD
#########################
# NOTE:  These instructions assume xsct has been installed
#        on the Linux Build Server and that the Device to
#        be programmed is connected vi JTAG cable.
#
# Place board into jtag mode and run xsct.
# NOTE: In the current setup, we are using an intelligent
#       outlet strip to control both power to the Device
#       to be programmed as well as placing the Device
#       into either JTAG mode or QSPI mode.
#
# Program LCR board using xsct
xsct% connect
xsct% targets
xsct% target 2
xsct% program_flash -f ~/projects/peta-bsp/image/linux/BOOT.BIN -fsbl ~/projects/peta-bsp/image/linux/zynq_fsbl.elf -offset 0 -flash_type qspi-x4-single
xsct% program_flash -f ~/projects/peta-bsp/image/linux/boot.scr -fsbl ~/projects/peta-bsp/image/linux/zynq_fsbl.elf -offset 0x800000 -flash_type qspi-x4-single
xsct% program_flash -f ~/projects/peta-bsp/image/linux/image.ub -fsbl ~/projects/peta-bsp/image/linux/zynq_fsbl.elf -offset 0x840000 -flash_type qspi-x4-single

# Note: place board into qspi mode and power cycle the board
# Board will boot up to u-boot prompt.
# Load and run linux kernel
Zynq> run bootcmd
 
# When board boots into linux, login
lcr-obmc login: root
Password: 0penBmc     (note: zero - not capital 'o')

#########################




#########################
#########################


#########################
# TO UPGRADE IMAGE WITH 
# NEW PETALINUX DATA
# (If board hardware is
#  changed, for instance)
#########################

# New xsa file
$ cp BCM_FPGA_Rx.xsa LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/BCM_LCR.xsa

# New Device Tree Files
$ cp new-dt-file LCR-ChassisManager-Repo/lcr-obmc/petalinux-files/configs/lcr-dts-mods.dtsi 

# New Kernel Configuration (after petalinux-config -c kernel changes)
$ merge project-spec/meta-user/recipes-kernel/linux/linux-xlnx/user_2025-03-11-02-00-00.cfg and bsp.cfg and copy to lcr-obmc/petalinux-files/configs/kernel_configs
# and then edit project-spec/meta-user/recipes-kernel/linux/linux-xlnx_%.bbappend to remove user file fragment append information


modifying .patch files:
bitbake -c devshell phosphor-ipmi-host

do this:
git add .
git commit -m "baseline"



make changes then Commit changes:
git add myfile.cpp
git commit -m "Added/modded file"



create patch:
git format-patch -1

copy patch into the directory containing the recipe


Use this command when attempting to use rmcp:
ipmitool -I lanplus -H 192.168.0.104 -U root -P 0penBmc -C 17 sensor list

The -C 17 is meant to ensure that the lanplus uses the correct cipher.
