// Temporary until the NDK build system can deal with there being no Java source.
package com.thegamecreators.agk_player;

//import com.google.ads.*;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.app.PendingIntent;
import android.content.Intent;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Runnable;
import java.lang.String;
import java.text.SimpleDateFormat;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;

import android.util.Log;
import android.provider.MediaStore;
import android.provider.Settings.Secure;
import android.os.Bundle;
import android.os.Looper;
import android.os.Message;

import com.thegamecreators.agk_player.iap.*;

import com.facebook.*;
import com.facebook.android.DialogError;
import com.facebook.android.Facebook;
import com.facebook.android.Facebook.DialogListener;
import com.facebook.android.FacebookError;
import com.google.ads.*;
import com.google.android.gcm.GCMRegistrar;

import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.graphics.PixelFormat;
import android.view.Gravity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.view.ViewGroup;
import android.widget.VideoView;
import android.widget.MediaController;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.content.DialogInterface;

// Used for agk::Message()
class RunnableMessage implements Runnable
{
	public Activity act;
	public String msg;
	
	public void run() {
        //Toast.makeText(act, msg, Toast.LENGTH_LONG).show();
		AlertDialog alertDialog;
		alertDialog = new AlertDialog.Builder(act).create();
		alertDialog.setTitle("Message");
		alertDialog.setMessage(msg);
		alertDialog.setButton( DialogInterface.BUTTON_POSITIVE, "OK", new DialogInterface.OnClickListener(){public void onClick(DialogInterface dialog, int which) {}});
		alertDialog.show();
    }
}

// Used for agk::CreateAdvert()
class RunnableAd implements Runnable
{
	public Activity act;
	public int action = 0;
	public int horz = 1;
	public int vert = 2;
	public int offsetX = 0;
	public int offsetY = 0;
	public String pubID = "";
	
	public static AdView ad = null;
	
	public WindowManager.LayoutParams makeLayout()
	{
		WindowManager.LayoutParams ll_lp;
		
		//Just a sample layout parameters.
		ll_lp = new WindowManager.LayoutParams();
		ll_lp.format = PixelFormat.TRANSPARENT;
		ll_lp.height = -2;
		ll_lp.width = -2; 
		ll_lp.gravity = 0;
		ll_lp.x = offsetX;
		ll_lp.y = offsetY;
		switch( horz )
		{
			case 0: ll_lp.gravity |= Gravity.LEFT; break;
			case 1: ll_lp.gravity |= Gravity.CENTER_HORIZONTAL; break;
			case 2: ll_lp.gravity |= Gravity.RIGHT; break;
			default: ll_lp.gravity |= Gravity.CENTER_HORIZONTAL; break;
		}
		switch( vert )
		{
			case 0: ll_lp.gravity |= Gravity.TOP; break;
			case 1: ll_lp.gravity |= Gravity.CENTER_VERTICAL; break;
			case 2: ll_lp.gravity |= Gravity.BOTTOM; break;
			default: ll_lp.gravity |= Gravity.BOTTOM; break;
		}
		ll_lp.type = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;
		ll_lp.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
		ll_lp.flags = ll_lp.flags | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
		
		return ll_lp;
	}
	
	public void run() 
	{
		switch ( action )
		{
			case 1: // make ad
			{
				if ( ad == null )
				{
					ad = new AdView(act, AdSize.BANNER, pubID);
					AdRequest request = new AdRequest();
			        //request.addTestDevice("8EEA49684422A6B864134230E0A5511E");
			        //request.addTestDevice("B64D549379981B01B7A5CEC68AC80EEE");
					ad.loadAd( request );
					
					WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
					WindowManager.LayoutParams layout = makeLayout();
					wm.addView(ad, layout);
				}
				break;
			}
			
			case 2: // position ad
			{
				if ( ad == null ) return;
			
				WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
				WindowManager.LayoutParams layout = makeLayout();
				wm.updateViewLayout(ad, layout);
				
				break;
			}
			
			case 3: // delete the ad
			{
				if ( ad == null ) return;
			
				WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
				wm.removeView(ad);
				ad = null;
				break;
			}
			
			case 4: // refresh the ad
			{
				if ( ad == null ) return;
			
				AdRequest request = new AdRequest();
		        request.addTestDevice("8EEA49684422A6B864134230E0A5511E");
		        request.addTestDevice("B64D549379981B01B7A5CEC68AC80EEE");
				ad.loadAd( request );
				break;
			}
			
			case 5: // hide ad
			{
				if ( ad == null ) return;
				ad.setVisibility(View.GONE);
				break;
			}
			
			case 6: // show ad
			{
				if ( ad == null ) return;
				ad.setVisibility(View.VISIBLE);
				break;
			}

		}
    }
}

