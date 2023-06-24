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
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/iov.h"
#include "qemu/error-report.h"
#include "hw/block/block.h"
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"
#include "hw/virtio/virtio-pcie.h"
#include "block/scsi.h"
#include <sys/ioctl.h>

#ifdef __linux__
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<time.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<errno.h>
#include<stdlib.h>
#include<signal.h>
#include<scsi/sg.h>
#include<sys/file.h>
#include<sys/syscall.h>
#endif
#include "hw/virtio/virtio-bus.h"
#include "hw/virtio/virtio-access.h"
#include "edcard-config/edcard-config.h"
#include "edcard-config/char_multi_dev.h"

#define DEVICE_NUM 1
extern int pv_id;
extern int pv_shared_id;
extern int buf_id;
extern int hDevice;
union semun
{
    	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
typedef struct _req_blk
{
    int pid;
    unsigned int start;
    unsigned int size;
    double proprity;
}req_blk;
union semun resources_edcard={DEVICE_NUM,NULL,NULL,NULL};
union semun shared_buf={1,NULL,NULL,NULL};
extern int flag_v;//判断密码卡资源有没有进行资源回收
extern int flag_shared_buf;//判断共享缓冲区的锁有没有释放
extern int flag_shared_v;

static void virtio_pcie_init_request(VirtIOPCIE *s, VirtQueue *vq, VirtIOPCIEReq *req)
{
    req->dev = s;
    req->vq = vq;
    req->qiov.size = 0;
    req->in_len = 0;
    req->next = NULL;
    req->mr_next = NULL;
    req->buff = NULL;//在VirtIOPCIEReq中添加这一项，用于指向数据缓冲区

    return ;
}

static void virtio_pcie_free_request(VirtIOPCIEReq *req)
{
    if (req){
        g_free(req);
     }
    
     return ;
}

static void virtio_pcie_req_complete(VirtIOPCIEReq *req, unsigned char status)
{
    
    VirtIOPCIE *s = req->dev;
    VirtIODevice *vdev = VIRTIO_DEVICE(s);

    stb_p(&req->in->status, status);
    virtqueue_push(req->vq, &req->elem, req->in_len);
    virtio_notify(vdev, req->vq);
    return ;
}

static int virtio_pcie_handle_rw_error(VirtIOPCIEReq *req, int error, bool is_read)
{
     /*暂时不需要任何处理*/
    return 0;
}

static void virtio_pcie_rw_complete(void *opaque, int ret)
{
    VirtIOPCIEReq *next = opaque;
    
    while (next) {
        VirtIOPCIEReq *req = next;
        next = req->mr_next;
       
        if (req->qiov.nalloc != -1) {
            /* If nalloc is != 1 req->qiov is a local copy of the original
             * external iovec. It was allocated in submit_merged_requests
             * to be able to merge requests. */
            qemu_iovec_destroy(&req->qiov);
        }

        if (ret) {
            int p = virtio_ldl_p(VIRTIO_DEVICE(req->dev), &req->out.type);
            bool is_read = !(p & VIRTIO_PCIE_T_OUT);
            /* Note that memory may be dirtied on read failure.  If the
             * virtio request is not completed here, as is the case for
             * BLOCK_ERROR_ACTION_STOP, the memory may not be copied
             * correctly during live migration.  While this is ugly,
             * it is acceptable because the device is free to write to
             * the memory until the request is completed (which will
             * happen on the other side of the migration).
             */
            if (virtio_pcie_handle_rw_error(req, -ret, is_read)) {
                continue;
            }
        }

        virtio_pcie_req_complete(req, VIRTIO_BLK_S_OK);
        virtio_pcie_free_request(req);
    }
    
    return ;
}

/*处理加解密请求函数*/
int get_proprity(int wait_time, int size)//bytes
{
    int w;
    if(wait_time<1)
    {
        w=0;
    }
    else
    {
        w=wait_time/10+1;
    }
    size=size/(1024*1024);
    if(size<=400)
    {
        return 3+w;
    }
    else if(size<=800)
    {
        return 2+w;
    }
    else if(size<=1200)
    {
        return 1+w;
    }
    else
    {
        return 0+w;
    }
}
int get_head_pid(req_blk* req_queue, int num)
{
    int i,res=0;
    for(i=1;i<num;i++)
    {
        if(req_queue[res].proprity<req_queue[i].proprity)
        {
            res=i;
        }
        else if(req_queue[res].proprity==req_queue[i].proprity)
        {
            if(req_queue[res].start>req_queue[i].start)
            {
                res=i;
            }
        }
    }
    return res;
}
static void virtio_pcie_handle_write(VirtIOPCIEReq *req, MultiReqBuff *mrb)
{
    
    req_blk req_queue[100];//等待队列
	unsigned char buffer[726800] = {0};
	int per_len=7168;//每次密码卡能处理的数据大小
	int length = req->out.real_len;//前端传过来的数据总长
    long start=time(NULL);
	unsigned char per_blk[7170];//最终向密码卡传递的数据：head+data
    //printf("here ok 0.1\n");
	per_blk[0]=(req->buff)[0];//head
	per_blk[1]=(req->buff)[1];//head
	int num=(int)(req->buff)[2];      //需要对密码卡请求的次数
	int eof_flag=(int)(req->buff)[3]; //0:首次请求，1:中间请求，2:最后请求
    unsigned int total_size;
    memcpy(&total_size,req->buff+4,4);//请求加解密的数据总量
	int offset_data=8; //第一个数据的位置
    //printf("here ok 1\n");
	//设备锁
	key_t pv_key=ftok("/tmp/",21);
	if(pv_key==-1)
	{
		printf("pv ftok error\n");
	}
	pv_id=semget(pv_key,1,(IPC_CREAT|IPC_EXCL)|0666);
	if(pv_id==-1)
	{
		if(errno==EEXIST)
		{
		    pv_id=semget(pv_key,1,IPC_CREAT|0666);
		}
		else 
		{
		    exit(-1);
		}
	}
	else 
	{
		if(semctl(pv_id,0,SETVAL,resources_edcard)==-1)
		{
		    printf("semctl error\n");
		}
	}
    //共享缓冲区锁
    key_t pv_shared_key=ftok("/tmp/",22);
	if(pv_shared_key==-1)
	{
		printf("pv ftok error\n");
	}
	pv_shared_id=semget(pv_shared_key,1,(IPC_CREAT|IPC_EXCL)|0666);
	if(pv_shared_id==-1)
	{
		if(errno==EEXIST)
		{
		    pv_shared_id=semget(pv_shared_key,1,IPC_CREAT|0666);
		}
		else 
		{
		    exit(-1);
		}
	}
	else 
	{
		if(semctl(pv_shared_id,0,SETVAL,shared_buf)==-1)
		{
		    printf("semctl error\n");
		}
	}
    //printf("here ok 3\n");
    //寻找空闲设备
	if(eof_flag==0)
	{
	printf("thread_id:%ld\n",syscall(SYS_gettid));
        key_t shared_key=ftok("/tmp/",23);
        char *shmaddr=NULL;
        buf_id=shmget(shared_key,1604,(IPC_CREAT|IPC_EXCL)|0666);
        if(buf_id==-1)
        {
            if(errno==EEXIST)
            {
                printf("memory exist\n");
                buf_id=shmget(shared_key,1604,IPC_CREAT|0666);
            }
            else 
            {
                exit(-1);
            }
        }
        else
        {
            flag_shared_v=0;
            P(pv_shared_id,0);
            shmaddr=(char*)shmat(buf_id,NULL,SHM_W);
            memset(shmaddr,0,1604);
            V(pv_shared_id,0);
            flag_shared_v=1;
        }
        //printf("here ok 4\n");
        flag_shared_v=0;
        P(pv_shared_id,0);
        shmaddr=(char*)shmat(buf_id,NULL,SHM_R);
        int wait_num;//等待的请求数目
        memcpy(&wait_num,shmaddr,4);
        printf("wait num(1): %d\n",wait_num);
        memcpy(req_queue,shmaddr+4,sizeof(req_blk)*wait_num);
        req_queue[wait_num].pid=getpid();
        req_queue[wait_num].start=start;
        req_queue[wait_num].size=total_size;
        req_queue[wait_num].proprity=0;//这里不用排序
        //printf("start: %u\n",req_queue[wait_num].start);
        //写回
        wait_num++;
        shmaddr=(char*)shmat(buf_id,NULL,SHM_W);
        memcpy(shmaddr,&wait_num,4);
        memcpy(shmaddr+4,req_queue,sizeof(req_blk)*wait_num);
        V(pv_shared_id,0);
        flag_shared_v=1;
        //printf("here ok 5\n");
        int condition=0;
        while(!condition)
		{
            flag_v=0;
            P(pv_id,0);
            flag_shared_v=0;
            P(pv_shared_id,0);
            shmaddr=(char*)shmat(buf_id,NULL,SHM_R);
            int wait_num;//等待的请求数目
            memcpy(&wait_num,shmaddr,4);
            //printf("wait num(2): %d\n",wait_num);
            memcpy(req_queue,shmaddr+4,sizeof(req_blk)*wait_num);
            int i;
            unsigned int now_time=time(NULL);
            for(i=0;i<wait_num;i++)
            {
                //req_queue[i].proprity=get_proprity(now_time-req_queue[i].start,req_queue[i].size);
		double nd_time=(req_queue[i].size/(1024*1024*40));
		req_queue[i].proprity=(now_time-req_queue[i].start)/(nd_time+50.0)+1;
                printf("pid:%d,wait_time:%d,nd_time:%lf,proprity:%lf\n",req_queue[i].pid,now_time-req_queue[i].start,nd_time,req_queue[i].proprity);
            }
            int res=get_head_pid(req_queue,wait_num);
            printf("res_max:%d\n",res);
            if(req_queue[res].pid==getpid())
            {
                //寻找空闲设备
                for(i=0;i<6;i++)
                {
                    hDevice = open(edcard_name[i], O_RDWR);
                    if(hDevice < 0)
                    {
                        error_report("open the device error.\n");
                    }
                    int err=flock(hDevice,LOCK_EX|LOCK_NB);
                    if(err<0)
                    {
                        close(hDevice);
                        hDevice=-1;
                    }
                    else
                    {
                        //printf("here ok 6\n");
                        printf("open device:%s, start encrypt/decrypt\n", edcard_name[i]);
                        condition=1;
                        //删除自己的请求
                        shmaddr=(char*)shmat(buf_id,NULL,SHM_W);
                        req_blk tmp[100];
                        //TO DO
                        memcpy(tmp,(unsigned char*)req_queue+(res+1)*sizeof(req_blk),(wait_num-res-1)*sizeof(req_blk));
                        
                        memcpy((unsigned char*)req_queue+res*sizeof(req_blk),tmp,(wait_num-res-1)*sizeof(req_blk));
                        wait_num--;
                        memcpy(shmaddr,&wait_num,4);
                        memcpy(shmaddr+4,req_queue,sizeof(req_blk)*wait_num);
                        V(pv_shared_id,0);
                        flag_shared_v=1;
                        break;
                    }
                }
            }
            else
            {
                printf("pid didn't match\n");
                V(pv_id,0);
                flag_v=1;
                V(pv_shared_id,0);
                flag_shared_v=1;
            }
        }
	    ioctl(hDevice,KEY_IMPORT,req->buff+offset_data);//导入密钥
	}
	if(eof_flag==1)
	{
		int offset=8,j;
		for(j=0;j<num;j++)
		{
			if(j==num-1)
			{
				per_len=(length-offset_data)%per_len==0?7168:(length-offset_data)%per_len;
			}
			memcpy(per_blk+2,req->buff+offset,per_len);
            //printf("write[0]:%x,[1]:%x\n",per_blk[2],per_blk[3]);
			int writeLen = write(hDevice, per_blk, per_len+2);
			if(writeLen != per_len+2)
			{
				error_report("write device error.\n");
				return;
			}
			
			int readLen = read(hDevice, buffer+offset-offset_data, 7168);
			if(readLen < 0)
			{
				error_report("read from device error.\n");
				return;
			}
			offset+=per_len;
		}
		memcpy(req->buff, buffer, length-offset_data);
		//printf("read[0]:%x,[1]:%x\n",(unsigned char)(req->buff)[0],(unsigned char)(req->buff)[1]);
	}

	if(eof_flag==2)
	{
        printf("end encrypt/decrypt\n");
		close(hDevice);
        hDevice=-1;
		V(pv_id,0);
		flag_v=1;
	}
	virtio_pcie_rw_complete(req,0);
	return ;
}


#ifdef __linux__

typedef struct {
    VirtIOPCIEReq *req;
    struct sg_io_hdr hdr;
} VirtIOPCIEIoctlReq;

#endif

static VirtIOPCIEReq *virtio_pcie_get_request(VirtIOPCIE *s, VirtQueue *vq)
{
    VirtIOPCIEReq *req = virtqueue_pop(vq, sizeof(VirtIOPCIEReq));

    if (req) {
        virtio_pcie_init_request(s, vq, req);
    }
    return req;
}

static int virtio_pcie_handle_request(VirtIOPCIEReq *req, MultiReqBuff *mrb)
{
    uint32_t type;
    struct iovec *in_iov = req->elem.in_sg;
    struct iovec *iov = req->elem.out_sg;
    unsigned in_num = req->elem.in_num;
    unsigned out_num = req->elem.out_num;
    VirtIOPCIE *s = req->dev;
    VirtIODevice *vdev = VIRTIO_DEVICE(s);

    if (req->elem.out_num < 1 || req->elem.in_num < 1) {
        virtio_error(vdev, "virtio-pcie missing headers");
        return -1;
    }
    iov_to_buf(iov, out_num, 0, &req->out,sizeof(req->out));
    
    /*error_report("out.real_len:%d",req->out.real_len);
    error_report("out.type:%d",req->out.type); 
    error_report("out.ioprio:%d",req->out.ioprio);
    error_report("out.sector:%d",req->out.sector);*/
    
    req->buff = (char *)(req->elem.out_sg[1].iov_base);//获取请求数据
    iov_discard_front(&iov, &out_num, sizeof(req->out));

    if (in_iov[in_num - 1].iov_len < sizeof(struct virtio_pcie_inhdr)) {
        virtio_error(vdev, "virtio-pcie request inhdr too short");
        //return -1;
    }

    /* We always touch the last byte, so just see how big in_iov is.  */
    req->in_len = iov_size(in_iov, in_num);
    req->in = (void *)in_iov[in_num - 1].iov_base
              + in_iov[in_num - 1].iov_len
              - sizeof(struct virtio_pcie_inhdr);
    
    //error_report("in.status:%c",req->in->status);
	
    iov_discard_back(in_iov, &in_num, sizeof(struct virtio_pcie_inhdr));

    type = virtio_ldl_p(VIRTIO_DEVICE(req->dev), &req->out.type);

    /* VIRTIO_PCIE_T_OUT defines the command direction. VIRTIO_PCIE_T_BARRIER
     * is an optional flag. Although a guest should not send this flag if
     * not negotiated we ignored it in the past. So keep ignoring it. */
    switch (type & ~(VIRTIO_PCIE_T_OUT | VIRTIO_PCIE_T_BARRIER)) {
    case VIRTIO_PCIE_T_IN:
    {
        bool is_write = type & VIRTIO_PCIE_T_OUT;
        req->sector_num = virtio_ldq_p(VIRTIO_DEVICE(req->dev),
                                       &req->out.sector);

        if (is_write) {
            qemu_iovec_init_external(&req->qiov, iov, out_num);
           virtio_pcie_handle_write(req, mrb);
        } else {
            qemu_iovec_init_external(&req->qiov, in_iov, in_num);
        }

        mrb->is_write = is_write;
        break;
    }

    default:
        virtio_pcie_req_complete(req, VIRTIO_BLK_S_UNSUPP);
        virtio_pcie_free_request(req);
    }
    return 0;
}

static bool virtio_pcie_handle_vq(VirtIOPCIE *s, VirtQueue *vq)
{
    VirtIOPCIEReq *req;
    MultiReqBuff mrb = {};
    bool progress = false;

    //error_report("%s is working...\n",__func__);

    do {
        virtio_queue_set_notification(vq, 0);

        while ((req = virtio_pcie_get_request(s, vq))) {//循环获取请求
            progress = true;
            if (virtio_pcie_handle_request(req, &mrb)) {//请求处理函数
                virtqueue_detach_element(req->vq, &req->elem, 0);
                virtio_pcie_free_request(req);
                break;
            }
        }

        virtio_queue_set_notification(vq, 1);
    } while (!virtio_queue_empty(vq));

    return progress;
}

static void virtio_pcie_handle_output_do(VirtIOPCIE *s, VirtQueue *vq)
{
    virtio_pcie_handle_vq(s, vq);
}

static void virtio_pcie_handle_output(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOPCIE *s = (VirtIOPCIE *)vdev;
    virtio_pcie_handle_output_do(s, vq);
}

static void virtio_pcie_dma_restart_cb(void *opaque, int running, RunState state)
{
    VirtIOPCIE *s = opaque;
    
    //error_report("%s is working 1 ...\n",__func__);
    if (!running) {
        return;
    }

    if (!s->bh) {
		error_report("%s is working ...2\n",__func__);
		error_report("%s is working ...3\n",__func__);
		error_report("%s is working ...4\n",__func__);
    }
}

static void virtio_pcie_reset(VirtIODevice *vdev)
{
    //VirtIOPCIE *s = VIRTIO_PCIE(vdev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, vdev, "virtio-pcie-device");
    VirtIOPCIEReq *req;

    /* We drop queued requests after blk_drain() because blk_drain() itself can
     * produce them. */
    while (s->rq) {
        req = s->rq;
        s->rq = req->next;
        virtqueue_detach_element(req->vq, &req->elem, 0);
        virtio_pcie_free_request(req);
    }
    
    return ;
}

/* coalesce internal state, copy to pci i/o region 0
 */
static void virtio_pcie_update_config(VirtIODevice *vdev, uint8_t *config)
{
    //VirtIOPCIE *s = VIRTIO_PCIE(vdev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, vdev, "virtio-pcie-device");
    struct virtio_pcie_config blkcfg;
    uint64_t capacity = 0;
    
    memset(&blkcfg, 0, sizeof(blkcfg));
    virtio_stq_p(vdev, &blkcfg.capacity, capacity);
    virtio_stl_p(vdev, &blkcfg.bufflength, 8192);
    virtio_stl_p(vdev, &blkcfg.flag, s->conf.flag);
    virtio_stl_p(vdev, &blkcfg.seg_max, 128 - 2);

    memcpy(config, &blkcfg, sizeof(struct virtio_pcie_config));
    //error_report("nothing to update...\n");
     
     return ;
}

static void virtio_pcie_set_config(VirtIODevice *vdev, const uint8_t *config)
{
    struct virtio_pcie_config blkcfg;

    memcpy(&blkcfg, config, sizeof(blkcfg));

    //error_report("%s:i need nothing to set...\n",__func__);

    return ;
}

//virtio_blk_get_feature反映在前端表现在virtio_has_feature,即后端在初始化前端的vdev->features
static uint64_t virtio_pcie_get_features(VirtIODevice *vdev, uint64_t features, Error **errp)
{
  
	virtio_add_feature(&features, VIRTIO_PCIE_F_CAPACITY);
	virtio_add_feature(&features, VIRTIO_PCIE_F_BUFFLENGTH);
	virtio_add_feature(&features, VIRTIO_PCIE_F_FLAG);
	virtio_add_feature(&features, VIRTIO_PCIE_F_SEG_MAX);

    	return features;
}

static void virtio_pcie_set_status(VirtIODevice *vdev, uint8_t status)
{
     /*暂时不做任何处理*/
    //error_report("%s:set nothing...\n",__func__);
    
    return ;
}

static void virtio_pcie_device_realize(DeviceState *dev, Error **errp)
{
    //error_report("%s:is working....\n", __func__);
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    //VirtIOPCIE *s = VIRTIO_PCIE(dev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, dev, "virtio-pcie-device");

	/*s->device = (char*)malloc(1024*sizeof(char));
	sprintf(s->device, "%s", local_device_list[0].device);*/
	    
    virtio_init(vdev, "virtio-pcie", 21,sizeof(struct virtio_pcie_config));//初始化VirtIODevice结构体
    s->rq = NULL;
    virtio_add_queue(vdev, 128, virtio_pcie_handle_output);//初始化vq结构体
    s->change = qemu_add_vm_change_state_handler(virtio_pcie_dma_restart_cb, s);
  
    return ;
}

static void virtio_pcie_device_unrealize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    //VirtIOPCIE *s = VIRTIO_PCIE(dev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, dev, "virtio-pcie-device");

