/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/window.h>

int checkformat( ANativeActivity* pActivity );
void init(void *ptr);
void updateptr(void *ptr);
void updateptr2(void *ptr);
void clearptr(void *ptr);
int gettextstate();
void begin();
void loop();
void updatesize();
void setaccel( float x, float y, float z );
void touchdown( int id, float x, float y );
void touchmoved( int id, float x, float y );
void touchup( int id, float x, float y );
void keydown( int key );
void keyup( int key );
void cleanup();
void pauseapp();
void resumeapp();
void exit(int);
void reset();
void devicerotate();

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

//0=16bit (faster), 1=32bit
#define g_iColorMode 1

/**
 * Our saved state data.
 */
struct saved_state {
    float angle;
    int32_t x;
    int32_t y;
};


/**
 * Shared state for our app.
 */
struct engine {
    struct android_app* app;

    ASensorManager* sensorManager;
    const ASensor* accelerometerSensor;
    ASensorEventQueue* sensorEventQueue;

    int animating;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    struct saved_state state;
};

struct egldata {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    struct ANativeActivity *activity;
};

EGLConfig config;

extern float AMotionEvent_getAxisValue(
    const AInputEvent* motion_event, int32_t axis, size_t pointer_index);
static typeof(AMotionEvent_getAxisValue) *p_AMotionEvent_getAxisValue;
#define AMotionEvent_getAxisValue (*p_AMotionEvent_getAxisValue)
#include <dlfcn.h>
/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(struct engine* engine) {
    // initialize OpenGL ES and EGL

	static int done = 0;

    /*
     * Here specify the attributes of the desired configuration.
     * Below, we select an EGLConfig with at least 8 bits per color
     * component compatible with on-screen windows
     */

#if g_iColorMode == 0
	// select 16 bit back buffer for performance
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BUFFER_SIZE, 16,
            EGL_DEPTH_SIZE, 16,
            EGL_STENCIL_SIZE, 0,
            EGL_CONFIG_CAVEAT, EGL_NONE,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
    };
#else
    // select 32 bit back buffer for quality
    const EGLint attribs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
				EGL_ALPHA_SIZE, 8,
                EGL_BUFFER_SIZE, 32,
                EGL_DEPTH_SIZE, 16,
                EGL_STENCIL_SIZE, 0,
                EGL_CONFIG_CAVEAT, EGL_NONE,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
        };
#endif

    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);

    /* Here, the application chooses the configuration it desires. In this
     * sample, we have a very simplified selection process, where we pick
     * the first EGLConfig that matches our criteria */
    EGLConfig allConfigs[20];
    eglChooseConfig(display, attribs, allConfigs, 20, &numConfigs);
    config = allConfigs[0];

    if ( numConfigs == 0 )
    {
    	LOGW( "Failed to find suitable render format" );
    	exit(0);
    	//return 0;
    }

    int i = 0;
    for ( i = 0; i < numConfigs; i++ )
    {
    	if ( i > 19 ) continue;
    	EGLint red;
    	EGLint green;
    	EGLint blue;
    	EGLint alpha;
    	EGLint depth;
    	EGLint stencil;
    	EGLint window;
    	EGLint render;
    	eglGetConfigAttrib( display, allConfigs[i], EGL_RED_SIZE, &red );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_GREEN_SIZE, &green );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_BLUE_SIZE, &blue );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_ALPHA_SIZE, &alpha );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_DEPTH_SIZE, &depth );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_STENCIL_SIZE, &stencil );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_SURFACE_TYPE, &window );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_NATIVE_VISUAL_ID, &format );
    	eglGetConfigAttrib( display, allConfigs[i], EGL_RENDERABLE_TYPE, &render );

    	LOGW( "R: %d, G: %d, B: %d, A: %d, D: %d, W: %d, F: %d, S: %d, R: %d", red, green, blue, alpha, depth, window, format, stencil, render );
    }

    int formatIndex = 0;

    // check for devices that need special render formats (Wildfire S being one)
    if ( checkformat( engine->app->activity ) > 0 )
    {
    	LOGW( "Adjusting render format for device" );

    	for ( i = 0; i < numConfigs; i++ )
		{
			if ( i > 19 ) continue;
			eglGetConfigAttrib( display, allConfigs[i], EGL_NATIVE_VISUAL_ID, &format );
			if ( format > 0 )
			{
				config = allConfigs[i];
				formatIndex = -1;
				break;
			}
		}
    }

    surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
	while ( surface == EGL_NO_SURFACE )
	{
		LOGW( "Failed to create EGL surface: %d, trying different format", eglGetError() );

		formatIndex++;
		if ( formatIndex >= numConfigs || formatIndex > 19 )
		{
			LOGW( "Failed to find compatible format" );
			return -1;
		}

		config = allConfigs[ formatIndex ];
		surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
	}

    /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
	 * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
	 * As soon as we picked a EGLConfig, we can safely reconfigure the
	 * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
	int result = ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);
	LOGW( "Result: %d", result );

	const EGLint contextAttribList[] = {EGL_CONTEXT_CLIENT_VERSION, 2,EGL_NONE};
	context = eglCreateContext(display, config, NULL, contextAttribList);
    if ( context == EGL_NO_CONTEXT )
	{
		LOGW( "Failed to create EGL context: %d", eglGetError() );
		return -1;
	}

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
    	int error = eglGetError();
        LOGW("Unable to eglMakeCurrent: %d", eglGetError());
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    LOGW( "Width: %d Height: %d", w, h );

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;
    engine->state.angle = 0;

    struct egldata data;
    data.display = display;
    data.surface = surface;
    data.context = context;
    data.activity = engine->app->activity;

    if ( done != 0 )
    {
    	updateptr( &data );
    }
    else
    {
    	init( &data );
    }

    //begin();
    p_AMotionEvent_getAxisValue =
        dlsym(RTLD_DEFAULT, "AMotionEvent_getAxisValue");
		
    done = 1;
    return 0;
}

