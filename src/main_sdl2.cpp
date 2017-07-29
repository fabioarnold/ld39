#include <assert.h>
#include <time.h> // used by log
#include <math.h> // for fabsf
#include <float.h> // for FLT_MAX
#include <stdlib.h>
#include <unistd.h>

// SDL2
#include <SDL.h>
#ifdef USE_GLEW
	#include <GL/glew.h>
#endif
#ifdef USE_OPENGLES
	#include <SDL_opengles2.h>
	#include <GLES2/gl2extimg.h> // texture compression ext
#else
	#define GL_GLEXT_PROTOTYPES
	#include <SDL_opengl.h>
	#ifndef __EMSCRIPTEN__
		#include <SDL_opengl_glext.h>
	#endif
#endif

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
#endif 

// fontstash
#include <fontstash.h>

// ImGui
#ifdef DEBUG
	#include <imgui.h>
	#include "imgui_impl_sdl2_gl2.h" // the impl we want to use
#endif

// gamelib
#include <system/defines.h>
#include <system/files.h>
#include <system/log.h>
#include <system/frametime.h>
#include <math/vector_math.h>
#include <math/transform.h>
#include <input/input.h>
#include <video/video_mode.h>
#include <video/camera.h>
#include <video/shader.h>
#include <video/debug_renderer.h>
#include <video/image.h>
#include <video/texture.h>
#include <video/model_mdl.h>

#include <math/transform.cpp>
#include <system/files.cpp>
#include <system/log.cpp>
#include <system/frametime_sdl2.cpp>
#include <video/camera.cpp>
#include <video/shader.cpp>
#include <video/debug_renderer.cpp>
#include <video/image.cpp>
#include <video/texture.cpp>
#include <video/texture_null.cpp>
#include <video/model_mdl.cpp>



#include "game.h"

#include "game.cpp"

const char *WINDOW_TITLE = "Ludum Dare 39";
SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;
int sdl_pixel_size; // used to immitate OS X pixel doubling behavior

/* inits sdl, sdl_net and creates an opengl window */
static void initSDL(VideoMode *video) {
	Uint32 sdl_init_flags =
		  SDL_INIT_VIDEO
		| SDL_INIT_AUDIO
		| SDL_INIT_JOYSTICK
		/*| SDL_INIT_GAMECONTROLLER*/;
	if (SDL_Init(sdl_init_flags) < 0) {
		LOGE("SDL_Init: %s", SDL_GetError());
		exit(1);
	}

#if 0
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#endif

#ifndef __EMSCRIPTEN__
	#ifdef USE_OPENGLES
		// gles 2.0
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
							SDL_GL_CONTEXT_PROFILE_ES);
	#else
		// opengl 2.1
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
							SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	#endif
#endif

	u32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
	if (video->fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;

	/*
	currently SDL_WINDOW_ALLOW_HIGHDPI only doubles pixels on apple platforms
	this will copy the behavior for other platforms
	*/
	sdl_pixel_size = 1;
#ifndef __APPLE__
	if (window_flags & SDL_WINDOW_ALLOW_HIGHDPI) {
		float dpi;
		SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr);
		if (dpi > 1.5f * 72.0f) {
			sdl_pixel_size = 2;
			LOGI("High DPI display detected. Scaling pixel with factor %d", sdl_pixel_size);
		}
	}
#endif

	sdl_window = SDL_CreateWindow(WINDOW_TITLE,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		sdl_pixel_size * video->width, sdl_pixel_size * video->height,
		window_flags);
	if (!sdl_window) {
		LOGE("SDL_CreateWindow: %s", SDL_GetError());
		exit(1);
	}
	if (video->fullscreen) SDL_ShowCursor(SDL_DISABLE); // hide mouse cursor

	sdl_gl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_gl_context) {
		LOGE("SDL_GL_CreateContext: %s", SDL_GetError());
		exit(1);
	}

	int drawable_width, drawable_height;
	SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
	if (drawable_width != video->width) {
		video->pixel_scale = (float)drawable_width / (float)video->width;
	} else {
		video->pixel_scale = 1.0f;
	}

