package com.thegamecreators.agk_player;

//import com.mycompany.myapp.R;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import java.security.MessageDigest; 


public class IAPActivity extends Activity 
{
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
    
        AGKHelper.mHelper.launchPurchaseFlow(this, AGKHelper.g_sPurchaseProductNames[AGKHelper.g_iIAPID], 10001, AGKHelper.mPurchaseFinishedListener, "");
    }
    
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d("IAB IAPActivity", "onActivityResult(" + requestCode + "," + resultCode + "," + data);

        // Pass on the activity result to the helper for handling
        if (!AGKHelper.mHelper.handleActivityResult(requestCode, resultCode, data)) {
            // not handled, so handle it ourselves (here's where you'd
            // perform any handling of activity results not related to in-app
            // billing...
            super.onActivityResult(requestCode, resultCode, data);
        }
        else {
            Log.d("IAB IAPActivity", "onActivityResult handled by IABUtil.");
            finish();
        }
    }

    /**
     * Called when this activity becomes visible.
     */
    @Override
    protected void onStart() 
    {
    	super.onStart();
    	Log.e("IAP Activity","Started");
    }

    /**
     * Called when this activity is no longer visible.
     */
    @Override
    protected void onStop() 
    {
    	Log.e("IAP Activity","Stopped");
    	super.onStop();
    }

    @Override
    protected void onDestroy() 
    {
    	Log.e("IAP Activity","Destroyed");
    	super.onDestroy();
    }

}