    qemu_del_vm_change_state_handler(s->change);
    virtio_cleanup(vdev);
    
    return ;
}

//迁移状态保存函数
static void virtio_pcie_save_device(VirtIODevice *vdev, QEMUFile *f)
{
    unsigned char buff[33];
    memset(buff,0,33);
    //VirtIOPCIE *s = VIRTIO_PCIE(dev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, vdev, "virtio-pcie-device");
    if(hDevice>0)
    {
        buff[0]=1;
        ioctl(hDevice, MIGRATE_STATE_SAVE, buff+1);//获取当前加密卡状态
        ioctl(hDevice, RESET);//将加密卡reset
        close(hDevice);
    }
    else
    {
        buff[0]=0;
    }
    //清除同步时使用的锁和内存
    if(pv_id!=-1)
    {
        int val;
        if(flag_v!=1)
        {
            V(pv_id,0);
            flag_v=1;
        }	
        val=semctl(pv_id,0,GETVAL,NULL);
        printf("[0]:val:%d\n",val);
        if(val==DEVICE_NUM)
        {
            printf("clear pv_id, buf_id\n");
            semctl(pv_id,0,IPC_RMID,NULL);
            shmctl(buf_id,IPC_RMID,NULL);
            pv_id=-1;
            semctl(pv_shared_id,0,IPC_RMID,NULL);
            pv_shared_id=-1;
        }

    }
    if(pv_shared_id!=-1)
    {
        if(flag_shared_v!=1)
        {
            V(pv_shared_id,0);
            flag_shared_v=1;
        }
    }

    printf("\nKEY: ");
    int i;
    for(i=0;i<33;i++)
    {
        printf("%x ",buff[i]);
    }
    printf("\n\n");

    qemu_put_buffer(f, buff, 33);//保存加密卡状态
    printf("MIGRATE_STATE_SAVE success\n");
	return ;
}

