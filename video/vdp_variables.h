#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <memory>
#include <unordered_map>
#include <ESP32Time.h>

#include "agon.h"
#include "agon_ps2.h"
#include "vdu_stream_processor.h"
#include "vdp_protocol.h"

std::unordered_map<uint16_t, uint16_t> featureFlags;	// Feature/Test flags
extern VDUStreamProcessor *processor;
extern ESP32Time		rtc;

// Forward declare getVDPVariable
uint16_t getVDPVariable(uint16_t flag);

void setVDPVariable(uint16_t flag, uint16_t value) {
	if (flag >= VDPVAR_VDU_VARIABLES_START && flag <= VDPVAR_VDU_VARIABLES_END) {
		processor->getContext()->setVariable(flag & VDPVAR_VDU_VARIABLES_MASK, value);
		return;
	}
	if (flag >= VDPVAR_SYSTEM_BEGIN && flag <= VDPVAR_SYSTEM_END) {
		switch (flag) {
			case VDPVAR_RTC_YEAR:
				rtc.setTime(rtc.getSecond(), rtc.getMinute(), rtc.getHour(true), rtc.getDay(), rtc.getMonth(), value);
				return;
			case VDPVAR_RTC_MONTH:
				rtc.setTime(rtc.getSecond(), rtc.getMinute(), rtc.getHour(true), rtc.getDay(), value, rtc.getYear());
				return;
			case VDPVAR_RTC_DAY:
				rtc.setTime(rtc.getSecond(), rtc.getMinute(), rtc.getHour(true), value, rtc.getMonth(), rtc.getYear());
				return;
			case VDPVAR_RTC_HOUR:
				rtc.setTime(rtc.getSecond(), rtc.getMinute(), value, rtc.getDay(), rtc.getMonth(), rtc.getYear());
				return;
			case VDPVAR_RTC_MINUTE:
				rtc.setTime(rtc.getSecond(), value, rtc.getHour(true), rtc.getDay(), rtc.getMonth(), rtc.getYear());
				return;
			case VDPVAR_RTC_SECOND:
				rtc.setTime(value, rtc.getMinute(), rtc.getHour(true), rtc.getDay(), rtc.getMonth(), rtc.getYear());
				return;
			case VDPVAR_RTC_MILLIS:
			case VDPVAR_RTC_WEEKDAY:
			case VDPVAR_RTC_YEARDAY:
			case VDPVAR_FREEPSRAM_LOW:
			case VDPVAR_FREEPSRAM_HIGH:
			case VDPVAR_BUFFERS_USED:
				return;

			case VDPVAR_KEYBOARD_LAYOUT:
				setKeyboardLayout(value);
				return;
			case VDPVAR_KEYBOARD_CTRL_KEYS:
				controlKeys = value != 0;
				return;
			case VDPVAR_KEYBOARD_REP_DELAY:
				setKeyboardState(value, kbRepeatRate, 255);
				return;
			case VDPVAR_KEYBOARD_REP_RATE:
				setKeyboardState(kbRepeatDelay, value, 255);
				return;
			case VDPVAR_KEYBOARD_LED:
				getKeyboard()->setLEDs(value & 4, value & 2, value & 1);
				return;
			case VDPVAR_KEYBOARD_LED_NUM: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				auto kb = getKeyboard();
				kb->getLEDs(&numLock, &capsLock, &scrollLock);
				numLock = value & 1;
				kb->setLEDs(numLock, capsLock, scrollLock);
				return;
			}
			case VDPVAR_KEYBOARD_LED_CAPS: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				auto kb = getKeyboard();
				kb->getLEDs(&numLock, &capsLock, &scrollLock);
				capsLock = value & 1;
				kb->setLEDs(numLock, capsLock, scrollLock);
				return;
			}
			case VDPVAR_KEYBOARD_LED_SCROLL: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				auto kb = getKeyboard();
				kb->getLEDs(&numLock, &capsLock, &scrollLock);
				scrollLock = value & 1;
				kb->setLEDs(numLock, capsLock, scrollLock);
				return;
			}
				
			case VDPVAR_CONTEXT_ID:
				processor->selectContext(value);
				processor->sendModeInformation();
				return;

			case VDPVAR_MOUSE_CURSOR:	// Mouse cursor ID
				setMouseCursor(value);
				return;
			case VDPVAR_MOUSE_ENABLED:	// Mouse cursor enabled
				if (value) {
					enableMouse();
					processor->sendMouseData();
				} else {
					disableMouse();
				}
				return;
			case VDPVAR_MOUSE_XPOS: {	// Mouse cursor X position (pixel coords)
				uint16_t mouseY = getVDPVariable(VDPVAR_MOUSE_YPOS);
				auto status = setMousePos(value, mouseY);
				value = status->X;
				setMouseCursorPos(status->X, status->Y);
				auto osPos = processor->getContext()->toCurrentCoordinates(status->X, status->Y);
				featureFlags[VDPVAR_MOUSE_XPOS_OS] = osPos.X;
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_YPOS: {	// Mouse cursor Y position (pixel coords)
				uint16_t mouseX = getVDPVariable(VDPVAR_MOUSE_XPOS);
				auto status = setMousePos(mouseX, value);
				value = status->Y;
				setMouseCursorPos(status->X, status->Y);
				auto osPos = processor->getContext()->toCurrentCoordinates(status->X, status->Y);
				featureFlags[VDPVAR_MOUSE_YPOS_OS] = osPos.Y;
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_BUTTONS:	// Mouse cursor button status
			case VDPVAR_MOUSE_WHEEL: 	// Mouse wheel
				processor->sendMouseData();
				break;
			case VDPVAR_MOUSE_SAMPLERATE:	// Mouse sample rate
				setMouseSampleRate(value);
				return;
			case VDPVAR_MOUSE_RESOLUTION:	// Mouse resolution
				setMouseResolution(value);
				return;
			case VDPVAR_MOUSE_SCALING:	// Mouse scaling
				setMouseScaling(value);
				return;
			case VDPVAR_MOUSE_ACCELERATION:	// Mouse acceleration
				setMouseAcceleration(value);
				return;
			case VDPVAR_MOUSE_WHEELACC:	// Mouse wheel acceleration
				setMouseWheelAcceleration(value);
				return;
			case VDPVAR_MOUSE_VISIBLE:	// Mouse cursor visible (1) or hidden (0)
				if (value) {
					showMouseCursor();
				} else {
					hideMouseCursor();
				}
				return;
			// we have a range here (0x24C-0x24F) reserved for mouse area
			case VDPVAR_MOUSE_XPOS_OS: {	// Mouse X position (OS coords)
				uint16_t mouseY = getVDPVariable(VDPVAR_MOUSE_YPOS_OS);
				auto screenPos = processor->getContext()->toScreenCoordinates(value, mouseY);
				auto status = setMousePos(screenPos.X, screenPos.Y);
				setMouseCursorPos(status->X, status->Y);
				featureFlags[VDPVAR_MOUSE_XPOS] = status->X;
				if (screenPos.X != status->X) {
					// position adjusted, so update variable
					auto newPos = processor->getContext()->toCurrentCoordinates(status->X, status->Y);
					value = newPos.X;
				}
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_YPOS_OS: {	// Mouse Y position (OS coords)
				uint16_t mouseX = getVDPVariable(VDPVAR_MOUSE_XPOS_OS);
				auto screenPos = processor->getContext()->toScreenCoordinates(mouseX, value);
				auto status = setMousePos(screenPos.X, screenPos.Y);
				setMouseCursorPos(status->X, status->Y);
				featureFlags[VDPVAR_MOUSE_YPOS] = status->Y;
				if (screenPos.Y != status->Y) {
					// position adjusted, so update variable
					auto newPos = processor->getContext()->toCurrentCoordinates(status->X, status->Y);
					value = newPos.Y;
				}
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_DELTAX: {		// Mouse delta X (pixel coords)
				uint16_t deltaY = getVDPVariable(VDPVAR_MOUSE_DELTAY);
				auto osPos = processor->getContext()->toCurrentCoordinates(value, deltaY);
				featureFlags[VDPVAR_MOUSE_DELTAX_OS] = osPos.X;
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_DELTAY: {		// Mouse delta Y (pixel coords)
				uint16_t deltaX = getVDPVariable(VDPVAR_MOUSE_DELTAX);
				auto osDelta = processor->getContext()->toCurrentCoordinates(deltaX, value);
				featureFlags[VDPVAR_MOUSE_DELTAY_OS] = osDelta.Y;
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_DELTAX_OS: {	// Mouse delta X (OS coords)
				uint16_t deltaY = getVDPVariable(VDPVAR_MOUSE_DELTAY_OS);
				auto screenDelta = processor->getContext()->toScreenCoordinates(value, deltaY);
				featureFlags[VDPVAR_MOUSE_DELTAX] = screenDelta.X;
				processor->sendMouseData();
			}	break;
			case VDPVAR_MOUSE_DELTAY_OS: {	// Mouse delta Y (OS coords)
				uint16_t deltaX = getVDPVariable(VDPVAR_MOUSE_DELTAX_OS);
				auto screenDelta = processor->getContext()->toScreenCoordinates(deltaX, value);
				featureFlags[VDPVAR_MOUSE_DELTAY] = screenDelta.Y;
				processor->sendMouseData();
			}	break;

			case VDPVAR_KEYEVENT_MODIFIERS: {
				// Set individual modifier variables based on new value
				for (uint8_t bit = 0; bit < 8; bit++) {
					featureFlags[VDPVAR_KEYEVENT_CTRL + bit] = (value & (1 << bit)) ? 1 : 0;
				}
			}	break;
			case VDPVAR_KEYEVENT_CTRL:
			case VDPVAR_KEYEVENT_SHIFT:
			case VDPVAR_KEYEVENT_LALT:
			case VDPVAR_KEYEVENT_RALT:
			case VDPVAR_KEYEVENT_CAPSLOCK:
			case VDPVAR_KEYEVENT_NUMLOCK:
			case VDPVAR_KEYEVENT_SCROLLLOCK:
			case VDPVAR_KEYEVENT_GUI: {
				// Update combined modifiers variable
				uint16_t modifierBit = 1 << (flag - VDPVAR_KEYEVENT_CTRL);
				if (value == 0) {
					// clear modifier bit
					featureFlags[VDPVAR_KEYEVENT_MODIFIERS] &= ~modifierBit;
				} else {
					// set modifier bit
					featureFlags[VDPVAR_KEYEVENT_MODIFIERS] |= modifierBit;
				}
				processor->sendKeyboardData();
			}	break;
			case VDPVAR_KEYEVENT_VK:
			case VDPVAR_KEYEVENT_DOWN: {
				processor->sendKeyboardData();
			}	break;
		}
	}
	switch (flag) {
		case VDPVAR_FULL_DUPLEX:
			setVDPProtocolDuplex(value != 0);
			debug_log("Full duplex mode requested\n\r");
			break;
		case TESTFLAG_ECHO:
			debug_log("Echo mode requested\n\r");
			processor->setEcho(value != 0);
			break;
		case TESTFLAG_VDPP_BUFFERSIZE:
			debug_log("Echo buffer size requested: %d\n\r", value);
			break;
	}
	if (flag >= VDPVAR_KEYMAP_START && flag < (VDPVAR_KEYMAP_START + fabgl::VK_LAST)) {
		return;
	}

	featureFlags[flag] = value;
}

void clearVDPVariable(uint16_t flag) {
	auto flagIter = featureFlags.find(flag);

	switch (flag) {
		case VDPVAR_FULL_DUPLEX:
			setVDPProtocolDuplex(false);
			debug_log("Full duplex mode disabled\n\r");
			break;

		case TESTFLAG_ECHO:
			debug_log("Echo mode disabled\n\r");
			processor->setEcho(false);
			break;

		case VDPVAR_MOUSE_CURSOR:	// Mouse cursor ID
			setMouseCursor(MOUSE_DEFAULT_CURSOR);
			hideMouseCursor();
			return;
		case VDPVAR_MOUSE_ENABLED:
			disableMouse();
			return;
		case VDPVAR_MOUSE_VISIBLE:
			hideMouseCursor();
			return;

		case VDPVAR_KEYEVENT_MODIFIERS:
		case VDPVAR_KEYEVENT_CTRL:
		case VDPVAR_KEYEVENT_SHIFT:
		case VDPVAR_KEYEVENT_LALT:
		case VDPVAR_KEYEVENT_RALT:
		case VDPVAR_KEYEVENT_CAPSLOCK:
		case VDPVAR_KEYEVENT_NUMLOCK:
		case VDPVAR_KEYEVENT_SCROLLLOCK:
		case VDPVAR_KEYEVENT_GUI: {
			setVDPVariable(flag, 0);
		}	return;
	}

	if (flagIter != featureFlags.end()) {
		featureFlags.erase(flagIter);
	}
}

bool isVDPVariableSet(uint16_t flag) {
	if (flag >= VDPVAR_VDU_VARIABLES_START && flag <= VDPVAR_VDU_VARIABLES_END) {
		return processor->getContext()->readVariable(flag & VDPVAR_VDU_VARIABLES_MASK, nullptr);
	}
	if (flag >= VDPVAR_SYSTEM_BEGIN && flag <= VDPVAR_SYSTEM_END) {
		switch (flag) {
			case VDPVAR_RTC_YEAR:
			case VDPVAR_RTC_MONTH:
			case VDPVAR_RTC_DAY:
			case VDPVAR_RTC_HOUR:
			case VDPVAR_RTC_MINUTE:
			case VDPVAR_RTC_SECOND:
			case VDPVAR_RTC_MILLIS:
			case VDPVAR_RTC_WEEKDAY:
			case VDPVAR_RTC_YEARDAY:
			case VDPVAR_FREEPSRAM_LOW:
			case VDPVAR_FREEPSRAM_HIGH:
			case VDPVAR_BUFFERS_USED:
			case VDPVAR_KEYBOARD_LAYOUT:
			case VDPVAR_KEYBOARD_CTRL_KEYS:
			case VDPVAR_KEYBOARD_REP_DELAY:
			case VDPVAR_KEYBOARD_REP_RATE:
			case VDPVAR_KEYBOARD_LED:
			case VDPVAR_KEYBOARD_LED_NUM:
			case VDPVAR_KEYBOARD_LED_CAPS:
			case VDPVAR_KEYBOARD_LED_SCROLL:
			case VDPVAR_CONTEXT_ID:
			case VDPVAR_MOUSE_CURSOR:
			case VDPVAR_MOUSE_ENABLED:
			case VDPVAR_MOUSE_SAMPLERATE:
			case VDPVAR_MOUSE_RESOLUTION:
			case VDPVAR_MOUSE_SCALING:
			case VDPVAR_MOUSE_ACCELERATION:
			case VDPVAR_MOUSE_WHEELACC:
			case VDPVAR_MOUSE_VISIBLE:
				return true;
		}
	}
	if (flag >= VDPVAR_KEYMAP_START && flag < (VDPVAR_KEYMAP_START + fabgl::VK_LAST)) {
		return true;
	}
	auto flagIter = featureFlags.find(flag);
	return flagIter != featureFlags.end();
}

uint16_t getVDPVariable(uint16_t flag) {
	if (flag >= VDPVAR_VDU_VARIABLES_START && flag <= VDPVAR_VDU_VARIABLES_END) {
		uint16_t value = 0;
		processor->getContext()->readVariable(flag & VDPVAR_VDU_VARIABLES_MASK, &value);
		return value;
	}
	auto flagIter = featureFlags.find(flag);
	if (flagIter != featureFlags.end()) {
		return featureFlags[flag];
	} else {
		switch (flag) {
			case VDPVAR_RTC_YEAR:
				return rtc.getYear();
			case VDPVAR_RTC_MONTH:
				return rtc.getMonth();		// 0 - 11
			case VDPVAR_RTC_DAY:
				return rtc.getDay();		// 1 - 31
			case VDPVAR_RTC_HOUR:
				return rtc.getHour(true);	// 0 - 23
			case VDPVAR_RTC_MINUTE:
				return rtc.getMinute();		// 0 - 59
			case VDPVAR_RTC_SECOND:
				return rtc.getSecond();		// 0 - 59
			case VDPVAR_RTC_MILLIS:
				return rtc.getMillis();		// 0 - 999
			case VDPVAR_RTC_WEEKDAY:
				return rtc.getDayofWeek();	// 0 - 6
			case VDPVAR_RTC_YEARDAY:
				return rtc.getDayofYear();	// 0 - 365

			case VDPVAR_FREEPSRAM_LOW:
				return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) & 0xFFFF;
			case VDPVAR_FREEPSRAM_HIGH:
				return heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 16;

			case VDPVAR_BUFFERS_USED:
				return buffers.size();

			case VDPVAR_KEYBOARD_LAYOUT:
				return kbRegion;
			case VDPVAR_KEYBOARD_CTRL_KEYS:
				return controlKeys ? 1 : 0;
			case VDPVAR_KEYBOARD_REP_DELAY:
				return kbRepeatDelay;
			case VDPVAR_KEYBOARD_REP_RATE:
				return kbRepeatRate;
			case VDPVAR_KEYBOARD_LED: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				getKeyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
				return scrollLock | (capsLock << 1) | (numLock << 2);
			}
			case VDPVAR_KEYBOARD_LED_NUM: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				getKeyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
				return numLock ? 1 : 0;
			}
			case VDPVAR_KEYBOARD_LED_CAPS: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				getKeyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
				return capsLock ? 1 : 0;
			}
			case VDPVAR_KEYBOARD_LED_SCROLL: {
				bool numLock;
				bool capsLock;
				bool scrollLock;
				getKeyboard()->getLEDs(&numLock, &capsLock, &scrollLock);
				return scrollLock ? 1 : 0;
			}
				
			case VDPVAR_CONTEXT_ID:
				return processor->contextId;

			case VDPVAR_MOUSE_CURSOR:
				return mCursor;
			case VDPVAR_MOUSE_ENABLED:
				return mouseEnabled ? 1 : 0;
			case VDPVAR_MOUSE_SAMPLERATE:
				return mSampleRate;
			case VDPVAR_MOUSE_RESOLUTION:
				return mResolution;
			case VDPVAR_MOUSE_SCALING:
				return mScaling;
			case VDPVAR_MOUSE_ACCELERATION:
				return mAcceleration;
			case VDPVAR_MOUSE_WHEELACC: {
				auto mouse = getMouse();
				if (mouse) {
					auto & currentAcceleration = mouse->wheelAcceleration();
					return currentAcceleration;
				}
			}	break;
			case VDPVAR_MOUSE_VISIBLE:
				return mouseVisible ? 1 : 0;
		}
		if (flag >= VDPVAR_KEYMAP_START && flag < (VDPVAR_KEYMAP_START + fabgl::VK_LAST)) {
			// Return 1/0 for key down in lower byte, and ASCII code in upper byte
			uint16_t key = flag - VDPVAR_KEYMAP_START;
			uint16_t value = getKeyboard()->isVKDown((fabgl::VirtualKey)key) ? 1 : 0;
			fabgl::VirtualKeyItem keyItem;
			keyItem.vk = (fabgl::VirtualKey)key;
			keyItem.CTRL = 0;
			keyItem.SHIFT = 0;
			keyItem.SCROLLLOCK = 0;
			auto keyASCII = fabgl::virtualKeyToASCII(keyItem, fabgl::CodePages::get(1252));
			if (keyASCII != -1) {
				value = value | (keyASCII << 8);
			}
			return value;
		}
	}
	return 0;
}

#endif // FEATURE_FLAGS_H
