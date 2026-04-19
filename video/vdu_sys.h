#ifndef VDU_SYS_H
#define VDU_SYS_H

#include <algorithm>
#include <vector>

#include <fabgl.h>

#include "agon.h"
#include "agon_ps2.h"
#include "agon_screen.h"
#include "vdp_variables.h"
#include "vdu_audio.h"
#include "vdu_buffered.h"
#include "vdu_context.h"
#include "vdu_fonts.h"
#include "vdu_sprites.h"
#include "updater.h"
#include "vdu_stream_processor.h"
#include "vdu_layers.h"
#include "ymodem.h"

extern void startTerminal();					// Start the terminal
extern void setConsoleMode(bool mode);			// Set console mode
extern bool controlKeys;	

bool			initialised = false;			// Is the system initialised yet?

// Buffer for serialised time
//
typedef union {
	struct {
		uint32_t month : 4;
		uint32_t day : 5;
		uint32_t dayOfWeek : 3;
		uint32_t dayOfYear : 9;
		uint32_t hour : 5;
		uint32_t minute : 6;
		uint8_t  second;
		uint8_t  year;
	};
	uint8_t packet[6];
} vdp_time_t;

// Wait for eZ80 to initialise
//
void VDUStreamProcessor::wait_eZ80() {
	if (esp_reset_reason() == ESP_RST_SW) {
		// We only perform a s/w reset after flashing, so MOS will already be running
		initialised = true;
	} else {
		debug_log("wait_eZ80: Start\n\r");
		while (!initialised) {
			if (byteAvailable()) {
				auto c = readByte();	// Only handle VDU 23 packets
				if (c == 23) {
					vdu_sys();
				}
			}
		}
		debug_log("wait_eZ80: End\n\r");	
	}

	sendModeInformation();
}

// Handle SYS
// VDU 23,mode
//
void VDUStreamProcessor::vdu_sys() {
	auto mode = readByte_t();

	//
	// If mode is -1, then there's been a timeout
	//
	if (mode == -1) {
		return;
	}
	//
	// If mode < 32, then it's a system command
	//
	else if (mode < 32) {
		switch (mode) {
			case 0x00: {					// VDU 23, 0
				vdu_sys_video();			// Video system control
			}	break;
			case 0x01: {					// VDU 23, 1
				auto b = readByte_t();		// Cursor control
				if (b >= 0) {
					context->enableCursor(b);
				}
			}	break;
			case 0x06: {					// VDU 23, 6
				uint8_t pattern[8];			// Set dotted line pattern
				auto remaining = readIntoBuffer(pattern, 8);
				if (remaining == 0) {
					context->setDottedLinePattern(pattern);
				}
			}	break;
			case 0x07: {					// VDU 23, 7
				vdu_sys_scroll();			// Scroll
			}	break;
			case 0x10: {					// VDU 23, 16
				vdu_sys_cursorBehaviour();	// Set cursor behaviour
			}	break;
			case 0x17: {					// VDU 23, 23, n
				auto b = readByte_t();		// Set line thickness
				if (b >= 0) {
					context->setLineThickness(b);
				}
			}	break;
			case 0x1B: {					// VDU 23, 27
				clearEcho();				// Don't echo bitmap/sprite commands
				vdu_sys_sprites();			// Sprite system control
			}	break;
			case 0x1C: {
				clearEcho();                        // Don't echo hexload/ymodem commands
				int16_t hlcmd = readByte_t();
				switch(hlcmd) {
					case -1:
						vdu_sys_hexload();          // VDU 23,28
						break;
					case 1:
						vdu_sys_ymodem_receive();   // VDU 23,28,1
						break;
					case 2:
						vdu_sys_ymodem_send();      // VDU 23,28,2
						break;
				}

			}   break;
		}
	}
	//
	// Otherwise, do
	// VDU 23,mode,n1,n2,n3,n4,n5,n6,n7,n8
	// Redefine character with ASCII code mode
	//
	else {
		waitPlotCompletion();
		vdu_sys_udg(mode);
	}
}

