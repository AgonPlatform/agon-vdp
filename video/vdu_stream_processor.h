#ifndef VDU_STREAM_PROCESSOR_H
#define VDU_STREAM_PROCESSOR_H

#include <memory>
#include <unordered_map>
#include <vector>

#include <Stream.h>
#include <fabgl.h>

#include "agon.h"
#include "buffers.h"
#include "context.h"
#include "buffer_stream.h"
#include "span.h"
#include "types.h"

extern uint16_t getVDPVariable(uint16_t flag);

class VDUStreamProcessor {
	private:
		std::shared_ptr<Stream> inputStream;
		std::shared_ptr<Stream> outputStream;
		std::shared_ptr<Stream> originalOutputStream;

		// Graphics context storage and management
		std::shared_ptr<Context> context;		// Current active context
		ContextVectorPtr contextStack;			// Current active context stack

		bool commandsEnabled = true;
		bool echoEnabled = false;
		bool echoBuffering = false;
		bool pendingMouseData = false;
		bool pendingKeyboardData = false;

		std::vector<uint8_t> echoBuffer;

		int16_t readByte_t(uint16_t timeout);
		int32_t readWord_t(uint16_t timeout);
		int32_t read24_t(uint16_t timeout);
		uint8_t readByte_b();
		uint32_t readIntoBuffer(uint8_t * buffer, uint32_t length, uint16_t timeout);
		uint32_t discardBytes(uint32_t length, uint16_t timeout);
		int16_t peekByte_t(uint16_t timeout);
		float readFloat_t(bool is16Bit, bool isFixed, int8_t shift, uint16_t timeout);
		bool readFloatArguments(float *values, int count, bool useBufferValue, bool useAdvancedOffsets, bool useMultiFormat);

		inline void pushEcho(uint8_t c);
		void pushEcho(uint8_t * chars, uint32_t length);
		inline void clearEcho();
		void flushEcho();

		void handleKeyboardAndMouse();
		void updateMouseVars(MouseDelta *delta);
		void sendPendingKeyboardAndMouse();

		void vdu_print(char c, bool usePeek);
		void vdu_colour();
		void vdu_gcol();
		void vdu_palette();
		void vdu_restorePalette();
		void vdu_graphicsViewport();
		void vdu_plot();
		void vdu_resetViewports();
		void vdu_textViewport();
		void vdu_origin();
		void vdu_cursorTab();

		void vdu_sys();
		void vdu_sys_video();
		void sendGeneralPoll();
		void vdu_sys_video_kblayout();
		void sendCursorPosition();
		void sendScreenChar(char c);
		void sendScreenPixel(uint16_t x, uint16_t y);
		void sendColour(uint8_t colour);
		void sendScrPixelPacket();
		void printBuffer(uint16_t bufferId);
		void sendTime();
		void vdu_sys_video_time();
		void sendKeyboardState();
		void vdu_sys_keystate();
		void vdu_sys_mouse();
		void vdu_sys_copper();
		void vdu_sys_scroll();
		void vdu_sys_cursorBehaviour();
		void vdu_sys_udg(char c);

		void vdu_sys_audio();
		void sendAudioStatus(uint8_t channel, uint8_t status);
		uint8_t loadSample(uint16_t bufferId, uint32_t length);
		uint8_t createSampleFromBuffer(uint16_t bufferId, uint8_t format, uint16_t sampleRate);
		uint8_t setVolumeEnvelope(uint8_t channel, uint8_t type);
		uint8_t setFrequencyEnvelope(uint8_t channel, uint8_t type);
		uint8_t setSampleFrequency(uint16_t bufferId, uint16_t frequency);
		uint8_t setSampleRepeatStart(uint16_t bufferId, uint32_t offset);
		uint8_t setSampleRepeatLength(uint16_t bufferId, uint32_t length);
		uint8_t setParameter(uint8_t channel, uint8_t parameter, uint16_t value);

		void vdu_sys_font();

		void vdu_sys_context();
		bool resetContext(uint8_t flags);
		void saveContext();
		void restoreContext();
		void saveAndSelectContext(uint8_t contextId);
		void restoreAllContexts();
		void clearContextStack();
		void resetAllContexts();

