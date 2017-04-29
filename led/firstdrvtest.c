#include <stdio.h>
#include <string.h>

void printformat(char ch)
{
	printf("Used :\n");
	printf("format : %s /dev/xxx <on | off>",ch);
}

int main(int argc,char *argv[])
{
	int fd;
	int val=1;
	if(argc!=3)
	{
		printfmat(argv[0]);
		return ;
	}
	fd = open(argv[1],O_RDWR);
	if(fd < 0 )
	{
		printf("cat't open!\n");
	}
	if(strcmp(argv[2],"on") == 0)
	{
		val = 1;	
	}
	else
 	{
		val = 0;
	}
		
	write(fd,&val,4);
	close(fd);
}
