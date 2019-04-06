#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <time.h>

// devices name
#define MEM_DEV "/dev/mem"
#define FND_DEV "/dev/fpga_fnd"
#define DOT_DEV "/dev/fpga_dot"
#define LCD_DEV "/dev/fpga_text_lcd"
#define SWI_DEV "/dev/fpga_push_switch"
#define BUT_DEV "/dev/input/event0"

#define FPGA_BASE_ADDR 0x08000000
#define LED_ADDR 0x16
#define BUF_SIZE 64
#define BUT_SIZE 3
#define SWI_SIZE 9
#define MODE 4

// message structure that send data from input process to main process
typedef struct _in_data{
	long mtype;
	int but[BUT_SIZE];
	unsigned char swi[SWI_SIZE];
}in_data;

// message structure that send data from main process to output process
typedef struct _out_data{
	long mtype;
	int end;	// if push the back button then end the program
	unsigned char fnd[4];
	int led;
	int settime;
}out_data;

void input_process();
int check_in_data(in_data,in_data);	// If the data has been changed then return 1, else return 0.
void main_process();
void output_process();

int in_msqid;	// message queue id 1
int out_msqid;	// message queue id 2

int main(void){
	in_msqid = msgget((key_t)1234, IPC_CREAT|0666);
	out_msqid = msgget((key_t)2345, IPC_CREAT|0666);
	if(in_msqid == -1 || out_msqid == -1){
		printf("msg queue id is not valid.\n");
		return -1;
	}

	pid_t pid = fork();
	switch(pid){
		case -1:
			printf("fork error [1]\n");
			return -1;
		case 0:
			input_process();
			break;
		default:
			pid = fork();
			switch(pid){
				case -1:
					printf("fork error [2]\n");
					return -1;
				case 0:
					output_process();
					break;
				default:
					main_process();
			}
	}
	
	return 0;
}

void input_process(){
	int dev_but;
	int dev_swi;
	in_data prev, data;
	struct input_event ev[BUF_SIZE];
	int but_size = sizeof(struct input_event);
	int swi_size = sizeof(data.swi);
	int but_flag = 0;
	int rd;
	int i;

	if((dev_but = open(BUT_DEV, O_RDONLY|O_NONBLOCK)) == -1){
		printf("%s is not valid device.\n", BUT_DEV);
		close(dev_but);
		return;
	}
	if((dev_swi = open(SWI_DEV, O_RDWR|O_NONBLOCK)) == -1){
		printf("%s is not valid device.\n", SWI_DEV);
		close(dev_but);
		close(dev_swi);
		return;
	}
	
	memset(&prev, 0, sizeof(prev));
	memset(&data, 0, sizeof(data));
	prev.mtype = 1;
	data.mtype = 1;
	msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);

	while(1){
		usleep(200000);
		//printf("INP\n");
		// button input
		if((rd = read(dev_but, ev, but_size * BUF_SIZE)) >= but_size){
			data.but[0] = ev[0].value && (ev[0].code == 115);	// VOL+
			data.but[1] = ev[0].value && (ev[0].code == 114);	// VOL-
		}
		if(ev[0].value && (ev[0].code == 158)){	// back
			close(dev_but);
			close(dev_swi);
			data.but[2] = 1;
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			//printf("INDIE\n");
			return;
		}

		// switch input
		read(dev_swi, &data.swi, swi_size);

		// if the data comes in then send it to the main process
		if(check_in_data(prev,data) || but_flag){
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			but_flag = 0;
			prev = data;
		}
		if(data.but[0] || data.but[1]){
			memset(&data.but, 0, sizeof(data.but));
			but_flag = 1;
		}
		/*if((rd = read(dev_but, ev, but_size * BUF_SIZE)) >= but_size){
			data.but[0] = ev[0].code == 115;	// VOL+
			data.but[1] = ev[0].code == 114;	// VOL-
		}
		if(ev[0].value && (ev[0].code == 158)){	// back
			close(dev_but);
			close(dev_swi);
			data.but[2] = 1;
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			//printf("INDIE\n");
			return;
		}

		// switch input
		read(dev_swi, &data.swi, swi_size);
		printf("INSWI: ");
		for(i = 0; i < SWI_SIZE; i++) printf("[%d] ", data.swi[i]);
		printf("\n");

		// if the data comes in then send it to the main process
		if(check_in_data(prev,data)){
			printf("sendsensensensnensnesnss!\n");
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			prev = data;
		}*/
	}
}

int check_in_data(in_data a, in_data b){
	int i;
	for(i = 0; i < BUT_SIZE; i++)
		if(a.but[i] != a.but[i]) return 1;
	for(i = 0; i < SWI_SIZE; i++)
		if(a.swi[i] != b.but[i]) return 1;

	return 0;
}

