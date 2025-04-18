/*
Copyright (c) 2012, Broadcom Europe Ltd.
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
     * Neither the name of the copyright holder nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "mailbox.h"
#include <stdint.h>  // Include the header for intptr_t
#define PAGE_SIZE (4*1024)

void *mapmem(unsigned base, unsigned size)
{
    int mem_fd;
    unsigned offset = base % PAGE_SIZE;
    base = base - offset;
    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem\nThis program should be run as root. Try prefixing command with: sudo\n");
        exit (-1);
    }
    void *mem = mmap(
        0,
        size,
        PROT_READ|PROT_WRITE,
        MAP_SHARED/*|MAP_FIXED*/,
        mem_fd,
        base);
#ifdef DEBUG
    printf("base=0x%x, mem=%p\n", base, mem);
#endif
    if (mem == MAP_FAILED) {
        printf("mmap error %ld\n", (intptr_t)mem); // This is the fixed line
        exit (-1);
    }
    close(mem_fd);
    return (char *)mem + offset;
}

void *unmapmem(void *addr, unsigned size)
{
    int s = munmap(addr, size);
    if (s != 0) {
        printf("munmap error %d\n", s);
        exit (-1);
    }

    return NULL;
}

/*
 * use ioctl to send mbox property message
 */

static int mbox_property(int file_desc, void *buf)
{
    int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

    if (ret_val < 0) {
        // something wrong somewhere, send some details to stderr
        perror("ioctl_set_msg failed");
    }

#ifdef DEBUG
    unsigned *p = buf; int i; unsigned size = *(unsigned *)buf;
    for (i=0; i<size/4; i++)
        printf("%04x: 0x%08x\n", i*sizeof *p, p[i]);
#endif
    return ret_val;
}

unsigned mem_alloc(int file_desc, unsigned size, unsigned align, unsigned flags)
{
    int i=0;
    unsigned p[32];
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x3000c; // (the tag id)
    p[i++] = 12; // (size of the buffer)
    p[i++] = 12; // (size of the data)
    p[i++] = size; // (num bytes? or pages?)
    p[i++] = align; // (alignment)
    p[i++] = flags; // (MEM_FLAG_L1_NONALLOCATING)

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("mem_alloc: mbox_property() error, abort!\n");
        exit (-1);
    }
    return p[5];
}

unsigned mem_free(int file_desc, unsigned handle)
{
    int i=0;
    unsigned p[32];
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x3000f; // (the tag id)
    p[i++] = 4; // (size of the buffer)
    p[i++] = 4; // (size of the data)
    p[i++] = handle;

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("mem_free: mbox_property() error, ignoring\n");
        return 0;
    }
    return p[5];
}

unsigned mem_lock(int file_desc, unsigned handle)
{
    int i=0;
    unsigned p[32];
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x3000d; // (the tag id)
    p[i++] = 4; // (size of the buffer)
    p[i++] = 4; // (size of the data)
    p[i++] = handle;

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("mem_lock: mbox_property() error, abort!\n");
        exit (-1);
    }
    return p[5];
}

unsigned mem_unlock(int file_desc, unsigned handle)
{
    int i=0;
    unsigned p[32];
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x3000e; // (the tag id)
    p[i++] = 4; // (size of the buffer)
    p[i++] = 4; // (size of the data)
    p[i++] = handle;

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("mem_unlock: mbox_property() error, ignoring\n");
        return 0;
    }
    return p[5];
}

unsigned execute_code(int file_desc, unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5)
{
    int i=0;
    unsigned p[32];
    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x30010; // (the tag id)
    p[i++] = 28; // (size of the buffer)
    p[i++] = 28; // (size of the data)
    p[i++] = code;
    p[i++] = r0;
    p[i++] = r1;
    p[i++] = r2;
    p[i++] = r3;
    p[i++] = r4;
    p[i++] = r5;

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("execute_code: mbox_property() error, ignoring\n");
        return 0;
    }
    return p[5];
}

unsigned qpu_enable(int file_desc, unsigned enable)
{
    int i=0;
    unsigned p[32];

    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request

    p[i++] = 0x30012; // (the tag id)
    p[i++] = 4; // (size of the buffer)
    p[i++] = 4; // (size of the data)
    p[i++] = enable;

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("qpu_enable: mbox_property() error, ignoring\n");
        return 0;
    }
    return p[5];
}

unsigned execute_qpu(int file_desc, unsigned num_qpus, unsigned control, unsigned noflush, unsigned timeout) {
    int i=0;
    unsigned p[32];

    p[i++] = 0; // size
    p[i++] = 0x00000000; // process request
    p[i++] = 0x30011; // (the tag id)
    p[i++] = 16; // (size of the buffer)
    p[i++] = 16; // (size of the data)
    p[i++] = num_qpus;
    p[i++] = control;
    p[i++] = noflush;
    p[i++] = timeout; // ms

    p[i++] = 0x00000000; // end tag
    p[0] = i*sizeof *p; // actual size

    if(mbox_property(file_desc, p) < 0) {
        printf("execute_qpu: mbox_property() error, ignoring\n");
        return 0;
    }
    return p[5];
}

int mbox_open() {
    int file_desc;

    // Open a char device file used for communicating with kernel mbox driver.
  
    // try to use the device node in /dev first (created by kernels 4.1+)
    file_desc = open(DEVICE_FILE_NAME, 0);
    if(file_desc >= 0) {
        //printf("Using mbox device " DEVICE_FILE_NAME ".\n");
        return file_desc;
    }
    
    // Try to create one
    unlink(LOCAL_DEVICE_FILE_NAME);
    if(mknod(LOCAL_DEVICE_FILE_NAME, S_IFCHR|0600, makedev(MAJOR_NUM_A, 0)) >= 0 &&
        (file_desc = open(LOCAL_DEVICE_FILE_NAME, 0)) >= 0) {
        printf("Using local mbox device file with major %d.\n", MAJOR_NUM_A);
        return file_desc;
    }

    unlink(LOCAL_DEVICE_FILE_NAME);
    if(mknod(LOCAL_DEVICE_FILE_NAME, S_IFCHR|0600, makedev(MAJOR_NUM_B, 0)) >= 0 &&
        (file_desc = open(LOCAL_DEVICE_FILE_NAME, 0)) >= 0) {
        printf("Using local mbox device file with major %d.\n", MAJOR_NUM_B);
        return file_desc;
    }

    printf("Unable to open / create kernel mbox device file, abort!\n");
    exit (-1);
}

void mbox_close(int file_desc) {
    close(file_desc);
}