		void vdu_sys_sprites();
		void receiveBitmap(uint16_t bufferId, uint16_t width, uint16_t height);
		void createBitmapFromScreen(uint16_t bufferId);
		void createEmptyBitmap(uint16_t bufferId, uint16_t width, uint16_t height, uint32_t color);
		void createBitmapFromBuffer(uint16_t bufferId, uint8_t format, uint16_t width, uint16_t height);

		void vdu_sys_hexload(void);
		void sendKeycodeByte(uint8_t b, bool waitack);

		void vdu_sys_buffered();
		uint32_t bufferWrite(uint16_t bufferId, uint32_t size);
		void bufferCall(uint16_t bufferId, AdvancedOffset offset);
		void bufferRemoveUsers(uint16_t bufferId);
		void bufferClear(uint16_t bufferId);
		std::shared_ptr<WritableBufferStream> bufferCreate(uint16_t bufferId, uint32_t size);
		void setOutputStream(uint16_t bufferId);
		AdvancedOffset getOffsetFromStream(bool isAdvanced);
		std::vector<uint16_t> getBufferIdsFromStream();
		void bufferAdjust(uint16_t bufferId);
		bool bufferConditional();
		void bufferJump(uint16_t bufferId, AdvancedOffset offset);
		void bufferCopy(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds);
		void bufferConsolidate(uint16_t bufferId);
		void clearTargets(tcb::span<const uint16_t> targets);
		void bufferSplitInto(uint16_t bufferId, uint16_t length, tcb::span<uint16_t> newBufferIds, bool iterate);
		void bufferSplitByInto(uint16_t bufferId, uint16_t width, uint16_t chunkCount, tcb::span<uint16_t> newBufferIds, bool iterate);
		void bufferSpreadInto(uint16_t bufferId, tcb::span<uint16_t> newBufferIds, bool iterate);
		void bufferReverseBlocks(uint16_t bufferId);
		void bufferReverse(uint16_t bufferId, uint8_t options);
		void bufferCopyRef(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds);
		void bufferCopyAndConsolidate(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds);
		void bufferAffineTransform(uint16_t bufferId, uint8_t command, bool is3D);
		void bufferMatrixManipulate(uint16_t bufferId, uint8_t command, MatrixSize size);
		void bufferTransformBitmap(uint16_t bufferId, uint8_t options, uint16_t transformBufferId, uint16_t sourceBufferId);
		void bufferTransformData(uint16_t bufferId, uint8_t options, uint8_t format, uint16_t transformBufferId, uint16_t sourceBufferId);
		void bufferReadVariable(uint16_t bufferId);
		void bufferCompress(uint16_t bufferId, uint16_t sourceBufferId);
		void bufferDecompress(uint16_t bufferId, uint16_t sourceBufferId);
		void bufferExpandBitmap(uint16_t bufferId, uint8_t options, uint16_t sourceBufferId);
		void bufferAddCallback(uint16_t bufferId, uint16_t type);
		void bufferRemoveCallback(uint16_t bufferId, uint16_t type);

		void vdu_sys_updater();
		void unlock();
		void receiveFirmware();
		void switchFirmware();

		// Begin: Tile Engine

		void vdu_sys_layers();
		void vdu_sys_layers_tilebank_init(uint8_t tileBankNum, uint8_t tileBankBitDepth);
		void vdu_sys_layers_tilebank_load(uint8_t tileBankNum, uint8_t tileId);
		void vdu_sys_layers_tilebank_draw(uint8_t tileBankNum, uint8_t tileId, uint8_t palette, uint8_t x, uint8_t y, uint8_t xOffset, uint8_t yOffset, uint8_t tileAttribute);
		void vdu_sys_layers_tilebank_free(uint8_t tileBankNum);
		void vdu_sys_layers_tilemap_init(uint8_t tileLayerNum, uint8_t tileMapSize);
		void vdu_sys_layers_tilemap_set(uint8_t tileLayerNum, uint8_t x, uint8_t y, uint8_t tileId, uint8_t tileAttribute);
		void vdu_sys_layers_tilemap_free(uint8_t tileMapNum);
		void vdu_sys_layers_tilelayer_init(uint8_t tileLayerNum, uint8_t tileLayerSize, uint8_t tileSize);
		void vdu_sys_layers_tilelayer_set_scroll(uint8_t tileLayerNum, uint8_t x, uint8_t y, uint8_t xOffset, uint8_t yOffset);
		void vdu_sys_layers_tilelayer_update_layerbuffer(uint8_t tileLayerNum);
		void vdu_sys_layers_tilelayer_draw_layerbuffer(uint8_t tileLayerNum);
		void vdu_sys_layers_tilelayer_draw(uint8_t tileLayerNum);
		void vdu_sys_layers_tilelayer_free(uint8_t tileLayerNum);
		void writeTileToBuffer(uint8_t tileId, uint8_t tileCount, uint8_t xOffset, uint8_t tileBuffer[], uint8_t tileLayerWidth);
		void writeTileToBufferFlipX(uint8_t tileId, uint8_t tileCount, uint8_t xOffset, uint8_t tileBuffer[], uint8_t tileLayerWidth);
		void writeTileToBufferFlipY(uint8_t tileId, uint8_t tileCount, uint8_t xOffset, uint8_t tileBuffer[], uint8_t tileLayerWidth);
		void writeTileToBufferFlipXY(uint8_t tileId, uint8_t tileCount, uint8_t xOffset, uint8_t tileBuffer[], uint8_t tileLayerWidth);

