#include "agk.h"
#include <android/keycodes.h>
#include "interpreter.h"
#include "CoreForApp.h"
#include <android/native_activity.h>
#include <jni.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <android/log.h>

#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

struct egldata {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    struct ANativeActivity *activity;
};

ANativeActivity *g_pActivity = 0;

extern "C"
{
	bool g_bFirstLoop = true;
	int wasPlaying = 0;
	bool g_bIsTablet = false;

	// to skip calibration set g_bCalibrated to true and g_iCalibrationSetup to 2
	bool g_bCalibrated = true;
	int g_iCalibrationSetup = 2;

	// default to portrait accelerometer, detect landscape later, or override with calibration
	int g_iPortraitAxis = 1; // 0=X, 1=Y
	int g_iPortraitModifier = 1;
	int g_iLandscapeAxis = 0; // 0=X, 1=Y
	int g_iLandscapeModifier = -1;

	int checkformat( ANativeActivity* pActivity )
	{
		int result = 0;

		JNIEnv* lJNIEnv = pActivity->env;

		jint lFlags = 0;
		JavaVM* vm = pActivity->vm;
		bool attached = false;
		switch (vm->GetEnv((void**)&lJNIEnv, JNI_VERSION_1_6))
		{
			case JNI_OK: break;
			case JNI_EDETACHED:
			{
				if (vm->AttachCurrentThread(&lJNIEnv, NULL)!=0)
				{
					agk::Warning("Could not attach current thread");
				}
				break;
			}
			case JNI_EVERSION: agk::Warning("Invalid java version");
		}

		jobject lNativeActivity = pActivity->clazz;
		jclass ClassNativeActivity = lJNIEnv->GetObjectClass(lNativeActivity);
		jboolean isCopy;

		// Retrieves Build class.
		jclass ClassContext = lJNIEnv->FindClass( "android/os/Build" );

		/*
		// Board
		jfieldID FieldBOARD = lJNIEnv->GetStaticFieldID( ClassContext, "BOARD", "Ljava/lang/String;" );
		jstring BOARD = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldBOARD );
		const char* szBoard = lJNIEnv->GetStringUTFChars( BOARD, &isCopy );
		agk::Warning( szBoard );

		// Brand
		FieldBOARD = lJNIEnv->GetStaticFieldID( ClassContext, "BRAND", "Ljava/lang/String;" );
		BOARD = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldBOARD );
		szBoard = lJNIEnv->GetStringUTFChars( BOARD, &isCopy );
		agk::Warning( szBoard );
		*/

		// Device
		jfieldID FieldDEVICE = lJNIEnv->GetStaticFieldID( ClassContext, "DEVICE", "Ljava/lang/String;" );
		jstring DEVICE = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldDEVICE );
		const char *szDevice = lJNIEnv->GetStringUTFChars( DEVICE, &isCopy );
		agk::Warning( szDevice );
		if ( strcmp( szDevice, "marvel" ) == 0 ) result = 1; //HTC Wildfire S
		lJNIEnv->ReleaseStringUTFChars( DEVICE, szDevice );

		/*
		// Display
		FieldBOARD = lJNIEnv->GetStaticFieldID( ClassContext, "DISPLAY", "Ljava/lang/String;" );
		BOARD = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldBOARD );
		szBoard = lJNIEnv->GetStringUTFChars( BOARD, &isCopy );
		agk::Warning( szBoard );

		// Hardware
		FieldBOARD = lJNIEnv->GetStaticFieldID( ClassContext, "HARDWARE", "Ljava/lang/String;" );
		BOARD = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldBOARD );
		szBoard = lJNIEnv->GetStringUTFChars( BOARD, &isCopy );
		agk::Warning( szBoard );
*/
		// Model
		jfieldID FieldMODEL = lJNIEnv->GetStaticFieldID( ClassContext, "MODEL", "Ljava/lang/String;" );
		jstring MODEL = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldMODEL );
		const char *szModel = lJNIEnv->GetStringUTFChars( MODEL, &isCopy );
		agk::Warning( szModel );
		lJNIEnv->ReleaseStringUTFChars( MODEL, szModel );
