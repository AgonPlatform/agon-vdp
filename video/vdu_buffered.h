#ifndef VDU_BUFFERED_H
#define VDU_BUFFERED_H

#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>
#include <esp_heap_caps.h>
#include <mat.h>
#include <dspm_mult.h>
#include <fabutils.h>

#include "agon.h"
#include "agon_ps2.h"
#include "agon_fonts.h"
#include "buffers.h"
#include "buffer_stream.h"
#include "compression.h"
#include "mem_helpers.h"
#include "multi_buffer_stream.h"
#include "sprites.h"
#include "vdp_variables.h"
#include "types.h"
#include "vdu_stream_processor.h"

// VDU 23, 0, &A0, bufferId; command: Buffered command support
//
void IRAM_ATTR VDUStreamProcessor::vdu_sys_buffered() {
	auto bufferId = readWord_t(); if (bufferId == -1) return;
	auto command = readByte_t(); if (command == -1) return;

	switch (command) {
		case BUFFERED_WRITE: {
			auto length = readWord_t(); if (length == -1) return;
			bufferWrite(bufferId, length);
		}	break;
		case BUFFERED_CALL: {
			bufferCall(bufferId, {});
		}	break;
		case BUFFERED_CLEAR: {
			bufferClear(bufferId);
		}	break;
		case BUFFERED_CREATE: {
			auto size = readWord_t(); if (size == -1) return;
			auto buffer = bufferCreate(bufferId, size);	
			if (buffer) {
				// Ensure buffer is empty
				memset(buffer->getBuffer(), 0, size);
			}
		}	break;
		case BUFFERED_SET_OUTPUT: {
			setOutputStream(bufferId);
		}	break;
		case BUFFERED_ADJUST: {
			bufferAdjust(bufferId);
		}	break;
		case BUFFERED_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; 6, <conditional arguments>  : Conditional call
			if (bufferConditional()) {
				bufferCall(bufferId, {});
			}
		}	break;
		case BUFFERED_JUMP: {
			// VDU 23, 0, &A0, bufferId; 7: Jump to buffer
			// a "jump" (without an offset) to buffer 65535 (-1) indicates a "jump to end"
			AdvancedOffset offset = {};
			offset.blockIndex = bufferId == 65535 ? -1 : 0;
			bufferJump(bufferId, offset);
		}	break;
		case BUFFERED_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; 8, <conditional arguments>  : Conditional jump
			if (bufferConditional()) {
				// ensure offset-less jump to buffer 65535 (-1) is treated as a "jump to end"
				AdvancedOffset offset = {};
				offset.blockIndex = bufferId == 65535 ? -1 : 0;
				bufferJump(bufferId, offset);
			}
		}	break;
		case BUFFERED_OFFSET_JUMP: {
			// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte  : Offset jump
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			bufferJump(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; &0A, offset; offsetHighByte, <conditional arguments>  : Conditional jump with offset
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			if (bufferConditional()) {
				bufferJump(bufferId, offset);
			}
		}	break;
		case BUFFERED_OFFSET_CALL: {
			// VDU 23, 0, &A0, bufferId; &0B, offset; offsetHighByte  : Offset call
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			bufferCall(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; &0C, offset; offsetHighByte, <conditional arguments>  : Conditional call with offset
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			if (bufferConditional()) {
				bufferCall(bufferId, offset);
			}
		}	break;
		case BUFFERED_COPY: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopy(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_CONSOLIDATE: {
			bufferConsolidate(bufferId);
		}	break;
		case BUFFERED_SPLIT: {
			auto length = readWord_t(); if (length == -1) return;
			uint16_t target[] = { (uint16_t)bufferId };
			bufferSplitInto(bufferId, length, target, false);
		}	break;
		case BUFFERED_SPLIT_INTO: {
			auto length = readWord_t(); if (length == -1) return;
			auto targetBufferIds = getBufferIdsFromStream();
			if (targetBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSplitInto(bufferId, length, targetBufferIds, false);
		}	break;
		case BUFFERED_SPLIT_FROM: {
			auto length = readWord_t(); if (length == -1) return;
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSplitInto(bufferId, length, target, true);
		}	break;
		case BUFFERED_SPLIT_BY: {
			auto width = readWord_t(); if (width == -1) return;
			auto chunks = readWord_t(); if (chunks == -1) return;
			uint16_t target[] = { (uint16_t)bufferId };
			bufferSplitByInto(bufferId, width, chunks, target, false);
		}	break;
		case BUFFERED_SPLIT_BY_INTO: {
			auto width = readWord_t(); if (width == -1) return;
			auto targetBufferIds = getBufferIdsFromStream();
			auto chunks = targetBufferIds.size();
			if (chunks == 0) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSplitByInto(bufferId, width, chunks, targetBufferIds, false);
		}	break;
		case BUFFERED_SPLIT_BY_FROM: {
			auto width = readWord_t(); if (width == -1) return;
			auto chunks = readWord_t(); if (chunks == -1) return;
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSplitByInto(bufferId, width, chunks, target, true);
		}	break;
		case BUFFERED_SPREAD_INTO: {
			auto targetBufferIds = getBufferIdsFromStream();
			if (targetBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSpreadInto(bufferId, targetBufferIds, false);
		}	break;
		case BUFFERED_SPREAD_FROM: {
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSpreadInto(bufferId, target, true);
		}	break;
		case BUFFERED_REVERSE_BLOCKS: {
			bufferReverseBlocks(bufferId);
		}	break;
		case BUFFERED_REVERSE: {
			auto options = readByte_t(); if (options == -1) return;
			bufferReverse(bufferId, options);
		}	break;
		case BUFFERED_COPY_REF: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopyRef(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_COPY_AND_CONSOLIDATE: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopyAndConsolidate(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_AFFINE_TRANSFORM: if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
			auto operation = readByte_t(); if (operation == -1) return;
			bufferAffineTransform(bufferId, operation, false);
		}	break;
		case BUFFERED_AFFINE_TRANSFORM_3D: if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
			auto operation = readByte_t(); if (operation == -1) return;
			bufferAffineTransform(bufferId, operation, true);
		}	break;
		case BUFFERED_MATRIX: if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
			auto operation = readByte_t(); if (operation == -1) return;
			auto rows = readByte_t(); if (rows == -1) return;
			auto columns = readByte_t(); if (columns == -1) return;
			MatrixSize size;
			size.rows = rows;
			size.columns = columns;
			bufferMatrixManipulate(bufferId, operation, size);
		}	break;
		case BUFFERED_TRANSFORM_BITMAP: if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
			auto options = readByte_t();
			auto transformBufferId = readWord_t();
			auto bitmapId = readWord_t();
			if (options == -1 || transformBufferId == -1 || bitmapId == -1) {
				return;
			}
			bufferTransformBitmap(bufferId, options, transformBufferId, bitmapId);
		}	break;
		case BUFFERED_TRANSFORM_DATA: if (isVDPVariableSet(TESTFLAG_AFFINE_TRANSFORM)) {
			// VDU 23, 0, &A0, bufferId; &29, options, format, transformBufferId; sourceBufferId; : Apply transform matrix to data in a buffer
			auto options = readByte_t();
			auto format = readByte_t();
			auto transformBufferId = readWord_t();
			auto sourceBufferId = readWord_t();
			if (options == -1 || format == -1 || transformBufferId == -1 || sourceBufferId == -1) {
				return;
			}
			bufferTransformData(bufferId, options, format, transformBufferId, sourceBufferId);
		}	break;
		case BUFFERED_READ_VARIABLE: {
			// VDU 23, 0, &A0, bufferId; &30, flags, offset; variableId; [default[;]]
			bufferReadVariable(bufferId);
		}	break;
		case BUFFERED_COMPRESS: {
			auto sourceBufferId = readWord_t();
			if (sourceBufferId == -1) return;
			bufferCompress(bufferId, sourceBufferId);
		}	break;
		case BUFFERED_DECOMPRESS: {
			auto sourceBufferId = readWord_t();
			if (sourceBufferId == -1) return;
			bufferDecompress(bufferId, sourceBufferId);
		}	break;
		case BUFFERED_EXPAND_BITMAP: {
			auto options = readByte_t(); if (options == -1) return;
			auto sourceBufferId = readWord_t();
			if (sourceBufferId == -1) return;
			bufferExpandBitmap(bufferId, options, sourceBufferId);
		}	break;
		case BUFFERED_ADD_CALLBACK: {
			auto type = readWord_t(); if (type == -1) return;
			bufferAddCallback(bufferId, type);
		}	break;
		case BUFFERED_REMOVE_CALLBACK: {
			auto type = readWord_t(); if (type == -1) return;
			bufferRemoveCallback(bufferId, type);
		}	break;
		case BUFFERED_DEBUG_INFO: {
			// force_debug_log("vdu_sys_buffered: debug info stack highwater %d\n\r",uxTaskGetStackHighWaterMark(nullptr));
			force_debug_log("vdu_sys_buffered: buffer %d, %d streams stored\n\r", bufferId, buffers[bufferId].size());
			if (buffers[bufferId].empty()) {
				return;
			}
			auto matrixSize = getMatrixSize(bufferId);
			if (matrixSize.value != 0) {
				float transform[matrixSize.size()];
				if (getMatrixFromBuffer(bufferId, transform, matrixSize)) {
					force_debug_log("buffer contains a %d x %d matrix with contents:\n\r", matrixSize.rows, matrixSize.columns);
					for (int i = 0; i < matrixSize.rows; i++) {
						for (int j = 0; j < matrixSize.columns; j++) {
							force_debug_log(" %f", transform[i * matrixSize.columns + j]);
						}
						force_debug_log("\n\r");
					}
					return;
				}
			}
			// output contents of buffer stream 0
			auto buffer = buffers[bufferId][0];
			auto bufferLength = buffer->size();
			for (auto i = 0; i < bufferLength; i++) {
				auto data = buffer->getBuffer()[i];
				force_debug_log("%02X ", data);
			}
			force_debug_log("\n\r");
		}	break;
		default: {
			debug_log("vdu_sys_buffered: unknown command %d, buffer %d\n\r", command, bufferId);
		}	break;
	}
}

// VDU 23, 0, &A0, bufferId; 0, length; data...: store stream into buffer
// This adds a new stream to the given bufferId
// allowing a single bufferId to store multiple streams of data
//
uint32_t VDUStreamProcessor::bufferWrite(uint16_t bufferId, uint32_t length) {
	auto bufferStream = make_shared_psram<BufferStream>(length);

	debug_log("bufferWrite: storing stream into buffer %d, length %d\n\r", bufferId, length);

	auto remaining = readIntoBuffer(bufferStream->getBuffer(), length);
	if (remaining > 0) {
		// NB this discards the data we just read
		debug_log("bufferWrite: timed out write for buffer %d (%d bytes remaining)\n\r", bufferId, remaining);
		return remaining;
	}

	if (bufferId == 65535) {
		// buffer ID of -1 (65535) reserved so we don't store it
		debug_log("bufferWrite: ignoring buffer 65535\n\r");
		return remaining;
	}

	buffers[bufferId].push_back(std::move(bufferStream));
	debug_log("bufferWrite: stored stream in buffer %d, length %d, %d streams stored\n\r", bufferId, length, buffers[bufferId].size());
	return remaining;
}

// VDU 23, 0, &A0, bufferId; 1: Call buffer
// VDU 23, 0, &A0, bufferId; &0B, offset; offsetHighByte  : Offset call
// Processes all commands from the streams stored against the given bufferId
//
void VDUStreamProcessor::bufferCall(uint16_t callBufferId, AdvancedOffset offset) {
	debug_log("bufferCall: buffer %d\n\r", callBufferId);
	auto bufferId = resolveBufferId(callBufferId, id);
	if (bufferId == -1) {
		debug_log("bufferCall: no buffer ID\n\r");
		return;
	}
	AdvancedOffset returnOffset;
	if (id != 65535) {
		if (inputStream->available() == 0) {
			// tail-call optimise - turn the call into a jump
			bufferJump(bufferId, offset);
			return;
		}
		// get the return offset before doing any BufferStream operations
		auto multiBufferStream = (MultiBufferStream *)inputStream.get();
		multiBufferStream->tellBuffer(returnOffset.blockOffset, returnOffset.blockIndex);
		if (id == bufferId) {
			// calling ourselves, just seek to the old offset after returning
			multiBufferStream->seekTo(offset.blockOffset, offset.blockIndex);
			processAllAvailable();
			multiBufferStream->seekTo(returnOffset.blockOffset, returnOffset.blockIndex);
			return;
		}
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferCall: buffer %d not found\n\r", bufferId);
		bufferRemoveCallback(bufferId, 65535);
		return;
	}
	auto &streams = bufferIter->second;
	std::shared_ptr<Stream> callInputStream = make_shared_psram<MultiBufferStream>(streams);
	if (offset.blockOffset != 0 || offset.blockIndex != 0) {
		auto multiBufferStream = (MultiBufferStream *)callInputStream.get();
		multiBufferStream->seekTo(offset.blockOffset, offset.blockIndex);
	}
	// Track our output streams so we can restore them after the call
	auto currentOutputStream = outputStream;
	auto currentOriginalOutputStream = originalOutputStream;
	// update originalOutputStream so it is correct for the context of the call
	originalOutputStream = outputStream;
	// using the current VDUStreamProcessor, swap in our new input stream
	std::swap(id, callBufferId);
	std::swap(inputStream, callInputStream);
	processAllAvailable();
	// restore the original buffer id and streams
	id = callBufferId;
	inputStream = std::move(callInputStream);
	outputStream = std::move(currentOutputStream);
	originalOutputStream = std::move(currentOriginalOutputStream);
	if (id != 65535) {
		// return to the appropriate offset
		auto multiBufferStream = (MultiBufferStream *)inputStream.get();
		multiBufferStream->seekTo(returnOffset.blockOffset, returnOffset.blockIndex);
	}
}

void VDUStreamProcessor::bufferRemoveUsers(uint16_t bufferId) {
	// remove all users of the given buffer
	context->unmapBitmapFromChars(bufferId);
	clearBitmap(bufferId);
	clearFont(bufferId);
	clearSample(bufferId);
	clearMouseCursor(bufferId);
}

// VDU 23, 0, &A0, bufferId; 2: Clear buffer
// Removes all streams stored against the given bufferId
// sending a bufferId of 65535 (i.e. -1) clears all buffers
//
void VDUStreamProcessor::bufferClear(uint16_t bufferId) {
	debug_log("bufferClear: buffer %d\n\r", bufferId);
	if (bufferId == 65535) {
		buffers.clear();
		matrixMetadata.clear();
		resetMouseCursors();
		resetBitmaps();
		// TODO reset current bitmaps in all processors
		context->setCurrentBitmap(BUFFERED_BITMAP_BASEID);
		context->resetCharToBitmap();
		resetFonts();
		resetSamples();
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferClear: buffer %d not found\n\r", bufferId);
		return;
	}
	bufferRemoveUsers(bufferId);
	buffers.erase(bufferIter);
	matrixMetadata.erase(bufferId);
	debug_log("bufferClear: cleared buffer %d\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; 3, size; : Create a writeable buffer
// This is used for creating buffers to redirect output to
//
std::shared_ptr<WritableBufferStream> VDUStreamProcessor::bufferCreate(uint16_t bufferId, uint32_t size) {
	if (bufferId == 65535) {
		debug_log("bufferCreate: bufferId %d is reserved\n\r", bufferId);
		return nullptr;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		debug_log("bufferCreate: buffer %d already exists\n\r", bufferId);
		return nullptr;
	}
	auto buffer = make_shared_psram<WritableBufferStream>(size);
	if (!buffer) {
		debug_log("bufferCreate: failed to create buffer %d\n\r", bufferId);
		return nullptr;
	}
	buffers[bufferId].push_back(buffer);
	debug_log("bufferCreate: created buffer %d, size %d\n\r", bufferId, size);
	return buffer;
}

// VDU 23, 0, &A0, bufferId; 4: Set output to buffer
// use an ID of -1 (65535) to clear the output buffer (no output)
// use an ID of 0 to reset the output buffer to it's original value
//
// TODO add a variant/command to adjust offset inside output stream
void VDUStreamProcessor::setOutputStream(uint16_t bufferId) {
	if (bufferId == 65535) {
		outputStream = nullptr;
		return;
	}
	// bufferId of 0 resets output buffer to it's original value
	// which will usually be the z80 serial port
	if (bufferId == 0) {
		outputStream = originalOutputStream;
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end() || bufferIter->second.empty()) {
		debug_log("setOutputStream: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &output = bufferIter->second.front();
	if (output->isWritable()) {
		outputStream = output;
	} else {
		debug_log("setOutputStream: buffer %d is not writable\n\r", bufferId);
	}
}

// Utility call to read offset from stream, supporting advanced offsets
AdvancedOffset VDUStreamProcessor::getOffsetFromStream(bool isAdvanced) {
	AdvancedOffset offset = {};
	if (isAdvanced) {
		offset.blockOffset = read24_t();
		if (offset.blockOffset != -1 && offset.blockOffset & 0x00800000) {
			// top bit of 24-bit offset is set, so we have a block index too
			auto blockIndex = readWord_t();
			if (blockIndex == -1) {
				offset.blockOffset = -1;
			} else {
				offset.blockOffset &= 0x007FFFFF;
				offset.blockIndex = blockIndex;
			}
		}
	} else {
		offset.blockOffset = readWord_t();
	}
	return offset;
}

// Utility call to read a sequence of buffer IDs from the stream
std::vector<uint16_t> VDUStreamProcessor::getBufferIdsFromStream() {
	// read buffer IDs until we get a 65535 (end of list) or a timeout
	std::vector<uint16_t> bufferIds;
	uint32_t bufferId;
	bool looping = true;
	while (looping) {
		bufferId = readWord_t();
		looping = (bufferId != 65535 && bufferId != -1);
		if (looping) {
			bufferIds.push_back(bufferId);
		}
	}
	if (bufferId == -1) {
		// timeout
		bufferIds.clear();
	}

	return bufferIds;
}

// Utility classes for specializing buffer adjust operations
// AdjustSingle must be specialized for every operation type
// The other classes provide default implementations using AdjustSingle,
// and may be specialized for specific operation types.
template<uint8_t Operator>
struct AdjustSingle {
	// Default implementation for unimplemented operations
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t, bool &) {
		return target;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t, bool &) {
		return target;
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t, bool&) {
		return target;
	}
};
template<>
struct AdjustSingle<ADJUST_NOT> {
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t, bool &) {
		return ~target;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t, bool &) {
		return ~target;
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t, bool &) {
		return ~target;
	}
};
template<>
struct AdjustSingle<ADJUST_NEG> {
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t, bool &) {
		return -target;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t, bool &) {
		static constexpr uint_fast16_t CARRY_MASK = 0x100;
		uint_fast16_t result = -target;
		return result + ((result ^ target) & CARRY_MASK);
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t, bool &) {
		static constexpr uint32_t SIGN_MASK = 0x7F7F7F7F;
		return ((target & SIGN_MASK) + SIGN_MASK) ^ (target | SIGN_MASK);
	}
};
template<>
struct AdjustSingle<ADJUST_SET> {
	static inline uint_fast8_t adjust(uint_fast8_t, uint_fast8_t operand, bool &) {
		return operand;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t, uint_fast16_t operand, bool &) {
		return operand;
	}
	static inline uint32_t adjustWord(uint32_t, uint32_t operand, bool &) {
		return operand;
	}
};
template<>
struct AdjustSingle<ADJUST_ADD> {
	// byte-wise add - no carry, so bytes may overflow
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t operand, bool &) {
		return target + operand;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t operand, bool &) {
		static constexpr uint_fast16_t CARRY_MASK = 0x100;
		uint_fast16_t result = target + operand;
		return result - ((result ^ target ^ operand) & CARRY_MASK);
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t operand, bool &) {
		static constexpr uint32_t SIGN_MASK = 0x7F7F7F7F;
		return ((target & SIGN_MASK) + (operand & SIGN_MASK)) ^ ((target ^ operand) & ~SIGN_MASK);
	}
	static inline uint_fast8_t fold(uint32_t accumulator) {
		static constexpr uint32_t BYTE_MASK = 0x00FF00FF;
		accumulator = (accumulator & BYTE_MASK) + ((accumulator >> 8) & BYTE_MASK);
		return accumulator + (accumulator >> 16);
	}
};
template<>
struct AdjustSingle<ADJUST_ADD_CARRY> {
	// byte-wise add with carry
	// bytes are treated as being in little-endian order
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t operand, bool &carry) {
		uint_fast16_t sum = (uint_fast16_t)(uint8_t)target + (uint8_t)operand + carry;
		carry = sum >> 8;
		return sum;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t operand, bool &carry) {
		// convert from little-endian to native and back
		uint32_t sum = (uint32_t)from_le16(target) + from_le16(operand) + carry;
		carry = sum >> 16;
		return to_le16(sum);
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t operand, bool &carry) {
		// convert from little-endian to native and back
		target = from_le32(target);
		operand = from_le32(operand);
#if defined(__XTENSA__) && XCHAL_HAVE_MINMAX
		operand += carry;
		target += operand;
		// Do branchless carry handling
		uint32_t min; //= std::min(target, operand);
		// Use assembly so the compiler can't reverse the min operation back into a branchy less-than
		asm("minu\t%0, %1, %2"
		  : "=r" (min)
		  : "r" (target), "r" (operand));
		// The following should compile to conditional moves
		// Intentionally preserves input carry if carry-adjusted operand is zero
		if (operand != 0) carry = false; // Initialize carry to false if carry-adjusted operand is non-zero
		if (min != operand) carry = true; // Set carry to true if sum < carry-adjusted operand
#elif __has_builtin(__builtin_addc)
		unsigned int carry_out;
		target = __builtin_addc(target, operand, carry, &carry_out);
		carry = carry_out;
#else
		uint64_t sum = (uint64_t)target + operand + carry;
		carry = sum >> 32;
		target = sum;
#endif
		return to_le32(target);
	}
};
template<>
struct AdjustSingle<ADJUST_AND> {
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t operand, bool &) {
		return target & operand;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t operand, bool &) {
		return target & operand;
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t operand, bool &) {
		return target & operand;
	}
	static inline uint_fast8_t fold(uint32_t accumulator) {
		accumulator &= accumulator >> 16;
		accumulator &= accumulator >> 8;
		return accumulator;
	}
};
template<>
struct AdjustSingle<ADJUST_OR> {
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t operand, bool &) {
		return target | operand;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t operand, bool &) {
		return target | operand;
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t operand, bool &) {
		return target | operand;
	}
	static inline uint_fast8_t fold(uint32_t accumulator) {
		accumulator |= accumulator >> 16;
		accumulator |= accumulator >> 8;
		return accumulator;
	}
};
template<>
struct AdjustSingle<ADJUST_XOR> {
	static inline uint_fast8_t adjust(uint_fast8_t target, uint_fast8_t operand, bool &) {
		return target ^ operand;
	}
	static inline uint_fast16_t adjustHalfWord(uint_fast16_t target, uint_fast16_t operand, bool &) {
		return target ^ operand;
	}
	static inline uint32_t adjustWord(uint32_t target, uint32_t operand, bool &) {
		return target ^ operand;
	}
	static inline uint_fast8_t fold(uint32_t accumulator) {
		accumulator ^= accumulator >> 16;
		accumulator ^= accumulator >> 8;
		return accumulator;
	}
};
template<uint8_t Operator>
struct AdjustMultiSingle {
	// Input operand is duplicated into all 4 bytes
	static void adjust(uint8_t * target, uint32_t operand, bool &carry, size_t count) {
		bool localCarry = carry;
		if (count >= sizeof(uint32_t)) {
			if (reinterpret_cast<uintptr_t>(target) & 1) {
				*target = AdjustSingle<Operator>::adjust(*target, operand, localCarry);
				target++, count--;
			}
			if (reinterpret_cast<uintptr_t>(target) & 2) {
				write16_aligned(target, AdjustSingle<Operator>::adjustHalfWord(read16_aligned(target), operand, localCarry));
				target += sizeof(uint16_t), count -= sizeof(uint16_t);
			}
			while (count >= sizeof(uint32_t)) {
				write32_aligned(target, AdjustSingle<Operator>::adjustWord(read32_aligned(target), operand, localCarry));
				target += sizeof(uint32_t), count -= sizeof(uint32_t);
			}
		}
		if (count & 2) {
			write16_unaligned(target, AdjustSingle<Operator>::adjustHalfWord(read16_unaligned(target), operand, localCarry));
			target += sizeof(uint16_t);
		}
		if (count & 1) {
			*target = AdjustSingle<Operator>::adjust(*target, operand, localCarry);
		}
		if (Operator == ADJUST_ADD_CARRY) {
			carry = localCarry;
		}
	}
};
template<uint8_t Operator, typename = void>
struct AdjustSingleMulti {
	static uint_fast8_t adjust(uint_fast8_t target, uint8_t * operand, bool &carry, size_t count) {
		bool localCarry = carry;
		for (size_t i = 0; i < count; i++) {
			target = AdjustSingle<Operator>::adjust(target, operand[i], localCarry);
		}
		if (Operator == ADJUST_ADD_CARRY) {
			carry = localCarry;
		}
		return target;
	}
};
// Specialization if a fold operation is provided
template<uint8_t Operator>
struct AdjustSingleMulti<Operator, decltype(void(AdjustSingle<Operator>::fold))> {
	static uint_fast8_t adjust(uint_fast8_t target, uint8_t * operand, bool &carry, size_t count) {
		bool localCarry = carry;
		if (count >= sizeof(uint32_t)) {
			if (reinterpret_cast<uintptr_t>(operand) & 1) {
				target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
				operand++, count--;
			}
			if (reinterpret_cast<uintptr_t>(operand) & 2) {
				target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
				operand++, count--;
				target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
				operand++, count--;
			}
			if (count >= sizeof(uint32_t)) {
				uint32_t accumulator = read32_aligned(operand);
				operand += sizeof(uint32_t), count -= sizeof(uint32_t);
				while (count >= sizeof(uint32_t)) {
					accumulator = AdjustSingle<Operator>::adjustWord(accumulator, read32_aligned(operand), localCarry);
					operand += sizeof(uint32_t), count -= sizeof(uint32_t);
				}
				target = AdjustSingle<Operator>::adjust(target, AdjustSingle<Operator>::fold(accumulator), localCarry);
			}
		}
		if (count & 2) {
			target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
			operand++;
			target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
			operand++;
		}
		if (count & 1) {
			target = AdjustSingle<Operator>::adjust(target, *operand, localCarry);
		}
		if (Operator == ADJUST_ADD_CARRY) {
			carry = localCarry;
		}
		return target;
	}
};
template<uint8_t Operator>
struct AdjustMulti {
	static void adjust(uint8_t * target, uint8_t * operand, bool &carry, size_t count, bool sameBuffer) {
		bool localCarry = carry;
		if (count >= sizeof(uint32_t) && (!sameBuffer || target <= operand || target >= operand + sizeof(uint32_t))) {
			if (reinterpret_cast<uintptr_t>(target) & 1) {
				*target = AdjustSingle<Operator>::adjust(*target, *operand, localCarry);
				target++, operand++, count--;
			}
			if (reinterpret_cast<uintptr_t>(target) & 2) {
				write16_aligned(target, AdjustSingle<Operator>::adjustHalfWord(read16_aligned(target), read16_unaligned(operand), localCarry));
				target += sizeof(uint16_t), operand += sizeof(uint16_t), count -= sizeof(uint16_t);
			}
			while (count >= sizeof(uint32_t)) {
				write32_aligned(target, AdjustSingle<Operator>::adjustWord(read32_aligned(target), read32_unaligned(operand), localCarry));
				target += sizeof(uint32_t), operand += sizeof(uint32_t), count -= sizeof(uint32_t);
			}
		}
		// Target pointer may be immediately ahead of operand pointer, so simply loop bytes
		for (size_t i = 0; i < count; i++) {
			target[i] = AdjustSingle<Operator>::adjust(target[i], operand[i], localCarry);
		}
		if (Operator == ADJUST_ADD_CARRY) {
			carry = localCarry;
		}
	}
};

