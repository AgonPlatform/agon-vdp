#ifndef AGON_SCREEN_H
#define AGON_SCREEN_H

#include <memory>
#include <fabgl.h>

#include "agon.h"								// Agon definitions
#include "agon_palette.h"						// Colour lookup table
#include "agon_ps2.h"

std::unique_ptr<fabgl::Canvas>	canvas;			// The canvas class
std::unique_ptr<fabgl::VGABaseController>	_VGAController;		// Pointer to the current VGA controller class

#include "agon_ttxt.h"

bool			legacyModes = false;			// Default legacy modes being false
uint8_t			_VGAColourDepth = -1;			// Number of colours per pixel (2, 4, 8, 16 or 64)
uint8_t			palette[64];					// Storage for the palette
uint16_t		canvasW;						// Canvas width
uint16_t		canvasH;						// Canvas height
double			logicalScaleX;					// Scaling factor for logical coordinates
double			logicalScaleY;
bool			rectangularPixels = false;		// Pixels are square by default
uint8_t			videoMode;						// Current video mode

extern void debug_log(const char * format, ...);		// Debug log function

void setLegacyModes(bool legacy) {
	legacyModes = legacy;
}

// Get a new VGA controller
// Parameters:
// - colours: Number of colours per pixel (2, 4, 8, 16 or 64)
// Returns:
// - A singleton instance of a VGAController class
//
std::unique_ptr<fabgl::VGABaseController> getVGAController(uint8_t colours) {
	switch (colours) {
		case  2: return std::move(std::unique_ptr<fabgl::VGA2Controller>(new fabgl::VGA2Controller()));
		case  4: return std::move(std::unique_ptr<fabgl::VGA4Controller>(new fabgl::VGA4Controller()));
		case  8: return std::move(std::unique_ptr<fabgl::VGA8Controller>(new fabgl::VGA8Controller()));
		case 16: return std::move(std::unique_ptr<fabgl::VGA16Controller>(new fabgl::VGA16Controller()));
		case 64: return std::move(std::unique_ptr<fabgl::VGA64Controller>(new fabgl::VGA64Controller()));
	}
	return nullptr;
}

// Update the internal FabGL LUT
//
void updateRGB2PaletteLUT() {
	if (_VGAColourDepth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->updateRGB2PaletteLUT();
	}
}

// Create a palette
//
void createPalette(uint16_t paletteId) {
	if (_VGAColourDepth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->createPalette(paletteId);
	}
}

// Delete a palette
//
void deletePalette(uint16_t paletteId) {
	if (_VGAColourDepth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->deletePalette(paletteId);
	}
}

// Set item in palette
//
void setItemInPalette(uint16_t paletteId, uint8_t index, RGB888 colour) {
	if (_VGAColourDepth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->setItemInPalette(paletteId, index, colour);
	}
}

// Update signal list
//
void updateSignalList(uint16_t * signalList, uint16_t count) {
	if (_VGAColourDepth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->updateSignalList(signalList, count);
	}
}

// Get current colour depth
//
inline uint8_t getVGAColourDepth() {
	return _VGAColourDepth;
}

// Set a palette item
// Parameters:
// - l: The logical colour to change
// - c: The new colour
//
void setPaletteItem(uint8_t l, RGB888 c) {
	auto depth = getVGAColourDepth();
	if (l < depth && depth <= 16) {
		fabgl::VGAPalettedController * controller = (fabgl::VGAPalettedController *)(_VGAController.get());
		controller->setPaletteItem(l, c);
	}
}

// Get the palette index for a given RGB888 colour
//
uint8_t getPaletteIndex(RGB888 colour) {
	for (uint8_t i = 0; i < getVGAColourDepth(); i++) {
		if (colourLookup[palette[i]] == colour) {
			return i;
		}
	}
	return 0;
}

