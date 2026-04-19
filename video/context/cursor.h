#ifndef CONTEXT_CURSOR_H
#define CONTEXT_CURSOR_H

#include <fabgl.h>

#include "agon_ps2.h"

// Definitions for the functions we're implementing here
#include "context.h"

// Private cursor management functions
//

// Functions to get measurements derived from behaviour, font and viewport

// Adjustments to ensure cursor position sits at the nearest character boundary
int Context::getXAdjustment() {
	return activeViewport->width() % getFont()->width;
}

int Context::getYAdjustment() {
	return activeViewport->height() % getFont()->height;
}

int Context::getNormalisedViewportWidth() {
	if (cursorBehaviour.flipXY) {
		return activeViewport->height() - getYAdjustment();
	}
	return activeViewport->width() - getXAdjustment();
}

int Context::getNormalisedViewportHeight() {
	auto font = getFont();
	if (cursorBehaviour.flipXY) {
		auto height = activeViewport->width() - getXAdjustment();
		if (!cursorBehaviour.invertHorizontal) {
			height -= font->width - 1;
		}
		return height;
	}
	auto height = activeViewport->height() - getYAdjustment();
	if (!cursorBehaviour.invertVertical) {
		height -= font->height - 1;
	}
	return height;
}

Point Context::getNormalisedCursorPosition() {
	return getNormalisedCursorPosition(activeCursor);
}

Point Context::getNormalisedCursorPosition(Point * cursor) {
	Point p;
	if (cursorBehaviour.flipXY) {
		// our normalised Y needs to take values from X and vice versa
		if (cursorBehaviour.invertHorizontal) {
			p.Y = activeViewport->X2 - cursor->X;
		} else {
			p.Y = cursor->X - activeViewport->X1;
		}
		if (cursorBehaviour.invertVertical) {
			p.X = activeViewport->Y2 - cursor->Y;
		} else {
			p.X = cursor->Y - activeViewport->Y1;
		}
	} else {
		if (cursorBehaviour.invertHorizontal) {
			p.X = activeViewport->X2 - cursor->X;
		} else {
			p.X = cursor->X - activeViewport->X1;
		}
		if (cursorBehaviour.invertVertical) {
			p.Y = activeViewport->Y2 - cursor->Y;
		} else {
			p.Y = cursor->Y - activeViewport->Y1;
		}
	}
	return p;
}


// Functions to check if the cursor is off the edge of the viewport

bool Context::cursorIsOffRight() {
	return getNormalisedCursorPosition().X >= getNormalisedViewportWidth();
}

bool Context::cursorIsOffLeft() {
	return getNormalisedCursorPosition().X < 0;
}

bool Context::cursorIsOffTop() {
	return getNormalisedCursorPosition().Y < 0;
}

bool Context::cursorIsOffBottom() {
	return getNormalisedCursorPosition().Y >= getNormalisedViewportHeight();
}

bool Context::cursorIsOnBottomRow() {
	// returns true if a newline would make cursorIsOffBottom true
	auto fontSize = cursorBehaviour.flipXY ? getFont()->width : getFont()->height;
	return getNormalisedCursorPosition().Y >= (getNormalisedViewportHeight() - fontSize);
}


// Functions to move the cursor to the edge of the viewport

void Context::cursorEndRow(Point * cursor, Rect * viewport) {
	if (cursorBehaviour.flipXY) {
		cursor->Y = cursorBehaviour.invertVertical ? viewport->Y1 : (viewport->Y2 + 1 - getFont()->height - getYAdjustment());
	} else {
		cursor->X = cursorBehaviour.invertHorizontal ? viewport->X1 : (viewport->X2 + 1 - getFont()->width - getXAdjustment());
	}
	updateTextCursorPosition();
}

void Context::cursorTop(Point * cursor, Rect * viewport) {
	if (cursorBehaviour.flipXY) {
		cursor->X = cursorBehaviour.invertHorizontal ? (viewport->X2 + 1 - getFont()->width - getXAdjustment()) : viewport->X1;
	} else {
		cursor->Y = cursorBehaviour.invertVertical ? (viewport->Y2 + 1 - getFont()->height - getYAdjustment()) : viewport->Y1;
	}
	updateTextCursorPosition();
}

void Context::cursorEndCol(Point * cursor, Rect * viewport) {
	if (cursorBehaviour.flipXY) {
		cursor->X = cursorBehaviour.invertHorizontal ? viewport->X1 : (viewport->X2 + 1 - getFont()->width - getXAdjustment());
	} else {
		cursor->Y = cursorBehaviour.invertVertical ? viewport->Y1 : (viewport->Y2 + 1 - getFont()->height - getYAdjustment());
	}
	updateTextCursorPosition();
}


