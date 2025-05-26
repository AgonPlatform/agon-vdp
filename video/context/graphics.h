#ifndef CONTEXT_GRAPHICS_H
#define CONTEXT_GRAPHICS_H

#include <algorithm>
#include <vector>

#include <fabgl.h>

#include "agon.h"
#include "agon_ps2.h"
#include "agon_screen.h"
#include "agon_palette.h"
#include "agon_ttxt.h"
#include "buffers.h"
#include "sprites.h"
#include "types.h"
#include "mat.h"

// Definitions for the functions we're implementing here
#include "context.h"

extern bool ttxtMode;							// Teletext mode
extern agon_ttxt ttxt_instance;					// Teletext instance


// Private graphics functions

// Get the paint options for a given GCOL mode
//
fabgl::PaintOptions Context::getPaintOptions(fabgl::PaintMode mode, fabgl::PaintOptions priorPaintOptions) {
	fabgl::PaintOptions p = priorPaintOptions;
	p.mode = mode;
	return p;
}

// Set up canvas for drawing graphics
//
void Context::setGraphicsOptions(uint8_t mode) {
	auto colourMode = mode & 0x03;
	setClippingRect(graphicsViewport);
	switch (colourMode) {
		case 0: break;	// move command
		case 1: {
			// use fg colour
			canvas->setPenColor(gfg);
			canvas->setPaintOptions(gpofg);
		} break;
		case 2: {
			// logical inverse colour - override paint options
			auto options = getPaintOptions(fabgl::PaintMode::Invert, gpofg);
			canvas->setPaintOptions(options);
			return;
		} break;
		case 3: {
			// use bg colour
			canvas->setPenColor(gbg);
			canvas->setPaintOptions(gpobg);
		} break;
	}
}

// Set up canvas for drawing filled graphics
//
void Context::setGraphicsFill(uint8_t mode) {
	auto colourMode = mode & 0x03;
	switch (colourMode) {
		case 0: break;	// move command
		case 1: {
			// use fg colour
			canvas->setBrushColor(gfg);
		} break;
		case 2: break;	// logical inverse colour (not suported)
		case 3: {
			// use bg colour
			canvas->setBrushColor(gbg);
		} break;
	}
}

// Update selected colours based on palette change in 64 colour modes
//
void Context::updateColours(uint8_t logical, uint8_t physical) {
	plottingText = false;
	auto lookedup = colourLookup[physical];
	if (logical == tfgc) {
		tfg = lookedup;
	}
	if (logical == tbgc) {
		tbg = lookedup;
	}
	if (logical == gfgc) {
		gfg = lookedup;
	}
	if (logical == gbgc) {
		gbg = lookedup;
	}
}

// Set a clipping rectangle
//
inline void Context::setClippingRect(Rect rect) {
	canvas->setClippingRect(rect);
}

//// Graphics drawing routines (private)

// Push point to list
//
void Context::pushPoint(Point p) {
	rp1 = Point(p.X - p1.X, p.Y - p1.Y);
	p3.X = p2.X;
	p3.Y = p2.Y;
	p2.X = p1.X;
	p2.Y = p1.Y;
	p1.X = p.X;
	p1.Y = p.Y;
}
void Context::pushPointRelative(int16_t x, int16_t y) {
	pushPoint(up1.X + x, up1.Y + y);
}

// Move to
//
void Context::moveTo() {
	canvas->moveTo(p1.X, p1.Y);
}

// Line plot
//
void Context::plotLine(bool omitFirstPoint, bool omitLastPoint, bool usePattern, bool resetPattern) {
	if (!textCursorActive()) {
		// if we're in graphics mode, we need to move the cursor to the last point
		canvas->moveTo(p2.X, p2.Y);
	}

	auto lineOptions = fabgl::LineOptions();
	lineOptions.omitFirst = omitFirstPoint;
	lineOptions.omitLast = omitLastPoint;
	lineOptions.usePattern = usePattern;
	if (resetPattern) {
		canvas->setLinePatternOffset(0);
	}
	canvas->setLineOptions(lineOptions);

	canvas->lineTo(p1.X, p1.Y);
}

// Point point
//
void Context::plotPoint() {
	canvas->setPixel(p1.X, p1.Y);
}