#ifndef RASPBERRYPI // vsync kills rpi performance
	if (SDL_GL_SetSwapInterval(1) == -1) { // sync with monitor refresh rate
		LOGW("Could not enable VSync. %s", SDL_GetError());
	}
#endif

#ifdef USE_GLEW
	glewInit();
#endif
}

void quitSDL() {
	SDL_GL_DeleteContext(sdl_gl_context);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();
}



SDL_Joystick *sdl_joysticks[ARRAY_COUNT(gamepads)];
int sdlJoystickGetGamepadIndexFromDeviceID(int device_id) {
	// find our previously opened joystick
	// it seems SDL_Joystick is simply an sint32 which is our previously used device_id
	// not sure if this will always be the case but on OS X it works...
	for (int gi = 0; gi < (int)ARRAY_COUNT(gamepads); gi++) {
		if (sdl_joysticks[gi] && *(int*)(sdl_joysticks[gi]) == device_id) {
			return gi;
		}
	}
	LOGE("matching gamepad to device id (%d) not found", device_id);
	return -1;
}
void initJoysticks() {
	memset(sdl_joysticks, 0, sizeof(sdl_joysticks));
	memset(gamepads, 0, sizeof(gamepads));
}
void sdlJoystickAdd(int device_id) {
	// scan for free slot
	for (int i = 0; i < (int)ARRAY_COUNT(sdl_joysticks); i++) {
		if (!sdl_joysticks[i]) {
			sdl_joysticks[i] = SDL_JoystickOpen(device_id);
			gamepads[i].plugged_in = true;
			gamepads[i].active = false; // no buttons pressed, yet
			gamepads[i].name = SDL_JoystickName(sdl_joysticks[i]);
			LOGI("gamepad #%d (%s) plugged in (device: %d)", i, gamepads[i].name, device_id);
			return;
		}
	}
	LOGE("too many gamepads plugged in");
}
void sdlJoystickRemove(int device_id) {
	int gamepad_idx = sdlJoystickGetGamepadIndexFromDeviceID(device_id);
	if (gamepad_idx == -1) {
		LOGE("tried to unplug already unplugged gamepad");
		return;
	}

	SDL_JoystickClose(sdl_joysticks[gamepad_idx]);
	sdl_joysticks[gamepad_idx] = nullptr;
	gamepads[gamepad_idx].plugged_in = false;
	gamepads[gamepad_idx].active = false;
	LOGI("gamepad #%d unplugged", gamepad_idx);
}

// maps joystick buttons to game related controls
void sdlJoystickProcessButton(int device_id, int button_id, bool button_down) {
	int gamepad_idx = sdlJoystickGetGamepadIndexFromDeviceID(device_id);
	if (gamepad_idx == -1) {
		LOGE("received joystick button event for unknown gamepad");
		return;
	}

	gamepads[gamepad_idx].onButton(button_id, button_down);
	//LOGI("button %d pressed", button_id);
}

void sdlJoystickProcessAxis(int device_id, int axis_id, s16 axis_value_s16) {
	int gamepad_idx = sdlJoystickGetGamepadIndexFromDeviceID(device_id);
	if (gamepad_idx == -1) {
		LOGE("received joystick axis event for unknown gamepad");
		return;
	}

	// convert axis_value to float
	float divider = (float)(axis_value_s16 > 0 ? (1<<15)-1 : (1<<15));
	float axis_value = (float)axis_value_s16 / divider;
	gamepads[gamepad_idx].onAxis(axis_id, axis_value);
}



void FrameTime::drawInfo() {
#ifdef DEBUG
	char fps_text[64];
	sprintf(fps_text, "FPS: %d\nframe time: %.3f ms", frames_per_second, 1000.0*smoothed_frame_time);
	ImGui::Text("%s", fps_text);
#endif
}

Game *game;

