#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <linux/ioctl.h>
#include "char_multi_dev.h"
#define BUF_SIZE 716800

unsigned int size;

int dialog(int hDevice, void* inputBuff, void* outputBuff, int inputLen, int outputLen)
{
    if (write(hDevice, inputBuff, inputLen) < 0)
    {
        printf("write to device error...\n");
        return -1;
    }

    if (read(hDevice, outputBuff, outputLen) < 0)
    {
        printf("read from device error...\n");
        return -1;
    }

    return 0;

}

int aes_ecb_128_enc_test()
{
    int hDevice, fin, fout;
    unsigned char head[] = { 0x01,/*MOD_ECB*/
                           0x00,/*ENC*/
        /*0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c/*key*/ };
    uint8_t buffer[BUF_SIZE] = { 0 };
    unsigned char in[BUF_SIZE+50] = { 0 };
    memcpy(in, head, sizeof(head));

    if ((hDevice = open("/dev/edcard", O_RDWR)) < 0)
    {
        perror("fail to open device...\n");
        return -1;
    }

    if ((fin = open("plain", O_RDWR)) < 0)
    {
        perror("fail to open file...\n");
    }

    if ((fout = open("cipher", O_RDWR | O_CREAT)) < 0)
    {
        perror("fail too open file...\n");
    }

    unsigned char data[BUF_SIZE] = { 0 };
    int readLen;
    in[3]=0;
    memcpy(in+4,&size,4);
    while ((readLen = read(fin, data, BUF_SIZE)) > 0)
    {
        unsigned char blk_num=(unsigned char)readLen%7168==0?readLen/7168:readLen/7168+1;
        in[2]=blk_num;
        memcpy(in + sizeof(head)+6, data, readLen);
        dialog(hDevice, in, buffer, sizeof(head)+6 + readLen, BUF_SIZE);
        write(fout, buffer, BUF_SIZE);
        in[3]=1;
        memset(data, 0, BUF_SIZE);
    }
    //printf("Please Enter to continue---\n");
    //sleep(3);
    in[3]=2;
    dialog(hDevice,in,buffer,sizeof(head)+6,BUF_SIZE);
    close(fin);
    close(fout);
    close(hDevice);

    return 0;
}

int aes_ecb_128_dec_test()
{
    int hDevice, fin, fout;
    unsigned char head[] = { 0x01,/*MOD_ECB*/
                           0x01,/*DEC*/
        /*0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c/*key*/ };
    uint8_t buffer[BUF_SIZE] = { 0 };
    unsigned char* in;
    in = (unsigned char*)malloc(sizeof(head) + BUF_SIZE);
    memcpy(in, head, sizeof(head));

    if ((hDevice = open("/dev/edcard", O_RDWR)) < 0)
    {
        perror("fail to open device...\n");
        return -1;
    }

    if ((fin = open("cipher", O_RDWR)) < 0)
    {
        perror("fail to open file...\n");
    }

    if ((fout = open("result", O_RDWR | O_CREAT)) < 0)
    {
        perror("fail too open file...\n");
    }

    unsigned char data[BUF_SIZE] = { 0 };
    int readLen;
    in[3]=0;
    memcpy(in+4,&size,4);
    while ((readLen = read(fin, data, BUF_SIZE)) > 0)
    {
        unsigned char blk_num=(unsigned char)readLen%7168==0?readLen/7168:readLen/7168+1;
        in[2]=blk_num;
        memcpy(in + sizeof(head)+6, data, readLen);
        dialog(hDevice, in, buffer, sizeof(head)+6 + readLen, BUF_SIZE);
        write(fout, buffer, BUF_SIZE);
        in[3]=1;
        memset(data, 0, BUF_SIZE);
    }
    in[3]=2;
    dialog(hDevice,in,buffer,sizeof(head)+6,BUF_SIZE);
    close(fin);
    close(fout);
    close(hDevice);

    return 0;
}

int main(void)
{
    unsigned char key[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c };
    int hDevice = open("/dev/edcard", O_RDWR);
    ioctl(hDevice, KEY_IMPORT, key);
    close(hDevice);



    int fd = open("plain", O_RDWR);
    size = lseek(fd, 0, SEEK_END);
    //aes_ecb_128_enc_test();

    clock_t start = clock();
    int i;
    for (i = 0; i < 1; i++)
    {
        aes_ecb_128_enc_test();
    }

    clock_t end = clock();
    float enc_total = (end - start);
    printf("encrypt total time:%fs\nencrypt speed:%fM/s\n", enc_total*1.0/1000000, size * 1.0 * 1000000 / 1024 / 1024 / enc_total);

    //aes_ecb_128_dec_test();

    return 0;
}
