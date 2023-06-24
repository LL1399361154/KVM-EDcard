#!/bin/sh
/sbin/insmod virtio_blk.ko
mknod /dev/edcard c 234 0
