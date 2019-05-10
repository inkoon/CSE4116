#include <linux/kernel.h>

asmlinkage unsigned long sys_newcall(int interval, int repeat, int start) {
	/* 4 byte stream
	+------------------------------------------------------------------------+
	|  fnd start position  |  fnd start value  |  interval  |  repeatnumber  |
	+------------------------------------------------------------------------+
		1 byte for each data */
	unsigned long res = 0;
	int i;	// fnd start position
	int val;	// fnd value
	int digit = 1000;
	for(i=0;i<4;i++){
		val = start/digit;
		if(val>0) break;
		digit/=10;
	}
	
	res = ((unsigned char)i<<24)|((unsigned char)val<<16)|
		((unsigned char)interval<<8)|((unsigned char)repeat);

	return res;
}
