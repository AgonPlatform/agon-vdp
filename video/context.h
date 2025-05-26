#ifndef CONTEXT_H
#define CONTEXT_H

// Text and graphics system context management
// This includes all cursor, viewport, and graphics contextual data
//

#include <memory>
#include <vector>

#include <fabgl.h>

#include "agon.h"
#include "sprites.h"

extern bool isVDPVariableSet(uint16_t flag);
extern uint16_t getVDPVariable(uint16_t flag);
extern void setVDPVariable(uint16_t flag, uint16_t value);
uint		lastFrameCounter = 0;			// Last frame counter for VSYNC callbacks

// Type declarations for ContextVector and ContextVectorPtr are after the Context class
// contextStacks global variable also defined after the Context class

// Support structures

typedef union {
	uint8_t value = 0;
	struct {
		uint8_t scrollProtect : 1;
		uint8_t invertHorizontal : 1;
		uint8_t invertVertical : 1;
		uint8_t flipXY : 1;
		uint8_t yWrap : 1;
		uint8_t xHold : 1;
		uint8_t grNoSpecialActions : 1;
		uint8_t _undefined : 1;
	};
} CursorBehaviour;

enum class CursorType : uint8_t {
	Text,
	Graphics,
};

enum class ViewportType : uint8_t {
	Text = 0,		// Text viewport
	Default,		// Default (whole screen) viewport
	Graphics,		// Graphics viewport
	Active,			// Active viewport
};


// Context object
//
class Context {
	private:
		// Font tracking
		// "activating" a context will need to set the font to the current font
		std::shared_ptr<fabgl::FontInfo>	font;				// Current active font
		std::shared_ptr<fabgl::FontInfo>	textFont;			// Current active font for text cursor
		std::shared_ptr<fabgl::FontInfo>	graphicsFont;		// Current active font for graphics cursor
		std::shared_ptr<BufferStream>		textFontData;
		std::shared_ptr<BufferStream>		graphicsFontData;

		// VDU command processor state info
		VDUProcessorState		processorState = VDUProcessorState::Active;	// Current VDU command processor state
		uint8_t			waitForFrames = 0;				// Count of frames for WaitForFrames state
		
		// Cursor management data
		bool			cursorEnabled = true;			// Cursor visibility
		bool			cursorFlashing = true;			// Cursor is flashing
		uint16_t		cursorFlashRate = pdMS_TO_TICKS(CURSOR_PHASE);	// Cursor flash rate
		CursorBehaviour cursorBehaviour;				// Cursor behavior byte
		Point			textCursor;						// Text cursor
		Point *			activeCursor = &textCursor;		// Pointer to the active text cursor (textCursor or p1)
		bool			cursorShowing = false;			// Cursor is currently showing on screen
		bool			cursorTemporarilyHidden = false;	// Cursor is temporarily hidden for command processing
		TickType_t		cursorTime;						// Time of last cursor flash event
		uint8_t			cursorCtrlPauseFrames = 3;		// Number of frames to pause on newline when ctrl held

		// Cursor rendering
		uint8_t			cursorVStart;					// Cursor vertical start offset
		uint8_t			cursorVEnd;						// Cursor vertical end
		uint8_t			cursorHStart;					// Cursor horizontal start offset
		uint8_t			cursorHEnd;						// Cursor horizontal end

		// Paged mode tracking
		PagedMode 		pagedMode = PagedMode::Disabled;	// Is output paged or not? Set by VDU 14 and 15
		uint8_t			pagedModeCount = 0;				// Remaining rows in paged mode
		uint8_t			pagedModeContext = 6;			// Number of context rows when paged mode enabled

		// Viewport management data
		Rect *			activeViewport;					// Pointer to the active text viewport (textViewport or graphicsViewport)
		Rect			defaultViewport;				// Default viewport
		Rect			textViewport;					// Text viewport
		Rect			graphicsViewport;				// Graphics viewport

		// Graphics management data
		fabgl::PaintOptions			gpofg;				// Graphics paint options foreground
		fabgl::PaintOptions			gpobg;				// Graphics paint options background
		fabgl::PaintOptions			tpo;				// Text paint options
		fabgl::PaintOptions			cpo;				// Cursor paint options
		RGB888			gfg, gbg;						// Graphics foreground and background colour
		RGB888			tfg, tbg;						// Text foreground and background colour
		uint8_t			gfgc, gbgc, tfgc, tbgc;			// Logical colour values for graphics and text
		uint8_t			lineThickness = 1;				// Line thickness
		uint16_t		currentBitmap = BUFFERED_BITMAP_BASEID;	// Current bitmap ID
		uint16_t		bitmapTransform = -1;			// Bitmap transform buffer ID
		fabgl::LinePattern	linePattern = fabgl::LinePattern();				// Dotted line pattern
		uint8_t			linePatternLength = 8;			// Dotted line pattern length
		std::vector<uint16_t>	charToBitmap = std::vector<uint16_t>(256, 65535);	// character to bitmap mapping
		bool			plottingText = false;			// Are we currently plotting text?
		bool			logicalCoords = true;			// Use BBC BASIC logical coordinates

		Point			origin;							// Screen origin
		Point			uOrigin;						// Screen origin, un-scaled
		Point			p1, p2, p3;						// Coordinate store for plot (p1 = the graphics cursor)
		Point			rp1;							// Relative coordinates store for plot
		Point			up1;							// Unscaled coordinates store for plot
		std::vector<Point>	pathPoints;					// Storage for path points
		uint8_t			lastPlotCommand = 0;			// Tracking of last plot command to allow continuing plots

