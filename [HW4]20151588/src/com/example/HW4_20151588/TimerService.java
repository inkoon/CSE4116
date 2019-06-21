package com.example.HW4_20151588;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Binder;

public class TimerService extends Service {
	private int counter = 0;
	private int min = 0;
    private int sec = 0;

    IBinder mBinder = new TimerBinder();
    
    class TimerBinder extends Binder {
    	TimerService getService() {
            return TimerService.this;
        }
    }
    
	@Override
	public IBinder onBind(Intent intent) {
		//return mBinder;
		return mBinder;
	}
	
	public int getMin(){
		return min;
	}
	
	public int getSec(){
		return sec;
	}
	
	public void timerStart(){
		counter = 0;
	}
	
	public void timerCount(){
		counter++;
		if(counter>3600) counter = 0;
		min = counter/60;
		sec = counter%60;
	}
	
	@Override 
    public void onCreate() {
        super.onCreate();
    }
	
	public int onStartCommand(Intent intent, int flags, int startId) {
        return super.onStartCommand(intent, flags, startId);
    }
	
    @Override
    public void onDestroy() {
        super.onDestroy();
        	counter = 0;
    }
}