/*
 * Virtio Block Device
 *
 * Copyright IBM, Corp. 2007
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_VIRTIO_PCIE_H
#define QEMU_VIRTIO_PCIE_H


#include "virtio-pcie-common.h"
#include "hw/virtio/virtio.h"
#include "hw/block/block.h"
#include "sysemu/iothread.h"
#include "sysemu/block-backend.h"

/* This is the last element of the write scatter-gather list */
struct virtio_pcie_inhdr
{
    unsigned char status;
};

struct VirtIOPCIEConf
{
    uint32_t flag;//只是一个标记，不做他用
};


struct VirtIOPCIEReq;

typedef struct VirtIOPCIE {
    VirtIODevice parent_obj;
    void *rq;
    QEMUBH *bh;
    struct VirtIOPCIEConf conf;
    unsigned short sector_mask;
    VMChangeStateEntry *change;
    //void *inputbuf;
    void *device;
} VirtIOPCIE;

typedef struct VirtIOPCIEReq {
    VirtQueueElement elem;
    int64_t sector_num;
    VirtIOPCIE *dev;
    VirtQueue *vq;
    struct virtio_pcie_inhdr *in;
    struct virtio_pcie_outhdr out;
    QEMUIOVector qiov;
    size_t in_len;
    struct VirtIOPCIEReq *next;
    struct VirtIOPCIEReq *mr_next;
    char *buff;

} VirtIOPCIEReq;

#define VIRTIO_PCIE_MAX_MERGE_REQS 32

typedef struct MultiReqBuff {
    VirtIOPCIEReq *reqs[VIRTIO_PCIE_MAX_MERGE_REQS];
    bool is_write;
} MultiReqBuff;

#endif