/*
		// PRODUCT
		FieldBOARD = lJNIEnv->GetStaticFieldID( ClassContext, "PRODUCT", "Ljava/lang/String;" );
		BOARD = (jstring) lJNIEnv->GetStaticObjectField( ClassContext, FieldBOARD );
		szBoard = lJNIEnv->GetStringUTFChars( BOARD, &isCopy );
		agk::Warning( szBoard );
		*/

		// clear error message for interpreter
		agk::Warning( "" );
		return result;
	}

	void init( void *ptr )
	{
		agk::SetExtraAGKPlayerAssetsMode ( 2 );

		agk::InitGL(ptr);

		if ( App.g_dwDeviceWidth==0 )
		{
			App.g_dwDeviceWidth = agk::GetDeviceWidth();
			App.g_dwDeviceHeight = agk::GetDeviceHeight();
		}

		egldata *eptr = (egldata*)ptr;

		g_pActivity = eptr->activity;

		// (OLD) detect if this screen reports as a native landscape and modify accelerometer data to compensate
		/*
		AConfiguration *config = AConfiguration_new();
		AConfiguration_fromAssetManager( config, eptr->activity->assetManager);
		int screenlong = AConfiguration_getScreenLong( config );
		LOGW("Screen Long: %d", screenlong);
		if ( screenlong == ACONFIGURATION_SCREENLONG_NO )
		{
			g_iPortraitAxis = 0; // 0=X, 1=Y
			g_iPortraitModifier = -1;
			g_iLandscapeAxis = 1; // 0=X, 1=Y
			g_iLandscapeModifier = -1;
			g_bIsTablet = true;
		}
		AConfiguration_delete( config );
		*/

		// (NEW) detect native orientation code
		jint lFlags = 0;
		JNIEnv* lJNIEnv = g_pActivity->env;
		JavaVM* vm = g_pActivity->vm;
		vm->AttachCurrentThread(&lJNIEnv, NULL);

		if ( !g_pActivity ) agk::Warning("Failed to get activity pointer");
		jobject lNativeActivity = g_pActivity->clazz;
		if ( !lNativeActivity ) agk::Warning("Failed to get native activity pointer");

		jclass classActivity = lJNIEnv->FindClass("android/app/NativeActivity");
		if ( !classActivity ) agk::Warning("Failed to get class NativeActivity");

		jmethodID getClassLoader = lJNIEnv->GetMethodID(classActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
		if( !getClassLoader ) agk::Warning("Exception occurred while getting getClassLoader methodId");

		jobject objClassLoader = lJNIEnv->CallObjectMethod(lNativeActivity, getClassLoader);
		if ( !objClassLoader ) agk::Warning("Exception occurred while getting class loader instance");

		jclass classLoader = lJNIEnv->FindClass("java/lang/ClassLoader");
		if ( !classLoader ) agk::Warning("Exception occurred while finding ClassLoader class definition");

		jmethodID methodFindClass = lJNIEnv->GetMethodID( classLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
		if( !methodFindClass ) agk::Warning("Exception occurred while getting loadClass method id");

		// Get AGK Helper class
		jclass classHelper = (jclass)lJNIEnv->CallObjectMethod(objClassLoader, methodFindClass, lJNIEnv->NewStringUTF("com.thegamecreators.agk_player.AGKHelper"));
		if( !classHelper ) agk::Warning("Failed to get AGKHelper class");

		jmethodID methodGetOrien = lJNIEnv->GetStaticMethodID( classHelper, "GetOrientation","(Landroid/app/Activity;)I");
		if ( !methodGetOrien ) agk::Warning("Failed to get method GetOrientation");

		int orien = lJNIEnv->CallStaticIntMethod( classHelper, methodGetOrien, lNativeActivity );

		vm->DetachCurrentThread();

		if ( agk::GetDeviceWidth() > agk::GetDeviceHeight() )
		{
			if (orien == 0 || orien == 2)
			{
				g_iPortraitAxis = 0; // 0=X, 1=Y
				g_iPortraitModifier = -1;
				g_iLandscapeAxis = 1; // 0=X, 1=Y
				g_iLandscapeModifier = -1;
				g_bIsTablet = true;
			}
		}
		else
		{
			if (orien == 1 || orien == 3)
			{
				g_iPortraitAxis = 0; // 0=X, 1=Y
				g_iPortraitModifier = -1;
				g_iLandscapeAxis = 1; // 0=X, 1=Y
				g_iLandscapeModifier = -1;
				g_bIsTablet = true;
			}
		}
	}

	void updateptr( void *ptr )
	{
		agk::UpdatePtr(ptr);
	}

	void updateptr2( void *ptr )
	{
		agk::UpdatePtr2(ptr);
	}

	void cleanup()
	{
		App.End();
		agk::MasterReset();
	}

	void devicerotate()
	{
		float aspect = agk::GetDisplayAspect();
		agk::SetVirtualResolution( agk::GetVirtualWidth(), agk::GetVirtualHeight() );
		agk::SetDisplayAspect( aspect );
	}

	void pauseapp()
	{
		wasPlaying = 0;
		if ( agk::GetMusicPlaying() )
		{
			wasPlaying = 1;
			agk::PauseMusic();
		}
	}

	void resumeapp()
	{
		if ( wasPlaying ) agk::ResumeMusic();
		agk::Resumed();
	}

	int gettextstate()
	{
		return agk::GetTextInputState();
	}

	void begin()
	{
		static int image1 = 0;
		static int text1 = 0;

		App.Begin();
		g_bFirstLoop = false;

		/*
		// accelerometer calibration
		if ( g_iCalibrationSetup == 0 )
		{
			g_iCalibrationSetup = 1;

			FILE *pFile = fopen( "/sdcard/AGK/calibration.cfg", "rb" );
			if ( pFile )
			{
				fread( &g_iPortraitAxis, 4, 1, pFile );
				fread( &g_iPortraitModifier, 4, 1, pFile );

				fread( &g_iLandscapeAxis, 4, 1, pFile );
				fread( &g_iLandscapeModifier, 4, 1, pFile );
				fclose( pFile );
				g_bCalibrated = true;
			}
			else
			{
				agk::SetOrientationAllowed( 1,0,0,0 );
				agk::SetDisplayAspect( agk::GetDeviceWidth() / (float) agk::GetDeviceHeight() );

				image1 = agk::LoadImage( "Avenir.png" );

				text1 = agk::CreateText( "Accelerometer Calibration\nPlease hold your device so\nthis text is the right way up" );
				agk::SetTextAlignment( text1, 1 );
				agk::SetTextPosition( text1, 50, 20 );
				agk::SetTextColor( text1, 255,255,255 );
				agk::SetTextFontImage( text1, image1 );

				agk::AddVirtualButton( 1, 50, 70, 25 );
				agk::SetVirtualButtonText( 1, "Ok" );
			}
		}

		if ( g_bCalibrated )
		{
			agk::SetOrientationAllowed( 1,1,1,1 );
			App.Begin();
			g_bFirstLoop = false;
		}
		else
		{
			if ( g_iCalibrationSetup == 1 )
			{
				if ( agk::GetVirtualButtonReleased( 1 ) == 1 )
				{
					float x = agk::GetRawAccelX();
					float y = agk::GetRawAccelY();

					if ( agk::Abs(y) > agk::Abs(x) )
					{
						g_iPortraitAxis = 1;
						if ( y < 0 ) g_iPortraitModifier = -1;
						else g_iPortraitModifier = 1;
					}
					else
					{
						g_iPortraitAxis = 0;
						if ( x < 0 ) g_iPortraitModifier = -1;
						else g_iPortraitModifier = 1;
					}

					agk::SetOrientationAllowed( 0,0,1,0 );
					agk::SetDisplayAspect( agk::GetDeviceHeight() / (float) agk::GetDeviceWidth() );
					agk::SetTextSize( text1, 4 );
					agk::SetVirtualButtonSize( 1, 20 );

					g_iCalibrationSetup = 2;
				}
			}
			else
			{
				if ( agk::GetVirtualButtonReleased( 1 ) == 1 )
				{
					float x = -agk::GetRawAccelY(); // ignore orientation related axis swapping
					float y = agk::GetRawAccelX();

					if ( agk::Abs(y) > agk::Abs(x) )
					{
						g_iLandscapeAxis = 1;
						if ( y > 0 ) g_iLandscapeModifier = -1;
						else g_iLandscapeModifier = 1;
					}
					else
					{
						g_iLandscapeAxis = 0;
						if ( x > 0 ) g_iLandscapeModifier = -1;
						else g_iLandscapeModifier = 1;
					}

					FILE *pFile = fopen( "/sdcard/AGK/calibration.cfg", "wb" );
					if ( pFile )
					{
						fwrite( &g_iPortraitAxis, 4, 1, pFile );
						fwrite( &g_iPortraitModifier, 4, 1, pFile );

						fwrite( &g_iLandscapeAxis, 4, 1, pFile );
						fwrite( &g_iLandscapeModifier, 4, 1, pFile );
						fclose( pFile );
					}
					else
					{
						agk::Warning( "Failed to write calibration data" );
					}

					agk::DeleteText( text1 );
					agk::DeleteImage( image1 );
					agk::DeleteVirtualButton( 1 );
					agk::SetOrientationAllowed( 1,1,1,1 );

					agk::Sync();

					g_iCalibrationSetup = 3;
					g_bCalibrated = true;
				}
			}

			agk::Sync();
		}
		*/
	}

	void reset()
	{
		//g_bFirstLoop = true;
	}

	void loop()
	{
		if ( g_bFirstLoop )
		{
			begin();
		}
		else
		{
			static int lastorien = 0;
			static int orien = 0;
			static float orientime = 0;
			if ( agk::Timer() > orientime )
			{
				orientime = agk::Timer() + 1;

				jint lFlags = 0;
				JNIEnv* lJNIEnv = g_pActivity->env;
				JavaVM* vm = g_pActivity->vm;
				vm->AttachCurrentThread(&lJNIEnv, NULL);

				if ( !g_pActivity ) agk::Warning("Failed to get activity pointer");
				jobject lNativeActivity = g_pActivity->clazz;
				if ( !lNativeActivity ) agk::Warning("Failed to get native activity pointer");

				jclass classActivity = lJNIEnv->FindClass("android/app/NativeActivity");
				if ( !classActivity ) agk::Warning("Failed to get class NativeActivity");

				jmethodID getClassLoader = lJNIEnv->GetMethodID(classActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");
				if( !getClassLoader ) agk::Warning("Exception occurred while getting getClassLoader methodId");

				jobject objClassLoader = lJNIEnv->CallObjectMethod(lNativeActivity, getClassLoader);
				if ( !objClassLoader ) agk::Warning("Exception occurred while getting class loader instance");

				jclass classLoader = lJNIEnv->FindClass("java/lang/ClassLoader");
				if ( !classLoader ) agk::Warning("Exception occurred while finding ClassLoader class definition");

				jmethodID methodFindClass = lJNIEnv->GetMethodID( classLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
				if( !methodFindClass ) agk::Warning("Exception occurred while getting loadClass method id");

				// Get AGK Helper class
				jclass classHelper = (jclass)lJNIEnv->CallObjectMethod(objClassLoader, methodFindClass, lJNIEnv->NewStringUTF("com.thegamecreators.agk_player.AGKHelper"));
				if( !classHelper ) agk::Warning("Failed to get AGKHelper class");

				jmethodID methodGetOrien = lJNIEnv->GetStaticMethodID( classHelper, "GetOrientation","(Landroid/app/Activity;)I");
				if ( !methodGetOrien ) agk::Warning("Failed to get method GetOrientation");

				orien = lJNIEnv->CallStaticIntMethod( classHelper, methodGetOrien, lNativeActivity );

				vm->DetachCurrentThread();

				if ( g_bIsTablet )
				{
					switch( orien )
					{
						case 0: orien = 3; break;
						case 1: orien = 2; break;
						case 2: orien = 4; break;
						case 3: orien = 1; break;
						default: break;
					}
				}
				else
				{
					switch( orien )
					{
						case 0: orien = 1; break;
						case 1: orien = 3; break;
						case 2: orien = 2; break;
						case 3: orien = 4; break;
						default: break;
					}
				}

				if ( orien != lastorien ) agk::OrientationChanged( orien );
				lastorien = orien;
			}

			App.Loop();
		}
	}

	void updatesize()
	{
		agk::UpdateDeviceSize();
	}

	void setaccel( float x, float y, float z )
	{
		x /= 10.0f;
		y /= 10.0f;
		z /= 10.0f;

		//uString str;
		//str.Format( "X: %.2f, Y: %.2f, Z: %.2f", x, y, z );
		//agk::Print( str );

		if ( g_bCalibrated )
		{
			float x2 = 0;
			float y2 = 0;

			if ( g_iPortraitAxis == 1 ) y2 = y * g_iPortraitModifier;
			else y2 = x * g_iPortraitModifier;

			if ( g_iLandscapeAxis == 1 ) x2 = y * g_iLandscapeModifier;
			else x2 = x * g_iLandscapeModifier;

			x = x2;
			y = y2;
		}

		// check orientation
		static int lastmode = 0;
		int mode = 0;
		if ( agk::Abs(y) > agk::Abs(x) + 0.2 )
		{
			if ( y < -0.4 ) mode = 2;
			if ( y > 0.4 ) mode = 1;
		}
		else if ( agk::Abs(x) > agk::Abs(y) + 0.2 )
		{
			if ( x < -0.4 ) mode = 3;
			if ( x > 0.4 ) mode = 4;
		}

		//if ( mode > 0 && mode != lastmode ) agk::OrientationChanged( mode );
		//lastmode = mode;

		agk::Accelerometer( x, y, z );
	}

	void touchdown( int id, float x, float y )
	{
		agk::TouchPressed( id+1, x, y );
	}

	void touchmoved( int id, float x, float y )
	{
		agk::TouchMoved( id+1, x, y );
	}

	void touchup( int id, float x, float y )
	{
	
		agk::TouchReleased( id+1, x, y );
	}

	int TranslateKey( int key )
	{
		switch( key )
		{
			case AKEYCODE_DEL: return AGK_KEY_BACK;
			case AKEYCODE_TAB: return AGK_KEY_TAB;
			case AKEYCODE_ENTER: return AGK_KEY_ENTER;
			case AKEYCODE_SHIFT_LEFT: return AGK_KEY_SHIFT;
			case AKEYCODE_SHIFT_RIGHT: return AGK_KEY_SHIFT;
			//case : return AGK_KEY_CONTROL;
			case AKEYCODE_BACK: return AGK_KEY_ESCAPE;
			case AKEYCODE_SPACE: return AGK_KEY_SPACE;
			case AKEYCODE_PAGE_UP: return AGK_KEY_PAGEUP;
			case AKEYCODE_PAGE_DOWN: return AGK_KEY_PAGEDOWN;
			//case : return AGK_KEY_END;
			//case : return AGK_KEY_HOME;
			case AKEYCODE_DPAD_LEFT: return AGK_KEY_LEFT;
			case AKEYCODE_DPAD_UP: return AGK_KEY_UP;
			case AKEYCODE_DPAD_RIGHT: return AGK_KEY_RIGHT;
			case AKEYCODE_DPAD_DOWN: return AGK_KEY_DOWN;
			//case : return AGK_KEY_INSERT;
			//case : return AGK_KEY_DELETE;

			case AKEYCODE_0: return AGK_KEY_0;
			case AKEYCODE_1: return AGK_KEY_1;
			case AKEYCODE_2: return AGK_KEY_2;
			case AKEYCODE_3: return AGK_KEY_3;
			case AKEYCODE_4: return AGK_KEY_4;
			case AKEYCODE_5: return AGK_KEY_5;
			case AKEYCODE_6: return AGK_KEY_6;
			case AKEYCODE_7: return AGK_KEY_7;
			case AKEYCODE_8: return AGK_KEY_8;
			case AKEYCODE_9: return AGK_KEY_9;

			case AKEYCODE_A: return AGK_KEY_A;
			case AKEYCODE_B: return AGK_KEY_B;
			case AKEYCODE_C: return AGK_KEY_C;
			case AKEYCODE_D: return AGK_KEY_D;
			case AKEYCODE_E: return AGK_KEY_E;
			case AKEYCODE_F: return AGK_KEY_F;
			case AKEYCODE_G: return AGK_KEY_G;
			case AKEYCODE_H: return AGK_KEY_H;
			case AKEYCODE_I: return AGK_KEY_I;
			case AKEYCODE_J: return AGK_KEY_J;
			case AKEYCODE_K: return AGK_KEY_K;
			case AKEYCODE_L: return AGK_KEY_L;
			case AKEYCODE_M: return AGK_KEY_M;
			case AKEYCODE_N: return AGK_KEY_N;
			case AKEYCODE_O: return AGK_KEY_O;
			case AKEYCODE_P: return AGK_KEY_P;
			case AKEYCODE_Q: return AGK_KEY_Q;
			case AKEYCODE_R: return AGK_KEY_R;
			case AKEYCODE_S: return AGK_KEY_S;
			case AKEYCODE_T: return AGK_KEY_T;
			case AKEYCODE_U: return AGK_KEY_U;
			case AKEYCODE_V: return AGK_KEY_V;
			case AKEYCODE_W: return AGK_KEY_W;
			case AKEYCODE_X: return AGK_KEY_X;
			case AKEYCODE_Y: return AGK_KEY_Y;
			case AKEYCODE_Z: return AGK_KEY_Z;

			case AKEYCODE_GRAVE: return 223; break; // `
			case AKEYCODE_MINUS: return 189; break; // -
			case AKEYCODE_EQUALS: return 187; break; // =
			case AKEYCODE_LEFT_BRACKET: return 219; break; // [
			case AKEYCODE_RIGHT_BRACKET: return 221; break; // ]
			case AKEYCODE_SEMICOLON: return 186; break; // ;
			case AKEYCODE_APOSTROPHE: return 192; break; // '
			case AKEYCODE_COMMA: return 188; break; // ,
			case AKEYCODE_PERIOD: return 190; break; // .
			case AKEYCODE_SLASH: return 191; break; // /
			case AKEYCODE_BACKSLASH: return 220; break; // \
			
			// O-U-Y-A keys:
			case 96:  return AGK_KEY_O;
			case 99:  return AGK_KEY_U;
			case 100: return AGK_KEY_Y;
			case 97:  return AGK_KEY_A;
			
			case 102: return 102; // L1 
			case 104: return 104; // L2
			case 106: return 106; // L3
			case 103: return 103; // R1
			case 105: return 105; // R2
			case 107: return 107; // R3

			default: return 0;
		}

		return 0;
	}

	int AsciiKey( int key )
	{
		if ( agk::GetRawKeyState( AGK_KEY_SHIFT ) == 1 )
		{
			// shift pressed
			switch( key )
			{
				case AKEYCODE_SPACE: return ' ';

				case AKEYCODE_0: return ')';
				case AKEYCODE_1: return '!';
				case AKEYCODE_2: return '@';
				case AKEYCODE_3: return '�';
				case AKEYCODE_4: return '$';
				case AKEYCODE_5: return '%';
				case AKEYCODE_6: return '^';
				case AKEYCODE_7: return '&';
				case AKEYCODE_8: return '*';
				case AKEYCODE_9: return '(';

				case AKEYCODE_A: return 'A';
				case AKEYCODE_B: return 'B';
				case AKEYCODE_C: return 'C';
				case AKEYCODE_D: return 'D';
				case AKEYCODE_E: return 'E';
				case AKEYCODE_F: return 'F';
				case AKEYCODE_G: return 'G';
				case AKEYCODE_H: return 'H';
				case AKEYCODE_I: return 'I';
				case AKEYCODE_J: return 'J';
				case AKEYCODE_K: return 'K';
				case AKEYCODE_L: return 'L';
				case AKEYCODE_M: return 'M';
				case AKEYCODE_N: return 'N';
				case AKEYCODE_O: return 'O';
				case AKEYCODE_P: return 'P';
				case AKEYCODE_Q: return 'Q';
				case AKEYCODE_R: return 'R';
				case AKEYCODE_S: return 'S';
				case AKEYCODE_T: return 'T';
				case AKEYCODE_U: return 'U';
				case AKEYCODE_V: return 'V';
				case AKEYCODE_W: return 'W';
				case AKEYCODE_X: return 'X';
				case AKEYCODE_Y: return 'Y';
				case AKEYCODE_Z: return 'Z';

				case AKEYCODE_COMMA: return '<';
				case AKEYCODE_PERIOD: return '>';
				case AKEYCODE_MINUS: return '_';
				case AKEYCODE_EQUALS: return '+';
				case AKEYCODE_LEFT_BRACKET: return '{';
				case AKEYCODE_RIGHT_BRACKET: return '}';
				case AKEYCODE_BACKSLASH: return '|';
				case AKEYCODE_SEMICOLON: return ':';
				case AKEYCODE_APOSTROPHE: return '"';
				case AKEYCODE_SLASH: return '?';

				default: return 0;
			}
		}
		else
		{
			// shift not pressed
			switch( key )
			{
				case AKEYCODE_SPACE: return ' ';

				case AKEYCODE_0: return '0';
				case AKEYCODE_1: return '1';
				case AKEYCODE_2: return '2';
				case AKEYCODE_3: return '3';
				case AKEYCODE_4: return '4';
				case AKEYCODE_5: return '5';
				case AKEYCODE_6: return '6';
				case AKEYCODE_7: return '7';
				case AKEYCODE_8: return '8';
				case AKEYCODE_9: return '9';

				case AKEYCODE_A: return 'a';
				case AKEYCODE_B: return 'b';
				case AKEYCODE_C: return 'c';
				case AKEYCODE_D: return 'd';
				case AKEYCODE_E: return 'e';
				case AKEYCODE_F: return 'f';
				case AKEYCODE_G: return 'g';
				case AKEYCODE_H: return 'h';
				case AKEYCODE_I: return 'i';
				case AKEYCODE_J: return 'j';
				case AKEYCODE_K: return 'k';
				case AKEYCODE_L: return 'l';
				case AKEYCODE_M: return 'm';
				case AKEYCODE_N: return 'n';
				case AKEYCODE_O: return 'o';
				case AKEYCODE_P: return 'p';
				case AKEYCODE_Q: return 'q';
				case AKEYCODE_R: return 'r';
				case AKEYCODE_S: return 's';
				case AKEYCODE_T: return 't';
				case AKEYCODE_U: return 'u';
				case AKEYCODE_V: return 'v';
				case AKEYCODE_W: return 'w';
				case AKEYCODE_X: return 'x';
				case AKEYCODE_Y: return 'y';
				case AKEYCODE_Z: return 'z';

				case AKEYCODE_STAR: return '*';
				case AKEYCODE_POUND: return '#';
				case AKEYCODE_COMMA: return ',';
				case AKEYCODE_PERIOD: return '.';
				case AKEYCODE_MINUS: return '-';
				case AKEYCODE_EQUALS: return '=';
				case AKEYCODE_LEFT_BRACKET: return '[';
				case AKEYCODE_RIGHT_BRACKET: return ']';
				case AKEYCODE_BACKSLASH: return '\\';
				case AKEYCODE_SEMICOLON: return ';';
				case AKEYCODE_APOSTROPHE: return '\'';
				case AKEYCODE_SLASH: return '/';
				case AKEYCODE_AT: return '@';
				case AKEYCODE_PLUS: return '+';

				default: return 0;
			}
		}
	}

	void keydown( int key )
	{
		agk::KeyDown( TranslateKey(key) );
		int ch = AsciiKey(key);
		if ( ch != 0 )
		{
			agk::CharDown( ch );
		}
	}

	void keyup( int key )
	{
		agk::KeyUp( TranslateKey(key) );
	}
}