void main_process(){
	in_data in;
	out_data out;
	int i;
	int mode = 0;	// default mode is 0
	struct tm *t;
	time_t timer = time(NULL);
	int prevtime[2];
	int gettime[2];
	int devtime[2];
	int inflag = 0;
	int outflag = 0;

	memset(&out, 0, sizeof(out));
	out.mtype = 1;

	// init dev msgsnd
	msgrcv(in_msqid, &in, sizeof(in_data) - sizeof(long), 0, IPC_NOWAIT);
	t = localtime(&timer);
	devtime[0] = t->tm_hour;
	devtime[1] = t->tm_min;
	prevtime[0] = devtime[0];
	prevtime[1] = devtime[1];
	out.fnd[0] = (devtime[0])/10;
	out.fnd[1] = (devtime[0])%10;
	out.fnd[2] = (devtime[1])/10;
	out.fnd[3] = (devtime[1])%10;
	out.led = 0x80;
	msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);

	while(1){
		inflag = msgrcv(in_msqid, &in, sizeof(in_data) - sizeof(long),
				0, IPC_NOWAIT);
		usleep(200000);

		/*//debug
		printf("after msgrcv\n");
		printf("BUT: [%d] [%d]\n", in.but[0], in.but[1]);
		printf("SWI: ");
		for(i = 0; i < SWI_SIZE; i++) printf("[%d] ", in.swi[i]);
		printf("\n\n");
		// debug*/

		if(in.but[0]){
			mode = (mode + 1) % MODE;
			// mode init
		}
		else if(in.but[1]){
			mode--;
			if(mode < 0) mode += MODE;
			// mode init
		}
		else if(in.but[2]){
			out.end = 1;
			msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);
			usleep(400000);
			msgctl(in_msqid, IPC_RMID, 0);
			msgctl(out_msqid, IPC_RMID, 0);
			//printf("MAINDIE\n");
			return;
		}
		
		//printf("MODE [%d]\n",mode);
		if(mode == 0){	// mode 1 [CLOCK]
			timer = time(NULL);
			t = localtime(&timer);
			gettime[0] = t->tm_hour;
			gettime[1] = t->tm_min;
			//printf("%d:%d | %d:%d\n",devtime[0],devtime[1],gettime[0],gettime[1]);
			if(in.swi[0]){
				out.settime = (out.settime+1)%2;
				if(out.settime) out.led = 0x20;
				else out.led = 0x80;
				outflag = 1;
			}

			if(out.settime){
				if(in.swi[1]){
					devtime[0] = gettime[0];
					devtime[1] = gettime[1];
					outflag = 1;
				}
				else if(in.swi[2]){
					devtime[0] = (devtime[0]+1)%24;
					outflag = 1;
				}
				else if(in.swi[3]){
					devtime[1]++;
					if(devtime[1] >= 60){
						devtime[0] = (devtime[0]+1)%24;
						devtime[1] = 0;
					}
					outflag = 1;
				}
			}

			if(prevtime[0] != gettime[0] || prevtime[1] != gettime[1]){
				prevtime[0] = gettime[0];
				prevtime[1] = gettime[1];
				devtime[1]++;
				if(devtime[1] >= 60){
					devtime[0] = (devtime[0]+1)%24;
					devtime[1] = 0;					
				}
				outflag = 1;
			}
			if(outflag){
				out.fnd[0] = (devtime[0])/10;
				out.fnd[1] = (devtime[0])%10;
				out.fnd[2] = (devtime[1])/10;
				out.fnd[3] = (devtime[1])%10;
			
			}
		}

		if(outflag){
			outflag = 0;
			msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);
		}
	}
}

void output_process(){
	out_data data;
	unsigned long *fpga_addr = 0;
	unsigned char *led_addr = 0;
	int fnd;
	int led;
	int led_n;
	int led_s = 0x10;
	int led_prev = 0;
	int slp_cnt=0;
	fnd = open(FND_DEV, O_RDWR);
	led = open(MEM_DEV, O_RDWR | O_SYNC);
	if(fnd < 0 || led < 0){
		printf("Device open error [%d%d]\n", fnd, led);
		return;
	}
	fpga_addr = (unsigned long*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
					MAP_SHARED, led, FPGA_BASE_ADDR);
	if(fpga_addr == MAP_FAILED){
		printf("mmap error.\n");
		return;
	}
	led_addr = (unsigned char*)((void*)fpga_addr + LED_ADDR);

	usleep(200000);
	while(1){
		msgrcv(out_msqid, &data, sizeof(out_data) - sizeof(long), 0, IPC_NOWAIT);
		usleep(100);
		slp_cnt++;

		if(data.end){
			munmap(led_addr, 4096);
			printf("OUTDIE\n");
			return;
		}
		
		write(fnd,&data.fnd,4);
		led_n = data.led;
		//printf("LED[%d]\n",led_n);
		if(data.settime){
			if(slp_cnt >= 10000){
				if(led_s == 0x20) led_s = 0x10;
				else led_s = 0x20;
				led_prev = led_s;
				*led_addr = led_s;
				slp_cnt -= 10000;
			}
		}
		else if(led_prev != led_n){
			if(led_n < 0 || 255 < led_n) printf("led range error.[%d]\n",led_n);
			else{
				led_prev = led_n;
				*led_addr = led_n;
			}
		}
	}
}