// Functions to handle automatic cursor repositioning

// Check if the cursor is off the edge of the viewport and take appropriate action
// returns true if the cursor wrapped, false if no action was taken or the screen scrolled
bool Context::cursorScrollOrWrap() {
	bool offLeft = cursorIsOffLeft();
	bool offRight = cursorIsOffRight();
	bool offTop = cursorIsOffTop();
	bool offBottom = cursorIsOffBottom();
	if (!offLeft && !offRight && !offTop && !offBottom) {
		// cursor within current viewport, so do nothing
		return false;
	}

	if (textCursorActive() && !cursorBehaviour.yWrap) {
		// text cursor, scrolling for our Y direction is enabled
		if (offTop) {
			// scroll screen down by 1 line
			scrollRegion(activeViewport, 6, 0);
			// move cursor down until it's within the viewport
			do {
				cursorDown(true);
			} while (cursorIsOffTop());
			return false;
		}
		if (offBottom) {
			// scroll screen up by 1 line
			scrollRegion(activeViewport, 7, 0);
			// move cursor up until it's within the viewport
			do {
				cursorUp(true);
			} while (cursorIsOffBottom());
			return false;
		}
	}

	// if we get here we have a graphics cursor, or text cursor with wrap enabled
	if (!textCursorActive() && cursorBehaviour.grNoSpecialActions) {
		return false;
	}

	// if we've reached here, we're wrapping, so move cursor to the opposite edge
	if (offLeft) {
		cursorEndRow();
	}
	if (offRight) {
		cursorCR();
	}
	if (offTop) {
		cursorEndCol();
	}
	if (offBottom) {
		cursorTop();
	}
	return true;
}

bool Context::cursorAutoNewline() {
	if (cursorIsOffRight() && (textCursorActive() || !cursorBehaviour.grNoSpecialActions)) {
		cursorCR();
		cursorDown();
		return true;
	}
	return false;
}

void Context::ensureCursorInViewport(Rect viewport) {
	if (textCursor.X < viewport.X1
		|| textCursor.X > viewport.X2 - getXAdjustment()
		|| textCursor.Y < viewport.Y1
		|| textCursor.Y > viewport.Y2 - getYAdjustment()
	) {
		cursorHome(&textCursor, &viewport);
	}
}


// Text cursor sprite functions

void Context::deleteTextCursor() {
	debug_log("Deleting text cursor sprite and bitmap\n");
	_VGAController->setTextCursor(nullptr);
	if (textCursorSprite != nullptr) {
		textCursorSprite = nullptr;
	}
	if (textCursorBitmap != nullptr) {
		if (textCursorBitmap->data != nullptr) {
			heap_caps_free(textCursorBitmap->data);
		}
		textCursorBitmap = nullptr;
	}
}