class AGKSurfaceView extends SurfaceView implements SurfaceHolder.Callback, MediaPlayer.OnCompletionListener
{
	public MediaPlayer player = null;
	public Activity act;
	public SurfaceHolder pHolder = null;
	public int m_x = -10;
	public int m_y = 0;
	public int m_width = 1;
	public int m_height = 1;
	public String m_filename = "";
	public int m_filetype = 0;
	
	public int prepared = 0;
	public int isPlaying = 0;
	public int pausePos = -1;
	
	public int isDisplayed = 0;
		
	public static WindowManager.LayoutParams makeLayout(int x, int y, int width, int height)
	{
		WindowManager.LayoutParams ll_lp;
		
		//Just a sample layout parameters.
		ll_lp = new WindowManager.LayoutParams();
		ll_lp.format = PixelFormat.OPAQUE;
		ll_lp.height = height;
		ll_lp.width = width; 
		ll_lp.gravity = Gravity.LEFT | Gravity.TOP;
		ll_lp.x = x;
		ll_lp.y = y;
		ll_lp.token = null;
		//ll_lp.gravity |= Gravity.CENTER_HORIZONTAL;
		//ll_lp.gravity |= Gravity.CENTER_VERTICAL;
		ll_lp.type = WindowManager.LayoutParams.TYPE_APPLICATION;
		ll_lp.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
		ll_lp.flags = ll_lp.flags | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
		ll_lp.flags = ll_lp.flags | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
		
		return ll_lp;
	}
	
	public AGKSurfaceView(Activity context) 
	{
		super(context);
		
		getHolder().addCallback(this);
		getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
		act = context;
	}
	
	public void LoadVideo( String filename, int type )
	{
		Log.e("Video","Load Video");
		
		m_filename = filename;
		m_filetype = type;
		
		if ( player != null )
		{
			StopVideo();
			player.reset();
			player.release();
			player = null;
		}
		
		MediaPlayer newplayer = new MediaPlayer();
		newplayer.setOnCompletionListener(this);
		
		try
		{
			if ( type == 0 )
			{
				AssetFileDescriptor afd = act.getAssets().openFd(filename);
				newplayer.setDataSource(afd.getFileDescriptor(),afd.getStartOffset(), afd.getLength());
			}
			else
			{
				newplayer.setDataSource(filename); 
			}
			
			// prepare will crash the media player (but not the app) on some devices if pHolder is null!
			
		}
		catch(Exception e)
		{
			Log.e("Exception", e.toString());
		}
		
		prepared = 0;
		player = newplayer;
	}
	
	public void DeleteVideo()
	{
		Log.e("Video","Delete Video");
		
		m_filename = "";
		m_filetype = 0;
		
		if ( player != null )
		{
			player.reset();
			player.release();
			player = null;
		}
		isPlaying = 0;
	}
	
	public void PlayVideo()
	{
		Log.e("Video","Play Video");
		
		AGKHelper.hasStartedVideo = 1;
		
		if ( isDisplayed == 0 )
		{
			WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
			WindowManager.LayoutParams layout = makeLayout(m_x,m_y,m_width,m_height);
			//wm.updateViewLayout(this, layout);
			wm.addView(this, layout);
			isDisplayed = 1;
		}
		
		if ( pHolder != null && isPlaying == 0 && player != null ) 
		{		
			try { if ( prepared == 0 ) { player.prepare(); prepared = 1; } } catch( Exception e ) { Log.e("Exception", e.toString()); }
			if ( pausePos >= 0 ) player.seekTo(pausePos);
			player.start();
			isPlaying = 1;
		}
	}
	
