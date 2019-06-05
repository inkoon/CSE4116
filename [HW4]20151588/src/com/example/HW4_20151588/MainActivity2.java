package com.example.HW4_20151588;

import com.example.HW4_20151588.R;

import java.util.Random;
import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity2 extends Activity{
	
	LinearLayout linear;
	EditText data;
	TextView tv;
	Button btn;
	OnClickListener ltn;
	Button btns[];
	LinearLayout linears[];
	
	int row,col,size;
	boolean click_flag = true;
	
	public void shuffle(int num[], int size){
		int i;
		int a,b,tmp;
		Random random = new Random();
		for(i=0;i<size;i++) num[i] = i;
		for(i=0;i<size;i++){
			a = random.nextInt(size);
			b = random.nextInt(size);
			tmp = num[a];
			num[a] = num[b];
			num[b] = tmp;
		}
	}
	public void make_button(int num[], int row, int col){
		int i;
		btns = new Button[row*col];
		//todo
	}
	
		@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main2);
		linear = (LinearLayout)findViewById(R.id.container);
		data = (EditText)findViewById(R.id.editText1);
		tv=(TextView)findViewById(R.id.textView1);
		Button btn=(Button)findViewById(R.id.button1);
		
		ltn=new OnClickListener(){
			public void onClick(View v){
				if(click_flag){
					String temp=data.getText().toString();
					String t[] = temp.split(" ",2);
					row = Integer.parseInt(t[0]);
					col = Integer.parseInt(t[1]);
					if(row>5||col>5){
						tv.setText("row&col should be less than 5");
						return;
					}
					click_flag = false;
					size = row*col;
					int num[] = new int[size];
					
					shuffle(num,size);
					make_button(num,row,col);
					
					/*********/
					temp = Integer.toString(num[0]);
					for(int i = 1; i < size; i++){
						temp = temp+" "+Integer.toString(num[i]);
					}
					tv.setText(temp);
					/**********/
					
				}
	
			}
		};
		btn.setOnClickListener(ltn);
		
	}

}