// Automatically generates a function table for all operators. Would be easier with C++14
template <template<uint8_t, typename...> typename T>
class AdjustFuncTable {
	using FuncPtr = decltype(T<ADJUST_NOT>::adjust)*;
	std::array<FuncPtr, ADJUST_OP_MASK+1> table;

	template<uint8_t... Operator>
	struct operator_sequence {};
	template<size_t N, uint8_t... Operator>
	struct make_operator_sequence : make_operator_sequence<N-1, N-1, Operator...> {};
	template<uint8_t... Operator>
	struct make_operator_sequence<0, Operator...> : operator_sequence<Operator...> {};

	template<uint8_t... Operator>
	constexpr AdjustFuncTable(operator_sequence<Operator...>)
		: table({{ &T<Operator>::adjust... }})
	{
	}

public:
	constexpr AdjustFuncTable()
		: AdjustFuncTable(make_operator_sequence<ADJUST_OP_MASK+1>{})
	{
	}

	constexpr inline FuncPtr operator[](uint8_t op) const {
		return table[op];
	}
};

// VDU 23, 0, &A0, bufferId; 5, operation, offset; [count;] [operand]: Adjust buffer
// This is used for adjusting the contents of a buffer
// It can be used to overwrite bytes, insert bytes, increment bytes, etc
// Basic operation are not, neg, set, add, add-with-carry, and, or, xor
// Upper bits of operation byte are used to indicate:
// - whether to use a long offset (24-bit) or short offset (16-bit)
// - whether the operand is a buffer-originated value or an immediate value
// - whether to adjust a single target or multiple targets
// - whether to use a single operand or multiple operands
//
void VDUStreamProcessor::bufferAdjust(uint16_t adjustBufferId) {
	static constexpr AdjustFuncTable<AdjustSingle> adjustSingleFuncs;
	static constexpr AdjustFuncTable<AdjustMultiSingle> adjustMultiSingleFuncs;
	static constexpr AdjustFuncTable<AdjustSingleMulti> adjustSingleMultiFuncs;
	static constexpr AdjustFuncTable<AdjustMulti> adjustMultiFuncs;

	const auto command = readByte_t();

	const bool useAdvancedOffsets = command & ADJUST_ADVANCED_OFFSETS;
	const bool useBufferValue = command & ADJUST_BUFFER_VALUE;
	const bool useMultiTarget = command & ADJUST_MULTI_TARGET;
	const bool useMultiOperand = command & ADJUST_MULTI_OPERAND;
	const uint8_t op = command & ADJUST_OP_MASK;
	// Operators that are greater than NEG have an operand value
	const bool hasOperand = op > ADJUST_NEG;

	auto offset = getOffsetFromStream(useAdvancedOffsets);
	const BufferVector * operandBuffer = nullptr;
	auto operandBufferId = 0;
	AdvancedOffset operandOffset = {};
	auto count = 1;

	if (useMultiTarget || useMultiOperand) {
		count = useAdvancedOffsets ? read24_t() : readWord_t();
	}
	if (useBufferValue && hasOperand) {
		operandBufferId = resolveBufferId(readWord_t(), id);
		operandOffset = getOffsetFromStream(useAdvancedOffsets);
		if (operandBufferId == -1) {
			debug_log("bufferAdjust: no operand buffer ID\n\r");
			return;
		}
		auto operandBufferIter = buffers.find(operandBufferId);
		if (operandBufferIter == buffers.end()) {
			debug_log("bufferAdjust: buffer %d not found\n\r", operandBufferId);
			return;
		}
		operandBuffer = &operandBufferIter->second;
	}

	auto bufferId = resolveBufferId(adjustBufferId, id);
	if (bufferId == -1) {
		debug_log("bufferAdjust: no target buffer ID\n\r");
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferAdjust: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;

	if (command == -1 || count == -1 || offset.blockOffset == -1 || operandOffset.blockOffset == -1) {
		debug_log("bufferAdjust: invalid command, count, offset or operand value\n\r");
		return;
	}

	MultiBufferStream * instream = nullptr;
	tcb::span<uint8_t> targetSpan;
	uint_fast8_t sourceValue = 0;
	auto operandValue = 0;
	bool carryValue = false;

	// if useMultiTarget is set, the we're updating multiple source values
	// if useMultiOperand is also set, we get multiple operand values
	// so...
	// if both useMultiTarget and useMultiOperand are false we're updating a single source value with a single operand
	// if useMultiTarget is false and useMultiOperand is true we're adding all operand values to the same source value
	// if useMultiTarget is true and useMultiOperand is false we're adding the same operand to all source values
	// if both useMultiTarget and useMultiOperand are true we're adding each operand value to the corresponding source value

	if (hasOperand) {
		if (!useMultiOperand) {
			// we have a singular operand value
			operandValue = operandBuffer ? getBufferByte(*operandBuffer, operandOffset) : readByte_t();
			if (operandValue == -1) {
				debug_log("bufferAdjust: invalid operand value\n\r");
				return;
			}
		} else if (!useBufferValue && id != 65535) {
			// multiple inline operands, get the underlying buffer if executing from one
			instream = (MultiBufferStream *)inputStream.get();
			operandBuffer = &instream->tellBuffer(operandOffset.blockOffset, operandOffset.blockIndex);
		}
	}
	if (!useMultiTarget) {
		// we have a singular target value
		targetSpan = getBufferSpan(buffer, offset);
		if (targetSpan.empty()) {
			debug_log("bufferAdjust: invalid target offset\n\r");
			return;
		}
		sourceValue = targetSpan.front();
	}

	debug_log("bufferAdjust: command %d, offset %d:%d, count %d, operandBufferId %d, operandOffset %d:%d, sourceValue %d, operandValue %d\n\r",
		command, (int)offset.blockIndex, offset.blockOffset, count, operandBufferId, (int)operandOffset.blockIndex, operandOffset.blockOffset, sourceValue, operandValue);
	debug_log("useMultiTarget %d, useMultiOperand %d, useAdvancedOffsets %d, useBufferValue %d\n\r", useMultiTarget, useMultiOperand, useAdvancedOffsets, useBufferValue);

	if (!useMultiTarget) {
		if (!hasOperand || !useMultiOperand) {
			auto func = adjustSingleFuncs[op];
			sourceValue = func(sourceValue, operandValue, carryValue);
		} else if (operandBuffer) {
			auto func = adjustSingleMultiFuncs[op];
			while (count > 0) {
				auto operandSpan = getBufferSpan(*operandBuffer, operandOffset);
				auto iterCount = std::min<size_t>(operandSpan.size(), count);
				if (iterCount == 0) {
					debug_log("bufferAdjust: operand buffer overflow\n\r");
					if (instream) {
						instream->seekTo(operandOffset.blockOffset, operandOffset.blockIndex);
					}
					return;
				}
				sourceValue = func(sourceValue, operandSpan.data(), carryValue, iterCount);
				operandOffset.blockOffset += iterCount;
				count -= iterCount;
			}
			if (instream) {
				instream->seekTo(operandOffset.blockOffset, operandOffset.blockIndex);
			}
		} else {
			auto func = adjustSingleFuncs[op];
			while (count > 0) {
				operandValue = readByte_t();
				if (operandValue == -1) {
					debug_log("bufferAdjust: operand timeout\n\r");
					return;
				}
				sourceValue = func(sourceValue, operandValue, carryValue);
				count--;
			}
		}
		debug_log("bufferAdjust: result %d\n\r", sourceValue);
		targetSpan.front() = sourceValue;
		// increment offset in case carry is used
		offset.blockOffset++;
	} else {
		if (!hasOperand || !useMultiOperand) {
			auto func = adjustMultiSingleFuncs[op];
			auto operandWord = (uint8_t)operandValue * (uint32_t)0x01010101;
			while (count > 0) {
				targetSpan = getBufferSpan(buffer, offset);
				auto iterCount = std::min<size_t>(targetSpan.size(), count);
				if (iterCount == 0) {
					debug_log("bufferAdjust: target buffer overflow\n\r");
					return;
				}
				func(targetSpan.data(), operandWord, carryValue, iterCount);
				offset.blockOffset += iterCount;
				count -= iterCount;
			}
		} else if (operandBuffer) {
			auto func = adjustMultiFuncs[op];
			while (count > 0) {
				targetSpan = getBufferSpan(buffer, offset);
				auto operandSpan = getBufferSpan(*operandBuffer, operandOffset);
				auto iterCount = std::min<size_t>(std::min(targetSpan.size(), operandSpan.size()), count);
				if (iterCount == 0) {
					debug_log("bufferAdjust: target or operand buffer overflow\n\r");
					if (instream) {
						instream->seekTo(operandOffset.blockOffset, operandOffset.blockIndex);
					}
					return;
				}
				bool sameBuffer = buffer[offset.blockIndex] == (*operandBuffer)[operandOffset.blockIndex];
				func(targetSpan.data(), operandSpan.data(), carryValue, iterCount, sameBuffer);
				offset.blockOffset += iterCount;
				operandOffset.blockOffset += iterCount;
				count -= iterCount;
			}
			if (instream) {
				instream->seekTo(operandOffset.blockOffset, operandOffset.blockIndex);
			}
		} else {
			auto func = adjustSingleFuncs[op];
			while (count > 0) {
				targetSpan = getBufferSpan(buffer, offset);
				auto iterCount = std::min<size_t>(targetSpan.size(), count);
				if (iterCount == 0) {
					debug_log("bufferAdjust: target buffer overflow\n\r");
					return;
				}
				for (size_t i = 0; i < iterCount; i++) {
					operandValue = readByte_t();
					if (operandValue == -1) {
						debug_log("bufferAdjust: operand timeout\n\r");
						return;
					}
					targetSpan[i] = func(targetSpan[i], operandValue, carryValue);
				}
				offset.blockOffset += iterCount;
				count -= iterCount;
			}
		}
	}

	if (op == ADJUST_ADD_CARRY) {
		// if we were using carry, store the final carry value
		if (!setBufferByte(carryValue, buffer, offset)) {
			debug_log("bufferAdjust: failed to set carry value %d at offset %d:%d\n\r", carryValue, (int)offset.blockIndex, offset.blockOffset);
			return;
		}
	}
}

// returns true or false depending on whether conditions are met
// Will read the following arguments from the stream
// operation, checkBufferId; offset; [operand]
// This works in a similar manner to bufferAdjust
// for now, this only supports single-byte comparisons
// as multi-byte comparisons are a bit more complex
//
bool VDUStreamProcessor::bufferConditional() {
	auto command = readByte_t();
	if (command == -1) {
		debug_log("bufferConditional: invalid command\n\r");
		return false;
	}

	bool useAdvancedOffsets = command & COND_ADVANCED_OFFSETS;
	bool useBufferValue = command & COND_BUFFER_VALUE;	// Operand is a buffer value
	bool useVariableValue = command & COND_VAR_VALUE;	// source to check is a feature flag
	bool use16BitValue = command & COND_16BIT;	// source and operand are 16-bit values

	auto checkBufferId = useVariableValue ? readWord_t() : resolveBufferId(readWord_t(), id);
	
	uint8_t op = command & COND_OP_MASK;
	// conditional operators that are greater than NOT_EXISTS require an operand
	bool hasOperand = op > COND_NOT_EXISTS;

	AdvancedOffset offset = {};
	if (!useVariableValue) {
		offset = getOffsetFromStream(useAdvancedOffsets);
	}

	auto operandBufferId = 0;
	AdvancedOffset operandOffset = {};
	if (useBufferValue && hasOperand) {
		operandBufferId = resolveBufferId(readWord_t(), id);
		operandOffset = getOffsetFromStream(useAdvancedOffsets);
	}

	if (checkBufferId == -1 || offset.blockOffset == -1 || operandOffset.blockOffset == -1) {
		debug_log("bufferConditional: invalid command, checkBufferId, offset or operand value\n\r");
		return false;
	}

	int32_t sourceValue = -1;
	if (useVariableValue) {
		if (isVDPVariableSet(checkBufferId)) {
			sourceValue = getVDPVariable(checkBufferId);
			if (!use16BitValue) {
				sourceValue &= 0xFF;
			}
		}
	} else {
		readBufferBytes(checkBufferId, offset, &sourceValue, use16BitValue ? 2 : 1);
	}

	int32_t operandValue = hasOperand ? -1 : 0;
	if (hasOperand) {
		if (useBufferValue) {
			readBufferBytes(operandBufferId, operandOffset, &operandValue, use16BitValue ? 2 : 1);
		} else {
			operandValue = use16BitValue ? readWord_t() : readByte_t();
		}
	}

	debug_log("bufferConditional: command %d, checkBufferId %d, offset %d:%d, operandBufferId %d, operandOffset %d:%d, sourceValue %d, operandValue %d\n\r",
		command, checkBufferId, (int)offset.blockIndex, offset.blockOffset, operandBufferId, (int)operandOffset.blockIndex, operandOffset.blockOffset, sourceValue, operandValue);

	if (useVariableValue && op <= COND_NOT_EXISTS) {	// Flag existence is a pure check, not check for zero
		if (op == COND_NOT_EXISTS) {
			return sourceValue == -1;
		}
		return sourceValue != -1;
	}

	if (sourceValue == -1 || operandValue == -1) {
		debug_log("bufferConditional: invalid source or operand value\n\r");
		return false;
	}

	bool shouldCall = false;

	switch (op) {
		case COND_EXISTS: {
			shouldCall = sourceValue != 0;
		}	break;
		case COND_NOT_EXISTS: {
			shouldCall = sourceValue == 0;
		}	break;
		case COND_EQUAL: {
			shouldCall = sourceValue == operandValue;
		}	break;
		case COND_NOT_EQUAL: {
			shouldCall = sourceValue != operandValue;
		}	break;
		case COND_LESS: {
			shouldCall = sourceValue < operandValue;
		}	break;
		case COND_GREATER: {
			shouldCall = sourceValue > operandValue;
		}	break;
		case COND_LESS_EQUAL: {
			shouldCall = sourceValue <= operandValue;
		}	break;
		case COND_GREATER_EQUAL: {
			shouldCall = sourceValue >= operandValue;
		}	break;
		case COND_AND: {
			shouldCall = sourceValue && operandValue;
		}	break;
		case COND_OR: {
			shouldCall = sourceValue || operandValue;
		}	break;
	}

	debug_log("bufferConditional: evaluated as %s\n\r", shouldCall ? "true" : "false");

	return shouldCall;
}

// VDU 23, 0, &A0, bufferId; 7: Jump to a buffer
// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte : Jump to (advanced) offset within buffer
// Change execution to given buffer (from beginning or at an offset)
//
void VDUStreamProcessor::bufferJump(uint16_t bufferId, AdvancedOffset offset) {
	debug_log("bufferJump: buffer %d\n\r", bufferId);
	if (id == 65535) {
		// we're currently the top-level stream, so we can't jump
		// so have to call instead
		return bufferCall(bufferId, offset);
	}
	if (bufferId == 65535 || bufferId == id) {
		// a buffer ID of 65535 is used to indicate current buffer, so we seek to offset
		auto instream = (MultiBufferStream *)inputStream.get();
		instream->seekTo(offset.blockOffset, offset.blockIndex);
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferJump: buffer %d not found\n\r", bufferId);
		bufferRemoveCallback(bufferId, 65535);
		return;
	}
	// replace our input stream with a new one
	auto &streams = bufferIter->second;
	auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
	if (offset.blockOffset != 0 || offset.blockIndex != 0) {
		multiBufferStream->seekTo(offset.blockOffset, offset.blockIndex);
	}
	id = bufferId;
	inputStream = std::move(multiBufferStream);
}

// VDU 23, 0, &A0, bufferId; &0D, sourceBufferId; sourceBufferId; ...; 65535; : Copy blocks from buffers
// Copy (blocks from) a list of buffers into a new buffer
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one
// This is useful to construct a single buffer from multiple buffers
// which can be used to construct more complex commands
// Target buffer ID can be included in the source list
//
void VDUStreamProcessor::bufferCopy(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopy: ignoring buffer %d\n\r", bufferId);
		return;
	}
	// prepare a vector for storing our buffers
	std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>> streams;
	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			// loop thru blocks stored against this ID
			for (const auto &block : sourceBufferIter->second) {
				// push a copy of the block into our vector
				auto bufferStream = make_shared_psram<BufferStream>(block->size());
				if (!bufferStream || !bufferStream->getBuffer()) {
					debug_log("bufferCopy: failed to create buffer\n\r");
					return;
				}
				debug_log("bufferCopy: copying stream %d bytes\n\r", block->size());
				bufferStream->writeBuffer(block->getBuffer(), block->size());
				streams.push_back(std::move(bufferStream));
			}
		} else {
			debug_log("bufferCopy: buffer %d not found\n\r", sourceId);
		}
	}
	// replace buffer with new one
	bufferRemoveUsers(bufferId);
	buffers[bufferId].assign(std::make_move_iterator(streams.begin()), std::make_move_iterator(streams.end()));
	debug_log("bufferCopy: copied %d streams into buffer %d (%d)\n\r", streams.size(), bufferId, buffers[bufferId].size());
}

