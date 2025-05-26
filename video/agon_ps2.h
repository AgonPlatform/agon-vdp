#ifndef AGON_PS2_H
#define AGON_PS2_H

#include <vector>
#include <algorithm>

#include <fabgl.h>

#include "agon.h"
#include "agon_screen.h"

uint8_t			_keycode = 0;					// Last pressed key code
uint16_t		kbRepeatDelay = 500;			// Keyboard repeat delay ms (250, 500, 750 or 1000)		
uint16_t		kbRepeatRate = 100;				// Keyboard repeat rate ms (between 33 and 500)
uint8_t			kbRegion = 0;					// Keyboard region
bool			kbEnabled = false;				// Keyboard enabled

bool			mouseEnabled = false;			// Mouse enabled
bool			mouseVisible = false;			// Mouse cursor visible
uint8_t			mSampleRate = MOUSE_DEFAULT_SAMPLERATE;	// Mouse sample rate
uint8_t			mResolution = MOUSE_DEFAULT_RESOLUTION;	// Mouse resolution
uint8_t			mScaling = MOUSE_DEFAULT_SCALING;	// Mouse scaling
uint16_t		mAcceleration = MOUSE_DEFAULT_ACCELERATION;	// Mouse acceleration
uint32_t		mWheelAcc = MOUSE_DEFAULT_WHEELACC;	// Mouse wheel acceleration

std::unordered_map<uint16_t, fabgl::Cursor> mouseCursors;	// Storage for custom mouse cursors
uint16_t		mCursor = MOUSE_DEFAULT_CURSOR;	// Selected mouse cursor

// Forward declarations
//
#ifdef USERSPACE
bool zdi_mode () { return false; }
void zdi_enter () {}
void zdi_process_cmd (uint8_t key) {}
#else
bool zdi_mode ();
void zdi_enter ();
void zdi_process_cmd (uint8_t key);
#endif

bool resetMousePositioner(uint16_t width, uint16_t height, fabgl::VGABaseController * display);
extern bool consoleMode;
extern HardwareSerial DBGSerial;

// Get keyboard instance
//
inline fabgl::Keyboard* getKeyboard() {
	return fabgl::PS2Controller::keyboard();
}

// Get mouse instance
//
inline fabgl::Mouse* getMouse() {
	return fabgl::PS2Controller::mouse();
}

// Keyboard and mouse setup
//
void setupKeyboardAndMouse() {
	fabgl::PS2Controller::begin();
	auto kb = getKeyboard();
	kb->setLayout(&fabgl::UKLayout);
	kb->setCodePage(fabgl::CodePages::get(1252));
	kb->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
	kbEnabled = true;
	resetMousePositioner(canvasW, canvasH, _VGAController.get());
}

// Set keyboard layout
//
void setKeyboardLayout(uint8_t region) {
	auto kb = getKeyboard();
	switch(region) {
		case 1:	kb->setLayout(&fabgl::USLayout); break;
		case 2:	kb->setLayout(&fabgl::GermanLayout); break;
		case 3:	kb->setLayout(&fabgl::ItalianLayout); break;
		case 4:	kb->setLayout(&fabgl::SpanishLayout); break;
		case 5:	kb->setLayout(&fabgl::FrenchLayout); break;
		case 6:	kb->setLayout(&fabgl::BelgianLayout); break;
		case 7:	kb->setLayout(&fabgl::NorwegianLayout); break;
		case 8:	kb->setLayout(&fabgl::JapaneseLayout); break;
		case 9: kb->setLayout(&fabgl::USInternationalLayout); break;
		case 10: kb->setLayout(&fabgl::USInternationalAltLayout); break;
		case 11: kb->setLayout(&fabgl::SwissGLayout); break;
		case 12: kb->setLayout(&fabgl::SwissFLayout); break;
		case 13: kb->setLayout(&fabgl::DanishLayout); break;
		case 14: kb->setLayout(&fabgl::SwedishLayout); break;
		case 15: kb->setLayout(&fabgl::PortugueseLayout); break;
		case 16: kb->setLayout(&fabgl::BrazilianPortugueseLayout); break;
		case 17: kb->setLayout(&fabgl::DvorakLayout); break;
		default:
			kb->setLayout(&fabgl::UKLayout);
			region = 0;
			break;
	}
	kbRegion = region;
}