void Context::updateTextCursorBitmap() {
	// TODO: Cursor - when we support custom cursor bitmaps/sprites
	//   we should just ensure that the sprite is properly set up...
	// but for teletext mode we should not support custom text cursors
	auto font = getFont();
	if (!font) {
		return;
	}
	auto width = std::min(cursorHEnd, font->width) - cursorHStart;
	auto height = std::min(cursorVEnd, font->height) - cursorVStart;

	// ensure the bitmap width or height is not negative
	// - if so the cursor can just be deleted
	if (width <= 0 || height <= 0) {
		deleteTextCursor();
		return;
	}

	// Our cursor color is derived from fg and bg colours, XOR'd together
	// we we do an XOR plot to ensure that we erase the bg and draw the fg colour
	auto r = (tfg.R ^ tbg.R) >> 6;
	auto g = (tfg.G ^ tbg.G) >> 6;
	auto b = (tfg.B ^ tbg.B) >> 6;
	// bitmap colour byte is RGBA2222 format, which uses bit order AABBGGRR
	uint8_t cursorColor = (3 << 6) | (b << 4) | (g << 2) | r;
	// We'll make a fake RGB888 colour to store as the foregroundColor on the bitmap
	// which can be used to check whether the colour has changed - not a true RGB888 colour
	RGB888 cursorRGB(cursorColor, cursorColor, cursorColor);

	auto differentSize = textCursorBitmap == nullptr
		|| textCursorBitmap->width != width
		|| textCursorBitmap->height != height;

	// If this info doesn't match our current bitmap, we may need to create a new one
	if (differentSize || (textCursorBitmap->foregroundColor.R != cursorColor)) {
		if (!differentSize) {
			// size matches so colour must be different - update it
			memset(textCursorBitmap->data, cursorColor, width * height);
			// update the tracking colour on the bitmap
			textCursorBitmap->foregroundColor = cursorRGB;
			debug_log("Updated text cursor bitmap with colour %02x\n", cursorColor);
		} else {
			// delete the old cursor sprite and bitmap if they exist
			deleteTextCursor();
	
			// make a new bitmap for the cursor, first by creating a data block
			auto data = (uint8_t*)ps_malloc(width * height);
			if (!data) {
				// if we can't allocate the data block, just return
				debug_log("Failed to allocate memory for text cursor bitmap data\n");
				return;
			}
			// fill the data block with the cursor colour
			memset(data, cursorColor, width * height);
	
			textCursorBitmap = make_shared_psram<Bitmap>(width, height, data, PixelFormat::RGBA2222, cursorRGB);
			if (!textCursorBitmap) {
				// if we couldn't create the bitmap, free the data and return
				heap_caps_free(data);
				debug_log("Failed to create text cursor bitmap\n");
				return;
			}
			debug_log("Created text cursor bitmap %dx%d with colour %02x\n",
				textCursorBitmap->width, textCursorBitmap->height, cursorColor);
		}
	}

	// If we reach here we have a textCursorBitmap

	// Ensure our sprite exists, and if not create it
	if (textCursorSprite == nullptr) {
		textCursorSprite = std::make_shared<Sprite>();
		if (!textCursorSprite) {
			debug_log("Failed to create text cursor sprite\n");
			return;
		}
		textCursorSprite->hardware = true;
		textCursorSprite->paintOptions.mode = fabgl::PaintMode::XOR;
		debug_log("Created new text cursor sprite\n");
	}

	if (textCursorSprite->getFrame() != textCursorBitmap.get()) {
		// if the sprite's frame is not the same as the bitmap, update the cursor
		debug_log("Updating text cursor sprite with new bitmap\n");
		textCursorSprite->clearBitmaps();
		textCursorSprite->addBitmap(textCursorBitmap.get());
	}

	updateTextCursorVisibility();
	updateTextCursorPosition();

	_VGAController->setTextCursor(textCursorSprite.get());
}


// Public cursor control functions
//

// Cursor management, behaviour, and appearance

void Context::doCursorFlash() {
	if (!cursorFlashing || cursorTemporarilyHidden) {
		return;
	}
	auto now = xTaskGetTickCountFromISR();
	if (now - cursorTime > cursorFlashRate) {
		cursorTime = now;
		if (ttxtMode) {
			ttxt_instance.flash();
		}
		if (textCursorActive() && cursorEnabled && textCursorSprite != nullptr) {
			textCursorSprite->visible = !textCursorSprite->visible;
		}
	}
}

inline bool Context::textCursorActive() {
	return activeCursor == &textCursor;
}

inline void Context::setActiveCursor(CursorType type) {
	switch (type) {
		case CursorType::Text:
			activeCursor = &textCursor;
			changeFont(textFont, textFontData, 0);
			setCharacterOverwrite(true);
			setActiveViewport(ViewportType::Text);
			updateTextCursorPosition();
			break;
		case CursorType::Graphics:
			activeCursor = &p1;
			changeFont(graphicsFont, graphicsFontData, 0);
			setCharacterOverwrite(false);
			setActiveViewport(ViewportType::Graphics);
			break;
	}
	updateTextCursorVisibility();
}

inline void Context::setCursorBehaviour(uint8_t setting, uint8_t mask = 0) {
	cursorBehaviour.value = (cursorBehaviour.value & mask) ^ setting;
}

inline void Context::enableCursor(uint8_t enable) {
	cursorEnabled = (bool) enable;
	updateTextCursorVisibility();
	if (enable == 2) {
		cursorFlashing = false;
	}
	if (enable == 3) {
		cursorFlashing = true;
	}
}
inline void Context::hideCursor() {
	// Temporarily hide the cursor if it's visible
	if (!cursorTemporarilyHidden && (textCursorSprite != nullptr) && textCursorSprite->visible) {
		textCursorSprite->visible = false;
		cursorTemporarilyHidden = true;
	}
}

inline void Context::showCursor() {
	// Restore the cursor visibility if it was temporarily hidden
	if ((textCursorSprite != nullptr) && cursorTemporarilyHidden) {
		textCursorSprite->visible = true;
		cursorTemporarilyHidden = false;
	}
}

