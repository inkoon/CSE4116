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
	long mtype;	// message type
	int but[BUT_SIZE];	// buttons's signal
	unsigned char swi[SWI_SIZE];	// switches's signal
}in_data;

// message structure that send data from main process to output process
typedef struct _out_data{
	long mtype;	// message type
	int end;	// if push the back button then end the program
	unsigned char fnd[4];
	int led;	// led value
	int settime;	// in mode 1 whether set the time or not [0]:set [1]:not set
	unsigned char dot[10];
	int cur[2];	// cursor position (x,y)
	int curmode;	// cursor mode [0]:don't care [1]:show [2]:hidden
	unsigned char text[32];	// text value
}out_data;

void input_process();
int check_in_data(in_data,in_data);	// If the data has been changed then return 1, else return 0.
void main_process();
void output_process();
void mode_init(out_data*,int,int*);
void change_n(int*,int,out_data*);

unsigned char dot_mode[2][10]={
	{0x1c,0x36,0x63,0x63,0x63,0x7f,0x7f,0x63,0x63,0x63},	// A
	{0x0c,0x1c,0x1c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e}		// 1
};

unsigned char txt_lis[9][3]={".QZ","ABC","DEF","GHI","JKL","MNO","PRS","TUV","WXY"};

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

void mode_init(out_data *data, int mode, int devtime[2]){
	int i;
	struct tm *t;
	time_t timer = time(NULL);
	
	memset(data, 0, sizeof(*data));
	data->mtype = 1;
	
	if(mode == 0){
		t = localtime(&timer);
		devtime[0] = t->tm_hour;
		devtime[1] = t->tm_min;
		data->fnd[0] = (devtime[0])/10;
		data->fnd[1] = (devtime[0])%10;
		data->fnd[2] = (devtime[1])/10;
		data->fnd[3] = (devtime[1])%10;
		data->led = 0x80;
	}
	else if(mode == 1) data->led = 0x40;
	else if(mode == 2){
		for(i=0;i<10;i++) data->dot[i] = dot_mode[0][i];
	}
	else if(mode == 3) data->curmode = 1;
	else data->end = 1;
}

void change_n(int *num, int mode, out_data *data){
	if(mode == 10){
		*num %= 1000;
		data->fnd[1] = *num/100;
		data->fnd[2] = (*num%100)/10;
		data->fnd[3] = *num%10;
	}
	else if(mode == 8){
		*num %= 512;
		data->fnd[1] = *num/64;
		data->fnd[2] = (*num%64)/8;
		data->fnd[3] = *num%8;
	}
	else if(mode == 4){
		*num %= 64;
		data->fnd[1] = *num/16;
		data->fnd[2] = (*num%16)/4;
		data->fnd[3] = *num%4;
	}
	else if(mode == 2){
		*num %= 4;
		data->fnd[1] = *num/4;
		data->fnd[2] = (*num%4)/2;
		data->fnd[3] = *num%2;
	}
}