/**
 * Just the current frame in the display.
 */
static void engine_draw_frame(struct engine* engine) {
    if (engine->display == NULL) {
        // No display.
        return;
    }

    if (engine->surface == NULL) {
		// No surface.
		return;
	}

    loop();
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(struct engine* engine)
{
	//cleanup();

    if (engine->display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->animating = 0;
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
    //exit(0);
}

/**
 * Process the next input event.
 */
enum {AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 11, AXIS_RZ = 14};

#define joystick_treshold 0.001f
 
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
	struct engine* engine = (struct engine*)app->userData;
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
	{
		// touch event

		int action = AMotionEvent_getAction( event ) & 0xff;
		int pointer = (AMotionEvent_getAction( event ) >> 8) & 0xff;
		int flags = AMotionEvent_getFlags( event );
		int pointers = AMotionEvent_getPointerCount( event );
		int id = AMotionEvent_getPointerId( event, pointer );

		float x = AMotionEvent_getX( event, pointer );
		float y = AMotionEvent_getY( event, pointer );
		//if ( action != 2 ) LOGW("Action: %d, pointer: %d, count: %d, id: %d", action, pointer, pointers, id);
		//__android_log_print(ANDROID_LOG_VERBOSE, "test", "Action: %d, pointer: %d, count: %d, id: %d, lx: %f, ly: %f, rx: %f, ry: %f", action, pointer, pointers, id, lx, ly, rx, ry);
		switch ( action )
		{
			case 0:
			case 5: touchdown( id, x, y ); break;
			case 1:
			case 3:
			case 6: touchup( id, x, y ); break;
			case 2:
				{
				float lx = AMotionEvent_getAxisValue(event, AXIS_X, pointer);
				float ly = AMotionEvent_getAxisValue(event, AXIS_Y, pointer);
				float rx = AMotionEvent_getAxisValue(event, AXIS_Z, pointer);
				float ry = AMotionEvent_getAxisValue(event, AXIS_RZ, pointer);
				
				if((lx > joystick_treshold || lx < -joystick_treshold) || (ly > joystick_treshold || ly < -joystick_treshold)) 
				{
					touchdown( 0, lx*1000.0f, ly*1000.0f );
//					touchmoved( 0, x*100.0f, y*100.0f );
				}
				else
					touchup( 0, lx*1000.0f, ly*1000.0f );

				if((rx > joystick_treshold || rx < -joystick_treshold) || (ry > joystick_treshold || ry < -joystick_treshold))
				{
					touchdown( 1, rx*1000.0f, ry*1000.0f );
//					touchmoved( -1, x*255.0f, y*255.0f );
//					__android_log_print(ANDROID_LOG_VERBOSE, "test", "right joystick moved");
				}			
				else
					touchup( 1, rx*1000.0f, ry*1000.0f );
					// only the primary pointer sends move messages so get the other pointer positions while we are here
					if ( pointers > 1 )
					{
						int i = 1;
						for ( i = 1; i < pointers; i++ )
						{
							int id = AMotionEvent_getPointerId( event, i );
							x = AMotionEvent_getX( event, i );
							y = AMotionEvent_getY( event, i );
							touchmoved( id, x, y );
						}
					}
					break;
				}
			default: break;
		}

		return 1;
	}
	else if ( AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY )
	{
		// key event

		int action = AKeyEvent_getAction( event );
		int flags = AKeyEvent_getFlags( event );
		int keyCode = AKeyEvent_getKeyCode( event );

		//LOGW("Action: %d, flags: %d, keyCode: %d", action, flags, keyCode);
		switch( action )
		{
		
			case AKEY_EVENT_ACTION_DOWN:
			{
				keydown( keyCode );
				break;
			}

			case AKEY_EVENT_ACTION_UP:
			{
				keyup( keyCode );
				break;
			}
		}

		if ( keyCode == AKEYCODE_BACK )
		{
			// if the back key is pressed whilst the keyboard is active this will crash the Nexus 7, return a marker value (2) to handle it in the native app glue
			if ( gettextstate() == 0 ) return 2; // keyboard is active
			else return 1;
		}
	}
	else {
		int action = AMotionEvent_getAction( event ) & 0xff;
		int pointer = (AMotionEvent_getAction( event ) >> 8) & 0xff;
		int flags = AMotionEvent_getFlags( event );
		int pointers = AMotionEvent_getPointerCount( event );
		int id = AMotionEvent_getPointerId( event, pointer );

		float x = AMotionEvent_getX( event, pointer );
		float y = AMotionEvent_getY( event, pointer );
		if ( action != 2 ) LOGW("Action: %d, pointer: %d, count: %d, id: %d", action, pointer, pointers, id);
		__android_log_print(ANDROID_LOG_VERBOSE, "test-unknown", "Action: %d, pointer: %d, count: %d, id: %d", action, pointer, pointers, id);
	}
	return 0;
}

int currOrien = 0;

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd)
{
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
        	LOGI("Save State");
            // The system has asked us to save our current state.  Do so.
            engine->app->savedState = malloc(sizeof(struct saved_state));
            *((struct saved_state*)engine->app->savedState) = engine->state;
            engine->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
        	LOGI("Window Init");
            // The window is being shown, get it ready.
            if (engine->app->window != NULL) {
                engine_init_display(engine);
                //engine_draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
        	LOGI("Window Terminate");
            // The window is being hidden or closed, clean it up.
            engine_term_display(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
        	LOGI("Gained Focus");
            // When our app gains focus, we start monitoring the accelerometer.
            if (engine->accelerometerSensor != NULL) {
                ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                        engine->accelerometerSensor);
                // We'd like to get 10 events per second (in us).
                ASensorEventQueue_setEventRate(engine->sensorEventQueue, engine->accelerometerSensor, 100*1000);
            }
            engine->animating = 1;
            resumeapp();
            break;
        case APP_CMD_LOST_FOCUS:
        	LOGI("Lost Focus");
            // When our app loses focus, we stop monitoring the accelerometer.
            // This is to avoid consuming battery while not being used.
            if (engine->accelerometerSensor != NULL) {
                ASensorEventQueue_disableSensor(engine->sensorEventQueue, engine->accelerometerSensor);
            }
            // Also stop animating.
            engine->animating = 0;
            //engine_draw_frame(engine);
            pauseapp();
            break;

       case APP_CMD_CONFIG_CHANGED:
       {
    	   usleep( 100000 );

    	   AConfiguration *config2 = AConfiguration_new();
    	   AConfiguration_fromAssetManager( config2, engine->app->activity->assetManager);
    	   int orien = AConfiguration_getOrientation( config2 );
    	   LOGW("Orientation: %d",orien);
    	   AConfiguration_delete( config2 );

    	   //if ( orien != currOrien )
    	   {
			   if (engine->surface == EGL_NO_SURFACE) break;

			   engine->animating = 0;
			   eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			   eglDestroySurface(engine->display, engine->surface);

			   engine->surface = eglCreateWindowSurface(engine->display, config, engine->app->window, NULL);
			   if (eglMakeCurrent(engine->display, engine->surface, engine->surface, engine->context) == EGL_FALSE)
			   {
				   int error = eglGetError();
				   LOGW("Unable to eglMakeCurrent: %d", eglGetError());
				   break;
			   }

			   EGLint w, h;
			   eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &w);
			   eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &h);

			   engine->width = w;
			   engine->height = h;
			   engine->state.angle = 0;

			   struct egldata data;
			   data.display = engine->display;
			   data.surface = engine->surface;
			   data.context = engine->context;
			   data.activity = engine->app->activity;

			   updateptr2( &data );
//			   devicerotate();
			   engine->animating = 1;
    	   }

    	   currOrien = orien;
    	   break;
       }

    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state) {
    struct engine engine;

    ANativeActivity_setWindowFlags( state->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0 );

    // Make sure glue isn't stripped.
    app_dummy();

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    engine.app = state;

    // Prepare to monitor accelerometer
    engine.sensorManager = ASensorManager_getInstance();
    engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
            ASENSOR_TYPE_ACCELEROMETER);
    engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
            state->looper, LOOPER_ID_USER, NULL, NULL);

    if (state->savedState != NULL) {
        // We are starting with a previous saved state; restore from it.
        engine.state = *(struct saved_state*)state->savedState;
    }

    // loop waiting for stuff to do.

    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.

        while ( (ident=ALooper_pollAll( 0, NULL, &events, (void**)&source)) >= 0 )
        {
            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // If a sensor has data, process it now.
            if (ident == LOOPER_ID_USER)
            {
                if (engine.accelerometerSensor != NULL)
                {
                	ASensorEvent event;
                    while ( ASensorEventQueue_getEvents(engine.sensorEventQueue, &event, 1) > 0)
                    {
                    	//LOGW( "Accel status: %d", event.acceleration.status );
                    	setaccel( event.acceleration.x, event.acceleration.y, event.acceleration.z );
                    }
                }
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0)
            {
                engine_term_display(&engine);
                return;
            }
        }

        if (engine.animating)
        {
            engine_draw_frame(&engine);
        }
        else
        {
        	usleep( 20000 );
        }
    }
}
//END_INCLUDE(all)
