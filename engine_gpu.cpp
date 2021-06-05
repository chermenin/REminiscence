/*
 * REminiscence / REinforced
 * Copyright (C) 2020-2021 Alex Chermenin (alex@chermenin.ru)
 */

#include <SDL2/SDL_gpu.h>
#include "resource.h"
#include "scaler.h"
#include "screenshot.h"
#include "engine.h"
#include "util.h"


static const uint32_t kPixelFormat = SDL_PIXELFORMAT_ABGR8888;

struct Engine_GPU : Engine {
	GPU_Target *_screen;
	GPU_Image *_image;
	GPU_Image *_widescreenImage;

	virtual ~Engine_GPU() {}
	virtual void initGraphics();
	virtual void copyWidescreenLeft(int w, int h, const uint8_t *buf, bool dark = true);
	virtual void copyWidescreenRight(int w, int h, const uint8_t *buf, bool dark = true);
	virtual void copyWidescreenMirror(int w, int h, const uint8_t *buf);
	virtual void copyWidescreenBlur(int w, int h, const uint8_t *buf);
	virtual void clearWidescreen();
	virtual void updateScreen(int shakeOffset);
	virtual void prepareGraphics();
	virtual void cleanupGraphics();

	virtual void saveScreenshot(char *filename);
};

Engine *Engine_GPU_create() {
	return new Engine_GPU();
}

void Engine_GPU::initGraphics() {
	_screen = 0;
	_image = 0;
	_widescreenImage = 0;
	_fmt = SDL_AllocFormat(kPixelFormat);
}

void Engine_GPU::copyWidescreenLeft(int w, int h, const uint8_t *buf, bool dark) {
	assert(w >= _wideMargin);
	uint32_t *rgb = (uint32_t *)malloc(_wideMargin * h * sizeof(uint32_t));
	const int offset = w - _wideMargin;
	if (buf) {
		for (int y = 0; y < h; ++y) {
			
			for (int x = offset; x < w; ++x) {
				if (dark) {
					rgb[y * _wideMargin + x - offset] = _darkPalette[buf[y * w + x]];
				} else {
					rgb[y * _wideMargin + x - offset] = _shadowPalette[buf[y * w + x]];
				}
			}
		}
	} else {
		const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
		for (int i = 0; i < _wideMargin * h; ++i) {
			rgb[i] = color;
		}
	}

	if (!dark) {
		uint32_t *tmp = (uint32_t *)malloc(_wideMargin * h * sizeof(uint32_t));

		if (rgb && tmp) {
			static const int radius = 2;
			blurH(radius, rgb, _wideMargin, _wideMargin, h, _fmt, tmp, _wideMargin);
			blurV(radius, tmp, _wideMargin, _wideMargin, h, _fmt, rgb, _wideMargin);
		}

		free(tmp);
	}

	GPU_Rect r = GPU_MakeRect(0, 0, _wideMargin, h);
	GPU_UpdateImageBytes(_widescreenImage, &r, (unsigned char *)rgb, _wideMargin * sizeof(uint32_t));
	free(rgb);
}

void Engine_GPU::copyWidescreenRight(int w, int h, const uint8_t *buf, bool dark) {
	assert(w >= _wideMargin);
	uint32_t *rgb = (uint32_t *)malloc(_wideMargin * h * sizeof(uint32_t));
	if (buf) {
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < _wideMargin; ++x) {
				if (dark) {
					rgb[y * _wideMargin + x] = _darkPalette[buf[y * w + x]];
				} else {
					rgb[y * _wideMargin + x] = _shadowPalette[buf[y * w + x]];
				}
			}
		}
	} else {
		const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
		for (int i = 0; i < _wideMargin * h; ++i) {
			rgb[i] = color;
		}
	}

	if (!dark) {
		uint32_t *tmp = (uint32_t *)malloc(_wideMargin * h * sizeof(uint32_t));

		if (rgb && tmp) {
			static const int radius = 2;
			blurH(radius, rgb, _wideMargin, _wideMargin, h, _fmt, tmp, _wideMargin);
			blurV(radius, tmp, _wideMargin, _wideMargin, h, _fmt, rgb, _wideMargin);
		}

		free(tmp);
	}

	GPU_Rect r = GPU_MakeRect(_wideMargin + _screenW, 0, _wideMargin, h);
	GPU_UpdateImageBytes(_widescreenImage, &r, (unsigned char *)rgb, _wideMargin * sizeof(uint32_t));
	free(rgb);
}

void Engine_GPU::copyWidescreenMirror(int w, int h, const uint8_t *buf) {
	assert(w >= _wideMargin);
	int wideW = _screenW + 2 * _wideMargin;
	uint32_t *rgb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	uint32_t *dst = (uint32_t *)malloc(wideW * h * sizeof(uint32_t));
	if (rgb && dst) {
		for (int i = 0; i < w * h; ++i) {
			rgb[i] = _darkPalette[buf[i]];
		}
		uint32_t *p = dst;
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < _wideMargin; ++x) {
				// left side
				const int xLeft = _wideMargin - 1 - x;
				p[x] = rgb[y * w + xLeft];

				// right side
				const int xRight = w - 1 - x;
				p[_wideMargin + _screenW + x] = rgb[y * w + xRight];
			}
			p += wideW;
		}
	}

	GPU_UpdateImageBytes(_widescreenImage, 0, (unsigned char *)dst, wideW * sizeof(uint32_t));

	free(dst);
	free(rgb);
}