// Set logical palette
// Parameters:
// - l: The logical colour to change
// - p: The physical colour to change
// - r: The red component
// - g: The green component
// - b: The blue component
// Returns:
// - Physical colour number or -1 for an Error
//
int8_t setLogicalPalette(uint8_t l, uint8_t p, uint8_t r, uint8_t g, uint8_t b) {
	RGB888 col;				// The colour to set

	if (p == 255) {					// If p = 255, then use the RGB values
		col = RGB888(r, g, b);
	} else if (p < 64) {			// If p < 64, use colour lookup table to convert physical to RGB888
		col = colourLookup[p];
	} else {
		debug_log("vdu_palette: p=%d not supported\n\r", p);
		return -1;
	}

	debug_log("vdu_palette: %d,%d,%d,%d,%d\n\r", l, p, r, g, b);
	uint8_t physicalColor = (col.R >> 6) << 4 | (col.G >> 6) << 2 | (col.B >> 6);
	// update palette entry
	palette[l & (getVGAColourDepth() - 1)] = physicalColor;
	if (getVGAColourDepth() < 64) {		// If it is a paletted video mode
		// change underlying output video palette
		setPaletteItem(l, col);
		updateRGB2PaletteLUT();
	}
	return physicalColor;
}

// Reset the palette and set the foreground and background drawing colours
// Parameters:
// - colour: Array of indexes into colourLookup table
// - sizeOfArray: Size of passed colours array
//
void resetPalette(const uint8_t colours[]) {
	for (uint8_t i = 0; i < 64; i++) {
		uint8_t c = colours[i % getVGAColourDepth()];
		palette[i] = c;
		setPaletteItem(i, colourLookup[c]);
	}
	updateRGB2PaletteLUT();
}

void restorePalette() {
	if (!ttxtMode) {
		switch (getVGAColourDepth()) {
			case  2: resetPalette(defaultPalette02); break;
			case  4: resetPalette(defaultPalette04); break;
			case  8: resetPalette(defaultPalette08); break;
			case 16: resetPalette(defaultPalette10); break;
			case 64: resetPalette(defaultPalette40); break;
		}
	}
}

// Update our VGA controller based on number of colours
// returns true on success, false if the number of colours was invalid
bool updateVGAController(uint8_t colours) {
	if (colours == _VGAColourDepth) {
		return true;
	}

	auto controller = getVGAController(colours);	// Get a new controller
	if (!controller) {
		return false;
	}

	_VGAColourDepth = colours;
	if (_VGAController) {						// If there is an existing controller running then
		_VGAController->end();					// end it
		_VGAController.reset();					// Delete it
	}
	_VGAController = std::move(controller);		// Switch to the new controller
	_VGAController->begin();					// And spin it up

	return true;
}

// Change video resolution
// Parameters:
// - colours: Number of colours per pixel (2, 4, 8, 16 or 64)
// - modeLine: A modeline string (see the FabGL documentation for more details)
// Returns:
// - 0: Successful
// - 1: Invalid # of colours
// - 2: Not enough memory for mode
//
int8_t changeResolution(uint8_t colours, const char * modeLine, bool doubleBuffered = false) {
	if (!updateVGAController(colours)) {			// If we can't update the controller then
		return 1;									// Return the error
	}

	canvas.reset();									// Delete the canvas

	if (modeLine) {									// If modeLine is not a null pointer then
		_VGAController->setResolution(modeLine, -1, -1, doubleBuffered);	// Set the resolution
	} else {
		debug_log("changeResolution: modeLine is null\n\r");
	}

	_VGAController->enableBackgroundPrimitiveExecution(true);
	_VGAController->enableBackgroundPrimitiveTimeout(false);

	canvas.reset(new fabgl::Canvas(_VGAController.get()));		// Create the new canvas
	debug_log("after change of canvas...\n\r");
	debug_log("  free internal: %d\n\r  free 8bit: %d\n\r  free 32bit: %d\n\r",
		heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
		heap_caps_get_free_size(MALLOC_CAP_8BIT),
		heap_caps_get_free_size(MALLOC_CAP_32BIT)
	);

	canvasW = canvas->getWidth();
	canvasH = canvas->getHeight();
	logicalScaleX = LOGICAL_SCRW / (double)canvasW;
	logicalScaleY = LOGICAL_SCRH / (double)canvasH;
	rectangularPixels = ((float)canvasW / (float)canvasH) > 2;

	//
	// Check whether the selected mode has enough memory for the vertical resolution
	//
	if (_VGAController->getScreenHeight() != _VGAController->getViewPortHeight()) {
		return 2;
	}
	// Return with no errors
	return 0;
}

