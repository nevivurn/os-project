#!/usr/bin/env bash

############################################
# OSSPR2022                                #
#                                          #
# Builds your ARM64 kernel.                 #
############################################

set -e

# Did you install ccache?
type ccache

# Some cleanups and setups
rm -f arch/arm64/boot/Image
rm -f arch/arm64/boot/dts/broadcom/*.dtb

if type aarch64-unknown-linux-gnu-gcc >/dev/null 2>&1; then
	CROSS_COMPILER='ccache aarch64-unknown-linux-gnu-'
elif type aarch64-linux-gnu-gcc >/dev/null 2>&1; then
	CROSS_COMPILER='ccache aarch64-linux-gnu-'
fi

# Build .config
make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILER" tizen_bcmrpi3_defconfig

# Build kernel
make ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILER" -j$(nproc)
