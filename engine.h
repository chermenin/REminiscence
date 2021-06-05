
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef ENGINE_H__
#define ENGINE_H__

#include <SDL.h>
#include "intern.h"
#include "scaler.h"

struct PlayerInput {
	enum {
		DIR_UP    = 1 << 0,
		DIR_DOWN  = 1 << 1,
		DIR_LEFT  = 1 << 2,
		DIR_RIGHT = 1 << 3
	};

	enum {
		DF_FASTMODE = 1 << 0,
		DF_DBLOCKS  = 1 << 1,
		DF_SETLIFE  = 1 << 2
	};

	uint8_t dirMask;
	bool enter;
	bool space;
	bool shift;
	bool backspace;
	bool escape;

	char lastChar;

	bool save;
	bool load;
	int stateSlot;
	bool rewind;

	uint8_t dbgMask;
	bool quit;
};

struct ScalerParameters {
	ScalerType type;
	char name[32];
	int factor;

	static ScalerParameters defaults();
};

struct Engine {
	typedef void (*AudioCallback)(void *param, int16_t *stream, int len);

	PlayerInput _pi;

	SDL_Window *_window;
	int _texW, _texH;
	SDL_GameController *_controller;
	SDL_PixelFormat *_fmt;
	const char *_caption;
	uint32_t *_screenBuffer;
	bool _fullscreen;
	uint8_t _overscanColor;
	uint32_t _rgbPalette[256];
	uint32_t _shadowPalette[256];
	uint32_t _darkPalette[256];
	int _screenW, _screenH;
	SDL_Joystick *_joystick;
	bool _fadeOnUpdateScreen;
	void (*_audioCbProc)(void *, int16_t *, int);
	void *_audioCbData;
	int _screenshot;
	ScalerType _scalerType;
	int _scaleFactor;
	const Scaler *_scaler;
	void *_scalerSo;
	int _widescreenMode;
	int _wideMargin;
	bool _enableWidescreen;

	virtual ~Engine() {}

	void init(const char *title, int w, int h, bool fullscreen, int widescreenMode, const ScalerParameters *scalerParameters);
	void destroy();

	bool hasWidescreen() const;
	void setScreenSize(int w, int h);
	void setPalette(const uint8_t *pal, int n);
	void getPalette(uint8_t *pal, int n);
	void setPaletteEntry(int i, const Color *c);
	void getPaletteEntry(int i, Color *c);
	void setOverscanColor(int i);
	void enableWidescreen(bool enable);
	void fadeScreen();

	void processEvents();
	void sleep(int duration);
	uint32_t getTimeStamp();

	void startAudio(AudioCallback callback, void *param);
	void stopAudio();
	uint32_t getOutputSampleRate();
	void lockAudio();
	void unlockAudio();

	void setPaletteColor(int color, int r, int g, int b);
	void processEvent(const SDL_Event &ev, bool &paused);
	void changeGraphics(bool fullscreen, int scaleFactor);
	void setScaler(const ScalerParameters *parameters);
	void changeScaler(int scalerNum);
	void drawRect(int x, int y, int w, int h, uint8_t color);
	void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch);
	void copyRectRgb24(int x, int y, int w, int h, const uint8_t *rgb);
	void blurH(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch);
	void blurV(int radius, const uint32_t *src, int srcPitch, int w, int h, const SDL_PixelFormat *fmt, uint32_t *dst, int dstPitch);

	virtual void initGraphics() = 0;
	virtual void copyWidescreenLeft(int w, int h, const uint8_t *buf, bool blur = false) = 0;
	virtual void copyWidescreenRight(int w, int h, const uint8_t *buf, bool blur = false) = 0;
	virtual void copyWidescreenMirror(int w, int h, const uint8_t *buf, bool blur = false) = 0;
	virtual void copyWidescreenBlur(int w, int h, const uint8_t *buf) = 0;
	virtual void clearWidescreen() = 0;
	virtual void updateScreen(int shakeOffset) = 0;
	virtual void prepareGraphics() = 0;
	virtual void cleanupGraphics() = 0;

	virtual void saveScreenshot(char *filename) = 0;
};

struct LockAudioStack {
	LockAudioStack(Engine *engine)
		: _engine(engine) {
		_engine->lockAudio();
	}
	~LockAudioStack() {
		_engine->unlockAudio();
	}
	Engine *_engine;
};

extern Engine *Engine_SDL_create();
extern Engine *Engine_GPU_create();

#endif // ENGINE_H__