		// Cursor management functions
		int getXAdjustment();
		int getYAdjustment();
		int getNormalisedViewportWidth();
		int getNormalisedViewportHeight();
		Point getNormalisedCursorPosition();
		Point getNormalisedCursorPosition(Point * cursor);

		bool cursorIsOffRight();
		bool cursorIsOffLeft();
		bool cursorIsOffTop();
		bool cursorIsOffBottom();

		void cursorEndRow();
		void cursorEndRow(Point * cursor, Rect * viewport);
		void cursorTop();
		void cursorTop(Point * cursor, Rect * viewport);
		void cursorEndCol();
		void cursorEndCol(Point * cursor, Rect * viewport);

		void cursorAutoNewline();
		void ensureCursorInViewport(Rect viewport);

		// Viewport management functions
		Rect * getViewport(ViewportType type);
		bool setTextViewport(Rect rect);
		Point invScale(Point p);

		// Font management functions
		const fabgl::FontInfo * getFont();
		void changeFont(std::shared_ptr<fabgl::FontInfo> newFont, std::shared_ptr<BufferStream> fontData, uint8_t flags);
		bool cmpChar(uint8_t * c1, uint8_t *c2, uint8_t len);
		char getScreenChar(Point p);
		inline void setCharacterOverwrite(bool overwrite);		// TODO integrate into setActiveCursor?
		inline std::shared_ptr<Bitmap> getBitmapFromChar(uint8_t c) {
			return getBitmap(charToBitmap[c]);
		}

		// Graphics functions
		fabgl::PaintOptions getPaintOptions(fabgl::PaintMode mode, fabgl::PaintOptions priorPaintOptions);
		void setGraphicsOptions(uint8_t mode);
		void setGraphicsFill(uint8_t mode);
		void updateColours(uint8_t logical, uint8_t physical);
		inline void setClippingRect(Rect rect);

		void pushPoint(Point p);
		void pushPointRelative(int16_t x, int16_t y);

		void moveTo();
		void plotLine(bool omitFirstPoint, bool omitLastPoint, bool usePattern, bool resetPattern);
		void plotPoint();
		void fillHorizontalLine(bool scanLeft, bool match, RGB888 matchColor);
		void plotTriangle();
		void plotRectangle();
		void plotParallelogram();
		void plotCircle(bool filled);
		void plotArc();
		void plotSegment();
		void plotSector();
		void plotCopyMove(uint8_t mode);
		void plotPath(uint8_t mode, uint8_t lastMode);
		void plotBitmap(uint8_t mode);

		void clearViewport(ViewportType viewport);
		void scrollRegion(Rect * region, uint8_t direction, int16_t movement);

		uint16_t scanH(int16_t x, int16_t y, RGB888 colour, int8_t direction);
		uint16_t scanHToMatch(int16_t x, int16_t y, RGB888 colour, int8_t direction);

	public:

		// Constructor
		Context() {
			cursorTime = xTaskGetTickCountFromISR();
			reset();
		};

		// Copy constructor
		Context(const Context &c);

		bool readVariable(uint16_t var, uint16_t * value);
		void setVariable(uint16_t var, uint16_t value);

		inline VDUProcessorState getProcessorState();
		void setProcessorState(VDUProcessorState state);
		void setWaitForFrames(uint8_t frames);
		bool checkForVSYNC();

		// Cursor management functions
		void hideCursor();
		void showCursor();
		void doCursorFlash();
		inline bool textCursorActive();
		inline void setActiveCursor(CursorType type);
		inline void setCursorBehaviour(uint8_t setting, uint8_t mask);
		inline void enableCursor(uint8_t enable);
		void setCursorAppearance(uint8_t appearance);
		void setCursorVStart(uint8_t start);
		void setCursorVEnd(uint8_t end);
		void setCursorHStart(uint8_t start);
		void setCursorHEnd(uint8_t end);
		void setPagedMode(PagedMode mode);
		void setTempPagedMode();
		void clearTempPagedMode();
		void resetTextCursor();

		void cursorUp();
		void cursorUp(bool moveOnly);
		void cursorDown();
		void cursorDown(bool moveOnly);
		void cursorLeft();
		void cursorRight();
		void cursorRight(bool scrollProtect);
		void cursorCR();
		void cursorCR(Point * cursor, Rect * viewport);
		void cursorHome();
		void cursorHome(Point * cursor, Rect * viewport);
		void cursorTab(uint8_t x, uint8_t y);
		void cursorRelativeMove(int8_t x, int8_t y);
		void getCursorTextPosition(uint8_t * x, uint8_t * y);
		bool cursorScrollOrWrap();
		void resetPagedModeCount();
		uint8_t getCharsRemainingInLine();