		void writeTileToLayerBuffer(uint8_t tileId, uint8_t xPos, uint8_t xOffset, uint8_t yPos, uint8_t yOffset, uint8_t * tileBuffer,  uint8_t tileLayerHeight, uint8_t tileLayerWidth);
		void writeTileToLayerBufferFlipX(uint8_t tileId, uint8_t xPos, uint8_t xOffset, uint8_t yPos, uint8_t yOffset, uint8_t * tileBuffer, uint8_t tileLayerHeight, uint8_t tileLayerWidth);
		void writeTileToLayerBufferFlipY(uint8_t tileId, uint8_t xPos, uint8_t xOffset, uint8_t yPos, uint8_t yOffset, uint8_t * tileBuffer, uint8_t tileLayerHeight, uint8_t tileLayerWidth);
		void writeTileToLayerBufferFlipXY(uint8_t tileId, uint8_t xPos, uint8_t xOffset, uint8_t yPos, uint8_t yOffset, uint8_t * tileBuffer, uint8_t tileLayerHeight, uint8_t tileLayerWidth);

		// Tile Bank variables

		void * tileBank0Data = NULL;
		uint8_t * tileBank0Ptr;

		Bitmap currentTile; 
		uint8_t currentTileDataBuffer[64];

		// Tile Map variables

		struct Tile {
			uint8_t	id;
			uint8_t attribute;
		};

		struct Tile** tileMap0 = NULL;

		struct TileMap {
			uint8_t height;
			uint8_t width;
		};

		TileMap tileMap0Properties;

		// Tile Layer variables

		Bitmap currentRow;
		uint8_t currentRowDataBuffer[5184];		// Buffer big enough for 64 byte tiles * 81 columns (the largest supported size +1)

		Bitmap tileLayer0Bitmap;					// Bitmap that points to the Layer 0 buffer

		void * tileLayer0Buffer = NULL;				// The offscreen buffer for Layer 0

		uint8_t * tileLayer0Ptr;					// A pointer to he tileLayer0Buffer

		struct TileLayer {
			uint8_t height;
			uint8_t width;
			uint8_t sourceXPos;
			uint8_t sourceYPos;
			uint8_t xOffset;
			uint8_t yOffset;
			uint8_t attribute;
			uint8_t backgroundColour = 0;			// Default the background colour of the layer to 0 (transparent)
		};

		TileLayer tileLayer0;
		TileLayer tileLayer1;
		TileLayer tileLayer2;

		uint8_t tileLayer0init = 0;		

		// End: Tile Engine

	public:
		uint16_t id = 65535;
		uint8_t contextId = 0;					// Current active context ID

		VDUStreamProcessor(Stream *input) :
			inputStream(std::shared_ptr<Stream>(input)), outputStream(inputStream), originalOutputStream(inputStream) {
				contextId = 0;
				// Get the default context stack, if it exists
				// (NB this will only be possible when we support multiple stream processors)
				if (contextExists(0)) {
					contextStack = contextStacks[0];
					context = contextStack->back();
				} else {
					context = make_shared_psram<Context>();
					contextStack = make_shared_psram<ContextVector>();
					contextStacks[0] = contextStack;
					contextStack->push_back(context);
				}
			}

