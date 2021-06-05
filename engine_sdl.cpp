/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include "resource.h"
#include "scaler.h"
#include "screenshot.h"
#include "engine.h"
#include "util.h"

static const uint32_t kPixelFormat = SDL_PIXELFORMAT_RGB888;

struct Engine_SDL : Engine {
	SDL_Renderer *_renderer;
	SDL_Texture *_texture;
	SDL_Texture *_widescreenTexture;

	virtual ~Engine_SDL() {}
	virtual void initGraphics();
	virtual void copyWidescreenLeft(int w, int h, const uint8_t *buf, bool blur = false);
	virtual void copyWidescreenRight(int w, int h, const uint8_t *buf, bool blur = false);
	virtual void copyWidescreenMirror(int w, int h, const uint8_t *buf, bool blur = false);
	virtual void copyWidescreenBlur(int w, int h, const uint8_t *buf);
	virtual void clearWidescreen();
	virtual void updateScreen(int shakeOffset);
	virtual void prepareGraphics();
	virtual void cleanupGraphics();

	virtual void saveScreenshot(char *filename);
};

Engine *Engine_SDL_create() {
	return new Engine_SDL();
}

void Engine_SDL::initGraphics() {
	_renderer = 0;
	_texture = 0;
	_widescreenTexture = 0;
	_fmt = SDL_AllocFormat(kPixelFormat);
}

static void clearTexture(SDL_Texture *texture, int h, SDL_PixelFormat *fmt) {
	void *dst = 0;
	int pitch = 0;
	if (SDL_LockTexture(texture, 0, &dst, &pitch) == 0) {
		assert((pitch & 3) == 0);
		const uint32_t color = SDL_MapRGB(fmt, 0, 0, 0);
		for (uint32_t i = 0; i < h * pitch / sizeof(uint32_t); ++i) {
			((uint32_t *)dst)[i] = color;
		}
		SDL_UnlockTexture(texture);
	}
}

void Engine_SDL::copyWidescreenLeft(int w, int h, const uint8_t *buf, bool blur) {
	assert(w >= _wideMargin);
	uint32_t *rgb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	if (rgb) {
		if (buf) {
			if (blur) {
				for (int i = 0; i < w * h; ++i) {
					rgb[i] = _shadowPalette[buf[i]];
				}
			} else {
				for (int i = 0; i < w * h; ++i) {
					rgb[i] = _darkPalette[buf[i]];
				}
			}
		} else {
			const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = color;
			}
		}

		if (blur) {
			uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));

			if (rgb && tmp) {
				static const int radius = 2;
				blurH(radius, rgb, w, w, h, _fmt, tmp, w);
				blurV(radius, tmp, w, w, h, _fmt, rgb, w);
			}

			free(tmp);
		}

		const int xOffset = w - _wideMargin;
		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = _wideMargin;
		r.h = h;
		SDL_UpdateTexture(_widescreenTexture, &r, rgb + xOffset, w * sizeof(uint32_t));
		free(rgb);
	}
}

void Engine_SDL::copyWidescreenRight(int w, int h, const uint8_t *buf, bool blur) {
	assert(w >= _wideMargin);
	uint32_t *rgb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	if (rgb) {
		if (buf) {
			if (blur) {
				for (int i = 0; i < w * h; ++i) {
					rgb[i] = _shadowPalette[buf[i]];
				}
			} else {
				for (int i = 0; i < w * h; ++i) {
					rgb[i] = _darkPalette[buf[i]];
				}
			}
		} else {
			const uint32_t color = SDL_MapRGB(_fmt, 0, 0, 0);
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = color;
			}
		}

		if (blur) {
			uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));

			if (rgb && tmp) {
				static const int radius = 2;
				blurH(radius, rgb, w, w, h, _fmt, tmp, w);
				blurV(radius, tmp, w, w, h, _fmt, rgb, w);
			}

			free(tmp);
		}

		const int xOffset = 0;
		SDL_Rect r;
		r.x = _wideMargin + _screenW;
		r.y = 0;
		r.w = _wideMargin;
		r.h = h;
		SDL_UpdateTexture(_widescreenTexture, &r, rgb + xOffset, w * sizeof(uint32_t));
		free(rgb);
	}
}

void Engine_SDL::copyWidescreenMirror(int w, int h, const uint8_t *buf, bool blur) {
	assert(w >= _wideMargin);
	uint32_t *rgb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
	if (rgb) {
		if (blur) {
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = _shadowPalette[buf[i]];
			}
		} else {
			for (int i = 0; i < w * h; ++i) {
				rgb[i] = _darkPalette[buf[i]];
			}
		}

		if (blur) {
			uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));

			if (rgb && tmp) {
				static const int radius = 2;
				blurH(radius, rgb, w, w, h, _fmt, tmp, w);
				blurV(radius, tmp, w, w, h, _fmt, rgb, w);
			}

			free(tmp);
		}

		void *dst = 0;
		int pitch = 0;
		if (SDL_LockTexture(_widescreenTexture, 0, &dst, &pitch) == 0) {
			assert((pitch & 3) == 0);
			uint32_t *p = (uint32_t *)dst;
			for (int y = 0; y < h; ++y) {
				for (int x = 0; x < _wideMargin; ++x) {
					// left side
					const int xLeft = _wideMargin - 1 - x;
					p[x] = rgb[y * w + xLeft];
					// right side
					const int xRight = w - 1 - x;
					p[_wideMargin + _screenW + x] = rgb[y * w + xRight];
				}
				p += pitch / sizeof(uint32_t);
			}
			SDL_UnlockTexture(_widescreenTexture);
		}
		free(rgb);
	}
}