// VDU 23,0: VDP control
// These can send responses back; the response contains a packet # that matches the VDU command mode byte
//
void VDUStreamProcessor::vdu_sys_video() {
	auto mode = readByte_t();

	// TODO - consider whether we want to clear echo for _all_ VDU 23 commands
	clearEcho();

	switch (mode) {
		case VDP_CURSOR_VSTART: {		// VDU 23, 0, &0A, offset
			auto offset = readByte_t();	// Set the vertical start of the cursor
			if (offset >= 0) {
				context->setCursorVStart(offset & 0x1F);
				context->setCursorAppearance((offset & 0x60) >> 5);
			}
		}	break;
		case VDP_CURSOR_VEND: {			// VDU 23, 0, &0B, offset
			auto offset = readByte_t();	// Set the vertical end of the cursor
			if (offset >= 0) {
				context->setCursorVEnd(offset);
			}
		}	break;
		case VDP_GP: {					// VDU 23, 0, &80
			sendGeneralPoll();			// Send a general poll packet
		}	break;
		case VDP_KEYCODE: {				// VDU 23, 0, &81, layout
			vdu_sys_video_kblayout();
		}	break;
		case VDP_CURSOR: {				// VDU 23, 0, &82
			sendCursorPosition();		// Send cursor position
		}	break;
		case VDP_SCRCHAR: {				// VDU 23, 0, &83, x; y;
			auto x = readWord_t();		// Get character at screen position x, y
			if (x == -1) return;
			auto y = readWord_t();
			if (y == -1) return;
			auto c = context->getScreenChar(x, y);
			sendScreenChar(c);
		}	break;
		case VDP_SCRPIXEL: {			// VDU 23, 0, &84, x; y;
			auto x = readWord_t();		// Get pixel value at screen position x, y
			if (x == -1) return;
			auto y = readWord_t();
			if (y == -1) return;
			sendScreenPixel((short)x, (short)y);
		}	break;
		case VDP_AUDIO: {				// VDU 23, 0, &85, channel, command, <args>
			vdu_sys_audio();
		}	break;
		case VDP_MODE: {				// VDU 23, 0, &86
			sendModeInformation();		// Send mode information (screen dimensions, etc)
		}	break;
		case VDP_RTC: {					// VDU 23, 0, &87, mode
			vdu_sys_video_time();		// Send time information
		}	break;
		case VDP_KEYSTATE: {			// VDU 23, 0, &88, repeatRate; repeatDelay; status
			vdu_sys_keystate();
		}	break;
		case VDP_MOUSE: {				// VDU 23, 0, &89, command, <args>
			vdu_sys_mouse();
		}	break;
		case VDP_CURSOR_HSTART: {		// VDU 23, 0, &8A, offset
			auto offset = readByte_t();	// Set the horizontal start of the cursor
			if (offset >= 0)
				context->setCursorHStart(offset);
		}	break;
		case VDP_CURSOR_HEND: {			// VDU 23, 0, &8B, offset
			auto offset = readByte_t();	// Set the vertical end of the cursor
			if (offset >= 0)
				context->setCursorHEnd(offset);
		}	break;
		case VDP_CURSOR_MOVE: {			// VDU 23, 0, &8C, x, y
			auto x = readByte_t();		// Relative move of current active cursor by x, y pixels
			if (x == -1) return;
			auto y = readByte_t();
			if (y == -1) return;
			context->cursorRelativeMove((int8_t) x, (int8_t) y);
		}	break;
		case VDP_UDG: {					// VDU 23, 0, &90, c, <args>
			auto c = readByte_t();		// Redefine a display character (system font only)
			if (c >= 0) {
				waitPlotCompletion();
				vdu_sys_udg(c);
			}
		}	break;
		case VDP_UDG_RESET: {			// VDU 23, 0, &91
			waitPlotCompletion();
			// TODO should this reset to system font?
			copy_font();				// Reset UDGs (system font only)
		}	break;
		case VDP_MAP_CHAR_TO_BITMAP: {	// VDU 23, 0, &92, c, bitmapId;
			auto c = readByte_t();		// Map a character to a bitmap
			auto bitmapId = readWord_t();
			if (c >= 0 && bitmapId >= 0) {
				context->mapCharToBitmap(c, bitmapId);
			}
		}	break;
		case VDP_SCRCHAR_GRAPHICS: {	// VDU 23, 0, &93, x; y;
			auto x = readWord_t();		// Get character at graphics position x, y
			if (x == -1) return;
			auto y = readWord_t();
			if (y == -1) return;
			char c = context->getScreenCharAt(x, y);
			sendScreenChar(c);
		}	break;
		case VDP_READ_COLOUR: {			// VDU 23, 0, &94, index
			auto index = readByte_t();	// Read colour from palette
			if (index >= 0) {
				sendColour(index);
			}
		}	break;
		case VDP_FONT: {				// VDU 23, 0, &95, command, [bufferId;] [<args>]
			vdu_sys_font();				// Font management
		}	break;
		case VDP_AFFINE_TRANSFORM: {	// VDU 23, 0, &96, flags, bufferId;
			if (!isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
				return;
			}
			auto flags = readByte_t();	// Set affine transform flags
			if (flags == -1) return;
			auto bufferId = readWord_t();
			if (bufferId >= 0) {
				debug_log("vdu_sys_video: affine transform, flags %d, buffer %d\n\r", flags, bufferId);
				context->setAffineTransform(flags, bufferId);
			}
		}	break;
		case VDP_CONTROLKEYS: {			// VDU 23, 0, &98, n
			auto b = readByte_t();		// Set control keys,  0 = off, 1 = on (default)
			if (b >= 0) {
				controlKeys = (bool) b;
			}
		}	break;
		case VDP_CHECKKEY: {
			auto key = readByte_t();	// VDU 23, 0, &99, virtualkey
			if (key == -1) return;
			// Inject an updated virtual key event for a key, forcing a new keycode packet to be sent
			// NB must use a virtual key here, as we can't convert a keycode to a virtual key
			auto keyboard = getKeyboard();
			keyboard->injectVirtualKey((VirtualKey) key, keyboard->isVKDown((VirtualKey) key), false);
		}	break;
		case VDP_TEMP_PAGED_MODE: {		// VDU 23, 0, &9A
			// Set temporary paged mode
			context->setTempPagedMode();
		}	break;
		case VDP_BUFFER_PRINT: {		// VDU 23, 0, &9B
			auto bufferId = readWord_t();
			if (bufferId == -1) return;
			printBuffer(bufferId);
		}	break;
		case VDP_TEXT_VIEWPORT: {		// VDU 23, 0, &9C
			// Set text viewport using graphics coordinates
			if (ttxtMode) {
				// We could consider supporting this by dividing points by font size
				debug_log("vdp_textViewport: Not supported in teletext mode\n\r");
				return;
			}
			if (context->setTextViewport()) {
				debug_log("vdp_textViewport: OK\n\r");
			} else {
				debug_log("vdp_textViewport: Invalid Viewport\n\r");
			}
			sendModeInformation();
		}	break;
		case VDP_GRAPHICS_VIEWPORT: {	// VDU 23, 0, &9D
			// Set graphics viewport using latest graphics coordinates
			if (context->setGraphicsViewport()) {
				debug_log("vdp_graphicsViewport: OK\n\r");
			} else {
				debug_log("vdp_graphicsViewport: Invalid Viewport\n\r");
			}
		}	break;
		case VDP_GRAPHICS_ORIGIN: {		// VDU 23, 0, &9E
			// Set graphics origin using latest graphics coordintes
			context->setOrigin();
		}	break;
		case VDP_SHIFT_ORIGIN: {		// VDU 23, 0, &9F
			// Shift graphics origin and viewports using latest graphics coordintes
			context->shiftOrigin();
		}	break;
		case VDP_BUFFERED: {			// VDU 23, 0, &A0, bufferId; command, <args>
			vdu_sys_buffered();
		}	break;
		case VDP_UPDATER: {				// VDU 23, 0, &A1, command, <args>
			vdu_sys_updater();
		}	break;
		case VDP_LOGICALCOORDS: {		// VDU 23, 0, &C0, n
			auto b = readByte_t();		// Set logical coord mode
			if (b >= 0) {
				context->setLogicalCoords((bool) b);	// (0 = off, 1 = on)
			}
		}	break;
		case VDP_LEGACYMODES: {			// VDU 23, 0, &C1, n
			auto b = readByte_t();		// Switch legacy modes on or off
			if (b >= 0) {
				setLegacyModes((bool) b);
			}
		}	break;
		case VDP_LAYERS: {				// VDU 23, 0, &C2, n
			if (!isVDPVariableSet(TESTFLAG_TILE_ENGINE)) {
				return;
			}
			vdu_sys_layers();
		}	break;
		case VDP_SWITCHBUFFER: {		// VDU 23, 0, &C3
			switchBuffer();
		}	break;
		case VDP_COPPER: {				// VDU 23, 0, &C4, command, [<args>]
			if (!isVDPVariableSet(VDPVAR_COPPER)) {
				return;
			}
			vdu_sys_copper();
		}	break;
		case VDP_CONTEXT: {				// VDU 23, 0, &C8, command, [<args>]
			vdu_sys_context();			// Context management
		}	break;
		case VDP_FLUSH_DRAWING_QUEUE: {	// VDU 23, 0, &CA
			waitPlotCompletion();
		}	break;
		case VDP_PATTERN_LENGTH: {		// VDU 23, 0, &F2, n
			auto b = readByte_t();		// Set pattern length
			if (b >= 0) {
				context->setDottedLinePatternLength(b);
			}
		}	break;
		case VDP_VDPVAR_SET: {		// VDU 23, 0, &F8, flag; value;
			auto flag = readWord_t();	// Set a test/feature flag
			auto value = readWord_t();
			setVDPVariable(flag, value);
		}	break;
		case VDP_VDPVAR_CLEAR: {	// VDU 23, 0, &F9, flag
			auto flag = readWord_t();	// Clear a test/feature flag
			clearVDPVariable(flag);
		}	break;
		case VDP_CONSOLEMODE: {			// VDU 23, 0, &FE, n
			auto b = readByte_t();
			setConsoleMode((bool) b);
		}	break;
		case VDP_TERMINALMODE: {		// VDU 23, 0, &FF
			startTerminal();		 	// Switch to, or resume, terminal mode
		}	break;
	}
}

