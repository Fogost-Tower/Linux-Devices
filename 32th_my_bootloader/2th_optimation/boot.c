#include "setup.h"

extern void uart_init(void);
extern void nand_read(unsigned int addr, unsigned char *buf, unsigned int len);
extern void puts(char *str);
extern void puthex(unsigned int val);

static struct tag *params;

int strlen(char *str)
{
	int i =0;
	while(str[i]){
		i++;
	}
	return i;
}

void strcpy(char *dest,char *src)
{
	while((*dest ++ = *src++) != '\0');
}

void setup_start_teg(void)
{
	params = (struct tag  *)0x30000100;

	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size(tag_core);

	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;

	params = tag_next(params);
}

void setup_memory_tags()
{
	params->hdr.tag = ATAG_MEM;
	params->hdr.size = tag_size(tag_mem32);

	params->u.mem.start = 0x30000000;
	params->u.mem.size = 64*1024*1024;

	params = tag_next(params);
}

void setup_commandline_tags(char *cmdline)
{
	int len = strlen(cmdline)+1;
	
	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size = (sizeof (struct tag_header) +len+3 )>>2;

	strcpy (params->u.cmdline.cmdline, cmdline);

	params = tag_next(params);
}

void setup_end_tag()
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}


int main(void)
{
	void (*theKernel) (int zero, int arch, unsigned int params);
	/*0.帮内核设置串口,设置串口*/
	uart_init();
	
	/*1.从nand FLash 里把内核拷贝到内存*/
	nand_read(0x60000+64, (unsigned char *)0x30008000, 0x200000);

	/*2.设置参数*/
	setup_start_teg();
	setup_memory_tags();
	setup_commandline_tags("noinitrd root=/dev/nfs nfsroot=192.168.1.110:/home/book/student/nfs_root/first_fs ip=192.168.1.11:192.168.1.110:192.168.1.1:255.255.255.0::eth0:off init=/linuxrc console=ttySAC0");
	setup_end_tag();
	
	/*3.跳转启动内核*/
	theKernel = (void (*)(int, int, unsigned int))0x30008000;
	theKernel(0, 362, 0x30000100); 
	
	/*
	*mov r0, #0
	*ldr r1, =362
	*ldr r2, =0x30000100
	*mov pc, #0x30008000
	*/

	/*如果一切正常不会执行到这里*/
	return -1;
	
}
