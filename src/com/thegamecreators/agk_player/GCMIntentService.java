
package com.thegamecreators.agk_player;

import android.app.NativeActivity;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
//import com.thegamecreators.agk_player.*;

import com.google.android.gcm.GCMBaseIntentService;

public class GCMIntentService extends GCMBaseIntentService {

    @Override
    public void onRegistered(Context context, String regId)
    {
        AGKHelper.PNRegID = regId;
    }
    
    @Override
    public void onUnregistered(Context context, String regId)
    {
    	
    }
    
    @Override
    public void onMessage(Context context, Intent intent)
    {
    	String message = intent.getStringExtra("message");
    	String title = intent.getStringExtra("title");
    	    	
    	NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE); 
    	
    	//Intent intent2 = new Intent(this, NotificationReceiver.class);
    	//PendingIntent pIntent = PendingIntent.getActivity(this, 0, intent2, 0);
    	
    	Intent intent2 = new Intent(this, NativeActivity.class);
    	PendingIntent pIntent = PendingIntent.getActivity(this, 0, intent2, 0);
    	
    	Notification notif = new Notification(
                R.drawable.icon, 
                message,
                System.currentTimeMillis());
     
        CharSequence from = title;
        notif.setLatestEventInfo(this, from, message, pIntent);

    	// Hide the notification after its selected
    	notif.flags |= Notification.FLAG_AUTO_CANCEL;
    	notificationManager.notify(0, notif);
    }
    
    @Override
    public void onError(Context context, String errorId)
    {
    	AGKHelper.PNRegID = "Error";
    }
}