// VDU 23, 0, &80, <echo>: Send a general poll/echo byte back to MOS
//
void VDUStreamProcessor::sendGeneralPoll() {
	auto b = readByte_t();
	if (b == -1) {
		debug_log("sendGeneralPoll: Timeout\n\r");
		return;
	}
	setVDPVariable(VDPVAR_GENERALPOLL_BYTE, b);
	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_GP);
	uint8_t packet[] = {
		(uint8_t) (getVDPVariable(VDPVAR_GENERALPOLL_BYTE) & 0xFF),
	};
	send_packet(PACKET_GP, sizeof packet, packet);
	initialised = true;
}

// VDU 23, 0, &81, <region>: Set the keyboard layout
//
void VDUStreamProcessor::vdu_sys_video_kblayout() {
	auto region = readByte_t();			// Fetch the region
	setKeyboardLayout(region);
}

// VDU 23, 0, &82: Send the cursor position back to MOS
//
void VDUStreamProcessor::sendCursorPosition() {
	// cursor position varies depending on behaviour
	// so if x/y are inverted, we need to invert them
	// and if x/y are swapped, we need to swap them
	uint8_t x, y;

	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_CURSOR);
	// NB whilst we're not fetching variables for x and y, changing the cursor position
	// via VDP variables in a callback would still affect the output of this function
	context->getCursorTextPosition(&x, &y);
	
	uint8_t packet[] = { x, y };
	send_packet(PACKET_CURSOR, sizeof packet, packet);
}

