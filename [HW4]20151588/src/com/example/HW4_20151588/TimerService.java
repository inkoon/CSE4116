package com.example.HW4_20151588;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.Binder;
import java.util.Timer;
import java.util.TimerTask;

public class TimerService extends Service {
	private int counter = 0;
	private int min = 0;
    private int sec = 0;
    private Timer timer;
    private TimerTask timerTask;

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
		timer = new Timer();
		timerTask = new TimerTask(){
			public void run(){
				counter++;
				if(counter>3600) counter = 0;
				min = counter/60;
				sec = counter%60;
			}
		};
		timer.schedule(timerTask,1000,1000);
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
        if(timer != null){
        	timer.cancel();
        	timer = null;
        }
    }
}