		inline bool byteAvailable() {
			return inputStream->available() > 0;
		}
		inline uint8_t readByte() {
			auto read = inputStream->read();
			pushEcho(read);
			return read;
		}
		inline void writeByte(uint8_t b) {
			if (outputStream) {
				outputStream->write(b);
			}
		}
		void send_packet(uint8_t code, uint16_t len, uint8_t data[]);

		inline void sendMouseData();
		inline void sendKeyboardData();

		void processAllAvailable();
		void processNext();
		void doCursorFlash() {
			context->doCursorFlash();
		}
		void hideCursor() {
			context->hideCursor();
		}
		void showCursor() {
			context->showCursor();
		}

		void vdu(uint8_t c, bool usePeek = true);

		void wait_eZ80();
		void sendModeInformation();

		void setEcho(bool enabled) {
			flushEcho();
			echoEnabled = enabled;
			if (!enabled) {
				// Send an echo end packet
				bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_ECHO_END);
				auto handle = getVDPVariable(TESTFLAG_ECHO);
				if (handle == 0) {
					return;
				}
				uint8_t packet[] = {
					static_cast<uint8_t>(handle & 0xFF),
				};
				send_packet(PACKET_ECHO_END, sizeof packet, packet);
			}
		}

		std::shared_ptr<Context> getContext() {
			return context;
		}
		bool contextExists(uint8_t id) {
			return contextStacks.find(id) != contextStacks.end();
		}
		void selectContext(uint8_t contextId);

		void vdu_mode(uint8_t mode);

		void bufferCallCallbacks(uint16_t type);
};

// Read an unsigned byte from the serial port, with a timeout
// Returns:
// - Byte value (0 to 255) if value read, otherwise -1
//
int16_t inline VDUStreamProcessor::readByte_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto read = inputStream->read();
	if (read != -1) {
		pushEcho(read);
		return read;
	}

	auto start = xTaskGetTickCountFromISR();
	const auto timeCheck = pdMS_TO_TICKS(timeout);

	do {
		read = inputStream->read();
	} while (read == -1 && (xTaskGetTickCountFromISR() - start < timeCheck));
	pushEcho(read);
	return read;
}

// Read an unsigned word from the serial port, with a timeout
// Returns:
// - Word value (0 to 65535) if 2 bytes read, otherwise -1
//
int32_t VDUStreamProcessor::readWord_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto l = readByte_t(timeout);
	if (l != -1) {
		auto h = readByte_t(timeout);
		if (h != -1) {
			return (h << 8) | l;
		}
	}
	return -1;
}

// Read an unsigned 24-bit value from the serial port, with a timeout
// Returns:
// - Value (0 to 16777215) if 3 bytes read, otherwise -1
//
int32_t VDUStreamProcessor::read24_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto l = readByte_t(timeout);
	if (l != -1) {
		auto m = readByte_t(timeout);
		if (m != -1) {
			auto h = readByte_t(timeout);
			if (h != -1) {
				return (h << 16) | (m << 8) | l;
			}
		}
	}
	return -1;
}

// Read an unsigned byte from the serial port (blocking)
//
uint8_t VDUStreamProcessor::readByte_b() {
	while (inputStream->available() == 0);
	return readByte();
}

// Read a given number of bytes from the serial port into a buffer
// Returns number of remaining bytes
// which should be zero if all bytes were read
// but will be non-zero if the read timed out
//
uint32_t VDUStreamProcessor::readIntoBuffer(uint8_t * buffer, uint32_t length, uint16_t timeout = COMMS_TIMEOUT) {
	uint32_t remaining = length;
	if (buffer == nullptr) {
		debug_log("readIntoBuffer: buffer is null\n\r");
		return remaining;
	}

	while (remaining > 0) {
		auto read = inputStream->readBytes(buffer, remaining);
		if (read == 0) {
			// timed out - perform a single retry
			read = inputStream->readBytes(buffer, remaining);
			if (read == 0) {
				debug_log("readIntoBuffer: timed out\n\r");
				return remaining;
			}
		}
		pushEcho(buffer, read);
		buffer += read;
		remaining -= read;
	}
	return remaining;
}

// Discard a given number of bytes from input stream
// Returns 0 on success, or the number of bytes remaining if timed out
//
uint32_t VDUStreamProcessor::discardBytes(uint32_t length, uint16_t timeout = COMMS_TIMEOUT) {
	uint32_t remaining = length;
	auto bufferSize = 64;
	auto readSize = bufferSize;
	auto buffer = make_unique_psram_array<uint8_t>(bufferSize);

	while (remaining > 0) {
		if (remaining < readSize) {
			readSize = remaining;
		}
		if (readIntoBuffer(buffer.get(), readSize, timeout) != 0) {
			// timed out
			return remaining;
		}
		remaining -= readSize;
	}
	return remaining;
}