// Fill horizontal line
//
void Context::fillHorizontalLine(bool scanLeft, bool match, RGB888 matchColor) {
	canvas->waitCompletion(false);
	int16_t y = p1.Y;
	int16_t x1 = scanLeft ? (match ? scanHToMatch(p1.X, y, matchColor, -1) : scanH(p1.X, y, matchColor, -1)) : p1.X;
	int16_t x2 = match ? scanHToMatch(p1.X, y, matchColor, 1) : scanH(p1.X, y, matchColor, 1);
	debug_log("fillHorizontalLine: (%d, %d) transformed to (%d,%d) -> (%d,%d)\n\r", p1.X, p1.Y, x1, y, x2, y);

	if (x1 == x2 || x1 > x2) {
		// Coordinate needs to be tweaked to match Acorn's behaviour
		auto p = toCurrentCoordinates(scanLeft ? x2 + 1 : x2, y);
		pushPoint(p.X, up1.Y);
		// nothing to draw
		return;
	}
	canvas->moveTo(x1, y);
	canvas->lineTo(x2, y);

	auto p = toCurrentCoordinates(x2, y);
	pushPoint(p.X, up1.Y);
}

// Triangle plot
//
void Context::plotTriangle() {
	Point p[3] = {
		p3,
		p2,
		p1,
	};
	// if (gpo.mode == fabgl::PaintMode::Set) {
	// 	canvas->drawPath(p, 3);
	// }
	canvas->fillPath(p, 3);
}

// Rectangle plot
//
void Context::plotRectangle() {
	canvas->fillRectangle(p2.X, p2.Y, p1.X, p1.Y);
}

// Parallelogram plot
//
void Context::plotParallelogram() {
	Point p[4] = {
		p3,
		p2,
		p1,
		Point(p1.X + (p3.X - p2.X), p1.Y + (p3.Y - p2.Y)),
	};
	// if (gpo.mode == fabgl::PaintMode::Set) {
	// 	canvas->drawPath(p, 4);
	// }
	canvas->fillPath(p, 4);
}

// Circle plot
//
void Context::plotCircle(bool filled) {
	auto size = 2 * sqrt(rp1.X * rp1.X + (rp1.Y * rp1.Y * (rectangularPixels ? 4 : 1)));
	if (filled) {
		canvas->fillEllipse(p2.X, p2.Y, size, rectangularPixels ? size / 2 : size);
	} else {
		canvas->drawEllipse(p2.X, p2.Y, size, rectangularPixels ? size / 2 : size);
	}
}