void Context::setCursorAppearance(uint8_t appearance) {
	switch (appearance) {
		case 0:		// cursor steady
			cursorFlashing = false;
			break;
		case 1:		// cursor off
			cursorEnabled = false;
			updateTextCursorVisibility();
			break;
		case 2:		// fast flash
			cursorFlashRate = pdMS_TO_TICKS(CURSOR_FAST_PHASE);
			cursorFlashing = true;
			break;
		case 3:		// slow flash
			cursorFlashRate = pdMS_TO_TICKS(CURSOR_PHASE);
			cursorFlashing = true;
			break;
	}
}

void Context::setCursorVStart(uint8_t start) {
	cursorVStart = start;
	updateTextCursorBitmap();
}

void Context::setCursorVEnd(uint8_t end) {
	cursorVEnd = end;
	updateTextCursorBitmap();
}

void Context::setCursorHStart(uint8_t start) {
	cursorHStart = start;
	updateTextCursorBitmap();
}

void Context::setCursorHEnd(uint8_t end) {
	cursorHEnd = end;
	updateTextCursorBitmap();
}

void Context::setPagedMode(PagedMode mode) {
	if (mode > PagedMode::TempEnabled_Enabled) {
		// Unknown mode
		return;
	}
	pagedMode = mode;
	resetPagedModeCount();
}

void Context::setTempPagedMode() {
	switch (pagedMode) {
		case PagedMode::Disabled:
			pagedMode = PagedMode::TempEnabled_Disabled;
			break;
		case PagedMode::Enabled:
			pagedMode = PagedMode::TempEnabled_Enabled;
			break;
	}
}

void Context::clearTempPagedMode() {
	switch (pagedMode) {
		case PagedMode::TempEnabled_Disabled:
			pagedMode = PagedMode::Disabled;
			break;
		case PagedMode::TempEnabled_Enabled:
			pagedMode = PagedMode::Enabled;
			break;
	}
}

void Context::checkPagedMode() {
	if (!textCursorActive()) {
		return;
	}
	if ((pagedMode != PagedMode::Disabled)) {
		pagedModeCount--;
		if (pagedModeCount <= 0) {
			setProcessorState(VDUProcessorState::PagedModePaused);
			return;
		}
	}
	if (ctrlKeyPressed()) {
		if (shiftKeyPressed()) {
			setProcessorState(VDUProcessorState::CtrlShiftPaused);
		} else if (cursorCtrlPauseFrames > 0) {
			setWaitForFrames(cursorCtrlPauseFrames);
		}
	}
}

// Reset basic cursor control
// used when changing screen modes
//
void Context::resetTextCursor() {
	// visual cursor appearance reset
	cursorEnabled = true;
	cursorFlashing = true;
	cursorFlashRate = pdMS_TO_TICKS(CURSOR_PHASE);
	cursorVStart = 0;
	cursorVEnd = 255;
	cursorHStart = 0;
	cursorHEnd = 255;

	updateTextCursorBitmap();

	// reset text viewport
	// and set the active viewport to text
	textViewport =	Rect(0, 0, canvasW - 1, canvasH - 1);
	setActiveCursor(CursorType::Text);

	// cursor behaviour however is _not_ reset here
	cursorHome();
	setPagedMode(PagedMode::Disabled);
}


// Cursor movement

// Move the active cursor up a line
//
void Context::cursorUp(bool moveOnly) {
	auto font = getFont();
	if (cursorBehaviour.flipXY) {
		activeCursor->X += (cursorBehaviour.invertHorizontal ? font->width : -font->width);
	} else {
		activeCursor->Y += (cursorBehaviour.invertVertical ? font->height : -font->height);
	}
	updateTextCursorPosition();
	if (!moveOnly) {
		cursorScrollOrWrap();
	}
}

// Move the active cursor down a line
//
void Context::cursorDown(bool moveOnly) {
	auto font = getFont();
	if (cursorBehaviour.flipXY) {
		activeCursor->X += (cursorBehaviour.invertHorizontal ? -font->width : font->width);
	} else {
		activeCursor->Y += (cursorBehaviour.invertVertical ? -font->height : font->height);
	}
	updateTextCursorPosition();
	if (!moveOnly) {
		cursorScrollOrWrap();
	}
}