// Peek at the next byte in the command stream
// returns -1 if timed out, or the byte value (0 to 255)
//
int16_t VDUStreamProcessor::peekByte_t(uint16_t timeout = COMMS_TIMEOUT) {
	auto start = xTaskGetTickCountFromISR();
	const auto timeCheck = pdMS_TO_TICKS(timeout);

	while (xTaskGetTickCountFromISR() - start < timeCheck) {
		if (inputStream->available() > 0) {
			return inputStream->peek();
		}
	}
	return -1;
}

// Read a float value from the stream, given the specified format
// Returns the float value, or INFINITY if timed out
//
float VDUStreamProcessor::readFloat_t(bool is16Bit, bool isFixed, int8_t shift, uint16_t timeout = COMMS_TIMEOUT) {
	// get the value that we're dealing with
	uint32_t rawValue = 0;
	auto bytesToRead = is16Bit ? 2 : 4;
	// read the value from the stream
	if (readIntoBuffer((uint8_t *)&rawValue, bytesToRead, timeout) != 0) {
		return INFINITY;
	}
	return convertValueToFloat(rawValue, is16Bit, isFixed, shift);
}

// Reads a series of float values from the stream
// Returns true if all values were read,
// or false if timed out or another issue occurred, such as a non-existant buffer was specified
// Stream will contain a format byte, followed by float values
// if useMultiFormat is true, then each float will be preceded by a format byte
// if useBufferValue is true, then the "value" will be a buffer ID and offset to fetch the value from
//
bool VDUStreamProcessor::readFloatArguments(float *values, int count, bool useBufferValue, bool useAdvancedOffsets, bool useMultiFormat) {
	bool isFixed, is16Bit;
	int8_t shift;
	int32_t sourceBufferId = -1;
	AdvancedOffset offset = {};

	for (int i = 0; i < count; i++) {
		if (i == 0 || useMultiFormat) {
			auto format = readByte_t();
			if (format == -1) {
				return false;
			}
			extractFormatInfo(format, isFixed, is16Bit, shift);
			if (useBufferValue) {
				sourceBufferId = readWord_t();
				if (sourceBufferId == -1) {
					return false;
				}
				offset = getOffsetFromStream(useAdvancedOffsets);
				if (offset.blockOffset == -1) {
					return false;
				}
			}
		}
		values[i] = useBufferValue ? readBufferFloat(sourceBufferId, offset, is16Bit, isFixed, shift, true) : readFloat_t(is16Bit, isFixed, shift);
		if (values[i] == INFINITY) {
			return false;
		}
	}
	return true;
};


// Send a packet of data to the MOS
//
void VDUStreamProcessor::send_packet(uint8_t code, uint16_t len, uint8_t data[]) {
	writeByte(code + 0x80);
	writeByte(len);
	for (int i = 0; i < len; i++) {
		writeByte(data[i]);
	}
	bufferCallCallbacks(CALLBACK_SENT_VDPP | code);
}

inline void VDUStreamProcessor::sendMouseData() {
	pendingMouseData = true;
}

inline void VDUStreamProcessor::sendKeyboardData() {
	pendingKeyboardData = true;
}

