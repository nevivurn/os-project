#!/usr/bin/env bash

dir="$(mktemp -d)"
sudo mount ../tizen-image/rootfs.img "$dir"
sudo cp test_ptree test_ptree_null test_ptree_zero testproc_nonmain_fork "$dir/root/"
sudo umount "$dir"