// VDU 23, 0, &83 / &93 Send a character back to MOS
//
void VDUStreamProcessor::sendScreenChar(char c) {
	setVDPVariable(VDPVAR_LAST_CHARACTER_READ, c);
	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_SCRCHAR);
	uint8_t packet[] = {
		uint8_t (getVDPVariable(VDPVAR_LAST_CHARACTER_READ)),
	};
	send_packet(PACKET_SCRCHAR, sizeof packet, packet);
}

// VDU 23, 0, &84: Send a pixel value back to MOS
//
void VDUStreamProcessor::sendScreenPixel(uint16_t x, uint16_t y) {
	waitPlotCompletion();
	RGB888 pixel = context->getPixel(x, y);
	uint8_t pixelIndex = getPaletteIndex(pixel);
	setVDPVariable(VDPVAR_LAST_COLOUR_RED, pixel.R);
	setVDPVariable(VDPVAR_LAST_COLOUR_GREEN, pixel.G);
	setVDPVariable(VDPVAR_LAST_COLOUR_BLUE, pixel.B);
	setVDPVariable(VDPVAR_LAST_COLOUR_LOGICAL, pixelIndex);
	RGB222 physical = RGB222(pixel);
	setVDPVariable(VDPVAR_LAST_COLOUR_PHYSICAL, physical.R << 4 | physical.G << 2 | physical.B);
	setVDPVariable(VDPVAR_LAST_COLOUR_X, x);
	setVDPVariable(VDPVAR_LAST_COLOUR_Y, y);
	bufferCallCallbacks(CALLBACK_READPIXEL);
	sendScrPixelPacket();
}

