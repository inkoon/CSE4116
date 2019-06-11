package com.example.HW4_20151588;

import com.example.HW4_20151588.R;
import com.example.HW4_20151588.TimerService.TimerBinder;

import java.util.Random;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.ComponentName;
import android.graphics.Color;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Binder;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity2 extends Activity{
	// components
	LinearLayout linear, content;
	EditText data;
	TextView tv;
	Button btn;
	Button btns[];
	LinearLayout lns[];
	
	// service object
	TimerService ts;
	boolean isService = false;
	
	// global variables
	int row,col,size;
	boolean click_flag = true;
	
	Handler handler = new Handler();
	
	private class TimerThread extends Thread{
		private int m,s;
		
		public TimerThread(){
			this.m = 0;
			this.s = 0;
		}
		
		@Override
		public void run(){
			runOnUiThread(new Runnable(){
				@Override
				public void run(){
					while(s<25){
						m = ts.getMin();
						s = ts.getSec();
						tv.setText(""+m+":"+s);
					}
				}
			});
		}
	}
	
	// "make buttons" OnClickListener
	OnClickListener ltn = new OnClickListener(){
		public void onClick(View v){
			if(click_flag){
				// get "row col"
				String temp=data.getText().toString();
				String t[] = temp.split(" ",2);
				row = Integer.parseInt(t[0]);
				col = Integer.parseInt(t[1]);
				// check the size
				if(row>5||col>5){
					tv.setText("row&col should be less than 5");
					return;
				}
				click_flag = false;		
				int num[] = new int[row*col];
				shuffle(num,row,col);
				make_button(num,row,col);
				
				if(isService){
					ts.timerStart();
					/*TimerThread tt = new TimerThread();
					tt.start();*/
					new TimerThread().start();
					/*new Thread(new Runnable(){
						public void run(){
							handler.post(new Runnable(){
								int m,s=0;
								public void run(){
									while(s<10){
										m = ts.getMin();
										s = ts.getSec();
										tv.setText(""+m+":"+s);
									}
								}
							});
						}
					}).start();*/
				}
			}
		}
	};
	
	// "puzzle buttons" OnClickListener
	OnClickListener b_ltn = new OnClickListener(){
		public void onClick(View v){
			int i = v.getId();
			if((i%col)!=0){	// check left
				if(btns[i-1].getText().toString().equals("")){
					remove_button();
					swap_button(i,i-1);
					set_button();
					check_puzzle();
				}
			}
			if((i%col)!=col-1){	// check right
				if(btns[i+1].getText().toString().equals("")){
					remove_button();
					swap_button(i,i+1);
					set_button();
					check_puzzle();
				}
			}
			if((i/col)!=0){	// check up
				if(btns[i-col].getText().toString().equals("")){
					remove_button();
					swap_button(i,i-col);
					set_button();
					check_puzzle();
				}
			}
			if((i/col)!=row-1){	// check down
				if(btns[i+col].getText().toString().equals("")){
					remove_button();
					swap_button(i,i+col);
					set_button();
					check_puzzle();
				}
			}
		}
	};
	
	// shuffle the number 0 ~ row*col
	public void shuffle(int num[], int row, int col){
		int i,j,dir,tmp;
		// using random seed
		Random random = new Random();
		for(i=0;i<row*col-1;i++) num[i]=i+1;
		num[row*col-1] = 0;
		for(i=0;i<10000;i++){
			dir = random.nextInt(4);
			for(j=0;num[j]!=0;j++);
			//tv.setText(""+j);
			switch(dir){
			case 0:	// left
				if(j%col != 0){
					tmp = num[j];
					num[j] = num[j-1];
					num[j-1] = tmp;
				}
				break;
			case 1:	// right
				if(j%col != col-1){
					tmp = num[j];
					num[j] = num[j+1];
					num[j+1] = tmp;
				}
				break;
			case 2:	// up
				if(j/col != 0){
					tmp = num[j];
					num[j] = num[j-col];
					num[j-col] = tmp;
				}
				break;
			case 3:	// down
				if(j/col != row-1){
					tmp = num[j];
					num[j] = num[j+col];
					num[j+col] = tmp;
				}
				break;
			}
		}
	}
	
	// set all buttons
	public void set_button(){
		int i,j;
		for(i=0;i<row;i++){
			for(j=0;j<col;j++){
				lns[i].addView(btns[i*col+j]); 
			}
		}
	}
	
	// swap two buttons
	public void swap_button(int a, int b){
		btns[b].setText(btns[a].getText());
		btns[b].setBackgroundResource(android.R.drawable.btn_default);
		btns[a].setText("");
		btns[a].setBackgroundColor(Color.BLACK);
	}
	
	// remove all buttons
	public void remove_button(){
		int i,j;
		for(i=0;i<row;i++){
			for(j=0;j<col;j++){
				lns[i].removeView(btns[i*col+j]);
			}
		}
	}
	
	// check whether puzzle is solved or not
	public void check_puzzle(){
		int i;
		boolean pass = true;
		for(i=0;i<row*col-1;i++){
			if(!btns[i].getText().equals(Integer.toString(i+1))){
				pass = false;
				break;
			}
		}
		if(pass){
			click_flag = true;
			unbindService(conn);
			finish();
		}
	}
	
	// make row*col buttons
	public void make_button(int num[], int row, int col){
		int i,j;
		btns = new Button[row*col];
		lns = new LinearLayout[row];
		for(i=0;i<row*col;i++) btns[i] = new Button(this);
		for(i=0;i<row;i++){
			lns[i] = new LinearLayout(this);
			lns[i].setOrientation(LinearLayout.HORIZONTAL);
			content.addView(lns[i]);
		}
		
		for(i=0;i<row*col;i++){
			// if num[i] then it will be blank button
			if(num[i] == 0){
				btns[i].setText("");
				btns[i].setBackgroundColor(Color.BLACK);
			}
			else btns[i].setText(""+num[i]);
			btns[i].setId(i);
			btns[i].setOnClickListener(b_ltn);
			btns[i].setWidth(1024/col);
			btns[i].setHeight(360/row);
		}		
		set_button();
	}
	
	ServiceConnection conn = new ServiceConnection(){
		public void onServiceConnected(ComponentName name,IBinder service){
			TimerBinder mb = (TimerBinder)service;
			ts = mb.getService();
			isService = true;
		}
		public void onServiceDisconnected(ComponentName name){
			isService = false;
		}
	};
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main2);
		linear = (LinearLayout)findViewById(R.id.container);
		content=(LinearLayout)findViewById(R.id.content);
		data = (EditText)findViewById(R.id.editText1);
		tv=(TextView)findViewById(R.id.textView1);
		tv.setTextSize(30);
		tv.setTextColor(Color.WHITE);
		tv.setBackgroundColor(Color.BLACK);
		btn=(Button)findViewById(R.id.button1);
		btn.setOnClickListener(ltn);

		Intent intent = new Intent(MainActivity2.this,TimerService.class);
		bindService(intent,conn,Context.BIND_AUTO_CREATE);
	}
}