//迁移状态加载函数
static int virtio_pcie_load_device(VirtIODevice *vdev, QEMUFile *f, int version_id)
{

    unsigned char buff[32];
    //VirtIOPCIE *s = VIRTIO_PCIE(dev);
    VirtIOPCIE *s = OBJECT_CHECK(VirtIOPCIE, vdev, "virtio-pcie-device");

    qemu_get_buffer(f, buff, 33);//获取状态
    //查看是否成功加载密钥
    printf("\nKEY: ");
    int i;
    for(i=0;i<33;i++)
    {
        printf("%x ",buff[i]);
    }
    printf("\n\n");

    if(buff[0]==1)
    {
        key_t pv_key=ftok("/tmp/",21);
        if(pv_key==-1)
        {
            printf("pv ftok error\n");
        }
        pv_id=semget(pv_key,1,(IPC_CREAT|IPC_EXCL)|0666);
        if(pv_id==-1)
        {
            if(errno==EEXIST)
            {
                pv_id=semget(pv_key,1,IPC_CREAT|0666);
            }
            else 
            {
                exit(-1);
            }
        }
        else 
        {
            if(semctl(pv_id,0,SETVAL,resources_edcard)==-1)
            {
                printf("semctl error\n");
            }
        }
        int condition=0;
        while(!condition)
		{
            flag_v=0;
            P(pv_id,0);
            //寻找空闲设备
            for(i=0;i<6;i++)
            {
                hDevice = open(edcard_name[i], O_RDWR);
                if(hDevice < 0)
                {
                    error_report("open the device error.\n");
                }
                int err=flock(hDevice,LOCK_EX|LOCK_NB);
                if(err<0)
                {
                    close(hDevice);
                    hDevice=-1;
                }
                else
                {
                    printf("open device:%s, restart encrypt/decrypt\n", edcard_name[i]);
                    condition=1;
                    break;
                }
            }
        }
        ioctl(hDevice, MIGRATE_STATE_LOAD, buff+1);//将状态加载到新的加密卡
        printf("MIGRATE_STATE_LOAD success\n");
    }
    /*hDevice = open(edcard_name[0], O_RDWR);
    ioctl(hDevice, MIGRATE_STATE_LOAD, buff+1);*/
	return 0;
}