// VDU 23, 0, &94, index: Send a colour back to MOS
//
void VDUStreamProcessor::sendColour(uint8_t colour) {
	RGB888 pixel;
	if (colour < 64) {
		// Colour is a palette lookup
		uint8_t c = palette[colour % getVGAColourDepth()];
		pixel = colourLookup[c];
	} else {
		// Colour may be an active colour lookup
		if (!context->getColour(colour, &pixel)) {
			// Unrecognised colour - no response
			return;
		}
		colour = getPaletteIndex(pixel);
	}

	setVDPVariable(VDPVAR_LAST_COLOUR_RED, pixel.R);
	setVDPVariable(VDPVAR_LAST_COLOUR_GREEN, pixel.G);
	setVDPVariable(VDPVAR_LAST_COLOUR_BLUE, pixel.B);
	setVDPVariable(VDPVAR_LAST_COLOUR_LOGICAL, colour);
	RGB222 physical = RGB222(pixel);
	setVDPVariable(VDPVAR_LAST_COLOUR_PHYSICAL, physical.R << 4 | physical.G << 2 | physical.B);
	setVDPVariable(VDPVAR_LAST_COLOUR_X, 32768);
	setVDPVariable(VDPVAR_LAST_COLOUR_Y, 32768);
	sendScrPixelPacket();
}

void VDUStreamProcessor::sendScrPixelPacket() {
	// Send the pixel packet
	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_SCRPIXEL);
	uint8_t packet[] = {
		(uint8_t) getVDPVariable(VDPVAR_LAST_COLOUR_RED),
		(uint8_t) getVDPVariable(VDPVAR_LAST_COLOUR_GREEN),
		(uint8_t) getVDPVariable(VDPVAR_LAST_COLOUR_BLUE),
		(uint8_t) getVDPVariable(VDPVAR_LAST_COLOUR_LOGICAL),
	};
	send_packet(PACKET_SCRPIXEL, sizeof packet, packet);
}