	public void PauseVideo()
	{
		Log.e("Video","Pause Video");
		
		if ( player == null ) return;
		if ( isPlaying == 0 ) return;
		isPlaying = 0;
		
		pausePos = player.getCurrentPosition();
		player.pause();
	}
	
	public void StopVideo()
	{
		Log.e("Video","Stop Video");
		
		pausePos = 0;
		AGKHelper.hasStartedVideo = 0;
		if ( isPlaying == 1 && player != null ) 
		{
			player.stop();
			prepared = 0;
			isPlaying = 0;
		}
		
		// hide the surface without destroying it
		if ( isDisplayed == 1 && pHolder != null )
		{
			WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
			WindowManager.LayoutParams layout = makeLayout(-10,0,1,1);
			//wm.updateViewLayout(this, layout);
			wm.removeView(this);
			isDisplayed = 0;
		}
	}
	
	public void SetDimensions( int x, int y, int width, int height )
	{
		Log.e("Video","Set Dimensions");
		
		m_x = x;
		m_y = y;
		m_width = width;
		m_height = height;
		
		if ( AGKHelper.hasStartedVideo == 1 && isPlaying == 0 )
		{
			PlayVideo();
		}
		
		if ( isDisplayed == 1 && pHolder != null )
		{
			WindowManager wm = (WindowManager) act.getSystemService(Context.WINDOW_SERVICE);
			WindowManager.LayoutParams layout = makeLayout(m_x,m_y,m_width,m_height);
			wm.updateViewLayout(this, layout);
		}
	}
		
	public void surfaceCreated(SurfaceHolder holder)
	{
		Log.e("Video","surface created");
		pHolder = holder;
		if ( player != null ) 
		{
			player.setDisplay(pHolder);
			prepared = 0;
		}
		else if ( isPlaying == 1 && m_filename != "" )
		{
			isPlaying = 0;
			LoadVideo(m_filename, m_filetype);
			player.setDisplay(pHolder);
		}
		
		if ( AGKHelper.hasStartedVideo == 1 ) 
		{
			//PlayVideo();
			RunnableVideo video = new RunnableVideo();
			video.act = act;
			video.action = 3;
			act.runOnUiThread(video);
		}
	}
	
	public void surfaceDestroyed(SurfaceHolder holder)
	{
		Log.e("Video","surface destroyed");
		pHolder = null;
		
		//PauseVideo();
		if ( player != null )
		{
			pausePos = player.getCurrentPosition();
			player.reset();
			player.release();
			player = null;
		}
	}
	
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		Log.e("Video","Surface changed");
	}
	
	public void onCompletion(MediaPlayer mp)
	{
		Log.e("Video","Completed");
		StopVideo();
	}
}

class RunnableVideo implements Runnable
{
	public Activity act;
	public static AGKSurfaceView video = null;
	public int action = 0;
	
	public String filename = "";
	public int fileType = 0;
	public SurfaceHolder pHolder = null;
	public int m_x,m_y,m_width,m_height;

	public void run() 
	{
		if ( video == null ) video = new AGKSurfaceView(act);
		
		switch( action )
		{
			case 1: // Load video
			{
				video.LoadVideo(filename, fileType);
				break;
			}
			case 2: // set dimensions
			{
				video.SetDimensions(m_x, m_y, m_width, m_height);
				break;
			}
			case 3: // play
			{
				video.PlayVideo();
				break;
			}
			case 4: // stop
			{
				video.StopVideo();
				break;
			}
			case 5: // pause
			{
				video.PauseVideo();
				break;
			}
			case 6: // delete
			{
				video.StopVideo();
				//video = null;
				break;
			}
		}
	}
}