void VDUStreamProcessor::sendPendingKeyboardAndMouse() {
	if (pendingMouseData) {
		bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_MOUSE);
		pendingMouseData = false;
		uint16_t mouseX = getVDPVariable(VDPVAR_MOUSE_XPOS_OS);
		uint16_t mouseY = getVDPVariable(VDPVAR_MOUSE_YPOS_OS);
		uint8_t buttons = getVDPVariable(VDPVAR_MOUSE_BUTTONS);
		uint8_t wheelDelta = getVDPVariable(VDPVAR_MOUSE_WHEEL);
		uint16_t deltaX = getVDPVariable(VDPVAR_MOUSE_DELTAX_OS);
		uint16_t deltaY = getVDPVariable(VDPVAR_MOUSE_DELTAY_OS);
		debug_log("sendMouseData: %d %d %d %d %d %d\n\r", mouseX, mouseY, buttons, wheelDelta, deltaX, deltaY);
		uint8_t packet[] = {
			(uint8_t) (mouseX & 0xFF),
			(uint8_t) ((mouseX >> 8) & 0xFF),
			(uint8_t) (mouseY & 0xFF),
			(uint8_t) ((mouseY >> 8) & 0xFF),
			(uint8_t) buttons,
			(uint8_t) wheelDelta,
			(uint8_t) (deltaX & 0xFF),
			(uint8_t) ((deltaX >> 8) & 0xFF),
			(uint8_t) (deltaY & 0xFF),
			(uint8_t) ((deltaY >> 8) & 0xFF),
		};
		send_packet(PACKET_MOUSE, sizeof packet, packet);
	}
	if (pendingKeyboardData) {
		bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_KEYCODE);
		pendingKeyboardData = false;
		// Create and send the packet back to MOS
		uint8_t packet[] = {
			uint8_t (getVDPVariable(VDPVAR_KEYEVENT_KEYCODE) & 0xFF),
			uint8_t (getVDPVariable(VDPVAR_KEYEVENT_MODIFIERS)),
			uint8_t (getVDPVariable(VDPVAR_KEYEVENT_VK) & 0xFF),
			uint8_t (getVDPVariable(VDPVAR_KEYEVENT_DOWN)),
		};
		send_packet(PACKET_KEYCODE, sizeof packet, packet);
	}
}

// Process all available commands from the stream
//
void VDUStreamProcessor::processAllAvailable() {
	while (byteAvailable()) {
		flushEcho();
		vdu(readByte());
	}
}

// Process next command from the stream
//
void VDUStreamProcessor::processNext() {
	if (getContext()->checkForVSYNC()) {
		bufferCallCallbacks(CALLBACK_VSYNC);
		if (!byteAvailable()) {
			getContext()->clearTempPagedMode();
			// Ensure output stream changes on main VDU processor are temporary
			if (outputStream != originalOutputStream) {
				outputStream = originalOutputStream;
			}
		}
	}

	handleKeyboardAndMouse();
	doCursorFlash();

	switch (getContext()->getProcessorState()) {
		case VDUProcessorState::Active:
			// process next byte, if available
			if (byteAvailable()) {
				hideCursor();
				flushEcho();
				vdu(readByte());
				if (!byteAvailable()) {
					showCursor();
				}
			}
			break;
		case VDUProcessorState::WaitingForFrames:
			// Do nothing - wait for frames to expire
			break;
		case VDUProcessorState::PagedModePaused:
			if (shiftKeyPressed()) {
				getContext()->setProcessorState(VDUProcessorState::Active);
			}
			break;
		case VDUProcessorState::CtrlShiftPaused:
			if (!shiftKeyPressed() || !ctrlKeyPressed()) {
				getContext()->setProcessorState(VDUProcessorState::Active);
			}
			break;
		default:
			break;
	}

	sendPendingKeyboardAndMouse();
}

inline void VDUStreamProcessor::pushEcho(uint8_t c) {
	if (echoBuffering) {
		echoBuffer.push_back(c);
	}
}

void VDUStreamProcessor::pushEcho(uint8_t * chars, uint32_t length) {
	if (echoBuffering) {
		for (uint32_t i = 0; i < length; i++) {
			echoBuffer.push_back(chars[i]);
		}
	}
}

inline void VDUStreamProcessor::clearEcho() {
	echoBuffering = false;
	echoBuffer.clear();
}

void VDUStreamProcessor::flushEcho() {
	echoBuffering = echoEnabled;

	if (!echoEnabled || echoBuffer.empty()) {
		return;
	}

	uint32_t bufferSize = getVDPVariable(TESTFLAG_VDPP_BUFFERSIZE);
	if (bufferSize == 0) {
		bufferSize = 16;
	}
	debug_log("Echo buffer size: %d\n\r", bufferSize);

	// Iterate over the echoBuffer, sending packets of max bufferSize bytes
	uint32_t remaining = echoBuffer.size();
	uint32_t offset = 0;
	while (remaining > 0) {
		bufferCallCallbacks(CALLBACK_SENDING_VDPP | PACKET_ECHO);
		uint32_t packetSize = remaining > bufferSize ? bufferSize : remaining;
		uint8_t packet[bufferSize];
		for (uint32_t i = 0; i < packetSize; i++) {
			packet[i] = echoBuffer[offset + i];
		}
		send_packet(PACKET_ECHO, packetSize, packet);
		debug_log("Echo %.*s\n\r", packetSize, packet);
		offset += packetSize;
		remaining -= packetSize;
	}

	// // send echo buffer as hex to debug log
	// debug_log("Echo: ");
	// for (auto c : echoBuffer) {
	// 	debug_log("%02X ", c);
	// }
	// debug_log("\n\r");
	
	echoBuffer.clear();
}