void VDUStreamProcessor::printBuffer(uint16_t bufferId) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("vdp_bufferPrint: buffer %d not found\n\r", bufferId);
		return;
	}

	auto buffer = bufferIter->second;
	for (const auto &block : bufferIter->second) {
		// grab strings from the buffer
		context->plotString(std::string((const char*)block->getBuffer(), block->size()));
	}
}

// VDU 23, 0, &87, 0: Send TIME information (from ESP32 RTC)
//
void VDUStreamProcessor::sendTime() {
	vdp_time_t	time;

	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_RTC);

	time.year = rtc.getYear() - EPOCH_YEAR;	// 0 - 255
	time.month = rtc.getMonth();			// 0 - 11
	time.day = rtc.getDay();				// 1 - 31
	time.dayOfYear = rtc.getDayofYear();	// 0 - 365
	time.dayOfWeek = rtc.getDayofWeek();	// 0 - 6
	time.hour = rtc.getHour(true);			// 0 - 23
	time.minute = rtc.getMinute();			// 0 - 59
	time.second = rtc.getSecond();			// 0 - 59

	send_packet(PACKET_RTC, sizeof time.packet, time.packet);
}

// VDU 23, 0, &86: Send MODE information (screen details)
//
void VDUStreamProcessor::sendModeInformation() {
	// our character dimensions are for the currently active viewport
	// needed as MOS's line editing system uses these
	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_MODE);
	uint8_t packet[] = {
		(uint8_t) (canvasW & 0xFF),			// Width in pixels (L)
		(uint8_t) ((canvasW >> 8) & 0xFF),	// Width in pixels (H)
		(uint8_t) (canvasH & 0xFF),			// Height in pixels (L)
		(uint8_t) ((canvasH >> 8) & 0xFF),	// Height in pixels (H)
		(uint8_t) context->getNormalisedViewportCharWidth(),		// Width in characters (byte)
		(uint8_t) context->getNormalisedViewportCharHeight(),		// Height in characters (byte)
		getVGAColourDepth(),				// Colour depth
		videoMode,							// The video mode number
	};
	send_packet(PACKET_MODE, sizeof packet, packet);
}

// VDU 23, 0, &87, <mode>, [args]: Handle time requests
//
void VDUStreamProcessor::vdu_sys_video_time() {
	auto mode = readByte_t();

	if (mode == 0) {
		sendTime();
	}
	else if (mode == 1) {
		auto yr = readByte_t(); if (yr == -1) return;
		auto mo = readByte_t(); if (mo == -1) return;
		auto da = readByte_t(); if (da == -1) return;
		auto ho = readByte_t(); if (ho == -1) return;
		auto mi = readByte_t(); if (mi == -1) return;
		auto se = readByte_t(); if (se == -1) return;

		yr = EPOCH_YEAR + (int8_t)yr;

		if (yr >= 1970) {
			rtc.setTime(se, mi, ho, da, mo, yr);
		}
	}
}

// Send the keyboard state
//
void VDUStreamProcessor::sendKeyboardState() {
	uint16_t	delay;
	uint16_t	rate;
	uint8_t		ledState;
	bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_KEYSTATE);
	getKeyboardState(&delay, &rate, &ledState);
	uint8_t		packet[] = {
		(uint8_t) (delay & 0xFF),
		(uint8_t) ((delay >> 8) & 0xFF),
		(uint8_t) (rate & 0xFF),
		(uint8_t) ((rate >> 8) & 0xFF),
		ledState
	};
	send_packet(PACKET_KEYSTATE, sizeof packet, packet);
}