void main_process(){
	in_data in;
	out_data out;
	int mode = 0;	// default mode is 0
	int inflag = 0;
	int outflag = 0;
	int i;
	int j;
	// variables for timer
	struct tm *t;
	time_t timer = time(NULL);
	int devtime[2];
	int prevtime[2];
	int gettime[2];
	// variables for counter
	int cnt_mode = 10;
	int cnt_n = 0;
	// variables for draw board
	int brd_cnt = 0;
	// variables for text editor
	int txt_cnt = 0;
	int txt_mode = 0;
	int txt_prev = -1;
	int txt_len = -1;

	// init
	msgrcv(in_msqid, &in, sizeof(in_data) - sizeof(long), 0, IPC_NOWAIT);
	mode_init(&out,mode,devtime);
	prevtime[0] = devtime[0];
	prevtime[1] = devtime[1];
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
			mode_init(&out,mode,devtime);
			cnt_mode = 10;
			cnt_n = 0;
			brd_cnt = 0;
			txt_cnt = 0;
			txt_mode = 0;
			txt_prev = -1;
			txt_len = -1;
			outflag = 1;
		}
		else if(in.but[1]){
			mode--;
			if(mode < 0) mode += MODE;
			mode_init(&out,mode,devtime);
			cnt_mode = 10;
			cnt_n = 0;
			brd_cnt = 0;
			txt_cnt = 0;
			txt_mode = 0;
			txt_prev = -1;
			txt_len = -1;
			outflag = 1;
		}
		
		if(in.but[2]){
			mode_init(&out,-1,devtime);
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
		else if(mode == 1){	// mode 2 [COUNTER]
			if(in.swi[0]){
				if(cnt_mode == 10){
					cnt_mode = 8;
					out.led = 0x20;
				}
				else if(cnt_mode == 8){
					cnt_mode = 4;
					out.led = 0x10;
				}
				else if(cnt_mode == 4){
					cnt_mode = 2;
					out.led = 0x80;
				}
				else if(cnt_mode == 2){
					cnt_mode = 10;
					out.led = 0x40;
				}
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
			else if(in.swi[1]){
				cnt_n += cnt_mode * cnt_mode;
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
			else if(in.swi[2]){
				cnt_n += cnt_mode;
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
			else if(in.swi[3]){
				cnt_n += 1;
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
		}
		else if(mode == 2){	// mode 3 [TEXT EDITOR]
			if(in.swi[1]&&in.swi[2]){	// text editor clear
				memset(out.text,0,sizeof(out.text));
				txt_cnt += 2;
				txt_len = -1;
				outflag = 1;
			}
			else if(in.swi[4]&&in.swi[5]){	// change eng and num
				txt_mode = !txt_mode;
				for(i=0;i<10;i++) out.dot[i] = dot_mode[txt_mode][i];
				txt_cnt += 2;
				outflag = 1;
			}
			else if(in.swi[7]&&in.swi[8]){	// space
				if(txt_prev == -1){
					if(txt_len == 31){
						strncpy(out.text,out.text+1,31);
						txt_len--;
					}
					out.text[++txt_len] = ' ';
				}
				else txt_prev = -1;
				txt_cnt += 2;
				outflag = 1;
			}
			else{
				for(i=0;i<9;i++){
					if(in.swi[i]){
						if(txt_mode){
							if(txt_len == 31){
								strncpy(out.text,out.text+1,31);
								txt_len--;
							}
							out.text[++txt_len] = '1'+i;
						}
						else{
							if(txt_prev == i){
								for(j=0;j<3;j++) if(out.text[txt_len] == txt_lis[i][j]) break;
								j = (j+1)%3;
								out.text[txt_len] = txt_lis[i][j];
							}
							else{
								if(txt_len == 31){
									strncpy(out.text,out.text+1,31);
									txt_len--;
								}
								out.text[++txt_len] = txt_lis[i][0];
								txt_prev = i;
							}
						}
						txt_cnt++;
						outflag = 1;
						break;
					}
				}
			}
			txt_cnt %= 10000;
			out.fnd[0] = txt_cnt/1000;
			out.fnd[1] = (txt_cnt%1000)/100;
			out.fnd[2] = (txt_cnt%100)/10;
			out.fnd[3] = txt_cnt%10;

		}
		else if(mode == 3){	// mode 4 [DRAW BOARD]
			if(in.swi[0]){	// reset
				memset(out.dot,0,sizeof(out.dot));
				memset(out.fnd,0,sizeof(out.fnd));
				out.cur[0] = 0;
				out.cur[1] = 0;
				brd_cnt = 0;
				outflag = 1;
			}
			else if(in.swi[1]){	// up
				if(out.cur[1] >= 1) out.cur[1]--;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[2]){	// cursor
				if(out.curmode == 1) out.curmode = 2;
				else if(out.curmode == 2) out.curmode = 1;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[3]){	// left
				if(out.cur[0] >= 1) out.cur[0]--;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[4]){	// select
				out.dot[out.cur[1]] = out.dot[out.cur[1]]^(0x40>>out.cur[0]);
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[5]){	// right
				if(out.cur[0] <= 5) out.cur[0]++;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[6]){	// clear
				memset(out.dot,0,sizeof(out.dot));
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[7]){	// down
				if(out.cur[1] <= 8) out.cur[1]++;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[8]){	// reverse
				for(i = 0; i < 10; i++) out.dot[i] = out.dot[i]^0x7f;
				brd_cnt++;
				outflag = 1;
			}
			brd_cnt %= 10000;
			out.fnd[0] = brd_cnt/1000;
			out.fnd[1] = (brd_cnt%1000)/100;
			out.fnd[2] = (brd_cnt%100)/10;
			out.fnd[3] = brd_cnt%10;
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
	int dot;
	int lcd;
	int dot_size;
	int dot_s;
	int led_n;
	int led_s = 0x10;
	int led_prev = 0;
	int slp_cnt=0;
	fnd = open(FND_DEV, O_RDWR);	// fnd open
	led = open(MEM_DEV, O_RDWR | O_SYNC);	// memory device open
	dot = open(DOT_DEV, O_WRONLY);	// dot matrix open
	lcd = open(LCD_DEV, O_WRONLY);	// text lcd open
	if(fnd < 0 || led < 0 || dot < 0 || lcd < 0){
		printf("Device open error [%d%d%d%d]\n", fnd, led, dot, lcd);
		return;
	}

	// led init
	fpga_addr = (unsigned long*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
					MAP_SHARED, led, FPGA_BASE_ADDR);
	if(fpga_addr == MAP_FAILED){
		printf("mmap error.\n");
		return;
	}
	led_addr = (unsigned char*)((void*)fpga_addr + LED_ADDR);

	// dot init
	dot_size = sizeof(data.dot);

	usleep(200000);
	while(1){
		msgrcv(out_msqid, &data, sizeof(out_data) - sizeof(long), 0, IPC_NOWAIT);
		usleep(10);
		slp_cnt++;
		
		led_n = data.led;
		//printf("LED[%d]\n",led_n);
		//printf("CUR[%d,%d]\n",data.cur[0],data.cur[1]);
		if(slp_cnt >= 5000){
			if(data.settime){
				if(led_s == 0x20) led_s = 0x10;
				else led_s = 0x20;
				led_prev = led_s;
				*led_addr = led_s;
			}
			if(data.curmode){
				data.dot[data.cur[1]] = data.dot[data.cur[1]]^(0x40>>data.cur[0]);
			}
			slp_cnt -= 5000;
		}
		
		if(led_prev != led_n && !data.settime){	// write led by mmap
			if(led_n < 0 || 255 < led_n) printf("led range error.[%d]\n",led_n);
			else{
				led_prev = led_n;
				*led_addr = led_n;
			}
		}
		
		write(fnd,&data.fnd,4);	// write fnd
		write(dot,&data.dot,dot_size);	// write dot matrix
		write(lcd,data.text,32);	// write text lcd

		if(data.end){
			close(fnd);
			close(dot);
			close(lcd);
			munmap(led_addr, 4096);
			printf("OUTDIE\n");
			return;
		}
	}
}
