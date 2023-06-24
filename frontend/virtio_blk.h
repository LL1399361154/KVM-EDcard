#ifndef _LINUX_VIRTIO_BLK_H
#define _LINUX_VIRTIO_BLK_H
/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */
#include <linux/types.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>

/* Feature bits */
#define VIRTIO_BLK_BARRIER	0	/* Does host support barriers? */
#define VIRTIO_PCIE_F_CAPACITY	1	/* Indicates maximum segment size */
#define VIRTIO_PCIE_F_BUFFLENGTH	2	/* Indicates maximum # of segments */
#define VIRTIO_PCIE_F_FLAG	4	/* Legacy geometry available  */
#define VIRTIO_PCIE_F_SEG_MAX	5	/* Disk is read-only */
//#define VIRTIO_BLK_F_BLK_SIZE	6	/* Block size of disk is available*/
//#define VIRTIO_BLK_F_SCSI	7	/* Supports scsi command passthru */
//#define VIRTIO_BLK_F_WCE	9	/* Writeback mode enabled after reset */
//#define VIRTIO_BLK_F_TOPOLOGY	10	/* Topology information is available */
//#define VIRTIO_BLK_F_CONFIG_WCE	11	/* Writeback mode available in config */

#ifndef __KERNEL__
/* Old (deprecated) name for VIRTIO_BLK_F_WCE. */
#define VIRTIO_BLK_F_FLUSH VIRTIO_BLK_F_WCE
#endif

#define VIRTIO_BLK_ID_BYTES	20	/* ID string length */

struct virtio_pcie_config {
	/* The capacity (in 512-byte sectors). */
	__u64 capacity;
	/* The maximum segment size (if VIRTIO_BLK_F_SIZE_MAX) */
	__u32 bufflength;
	/* The maximum number of segments (if VIRTIO_BLK_F_SEG_MAX) */
	__u32 flag;
	/* geometry the device (if VIRTIO_BLK_F_GEOMETRY) */

	/* block size of device (if VIRTIO_BLK_F_BLK_SIZE) */
	__u32 seg_max;

} __attribute__((packed));

/*
 * Command types
 *
 * Usage is a bit tricky as some bits are used as flags and some are not.
 *
 * Rules:
 *   VIRTIO_BLK_T_OUT may be combined with VIRTIO_BLK_T_SCSI_CMD or
 *   VIRTIO_BLK_T_BARRIER.  VIRTIO_BLK_T_FLUSH is a command of its own
 *   and may not be combined with any of the other flags.
 */

/* These two define direction. */
#define VIRTIO_BLK_T_IN		0
#define VIRTIO_BLK_T_OUT	1

/* This bit says it's a scsi command, not an actual read or write. */
#define VIRTIO_BLK_T_SCSI_CMD	2

/* Cache flush command */
#define VIRTIO_BLK_T_FLUSH	4

/* Get device ID command */
#define VIRTIO_BLK_T_GET_ID    8

/* Barrier before this op. */
#define VIRTIO_BLK_T_BARRIER	0x80000000

/* This is the first element of the read scatter-gather list. */
struct virtio_pcie_outhdr {
	/* VIRTIO_BLK_T* */
	__u32 type;
	/* io priority. */
	__u32 ioprio;
	/* Sector (ie. 512 byte offset) */
	__u32 sector;
	__u32 real_len;
};

struct virtio_scsi_inhdr {
	__u32 errors;
	__u32 data_len;
	__u32 sense_len;
	__u32 residual;
};

/* And this is the final byte of the write scatter-gather list. */
#define VIRTIO_BLK_S_OK		0
#define VIRTIO_BLK_S_IOERR	1
#define VIRTIO_BLK_S_UNSUPP	2
#endif /* _LINUX_VIRTIO_BLK_H */