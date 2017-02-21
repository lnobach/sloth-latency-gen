#!/bin/bash

DPDK_DIR=../deps/dpdk
DPDK_PLAF=x86_64-native-linuxapp-gcc

DPDK_DEVBIND=$DPDK_DIR/tools/dpdk-devbind.py
#DPDK_DEVBIND=$DPDK_DIR/tools/dpdk_nic_bind.py

#Required kernel modules
modprobe uio
insmod $DPDK_DIR/$DPDK_PLAF/kmod/igb_uio.ko
#insmod $DPDK_DIR/$DPDK_PLAF/kmod/rte_kni.ko

#The following must be done for every device we want to use. Only for VirtIO devices this is not required.

$DPDK_DEVBIND --bind igb_uio 00:09.0
$DPDK_DEVBIND --bind igb_uio 00:0a.0

export LD_LIBRARY_PATH=$DPDK_DIR/$DPDK_PLAF/lib
./latgen -c 0x7 -w 00:09.0 -w 00:0a.0 -n 2 -d $DPDK_DIR/$DPDK_PLAF/lib/librte_pmd_nfp.so $@

# coremask MUST assign at least cores equal to number of interfaces * 2 + 1
# Since DPDK 16.04, PMD drivers must always be assigned with the "-d" parameter

#  -d $DPDK_DIR/$DPDK_PLAF/lib/librte_pmd_ixgbe.so
#  -d lib/librte_pmd_mlx4.so

