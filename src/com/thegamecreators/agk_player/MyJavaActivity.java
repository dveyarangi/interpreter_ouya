package com.thegamecreators.agk_player;

//import com.mycompany.myapp.R;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;

import android.app.Activity;
import android.content.Intent; 
import android.net.Uri;
import android.os.Bundle;

// Getting file from image intent return data
import android.provider.MediaStore;
import android.util.Log;
import android.database.Cursor;

// Dynamic Java Code
public class MyJavaActivity extends Activity
{
	// Mode to trigger photo selection
    private static final int SELECT_PHOTO = 100;
    private static Uri selectedImage;

	// Called when the activity is first created.
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		// call Image Picker right away
		super.onCreate(savedInstanceState);
		setContentView(R.layout.facebookmain);
		
		Intent photoPickerIntent = new Intent(Intent.ACTION_PICK);
		photoPickerIntent.setType("image/*");
		this.startActivityForResult(photoPickerIntent, SELECT_PHOTO);    
	}	
	
	private void downloadImage(Uri imageUri) 
	{

	    File cacheDir;
	    // if the device has an SD card
	    if (android.os.Environment.getExternalStorageState().equals(android.os.Environment.MEDIA_MOUNTED)) {
	        cacheDir = android.os.Environment.getExternalStorageDirectory();
	    } else {
	        // it does not have an SD card
	        cacheDir = getCacheDir();
	    }

	    if(!cacheDir.exists()) cacheDir.mkdirs();
	    File f = new File(cacheDir, "chosenImage.jpg");

	    try {

	        InputStream is = null;
	        if (imageUri.toString().startsWith("content://com.google.android.gallery3d")
	         || imageUri.toString().startsWith("content://com.android.gallery3d")) {
	            is = getContentResolver().openInputStream(imageUri);
	        } else {
	            is = new URL(imageUri.toString()).openStream();
	        }

	        OutputStream os = new FileOutputStream(f);
	        
        	byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = is.read(buffer)) != -1)
            {
                os.write(buffer, 0, bytesRead);
            }
            
            is.close();
            os.close();
	        
            AGKHelper.StoreImagePath(f.getAbsolutePath());
	        finish();
	        return;
	    } 
	    catch (Exception ex) 
	    {
	        Log.e("Download Image", "Exception: " + ex.toString());
	    }
	    
	    AGKHelper.StoreImagePath("");
        finish();
	}
	
	//@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent imageReturnedIntent)
	{ 
		boolean bFinish = true;
		
		// we can now intercept the return value in our dynamically created Java code
	    super.onActivityResult(requestCode, resultCode, imageReturnedIntent); 
	    Log.e("Choose Image","Request: "+Integer.toString(requestCode)+", Result: "+Integer.toString(resultCode));
	    switch(requestCode) 
	    { 
	    	case SELECT_PHOTO:
	    	{
		        if(resultCode == RESULT_OK)
		        {  
		        	// Horrible process of getting the file path (sheesh)
		        	selectedImage = imageReturnedIntent.getData();
		        	//if (selectedImage.toString().startsWith("content://com.android.gallery3d.provider"))  {
		                // use the com.google provider, not the com.android provider.
		        		//selectedImage = Uri.parse(selectedImage.toString().replace("com.android.gallery3d","com.google.android.gallery3d"));
		            //}
		        	Log.e("Choose Image", "URI: "+selectedImage.toString() );
		            String[] filePathColumn = {MediaStore.Images.Media.DATA};
		            Cursor cursor = getContentResolver().query(selectedImage, filePathColumn, null, null, null);
		            
		            String filePath = "";
		            
		            if (cursor != null) {

		                int dataColumnIndex = cursor.getColumnIndex(filePathColumn[0]);
		                cursor.moveToFirst();

		                // if it is a picasa image on newer devices with OS 3.0 and up
		                if (selectedImage.toString().startsWith("content://com.google.android.gallery3d")
		                 || selectedImage.toString().startsWith("content://com.android.gallery3d")) 
		                {
		                	bFinish = false;
		                	Log.e("Choose Image", "Attempting Download" );
		                	new Thread(new Runnable() {
		                        public void run() {
		                        	downloadImage(selectedImage);
		                        }
		                	}).start();                       
		                } 
		                else 
		                { // it is a regular local image file
		                	filePath = cursor.getString(dataColumnIndex);
		                }

		                cursor.close();
		            } 
		            else 
		            {
		            	bFinish = false;
		            	Log.e("Choose Image", "Null - Attempting Download" );
		            	new Thread(new Runnable() {
	                        public void run() {
	                        	downloadImage(selectedImage);
	                        }
	                	}).start();		
		            }
		            
		            Log.e("Choose Image","Final Path: "+filePath);
		            
		            // Store it in my static Java Class (global)
		            if ( bFinish ) AGKHelper.StoreImagePath(filePath);
		        }
	    	}
	    }
	    
        // end this activity
	    if ( bFinish ) this.finish();
	}
}

	