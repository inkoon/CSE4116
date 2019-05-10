#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>

#define DRIVER "/dev/dev_driver"
#define MAJOR_NUM 242
#define IOCTL_WCMD _IOW(MAJOR_NUM,0,char *)

int main(int argc, char **argv){
	int num[3];
	unsigned long data;
	int dev;

	if(argc!=4){
		printf("Input 3 argument\n");
		return -1;
	}
	num[0] = atoi(argv[1]);
	num[1] = atoi(argv[2]);
	num[2] = atoi(argv[3]);

	data = syscall(376,num[0],num[1],num[2]);
	dev = open(DRIVER,O_RDWR);
	if(dev < 0){
		printf("Device open error\n");
		exit(1);
	}

	write(dev,(const char *)data,4);
	ioctl(dev,IOCTL_WCMD,data);

	close(dev);

	return 0;
}