// VDU 23, 0, &A0, bufferId; &0E : Consolidate blocks within buffer
// Consolidate multiple streams/blocks into a single block
// This is useful for using bitmaps sent in multiple blocks
//
void VDUStreamProcessor::bufferConsolidate(uint16_t bufferId) {
	// Create a new stream big enough to contain all streams in the given buffer
	// Copy all streams into the new stream
	// Replace the given buffer with the new stream
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferConsolidate: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	if (buffer.size() == 1) {
		// only one stream, so nothing to consolidate
		return;
	}
	// buffer ID exists
	auto bufferStream = consolidateBuffers(buffer);
	if (!bufferStream) {
		debug_log("bufferConsolidate: failed to create buffer\n\r");
		return;
	}
	bufferRemoveUsers(bufferId);
	buffer.clear();
	buffer.push_back(std::move(bufferStream));
	debug_log("bufferConsolidate: consolidated %d streams into buffer %d\n\r", buffer.size(), bufferId);
}

void VDUStreamProcessor::clearTargets(tcb::span<const uint16_t> targets) {
	for (const auto target : targets) {
		bufferClear(target);
	}
}

// VDU 23, 0, &A0, bufferId; &0F, length; : Split buffer into blocks by length
// VDU 23, 0, &A0, bufferId; &10, length; <bufferIds>; 65535; : Split buffer by length to new buffers
// VDU 23, 0, &A0, bufferId; &11, length; targetBufferId; : Split buffer by length to new buffers from target onwards
// Split a buffer into multiple blocks/streams to new buffers
// Will overwrite any existing buffers
//
void VDUStreamProcessor::bufferSplitInto(uint16_t bufferId, uint16_t length, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSplitInto: buffer %d not found\n\r", bufferId);
		return;
	}
	// get a consolidated version of the buffer
	auto bufferStream = consolidateBuffers(bufferIter->second);
	if (!bufferStream) {
		debug_log("bufferSplitInto: failed to create buffer\n\r");
		return;
	}
	if (!iterate) {
		clearTargets(newBufferIds);
	}

	auto chunks = splitBuffer(std::move(bufferStream), length);
	if (chunks.empty()) {
		debug_log("bufferSplitInto: failed to split buffer\n\r");
		return;
	}
	// distribute our chunks to destination buffers
	auto targetIter = newBufferIds.begin();
	for (auto &chunk : chunks) {
		auto targetId = *targetIter;
		if (iterate) {
			bufferClear(targetId);
		}
		buffers[targetId].push_back(std::move(chunk));
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}
	debug_log("bufferSplitInto: split buffer %d into %d blocks of length %d\n\r", bufferId, chunks.size(), length);
}