void mainLoop() {
	keyboard.beginFrame();
	for (int gi = 0; gi < (int)ARRAY_COUNT(gamepads); gi++) {
		gamepads[gi].beginFrame();
	}

	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
#ifdef DEBUG
		ImGui_ImplSdlGL2_ProcessEvent(&sdl_event);
#endif
		switch (sdl_event.type) {
		case SDL_WINDOWEVENT:
			switch (sdl_event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				game->video.width  = sdl_event.window.data1 / sdl_pixel_size;
				game->video.height = sdl_event.window.data2 / sdl_pixel_size;
				{ // update opengl viewport
					int drawable_width, drawable_height;
					SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
					glViewport(0, 0, drawable_width, drawable_height);
				}
				break;
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			keyboard.onKey(sdl_event.key.keysym.scancode,
				sdl_event.key.state == SDL_PRESSED);
			game->quit = sdl_event.key.keysym.sym == SDLK_ESCAPE;
			break;
		case SDL_JOYDEVICEADDED:
			sdlJoystickAdd(sdl_event.jdevice.which);
			break;
		case SDL_JOYDEVICEREMOVED:
			sdlJoystickRemove(sdl_event.jdevice.which);
			break;
		case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP:
			sdlJoystickProcessButton(sdl_event.jbutton.which, sdl_event.jbutton.button, sdl_event.jbutton.state == SDL_PRESSED);
			break;
		case SDL_JOYAXISMOTION:
			sdlJoystickProcessAxis(sdl_event.jaxis.which, sdl_event.jaxis.axis, sdl_event.jaxis.value);
			break;
		case SDL_QUIT:
			game->quit = true;
			break;
		}
	}

#ifdef DEBUG
	ImGui_ImplSdlGL2_NewFrame();
#endif

	game->tick((float)frametime.smoothed_frame_time);

#ifdef DEBUG
	frametime.drawInfo();
	ImGui::Render();
#endif

	SDL_GL_SwapWindow(sdl_window);
	frametime.update();
}

int main(int argc, char *argv[]) {
	game = new Game();

	// reasonable defaults
	game->video.width = 1024;
	game->video.height = 640;
	game->video.fullscreen = false;
#ifdef USE_OPENGLES
	game->video.fullscreen = true;
#endif

	initSDL(&game->video);
	initJoysticks();
	// debug stuff
#ifdef DEBUG
	ImGui_ImplSdlGL2_Init(sdl_window);
#endif
	debug_renderer.init();

	// init default key bindings
	/*
	keyboard.bind(SDL_SCANCODE_A, &game->player_controls[0].button_aim_left);
	keyboard.bind(SDL_SCANCODE_D, &game->player_controls[0].button_aim_right);
	keyboard.bind(SDL_SCANCODE_W, &game->player_controls[0].button_aim_up);
	keyboard.bind(SDL_SCANCODE_S, &game->player_controls[0].button_aim_down);
	keyboard.bind(SDL_SCANCODE_LEFT,  &game->player_controls[0].button_move_left);
	keyboard.bind(SDL_SCANCODE_RIGHT, &game->player_controls[0].button_move_right);
	keyboard.bind(SDL_SCANCODE_UP,    &game->player_controls[0].button_move_up);
	keyboard.bind(SDL_SCANCODE_DOWN,  &game->player_controls[0].button_move_down);
	keyboard.bind(SDL_SCANCODE_SPACE, &game->player_controls[0].button_fire);
	game->player_controls[0].use_keyboard = true;
	*/

	// enable OpenGL alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	game->init();

	// init this last for sake of last_ticks
	frametime.init();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(mainLoop, 0, 1);
#else
	do {
		Uint32 beginTicks = SDL_GetTicks();
		mainLoop();
		// hack for when vsync isn't working
		Uint32 elapsedTicks = SDL_GetTicks() - beginTicks;
		if (elapsedTicks < 16) {
			SDL_Delay(16 - elapsedTicks);
		}
	} while (!game->quit);
#endif

	game->destroy();
	debug_renderer.destroy();
#ifdef DEBUG
	ImGui_ImplSdlGL2_Shutdown();
#endif

	quitSDL();
}