// Do the mode change
// Parameters:
// - mode: The video mode
// Returns:
// -  0: Successful
// -  1: Invalid # of colours
// -  2: Not enough memory for mode
// - -1: Invalid mode
//
int8_t changeMode(uint8_t mode) {
	int8_t errVal = -1;

	switch (mode) {
		case 0:
			if (legacyModes == true) {
				errVal = changeResolution(2, SVGA_1024x768_60Hz);
			} else {
				errVal = changeResolution(16, VGA_640x480_60Hz);	// VDP 1.03 Mode 3, VGA Mode 12h
			}
			break;
		case 1:
			if (legacyModes == true) {
				errVal = changeResolution(16, VGA_512x384_60Hz);
			} else {
				errVal = changeResolution(4, VGA_640x480_60Hz);
			}
			break;
		case 2:
			if (legacyModes == true) {
				errVal = changeResolution(64, VGA_320x200_75Hz);
			} else {
				errVal = changeResolution(2, VGA_640x480_60Hz);
			}
			break;
		case 3:
			if (legacyModes == true) {
				errVal = changeResolution(16, VGA_640x480_60Hz);
			} else {
				errVal = changeResolution(64, VGA_640x240_60Hz);
			}
			break;
		case 4:
			errVal = changeResolution(16, VGA_640x240_60Hz);
			break;
		case 5:
			errVal = changeResolution(4, VGA_640x240_60Hz);
			break;
		case 6:
			errVal = changeResolution(2, VGA_640x240_60Hz);
			break;
		case 7:
			errVal = changeResolution(16, VGA_640x480_60Hz);
			if (errVal == 0) {
				errVal = ttxt_instance.init();
				if (errVal == 0) {
					ttxtMode = true;
				} else {
					debug_log("changeMode: ttxt_instance.init() failed %d\n\r", errVal);
				}
			}
			break;
		case 8:
			errVal = changeResolution(64, QVGA_320x240_60Hz);		// VGA "Mode X"
			break;
		case 9:
			errVal = changeResolution(16, QVGA_320x240_60Hz);
			break;
		case 10:
			errVal = changeResolution(4, QVGA_320x240_60Hz);
			break;
		case 11:
			errVal = changeResolution(2, QVGA_320x240_60Hz);
			break;
		case 12:
			errVal = changeResolution(64, VGA_320x200_70Hz);		// VGA Mode 13h
			break;
		case 13:
			errVal = changeResolution(16, VGA_320x200_70Hz);
			break;
		case 14:
			errVal = changeResolution(4, VGA_320x200_70Hz);
			break;
		case 15:
			errVal = changeResolution(2, VGA_320x200_70Hz);
			break;
		case 16:
			errVal = changeResolution(4, SVGA_800x600_60Hz);
			break;
		case 17:
			errVal = changeResolution(2, SVGA_800x600_60Hz);
			break;
		case 18:
			errVal = changeResolution(2, SVGA_1024x768_60Hz);		// VDP 1.03 Mode 0
			break;
		case 19:
			errVal = changeResolution(4, SVGA_1024x768_60Hz);
			break;
		case 20:
			errVal = changeResolution(64, VGA_512x384_60Hz);
			break;
		case 21:
			errVal = changeResolution(16, VGA_512x384_60Hz);
			break;
		case 22:
			errVal = changeResolution(4, VGA_512x384_60Hz);
			break;
		case 23:
			errVal = changeResolution(2, VGA_512x384_60Hz);
			break;
		case 24:
			errVal = changeResolution(16, QSVGA_640x512_60Hz);
			break;
		case 25:
			errVal = changeResolution(4, QSVGA_640x512_60Hz);
			break;
		case 26:
			errVal = changeResolution(2, QSVGA_640x512_60Hz);
			break;
		case 27:
			errVal = changeResolution(64, QSVGA_640x256_60Hz);
			break;
		case 28:
			errVal = changeResolution(16, QSVGA_640x256_60Hz);
			break;
		case 29:
			errVal = changeResolution(4, QSVGA_640x256_60Hz);
			break;
		case 30:
			errVal = changeResolution(2, QSVGA_640x256_60Hz);
			break;
		case 129:
			errVal = changeResolution(4, VGA_640x480_60Hz, true);
			break;
		case 130:
			errVal = changeResolution(2, VGA_640x480_60Hz, true);
			break;
		case 132:
			errVal = changeResolution(16, VGA_640x240_60Hz, true);
			break;
		case 133:
			errVal = changeResolution(4, VGA_640x240_60Hz, true);
			break;
		case 134:
			errVal = changeResolution(2, VGA_640x240_60Hz, true);
			break;
		case 136:
			errVal = changeResolution(64, QVGA_320x240_60Hz, true);		// VGA "Mode X"
			break;
		case 137:
			errVal = changeResolution(16, QVGA_320x240_60Hz, true);
			break;
		case 138:
			errVal = changeResolution(4, QVGA_320x240_60Hz, true);
			break;
		case 139:
			errVal = changeResolution(2, QVGA_320x240_60Hz, true);
			break;
		case 140:
			errVal = changeResolution(64, VGA_320x200_70Hz, true);		// VGA Mode 13h
			break;
		case 141:
			errVal = changeResolution(16, VGA_320x200_70Hz, true);
			break;
		case 142:
			errVal = changeResolution(4, VGA_320x200_70Hz, true);
			break;
		case 143:
			errVal = changeResolution(2, VGA_320x200_70Hz, true);
			break;
		case 145:
			errVal = changeResolution(2, SVGA_800x600_60Hz, true);
			break;
		case 146:
			errVal = changeResolution(2, SVGA_1024x768_60Hz, true);
			break;
		case 149:
			errVal = changeResolution(16, VGA_512x384_60Hz, true);
			break;
		case 150:
			errVal = changeResolution(4, VGA_512x384_60Hz, true);
			break;
		case 151:
			errVal = changeResolution(2, VGA_512x384_60Hz, true);
			break;
		case 153:
			errVal = changeResolution(4, QSVGA_640x512_60Hz, true);
			break;
		case 154:
			errVal = changeResolution(2, QSVGA_640x512_60Hz, true);
			break;
		case 156:
			errVal = changeResolution(16, QSVGA_640x256_60Hz, true);
			break;
		case 157:
			errVal = changeResolution(4, QSVGA_640x256_60Hz, true);
			break;
		case 158:
			errVal = changeResolution(2, QSVGA_640x256_60Hz, true);
			break;


	}

	debug_log("changeMode: canvas(%d,%d), scale(%f,%f), mode %d, videoMode %d\n\r", canvasW, canvasH, logicalScaleX, logicalScaleY, mode, videoMode);
	if (errVal == 0) {
		videoMode = mode;
	}
	if (errVal != -1) {
		restorePalette();
	}
	return errVal;
}

// Ask our screen controller if we're double buffered
//
inline bool isDoubleBuffered() {
	return _VGAController->isDoubleBuffered();
}

// Wait for plot completion
//
inline void waitPlotCompletion(bool waitForVSync = false) {
	canvas->waitCompletion(waitForVSync);
}

// Swap to other buffer if we're in a double-buffered mode
// Always waits for VSYNC
//
void switchBuffer() {
	if (isDoubleBuffered()) {
		canvas->swapBuffers();
	} else {
		canvas->noOp();
		waitPlotCompletion(true);
	}
}

inline void setMouseCursorPos(uint16_t x, uint16_t y) {
	_VGAController->setMouseCursorPos(x, y);
}

#endif // AGON_SCREEN_H