// VDU 23, 0, &A0, bufferId; &12, width; chunkCount; : Split buffer by width (in-place)
// VDU 23, 0, &A0, bufferId; &13, width; <bufferIds>; 65535; : Split buffer by width to new buffers
// VDU 23, 0, &A0, bufferId; &14, width; chunkCount; targetBufferId; : Split buffer by width to new buffers from ID onwards
// Split a buffer into multiple blocks/streams to new buffers/chunks by width
// Will overwrite any existing buffers
//
void VDUStreamProcessor::bufferSplitByInto(uint16_t bufferId, uint16_t width, uint16_t chunkCount, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSplitByInto: buffer %d not found\n\r", bufferId);
		return;
	}
	// get a consolidated version of the buffer
	auto bufferStream = consolidateBuffers(bufferIter->second);
	if (!bufferStream) {
		debug_log("bufferSplitByInto: failed to create buffer\n\r");
		return;
	}
	if (!iterate) {
		clearTargets(newBufferIds);
	}

	std::vector<BufferVector> chunks;
	chunks.resize(chunkCount);
	{
		// split to get raw chunks
		auto rawchunks = splitBuffer(std::move(bufferStream), width);
		if (rawchunks.empty()) {
			debug_log("bufferSplitByInto: failed to split buffer\n\r");
			return;
		}
		// and re-jig into our chunks vector
		auto chunkIndex = 0;
		for (auto &chunk : rawchunks) {
			chunks[chunkIndex].push_back(std::move(chunk));
			chunkIndex++;
			if (chunkIndex >= chunkCount) {
				chunkIndex = 0;
			}
		}
	}

	// consolidate our chunks, and distribute to buffers
	auto targetIter = newBufferIds.begin();
	for (auto &stream : chunks) {
		auto targetId = *targetIter;
		if (iterate) {
			bufferClear(targetId);
		}
		auto chunk = consolidateBuffers(stream);
		if (!chunk) {
			debug_log("bufferSplitByInto: failed to create buffer\n\r");
			return;
		}
		buffers[targetId].push_back(std::move(chunk));
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}

	debug_log("bufferSplitByInto: split buffer %d into %d chunks of width %d\n\r", bufferId, chunkCount, width);
}

// VDU 23, 0, &A0, bufferId; &15, <bufferIds>; 65535; : Spread blocks from buffer into new buffers
// VDU 23, 0, &A0, bufferId; &16, targetBufferId; : Spread blocks from target buffer onwards
//
void VDUStreamProcessor::bufferSpreadInto(uint16_t bufferId, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSpreadInto: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	// swap the source buffer contents into a local vector so it can be iterated safely even if it's a target
	BufferVector localBuffer;
	localBuffer.swap(buffer);
	if (!iterate) {
		clearTargets(newBufferIds);
	}
	// iterate over its blocks and send to targets
	auto targetIter = newBufferIds.begin();
	for (const auto &block : localBuffer) {
		auto targetId = *targetIter;
		if (iterate) {
			bufferClear(targetId);
		}
		buffers[targetId].push_back(block);
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}
	// if the source buffer is still empty, move the original contents back
	if (buffer.empty()) {
		buffer = std::move(localBuffer);
	}
}