// VDU 23, 0, &88, delay; repeatRate; LEDs: Handle the keystate
// Send 255 for LEDs to leave them unchanged
//
void VDUStreamProcessor::vdu_sys_keystate() {
	auto delay = readWord_t(); if (delay == -1) return;
	auto rate = readWord_t(); if (rate == -1) return;
	auto ledState = readByte_t(); if (ledState == -1) return;

	setKeyboardState(delay, rate, ledState);
	debug_log("vdu_sys_video: keystate: delay=%d, rate=%d, led=%d\n\r", kbRepeatDelay, kbRepeatRate, ledState);
	sendKeyboardState();
}

// VDU 23, 0, &89, command, [<args>]: Handle mouse requests
//
void VDUStreamProcessor::vdu_sys_mouse() {
	auto command = readByte_t(); if (command == -1) return;

	switch (command) {
		case MOUSE_ENABLE: {
			if (enableMouse()) {
				debug_log("vdu_sys_mouse: mouse enabled\n\r");
			} else {
				debug_log("vdu_sys_mouse: mouse enable failed\n\r");
			}
			// send mouse data (with no delta) to indicate command processed successfully
			setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
			setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
		}	break;

		case MOUSE_DISABLE: {
			if (disableMouse()) {
				debug_log("vdu_sys_mouse: mouse disabled\n\r");
			} else {
				debug_log("vdu_sys_mouse: mouse disable failed\n\r");
			}
			setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
			setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
		}	break;

		case MOUSE_RESET: {
			debug_log("vdu_sys_mouse: reset mouse\n\r");
			if (resetMouse()) {
				// mouse successfully reset, so make sure the mouse cursor is visible
				showMouseCursor();
			}
			setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
			setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
		}	break;

		case MOUSE_SET_CURSOR: {
			auto cursor = readWord_t();	if (cursor == -1) return;
			if (mouseEnabled) {
				setMouseCursor(cursor);
			}
			setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
			setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			debug_log("vdu_sys_mouse: set cursor\n\r");
		}	break;

		case MOUSE_SET_POSITION: {
			auto x = readWord_t();	if (x == -1) return;
			auto y = readWord_t();	if (y == -1) return;
			setVDPVariable(VDPVAR_MOUSE_XPOS_OS, x);
			setVDPVariable(VDPVAR_MOUSE_YPOS_OS, y);
			setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
			setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			debug_log("vdu_sys_mouse: set position\n\r");
		}	break;

		case MOUSE_SET_AREA: {
			auto x = readWord_t();	if (x == -1) return;
			auto y = readWord_t();	if (y == -1) return;
			auto x2 = readWord_t();	if (x2 == -1) return;
			auto y2 = readWord_t();	if (y2 == -1) return;
			debug_log("vdu_sys_mouse: set area can't be properly supported with current fab-gl\n\r");
			// TODO set area to width/height using bottom/right only
		}	break;

		case MOUSE_SET_SAMPLERATE: {
			auto rate = readByte_t();	if (rate == -1) return;
			if (setMouseSampleRate(rate)) {
				debug_log("vdu_sys_mouse: set sample rate %d\n\r", rate);
				// Clear deltas, which will trigger a mouse packet send
				setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
				setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			} else {
				debug_log("vdu_sys_mouse: set sample rate %d failed\n\r", rate);
			}
		}	break;

		case MOUSE_SET_RESOLUTION: {
			auto resolution = readByte_t();	if (resolution == -1) return;
			if (setMouseResolution(resolution)) {
				debug_log("vdu_sys_mouse: set resolution %d\n\r", resolution);
				// Clear deltas, which will trigger a mouse packet send
				setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
				setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			} else {
				debug_log("vdu_sys_mouse: set resolution %d failed\n\r", resolution);
			}
		}	break;

		case MOUSE_SET_SCALING: {
			auto scaling = readByte_t();	if (scaling == -1) return;
			if (setMouseScaling(scaling)) {
				debug_log("vdu_sys_mouse: set scaling %d\n\r", scaling);
				// Clear deltas, which will trigger a mouse packet send
				setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
				setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			}
		}	break;

		case MOUSE_SET_ACCERATION: {
			auto acceleration = readWord_t();	if (acceleration == -1) return;
			if (setMouseAcceleration(acceleration)) {
				debug_log("vdu_sys_mouse: set acceleration %d\n\r", acceleration);
				// Clear deltas, which will trigger a mouse packet send
				setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
				setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			}
		}	break;

		case MOUSE_SET_WHEELACC: {
			auto wheelAcc = read24_t();	if (wheelAcc == -1) return;
			if (setMouseWheelAcceleration(wheelAcc)) {
				debug_log("vdu_sys_mouse: set wheel acceleration %d\n\r", wheelAcc);
				// Clear deltas, which will trigger a mouse packet send
				setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
				setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
			}
		}	break;
	}
}