void Engine_GPU::copyWidescreenBlur(int w, int h, const uint8_t *buf) {
	assert(w == _screenW && h == _screenH);

	uint32_t *src = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	uint32_t *dst = (uint32_t *)malloc(w * h * sizeof(uint32_t));

	if (src && tmp && dst) {
		for (int i = 0; i < w * h; ++i) {
			src[i] = _rgbPalette[buf[i]];
		}
		static const int radius = 8;
		blurH(radius, src, w, w, h, _fmt, tmp, w);
		blurV(radius, tmp, w, w, h, _fmt, dst, w);
	}

	GPU_UpdateImageBytes(_widescreenImage, 0, (unsigned char *)dst, w * sizeof(uint32_t));

	free(src);
	free(tmp);
	free(dst);
}

void Engine_GPU::clearWidescreen() {
	// @todo: clear widescreen image?
}

void Engine_GPU::updateScreen(int shakeOffset) {
	if (_texW != _screenW || _texH != _screenH) {
		uint32_t *dst = (uint32_t *)malloc(_texW * _texH * sizeof(uint32_t));
		_scaler->scale(_scaleFactor, (uint32_t *)dst, _texW, _screenBuffer, _screenW, _screenW, _screenH);
		GPU_UpdateImageBytes(_image, 0, (unsigned char *)dst, _texW * sizeof(uint32_t));
		free(dst);
	} else {
		GPU_UpdateImageBytes(_image, 0, (unsigned char *)_screenBuffer, _screenW * sizeof(uint32_t));
	}
	GPU_Clear(_screen);
	if (_widescreenMode != kWidescreenNone) {
		if (_enableWidescreen) {
			// borders / background screen
			int h = (_widescreenMode == kWidescreenBlur) ? _screen->h * _screen->w / _texW : _texH;
			GPU_Rect r = GPU_MakeRect(0, 0, _screen->w, _screen->h);
			GPU_BlitRect(_widescreenImage, NULL, _screen, &r);
		}
		// game screen
		GPU_Rect r = GPU_MakeRect((_screen->w - _texW) / 2 - 1, 0, _texW + 2, _screen->h);
		GPU_BlitRect(_image, NULL, _screen, &r);
	} else {
		if (_fadeOnUpdateScreen) {
			GPU_SetShapeBlendMode(GPU_BLEND_PREMULTIPLIED_ALPHA);
			for (int i = 1; i <= 16; ++i) {
				GPU_Blit(_image, NULL, _screen,  _screen->w / 2, _screen->h / 2);
				GPU_RectangleFilled(_screen, 0, 0, _screen->w, _screen->h, GPU_MakeColor(0, 0, 0, 256 - i * 16));
				GPU_Flip(_screen);
				SDL_Delay(30);
			}
			_fadeOnUpdateScreen = false;
			GPU_SetShapeBlendMode(GPU_BLEND_NORMAL);
			return;
		}
		GPU_Blit(_image, NULL, _screen, _screen->w / 2, _screen->h / 2);
	}
	GPU_Flip(_screen);
}

void Engine_GPU::prepareGraphics() {
	_texW = _screenW;
	_texH = _screenH;
	if (_scalerType == kScalerTypeInternal || _scalerType == kScalerTypeExternal) {
		_texW *= _scaleFactor;
		_texH *= _scaleFactor;
	}
	int windowW = _screenW * _scaleFactor;
	int windowH = _screenH * _scaleFactor;
	int flags = SDL_WINDOW_OPENGL;
	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	if (_widescreenMode != kWidescreenNone) {
		if (_fullscreen) {
			SDL_DisplayMode current;
			SDL_GetCurrentDisplayMode(0, &current);
			windowW = windowH * current.w / current.h;
		} else {
			windowW = windowH * 16 / 9;
		}
	}
	_window = SDL_CreateWindow(_caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	GPU_SetInitWindow(SDL_GetWindowID(_window));
	_screen = GPU_Init(windowW, windowH, GPU_DEFAULT_INIT_FLAGS);
	_image = GPU_CreateImage(_texW, _texH, GPU_FORMAT_RGBA);
	if (_widescreenMode != kWidescreenNone) {
		// in blur mode, the background texture has the same dimensions as the game texture
		const int w = (_widescreenMode == kWidescreenBlur) ? _screenW : _screenH * windowW / windowH;
		const int h = _screenH;
		_widescreenImage = GPU_CreateImage(w, h, GPU_FORMAT_RGBA);

		// left and right borders
		_wideMargin = (w - _screenW) / 2;
	}
	if (_scalerType == kScalerTypePoint) {
		GPU_SetImageFilter(_image, GPU_FILTER_NEAREST);
		if (_widescreenImage) {
			GPU_SetImageFilter(_widescreenImage, GPU_FILTER_NEAREST);
		}
	} else {
		GPU_SetImageFilter(_image, GPU_FILTER_LINEAR);
		if (_widescreenImage) {
			GPU_SetImageFilter(_widescreenImage, GPU_FILTER_LINEAR);
		}
	}
}

void Engine_GPU::cleanupGraphics() {
	if (_image) {
		GPU_FreeImage(_image);
		_image = 0;
	}
	if (_widescreenImage) {
		GPU_FreeImage(_widescreenImage);
		_widescreenImage = 0;
	}
	if (_screen) {
		GPU_FreeTarget(_screen);
		GPU_CloseCurrentRenderer();
		_screen = 0;
	}
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
}

void Engine_GPU::saveScreenshot(char *filename) {
	GPU_SaveImage(_image, filename, GPU_FILE_TGA);
}
