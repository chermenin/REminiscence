
/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef GRAPHICS_H__
#define GRAPHICS_H__

#include "intern.h"

struct Graphics {
	static const int AREA_POINTS_SIZE = 256 * 2; // maxY * sizeof(Point) / sizeof(int16_t)
	uint8_t *_layer;
	int _layerPitch;
	int16_t _areaPoints[AREA_POINTS_SIZE * 2];
	int16_t _crx, _cry, _crw, _crh;

	void setLayer(uint8_t *layer, int pitch);
	void setClippingRect(int16_t vx, int16_t vy, int16_t vw, int16_t vh);
	void drawPoint(uint8_t color, const Point *pt);
	void drawLine(uint8_t color, const Point *pt1, const Point *pt2);
	void addEllipseRadius(int16_t y, int16_t x1, int16_t x2);
	void drawEllipse(uint8_t color, bool hasAlpha, const Point *pt, int16_t rx, int16_t ry);
	void fillArea(uint8_t color, bool hasAlpha);
	void drawSegment(uint8_t color, bool hasAlpha, int16_t ys, const Point *pts, uint8_t numPts);
	void drawPolygonOutline(uint8_t color, const Point *pts, uint8_t numPts);
	void drawPolygon(uint8_t color, bool hasAlpha, const Point *pts, uint8_t numPts);
};

#endif // GRAPHICS_H__