// Arc plot
void Context::plotArc() {
	debug_log("plotArc: (%d,%d) -> (%d,%d), (%d,%d)\n\r", p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
	canvas->drawArc(p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
}

// Segment plot
void Context::plotSegment() {
	debug_log("plotSegment: (%d,%d) -> (%d,%d), (%d,%d)\n\r", p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
	canvas->fillSegment(p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
}

// Sector plot
void Context::plotSector() {
	debug_log("plotSector: (%d,%d) -> (%d,%d), (%d,%d)\n\r", p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
	canvas->fillSector(p3.X, p3.Y, p2.X, p2.Y, p1.X, p1.Y);
}

// Copy or move a rectangle
//
void Context::plotCopyMove(uint8_t mode) {
	uint16_t width = abs(p3.X - p2.X);
	uint16_t height = abs(p3.Y - p2.Y);
	int16_t sourceX = p3.X < p2.X ? p3.X : p2.X;
	int16_t sourceY = p3.Y < p2.Y ? p3.Y : p2.Y;
	int16_t destX = p1.X;
	int16_t destY = p1.Y - height;

	debug_log("plotCopyMove: mode %d, (%d,%d) -> (%d,%d), width: %d, height: %d\n\r", mode, sourceX, sourceY, destX, destY, width, height);

	// Source needs to sit within screen bounds so therefore needs to be truncated accordingly
	// (coordinates already adjusted for origin)
	Rect sourceRect = Rect(sourceX, sourceY, sourceX + width, sourceY + height);
	Rect screenSrc = sourceRect.intersection(Rect(0, 0, canvasW - 1, canvasH - 1));
	canvas->copyRect(screenSrc.X1, screenSrc.Y1, destX, destY, screenSrc.width(), screenSrc.height());
	if (mode == 1 || mode == 5) {
		// move rectangle needs to clear source rectangle
		// being careful not to clear the destination rectangle
		canvas->setBrushColor(gbg);
		canvas->setPaintOptions(getPaintOptions(fabgl::PaintMode::Set, gpobg));
		debug_log("plotCopyMove: source rectangle (%d,%d) -> (%d,%d)\n\r", sourceRect.X1, sourceRect.Y1, sourceRect.X2, sourceRect.Y2);
		Rect destRect = Rect(destX, destY, destX + width, destY + height);
		debug_log("plotCopyMove: destination rectangle (%d,%d) -> (%d,%d)\n\r", destRect.X1, destRect.Y1, destRect.X2, destRect.Y2);
		if (sourceRect.intersects(destRect)) {
			// we can use clipping rects to block out parts of the screen
			// the areas above, below, left, and right of the destination rectangle
			// and then draw rectangles over our source rectangle
			auto intersection = sourceRect.intersection(destRect);
			debug_log("intersection: (%d,%d) -> (%d,%d)\n\r", intersection.X1, intersection.Y1, intersection.X2, intersection.Y2);

			if (intersection.X1 > sourceRect.X1) {
				// fill in source area to the left of destination
				debug_log("clearing left of destination\n\r");
				auto clearClip = Rect(sourceRect.X1, sourceRect.Y1, intersection.X1 - 1, sourceRect.Y2);
				debug_log("clearClip: (%d,%d) -> (%d,%d)\n\r", clearClip.X1, clearClip.Y1, clearClip.X2, clearClip.Y2);
				setClippingRect(clearClip);
				canvas->fillRectangle(sourceRect);
			}
			if (intersection.X2 < sourceRect.X2) {
				// fill in source area to the right of destination
				debug_log("clearing right of destination\n\r");
				auto clearClip = Rect(intersection.X2 + 1, sourceRect.Y1, sourceRect.X2, sourceRect.Y2);
				debug_log("clearClip: (%d,%d) -> (%d,%d)\n\r", clearClip.X1, clearClip.Y1, clearClip.X2, clearClip.Y2);
				setClippingRect(clearClip);
				canvas->fillRectangle(sourceRect);
			}
			if (intersection.Y1 > sourceRect.Y1) {
				// fill in source area above destination
				debug_log("clearing above destination\n\r");
				auto clearClip = Rect(sourceRect.X1, sourceRect.Y1, sourceRect.X2, intersection.Y1 - 1);
				debug_log("clearClip: (%d,%d) -> (%d,%d)\n\r", clearClip.X1, clearClip.Y1, clearClip.X2, clearClip.Y2);
				setClippingRect(clearClip);
				canvas->fillRectangle(sourceRect);
			}
			if (intersection.Y2 < sourceRect.Y2) {
				// fill in source area below destination
				debug_log("clearing below destination\n\r");
				auto clearClip = Rect(sourceRect.X1, intersection.Y2 + 1, sourceRect.X2, sourceRect.Y2);
				debug_log("clearClip: (%d,%d) -> (%d,%d)\n\r", clearClip.X1, clearClip.Y1, clearClip.X2, clearClip.Y2);
				setClippingRect(clearClip);
				canvas->fillRectangle(sourceRect);
			}
		} else {
			canvas->fillRectangle(sourceRect);
		}
	}
}

// Path plot
//
void Context::plotPath(uint8_t mode, uint8_t lastMode) {
	debug_log("plotPath: mode %d, lastMode %d, pathPoints.size() %d\n\r", mode, lastMode, pathPoints.size());
	// if the mode indicates a "move", then this is a "commit" command
	// so we should draw the path and clear the pathPoints array
	if ((mode & 0x03) == 0) {
		if (pathPoints.size() < 3) {
			// we need at least three points to draw a path
			debug_log("plotPath: not enough points to draw a path - clearing\n\r");
			pathPoints.clear();
			return;
		}
		debug_log("plotPath: drawing path\n\r");
		// iterate over our pathPoints and output in debug statement
		for (auto p : pathPoints) {
			debug_log("plotPath: (%d,%d)\n\r", p.X, p.Y);
		}
		debug_log("plotPath: setting graphics fill with lastMode %d\n\r", lastMode);
		// i'm not entirely sure yet whether this is needed
		setGraphicsOptions(lastMode);
		setGraphicsFill(lastMode);
		canvas->fillPath(pathPoints.data(), pathPoints.size());
		pathPoints.clear();
		return;
	}

	// if we have an empty pathPoints list, then push two points
	if (pathPoints.size() == 0) {
		pathPoints.push_back(p3);
		pathPoints.push_back(p2);
	}
	// push latest point
	pathPoints.push_back(p1);
}

// Plot bitmap
//
void Context::plotBitmap(uint8_t mode) {
	if ((mode & 0x03) == 0x03) {
		// get a copy of gpobg, without changing it's paint mode
		auto paintOptions = getPaintOptions(gpobg.mode, gpobg);
		// swapFGBG on bitmap plots indicates to plot using pen color instead of bitmap
		paintOptions.swapFGBG = true;
		canvas->setPaintOptions(paintOptions);
	}
	drawBitmap(p1.X, p1.Y, true, false);
	plottingText = false;
}


// Clear a viewport
//
void Context::clearViewport(ViewportType type) {
	if (ttxtMode) {
		ttxt_instance.cls();
	} else {
		canvas->fillRectangle(*getViewport(type));
	}
}

void Context::scrollRegion(Rect * region, uint8_t direction, int16_t movement) {
	auto moveX = 0;
	auto moveY = 0;
	canvas->setScrollingRegion(region->X1, region->Y1, region->X2, region->Y2);
	canvas->setPenColor(tbg);
	canvas->setBrushColor(tbg);
	canvas->setPaintOptions(tpo);
	plottingText = false;
	switch (direction) {
		case 0:		// Right
			moveX = 1;
			break;
		case 1:		// Left
			moveX = -1;
			break;
		case 2:		// Down
			moveY = 1;
			break;
		case 3:		// Up
			moveY = -1;
			break;
		case 4:		// positive X
			if (cursorBehaviour.flipXY) {
				moveY = cursorBehaviour.invertVertical ? -1 : 1;
			} else {
				moveX = cursorBehaviour.invertHorizontal ? -1 : 1;
			}
			break;
		case 5:		// negative X
			if (cursorBehaviour.flipXY) {
				moveY = cursorBehaviour.invertVertical ? 1 : -1;
			} else {
				moveX = cursorBehaviour.invertHorizontal ? 1 : -1;
			}
			break;
		case 6:		// positive Y
			if (cursorBehaviour.flipXY) {
				moveX = cursorBehaviour.invertHorizontal ? -1 : 1;
			} else {
				moveY = cursorBehaviour.invertVertical ? -1 : 1;
			}
			break;
		case 7:		// negative Y
			if (cursorBehaviour.flipXY) {
				moveX = cursorBehaviour.invertHorizontal ? 1 : -1;
			} else {
				moveY = cursorBehaviour.invertVertical ? 1 : -1;
			}
			break;
	}
	if (moveX != 0 || moveY != 0) {
		if (ttxtMode) {
			ttxt_instance.scroll(moveX, moveY);
		} else {
			if (movement == 0) {
				if (moveX != 0) {
					movement = getFont()->width;
				} else {
					movement = getFont()->height;
				}
			}
			canvas->scroll(movement * moveX, movement * moveY);
		}
	}
	if (textCursorActive()) {
		canvas->setPenColor(tfg);
		canvas->setBrushColor(tbg);
	} else {
		canvas->setPenColor(gfg);
		canvas->setBrushColor(gfg);
		canvas->setPaintOptions(gpofg);
	}
}


// Horizontal scan until we find a pixel not non-equalto given colour
// returns x coordinate for the last pixel before the match
uint16_t Context::scanH(int16_t x, int16_t y, RGB888 colour, int8_t direction = 1) {
	uint16_t w = direction > 0 ? canvas->getWidth() - 1 : 0;
	if (x < 0 || x >= canvas->getWidth()) return x;

	while (x != w) {
		RGB888 pixel = canvas->getPixel(x, y);
		if (pixel == colour) {
			x += direction;
		} else {
			return x - direction;
		}
	}

	return w;
}

// Horizontal scan until we find a pixel matching the given colour
// returns x coordinate for the last pixel before the match
uint16_t Context::scanHToMatch(int16_t x, int16_t y, RGB888 colour, int8_t direction = 1) {
	uint16_t w = direction > 0 ? canvas->getWidth() - 1 : 0;
	if (x < 0 || x >= canvas->getWidth()) return x;

	while (x != w) {
		RGB888 pixel = canvas->getPixel(x, y);
		if (pixel == colour) {
			return x - direction;
		}
		x += direction;
	}

	return w;
}


// Public graphics functions

void Context::setLineThickness(uint8_t thickness) {
	lineThickness = thickness;
	canvas->setPenWidth(thickness);
}

void Context::setDottedLinePattern(uint8_t pattern[8]) {
	linePattern.setPattern(pattern);
	canvas->setLinePattern(linePattern);
}

void Context::setDottedLinePatternLength(uint8_t length) {
	linePatternLength = length == 0 ? 8 : length;
	if (length == 0) {
		// reset the line pattern
		linePattern = fabgl::LinePattern();
		canvas->setLinePattern(linePattern);
	}
	canvas->setLinePatternLength(length);
}


// Set text colour (handles COLOUR / VDU 17)
//
void Context::setTextColour(uint8_t colour) {
	if (ttxtMode) return;

	uint8_t col = colour % getVGAColourDepth();
	uint8_t c = palette[col];

	if (colour < 64) {
		tfg = colourLookup[c];
		tfgc = col;
		if (plottingText && textCursorActive()) {
			canvas->setPenColor(tfg);
		}
		debug_log("vdu_colour: tfg %d = %02X : %02X,%02X,%02X\n\r", colour, c, tfg.R, tfg.G, tfg.B);
	}
	else if (colour >= 128 && colour < 192) {
		tbg = colourLookup[c];
		tbgc = col;
		if (plottingText && textCursorActive()) {
			canvas->setBrushColor(tbg);
		}
		debug_log("vdu_colour: tbg %d = %02X : %02X,%02X,%02X\n\r", colour, c, tbg.R, tbg.G, tbg.B);
	}
	else {
		debug_log("vdu_colour: invalid colour %d\n\r", colour);
	}
}

// Set graphics colour (handles GCOL / VDU 18)
//
void Context::setGraphicsColour(uint8_t mode, uint8_t colour) {
	if (ttxtMode) return;

	uint8_t col = colour % getVGAColourDepth();
	uint8_t c = palette[col];

	if (mode <= 7) {
		if (colour < 64) {
			gfg = colourLookup[c];
			gfgc = col;
			debug_log("vdu_gcol: mode %d, gfg %d = %02X : %02X,%02X,%02X\n\r", mode, colour, c, gfg.R, gfg.G, gfg.B);
		}
		else if (colour >= 128 && colour < 192) {
			gbg = colourLookup[c];
			gbgc = col;
			debug_log("vdu_gcol: mode %d, gbg %d = %02X : %02X,%02X,%02X\n\r", mode, colour, c, gbg.R, gbg.G, gbg.B);
		}
		else {
			debug_log("vdu_gcol: invalid colour %d\n\r", colour);
		}
		if (colour < 128) {
			gpofg = getPaintOptions((fabgl::PaintMode)mode, gpofg);
		} else {
			gpobg = getPaintOptions((fabgl::PaintMode)mode, gpobg);
		}
	}
	else {
		debug_log("vdu_gcol: invalid mode %d\n\r", mode);
	}
	plottingText = false;
}

// Get currently set colour values
//
bool Context::getColour(uint8_t colour, RGB888 * pixel) {
	switch (colour) {
		case 128:	// text foreground
			*pixel = tfg;
			break;
		case 129:	// text background
			*pixel = tbg;
			break;
		case 130:	// graphics foreground
			*pixel = gfg;
			break;
		case 131:	// graphics background
			*pixel = gbg;
			break;
		default:
			return false;
	}
	return true;
}

// Get pixel value at screen coordinates
//
RGB888 Context::getPixel(uint16_t x, uint16_t y) {
	Point p = toScreenCoordinates(x, y);
	if (p.X >= 0 && p.Y >= 0 && p.X < canvasW && p.Y < canvasH) {
		return canvas->getPixel(p.X, p.Y);
	}
	return RGB888(0,0,0);
}


void Context::pushPoint(uint16_t x, uint16_t y) {
	up1 = Point(x, y);
	pushPoint(toScreenCoordinates(x, y));
}

// Get rect from last two points
// ensuring that the rect is on-screen coordinates
//
Rect Context::getGraphicsRect() {
	return defaultViewport.intersection(
		Rect(
			std::min(p1.X, p2.X),
			std::min(p1.Y, p2.Y),
			std::max(p1.X, p2.X),
			std::max(p1.Y, p2.Y)
		)
	);
}


// Plot command handler
//
bool IRAM_ATTR Context::plot(int16_t x, int16_t y, uint8_t command) {
	auto mode = command & 0x07;
	auto operation = command & 0xF8;
	bool pending = false;
	plottingText = false;

	if (mode < 4) {
		pushPointRelative(x, y);
	} else {
		pushPoint(x, y);
	}

	debug_log("vdu_plot: operation: %X, mode %d, lastPlotCommand %X, (%d,%d) -> (%d,%d)\n\r", operation, mode, lastPlotCommand, x, y, p1.X, p1.Y);

	if (((lastPlotCommand & 0xF8) == 0xD8) && ((lastPlotCommand & 0xFB) != (command & 0xFB))) {
		debug_log("vdu_plot: last plot was a path, but different command detected\n\r");
		// We're not doing a path any more - so commit it
		plotPath(0, lastPlotCommand & 0x03);
	}

	setGraphicsOptions(mode);

	// if (mode != 0 && mode != 4) {
	if (mode & 0x03) {
		switch (operation) {
			case 0x00:	// line
				plotLine(false, false, false, false);
				break;
			case 0x08:	// line, omitting last point
				plotLine(false, true, false, false);
				break;
			case 0x10:	// dot-dash line
				plotLine(false, false, true, false);
				break;
			case 0x18:	// dot-dash line, omitting last point
				plotLine(false, true, true, false);
				break;
			case 0x30:	// dot-dash line, omitting first, pattern continued
				plotLine(true, false, true, false);
				break;
			case 0x38:	// dot-dash line, omitting both, pattern continued
				plotLine(true, true, true, false);
				break;
			case 0x20:	// solid line, first point omitted
				plotLine(true, false, false, false);
				break;
			case 0x28:	// solid line, first and last points omitted
				plotLine(true, true, false, false);
				break;
			case 0x40:	// point
				plotPoint();
				break;
			case 0x48:	// line fill left/right to non-bg
				fillHorizontalLine(true, false, gbg);
				break;
			case 0x50:	// triangle fill
				setGraphicsFill(mode);
				plotTriangle();
				break;
			case 0x58:	// line fill right to bg
				fillHorizontalLine(false, true, gbg);
				break;
			case 0x60:	// rectangle fill
				setGraphicsFill(mode);
				plotRectangle();
				break;
			case 0x68:	// line fill left/left to fg
				fillHorizontalLine(true, true, gfg);
				break;
			case 0x70:	// parallelogram fill
				setGraphicsFill(mode);
				plotParallelogram();
				break;
			case 0x78:	// line fill right to non-fg
				fillHorizontalLine(false, false, gfg);
				break;
			case 0x80:	// flood to non-bg
			case 0x88:	// flood to fg
				debug_log("plot flood fill not implemented\n\r");
				break;
			case 0x90:	// circle outline
				plotCircle(false);
				break;
			case 0x98:	// circle fill
				setGraphicsFill(mode);
				plotCircle(true);
				break;
			case 0xA0:	// circular arc
				plotArc();
				break;
			case 0xA8:	// circular segment
				setGraphicsFill(mode);
				plotSegment();
				break;
			case 0xB0:	// circular sector
				setGraphicsFill(mode);
				plotSector();
				break;
			case 0xB8:	// copy/move
				plotCopyMove(mode);
				break;
			case 0xC0:	// ellipse outline
			case 0xC8:	// ellipse fill
				// fab-gl's ellipse isn't compatible with BBC BASIC
				debug_log("plot ellipse not implemented\n\r");
				break;
			case 0xD8:	// plot path (unassigned on Acorn and other BBC BASIC versions)
				plotPath(mode, lastPlotCommand & 0x03);
				// vdu_plotPath(mode);
				pending = true;
				break;
			case 0xD0:	// unassigned ("Font printing" (do not use) in RISC OS)
			case 0xE0:	// unassigned
				debug_log("plot operation unassigned\n\r");
				break;
			case 0xE8:	// Bitmap plot
				plotBitmap(mode);
				break;
			case 0xF0:	// unassigned
			case 0xF8:	// Swap rectangle (BBC Basic for Windows extension)
				// only actually supports "foreground" codes &F9 and &FD
				debug_log("plot swap rectangle not implemented\n\r");
				break;
		}
	}
	lastPlotCommand = command;
	moveTo();
	return pending;
}

void Context::plotPending(int16_t peeked) {
	// Currently pending plot commands can only be flagged for path drawing
	// In future we may need to check the lastPlotCommand here
	if (peeked == -1 || peeked != 25) {
		plotPath(0, lastPlotCommand & 0x03);
	}
}


// Plot a string
//
void Context::plotString(const std::string& s) {
	if (!ttxtMode && !plottingText) {
		if (textCursorActive()) {
			setClippingRect(textViewport);
			canvas->setPenColor(tfg);
			canvas->setBrushColor(tbg);
			canvas->setPaintOptions(tpo);
		} else {
			setClippingRect(graphicsViewport);
			canvas->setPenColor(gfg);
			canvas->setPaintOptions(gpofg);
		}
		plottingText = true;
	}

	auto font = getFont();
	// iterate over the string and plot each character
	for (const char c : s) {
		if (cursorBehaviour.scrollProtect) {
			cursorAutoNewline();
		}
		if (ttxtMode) {
			ttxt_instance.draw_char(activeCursor->X, activeCursor->Y, c);
		} else {
			auto bitmap = getBitmapFromChar(c);
			if (bitmap) {
				canvas->drawBitmap(activeCursor->X, activeCursor->Y + font->height - bitmap->height, bitmap.get());
			} else {
				canvas->drawChar(activeCursor->X, activeCursor->Y, c);
			}
		}
		if (!cursorBehaviour.xHold) {
			cursorRight(cursorBehaviour.scrollProtect);
		}
	}
}

// Backspace plot
//
void Context::plotBackspace() {
	cursorLeft();
	if (ttxtMode) {
		ttxt_instance.draw_char(activeCursor->X, activeCursor->Y, ' ');
	} else {
		canvas->setBrushColor(textCursorActive() ? tbg : gbg);
		canvas->fillRectangle(activeCursor->X, activeCursor->Y, activeCursor->X + getFont()->width - 1, activeCursor->Y + getFont()->height - 1);
		plottingText = false;
	}
}

// draw bitmap
//
void Context::drawBitmap(uint16_t x, uint16_t y, bool compensateHeight, bool forceSet) {
	auto bitmap = getBitmap(currentBitmap);
	if (bitmap) {
		if (forceSet) {
			auto options = getPaintOptions(fabgl::PaintMode::Set, gpofg);
			canvas->setPaintOptions(options);
		}
		auto yPos = (compensateHeight && logicalCoords) ? (y + 1 - bitmap->height) : y;
		if (bitmapTransform != 65535) {
			auto transformBufferIter = buffers.find(bitmapTransform);
			if (transformBufferIter != buffers.end()) {
				auto &transformBuffer = transformBufferIter->second;
				if (!checkTransformBuffer(transformBuffer)) {
					debug_log("drawBitmap: transform buffer %d is invalid\n\r", bitmapTransform);
					bitmapTransform = 65535;
					canvas->drawBitmap(x, yPos, bitmap.get());
					return;
				}
				// NB: if we're drawing via PLOT and are using OS coords, then we _should_ be using bottom left of bitmap as our "origin" for transforms
				// however we're not doing that here - the origin for transforms is top left of the bitmap
				// attempting to transform based on bottom left would require translates to be added to the matrix, custom for the bitmap being plotted
				// which would mean they could not be cached

				// we should have a valid transform buffer now, which includes an inverse chunk
				canvas->drawTransformedBitmap(x, yPos, bitmap.get(), (float *)transformBuffer[0]->getBuffer(), (float *)transformBuffer[1]->getBuffer());
				return;
			}
			// if buffer not found, we should fall back to normal drawing
		}
		canvas->drawBitmap(x, yPos, bitmap.get());
	} else {
		debug_log("drawBitmap: bitmap %d not found\n\r", currentBitmap);
	}
}

// Draw cursor
//
void Context::drawCursor(Point p) {
	if (textCursorActive()) {
		auto font = getFont();
		if (cursorHStart < font->width && cursorHStart <= cursorHEnd && cursorVStart < font->height && cursorVStart <= cursorVEnd) {
			canvas->setPaintOptions(cpo);
			canvas->setBrushColor(tbg);
			canvas->fillRectangle(p.X + cursorHStart, p.Y + cursorVStart, p.X + std::min(((int)cursorHEnd), font->width - 1), p.Y + std::min(((int)cursorVEnd), font->height - 1));
			canvas->setBrushColor(tfg);
			canvas->fillRectangle(p.X + cursorHStart, p.Y + cursorVStart, p.X + std::min(((int)cursorHEnd), font->width - 1), p.Y + std::min(((int)cursorVEnd), font->height - 1));
			canvas->setPaintOptions(tpo);
			plottingText = false;
		}
	}
}

// Set affine transform
//
void Context::setAffineTransform(uint8_t flags, uint16_t bufferId) {
	if (flags & 0x01) {
		bitmapTransform = bufferId;
	}
}

// Clear the screen
//
void Context::cls() {
	hideCursor();
	if (hasActiveSprites()) {
		activateSprites(0);
	}
	if (canvas) {
		canvas->setPenColor(tfg);
		canvas->setBrushColor(tbg);
		canvas->setPaintOptions(tpo);
		setClippingRect(textViewport);
		clearViewport(ViewportType::Text);
		plottingText = true;
	}
	cursorHome();
	setPagedMode(pagedMode);
	showCursor();
}

// Clear the graphics area
//
void Context::clg() {
	if (canvas) {
		canvas->setPenColor(gfg);
		canvas->setBrushColor(gbg);
		canvas->setPaintOptions(gpobg);
		setClippingRect(graphicsViewport);
		clearViewport(ViewportType::Graphics);
		plottingText = false;
	}
	pushPoint(0, 0);		// Reset graphics cursor position (as per BBC Micro CLG)
}

void Context::scrollRegion(ViewportType viewport, uint8_t direction, int16_t movement) {
	scrollRegion(getViewport(viewport), direction, movement);
}


// Reset graphics colours and painting options
//
void Context::resetGraphicsPainting() {
	gbgc = tbgc = 0;
	gfgc = tfgc = 15 % getVGAColourDepth();
	gfg = colourLookup[0x3F];
	gbg = colourLookup[0x00];
	gpofg = getPaintOptions(fabgl::PaintMode::Set, gpofg);
	gpobg = getPaintOptions(fabgl::PaintMode::Set, gpobg);
}

void Context::resetGraphicsOptions() {
	setLineThickness(1);
	setCurrentBitmap(BUFFERED_BITMAP_BASEID);
	setDottedLinePatternLength(0);
	setAffineTransform(255, -1);
}

void Context::resetGraphicsPositioning() {
	setOrigin(0,0);
	pushPoint(0,0);
	pushPoint(0,0);
	pushPoint(0,0);
	moveTo();
	graphicsViewport = Rect(0, 0, canvasW - 1, canvasH - 1);
}

void Context::resetTextPainting() {
	tfg = colourLookup[0x3F];
	tbg = colourLookup[0x00];
	tpo = getPaintOptions(fabgl::PaintMode::Set, tpo);
	cpo = getPaintOptions(fabgl::PaintMode::XOR, tpo);
	plottingText = false;
}

// Reset graphics context, called after a mode change
//
void Context::reset() {
	defaultViewport = Rect(0, 0, canvasW - 1, canvasH - 1);
	resetGraphicsPainting();
	resetTextPainting();
	resetGraphicsPositioning();
	setLineThickness(1);
	setAffineTransform(255, -1);
	resetFonts();
	resetTextCursor();
}

// Activate the context, setting up canvas as required
//
void Context::activate() {
	plottingText = false;
	if (!ttxtMode) {
		canvas->selectFont(font == nullptr ? &FONT_AGON : font.get());
	}
	setLineThickness(lineThickness);
	// reset line pattern
	canvas->setLinePattern(linePattern);
	canvas->setLinePatternLength(linePatternLength);
	moveTo();
}

#endif // CONTEXT_GRAPHICS_H