class RunnableFacebook implements Runnable
{
	String szID;
	String szLink;
	String szPicture;
	String szName;
	String szCaption;
	String szDescription;
	Activity act;
	
	public void run()
	{
		Facebook feed = new Facebook(AGKHelper.appID);
		Bundle parameters = new Bundle();
	    
		if ( szID != "" ) parameters.putString("to", szID);
		if ( szLink != "" ) parameters.putString("link", szLink);
	    if ( szPicture != "" ) parameters.putString("picture",szPicture);
	    if ( szName != "" ) parameters.putString("name",szName);
	    if ( szCaption != "" ) parameters.putString("caption",szCaption);
	    if ( szDescription != "" ) parameters.putString("description", szDescription);
	    
		feed.dialog(act, "feed", parameters,new DialogListener() {
	        public void onFacebookError(FacebookError arg0) { }
	        public void onError(DialogError arg0) { } 
	        public void onComplete(Bundle arg0) { }
	        public void onCancel() { }
	    });
	}
}

// Entry point for all AGK Helper calls
public class AGKHelper {
	
	public static int hasStartedVideo = 0;
	
	public static void LoadVideo( Activity act, String filename, int type )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 1;
		video.filename = filename;
		video.fileType = type;
		act.runOnUiThread(video);
	}
	
	public static void SetVideoDimensions( Activity act, int x, int y, int width, int height )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 2;
		video.m_x = x;
		video.m_y = y;
		video.m_width = width;
		video.m_height = height;
		act.runOnUiThread(video);
	}
	
	public static void PlayVideo( Activity act )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 3;
		act.runOnUiThread(video);
		
		hasStartedVideo = 1;
	}
	
	public static void PauseVideo( Activity act )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 5;
		act.runOnUiThread(video);
	}
	
	public static void StopVideo( Activity act )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 4;
		act.runOnUiThread(video);
	}
	
	public static void DeleteVideo( Activity act )
	{
		RunnableVideo video = new RunnableVideo();
		video.act = act;
		video.action = 6;
		act.runOnUiThread(video);
	}
	
	public static int GetVideoPlaying( Activity act )
	{
		return hasStartedVideo;
	}
	
	public static float GetVideoValue( Activity act, int value )
	{
		if ( RunnableVideo.video == null || RunnableVideo.video.player == null ) return -1;
		if ( RunnableVideo.video.prepared == 0 )
		{
			if ( hasStartedVideo == 1 ) return 0;
			else return -1;
		}
		
		switch(value)
		{
			case 1: return RunnableVideo.video.player.getCurrentPosition()/1000.0f; // video position
			case 2: return RunnableVideo.video.player.getDuration()/1000.0f; // video duration
			case 3: return RunnableVideo.video.player.getVideoWidth(); // video width
			case 4: return RunnableVideo.video.player.getVideoHeight(); // video height
		}
		
		return 0;
	}
	
	public static void SetVideoVolume( float volume )
	{
		// do nothing for now
	}
	
	public static void ShowMessage( Activity act, String msg )
	{
		RunnableMessage run = new RunnableMessage();
		run.act = act;
		run.msg = msg;
		act.runOnUiThread( run );
	}
	
	public static void CreateAd(Activity act, String publisherID, int horz, int vert, int offsetX, int offsetY)
	{
		RunnableAd run = new RunnableAd();
		run.pubID = publisherID;
		run.horz = horz;
		run.vert = vert;
		run.offsetX = offsetX;
		run.offsetY = offsetY;
		run.action = 1;
		run.act = act;
		act.runOnUiThread( run );
	}
	
	public static void PositionAd(Activity act, int horz, int vert, int offsetX, int offsetY)
	{
		RunnableAd run = new RunnableAd();
		run.horz = horz;
		run.vert = vert;
		run.offsetX = offsetX;
		run.offsetY = offsetY;
		run.action = 2;
		run.act = act;
		act.runOnUiThread( run );
	}
	
	public static void DeleteAd(Activity act)
	{
		RunnableAd run = new RunnableAd();
		run.action = 3;
		run.act = act;
		act.runOnUiThread( run );
	}
	
	public static void RefreshAd(Activity act)
	{
		RunnableAd run = new RunnableAd();
		run.action = 4;
		run.act = act;
		act.runOnUiThread( run );
	}
	
	public static void SetAdVisible(Activity act, int visible)
	{
		RunnableAd run = new RunnableAd();
		run.action = visible>0 ? 6 : 5;
		run.act = act;
		act.runOnUiThread( run );
	}
	
	public static void SetOrientation( Activity act, int orien )
	{
		act.setRequestedOrientation( orien );
	}
	
	public static int GetOrientation( Activity act )
	{
		return act.getWindowManager().getDefaultDisplay().getRotation();
	}
	
	public static String GetDeviceID(Activity nativeactivityptr)
	{
		// This ID will remain constant for this device until a factory reset is performed
		String uuid = Secure.getString(nativeactivityptr.getContentResolver(), Secure.ANDROID_ID);		
		return uuid;
	}
	
	// ********************
	// In App Purchase
	// ********************
	
	public static final int MAX_PRODUCTS = 15;
	public static int g_iPurchaseState = 1;
	public static int g_iNumProducts = 0; 
	public static int[] g_iPurchaseProductStates = new int[MAX_PRODUCTS];
	public static String[] g_sPurchaseProductNames = new String[MAX_PRODUCTS];
	public static int[] g_iPurchaseProductTypes = new int[MAX_PRODUCTS];
	public static final String base64EncodedPublicKey = "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAo7Q61/t5YFduAYmKjR/T8KIROI9jN8l2kscWZy39zrziI5IkHnvklcQNah0B0ijlTeyJXka4q9HkAeoyL4LPHGX7Xk9cHEp0voCBqR6S9XjpzX49m6jeqRRQA+UB4cXTLmb4BNt1lQXctWuuAlKcdkjNEiNYHWtHc6qG6RW4EqRlx8eU+5Gx7gTieqZqUv5FZGnywz4iHXomcpIUasFm6r5Cf5/UNvZlRC4YqWj6IY42fMEzmpSuI3W1tT6Ks+yRaJWdkyMPDEd7idM7Mh0/2si6JsoNiTZt2C6MpBaRT21qQzLROK9wCvd6FeHhBn1Odgo4fouE5aAKiSIZgXWbCQIDAQAB";
	public static IabHelper mHelper;
	public static int g_iIAPID = -1;
	public static Activity g_pAct = null;
	
	static IabHelper.QueryInventoryFinishedListener mGotInventoryListener = new IabHelper.QueryInventoryFinishedListener() {
        public void onQueryInventoryFinished(IabResult result, Inventory inventory) {
            Log.d("IAB InventoryFinished", "Query inventory finished.");
            if (result.isFailure()) {
                Log.e("IAB InventoryFinished","Failed to query inventory: " + result);
                return;
            }

            Log.d("IAB InventoryFinished", "Query inventory was successful.");
            
            for( int i = 0; i < g_iNumProducts; i++ )
            {
	            Purchase purchased = inventory.getPurchase(g_sPurchaseProductNames[i]);
	            if (purchased != null) 
	            {
	            	// is it consumable?
	            	if ( g_iPurchaseProductTypes[i] == 1 ) mHelper.consumeAsync(inventory.getPurchase(g_sPurchaseProductNames[i]), mConsumeFinishedListener);
	            	else 
	            	{
	            		g_iPurchaseProductStates[i] = 1;
	            		Log.d("IAB InventoryFinished", "Purchase successful: " + g_sPurchaseProductNames[i]);
	            	}
	            }
            }
        }
    };
    
    static IabHelper.OnIabPurchaseFinishedListener mPurchaseFinishedListener = new IabHelper.OnIabPurchaseFinishedListener() {
        public void onIabPurchaseFinished(IabResult result, Purchase purchase) {
            Log.d("IAB PurchaseFinished", "Purchase finished: " + result + ", purchase: " + purchase);
            if (result.isFailure()) {
                Log.e("IAB PurchaseFinished","Error purchasing: " + result);
                if ( result.getMessage().contains("User cancelled") == false ) AGKHelper.ShowMessage(g_pAct, "Purchase Result: " + result.getMessage());
                g_iPurchaseState = 1;
                return;
            }
            
            for( int i = 0; i < g_iNumProducts; i++ )
            {
            	if ( purchase.getSku().equals(g_sPurchaseProductNames[i]) )
	            {
	            	// is it consumable?
	            	if ( g_iPurchaseProductTypes[i] == 1 ) mHelper.consumeAsync(purchase, mConsumeFinishedListener);
	            	else 
	            	{
	            		g_iPurchaseProductStates[i] = 1;
	            		g_iPurchaseState = 1;
	            		Log.d("IAB PurchaseFinished", "Purchase successful: " + g_sPurchaseProductNames[i]);
	            	}
	            	return;
	            }
            }

            g_iPurchaseState = 1;
            Log.e("IAB PurchaseFinished", "Purchase failure SKU not found: " + purchase.getSku());
        }
    };
    
    // Called when consumption is complete
    static IabHelper.OnConsumeFinishedListener mConsumeFinishedListener = new IabHelper.OnConsumeFinishedListener() {
        public void onConsumeFinished(Purchase purchase, IabResult result) {
            Log.d("IAB ConsumeFinished", "Consumption finished. Purchase: " + purchase + ", result: " + result);

            int ID = -1;
            for( int i = 0; i < g_iNumProducts; i++ )
            {
            	if ( purchase.getSku().equals(g_sPurchaseProductNames[i]) ) 
            	{
            		ID = i;
            		break;
            	}
            }
            
            if ( ID < 0 ) 
            {
            	Log.e("IAB ConsumeFinished","Error while consuming: SKU not found " + purchase.getSku());
            	g_iPurchaseState = 1;
            	return;
            }
            
            if (result.isSuccess()) {
            	g_iPurchaseProductStates[ID] = 1;
                Log.d("IAB ConsumeFinished", "Consumption successful. Provisioning.");
            }
            else {
                Log.e("IAB ConsumeFinished","Error while consuming: " + result);
                AGKHelper.ShowMessage(g_pAct, "Error while consuming purchase: " + result);
            }
            
            g_iPurchaseState = 1;
        }
    };
	
	public static void iapAddProduct( String name, int ID, int type )
	{		
		name = name.toLowerCase();
		Log.w("IAB AddProduct","Adding: " + name + " to ID: " + Integer.toString(ID));
		if ( ID < 0 || ID >= MAX_PRODUCTS ) return;
		g_iPurchaseProductStates[ ID ] = 0;
		g_sPurchaseProductNames[ ID ] = name;
		g_iPurchaseProductTypes[ ID ] = type;
		Log.w("IAB AddProduct","Added: " + name);
		if ( ID+1 > g_iNumProducts ) g_iNumProducts = ID+1;
    }
	
	public static void iapSetup( Activity act )
	{		
		g_pAct = act;
		mHelper = new IabHelper(act, base64EncodedPublicKey);
		mHelper.enableDebugLogging(true);
		
		mHelper.startSetup(new IabHelper.OnIabSetupFinishedListener() {
            public void onIabSetupFinished(IabResult result) {
                Log.d("In App Billing", "Setup finished.");

                if (!result.isSuccess()) {
                	Log.e("In App Billing", "Problem setting up in-app billing: " + result);
                    return;
                }

                // Hooray, IAB is fully set up. Now, let's get an inventory of stuff we own.
                Log.d("In App Billing", "Setup successful. Querying inventory.");
                mHelper.queryInventoryAsync(mGotInventoryListener);
            }
        });
    }
	
	public static void iapMakePurchase( Activity act, int ID )
	{		
		if ( ID < 0 || ID >= MAX_PRODUCTS ) return;
		if ( g_iPurchaseProductTypes[ ID ] == 0 && g_iPurchaseProductStates[ ID ] == 1 ) 
		{
			AGKHelper.ShowMessage(act,"You have already purchased that item");
			return; // non-consumable item already purchased
		}
		
		g_iPurchaseState = 0;
		g_iPurchaseProductStates[ ID ] = 0;
		g_iIAPID = ID;
		Log.w("IAB MakePurchase","Buying "+g_sPurchaseProductNames[ID]);
		
		Intent myIntent = new Intent(act, IAPActivity.class);
		act.startActivity(myIntent);
    }
	
	public static int iapCheckPurchaseState()
	{
		return g_iPurchaseState;
	}
	
	public static int iapCheckPurchase( int ID )
	{
		return g_iPurchaseProductStates[ ID ];
	}
	
	// ******************
	// Push Notifications
	// ******************
	
	public static String PACKAGE_NAME = "";
	public static String PNRegID = "";
	
	public static int registerPushNotification( Activity nativeactivityptr )
	{
		PACKAGE_NAME = nativeactivityptr.getApplicationContext().getPackageName();
		
		try
		{
			GCMRegistrar.checkDevice(nativeactivityptr);
			GCMRegistrar.checkManifest(nativeactivityptr);
			String regId = GCMRegistrar.getRegistrationId(nativeactivityptr);
			if (regId.equals("")) {
			  //GCMRegistrar.register(nativeactivityptr, "210280521980"); // TGC
				GCMRegistrar.register(nativeactivityptr, "1083864810983"); // Focus
			}
			else 
			{
				PNRegID = regId;
			}
		}
		catch( Exception e )
		{
			Log.e("Push Notification", e.toString());
			return 0;
		}
		
		return 1;
	}
	
	public static String getPNRegID()
	{
		return PNRegID;
	}
	
	public static String GetAppName(Activity act)
	{
		final PackageManager pm = act.getApplicationContext().getPackageManager();
		
		ApplicationInfo ai;
		try 
		{
		    ai = pm.getApplicationInfo( act.getPackageName(), 0);
		} 
		catch (final NameNotFoundException e) 
		{
		    ai = null;
		}
		final String applicationName = (String) (ai != null ? pm.getApplicationLabel(ai) : "unknown");
		return applicationName;
	}
	
	// image chooser code
	//private static MyJavaActivity myActivity = null;
	//private static MyJavaActivity imageActivity = null;
    private static String storeimagepath = null;
	public static void StoreImagePath(String path) { storeimagepath=path; }
	
	// Function to launch Choose Image intent
	public static String StartChooseImage(Activity nativeactivityptr)
	{
		// Ensure we can create a new activity in this static function
		Looper.prepare();
		
		// Create new intent and launch it (choose image)
		Intent myIntent = new Intent(nativeactivityptr, MyJavaActivity.class);
		nativeactivityptr.startActivity(myIntent);
		
		// return immediately - fun string return (can be replaced with boolean/int)
		return "success";
    }
	
	// Retrieve image path string when we return to main NativeActivity
	public static String GetChosenImagePath()
	{
		if ( storeimagepath!=null )
			return storeimagepath;
		else
			return "";
    }
	
	// camera
	public static void CaptureImage(Activity nativeactivityptr)
	{
		// Ensure we can create a new activity in this static function
		Looper.prepare();
				
		Intent cameraIntent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
		cameraIntent.putExtra(MediaStore.EXTRA_OUTPUT, Uri.parse("file:///sdcard/capturedimage.jpg"));
		nativeactivityptr.startActivity(cameraIntent);    
    }
	
	public static String GetLanguage()
	{
		return Locale.getDefault().getLanguage();
	}
	
	public static int isNetworkAvailable( Activity act ) 
	{
	    ConnectivityManager connectivityManager = (ConnectivityManager) act.getSystemService(Context.CONNECTIVITY_SERVICE);
	    NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
	    if ( activeNetworkInfo != null && activeNetworkInfo.isConnected() ) return 1;
	    else return 0;
	}
	
	// Facebook
	static int facebookLoginState = 1;
	static String appID = "";
		
	public static void FacebookLogin(Activity act, String ID)
	{
		appID = ID;
		facebookLoginState = 1;
		Looper.prepare();
		
		Intent myIntent = new Intent(act, MyFacebookActivity.class);
		act.startActivity(myIntent);
	}
	
	public static void FacebookLogout()
	{
		facebookLoginState = 1;
		Session session = Session.getActiveSession();
        if ( session != null && !session.isClosed() ) 
        {
            session.closeAndClearTokenInformation();
        }
        Session.setActiveSession(null);
        facebookLoginState = 0;
	}
	
	public static int FacebookGetLoginState()
	{
		if( facebookLoginState == 1 ) return 0;
		if ( Session.getActiveSession() == null ) return -1;
		{
			if( Session.getActiveSession().getAccessToken() == "" ) 
			{
				FacebookLogout();
				return -1;
			}
		}
		return 1; 
	}
	
	public static String FacebookGetAccessToken()
	{
		//Log.e("AGK",Session.getActiveSession().getAccessToken());
		if ( facebookLoginState == 1 ) return "";
		if ( Session.getActiveSession() == null ) return "Error";
		return Session.getActiveSession().getAccessToken();
	}
	
	public static void FacebookPost( Activity act, String szID, String szLink, String szPicture, String szName, String szCaption, String szDescription )
	{
		RunnableFacebook feed = new RunnableFacebook();
		feed.szID = szID;
		feed.szLink = szLink;
		feed.szPicture = szPicture;
		feed.szName = szName;
		feed.szCaption = szCaption;
		feed.szDescription = szDescription;
		feed.act = act;
		act.runOnUiThread( feed );	
	}
	
	public static String ConvertString( String s )
	{
		String s2 = java.text.Normalizer.normalize(s, java.text.Normalizer.Form.NFD).replaceAll("\\p{InCombiningDiacriticalMarks}+","");
		return s2;
	}
	
	public static void GenerateCrashReport(Activity act)
	{
		Process mLogcatProc = null;
	    try 
	    {
			mLogcatProc = Runtime.getRuntime().exec( new String[] {"logcat", "-d", "*:V" });
			
			InputStream is = mLogcatProc.getInputStream();
			
			SimpleDateFormat s = new SimpleDateFormat("yyyy-MM-dd-hh-mm-ss");
			String format = s.format(new Date());
	    
		   // File f = new File("/sdcard/crashreport "+format+".txt");
			File f = new File("/sdcard/crashreport.txt");
		    OutputStream os;
			os = new FileOutputStream(f);
		
			byte[] buffer = new byte[1024];
			int bytesRead;
        
			while ((bytesRead = is.read(buffer)) != -1)
			{
			    os.write(buffer, 0, bytesRead);
			}
		
        	//is.close();
			os.close();
			
			Intent emailIntent = new Intent(android.content.Intent.ACTION_SEND);
			emailIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
			emailIntent.putExtra(android.content.Intent.EXTRA_EMAIL, new String[]{"paul@thegamecreators.com"});
			emailIntent.putExtra(android.content.Intent.EXTRA_SUBJECT, "AGK Bug Report");
			emailIntent.putExtra(android.content.Intent.EXTRA_TEXT, "Bug report attached. Any additional information: ");

			String rawFolderPath = "file:///mnt/sdcard/crashreport.txt";

			// Here my file name is shortcuts.pdf which i have stored in /res/raw folder
			Uri emailUri = Uri.parse(rawFolderPath );
			emailIntent.putExtra(Intent.EXTRA_STREAM, emailUri);
			emailIntent.setType("text/plain");
			act.startActivity(Intent.createChooser(emailIntent, "Send bug report..."));
		} 
	    catch (IOException e) 
	    {
			e.printStackTrace();
		}
	}
	
	public static void SetExpansionVersion(int version)
	{
		
	}
	
	public static int GetExpansionState(Activity act)
	{
		return 0;
	}
	
	public static void DownloadExpansion(Activity act)
	{
		
	}
	
	public static float GetExpansionProgress(Activity act)
	{
		return 0;
	}
}