// VDU 23, 0, &A0, bufferId; &17 : Reverse blocks within buffer
// Reverses the order of blocks within a buffer
// may be useful for mirroring bitmaps if they have been split by row
//
void VDUStreamProcessor::bufferReverseBlocks(uint16_t bufferId) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter != buffers.end()) {
		// reverse the order of the streams
		auto &buffer = bufferIter->second;
		std::reverse(buffer.begin(), buffer.end());
		debug_log("bufferReverseBlocks: reversed blocks in buffer %d\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; &18, options, <parameters> : Reverse buffer
// Reverses the contents of blocks within a buffer
// may be useful for mirroring bitmaps
//
void VDUStreamProcessor::bufferReverse(uint16_t bufferId, uint8_t options) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferReverse: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	bool use16Bit = options & REVERSE_16BIT;
	bool use32Bit = options & REVERSE_32BIT;
	bool useSize  = (options & REVERSE_SIZE) == REVERSE_SIZE;
	bool useChunks = options & REVERSE_CHUNKED;
	bool reverseBlocks = options & REVERSE_BLOCK;
	uint8_t unused = options & REVERSE_UNUSED_BITS;

	// future expansion may include:
	// reverse at an offset for a set length (within blocks)
	// reversing across whole buffer (not per block)

	if (unused != 0) {
		debug_log("bufferReverse: warning - unused bits in options byte\n\r");
	}

	auto valueSize = 1;
	auto chunkSize = 0;

	if (useSize) {
		// size follows as a word
		valueSize = readWord_t();
		if (valueSize == -1) {
			return;
		}
	} else if (use32Bit) {
		valueSize = 4;
	} else if (use16Bit) {
		valueSize = 2;
	}

	if (useChunks) {
		chunkSize = readWord_t();
		if (chunkSize == -1) {
			return;
		}
	}

	// verify that our blocks are a multiple of valueSize
	for (const auto &block : buffer) {
		auto size = block->size();
		if (size % valueSize != 0 || (chunkSize != 0 && size % chunkSize != 0)) {
			debug_log("bufferReverse: error - buffer %d contains block not a multiple of value/chunk size %d\n\r", bufferId, valueSize);
			return;
		}
	}

	debug_log("bufferReverse: reversing buffer %d, value size %d, chunk size %d\n\r", bufferId, valueSize, chunkSize);

	for (const auto &block : buffer) {
		if (chunkSize == 0) {
			// no chunking, so simpler reverse
			reverseValues(block->getBuffer(), block->size(), valueSize);
		} else {
			// reverse in chunks
			auto data = block->getBuffer();
			auto chunkCount = block->size() / chunkSize;
			for (auto i = 0; i < chunkCount; i++) {
				reverseValues(data + (i * chunkSize), chunkSize, valueSize);
			}
		}
	}

	if (reverseBlocks) {
		// reverse the order of the streams
		std::reverse(buffer.begin(), buffer.end());
		debug_log("bufferReverse: reversed blocks in buffer %d\n\r", bufferId);
	}

	debug_log("bufferReverse: reversed buffer %d\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; &19, sourceBufferId; sourceBufferId; ...; 65535; : Copy references to blocks from buffers
// Copy references to (blocks from) a list of buffers into a new buffer
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one
// This is useful to construct a single buffer from multiple buffers without the copy overhead
// If target buffer is included in the source list it will be skipped to prevent a reference loop
//
void VDUStreamProcessor::bufferCopyRef(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopyRef: ignoring buffer %d\n\r", bufferId);
		return;
	}
	bufferClear(bufferId);
	auto &buffer = buffers[bufferId];

	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			debug_log("bufferCopyRef: skipping buffer %d as it's the target\n\r", sourceId);
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			auto &sourceBuffer = sourceBufferIter->second;
			// push pointers to the blocks into our target buffer
			buffer.insert(buffer.end(), sourceBuffer.begin(), sourceBuffer.end());
		} else {
			debug_log("bufferCopyRef: buffer %d not found\n\r", sourceId);
		}
	}
	debug_log("bufferCopyRef: copied %d block references into buffer %d\n\r", buffers[bufferId].size(), bufferId);
}

// VDU 23, 0, &A0, bufferId; &1A, sourceBufferId; sourceBufferId; ...; 65535; : Copy blocks from buffers and consolidate
// Copy (blocks from) a list of buffers into a new buffer and consolidate them
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one, but will re-use the memory if it is the same size
// This is useful for constructing bitmaps from multiple buffers without needing an extra consolidate step
// If target buffer is included in the source list it will be skipped.
//
void VDUStreamProcessor::bufferCopyAndConsolidate(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopyAndConsolidate: ignoring buffer %d\n\r", bufferId);
		return;
	}

	// work out total length of buffer
	uint32_t length = 0;
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			auto &sourceBuffer = sourceBufferIter->second;
			for (const auto &block : sourceBuffer) {
				length += block->size();
			}
		}
	}

	// Ensure the buffer has 1 block of the correct size
	auto &buffer = buffers[bufferId];
	if (buffer.size() != 1 || buffer.front()->size() != length) {
		bufferRemoveUsers(bufferId);
		buffer.clear();
		auto bufferStream = make_shared_psram<BufferStream>(length);
		if (!bufferStream || !bufferStream->getBuffer()) {
			// buffer couldn't be created
			debug_log("bufferCopyAndConsolidate: failed to create buffer %d\n\r", bufferId);
			return;
		}
		buffer.push_back(std::move(bufferStream));
	}

	auto destination = buffer.front()->getBuffer();

	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			debug_log("bufferCopyAndConsolidate: skipping buffer %d as it's the target\n\r", sourceId);
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			auto &sourceBuffer = sourceBufferIter->second;
			// loop thru blocks stored against this ID
			for (const auto &block : sourceBuffer) {
				// copy the block into our target buffer
				auto bufferLength = block->size();
				memcpy(destination, block->getBuffer(), bufferLength);
				destination += bufferLength;
			}
		} else {
			debug_log("bufferCopyAndConsolidate: buffer %d not found\n\r", sourceId);
		}
	}
	debug_log("bufferCopyAndConsolidate: copied %d bytes into buffer %d\n\r", length, bufferId);
}