static const VMStateDescription vmstate_virtio_pcie = {
    .name = "virtio-pcie",
    .minimum_version_id = 2,
    .version_id = 2,
    .fields = (VMStateField[]) {
        VMSTATE_VIRTIO_DEVICE,
        VMSTATE_END_OF_LIST()
    },
};

static Property virtio_blk_properties[] = {
    DEFINE_PROP_UINT32("flags", VirtIOPCIE, conf.flag, 2),
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_pcie_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);

    dc->props = virtio_blk_properties;
    dc->vmsd = &vmstate_virtio_pcie;//no need
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
    vdc->realize = virtio_pcie_device_realize;
    vdc->unrealize = virtio_pcie_device_unrealize;
    vdc->get_config = virtio_pcie_update_config;
    vdc->set_config = virtio_pcie_set_config;
    vdc->get_features = virtio_pcie_get_features;
    vdc->set_status = virtio_pcie_set_status;
    vdc->reset = virtio_pcie_reset;
    vdc->save = virtio_pcie_save_device;
    vdc->load = virtio_pcie_load_device;

    return ;
}

static void virtio_pcie_instance_init(Object *obj)
{

}

static const TypeInfo virtio_pcie_info = {
    .name = "virtio-pcie-device",
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOPCIE),
    .instance_init = virtio_pcie_instance_init,
    .class_init = virtio_pcie_class_init,
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_pcie_info);

    return ;
}

type_init(virtio_register_types)
