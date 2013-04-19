package com.thegamecreators.agk_player;

//import com.mycompany.myapp.R;
import com.facebook.Session;
import com.facebook.SessionLoginBehavior;
import com.facebook.SessionState;

import android.app.Activity;
import android.content.Intent; 
import android.net.Uri;
import android.os.Bundle;

// Getting file from image intent return data
import android.provider.MediaStore;
import android.util.Log;
import android.database.Cursor;

// Dynamic Java Code
public class MyFacebookActivity extends Activity
{
	Session.StatusCallback statusCallback = new SessionStatusCallback();

	// Called when the activity is first created.
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		// call Image Picker right away
		super.onCreate(savedInstanceState);
		setContentView(R.layout.facebookmain);
		
		Session session = Session.getActiveSession();
        if (session == null) {
            if (savedInstanceState != null) {
            	session = Session.restoreSession(this, null, statusCallback, savedInstanceState);
            }
            if (session == null) {
            	session = new Session.Builder(this).setApplicationId(AGKHelper.appID).build();
            }
            Session.setActiveSession(session);
            
            session.openForRead(new Session.OpenRequest(this).setCallback(statusCallback));
        }
        else 
        {
        	AGKHelper.facebookLoginState = 0;
        	finish();
        }
	}	
	
	//@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{ 
		// we can now intercept the return value in our dynamically created Java code
	    super.onActivityResult(requestCode, resultCode, data); 
	    Session.getActiveSession().onActivityResult(this, requestCode, resultCode, data);
	}
	
	@Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        Session session = Session.getActiveSession();
        Session.saveSession(session, outState); 
    }
	
	private class SessionStatusCallback implements Session.StatusCallback {
        public void call(Session session, SessionState state, Exception exception) {
        	if ( session.isOpened() || session.isClosed() ) 
        	{
        		AGKHelper.facebookLoginState = 0;
        		finish();
        	}
        }
    }
}

	