void VDUStreamProcessor::handleKeyboardAndMouse() {
	MouseDelta delta;
	fabgl::VirtualKeyItem kbItem;

	// Send any pending packets to MOS (possibly created by a callback)
	sendPendingKeyboardAndMouse();

	// Check for keyboard event
	while (getKeyboardKey(&kbItem)) {
		// Set system variables corresponding to the keyboard event
		setVDPVariable(VDPVAR_KEYEVENT_KEYCODE, _keycode | kbItem.ASCII << 8);
		setVDPVariable(VDPVAR_KEYEVENT_VK, kbItem.vk);
		setVDPVariable(VDPVAR_KEYEVENT_DOWN, kbItem.down);
		setVDPVariable(VDPVAR_KEYEVENT_MODIFIERS, packKeyboardModifiers(&kbItem));
		// Individual modifiers get automatically set
		setVDPVariable(VDPVAR_KEYEVENT_SCANCODE1, reinterpret_cast<const uint16_t*>(kbItem.scancode)[0]);
		setVDPVariable(VDPVAR_KEYEVENT_SCANCODE2, reinterpret_cast<const uint16_t*>(kbItem.scancode)[1]);
		setVDPVariable(VDPVAR_KEYEVENT_SCANCODE3, reinterpret_cast<const uint16_t*>(kbItem.scancode)[2]);
		setVDPVariable(VDPVAR_KEYEVENT_SCANCODE4, reinterpret_cast<const uint16_t*>(kbItem.scancode)[3]);

		// Perform callback for keyboard h/w event
		bufferCallCallbacks(CALLBACK_KEYBOARD);

		// Handle some control keys
		if (controlKeys && getVDPVariable(VDPVAR_KEYEVENT_DOWN)) {
			uint16_t keycode = getVDPVariable(VDPVAR_KEYEVENT_KEYCODE);
			uint8_t ascii = keycode >> 8;
			switch (ascii) {
				case 2:		// printer on
				case 3:		// printer off
				case 6:		// VDU commands enable
				case 7:		// Bell
				case 12:	// CLS
				case 14 ... 15:	// paged mode on/off
					vdu(ascii, false);
					break;
				case 16:
					// control-P toggles "printer" on R.T.Russell's BASIC
					printerOn = !printerOn;
			}
		}

		// Send any pending event packets to MOS
		sendPendingKeyboardAndMouse();
	}

	// has the mouse moved?
	if (mouseMoved(&delta)) {
		// Absolute position of mouse will have been updated from the mouse delta
		// Update all of our mouse variables, which will trigger a mouse event to be sent
		// when the current processNext() call completes
		updateMouseVars(&delta);
		// Perform mouse h/w event callback
		bufferCallCallbacks(CALLBACK_MOUSE);
		// Send any pending event packets to MOS
		sendPendingKeyboardAndMouse();
	}
}

void VDUStreamProcessor::updateMouseVars(MouseDelta *delta) {
	auto mStatus = getMouse()->status();
	setVDPVariable(VDPVAR_MOUSE_XPOS, mStatus.X);
	setVDPVariable(VDPVAR_MOUSE_YPOS, mStatus.Y);
	setVDPVariable(VDPVAR_MOUSE_BUTTONS, mStatus.buttons.left << 0 | mStatus.buttons.right << 1 | mStatus.buttons.middle << 2);
	setVDPVariable(VDPVAR_MOUSE_WHEEL, mStatus.wheelDelta);
	if (delta == nullptr) {
		setVDPVariable(VDPVAR_MOUSE_DELTAX, 0);
		setVDPVariable(VDPVAR_MOUSE_DELTAY, 0);
	} else {
		setVDPVariable(VDPVAR_MOUSE_DELTAX, delta->deltaX);
		setVDPVariable(VDPVAR_MOUSE_DELTAY, delta->deltaY);
	}
}

#include "vdu.h"

#endif // VDU_STREAM_PROCESSOR_H