// Get keyboard key presses
// returns true only if there's a new keypress update
//
bool getKeyboardKey(fabgl::VirtualKeyItem * item) {
	auto kb = getKeyboard();

	if (consoleMode) {
		if (DBGSerial.available()) {
			_keycode = DBGSerial.read();			
			if (!zdi_mode()) {
				if (_keycode == 0x1A) {
					zdi_enter();
					return false;
				}
			} else {
				zdi_process_cmd(_keycode);
				return false;
			}
			item->vk = fabgl::VK_NONE;	// Not actually a virtual key press
			item->scancode[0] = 0;
			item->down = 1;
			item->ASCII = _keycode;
			item->CTRL = 0;
			item->LALT = 0;
			item->RALT = 0;
			item->SHIFT = 0;
			item->GUI = 0;
			item->CAPSLOCK = 0;
			item->NUMLOCK = 0;
			item->SCROLLLOCK = 0;
			return true;			
		}
	}

	if (kb->getNextVirtualKey(item, 0)) {
		if (item->down) {
			// Update stored/global keycode and modifiers values
			switch (item->vk) {
				case fabgl::VK_LEFT:
					_keycode = 0x08;
					break;
				case fabgl::VK_TAB:
					_keycode = 0x09;
					break;
				case fabgl::VK_RIGHT:
					_keycode = 0x15;
					break;
				case fabgl::VK_DOWN:
					_keycode = 0x0A;
					break;
				case fabgl::VK_UP:
					_keycode = 0x0B;
					break;
				case fabgl::VK_BACKSPACE:
					_keycode = 0x7F;
					break;
				default:
					_keycode = item->ASCII;
					break;
			}
		}
		return true;
	}

	return false;
}

uint8_t packKeyboardModifiers(fabgl::VirtualKeyItem * item) {
	return 
		item->CTRL			<< 0 |
		item->SHIFT			<< 1 |
		item->LALT			<< 2 |
		item->RALT			<< 3 |
		item->CAPSLOCK		<< 4 |
		item->NUMLOCK		<< 5 |
		item->SCROLLLOCK	<< 6 |
		item->GUI			<< 7
	;
}

bool shiftKeyPressed() {
	auto kb = getKeyboard();
	fabgl::VirtualKeyItem item;
	return kb->isVKDown(fabgl::VK_LSHIFT) || kb->isVKDown(fabgl::VK_RSHIFT);
}

bool ctrlKeyPressed() {
	if (!kbEnabled) {
		return false;
	}
	auto kb = getKeyboard();
	fabgl::VirtualKeyItem item;
	return kb->isVKDown(fabgl::VK_LCTRL) || kb->isVKDown(fabgl::VK_RCTRL);
}

void getKeyboardState(uint16_t *repeatDelay, uint16_t *repeatRate, uint8_t *ledState) {
	bool numLock;
	bool capsLock;
	bool scrollLock;
	getKeyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
	*ledState = scrollLock | (capsLock << 1) | (numLock << 2);
    *repeatDelay = kbRepeatDelay;
    *repeatRate = kbRepeatRate;
}

void setKeyboardState(uint16_t delay, uint16_t rate, uint8_t ledState) {
	auto kb = getKeyboard();
	if (delay >= 250 && delay <= 1000) kbRepeatDelay = (delay / 250) * 250;	// In steps of 250ms
	if (rate >=  33 && rate <=  500) kbRepeatRate  = rate;
	if (ledState != 255) {
		kb->setLEDs(ledState & 4, ledState & 2, ledState & 1);
	}
    kb->setTypematicRateAndDelay(kbRepeatRate, kbRepeatDelay);
}

bool isSystemMouseCursor(uint16_t cursor) {
	auto minValue = static_cast<CursorName>(std::numeric_limits<std::underlying_type<CursorName>::type>::min());
	auto maxValue = static_cast<CursorName>(std::numeric_limits<std::underlying_type<CursorName>::type>::max());
	return (minValue <= cursor && cursor <= maxValue);
}

void hideMouseCursor() {
	_VGAController->setMouseCursor(nullptr);
	mouseVisible = false;
}

void showMouseCursor() {
	if (isSystemMouseCursor(mCursor)) {
		_VGAController->setMouseCursor(static_cast<CursorName>(mCursor));
	} else if (mouseCursors.find(mCursor) != mouseCursors.end()) {
		_VGAController->setMouseCursor(&mouseCursors[mCursor]);
	} else {
		// Something went wrong, cursor not found
		hideMouseCursor();
		return;
	}
	mouseVisible = true;
}

bool enableMouse() {
	if (mouseEnabled) {
		if (!mouseVisible) {
			showMouseCursor();
		}
		return true;
	}
	auto mouse = getMouse();
	if (!mouse) {
		mouseEnabled = false;
	} else {
		mouse->resumePort();
		mouseEnabled = mouse->isMouseAvailable();
	}
	if (mouseEnabled) {
		showMouseCursor();
	} else {
		hideMouseCursor();
	}
	return mouseEnabled;
}

bool disableMouse() {
	hideMouseCursor();
	if (!mouseEnabled) {
		return true;
	}
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	mouse->suspendPort();
	mouseEnabled = false;
	return true;
}

bool setMouseSampleRate(uint8_t rate) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	// sampleRate: valid values are 10, 20, 40, 60, 80, 100, and 200 (samples/sec)
	if (rate == 0) rate = MOUSE_DEFAULT_SAMPLERATE;
	auto rates = std::vector<uint16_t>{ 10, 20, 40, 60, 80, 100, 200 };
	if (std::find(rates.begin(), rates.end(), rate) == rates.end()) {
		debug_log("vdu_sys_mouse: invalid sample rate %d\n\r", rate);
		return false;
	}

	if (mouse->setSampleRate(rate)) {
		mSampleRate = rate;
		return true;
	}
	return false;
}

