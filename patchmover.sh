#!/bin/bash

cp -r ./devtool-mods/host-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-host/

cp ./devtool-mods/vita4611/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-host/vita4611/
cp -r ./devtool-mods/ipmb_directory-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-ipmb/
cp -r ./devtool-mods/phosphor-ipmi-net-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-ipmi-net/
cp -r ./devtool-mods/bmcweb-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/bmcweb/
cp -r ./devtool-mods/dbus-sensors/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/dbus-sensors/
cp -r ./devtool-mods/phosphor-fan-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/phosphor-fan/
cp -r ./devtool-mods/entity-manager-mods/* ./lcr-obmc/build/zc702-zynq7/workspace/sources/entity-manager/

