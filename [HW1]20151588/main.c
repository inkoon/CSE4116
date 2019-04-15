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
#define MODE 5

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
	int curmode;	// cursor mode [0]:hidden [1]:show
	unsigned char text[32];	// text value
}out_data;

void input_process();	// input process
int check_in_data(in_data,in_data);	// if the data has been changed then return 1, else return 0
void main_process();	// main process
void output_process();	// output process
void mode_init(out_data*,int,int*,int*);	// initialize the each mode status
void change_n(int*,int,out_data*);	// convert the number to the notation

unsigned char dot_mode[2][10]={	// for text editor mode
	{0x1c,0x36,0x63,0x63,0x63,0x7f,0x7f,0x63,0x63,0x63},	// A
	{0x0c,0x1c,0x1c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x1e}		// 1
};

unsigned char txt_lis[9][3]={	// for text editor mode
	".QZ","ABC","DEF","GHI","JKL","MNO","PRS","TUV","WXY"
};

unsigned char mol_lis[8] = {	// for mole game mode
	0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01
};

int in_msqid;	// message queue id 1
int out_msqid;	// message queue id 2

int main(void){
	in_msqid = msgget((key_t)1234, IPC_CREAT|0666);	// get input process message queue id
	out_msqid = msgget((key_t)2345, IPC_CREAT|0666);	// get output process message queue id
	if(in_msqid == -1 || out_msqid == -1){
		printf("msg queue id is not valid.\n");
		return -1;
	}

	pid_t pid = fork();	// fork input process
	switch(pid){
		case -1:
			printf("fork error [1]\n");
			return -1;
		case 0:
			input_process();	// input process
			break;
		default:
			pid = fork();	// fork output process
			switch(pid){
				case -1:
					printf("fork error [2]\n");
					return -1;
				case 0:
					output_process();	// output process
					break;
				default:
					main_process();	// main process
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
	int rd;

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
	
	// init data
	memset(&prev, 0, sizeof(prev));
	memset(&data, 0, sizeof(data));
	prev.mtype = 1;
	data.mtype = 1;
	msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);

	while(1){
		usleep(100000);
		// button input
		if((rd = read(dev_but, ev, but_size * BUF_SIZE)) >= but_size){	// read the buttons input
			data.but[0] = ev[0].value && (ev[0].code == 115);	// VOL+
			data.but[1] = ev[0].value && (ev[0].code == 114);	// VOL-
		}
		if(ev[0].value && (ev[0].code == 158)){	// back
			close(dev_but);
			close(dev_swi);
			data.but[2] = 1;
			// send the message to main process to exit
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			return;
		}

		// switch input
		read(dev_swi, &data.swi, swi_size);	// read the switches input
		
		// if the data comes in then send it to the main process
		if(check_in_data(prev,data)){
			msgsnd(in_msqid, &data, sizeof(in_data) - sizeof(long), IPC_NOWAIT);
			prev = data;
		}
	}
}

// if the data has been changed then return 1, else return 0
int check_in_data(in_data a, in_data b){
	int i;
	for(i = 0; i < BUT_SIZE; i++)
		if(a.but[i] != b.but[i]) return 1;
	for(i = 0; i < SWI_SIZE; i++)
		if(a.swi[i] != b.swi[i]) return 1;

	return 0;
}

// initialize the each mode status
void mode_init(out_data *data, int mode, int devtime[2], int *mol_n){
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
	else if(mode == 2) for(i=0;i<10;i++) data->dot[i] = dot_mode[0][i];
	else if(mode == 3) data->curmode = 1;
	else if(mode == 4){
		srand(time(NULL));
		*mol_n = rand()%8;
		data->led = mol_lis[*mol_n];
	}
	else if(mode == -1) data->end = 1;
}

// convert the number to the notation
void change_n(int *num, int mode, out_data *data){
	*num %= mode*mode*mode;
	data->fnd[1] = *num/(mode*mode);
	data->fnd[2] = (*num%(mode*mode))/mode;
	data->fnd[3] = *num%mode;
}

void main_process(){
	in_data in;
	out_data out;
	int mode = 0;	// default mode is 0
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
	// variables for mole game
	int mol_pnt = 0;
	int mol_n = 0;

	// init
	mode_init(&out,mode,devtime,&mol_n);
	prevtime[0] = devtime[0];
	prevtime[1] = devtime[1];
	msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);
	srand(time(NULL));

	while(1){
		// receive the message from the input process
		msgrcv(in_msqid, &in, sizeof(in_data) - sizeof(long), 0, 0);
		usleep(100000);

		if(in.but[0]){	// if push VOL+ button then change the mode
			mode = (mode + 1) % MODE;
			mode_init(&out,mode,devtime,&mol_n);
			cnt_mode = 10;
			cnt_n = 0;
			brd_cnt = 0;
			txt_cnt = 0;
			txt_mode = 0;
			txt_prev = -1;
			txt_len = -1;
			mol_pnt = 0;
			outflag = 1;
		}
		else if(in.but[1]){	// if push VOL- button then change the mode
			mode--;
			if(mode < 0) mode += MODE;
			mode_init(&out,mode,devtime,&mol_n);
			cnt_mode = 10;
			cnt_n = 0;
			brd_cnt = 0;
			txt_cnt = 0;
			txt_mode = 0;
			txt_prev = -1;
			txt_len = -1;
			mol_pnt = 0;
			outflag = 1;
		}
		
		if(in.but[2]){	// if push back button then exit
			mode_init(&out,-1,devtime,&mol_n);
			// send the message to outprocess to exit
			msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);
			usleep(400000);
			msgctl(in_msqid, IPC_RMID, 0);
			msgctl(out_msqid, IPC_RMID, 0);
			return;
		}
		
		if(mode == 0){	// mode 1 [CLOCK]
			timer = time(NULL);
			t = localtime(&timer);
			gettime[0] = t->tm_hour;
			gettime[1] = t->tm_min;
			if(in.swi[0]){	// change settime mode
				out.settime = (out.settime+1)%2;
				if(out.settime) out.led = 0x20;
				else out.led = 0x80;	// set led
				outflag = 1;
			}

			if(out.settime){	// if time set mode
				if(in.swi[1]){	// reset
					devtime[0] = gettime[0];
					devtime[1] = gettime[1];
					outflag = 1;
				}
				else if(in.swi[2]){	// plus one hour
					devtime[0] = (devtime[0]+1)%24;
					outflag = 1;
				}
				else if(in.swi[3]){	// plus one minute
					devtime[1]++;
					if(devtime[1] >= 60){
						devtime[0] = (devtime[0]+1)%24;
						devtime[1] = 0;
					}
					outflag = 1;
				}
			}

			// change the device time as time goes on
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
			
			// if data has been changed then change the output data
			if(outflag){
				out.fnd[0] = (devtime[0])/10;
				out.fnd[1] = (devtime[0])%10;
				out.fnd[2] = (devtime[1])/10;
				out.fnd[3] = (devtime[1])%10;
			}
		}
		else if(mode == 1){	// mode 2 [COUNTER]
			if(in.swi[0]){	// change the notation and set led
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
			else if(in.swi[1]){	// increase the hundreds digit
				cnt_n += cnt_mode * cnt_mode;
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
			else if(in.swi[2]){	// increase the tens digit
				cnt_n += cnt_mode;
				change_n(&cnt_n,cnt_mode,&out);
				outflag = 1;
			}
			else if(in.swi[3]){	// increase the units digit
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
					if(in.swi[i]){	// write the char or number to the text lcd
						if(txt_mode){
							if(txt_len == 31){	// number mode
								strncpy(out.text,out.text+1,31);
								txt_len--;
							}
							out.text[++txt_len] = '1'+i;
						}
						else{	// char mode
							if(txt_prev == i){	// if push same switch right before
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
				out.curmode = 1;
				outflag = 1;
			}
			else if(in.swi[1]){	// up
				if(out.cur[1] >= 1) out.cur[1]--;
				brd_cnt++;
				outflag = 1;
			}
			else if(in.swi[2]){	// cursor
				out.curmode = !out.curmode;
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
		else if(mode == 4){	// mode 5 [MOLE GAME]
			if(in.swi[8]){	// reset
				mol_pnt = 0;
				outflag = 1;
			}
			for(i=0;i<8;i++){	// catch the mole
				if(in.swi[i] && i==mol_n){
					mol_pnt += 100;
					mol_n = rand()%8;
					out.led = mol_lis[mol_n];
					outflag = 1;
				}
			}
			mol_pnt %= 10000;
			out.fnd[0] = mol_pnt/1000;
			out.fnd[1] = (mol_pnt%1000)/100;
			out.fnd[2] = (mol_pnt%100)/10;
			out.fnd[3] = mol_pnt%10;
		}

		if(outflag){	// if outout data has been changed, then send the message to the output process
			outflag = 0;
			msgsnd(out_msqid, &out, sizeof(out_data) - sizeof(long), IPC_NOWAIT);
		}
	}
}

void output_process(){
	out_data data;
	// led mmap addr
	unsigned long *fpga_addr = 0;
	unsigned char *led_addr = 0;
	// devices
	int fnd;
	int led;
	int dot;
	int lcd;
	// variables for dot matrix
	int dot_size;
	int dot_s;
	// variables for led
	int led_n;
	int led_s = 0x10;
	int led_prev = 0;
	// varibale for blinking led or cursor
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
		// receive the message from the main process
		msgrcv(out_msqid, &data, sizeof(out_data) - sizeof(long), 0, IPC_NOWAIT);
		// for count the one second
		usleep(10);
		slp_cnt++;
		
		led_n = data.led;
		// if one second passes, then blink the led or cursor
		if(slp_cnt >= 7000){
			if(data.settime){	// in mode 1 led blink
				if(led_s == 0x20) led_s = 0x10;
				else led_s = 0x20;
				led_prev = led_s;
				*led_addr = led_s;
			}
			if(data.curmode){
				data.dot[data.cur[1]] = data.dot[data.cur[1]]^(0x40>>data.cur[0]);
			}
			slp_cnt -= 7000;
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
			return;
		}
	}
}