bool setMouseResolution(int8_t resolution) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	if (resolution == -1) resolution = MOUSE_DEFAULT_RESOLUTION;
	if (resolution > 3) {
		debug_log("setMouseResolution: invalid resolution %d\n\r", resolution);
		return false;
	}

	if (mouse->setResolution(resolution)) {
		mResolution = resolution;
		return true;
	}
	return false;
}

bool setMouseScaling(uint8_t scaling) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	if (scaling == 0) scaling = MOUSE_DEFAULT_SCALING;
	if (scaling > 2) {
		debug_log("setMouseScaling: invalid scaling %d\n\r", scaling);
		return false;
	}

	if (mouse->setScaling(scaling)) {
		mScaling = scaling;
		return true;
	}
	return false;
}

bool setMouseAcceleration(uint16_t acceleration) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	if (acceleration == 0) acceleration = MOUSE_DEFAULT_ACCELERATION;

	// get current acceleration
	auto & currentAcceleration = mouse->movementAcceleration();

	// change the acceleration
	currentAcceleration = acceleration;

	return false;
}

bool setMouseWheelAcceleration(uint32_t acceleration) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	if (acceleration == 0) acceleration = MOUSE_DEFAULT_WHEELACC;

	// get current acceleration
	auto & currentAcceleration = mouse->wheelAcceleration();

	// change the acceleration
	currentAcceleration = acceleration;

	return false;
}

bool resetMousePositioner(uint16_t width, uint16_t height, fabgl::VGABaseController * display) {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	// setup and then terminate absolute positioner
	// this will set width/height of mouse area for updateAbsolutePosition calls
	mouse->setupAbsolutePositioner(width, height, false, display);
	mouse->terminateAbsolutePositioner();
	return true;
}

fabgl::MouseStatus * setMousePos(uint16_t x, uint16_t y) {
	auto mouse = getMouse();
	if (!mouse) {
		return nullptr;
	}
	auto & m_status = mouse->status();
	m_status.X = fabgl::tclamp((int)x, 0, canvasW - 1);
	m_status.Y = fabgl::tclamp((int)y, 0, canvasH - 1);
	return & m_status;
}

bool resetMouse() {
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	// reset parameters for mouse
	// set default sample rate, resolution, scaling, acceleration, wheel acceleration
	setMouseSampleRate(0);
	setMouseResolution(-1);
	setMouseScaling(0);
	setMouseAcceleration(0);
	setMouseWheelAcceleration(0);
	return mouse->reset();
}

bool mouseMoved(MouseDelta * delta) {
	if (!mouseEnabled) {
		return false;
	}
	auto mouse = getMouse();
	if (!mouse) {
		return false;
	}
	if (mouse->deltaAvailable()) {
		mouse->getNextDelta(delta, -1);
		mouse->updateAbsolutePosition(delta);
		return true;
	}
	return false;
}

void makeMouseCursor(uint16_t bitmapId, std::shared_ptr<Bitmap> bitmap, uint16_t hotX, uint16_t hotY) {
	fabgl::Cursor c;
	c.bitmap = *bitmap;
	c.hotspotX = std::min(static_cast<uint16_t>(std::max(static_cast<int>(hotX), 0)), static_cast<uint16_t>(bitmap->width - 1));
	c.hotspotY = std::min(static_cast<uint16_t>(std::max(static_cast<int>(hotY), 0)), static_cast<uint16_t>(bitmap->height - 1));
	mouseCursors[bitmapId] = c;
}

// Sets the mouse cursor to the given ID
// Works whether mouse is enabled or not
// Cursor will be shown if it exists, otherwise it will be hidden
// Calling with 65535 to hide the cursor (but remember old cursor ID)
bool setMouseCursor(uint16_t cursor = mCursor) {
	bool showing = false;
	if (mouseVisible && cursor == mCursor) {
		// Cursor is already set and visible
		return true;
	}
	if (isSystemMouseCursor(cursor)) {
		mCursor = cursor;
		showing = true;
	} else if (mouseCursors.find(cursor) != mouseCursors.end()) {
		mCursor = cursor;
		showing = true;
	}
	
	if (showing) {
		showMouseCursor();
	} else {
		hideMouseCursor();
	}
	return mouseVisible;
}

void clearMouseCursor(uint16_t cursor) {
	if (cursor == mCursor) {
		mCursor = MOUSE_DEFAULT_CURSOR;
		if (mouseVisible) {
			// Force the cursor to be updated
			showMouseCursor();
		}
	}
	if (mouseCursors.find(cursor) != mouseCursors.end()) {
		mouseCursors.erase(cursor);
	}
}

void resetMouseCursors() {
	if (!isSystemMouseCursor(mCursor)) {
		mCursor = MOUSE_DEFAULT_CURSOR;
		if (mouseVisible) {
			// Force the cursor to be updated
			showMouseCursor();
		}
	}
	mouseCursors.clear();
}

#endif // AGON_PS2_H
