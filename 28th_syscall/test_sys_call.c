/*
*仿照glibc函数来写系统调用 
*/

#include <errno.h>
#include <unistd.h>
//#include <sysdep.h>
#define __NR_SYSCALL_BASE	0x900000

void hello(char *buf, int count)
{
	/*swi*/
	asm ("mov r0, %0\n"
		"mov r1, %1\n"
		"swi %2\n"
		:
		: "r"(buf), "r"(count), "i" (__NR_SYSCALL_BASE + 352)
		: "r0", "r1");
}

int main(int argc, char **argv)
{
	printf("in app , call hello \n");
	hello("luorongwei", 11);
	return 0;
}