void Engine_SDL::copyWidescreenBlur(int w, int h, const uint8_t *buf) {
	assert(w == _screenW && h == _screenH);
	void *ptr = 0;
	int pitch = 0;
	if (SDL_LockTexture(_widescreenTexture, 0, &ptr, &pitch) == 0) {
		assert((pitch & 3) == 0);

		uint32_t *src = (uint32_t *)malloc(w * h * sizeof(uint32_t));
		uint32_t *tmp = (uint32_t *)malloc(w * h * sizeof(uint32_t));
		uint32_t *dst = (uint32_t *)ptr;

		if (src && tmp) {
			for (int i = 0; i < w * h; ++i) {
				src[i] = _rgbPalette[buf[i]];
			}
			static const int radius = 8;
			blurH(radius, src, w, w, h, _fmt, tmp, w);
			blurV(radius, tmp, w, w, h, _fmt, dst, pitch / sizeof(uint32_t));
		}

		free(src);
		free(tmp);

		SDL_UnlockTexture(_widescreenTexture);
	}
}

void Engine_SDL::clearWidescreen() {
	clearTexture(_widescreenTexture, _screenH, _fmt);
}

void Engine_SDL::updateScreen(int shakeOffset) {
	if (_texW != _screenW || _texH != _screenH) {
		void *dst = 0;
		int pitch = 0;
		if (SDL_LockTexture(_texture, 0, &dst, &pitch) == 0) {
			assert((pitch & 3) == 0);
			_scaler->scale(_scaleFactor, (uint32_t *)dst, pitch / sizeof(uint32_t), _screenBuffer, _screenW, _screenW, _screenH);
			SDL_UnlockTexture(_texture);
		}
	} else {
		SDL_UpdateTexture(_texture, 0, _screenBuffer, _screenW * sizeof(uint32_t));
	}
	SDL_RenderClear(_renderer);
	if (_widescreenMode != kWidescreenNone) {
		if (_enableWidescreen) {
			// borders / background screen
			SDL_RenderCopy(_renderer, _widescreenTexture, 0, 0);
		}
		// game screen
		SDL_Rect r;
		r.y = shakeOffset * _scaleFactor;
		SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
		r.x = (r.w - _texW) / 2;
		r.w = _texW;
		SDL_RenderCopy(_renderer, _texture, 0, &r);
	} else {
		if (_fadeOnUpdateScreen) {
			SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
			SDL_Rect r;
			r.x = r.y = 0;
			SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
			for (int i = 1; i <= 16; ++i) {
				SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 256 - i * 16);
				SDL_RenderCopy(_renderer, _texture, 0, 0);
				SDL_RenderFillRect(_renderer, &r);
				SDL_RenderPresent(_renderer);
				SDL_Delay(30);
			}
			_fadeOnUpdateScreen = false;
			SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_NONE);
			return;
		}
		SDL_Rect r;
		r.x = 0;
		r.y = shakeOffset * _scaleFactor;
		SDL_RenderGetLogicalSize(_renderer, &r.w, &r.h);
		SDL_RenderCopy(_renderer, _texture, 0, &r);
	}
	SDL_RenderPresent(_renderer);
}

void Engine_SDL::prepareGraphics() {
	_texW = _screenW;
	_texH = _screenH;
	switch (_scalerType) {
	case kScalerTypePoint:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // nearest pixel sampling
		break;
	case kScalerTypeLinear:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // linear filtering
		break;
	case kScalerTypeInternal:
	case kScalerTypeExternal:
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
		_texW *= _scaleFactor;
		_texH *= _scaleFactor;
		break;
	}
	int windowW = _screenW * _scaleFactor;
	int windowH = _screenH * _scaleFactor;
	int flags = 0;
	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		flags |= SDL_WINDOW_RESIZABLE;
	}
	if (_widescreenMode != kWidescreenNone) {
		windowW = windowH * 16 / 9;
	}
	_window = SDL_CreateWindow(_caption, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, flags);
	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderSetLogicalSize(_renderer, windowW, windowH);
	_texture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, _texW, _texH);
	if (_widescreenMode != kWidescreenNone) {
		// in blur mode, the background texture has the same dimensions as the game texture
		// SDL stretches the texture to 16:9
		const int w = (_widescreenMode == kWidescreenBlur) ? _screenW : _screenH * 16 / 9;
		const int h = _screenH;
		_widescreenTexture = SDL_CreateTexture(_renderer, kPixelFormat, SDL_TEXTUREACCESS_STREAMING, w, h);
		clearTexture(_widescreenTexture, _screenH, _fmt);

		// left and right borders
		_wideMargin = (w - _screenW) / 2;
	}
}

void Engine_SDL::cleanupGraphics() {
	if (_texture) {
		SDL_DestroyTexture(_texture);
		_texture = 0;
	}
	if (_widescreenTexture) {
		SDL_DestroyTexture(_widescreenTexture);
		_widescreenTexture = 0;
	}
	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = 0;
	}
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = 0;
	}
}

void Engine_SDL::saveScreenshot(char *filename) {
	saveTGA(filename, (const uint8_t *)_screenBuffer, _screenW, _screenH);
}