// VDU 23, 0, &A0, bufferId; &20, operation, <args> : Affine transform creation/combination (2D)
// VDU 23, 0, &A0, bufferId; &21, operation, <args> : Affine transform creation/combination (3D)
// Create or combine an affine transformation matrix
//
void VDUStreamProcessor::bufferAffineTransform(uint16_t bufferId, uint8_t command, bool is3D) {
	const auto op = command & AFFINE_OP_MASK;
	const bool useAdvancedOffsets = command & AFFINE_OP_ADVANCED_OFFSETS;
	const bool useBufferValue = command & AFFINE_OP_BUFFER_VALUE;
	const bool useMultiFormat = command & AFFINE_OP_MULTI_FORMAT;

	// transform is a 3x3 or 4x4 identity matrix of float values
	auto dimensions = is3D ? 3 : 2;
	MatrixSize size;
	size.rows = dimensions + 1;
	size.columns = size.rows;
	float transform[size.size()];
	memset(transform, 0, sizeof(transform));
	for (int i = 0; i < size.rows; i++) {
		transform[i * size.rows + i] = 1.0f;
	}
	bool replace = false;

	switch (op) {
		case AFFINE_IDENTITY: {
			// create an identity matrix
			replace = true;
		}	break;
		case AFFINE_INVERT: {
			// this will only work if we already have a transform matrix...
			if (!getMatrixFromBuffer(bufferId, transform, size, false)) {
				debug_log("bufferAffineTransform: failed to read matrix from buffer %d to invert\n\r", bufferId);
				return;
			}
			auto matrix = dspm::Mat(transform, size.rows, size.columns).inverse();
			// copy data from matrix back to our working transform matrix
			memcpy(transform, matrix.data, size.sizeBytes());
			replace = true;
		}	break;
		case AFFINE_ROTATE:
		case AFFINE_ROTATE_RAD: {
			float conversion = op == AFFINE_ROTATE ? DEG_TO_RAD : 1.0f;
			if (!is3D) {
				// rotate anticlockwise (given inverted Y axis) by a given angle in degrees
				float angle;
				if (!readFloatArguments(&angle, 1, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
					return;
				}
				angle = angle * conversion;
				const auto cosAngle = cosf(angle);
				const auto sinAngle = sinf(angle);
				transform[0] = cosAngle;
				transform[1] = sinAngle;
				transform[3] = -sinAngle;
				transform[4] = cosAngle;
			} else {
				float angles[3] = {0.0f, 0.0f, 0.0f};
				if (!readFloatArguments(angles, 3, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
					return;
				}
				// rotate around X, Y, Z axes
				const auto cosX = cosf(angles[0] * conversion);
				const auto sinX = sinf(angles[0] * conversion);
				const auto cosY = cosf(angles[1] * conversion);
				const auto sinY = sinf(angles[1] * conversion);
				const auto cosZ = cosf(angles[2] * conversion);
				const auto sinZ = sinf(angles[2] * conversion);
				transform[0] = cosY * cosZ;
				transform[1] = cosY * sinZ;
				transform[2] = -sinY;
				transform[4] = sinX * sinY * cosZ - cosX * sinZ;
				transform[5] = sinX * sinY * sinZ + cosX * cosZ;
				transform[6] = sinX * cosY;
				transform[8] = cosX * sinY * cosZ + sinX * sinZ;
				transform[9] = cosX * sinY * sinZ - sinX * cosZ;
				transform[10] = cosX * cosY;
			}
		}	break;
		case AFFINE_MULTIPLY: {
			// scalar multiply of values in an existing matrix by (single) argument value
			float scalar;
			if (!readFloatArguments(&scalar, 1, useBufferValue, useAdvancedOffsets, useMultiFormat)
				|| !getMatrixFromBuffer(bufferId, transform, size, false)) {
				debug_log("bufferAffineTransform: failed to read scalar, or matrix from buffer %d to multiply\n\r", bufferId);
				return;
			}
			// scale transform matrix by scalar, skipping last row
			for (int i = 0; i < (size.size() - size.columns); i++) {
				transform[i] *= scalar;
			}
			replace = true;
		}	break;
		case AFFINE_SCALE: {
			// scale by a given factor in each dimension
			float scales[dimensions];
			memset(scales, 0, sizeof(scales));
			if (!readFloatArguments(scales, dimensions, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			for (int i = 0; i < dimensions; i++) {
				transform[i * size.columns + i] = scales[i];
			}
		}	break;
		case AFFINE_TRANSLATE: {
			// translate by a given amount
			float translateXY[dimensions];
			memset(translateXY, 0, sizeof(translateXY));
			if (!readFloatArguments(translateXY, dimensions, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			for (int i = 0; i < dimensions; i++) {
				transform[i * size.columns + dimensions] = translateXY[i];
			}
		}	break;
		case AFFINE_TRANSLATE_OS_COORDS: {
			// translate by a given amount of pixels where x and y match current coordinate system scaling
			float translateXY[dimensions];
			memset(translateXY, 0, sizeof(translateXY));
			if (!readFloatArguments(translateXY, dimensions, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			auto scaled = context->scale(translateXY[0], translateXY[1]);
			transform[dimensions] = scaled.X;
			transform[size.columns + dimensions] = scaled.Y;
			if (is3D) {		// Z translate is not scaled in 3D
				transform[size.columns * dimensions - 1] = translateXY[2];
			}
		}	break;
		case AFFINE_SHEAR: {
			// shear by a given amount
			float shearXY[dimensions];
			memset(shearXY, 0, sizeof(shearXY));
			if (!readFloatArguments(shearXY, dimensions, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			for (int i = 0; i < dimensions; i++) {
				transform[i * size.columns + (i + 1)] = shearXY[i];
			}
		}	break;
		case AFFINE_SKEW:
		case AFFINE_SKEW_RAD: {
			// skew by a given amount (angle)
			float conversion = op == AFFINE_SKEW ? DEG_TO_RAD : 1.0f;
			float skewXY[dimensions];
			memset(skewXY, 0, sizeof(skewXY));
			if (!readFloatArguments(skewXY, dimensions, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			for (int i = 0; i < dimensions; i++) {
				transform[i * size.columns + (i + 1)] = tanf(conversion * skewXY[i]);
			}
		}	break;
		case AFFINE_TRANSFORM: {
			// we'll either be creating a new matrix, or combining with an existing one
			// omit reading the last row, as these are affine transforms so don't need setting
			if (!readFloatArguments(transform, size.size() - size.columns, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
		}	break;
		case AFFINE_TRANSLATE_BITMAP: {
			// translates x and y by amounts proportional to width and height of bitmap
			// first argument is a 16-bit bitmap ID
			auto bitmapId = readWord_t();
			if (bitmapId == -1) {
				return;
			}
			float translateXY[2] = {0.0f, 0.0f};
			if (!readFloatArguments(translateXY, 2, useBufferValue, useAdvancedOffsets, useMultiFormat)) {
				return;
			}
			auto bitmap = getBitmap(bitmapId);
			if (!bitmap) {
				debug_log("bufferAffineTransform: bitmap %d not found\n\r", bitmapId);
				return;
			}
			transform[dimensions] = translateXY[0] * bitmap->width;
			transform[dimensions + size.columns] = translateXY[1] * bitmap->height;
		}	break;
		default:
			debug_log("bufferAffineTransform: unknown operation %d\n\r", op);
			return;
	}

	if (!replace) {
		// we are combining - for now, only if the existing matrix is the same size
		// TODO consider handling different size matrices - could combine at larger size, and then truncate
		float existing[size.size()];
		memset(existing, 0, sizeof(existing));
		if (getMatrixFromBuffer(bufferId, existing, size, false)) {
			// combine the two matrices together
			float newTransform[size.size()];
			dspm_mult_f32(transform, existing, newTransform, size.rows, size.columns, size.columns);
			// copy data from matrix back to our working transform matrix
			memcpy(transform, newTransform, size.sizeBytes());
		}
	}

	auto bufferStream = make_shared_psram<BufferStream>(size.sizeBytes());
	if (!bufferStream || !bufferStream->getBuffer()) {
		debug_log("bufferAffineTransform: failed to create buffer %d\n\r", bufferId);
		return;
	}
	bufferStream->writeBuffer((uint8_t *)transform, size.sizeBytes());
	bufferClear(bufferId);
	buffers[bufferId].push_back(std::move(bufferStream));
	matrixMetadata[bufferId] = size;
	debug_log("bufferAffineTransform: created new matrix buffer %d\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; &22, operation, rows, columns, <args> : Generic matrix creation/manipulation
// Create or manipulate a matrix of float values
// These operations will always replace the target buffer, so long as they succeed
//
void VDUStreamProcessor::bufferMatrixManipulate(uint16_t bufferId, uint8_t command, MatrixSize size) {
	const auto op = command & MATRIX_OP_MASK;
	const bool useAdvancedOffsets = command & MATRIX_OP_ADVANCED_OFFSETS;
	const bool useBufferValue = command & MATRIX_OP_BUFFER_VALUE;

	float matrix[size.size()];
	memset(matrix, 0, sizeof(matrix));
	
	switch (op) {
		case MATRIX_SET: {
			if (!readFloatArguments(matrix, size.size(), useBufferValue, useAdvancedOffsets, false)) {
				return;
			}
		}	break;
		case MATRIX_SET_VALUE: {
			// sets a single value at a given row and column within the source matrix
			auto sourceId = readWord_t();
			if (sourceId == -1) {
				return;
			}
			auto row = readByte_t();
			auto column = readByte_t();
			if (row < 0 || column < 0 || row >= size.rows || column >= size.columns) {
				return;
			}
			// read source matrix into matrix, enforcing size match
			if (!getMatrixFromBuffer(sourceId, matrix, size, false)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d\n\r", sourceId);
				return;
			}
			// read value to set
			if (!readFloatArguments(&matrix[row * size.columns + column], 1, useBufferValue, useAdvancedOffsets, false)) {
				return;
			}
		}	break;
		case MATRIX_FILL: {
			float value;
			if (!readFloatArguments(&value, 1, useBufferValue, useAdvancedOffsets, false)) {
				return;
			}
			// set all values in matrix to the given value
			for (int i = 0; i < size.size(); i++) {
				matrix[i] = value;
			}
		}	break;
		case MATRIX_DIAGONAL: {
			// diagonal matrix with given values
			auto argCount = fabgl::imin(size.rows, size.columns);
			float args[argCount];
			memset(args, 0, sizeof(args));
			if (!readFloatArguments(args, argCount, useBufferValue, useAdvancedOffsets, false)) {
				return;
			}
			for (int i = 0; i < argCount; i++) {
				matrix[i * size.columns + i] = args[i];
			}
		}	break;
		case MATRIX_ADD: {
			// simple add of two matrixes
			auto sourceId1 = readWord_t(); if (sourceId1 == -1) return;
			auto sourceId2 = readWord_t(); if (sourceId2 == -1) return;
			// Get the matrixes, for our target size, padding or truncating as necessary
			float source[size.size()];
			memset(source, 0, sizeof(source));
			if (!getMatrixFromBuffer(sourceId1, matrix, size) || !getMatrixFromBuffer(sourceId2, source, size)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d or %d\n\r", sourceId1, sourceId2);
				return;
			}
			// add values in source to matrix
			for (int i = 0; i < size.size(); i++) {
				matrix[i] += source[i];
			}
		}	break;
		case MATRIX_SUBTRACT: {
			// simple subtract of two matrixes
			auto sourceId1 = readWord_t(); if (sourceId1 == -1) return;
			auto sourceId2 = readWord_t(); if (sourceId2 == -1) return;
			// Get the matrixes, for our target size, padding or truncating as necessary
			float source[size.size()];
			memset(source, 0, sizeof(source));
			if (!getMatrixFromBuffer(sourceId1, matrix, size) || !getMatrixFromBuffer(sourceId2, source, size)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d or %d\n\r", sourceId1, sourceId2);
				return;
			}
			// subtract values in source2 from matrix
			for (int i = 0; i < size.size(); i++) {
				matrix[i] -= source[i];
			}
		}	break;
		case MATRIX_MULTIPLY: {
			// multiply two matrixes
			// matrixes of arbitrary dimensions can be multiplied by making them square and padding with zeros
			// where the square size is the maximum of the two source size dimensions and target dimensions
			auto sourceId1 = readWord_t(); if (sourceId1 == -1) return;
			auto sourceId2 = readWord_t(); if (sourceId2 == -1) return;
			auto sourceSize1 = getMatrixSize(sourceId1);
			auto sourceSize2 = getMatrixSize(sourceId2);
			if (sourceSize1.value == 0 || sourceSize2.value == 0) {
				debug_log("bufferMatrixManipulate: source matrix %d or %d not found\n\r", sourceId1, sourceId2);
				return;
			}
			uint8_t dimensions = std::max({ (uint8_t)size.rows, (uint8_t)size.columns, (uint8_t)sourceSize1.rows, (uint8_t)sourceSize1.columns, (uint8_t)sourceSize2.rows, (uint8_t)sourceSize2.columns });
			MatrixSize resultSize;
			resultSize.rows = dimensions;
			resultSize.columns = dimensions;
			float source1[resultSize.size()];
			float source2[resultSize.size()];
			memset(source1, 0, sizeof(source1));
			memset(source2, 0, sizeof(source2));
			if (!getMatrixFromBuffer(sourceId1, source1, resultSize) || !getMatrixFromBuffer(sourceId2, source2, resultSize)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d or %d\n\r", sourceId1, sourceId2);
				return;
			}
			// multiply values in source1 and source2
			float result[resultSize.size()];
			memset(result, 0, sizeof(result));
			dspm_mult_f32(source1, source2, result, resultSize.rows, resultSize.columns, resultSize.columns);
			for (int row = 0; row < size.rows; row++) {
				for (int column = 0; column < size.columns; column++) {
					matrix[row * size.columns + column] = result[row * resultSize.columns + column];
				}
			}
		}	break;
		case MATRIX_SCALAR_MULTIPLY: {
			// multiply all values in a source matrix by a scalar
			auto sourceId = readWord_t(); if (sourceId == -1) return;
			float scalar;
			if (!readFloatArguments(&scalar, 1, useBufferValue, useAdvancedOffsets, false)) {
				return;
			}
			if (!getMatrixFromBuffer(sourceId, matrix, size)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d\n\r", sourceId);
				return;
			}
			// multiply all values by scalar
			for (int i = 0; i < size.size(); i++) {
				matrix[i] *= scalar;
			}
		}	break;
		case MATRIX_SUBMATRIX: {
			// extract a submatrix from a source matrix at given row and column
			// target matrix will be filled from top-left, truncating or padding as necessary
			auto sourceId = readWord_t(); if (sourceId == -1) return;
			auto row = readByte_t(); if (row == -1) return;
			auto column = readByte_t(); if (column == -1) return;
			auto sourceSize = getMatrixSize(sourceId);
			if (sourceSize.value == 0) {
				debug_log("bufferMatrixManipulate: source matrix %d not found\n\r", sourceId);
				return;
			}
			float source[sourceSize.size()];
			memset(source, 0, sizeof(source));
			if (!getMatrixFromBuffer(sourceId, source, sourceSize)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d\n\r", sourceId);
				return;
			}
			for (int i = 0; i < size.rows; i++) {
				for (int j = 0; j < size.columns; j++) {
					if (i + row < sourceSize.rows && j + column < sourceSize.columns) {
						matrix[i * size.columns + j] = source[(i + row) * sourceSize.columns + (j + column)];
					}
				}
			}
		}	break;
		case MATRIX_INSERT_ROW:
		case MATRIX_INSERT_COLUMN:
		case MATRIX_DELETE_ROW:
		case MATRIX_DELETE_COLUMN: {
			bool rowOp = op == MATRIX_INSERT_ROW || op == MATRIX_DELETE_ROW;
			bool insertOp = op == MATRIX_INSERT_ROW || op == MATRIX_INSERT_COLUMN;
			auto sourceId = readWord_t(); if (sourceId == -1) return;
			auto index = readByte_t(); if (index == -1) return;
			auto sourceSize = getMatrixSize(sourceId);
			if (sourceSize.value == 0) {
				debug_log("bufferMatrixManipulate: source matrix %d not found\n\r", sourceId);
				return;
			}
			// read source matrix
			float source[sourceSize.size()];
			memset(source, 0, sizeof(source));
			if (!getMatrixFromBuffer(sourceId, source, sourceSize)) {
				debug_log("bufferMatrixManipulate: failed to read matrix from buffer %d\n\r", sourceId);
				return;
			}
			// iterate over target matrix, copying from source matrix, skipping as necessary
			for (int i = 0; i < size.rows; i++) {
				for (int j = 0; j < size.columns; j++) {
					if (insertOp && (rowOp && i == index || !rowOp && j == index)) {
						// skip target at row/column we're inserting
						continue;
					}
					int sourceRow = i;
					int sourceColumn = j;
					// adjust our source row/column if we're past the index
					if (rowOp && i >= index) {
						sourceRow += insertOp ? -1 : 1;
					}
					if (!rowOp && j >= index) {
						sourceColumn += insertOp ? -1 : 1;
					}
					if (sourceRow < 0 || sourceRow >= sourceSize.rows || sourceColumn < 0 || sourceColumn >= sourceSize.columns) {
						// out of bounds, so skip
						continue;
					}
					matrix[i * size.columns + j] = source[sourceRow * sourceSize.columns + sourceColumn];
				}
			}
		}	break;
	}

	auto bufferStream = make_shared_psram<BufferStream>(size.sizeBytes());
	if (!bufferStream || !bufferStream->getBuffer()) {
		debug_log("bufferMatrixManipulate: failed to create buffer %d\n\r", bufferId);
		return;
	}
	bufferStream->writeBuffer((uint8_t *)matrix, size.sizeBytes());
	bufferClear(bufferId);
	buffers[bufferId].push_back(std::move(bufferStream));
	matrixMetadata[bufferId] = size;
	debug_log("bufferMatrixManipulate: created new matrix buffer %d\n\r", bufferId);
}


// VDU 23, 0, &A0, bufferId; &28, options, transformBufferId; bitmapId; : Apply 2d affine transformation to bitmap
// Apply an affine transformation to a bitmap, creating a new RGBA2222 format bitmap
// Replaces the target buffer with the new bitmap, and creates a corresponding bitmap
//
void VDUStreamProcessor::bufferTransformBitmap(uint16_t bufferId, uint8_t options, uint16_t transformBufferId, uint16_t bitmapId) {
	// resize flag indicates that new bitmap should get a new size
	bool shouldResize = options & TRANSFORM_BITMAP_RESIZE;
	bool explicitSize = options & TRANSFORM_BITMAP_EXPLICIT_SIZE;
	bool autoTranslate = options & TRANSFORM_BITMAP_TRANSLATE;
	if (explicitSize && !shouldResize) {
		debug_log("bufferTransformBitmap: warning - explicit size without resize flag\n\r");
		// we'll let this pass for now, but it may be an error
	}

	int xOffset = 0;
	int yOffset = 0;
	int width = 0;
	int height = 0;
	if (explicitSize) {
		width = readWord_t();
		height = readWord_t();
	}
	if (width == -1 || height == -1) {
		debug_log("bufferTransformBitmap: failed to read explicit size\n\r");
		return;
	}

	auto bitmap = getBitmap(bitmapId);
	if (!bitmap) {
		debug_log("bufferTransformBitmap: bitmap %d not found\n\r", bitmapId);
		return;
	}
	auto transformBufferIter = buffers.find(transformBufferId);
	if (transformBufferIter == buffers.end()) {
		debug_log("bufferTransformBitmap: buffer %d not found\n\r", transformBufferId);
		return;
	}
	auto &transformBuffer = transformBufferIter->second;
	if (!checkTransformBuffer(transformBuffer)) {
		debug_log("bufferTransformBitmap: buffer %d not a 2d transform matrix\n\r", transformBufferId);
		return;
	}

	auto srcWidth = bitmap->width;
	float srcWidthF = (float)srcWidth;
	auto srcHeight = bitmap->height;
	float srcHeightF = (float)srcHeight;
	auto transform = (float *)transformBuffer[0]->getBuffer();
	auto inverse = (float *)transformBuffer[1]->getBuffer();

	if (!explicitSize) {
		width = srcWidth;
		height = srcHeight;
	}
	if (shouldResize || autoTranslate) {
		int minX = INT_MAX;
		int minY = INT_MAX;
		int maxX = INT_MIN;
		int maxY = INT_MIN;
		float pos[3] = { 0.0f, 0.0f, 1.0f };
		float transformed[3];
		dspm_mult_3x3x1_f32(transform, pos, transformed);
		minX = fabgl::imin(minX, (int)transformed[0]);
		minY = fabgl::imin(minY, (int)transformed[1]);
		maxX = fabgl::imax(maxX, (int)transformed[0]);
		maxY = fabgl::imax(maxY, (int)transformed[1]);

		pos[0] = srcWidthF;
		dspm_mult_3x3x1_f32(transform, pos, transformed);
		minX = fabgl::imin(minX, (int)transformed[0]);
		minY = fabgl::imin(minY, (int)transformed[1]);
		maxX = fabgl::imax(maxX, (int)transformed[0]);
		maxY = fabgl::imax(maxY, (int)transformed[1]);

		pos[1] = srcHeightF;
		dspm_mult_3x3x1_f32(transform, pos, transformed);
		minX = fabgl::imin(minX, (int)transformed[0]);
		minY = fabgl::imin(minY, (int)transformed[1]);
		maxX = fabgl::imax(maxX, (int)transformed[0]);
		maxY = fabgl::imax(maxY, (int)transformed[1]);

		pos[0] = 0.0f;
		dspm_mult_3x3x1_f32(transform, pos, transformed);
		minX = fabgl::imin(minX, (int)transformed[0]);
		minY = fabgl::imin(minY, (int)transformed[1]);
		maxX = fabgl::imax(maxX, (int)transformed[0]);
		maxY = fabgl::imax(maxY, (int)transformed[1]);

		debug_log("bufferTransformBitmap: minX %d, minY %d, maxX %d, maxY %d\n\r", minX, minY, maxX, maxY);

		Rect transformedBox = Rect(minX, minY, maxX, maxY);
		debug_log("bufferTransformBitmap: transformed box %d, %d\n\r", transformedBox.width(), transformedBox.height());

		if (shouldResize && !explicitSize) {
			width = (maxX - (autoTranslate ? minX : 0)) + 1;
			height = (maxY - (autoTranslate ? minY : 0)) + 1;
		}
		if (autoTranslate) {
			xOffset = minX;
			yOffset = minY;
		}
	}

	// create a destination buffer using our calculated width and height
	auto bufferStream = make_shared_psram<BufferStream>(width * height);
	if (!bufferStream || !bufferStream->getBuffer()) {
		debug_log("bufferTransformBitmap: failed to create buffer %d\n\r", bufferId);
		return;
	}

	// iterate over our destination buffer, and apply the transformation to each pixel
	auto destination = (RGBA2222 *)bufferStream->getBuffer();
	float pos[3] = {0.0f, 0.0f, 1.0f};
	float srcPos[3] = {0.0f, 0.0f, 1.0f};

	debug_log("bufferTransformBitmap: width %d, height %d, xOffset %d, yOffset %d\n\r", width, height, xOffset, yOffset);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			// calculate the source pixel
			// NB we will need to adjust x,y here if we are auto-translating
			pos[0] = (float)x + xOffset;
			pos[1] = (float)y + yOffset;
			dspm_mult_3x3x1_f32(inverse, pos, srcPos);
			auto srcPixel = RGBA2222(0,0,0,0);
			if (srcPos[0] >= 0.0f && srcPos[0] < srcWidthF && srcPos[1] >= 0.0f && srcPos[1] < srcHeightF) {
				srcPixel = bitmap->getPixel2222((int)srcPos[0], (int)srcPos[1]);
			}
			destination[(int)y * width + (int)x] = srcPixel;
		}
	}

	// save new bitmap data to target buffer
	bufferClear(bufferId);
	buffers[bufferId].push_back(bufferStream);

	// create a new bitmap object
	createBitmapFromBuffer(bufferId, 1, width, height);
}

// VDU 23, 0, &A0, bufferId; &29, options, format, transformBufferId; sourceBufferId; <optional-args> : Apply transform matrix to data in a buffer
// Apply a transform matrix to data in a buffer, creating a new buffer
// Replaces the target buffer with the new buffer
// Will accept options to control the transformation
//
void VDUStreamProcessor::bufferTransformData(uint16_t bufferId, uint8_t options, uint8_t format, uint16_t transformBufferId, uint16_t sourceBufferId) {
	bool hasSize = options & TRANSFORM_DATA_HAS_SIZE;
	bool hasOffset = options & TRANSFORM_DATA_HAS_OFFSET;
	bool hasStride = options & TRANSFORM_DATA_HAS_STRIDE;
	bool hasLimit = options & TRANSFORM_DATA_HAS_LIMIT;
	bool advancedOffsets = options & TRANSFORM_DATA_ADVANCED;
	bool useBufferArgs = options & TRANSFORM_DATA_BUFFER_ARGS;
	bool perBlock = options & TRANSFORM_DATA_PER_BLOCK;

	// Default data size is derived from the transform matrix size (rows-1)
	// If "has size" flag is set, but a zero size is read, then the number of data elements will be matrix row count
	// A set of data elements to be transformed must to be contiguous, and cannot overlap block boundaries
	// Stride will default to the data size if not set (or explicitly set to 0) (in bytes)

	AdvancedOffset offsetInfo = {};
	uint32_t stride = 0;
	uint32_t limit = 0;
	uint32_t dataSize = 0;

	// Utility lambda to get an 8 or 16-bit argument, possibly from a buffer+offset
	auto getArg = [this](uint32_t &value, bool useBuffer, bool useAdvancedOffsets, bool is16Bit = true) -> bool {
		if (useBuffer) {
			auto bufferId = readWord_t();
			if (bufferId == -1) {
				return false;
			}
			auto offset = getOffsetFromStream(useAdvancedOffsets);
			if (!readBufferBytes(bufferId, offset, &value, is16Bit ? 2 : 1)) {
				return false;
			}
		} else {
			value = is16Bit ? readWord_t() : readByte_t();
			if (value == -1) {
				return false;
			}
		}
		return true;
	};

	if (hasSize && !getArg(dataSize, useBufferArgs, advancedOffsets, false)) {
		return;
	}

	if (hasOffset) {
		if (useBufferArgs) {
			auto argBuffer = readWord_t();
			if (argBuffer == -1) {
				return;
			}
			auto argOffset = getOffsetFromStream(advancedOffsets);
			if (advancedOffsets) {
				if (!readBufferBytes(argBuffer, argOffset, &(offsetInfo.blockOffset), 3, true)) {	// 3 bytes for 24-bit value
					return;
				}
				if (offsetInfo.blockOffset & 0x00800000) {	// we have a block index
					if (!readBufferBytes(argBuffer, argOffset, &(offsetInfo.blockIndex), 2)) {
						return;
					}
					offsetInfo.blockOffset &= 0x007FFFFF;
				}
			} else {
				if (!readBufferBytes(argBuffer, argOffset, &(offsetInfo.blockOffset), 2)) {	// non-advanced offset, so just read 2 bytes
					return;
				}
			}
		} else {
			offsetInfo = getOffsetFromStream(advancedOffsets);
			if (offsetInfo.blockOffset == -1) {
				return;
			}
		}
	}

	if (hasStride && !getArg(stride, useBufferArgs, advancedOffsets)) {
		return;
	}
	if (hasLimit && !getArg(limit, useBufferArgs, advancedOffsets)) {
		return;
	}
	if (limit == 0) {	// limit of 0 means transform all the data
		limit = 0xFFFFFFFF;
	}

	auto sourceBufferIter = buffers.find(sourceBufferId);
	if (sourceBufferIter == buffers.end()) {
		debug_log("bufferTransformData: buffer %d not found\n\r", sourceBufferId);
		return;
	}

	auto transformSize = getMatrixSize(transformBufferId);
	if (transformSize.value == 0) {
		debug_log("bufferTransformData: matrix %d not found\n\r", transformBufferId);
		return;
	}
	// How many source values are we reading at a time for this matrix size?
	if (dataSize == 0) {
		// no explicit data size set, so we'll use 1 row less than the matrix size
		dataSize = transformSize.rows - (hasSize ? 0 : 1);
	}
	if (dataSize > transformSize.rows) {
		debug_log("bufferTransformData: data size %d exceeds matrix rows %d\n\r", dataSize, transformSize.rows);
		return;
	}
	// as we have metadata, buffer _should_ exist, but we'll check anyway
	auto transformBufferIter = buffers.find(transformBufferId);
	if (transformBufferIter == buffers.end()) {
		debug_log("bufferTransformBitmap: buffer %d not found\n\r", transformBufferId);
		return;
	}
	auto &transformBuffer = transformBufferIter->second;
	if (transformBuffer.size() == 0 || transformBuffer[0]->size() != transformSize.sizeBytes()) {
		debug_log("bufferTransformData: matrix %d not found\n\r", transformBufferId);
		return;
	}
	auto transform = (float *)transformBuffer[0]->getBuffer();

	bool isFixed, is16Bit;
	int8_t shift;
	extractFormatInfo(format, isFixed, is16Bit, shift);
	auto bytesPerValue = is16Bit ? 2 : 4;
	if (stride == 0) {
		stride = bytesPerValue * dataSize;
	}

	// Make 1-dimensional arrays for our source and transformed data
	float srcData[transformSize.rows];
	std::fill_n(srcData, transformSize.rows, 1.0f);
	float transformed[transformSize.rows];

	// our destination buffer will be a copy of the source, with the data transformed
	BufferVector streams;
	auto workingOffset = offsetInfo;
	auto workingLimit = limit;
	for (const auto &block : sourceBufferIter->second) {
		// push a copy of the source block into our new vector
		auto bufferStream = make_shared_psram<BufferStream>(block->size());
		if (!bufferStream || !bufferStream->getBuffer()) {
			debug_log("bufferTransformData: failed to create buffer\n\r");
			return;
		}
		bufferStream->writeBuffer(block->getBuffer(), block->size());
		streams.push_back(bufferStream);

		if (perBlock && workingLimit != limit) {
			workingLimit = limit;
			// per-block, and we've adjusted at least one value, so reset blockOffset
			workingOffset.blockOffset = offsetInfo.blockOffset;
			// getBufferSpan below in the while condition should be incrementing our blockIndex
		}

		// now transform data in the buffer, according to the rules we have
		uint32_t sourceData = 0;
		while (!getBufferSpan(streams, workingOffset, bytesPerValue * dataSize).empty() && workingLimit) {
			workingLimit--;
			// read the source data - we will always have enough data to read
			auto sourceOffset = workingOffset;
			for (int i = 0; i < dataSize; i++) {
				auto span = getBufferSpan(streams, sourceOffset, bytesPerValue);
				sourceOffset.blockOffset += bytesPerValue;
				sourceData = 0;
				memcpy(&sourceData, span.data(), bytesPerValue);
				srcData[i] = convertValueToFloat(sourceData, is16Bit, isFixed, shift);
			}
			// apply the transform and write back to the buffer
			dspm_mult_f32(transform, srcData, transformed, transformSize.rows, transformSize.columns, 1);
			for (int i = 0; i < dataSize; i++) {
				auto value = convertFloatToValue(transformed[i], is16Bit, isFixed, shift);
				bufferStream->writeBuffer((uint8_t *)&value, bytesPerValue, workingOffset.blockOffset + (i * bytesPerValue));
			}
			workingOffset.blockOffset += stride;
		}
	}
	// replace buffer with new one
	bufferRemoveUsers(bufferId);
	buffers[bufferId].assign(std::make_move_iterator(streams.begin()), std::make_move_iterator(streams.end()));
	debug_log("bufferTransformData: copied %d streams into buffer %d (%d)\n\r", streams.size(), bufferId, buffers[bufferId].size());
}

// VDU 23, 0, &A0, bufferId; &30, options, offset; variableId; [default[;]]
// Copy a VDP Variable value into a buffer at a given offset
//
void VDUStreamProcessor::bufferReadVariable(uint16_t bufferId) {
	auto options = readByte_t();
	if (options == -1) {
		return;
	}
	bool useBigEndian = options & READ_VAR_BIG_ENDIAN;
	bool useAdvancedOffsets = options & READ_VAR_ADVANCED_OFFSETS;
	bool useDefault = options & READ_VAR_USE_DEFAULT;
	bool use16Bit = options & READ_VAR_16BIT;

	auto offset = getOffsetFromStream(useAdvancedOffsets);
	if (offset.blockOffset == -1) {
		return;
	}
	auto variableId = readWord_t();
	if (variableId == -1) {
		return;
	}

	uint32_t defaultValue = 0;
	if (useDefault) {
		defaultValue = use16Bit ? readWord_t() : readByte_t();
		if (defaultValue == -1) {
			return;
		}
	}

	// Does our target exist?
	auto target = getBufferSpan(bufferId, offset, use16Bit ? 2 : 1);
	if (target.empty()) {
		debug_log("bufferReadVariable: buffer %d not found or offset %d out of range\n\r", bufferId, offset.blockOffset);
		return;
	}

	if (isVDPVariableSet(variableId)) {
		// flag exists, so write it to the buffer
		auto value = getVDPVariable(variableId);
		if (useBigEndian) {
			value = value << 8 | (value >> 8);
		}
		target.front() = value & 0xFF;
		if (use16Bit) {
			target[1] = value >> 8;
		}
	} else if (useDefault) {
		// flag doesn't exist, so write the default value to the buffer
		target.front() = defaultValue & 0xFF;
		if (use16Bit) {
			target[1] = defaultValue >> 8;
		}
	} else {
		debug_log("bufferReadVariable: flag %d not found and no default value\n\r", variableId);
	}
}

// VDU 23, 0, &A0, bufferId; &40, sourceBufferId; : Compress blocks from a buffer
// Compress (blocks from) a buffer into a new buffer.
// Replaces the target buffer with the new one.
//
void VDUStreamProcessor::bufferCompress(uint16_t bufferId, uint16_t sourceBufferId) {
	debug_log("Compressing into buffer %u\n\r", bufferId);

	auto sourceBufferIter = buffers.find(sourceBufferId);
	if (sourceBufferIter == buffers.end()) {
		debug_log("bufferCompress: buffer %d not found\n\r", sourceBufferId);
		return;
	}

	// create a temporary output buffer, which may be expanded during compression
	uint8_t* p_temp = (uint8_t*) ps_malloc(COMPRESSION_OUTPUT_CHUNK_SIZE);
	if (p_temp) {
		// prepare for doing compression
		CompressionData cd;
		agon_init_compression(&cd, &p_temp, &local_write_compressed_byte);

		// Output the compression header
		CompressionFileHeader hdr;
		hdr.marker[0] = 'C';
		hdr.marker[1] = 'm';
		hdr.marker[2] = 'p';
		hdr.type = COMPRESSION_TYPE_TURBO;
		hdr.orig_size = 0;

		auto p_hdr_bytes = hdr.marker;
		for (int i = 0; i < sizeof(hdr); i++) {
			local_write_compressed_byte(&cd, *p_hdr_bytes++);
		}

		// loop thru blocks stored against the source buffer ID
		uint32_t orig_size = 0;
		auto &sourceBuffer = sourceBufferIter->second;
		for (const auto &block : sourceBuffer) {
			// compress the block into our temporary buffer
			auto bufferLength = block->size();
			auto p_data = block->getBuffer();
			orig_size += bufferLength;
			debug_log(" from buffer %u [%08X] %u bytes\n\r", sourceBufferId, p_data, bufferLength);
			debug_log(" %02hX %02hX %02hX %02hX\n\r",
						p_data[0], p_data[1], p_data[2], p_data[3]);
			cd.input_count += bufferLength;
			while (bufferLength--) {
				agon_compress_byte(&cd, *p_data++);
			}
		}
		agon_finish_compression(&cd);

		// make a single buffer with all of the temporary output data
		auto bufferStream = make_shared_psram<BufferStream>(cd.output_count);
		if (!bufferStream || !bufferStream->getBuffer()) {
			// buffer couldn't be created
			debug_log("bufferCompress: failed to create buffer %d\n\r", bufferId);
			return;
		}

		auto p_hdr = (CompressionFileHeader*) p_temp;
		p_hdr->orig_size = orig_size;

		auto destination = bufferStream->getBuffer();
		debug_log(" %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX\n\r",
					p_temp[0], p_temp[1], p_temp[2], p_temp[3],
					p_temp[4], p_temp[5], p_temp[6], p_temp[7],
					p_temp[8], p_temp[9], p_temp[10], p_temp[11]);
		memcpy(destination, p_temp, cd.output_count);
		heap_caps_free(p_temp);
		bufferClear(bufferId);
		buffers[bufferId].push_back(bufferStream);

		uint32_t pct = (cd.output_count * 100) / cd.input_count;
		debug_log("Compressed %u input bytes to %u output bytes (%u%%) at %08X\n\r",
				cd.input_count, cd.output_count, pct, destination);
	} else {
		debug_log("bufferCompress: cannot allocate temporary buffer of %d bytes\n\r", COMPRESSION_OUTPUT_CHUNK_SIZE);
	}
}

// VDU 23, 0, &A0, bufferId; &41, sourceBufferId; : Decompress blocks from a buffer
// Decompress (blocks from) a buffer into a new buffer.
// Replaces the target buffer with the new one.
//
void VDUStreamProcessor::bufferDecompress(uint16_t bufferId, uint16_t sourceBufferId) {
	#ifdef DEBUG
	auto start = millis();
	#endif
	auto sourceBufferIter = buffers.find(sourceBufferId);
	if (sourceBufferIter == buffers.end()) {
		debug_log("bufferDeompress: buffer %d not found\n\r", sourceBufferId);
		return;
	}
	auto &sourceBuffer = sourceBufferIter->second;

	// Validate the compression header
	if (sourceBuffer.size() >= 1 && sourceBuffer[0]->size() < sizeof(CompressionFileHeader)) {
		debug_log("bufferDecompress: buffer too small for header\n\r");
		return;
	}
	
	auto p_hdr = (const CompressionFileHeader*) sourceBuffer[0]->getBuffer();
	if (p_hdr->marker[0] != 'C' ||
		p_hdr->marker[1] != 'm' ||
		p_hdr->marker[2] != 'p' ||
		p_hdr->type != COMPRESSION_TYPE_TURBO) {
		debug_log("bufferDecompress: header is invalid\n\r");
		return;
	}
	auto orig_size = p_hdr->orig_size;

	debug_log("Decompressing into buffer %u\n\r", bufferId);

	// create output buffer
	auto bufferStream = make_shared_psram<BufferStream>(orig_size);
	if (!bufferStream || !bufferStream->getBuffer()) {
		// buffer couldn't be created
		debug_log("bufferDecompress: failed to create buffer %d\n\r", bufferId);
		return;
	}

	// prepare for doing compression
	auto buffer = bufferStream->getBuffer();
	DecompressionData dd;
	agon_init_decompression(&dd, &buffer, &local_write_decompressed_byte, orig_size);

	// loop thru blocks stored against the source buffer ID
	uint32_t skip_hdr = sizeof(CompressionFileHeader);
	dd.input_count = skip_hdr;
	for (const auto &block : sourceBuffer) {
		// decompress the block into our temporary buffer
		auto bufferLength = block->size() - skip_hdr;
		auto p_data = block->getBuffer();
		debug_log(" from buffer %u [%08X] %u bytes\n\r", sourceBufferId, p_data, bufferLength);
		debug_log(" %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX\n\r",
					p_data[0], p_data[1], p_data[2], p_data[3],
					p_data[4], p_data[5], p_data[6], p_data[7],
					p_data[8], p_data[9], p_data[10], p_data[11]);
		p_data += skip_hdr;
		skip_hdr = 0;
		dd.input_count += bufferLength;
		while (bufferLength--) {
			agon_decompress_byte(&dd, *p_data++);
		}
	}

	debug_log(" %02hX %02hX %02hX %02hX\n\r",
				buffer[0], buffer[1], buffer[2], buffer[3]);
	bufferClear(bufferId);
	buffers[bufferId].push_back(bufferStream);

	uint32_t pct = (dd.output_count * 100) / dd.input_count;
	debug_log("Decompressed %u input bytes to %u output bytes (%u%%) at %08X\n\r",
				dd.input_count, dd.output_count, pct, buffer);

	if (dd.output_count != orig_size) {
		debug_log("Decompressed buffer size %u does not equal original size %u\r\n",
					dd.output_count, orig_size);
	}
	#ifdef DEBUG
	debug_log("Decompress took %u ms\n\r", millis() - start);
	#endif
}

// VDU 23, 0, &A0, bufferId; &48, options, sourceBufferId; [width;] [mapBufferId;] [mapValues...] : Expand a bitmap buffer
// Expands a bitmap buffer into a new buffer with 8-bit values
// options dictates how the expansion is done
// width will be provided to give a pixel width at which a byte-align is done
//
void VDUStreamProcessor::bufferExpandBitmap(uint16_t bufferId, uint8_t options, uint16_t sourceBufferId) {
	auto sourceBufferIter = buffers.find(sourceBufferId);
	if (sourceBufferIter == buffers.end()) {
		debug_log("bufferExpandBitmap: source buffer %d not found\n\r", sourceBufferId);
		return;
	}
	auto &sourceBuffer = sourceBufferIter->second;

	// pixelSize is our number of bits in a pixel
	auto pixelSize = options & EXPAND_BITMAP_SIZE;
	if (pixelSize == 0) {
		pixelSize = 8;
	}
	// do we have an aligned width?
	bool aligned = options & EXPAND_BITMAP_ALIGNED;
	// do we have a map buffer, or are we just reading the values from the stream?
	bool useBuffer = options & EXPAND_BITMAP_USEBUFFER;
	int16_t width = -1;

	uint8_t * mapValues = nullptr;

	if (aligned) {
		width = readWord_t();
		if (width == -1) {
			debug_log("bufferExpandBitmap: failed to read width\n\r");
			return;
		}
	}

	auto numValues = 1 << pixelSize;

	if (useBuffer) {
		auto mapId = readWord_t();
		if (mapId == -1) {
			debug_log("bufferExpandBitmap: failed to read map buffer ID\n\r");
			return;
		}
		auto bufferIter = buffers.find(mapId);
		if (bufferIter == buffers.end()) {
			debug_log("bufferExpandBitmap: map buffer %d not found\n\r", mapId);
			return;
		}
		auto &buffer = bufferIter->second;
		if (buffer.size() != 1) {
			debug_log("bufferExpandBitmap: map buffer %d does not contain a single block\n\r", mapId);
			return;
		}
		if (buffer[0]->size() < numValues) {
			debug_log("bufferExpandBitmap: map buffer %d does not contain at least %d values\n\r", mapId, numValues);
			return;
		}
		mapValues = buffer[0]->getBuffer();
	} else {
		// read pixelSize bytes from stream
		mapValues = (uint8_t *) ps_malloc(numValues);
		if (!mapValues) {
			debug_log("bufferExpandBitmap: failed to allocate map values\n\r");
			return;
		}
		if (readIntoBuffer(mapValues, 1 << pixelSize) != 0) {
			debug_log("bufferExpandBitmap: failed to read map values\n\r");
			free(mapValues);
			return;
		}
		debug_log("bufferExpandBitmap: read map values ");
		for (int i = 0; i < numValues; i++) {
			debug_log("%02hX ", mapValues[i]);
		}
		debug_log("\n\r");
	}

	// work out source size
	uint32_t sourceSize = 0;
	for (const auto &block : sourceBuffer) {
		sourceSize += block->size();
	}

	// if we are aligning we need to work out our byte width, based off the pixel width
	uint32_t byteWidth = 0;	
	if (aligned) {
		byteWidth = ((pixelSize * width) + (8 - pixelSize)) / 8;
	}

	// work out our output size
	uint32_t outputSize = 0;
	if (aligned) {
		outputSize = (sourceSize / byteWidth) * width;
	} else {
		outputSize = (sourceSize * 8) / pixelSize;
	}

	debug_log("bufferExpandBitmap: source size %d, output size %d, pixel size %d, width %d, byte width %d\n\r",
		sourceSize, outputSize, pixelSize, width, byteWidth);

	// create output buffer
	auto bufferStream = make_shared_psram<BufferStream>(outputSize);

	if (!bufferStream || !bufferStream->getBuffer()) {
		// buffer couldn't be created
		debug_log("bufferExpandBitmap: failed to create buffer %d\n\r", bufferId);
		if (!useBuffer) {
			free(mapValues);
		}
		return;
	}

	auto destination = bufferStream->getBuffer();

	// iterate through source buffer
	auto p_data = destination;
	uint8_t bit = 0;
	uint8_t pixel = 0;
	uint16_t pixelCount = 0;
	for (const auto &block : sourceBuffer) {
		auto bufferLength = block->size();
		auto p_source = block->getBuffer();

		// go through one byte at a time,
		// and expand the pixels into the destination buffer
		// aligning when our pixel count reaches our pixel width if required

		while (bufferLength--) {
			auto value = *p_source++;
			for (uint8_t i = 0; i < 8; i++) {
				pixel = (pixel << 1) | ((value >> (7 - i)) & 1);
				bit++;
				if (bit == pixelSize) {
					bit = 0;
					*p_data++ = mapValues[pixel];
					// debug_log("pixel map: %02hX %02hX (%02hX) %d\n\r", pixel, mapValues[pixel], value, i);
					pixel = 0;
					if (aligned) {
						if (++pixelCount == width) {
							// byte align
							// debug_log("aligned... skipping to next byte at byte bit %d\n\r", i);
							pixelCount = 0;
							// jump to next byte
							break;
						}
					}
				}
			}
		}
	}

	// save our bufferStream to the buffer
	bufferClear(bufferId);
	buffers[bufferId].push_back(std::move(bufferStream));
	if (!useBuffer) {
		free(mapValues);
	}
	debug_log("bufferExpandBitmap: expanded %d bytes into buffer %d\n\r", outputSize, bufferId);
}

void VDUStreamProcessor::bufferAddCallback(uint16_t bufferId, uint16_t type) {
	callbackBuffers[type].insert(bufferId);
}

void VDUStreamProcessor::bufferRemoveCallback(uint16_t bufferId, uint16_t type) {
	if (type == 65535) {
		for (auto & cb : callbackBuffers) {
			bufferRemoveCallback(bufferId, cb.first);
		}
		return;
	}
	if (bufferId == 65535) {
		callbackBuffers.erase(type);
		return;
	}
	callbackBuffers[type].erase(bufferId);
}

void VDUStreamProcessor::bufferCallCallbacks(uint16_t type) {
	for (const auto & bufferId : callbackBuffers[type]) {
		bufferCall(bufferId, {});
	}
}


#endif // VDU_BUFFERED_H