		// Viewport management functions
		void viewportReset();
		void setActiveViewport(ViewportType type);
		bool setGraphicsViewport(Point p1, Point p2);
		bool setGraphicsViewport(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
		bool setGraphicsViewport();
		bool setTextViewport(uint8_t cx1, uint8_t cy1, uint8_t cx2, uint8_t cy2);
		bool setTextViewport();
		uint8_t getNormalisedViewportCharWidth();
		uint8_t getNormalisedViewportCharHeight();
		void setOrigin(int x, int y);
		void setOrigin();
		void shiftOrigin();
		void setLogicalCoords(bool b);
		Point scale(int16_t X, int16_t Y);
		Point toCurrentCoordinates(int16_t X, int16_t Y);
		Point toScreenCoordinates(int16_t X, int16_t Y);

		// Font management functions
		void changeFont(uint16_t newFontId, uint8_t flags);
		void resetFonts();
		bool usingSystemFont();
		char getScreenChar(uint8_t x, uint8_t y);
		char getScreenCharAt(uint16_t px, uint16_t py);
		void mapCharToBitmap(uint8_t c, uint16_t bitmapId);
		void unmapBitmapFromChars(uint16_t bitmapId);
		void resetCharToBitmap();
		
		// Graphics functions
		inline void setCurrentBitmap(uint16_t b) {
			currentBitmap = b;
		}
		inline uint16_t getCurrentBitmapId() {
			return currentBitmap;
		}

		void setLineThickness(uint8_t thickness);
		void setDottedLinePattern(uint8_t pattern[8]);
		void setDottedLinePatternLength(uint8_t length);

		void setTextColour(uint8_t colour);
		void setGraphicsColour(uint8_t mode, uint8_t colour);
		bool getColour(uint8_t colour, RGB888 * pixel);
		RGB888 getPixel(uint16_t x, uint16_t y);
		static void updateColoursInAllContexts(uint8_t logical, uint8_t physical);

		void pushPoint(uint16_t x, uint16_t y);
		Rect getGraphicsRect();							// Used by sprites system to capture screen area

		bool plot(int16_t x, int16_t y, uint8_t command);
		void plotPending(int16_t peeked);

		void plotString(const std::string & s);
		void plotBackspace();
		void drawBitmap(uint16_t x, uint16_t y, bool compensateHeight, bool forceSet);
		void drawCursor(Point p);

		void setAffineTransform(uint8_t flags, uint16_t bufferId);

		void cls();
		void clg();
		void scrollRegion(ViewportType viewport, uint8_t direction, int16_t movement);

		void resetGraphicsPainting();
		void resetGraphicsOptions();
		void resetGraphicsPositioning();
		void resetTextPainting();

		void reset();
		void activate();
};

using ContextVector = std::vector<std::shared_ptr<Context>, psram_allocator<std::shared_ptr<Context>>>;
using ContextVectorPtr = std::shared_ptr<ContextVector>;
std::unordered_map<uint16_t, ContextVectorPtr,
	std::hash<uint16_t>, std::equal_to<uint16_t>,
	psram_allocator<std::pair<const uint16_t, ContextVectorPtr>>> contextStacks;


Context::Context(const Context &c) {
	// Copy almost all the data
	// VDU command processor state is explicitly not copied, so will get defaults

	// Font tracking
	font = c.font;
	textFont = c.textFont;
	graphicsFont = c.graphicsFont;
	textFontData = c.textFontData;
	graphicsFontData = c.graphicsFontData;

	// Text cursor management data
	cursorEnabled = c.cursorEnabled;
	cursorFlashing = c.cursorFlashing;
	cursorFlashRate = c.cursorFlashRate;
	cursorBehaviour = c.cursorBehaviour;
	textCursor = c.textCursor;
	cursorVStart = c.cursorVStart;
	cursorVEnd = c.cursorVEnd;
	cursorHStart = c.cursorHStart;
	cursorHEnd = c.cursorHEnd;
	// Data related to cursor rendering
	cursorShowing = c.cursorShowing;
	cursorTemporarilyHidden = c.cursorTemporarilyHidden;
	cursorTime = c.cursorTime;
	cursorCtrlPauseFrames = c.cursorCtrlPauseFrames;

	pagedMode = (PagedMode)((uint8_t)c.pagedMode & 1);
	pagedModeCount = c.pagedModeCount;
	pagedModeContext = c.pagedModeContext;

	// Viewport management data
	defaultViewport = c.defaultViewport;
	textViewport = c.textViewport;
	graphicsViewport = c.graphicsViewport;

	// Graphics painting options
	gpofg = c.gpofg;
	gpobg = c.gpobg;
	gfg = c.gfg;
	gbg = c.gbg;
	gfgc = c.gfgc;
	gbgc = c.gbgc;
	lineThickness = c.lineThickness;
	currentBitmap = c.currentBitmap;
	linePattern.setPattern(c.linePattern.pattern);
	linePatternLength = c.linePatternLength;

	// Graphics positioning data
	logicalCoords = c.logicalCoords;
	origin = c.origin;
	uOrigin = c.uOrigin;
	p1 = c.p1;
	p2 = c.p2;
	p3 = c.p3;
	rp1 = c.rp1;
	up1 = c.up1;
	// pathPoints and lastPlotCommand are currently completely transient, so don't need to be copied

	// Text painting options
	tfg = c.tfg;
	tbg = c.tbg;
	tfgc = c.tfgc;
	tbgc = c.tbgc;
	tpo = c.tpo;
	cpo = c.cpo;
	charToBitmap = c.charToBitmap;

	if (c.activeCursor == &c.textCursor) {
		activeCursor = &textCursor;
	} else {
		activeCursor = &p1;
	}

	if (c.activeViewport == &c.graphicsViewport) {
		activeViewport = &graphicsViewport;
	} else {
		activeViewport = &textViewport;
	}
}

void Context::updateColoursInAllContexts(uint8_t logical, uint8_t physical) {
	for (auto& stackPair : contextStacks) {
		auto contextStack = stackPair.second;
		if (contextStack) {
			for (auto& context : *contextStack) {
				if (context) {
					context->updateColours(logical, physical);
				}
			}
		}
	}
}

bool Context::readVariable(uint16_t var, uint16_t * value) {
	if (var >= VDU_VAR_PALETTE && var <= VDU_VAR_PALETTE_END) {
		if (value) {
			*value = palette[(var - VDU_VAR_PALETTE) & (getVGAColourDepth() - 1)];
		}
		return true;
	}
	if (var >= VDU_VAR_CHARMAPPING && var <= VDU_VAR_CHARMAPPING_END) {
		auto c = var - VDU_VAR_CHARMAPPING;
		if (charToBitmap[c] != 65535) {
			if (value) {
				*value = charToBitmap[c];
			}
			return true;
		}
		return false;
	}
	switch (var) {
		// Mode variables
		// 0 is "mode flags" - omitting for now
		case 1:		// Text columns - 1 (characters)
			if (value) {
				*value = (canvasW / getFont()->width) - 1;
			}
			break;
		case 2:		// Text rows - 1
			if (value) {
				*value = (canvasH / getFont()->height) - 1;
			}
			break;
		case 3:		// Max logical colour
			if (value) {
				*value = getVGAColourDepth() - 1;
			}
			break;
		// Variables 4 and 5 are X and Y Eigen factor - omitting for now
		// Omitting variables 6-10 as they aren't really relevant without direct screen memory access
		case 11:	// Screen width in pixels - 1
			if (value) {
				*value = canvasW - 1;
			}
			break;
		case 12:	// Screen height in pixels - 1
			if (value) {
				*value = canvasH - 1;
			}
			break;
		case 13:	// Number of screen banks
			if (value) {
				*value = isDoubleBuffered() ? 2 : 1;
			}
			break;

		// Variables 14-127 are currently undefined
		// so _some_ will be used for Agon-specific variables

		case 0x17:	// 23 - Line thickness
			if (value) {
				*value = lineThickness;
			}
			break;

		// Text cursor absolute position
		// NB this does not take into account cursor behaviour
		case 0x18:	// Text cursor, absolute X position (chars)
			if (value) {
				*value = textCursor.X / getFont()->width;
			}
			break;
		case 0x19:	// Text cursor, absolute Y position (chars)
			if (value) {
				*value = textCursor.Y / getFont()->height;
			}
			break;

		case 0x20:	// Frame counter low
			if (value) {
				*value = lastFrameCounter & 0xFFFF;
			}
			break;
		case 0x21:	// Frame counter high
			if (value) {
				*value = lastFrameCounter >> 16;
			}
			break;
		case 0x22:	// Number of frames to pause on newline when ctrl held
			if (value) {
				*value = cursorCtrlPauseFrames;
			}
			break;
		case 0x23:	// Number of frames currently being waited for
			if (value) {
				*value = waitForFrames;
			}
			break;

		case 0x55:	// Current screen mode number
			if (value) {
				*value = videoMode;
			}
			break;
		case 0x56:	// Legacy modes flag
			if (value) {
				*value = legacyModes ? 1 : 0;
			}
			break;
		case 0x57:	// Coordinate system (1 = logical/OS coordinates, 0 = screen)
			if (value) {
				*value = logicalCoords ? 1 : 0;
			}
			break;
		case 0x58:	// Paged mode flag
			if (value) {
				*value = (uint16_t)pagedMode;
			}
			break;
		case 0x59:	// Paged mode context amount
			if (value) {
				*value = pagedModeContext;
			}
			break;

		case 0x66:	// Cursor behaviour
			if (value) {
				*value = cursorBehaviour.value;
			}
			break;
		case 0x67:	// Cursor enabled
			if (value) {
				*value = cursorEnabled ? 1 : 0;
			}
			break;
		case 0x68:	// Cursor hstart
			if (value) {
				*value = cursorHStart;
			}
			break;
		case 0x69:	// Cursor hend
			if (value) {
				*value = cursorHEnd;
			}
			break;
		case 0x6A:	// Cursor vstart
			if (value) {
				*value = cursorVStart;
			}
			break;
		case 0x6B:	// Cursor vend
			if (value) {
				*value = cursorVEnd;
			}
			break;
		// Space for cursor timing variables etc
		case 0x70:	// Active cursor type (read only)
			if (value) {
				*value = (activeCursor == &textCursor) ? 0 : 1;
			}
			break;

		// VDU Variables

		// - text and graphics windows
		case 0x80:	// Graphics window, LH column, pixel coordinates
			if (value) {
				*value = getViewport(ViewportType::Graphics)->X1;
			}
			break;
		case 0x81:	// Graphics window, Bottom row, pixel coordinates
			if (value) {
				*value = getViewport(ViewportType::Graphics)->Y2;
			}
			break;
		case 0x82:	// Graphics window, RH column, pixel coordinates
			if (value) {
				*value = getViewport(ViewportType::Graphics)->X2;
			}
			break;
		case 0x83:	// Graphics window, Top row, pixel coordinates
			if (value) {
				*value = getViewport(ViewportType::Graphics)->Y1;
			}
			break;
		case 0x84:	// Text window, LH column, character coordinates
			if (value) {
				*value = getViewport(ViewportType::Text)->X1 / getFont()->width;
			}
			break;
		case 0x85:	// Text window, Bottom row, character coordinates
			if (value) {
				*value = getViewport(ViewportType::Text)->Y2 / getFont()->height;
			}
			break;
		case 0x86:	// Text window, RH column, character coordinates
			if (value) {
				*value = getViewport(ViewportType::Text)->X2 / getFont()->width;
			}
			break;
		case 0x87:	// Text window, Top row, character coordinates
			if (value) {
				*value = getViewport(ViewportType::Text)->Y1 / getFont()->height;
			}
			break;

		// Graphics origin
		// NB these are OS coordinates, as per Acorn's implementation
		// we may wish to add a parallel set of variables for screen coordinates
		case 0x88:	// Graphics origin, X, OS coordinates
			if (value) {
				*value = uOrigin.X;
			}
			break;
		case 0x89:	// Graphics origin, Y, OS coordinates
			if (value) {
				*value = uOrigin.Y;
			}
			break;

		// Graphics cursor data
		case 0x8A:	// Graphics cursor, X, OS coordinates
			if (value) {
				*value = up1.X;
			}
			break;
		case 0x8B:	// Graphics cursor, Y, OS coordinates
			if (value) {
				*value = up1.Y;
			}
			break;
		case 0x8C:	// Oldest Graphics cursor, X, screen coordinates
			if (value) {
				*value = p3.X;
			}
			break;
		case 0x8D:	// Oldest Graphics cursor, Y, screen coordinates
			if (value) {
				*value = p3.Y;
			}
			break;
		case 0x8E:	// Previous Graphics cursor, X, screen coordinates
			if (value) {
				*value = p2.X;
			}
			break;
		case 0x8F:	// Previous Graphics cursor, Y, screen coordinates
			if (value) {
				*value = p2.Y;
			}
			break;
		case 0x90:	// Graphics cursor, X, screen coordinates
		case 0x92:	// New point, X, screen coordinates
			if (value) {
				*value = p1.X;
			}
			break;
		case 0x91:	// Graphics cursor, Y, screen coordinates
		case 0x93:	// New point, X, screen coordinates
			if (value) {
				*value = p1.Y;
			}
			break;
		// Acorn has variables 0x92 and 0x93 as "new point"
		// but it is not clear how they would differ from the current graphics cursor
		// so we are treating them as the same

		// Variables 0x94-0x96 are not relevant on Agon, as there is no direct screen memory access

		// GCOL actions and selected colours
		case 0x97:	// GCOL action for foreground colour
			if (value) {
				*value = (uint8_t)gpofg.mode;
			}
			break;
		case 0x98:	// GCOL action for background colour
			if (value) {
				*value = (uint8_t)gpobg.mode;
			}
			break;
		case 0x99:	// Graphics foreground (logical) colour
			if (value) {
				*value = gfgc;
			}
			break;
		case 0x9A:	// Graphics background (logical) colour
			if (value) {
				*value = gbgc;
			}
			break;
		case 0x9B:	// Text foreground (logical) colour
			if (value) {
				*value = tfgc;
			}
			break;
		case 0x9C:	// Text background (logical) colour
			if (value) {
				*value = tbgc;
			}
			break;
		// Variables &9D-&A0 are not relevant on Agon, as they are "tint" values which we can't support

		case 0xA1:	// Max mode number (not double-buffered)
			// NB this is hard-coded
			if (value) {
				*value = 23;
			}
			break;

		// Font size info
		// NB Agon currently doesn't support changing font spacing
		case 0xA2:	// X font size, graphics cursor
			if (value) {
				*value = graphicsFont ? graphicsFont->width : 8;
			}
			break;
		case 0xA3:	// Y font size, graphics cursor
			if (value) {
				*value = graphicsFont ? graphicsFont->height : 8;
			}
			break;
		case 0xA4:	// X font spacing, graphics cursor
			if (value) {
				*value = graphicsFont ? graphicsFont->width : 8;
			}
			break;
		case 0xA5:	// Y font spacing, graphics cursor
			if (value) {
				*value = graphicsFont ? graphicsFont->height : 8;
			}
			break;
		// &A6 omitted as it is not relevant on Agon ("address of horizontal line-draw routine")
		case 0xA7:	// X font size, text cursor
			if (value) {
				*value = textFont ? textFont->width : 8;
			}
			break;
		case 0xA8:	// Y font size, text cursor
			if (value) {
				*value = textFont ? textFont->height : 8;
			}
			break;
		case 0xA9:	// X font spacing, text cursor
			if (value) {
				*value = textFont ? textFont->width : 8;
			}
			break;
		case 0xAA:	// Y font spacing, text cursor
			if (value) {
				*value = textFont ? textFont->height : 8;
			}
			break;
		// NB we have more font info available so may add more variables to expose some of it
		// also note that if/when we support variable width fonts, the width here will be zero

		// RISC OS also defines variables &AB, &AC, &AE-B1 and &C0 which are not relevant on Agon

		// Line pattern info
		case 0xF2:	// Line pattern length
			if (value) {
				*value = linePatternLength;
			}
			break;
		case 0xF3:	// Line pattern, bytes 0-1
			if (value) {
				*value = reinterpret_cast<uint16_t *>(linePattern.pattern)[0];
			}
			break;
		case 0xF4:	// Line pattern, bytes 2-3
			if (value) {
				*value = reinterpret_cast<uint16_t *>(linePattern.pattern)[1];
			}
			break;
		case 0xF5:	// Line pattern, bytes 4-5
			if (value) {
				*value = reinterpret_cast<uint16_t *>(linePattern.pattern)[2];
			}
			break;
		case 0xF6:	// Line pattern, bytes 6-7
			if (value) {
				*value = reinterpret_cast<uint16_t *>(linePattern.pattern)[3];
			}
			break;

		// Text window size, as per RISC OS
		case 0x100:		// width of text window in chars
			if (value) {
				// We will assume that 
				*value = getNormalisedViewportCharWidth();
			}
			break;
		case 0x101:		// height of text window in chars
			if (value) {
				// Acorn seems to reduce this value by 1 on RISC OS
				*value = getNormalisedViewportCharHeight() - 1;
			}
			break;

		// we have other areas of context state that could be exposed as variables
		// such as cursor size info, cursor drawing status, line thickness, line pattern, etc.
		// plus we wish to expose the current context ID
		// Exposing the context ID needs the "default context" changes to be merged

		// Text cursor, character position, as per TAB within text window
		case 0x118:		// X position of text cursor within text window
			if (value) {
				uint8_t x, y;
				getCursorTextPosition(&x, &y);
				*value = x;
			}
			break;
		case 0x119:		// Y position of text cursor within text window
			if (value) {
				uint8_t x, y;
				getCursorTextPosition(&x, &y);
				*value = y;
			}
			break;
		// Text cursor position, in screen coordinates
		case 0x11A:		// X position of text cursor in screen coordinates
			if (value) {
				*value = textCursor.X;
			}
			break;
		case 0x11B:		// Y position of text cursor in screen coordinates
			if (value) {
				*value = textCursor.Y;
			}
			break;

		case 0x400:	// Current bitmap ID
			if (value) {
				*value = currentBitmap;
			}
			break;
		case 0x401:	// Count of bitmaps used
			if (value) {
				*value = bitmaps.size();
			}
			break;
		case 0x402:	// Current bitmap transform ID
			if (value) {
				*value = bitmapTransform;
			}
			break;

		case 0x410:	// Current sprite ID
			if (value) {
				*value = current_sprite;
			}
			break;
		case 0x411:	// Number of sprites in use (not necessarily visible)
			if (value) {
				*value = numsprites;
			}
			break;
		// case 0x412:	// Current sprite transform ID - not supported

		case 0x440:	// Mouse cursor ID
		case 0x441:	// Mouse cursor enabled
		case 0x442:	// Mouse cursor X position (pixel coords)
		case 0x443:	// Mouse cursor Y position
		case 0x444:	// Mouse cursor button status
		case 0x445:	// Mouse wheel delta
		case 0x446:	// Mouse sample rate
		case 0x447:	// Mouse resolution
		case 0x448:	// Mouse scaling
		case 0x449:	// Mouse acceleration
		case 0x44A: {	// Mouse wheel acceleration
			auto flagId = (var - 0x440) + VDPVAR_MOUSE_CURSOR;
			auto flagExists = isVDPVariableSet(flagId);
			if (flagExists) {
				*value = getVDPVariable(flagId);
			} else {
				// This shouldn't happen, but just in case
				return false;
			}
		}	break;

		default:
			debug_log("readVariable: variable %d not found\n\r", var);
			return false;
	}
	
	return true;
}

void Context::setVariable(uint16_t var, uint16_t value) {
	if (var >= VDU_VAR_PALETTE && var <= VDU_VAR_PALETTE_END) {
		if (value >= 64) {
			return;
		}
		// NB Setting a palette entry via the VDU variable explicitly does not
		// trigger a palette update event or update the "last colour" VDP variables
		auto logical = (var - VDU_VAR_PALETTE) & (getVGAColourDepth() - 1);
		auto physical = setLogicalPalette(logical, value, 0, 0, 0);
		if (physical != -1) {
			Context::updateColoursInAllContexts(logical, physical);
		}
		return;
	}
	if (var >= VDU_VAR_CHARMAPPING && var <= VDU_VAR_CHARMAPPING_END) {
		mapCharToBitmap(var - VDU_VAR_CHARMAPPING, value);
		return;
	}
	switch (var) {
		// Mode variables (0-13)
		// All mode variables are read-only

		// Variables 14-127 are currently undefined, but we will use some for agon-specific variables
		case 0x17:	// 23 - line thickness
			setLineThickness(value);
			break;

		// BBC Micro had a few text cursor things which seem to be the following
		// It's not clear _exactly_ how these work, and whether they obey cursor behaviour
		// On the Agon, we will present a simplistic absolute cursor position
		// and provide a parallel set of variables for cursor position within the text window
		//    &18       Current absolute text X position (=POS+vduvar &08)
		//    &19       Current absolute text Y position (=VPOS+vduvar &0B)
		//    &64       Text input absolute X position (for copy-source cursor - N/A on Agon)
		//    &65       Text input absolute Y position

		case 0x18:	// Text cursor, absolute X (chars)
			textCursor.X = value * getFont()->width;
			ensureCursorInViewport(textViewport);
			break;
		case 0x19:	// Text cursor, absolute Y (chars)
			textCursor.Y = value * getFont()->height;
			ensureCursorInViewport(textViewport);
			break;

		case 0x20:	// Frame counter low
			lastFrameCounter = (lastFrameCounter & 0xFFFF0000) | (value & 0xFFFF);
			_VGAController->frameCounter = lastFrameCounter;
			break;
		case 0x21:	// Frame counter high
			lastFrameCounter = (lastFrameCounter & 0xFFFF) | (value << 16);
			_VGAController->frameCounter = lastFrameCounter;
			break;
		case 0x22:	// Number of frames to pause on newline when ctrl held
			cursorCtrlPauseFrames = value;
			break;
		case 0x23:	// Number of frames currently being waited for
			setWaitForFrames(value);
			break;

		case 0x56:	// Legacy modes flag
			setLegacyModes(value);
			break;
		case 0x57:	// Coordinate system (1 = logical/OS coordinates, 0 = screen)
			setLogicalCoords(value);
			break;
		case 0x58:	// Paged mode flag
			setPagedMode((PagedMode)value);
			break;
		case 0x59:	// Paged mode context amount
			pagedModeContext = value;
			break;

		case 0x66:	// Cursor behaviour
			setCursorBehaviour(value, 0);
			break;
		case 0x67:	// Cursor enabled (boolean)
			enableCursor(value);
			break;
		case 0x68:	// Cursor hstart
			setCursorHStart(value);
			break;
		case 0x69:	// Cursor hend
			setCursorHEnd(value);
			break;
		case 0x6A:	// Cursor vstart
			setCursorVStart(value & 0x1F);
			break;
		case 0x6B:	// Cursor vend
			setCursorVEnd(value);
			break;
		case 0x6C:	// Cursor appearance
			setCursorAppearance(value & 0x03);
			break;
		// then we have vertical start row, vertical end row, horizontal start column, horizontal end column
		// the cursor frequency stuff, and whether the cursor is on or off
		// plus which is the "active" cursor (text or graphics) for text output

		// VDU Variables

		// - text and graphics windows 0x80-0x87
		// Currently read-only, as changing them here could break things
		// since we need to ensure that x,y pairs are correctly ordered

		// - text and graphics windows
		case 0x80: {	// Graphics window, LH column, pixel coordinates
			uint16_t y1, x2, y2;
			readVariable(0x81, &y1);
			readVariable(0x82, &x2);
			readVariable(0x83, &y2);
			setGraphicsViewport(Point(value, y1), Point(x2, y2));
		}	break;
		case 0x81: {	// Graphics window, Bottom row, pixel coordinates
			uint16_t x1, x2, y2;
			readVariable(0x80, &x1);
			readVariable(0x82, &x2);
			readVariable(0x83, &y2);
			setGraphicsViewport(Point(x1, value), Point(x2, y2));
		}	break;
		case 0x82: {	// Graphics window, RH column, pixel coordinates
			uint16_t x1, y1, y2;
			readVariable(0x80, &x1);
			readVariable(0x81, &y1);
			readVariable(0x83, &y2);
			setGraphicsViewport(Point(x1, y1), Point(value, y2));
		}	break;
		case 0x83: {	// Graphics window, Top row, pixel coordinates
			uint16_t x1, y1, x2;
			readVariable(0x80, &x1);
			readVariable(0x81, &y1);
			readVariable(0x82, &x2);
			setGraphicsViewport(Point(x1, y1), Point(x2, value));
		}	break;
		case 0x84: {	// Text window, LH column, character coordinates
			uint16_t y1, x2, y2;
			readVariable(0x85, &y1);
			readVariable(0x86, &x2);
			readVariable(0x87, &y2);
			setTextViewport(value, y2, x2, y1);
		}	break;
		case 0x85: {	// Text window, Bottom row, character coordinates
			uint16_t x1, y2, x2;
			readVariable(0x84, &x1);
			readVariable(0x86, &x2);
			readVariable(0x87, &y2);
			setTextViewport(x1, y2, x2, value);
		}	break;
		case 0x86: {	// Text window, RH column, character coordinates
			uint16_t x1, y1, y2;
			readVariable(0x84, &x1);
			readVariable(0x85, &y1);
			readVariable(0x87, &y2);
			setTextViewport(x1, y2, value, y1);
		}	break;
		case 0x87: {	// Text window, Top row, character coordinates
			uint16_t x1, y1, x2;
			readVariable(0x84, &x1);
			readVariable(0x85, &y1);
			readVariable(0x86, &x2);
			setTextViewport(x1, value, x2, y1);
		}	break;

		// Graphics origin (0x88-0x89)
		case 0x88:	// Graphics origin, X, OS coordinates
			setOrigin(value, uOrigin.Y);
			break;
		case 0x89:	// Graphics origin, Y, OS coordinates
			setOrigin(uOrigin.X, value);
			break;

		// Graphics cursor data
		case 0x8A:	// Graphics cursor, X, OS coordinates
			up1.X = value;
			p1 = toScreenCoordinates(up1.X, up1.Y);
			break;
		case 0x8B:	// Graphics cursor, Y, OS coordinates
			up1.Y = value;
			p1 = toScreenCoordinates(up1.X, up1.Y);
			break;
		case 0x8C:	// Oldest Graphics cursor, X, screen coordinates
			p3.X = value;
			break;
		case 0x8D:	// Oldest Graphics cursor, Y, screen coordinates
			p3.Y = value;
			break;
		case 0x8E:	// Previous Graphics cursor, X, screen coordinates
			p2.X = value;
			break;
		case 0x8F:	// Previous Graphics cursor, Y, screen coordinates
			p2.Y = value;
			break;
		case 0x90:	// Graphics cursor, X, screen coordinates
		case 0x92:	// New point, X, screen coordinates
			p1.X = value;
			up1.X = toCurrentCoordinates(p1.X, p1.Y).X;
			break;
		case 0x91:	// Graphics cursor, Y, screen coordinates
		case 0x93:	// New point, X, screen coordinates
			p1.Y = value;
			up1.Y = toCurrentCoordinates(p1.X, p1.Y).Y;
			break;

		// Variables 0x94-0x96 are not relevant on Agon, as there is no direct screen memory access

		// GCOL actions and selected colours
		case 0x97:	// GCOL action for foreground colour
			if (value <= 7) {
				gpofg = getPaintOptions((fabgl::PaintMode)value, gpofg);
			}
			break;
		case 0x98:	// GCOL action for background colour
			if (value <= 7) {
				gpobg = getPaintOptions((fabgl::PaintMode)value, gpobg);
			}
			break;
		case 0x99:	// Graphics foreground (logical) colour
			setGraphicsColour((uint8_t)gpofg.mode, value & 63);
			break;
		case 0x9A:	// Graphics background (logical) colour
			setGraphicsColour((uint8_t)gpobg.mode, (value & 63) + 128);
			break;
		case 0x9B:	// Text foreground (logical) colour
			setTextColour(value & 63);
			break;
		case 0x9C:	// Text background (logical) colour
			setTextColour((value & 63) + 128);
			break;
		// Variables &9D-&A0 are not relevant on Agon, as they are "tint" values which we can't support

		// Max mode number (0xA1) and font info (0xA2-0xAA) are read-only

		// Line pattern info
		case 0xF2:	// Line pattern length
			setDottedLinePatternLength(value);
			break;
		case 0xF3:	// Line pattern, bytes 0-1
			reinterpret_cast<uint16_t *>(linePattern.pattern)[0] = value;
			setDottedLinePattern(linePattern.pattern);
			break;
		case 0xF4:	// Line pattern, bytes 2-3
			reinterpret_cast<uint16_t *>(linePattern.pattern)[1] = value;
			setDottedLinePattern(linePattern.pattern);
			break;
		case 0xF5:	// Line pattern, bytes 4-5
			reinterpret_cast<uint16_t *>(linePattern.pattern)[2] = value;
			setDottedLinePattern(linePattern.pattern);
			break;
		case 0xF6:	// Line pattern, bytes 6-7
			reinterpret_cast<uint16_t *>(linePattern.pattern)[3] = value;
			setDottedLinePattern(linePattern.pattern);
			break;
		
		// Text cursor, character position, as per TAB within text window
		case 0x118:	{	// X position of text cursor within text window
			uint8_t x, y;
			getCursorTextPosition(&x, &y);
			cursorTab(value, y);
		}	break;
		case 0x119:	{	// Y position of text cursor within text window
			uint8_t x, y;
			getCursorTextPosition(&x, &y);
			cursorTab(x, value);
		}	break;
		// Text cursor position, in screen coordinates
		case 0x11A:		// X position of text cursor in screen coordinates
			textCursor.X = value;
			ensureCursorInViewport(textViewport);
			break;
		case 0x11B:		// Y position of text cursor in screen coordinates
			textCursor.Y = value;
			ensureCursorInViewport(textViewport);
			break;

		case 0x400:	// Current bitmap ID
			setCurrentBitmap(value);
			break;
		case 0x402:	// Current bitmap transform ID
			if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
				bitmapTransform = value;
			}
			break;

		case 0x410:	// Current sprite ID
			setCurrentSprite(value);
			break;

		case 0x440:	// Mouse cursor ID
		case 0x441:	// Mouse cursor enabled
		case 0x442:	// Mouse cursor X position (pixel coords)
		case 0x443:	// Mouse cursor Y position
		case 0x444:	// Mouse cursor button status
		case 0x445:	// Mouse wheel delta
		case 0x446:	// Mouse sample rate
		case 0x447:	// Mouse resolution
		case 0x448:	// Mouse scaling
		case 0x449:	// Mouse acceleration
		case 0x44A:	// Mouse wheel acceleration
			setVDPVariable((var - 0x440) + VDPVAR_MOUSE_CURSOR, value);
			break;
		// Candidate variables for mouse area (0x44C-0x44F) won't be passed through
	}
}