// Move the active cursor back one character
//
void Context::cursorLeft() {
	auto font = getFont();
	if (cursorBehaviour.flipXY) {
		activeCursor->Y += (cursorBehaviour.invertVertical ? font->height : -font->height);
	} else {
		activeCursor->X += (cursorBehaviour.invertHorizontal ? font->width : -font->width);
	}
	updateTextCursorPosition();
	if (cursorScrollOrWrap()) {
		// wrapped, so move cursor up a line
		cursorUp();
	}
}

// Advance the active cursor right one character
// NB for scroll protect reasons, auto-newline must be handled by the caller
//
void Context::cursorRight() {
	auto font = getFont();

	if (cursorBehaviour.flipXY) {
		activeCursor->Y += (cursorBehaviour.invertVertical ? -font->height : font->height);
	} else {
		activeCursor->X += (cursorBehaviour.invertHorizontal ? -font->width : font->width);
	}
	updateTextCursorPosition();
}

// Move the active cursor to the leftmost position in the viewport
//
void Context::cursorCR(Point * cursor, Rect * viewport) {
	if (cursorBehaviour.flipXY) {
		cursor->Y = cursorBehaviour.invertVertical ? (viewport->Y2 + 1 - getFont()->height - getYAdjustment()) : viewport->Y1;
	} else {
		cursor->X = cursorBehaviour.invertHorizontal ? (viewport->X2 + 1 - getFont()->width - getXAdjustment()) : viewport->X1;
	}
	updateTextCursorPosition();
}

// Move the active cursor to the top-left position in the viewport
//
void Context::cursorHome(Point * cursor, Rect * viewport) {
	cursorCR(cursor, viewport);
	cursorTop(cursor, viewport);
}

// TAB(x,y)
//
void Context::cursorTab(uint8_t x, uint8_t y) {
	auto font = getFont();
	int xPos, yPos;
	if (cursorBehaviour.flipXY) {
		if (cursorBehaviour.invertHorizontal) {
			xPos = (activeViewport->X2 + 1) - ((y + 1) * font->width) - getXAdjustment();
		} else {
			xPos = activeViewport->X1 + (y * font->width);
		}
		if (cursorBehaviour.invertVertical) {
			yPos = (activeViewport->Y2 + 1) - ((x + 1) * font->height) - getYAdjustment();
		} else {
			yPos = activeViewport->Y1 + (x * font->height);
		}
	} else {
		if (cursorBehaviour.invertHorizontal) {
			xPos = (activeViewport->X2 + 1) - ((x + 1) * font->width) - getXAdjustment();
		} else {
			xPos = activeViewport->X1 + (x * font->width);
		}
		if (cursorBehaviour.invertVertical) {
			yPos = (activeViewport->Y2 + 1) - ((y + 1) * font->height) - getYAdjustment();
		} else {
			yPos = activeViewport->Y1 + (y * font->height);
		}
	}
	if (activeViewport->X1 <= xPos
		&& xPos < activeViewport->X2 - getXAdjustment()
		&& activeViewport->Y1 <= yPos
		&& yPos < activeViewport->Y2 - getYAdjustment()
	) {
		activeCursor->X = xPos;
		activeCursor->Y = yPos;
	}
	updateTextCursorPosition();
}

void Context::cursorRelativeMove(int8_t x, int8_t y) {
	// perform a pixel-relative movement of the cursor
	// does _not_ obey cursor behaviour for directions
	// but does for wrapping and scrolling
	activeCursor->X += x;
	activeCursor->Y += y;
	updateTextCursorPosition();

	// TODO think more about this logic
	if (!textCursorActive() || !cursorBehaviour.scrollProtect) {
		if (cursorIsOffRight()) {
			if (cursorAutoNewline()) {
				checkPagedMode();
			}
		} else {
			cursorScrollOrWrap();
		}
	}
}

void Context::getCursorTextPosition(uint8_t * x, uint8_t * y) {
	auto font = getFont();
	Point p = getNormalisedCursorPosition();
	*x = p.X / font->width;
	*y = p.Y / font->height;
}

void Context::resetPagedModeCount() {
	// set count of rows to print when in paged mode
	uint8_t x, y;
	auto pageRows = getNormalisedViewportCharHeight();
	getCursorTextPosition(&x, &y);
	pagedModeCount = max(pageRows - y, pageRows - pagedModeContext);
}

// Get number of characters remaining beyond the cursor position in the current line
uint8_t Context::getCharsRemainingInLine() {
	uint8_t x, y;
	auto columns = getNormalisedViewportCharWidth();
	getCursorTextPosition(&x, &y);
	int remaining = (columns - 1) - x;
	if (remaining >= 0) {
		return remaining;
	}
	return columns;
}

#endif	// CONTEXT_CURSOR_H