// VDU 23, 0, &C4, command, [<args>]: Handle copper requests
void VDUStreamProcessor::vdu_sys_copper() {
	auto command = readByte_t(); if (command == -1) return;

	switch (command) {
		case COPPER_CREATE_PALETTE: {
			auto paletteId = readWord_t(); if (paletteId == -1) return;

			createPalette(paletteId);
		}	break;
		case COPPER_DELETE_PALLETE: {
			auto paletteId = readWord_t(); if (paletteId == -1) return;

			deletePalette(paletteId);
		}	break;
		case COPPER_SET_PALETTE_COLOUR: {
			auto paletteId = readWord_t(); if (paletteId == -1) return;
			auto index = readByte_t(); if (index == -1) return;
			auto r = readByte_t(); if (r == -1) return;
			auto g = readByte_t(); if (g == -1) return;
			auto b = readByte_t(); if (b == -1) return;

			setItemInPalette(paletteId, index, RGB888(r, g, b));
		}	break;
		case COPPER_UPDATE_SIGNALLIST: {
			auto bufferId = readWord_t(); if (bufferId == -1) return;

			auto bufferIter = buffers.find(bufferId);
			if (bufferIter == buffers.end()) {
				debug_log("vdu_sys_copper: buffer %d not found\n\r", bufferId);
				return;
			}

			// only use first block in buffer
			auto buffer = bufferIter->second[0];
			updateSignalList((uint16_t *)buffer->getBuffer(), buffer->size() / 4);
		}	break;
		case COPPER_RESET_SIGNALLIST: {
			uint16_t signalList[2] = { 0, 0 };
			updateSignalList(signalList, 1);
		}	break;
	}
}

// VDU 23,7: Scroll rectangle on screen
//
void VDUStreamProcessor::vdu_sys_scroll() {
	auto extent = readByte_t();		if (extent == -1) return;	// Extent (0 = text viewport, 1 = entire screen, 2 = graphics viewport)
	auto direction = readByte_t();	if (direction == -1) return;	// Direction
	auto movement = readByte_t();	if (movement == -1) return;	// Number of pixels to scroll

	context->scrollRegion(static_cast<ViewportType>(extent), direction, movement);
}

// VDU 23,16: Set cursor behaviour
//
void VDUStreamProcessor::vdu_sys_cursorBehaviour() {
	auto setting = readByte_t();	if (setting == -1) return;
	auto mask = readByte_t();		if (mask == -1) return;

	context->setCursorBehaviour((uint8_t) setting, (uint8_t) mask);
	sendModeInformation();
}

// VDU 23, c, n1, n2, n3, n4, n5, n6, n7, n8: Redefine a display character
// Parameters:
// - c: The character to redefine
//
void VDUStreamProcessor::vdu_sys_udg(char c) {
	uint8_t		buffer[8];

	auto read = readIntoBuffer(buffer, 8);
	if (read == 0) {
		if (context->usingSystemFont()) {
			redefineCharacter(c, buffer);
		} else {
			debug_log("vdu_sys_udg: system font not active, ignoring\n\r");
		}
	}
}

#endif // VDU_SYS_H