inline VDUProcessorState Context::getProcessorState() {
	return processorState;
}

void Context::setProcessorState(VDUProcessorState state) {
	switch (processorState) {
		case VDUProcessorState::Active:
			processorState = state;
			switch (state) {
				case VDUProcessorState::CtrlShiftPaused:
				case VDUProcessorState::PagedModePaused:
					resetPagedModeCount();
					break;
			}
			break;
		case VDUProcessorState::WaitingForFrames:
		case VDUProcessorState::PagedModePaused:
		case VDUProcessorState::CtrlShiftPaused:
			if (state != processorState) {
				cursorScrollOrWrap();
				processorState = state;
			}
			break;
	}
}

void Context::setWaitForFrames(uint8_t frames) {
	if (frames == 0) {
		// Waiting for frames is disabled
		setProcessorState(VDUProcessorState::Active);
		return;
	}
	waitForFrames = frames;
	setProcessorState(VDUProcessorState::WaitingForFrames);
}

bool Context::checkForVSYNC() {
	if (_VGAController->frameCounter != lastFrameCounter) {
		lastFrameCounter = _VGAController->frameCounter;
		if (processorState == VDUProcessorState::WaitingForFrames) {
			waitForFrames--;
			if (waitForFrames == 0) {
				setProcessorState(VDUProcessorState::Active);
			}
		}
		return true;
	}
	return false;
}

#include "context/cursor.h"
#include "context/fonts.h"
#include "context/graphics.h"
#include "context/viewport.h"

#endif // CONTEXT_H
