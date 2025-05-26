//
// Title:			Agon Video BIOS - Function prototypes
// Author:			Dean Belfield
// Created:			05/09/2022
// Last Updated:	13/08/2023
//
// Modinfo:
// 04/03/2023:		Added LOGICAL_SCRW and LOGICAL_SCRH
// 17/03/2023:		Added PACKET_RTC, EPOCH_YEAR, MAX_SPRITES, MAX_BITMAPS
// 21/03/2023:		Added PACKET_KEYSTATE
// 22/03/2023:		Added VDP codes
// 23/03/2023:		Increased baud rate to 1152000
// 09/08/2023:		Added VDP_SWITCHBUFFER
// 13/08/2023:		Added additional modelines

#pragma once

#define EPOCH_YEAR				1980	// 1-byte dates are offset from this (for FatFS)
#define MAX_SPRITES				256		// Maximum number of sprites
#define MAX_BITMAPS				256		// Maximum number of bitmaps

// #define VDP_USE_WDT						// Use the esp watchdog timer (experimental)

#define UART_BR					1152000	// Max baud rate; previous stable value was 384000
#define UART_NA					-1
#define UART_TX					2
#define UART_RX					34
#define UART_RTS				13		// The ESP32 RTS pin (eZ80 CTS)
#define UART_CTS	 			14		// The ESP32 CTS pin (eZ80 RTS)

#define COMMS_TIMEOUT			200		// Timeout for VDP commands (ms)
#define FAST_COMMS_TIMEOUT		10		// Fast timeout for VDP commands (ms)

#define UART_RX_SIZE			256		// The RX buffer size
#define UART_RX_THRESH			128		// Point at which RTS is toggled

#define GPIO_ITRP				17		// VSync Interrupt Pin - for reference only

#define CURSOR_PHASE			640		// Cursor blink phase (ms)
#define CURSOR_FAST_PHASE		320		// Cursor blink phase (ms)

// Commands for VDU 23, 0, n
//
#define VDP_CURSOR_VSTART		0x0A	// Cursor start line offset (0-15)
#define VDP_CURSOR_VEND			0x0B	// Cursor end line offset
#define VDP_GP					0x80	// General poll data
#define VDP_KEYCODE				0x81	// Keyboard data
#define VDP_CURSOR				0x82	// Cursor positions
#define VDP_SCRCHAR				0x83	// Character read from screen
#define VDP_SCRPIXEL			0x84	// Pixel read from screen
#define VDP_AUDIO				0x85	// Audio commands
#define VDP_MODE				0x86	// Get screen dimensions
#define VDP_RTC					0x87	// RTC
#define VDP_KEYSTATE			0x88	// Keyboard repeat rate and LED status
#define VDP_MOUSE				0x89	// Mouse data
#define VDP_CURSOR_HSTART		0x8A	// Cursor start row offset (0-15)
#define VDP_CURSOR_HEND			0x8B	// Cursor end row offset
#define VDP_CURSOR_MOVE			0x8C	// Cursor relative move
#define VDP_UDG					0x90	// User defined characters
#define VDP_UDG_RESET			0x91	// Reset UDGs
#define VDP_MAP_CHAR_TO_BITMAP	0x92	// Map a character to a bitmap
#define VDP_SCRCHAR_GRAPHICS	0x93	// Character read from screen at graphics coordinates
#define VDP_READ_COLOUR			0x94	// Read colour
#define VDP_FONT				0x95	// Font management commands
#define VDP_AFFINE_TRANSFORM	0x96	// Set affine transform
#define VDP_CONTROLKEYS			0x98	// Control keys on/off
#define VDP_CHECKKEY			0x99	// Request updated keyboard data for a key
#define VDP_TEMP_PAGED_MODE		0x9A	// Emable temporary paged mode
#define VDP_BUFFER_PRINT		0x9B	// Print a buffer of characters literally with no command interpretation
#define VDP_TEXT_VIEWPORT		0x9C	// Set text viewport using current graphics coordinates
#define VDP_GRAPHICS_VIEWPORT	0x9D	// Set graphics viewport using current graphics coordinates
#define VDP_GRAPHICS_ORIGIN		0x9E	// Set graphics origin using current graphics coordinates
#define VDP_SHIFT_ORIGIN		0x9F	// Move origin to new position from graphics coordinates, and viewports too
#define VDP_BUFFERED			0xA0	// Buffered commands
#define VDP_UPDATER				0xA1	// Update VDP
#define VDP_LOGICALCOORDS		0xC0	// Switch BBC Micro style logical coords on and off
#define VDP_LEGACYMODES			0xC1	// Switch VDP 1.03 compatible modes on and off
#define VDP_LAYERS				0xC2	// Tile engine layer management commands (experimental)
#define VDP_SWITCHBUFFER		0xC3	// Double buffering control
#define VDP_COPPER				0xC4	// "Copper" style commands
#define VDP_CONTEXT				0xC8	// Context management commands
#define VDP_FLUSH_DRAWING_QUEUE	0xCA	// Flush the drawing queue
#define VDP_PATTERN_LENGTH		0xF2	// Set pattern length (*FX 163,242,n)
#define VDP_VDPVAR_SET			0xF8	// Set a test flag
#define VDP_VDPVAR_CLEAR		0xF9	// Clear a test flag
#define VDP_CONSOLEMODE			0xFE	// Switch console mode on and off
#define VDP_TERMINALMODE		0xFF	// Switch to terminal mode

// And the corresponding return packets
// By convention, these match their VDP counterpart, but with the top bit reset
//
#define PACKET_GP				0x00	// General poll data
#define PACKET_KEYCODE			0x01	// Keyboard data
#define PACKET_CURSOR			0x02	// Cursor positions
#define PACKET_SCRCHAR			0x03	// Character read from screen
#define PACKET_SCRPIXEL			0x04	// Pixel read from screen
#define PACKET_AUDIO			0x05	// Audio acknowledgement
#define PACKET_MODE				0x06	// Get screen dimensions
#define PACKET_RTC				0x07	// RTC
#define PACKET_KEYSTATE			0x08	// Keyboard repeat rate and LED status
#define PACKET_MOUSE			0x09	// Mouse data
#define PACKET_ECHO				0x0A	// Echo
#define PACKET_ECHO_END			0x0B	// Echo end

#define AUDIO_CHANNELS			3		// Default number of audio channels
#define AUDIO_DEFAULT_SAMPLE_RATE	16384	// Default sample rate
#define MAX_AUDIO_CHANNELS		32		// Maximum number of audio channels
#define AUDIO_CHANNEL_PRIORITY	3		// Sound driver task priority with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest
#define AUDIO_CORE				0		// Core to run audio tasks on

// Audio command definitions
//
#define AUDIO_CMD_PLAY			0		// Play a sound
#define AUDIO_CMD_STATUS		1		// Get the status of a channel
#define AUDIO_CMD_VOLUME		2		// Set the volume of a channel
#define AUDIO_CMD_FREQUENCY		3		// Set the frequency of a channel
#define AUDIO_CMD_WAVEFORM		4		// Set the waveform type for a channel
#define AUDIO_CMD_SAMPLE		5		// Sample management
#define AUDIO_CMD_ENV_VOLUME	6		// Define/set a volume envelope
#define AUDIO_CMD_ENV_FREQUENCY	7		// Define/set a frequency envelope
#define AUDIO_CMD_ENABLE		8		// Enables a channel
#define AUDIO_CMD_DISABLE		9		// Disables (destroys) a channel
#define AUDIO_CMD_RESET			10		// Reset audio channel
#define AUDIO_CMD_SEEK			11		// Seek to a position in a sample
#define AUDIO_CMD_DURATION		12		// Set the duration of a channel
#define AUDIO_CMD_SAMPLERATE	13		// Set the samplerate for channel or underlying audio system
#define AUDIO_CMD_SET_PARAM		14		// Set a waveform parameter

#define AUDIO_WAVE_DEFAULT		0		// Default waveform (Square wave)
#define AUDIO_WAVE_SQUARE		0		// Square wave
#define AUDIO_WAVE_TRIANGLE		1		// Triangle wave
#define AUDIO_WAVE_SAWTOOTH		2		// Sawtooth wave
#define AUDIO_WAVE_SINE			3		// Sine wave
#define AUDIO_WAVE_NOISE		4		// Noise (simple, no frequency support)
#define AUDIO_WAVE_VICNOISE		5		// VIC-style noise (supports frequency)
#define AUDIO_WAVE_SAMPLE		8		// Sample playback, explicit buffer ID sent in following 2 bytes
// negative values for waveforms indicate a sample number

#define AUDIO_SAMPLE_LOAD		0		// Send a sample to the VDP
#define AUDIO_SAMPLE_CLEAR		1		// Clear/delete a sample
#define AUDIO_SAMPLE_FROM_BUFFER				2	// Load a sample from a buffer
#define AUDIO_SAMPLE_SET_FREQUENCY				3	// Set the base frequency of a sample
#define AUDIO_SAMPLE_BUFFER_SET_FREQUENCY		4	// Set the base frequency of a sample (using buffer ID)
#define AUDIO_SAMPLE_SET_REPEAT_START			5	// Set the repeat start point of a sample
#define AUDIO_SAMPLE_BUFFER_SET_REPEAT_START	6	// Set the repeat start point of a sample (using buffer ID)
#define AUDIO_SAMPLE_SET_REPEAT_LENGTH			7	// Set the repeat length of a sample
#define AUDIO_SAMPLE_BUFFER_SET_REPEAT_LENGTH	8	// Set the repeat length of a sample (using buffer ID)
#define AUDIO_SAMPLE_DEBUG_INFO 0x10	// Get debug info about a sample

#define AUDIO_DEFAULT_FREQUENCY	523		// Default sample frequency (C5, or C above middle C)

#define AUDIO_FORMAT_8BIT_SIGNED	0	// 8-bit signed sample
#define AUDIO_FORMAT_8BIT_UNSIGNED	1	// 8-bit unsigned sample
#define AUDIO_FORMAT_DATA_MASK		7	// data bit mask for format
#define AUDIO_FORMAT_WITH_RATE		8	// OR this with the format to indicate a sample rate follows
#define AUDIO_FORMAT_TUNEABLE		16	// OR this with the format to indicate sample can be tuned (frequency adjustable)

#define AUDIO_ENVELOPE_NONE			0		// No envelope
#define AUDIO_ENVELOPE_ADSR			1		// Simple ADSR volume envelope
#define AUDIO_ENVELOPE_MULTIPHASE_ADSR		2		// Multi-phase ADSR envelope

#define AUDIO_FREQUENCY_ENVELOPE_STEPPED	1		// Stepped frequency envelope

#define AUDIO_FREQUENCY_REPEATS		0x01	// Repeat/loop the frequency envelope
#define AUDIO_FREQUENCY_CUMULATIVE	0x02	// Reset frequency envelope when looping
#define AUDIO_FREQUENCY_RESTRICT	0x04	// Restrict frequency envelope to the range 0-65535

#define AUDIO_PARAM_DUTY_CYCLE		0		// Square wave duty cycle
#define AUDIO_PARAM_VOLUME			2		// Volume
#define AUDIO_PARAM_FREQUENCY		3		// Frequency
#define AUDIO_PARAM_16BIT			0x80	// 16-bit value
#define AUDIO_PARAM_MASK			0x0F	// Parameter mask

#define AUDIO_STATUS_ACTIVE		0x01	// Has an active waveform
#define AUDIO_STATUS_PLAYING	0x02	// Playing a note (not in release phase)
#define AUDIO_STATUS_INDEFINITE	0x04	// Indefinite duration sound playing
#define AUDIO_STATUS_HAS_VOLUME_ENVELOPE	0x08	// Channel has a volume envelope set
#define AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE	0x10	// Channel has a frequency envelope set

// Mouse commands
#define MOUSE_ENABLE			0		// Enable mouse
#define MOUSE_DISABLE			1		// Disable mouse
#define MOUSE_RESET				2		// Reset mouse
#define MOUSE_SET_CURSOR		3		// Set cursor
#define MOUSE_SET_POSITION		4		// Set mouse position
#define MOUSE_SET_AREA			5		// Set mouse area
#define MOUSE_SET_SAMPLERATE	6		// Set mouse sample rate
#define MOUSE_SET_RESOLUTION	7		// Set mouse resolution
#define MOUSE_SET_SCALING		8		// Set mouse scaling
#define MOUSE_SET_ACCERATION	9		// Set mouse acceleration (1-2000)
#define MOUSE_SET_WHEELACC		10		// Set mouse wheel acceleration

#define MOUSE_DEFAULT_CURSOR		0		// Default mouse cursor
#define MOUSE_DEFAULT_SAMPLERATE	60		// Default mouse sample rate
#define MOUSE_DEFAULT_RESOLUTION	2		// Default mouse resolution (4 counts/mm)
#define MOUSE_DEFAULT_SCALING		1		// Default mouse scaling (1:1)
#define MOUSE_DEFAULT_ACCELERATION	180		// Default mouse acceleration 
#define MOUSE_DEFAULT_WHEELACC		60000	// Default mouse wheel acceleration

// Font management commands
#define FONT_SELECT						0		// Select a font (by buffer ID, 65535 for system font)
#define FONT_FROM_BUFFER				1		// Load/define a font from a buffer
#define FONT_SET_INFO					2		// Set font information
#define FONT_SET_NAME					3		// Set font name
#define FONT_CLEAR						4		// Clear a font
#define FONT_COPY_SYSTEM				5		// Copy system font to a buffer
#define FONT_SELECT_BY_NAME				0x10	// Select a font by name
#define FONT_DEBUG_INFO					0x20	// Get debug info about a font
// Future commands may include ability to search for fonts based on their info settings

#define FONT_INFO_WIDTH					0		// Font width
#define FONT_INFO_HEIGHT				1		// Font height
#define FONT_INFO_ASCENT				2		// Font ascent
#define FONT_INFO_FLAGS					3		// Font flags
#define FONT_INFO_CHARPTRS_BUFFER		4		// Font character pointers (for variable width fonts)
#define FONT_INFO_POINTSIZE				5		// Font point size
#define FONT_INFO_INLEADING				6		// Font inleading
#define FONT_INFO_EXLEADING				7		// Font exleading
#define FONT_INFO_WEIGHT				8		// Font weight
#define FONT_INFO_CHARSET				9		// Font character set ??
#define FONT_INFO_CODEPAGE				10		// Font code page

#define FONT_SELECTFLAG_ADJUSTBASE		0x01	// Adjust font baseline, based on ascent

// Context management commands
#define CONTEXT_SELECT					0		// Select a context stack
#define CONTEXT_DELETE					1		// Delete a context stack
#define CONTEXT_RESET					2		// Reset current context
#define CONTEXT_SAVE					3		// Save a context to stack
#define CONTEXT_RESTORE					4		// Restore a context from stack
#define CONTEXT_SAVE_AND_SELECT			5		// Save and get a copy of topmost context from numbered stack
#define CONTEXT_RESTORE_ALL				6		// Clear stack and restore to first context in stack
#define CONTEXT_CLEAR_STACK				7		// Clear stack, keeping current context
#define CONTEXT_DEBUG					0x80	// Get debug info about a context

#define CONTEXT_RESET_GPAINT			0x01	// graphics painting options
#define CONTEXT_RESET_GPOS				0x02	// graphics positioning incl graphics viewport
#define CONTEXT_RESET_TPAINT			0x04	// text painting options
#define CONTEXT_RESET_TCURSOR			0x08	// text cursor incl text viewport
#define CONTEXT_RESET_TBEHAVIOUR		0x10	// text cursor behaviour
#define CONTEXT_RESET_FONTS				0x20	// fonts
#define CONTEXT_RESET_CHAR2BITMAP		0x40	// char-to-bitmap mappings
#define CONTEXT_RESET_RESERVED			0x80	// reserved for future use

// Buffered commands
#define BUFFERED_WRITE					0x00	// Write to a numbered buffer
#define BUFFERED_CALL					0x01	// Call buffered commands
#define BUFFERED_CLEAR					0x02	// Clear buffered commands
#define BUFFERED_CREATE					0x03	// Create a new empty buffer
#define BUFFERED_SET_OUTPUT				0x04	// Set the output buffer
#define BUFFERED_ADJUST					0x05	// Adjust buffered commands
#define BUFFERED_COND_CALL				0x06	// Conditionally call a buffer
#define BUFFERED_JUMP					0x07	// Jump to a buffer
#define BUFFERED_COND_JUMP				0x08	// Conditionally jump to a buffer
#define BUFFERED_OFFSET_JUMP			0x09	// Jump to a buffer with an offset
#define BUFFERED_OFFSET_COND_JUMP		0x0A	// Conditionally jump to a buffer with an offset
#define BUFFERED_OFFSET_CALL			0x0B	// Call a buffer with an offset
#define BUFFERED_OFFSET_COND_CALL		0x0C	// Conditionally call a buffer with an offset
#define BUFFERED_COPY					0x0D	// Copy blocks from multiple buffers into one buffer
#define BUFFERED_CONSOLIDATE			0x0E	// Consolidate blocks inside a buffer into one
#define BUFFERED_SPLIT					0x0F	// Split a buffer into multiple blocks
#define BUFFERED_SPLIT_INTO				0x10	// Split a buffer into multiple blocks to new buffer(s)
#define BUFFERED_SPLIT_FROM				0x11	// Split to new buffers from a target bufferId onwards
#define BUFFERED_SPLIT_BY				0x12	// Split a buffer into multiple blocks by width (columns)
#define BUFFERED_SPLIT_BY_INTO			0x13	// Split by width into new buffer(s)
#define BUFFERED_SPLIT_BY_FROM			0x14	// Split by width to new buffers from a target bufferId onwards
#define BUFFERED_SPREAD_INTO			0x15	// Spread blocks from a buffer to multiple target buffers
#define BUFFERED_SPREAD_FROM			0x16	// Spread blocks from target buffer ID onwards
#define BUFFERED_REVERSE_BLOCKS			0x17	// Reverse the order of blocks in a buffer
#define BUFFERED_REVERSE				0x18	// Reverse the order of data in a buffer
#define BUFFERED_COPY_REF				0x19	// Copy references to blocks from multiple buffers into one buffer
#define BUFFERED_COPY_AND_CONSOLIDATE	0x1A	// Copy blocks from multiple buffers into one buffer and consolidate them
#define BUFFERED_AFFINE_TRANSFORM		0x20	// Create or combine a 3x3 2d affine transform matrix buffer
#define BUFFERED_AFFINE_TRANSFORM_3D	0x21	// Create or combine a 4x4 3d affine transform matrix buffer
#define BUFFERED_MATRIX					0x22	// Create or combine a matrix buffer of arbitrary dimensions
#define BUFFERED_TRANSFORM_BITMAP		0x28	// Create a new bitmap from an existing one by applying a 2d transform
#define BUFFERED_TRANSFORM_DATA			0x29	// Transform data using a given matrix
#define BUFFERED_READ_VARIABLE			0x30	// Read a VDP variable value into a buffer
#define BUFFERED_COMPRESS				0x40	// Compress blocks from multiple buffers into one buffer
#define BUFFERED_DECOMPRESS				0x41	// Decompress blocks from multiple buffers into one buffer
#define BUFFERED_EXPAND_BITMAP			0x48	// Expand a bitmap buffer
#define BUFFERED_ADD_CALLBACK			0x50	// Add a callback
#define BUFFERED_REMOVE_CALLBACK		0x51	// Remove a callback
// #define BUFFERED_ADD_TIMER_CALLBACK		0x52	// Add a timer callback
// #define BUFFERED_REMOVE_TIMER_CALLBACK	0x53	// Remove a timer callback

#define BUFFERED_DEBUG_INFO				0x80	// Get debug info about a buffer

// Adjust operation codes
#define ADJUST_NOT				0x00	// Adjust: NOT
#define ADJUST_NEG				0x01	// Adjust: Negative
#define ADJUST_SET				0x02	// Adjust: set new value (replace)
#define ADJUST_ADD				0x03	// Adjust: add
#define ADJUST_ADD_CARRY		0x04	// Adjust: add with carry
#define ADJUST_AND				0x05	// Adjust: AND
#define ADJUST_OR				0x06	// Adjust: OR
#define ADJUST_XOR				0x07	// Adjust: XOR

// Adjust operation flags
#define ADJUST_OP_MASK			0x0F	// operation code mask
#define ADJUST_ADVANCED_OFFSETS	0x10	// advanced, 24-bit offsets (16-bit block offset follows if top bit set)
#define ADJUST_BUFFER_VALUE		0x20	// operand is a buffer fetched value
#define ADJUST_MULTI_TARGET		0x40	// multiple target values will be adjusted
#define ADJUST_MULTI_OPERAND	0x80	// multiple operand values used for adjustments

// Conditional operation codes
#define COND_EXISTS				0x00	// Conditional: exists (non-zero value)
#define COND_NOT_EXISTS			0x01	// Conditional: NOT exists (zero value)
#define COND_EQUAL				0x02	// Conditional: equal
#define COND_NOT_EQUAL			0x03	// Conditional: not equal
#define COND_LESS				0x04	// Conditional: less than
#define COND_GREATER			0x05	// Conditional: greater than
#define COND_LESS_EQUAL			0x06	// Conditional: less than or equal
#define COND_GREATER_EQUAL		0x07	// Conditional: greater than or equal
#define COND_AND				0x08	// Conditional: AND
#define COND_OR					0x09	// Conditional: OR

// Conditional operation flags
#define COND_OP_MASK			0x0F	// conditional operation code mask
#define COND_ADVANCED_OFFSETS	0x10	// advanced offset values
#define COND_BUFFER_VALUE		0x20	// value to compare against is a buffer-fetched value
#define COND_VAR_VALUE			0x40	// condition source value is a flag
#define COND_16BIT				0x80	// values to compare are a 16-bit

// Reverse operation flags
#define REVERSE_16BIT			0x01	// 16-bit value length
#define REVERSE_32BIT			0x02	// 32-bit value length
#define REVERSE_SIZE			0x03	// when both length flags are set, a 16-bit length value follows
#define REVERSE_CHUNKED			0x04	// chunked reverse, 16-bit size value follows
#define REVERSE_BLOCK			0x08	// reverse block order
#define REVERSE_UNUSED_BITS		0xF0	// unused bits

// Expand bitmap operation flags
#define EXPAND_BITMAP_SIZE		0x07	// bottom bits indicate the number of bits per pixel in bitmap, 0=8bpp
#define EXPAND_BITMAP_ALIGNED	0x08	// includes pixel width value to indicate where a byte alignment should be performed
#define EXPAND_BITMAP_USEBUFFER	0x10	// use buffer ID for mapping data

// Affine transform operation codes
// if applying to an empty buffer, generate a matrix with the given operation
// otherwise combine the existing matrix with the given operation
#define AFFINE_IDENTITY			0		// Create/reset to an identity matrix (no arguments)
#define AFFINE_INVERT			1		// Invert (no arguments)
#define AFFINE_ROTATE			2		// Rotate (anticlockwise by angle, 1 argument for 2d, 3 arguments for 3d)
#define AFFINE_ROTATE_RAD		3		// Rotate (anticlockwise by angle in radians, 1 argument for 2d, 3 arguments for 3d)
#define AFFINE_MULTIPLY			4		// Scalar multiply (1 argument)
#define AFFINE_SCALE			5		// Scale (1 argument per dimension for X, Y and Z)
#define AFFINE_TRANSLATE		6		// Translate (1 argument per dimension)
#define AFFINE_TRANSLATE_OS_COORDS		7		// Translate (1 argument per dimension, X and Y in OS coordinates, Z un-scaled)
#define AFFINE_SHEAR			8		// Shear (1 argument per dimension)
#define AFFINE_SKEW				9		// Skew (by angle, 1 argument per dimension)
#define AFFINE_SKEW_RAD			10		// Skew (by angle in radians, 1 argument per dimension)
#define AFFINE_TRANSFORM		11		// Combine in a transform matrix (6 arguments for 2d, 12 for 3d, last row automatically identity value, or a buffer)
#define AFFINE_TRANSLATE_BITMAP	12		// Translate using bitmap size multiplier (X and Y only)

#define AFFINE_OP_MASK			0x0F	// operation code mask
#define AFFINE_OP_ADVANCED_OFFSETS	0x10	// advanced, 24-bit offsets (16-bit block offset follows if top bit set)
#define AFFINE_OP_BUFFER_VALUE		0x20	// operand values are fetched from buffers
#define AFFINE_OP_MULTI_FORMAT		0x40	// each argument has its own format byte

// Floating point value format byte
// a format of 0 would indicate a 32-bit float value - "native" for transform matrix data
// using a value of 0xC8 would indicate a 16-bit fixed point value with the binary point shifted left 8 bits (for an 8/8 split)
// a value of 0xC0 indicates 16-bit fixed point values with no fractional part
#define FLOAT_FORMAT_SHIFT_MASK	0x1F	// bits used for shift value (used for fixed point values)
#define FLOAT_FORMAT_SHIFT_TOPBIT	0x10	// top bit of shift (used to work out if shift is negative)
#define FLOAT_FORMAT_FLAGS		0xE0	// flags
#define FLOAT_FORMAT_FIXED		0x40	// if set, values are fixed-point, vs floats
#define FLOAT_FORMAT_16BIT		0x80	// if set, values are 16-bit, vs 32-bit

// Matrix operations
#define MATRIX_SET				0		// Set a new matrix with given values
#define MATRIX_SET_VALUE		1		// New matrix with single value set at row/column
#define MATRIX_FILL				2		// Fill a matrix with a given value
#define MATRIX_DIAGONAL			3		// Diagonal matrix from input values
#define MATRIX_ADD				4		// Add matrixes (target = source1 + source2)
#define MATRIX_SUBTRACT			5		// Subtract (target = source1 - source2)
#define MATRIX_MULTIPLY			6		// Multiply (target = source1 * source2)
#define MATRIX_SCALAR_MULTIPLY	7		// Scalar multiply (target = source1 * scalar)
#define MATRIX_SUBMATRIX		8		// Extract a submatrix
#define MATRIX_INSERT_ROW		9		// Insert a row
#define MATRIX_INSERT_COLUMN	10		// Insert a column
#define MATRIX_DELETE_ROW		11		// Delete a row
#define MATRIX_DELETE_COLUMN	12		// Delete a column

#define MATRIX_OP_MASK			0x0F	// operation code mask
#define MATRIX_OP_ADVANCED_OFFSETS	0x10	// advanced, 24-bit offsets (16-bit block offset follows if top bit set)
#define MATRIX_OP_BUFFER_VALUE	0x20	// operand values are fetched from buffers

// Transform bitmap flags
#define TRANSFORM_BITMAP_RESIZE		0x01	// Resize
#define TRANSFORM_BITMAP_EXPLICIT_SIZE	0x02	// Use an explicit size (width and height)
#define TRANSFORM_BITMAP_TRANSLATE	0x04	// Translate

// Transform data flags
#define TRANSFORM_DATA_HAS_SIZE		0x01	// Explicit size set (otherwise size = rows - 1)
#define TRANSFORM_DATA_HAS_OFFSET	0x02	// Has an offset to the data
#define TRANSFORM_DATA_HAS_STRIDE	0x04	// Has an explicit stride (in bytes) between value sets
#define TRANSFORM_DATA_HAS_LIMIT	0x08	// Has a limited number of data items transformed
#define TRANSFORM_DATA_ADVANCED		0x10	// Advanced offsets
#define TRANSFORM_DATA_BUFFER_ARGS	0x20	// Optional argument values are fetched from buffers
#define TRANSFORM_DATA_PER_BLOCK	0x40	// Transform data per block

// Read flag flags
#define READ_VAR_BIG_ENDIAN			0x01	// read variable value as big-endian (default is little-endian)
#define READ_VAR_ADVANCED_OFFSETS	0x10	// advanced, 24-bit offsets (16-bit block offset follows if top bit set)
#define READ_VAR_USE_DEFAULT		0x40	// use default value if flag not set
#define READ_VAR_16BIT				0x80	// value to set is 16-bit

// Buffered bitmap and sample info
#define BUFFERED_BITMAP_BASEID		0xFA00	// Base ID for buffered bitmaps
#define BUFFERED_SAMPLE_BASEID		0xFB00	// Base ID for buffered samples

// Copper commands
#define COPPER_CREATE_PALETTE		0		// Create a palette
#define COPPER_DELETE_PALLETE		1		// Delete a palette
#define COPPER_SET_PALETTE_COLOUR	2		// Set a palette item (colour)
#define COPPER_UPDATE_SIGNALLIST	3		// Update the signal list
#define COPPER_RESET_SIGNALLIST		4		// Reset the signal list

// Callback/event types
#define CALLBACK_VSYNC				0		// VSync
#define CALLBACK_MODE_CHANGE		1		// Mode changed
#define CALLBACK_KEYBOARD			2		// Keyboard hardware event
#define CALLBACK_MOUSE				3		// Mouse hardware update
#define CALLBACK_PALETTE			4		// Palette entry changed
#define CALLBACK_READPIXEL			5		// Read pixel event
#define CALLBACK_SENDING_VDPP		0x0100	// Sending VDP protocol packet (OR with PACKET_* packet type value)
#define CALLBACK_SENT_VDPP			0x0180	// Sent VDP protocol packet (OR with PACKET_* packet type value)
// Future callback types may include...
// * timer events (which may require metadata for timer duration, etc),
// * audio events (a singular audio status event probably won't be enough)
// * cursor events (movement, blink?, etc.  may need metadata to control debouncing on movement, and when events are sent)
// * paged mode events (output paused, about to resume, setting changed)

// Test flags and variables
#define TESTFLAG_AFFINE_TRANSFORM	1	// Affine transform test flag
#define TESTFLAG_HW_SPRITES			2	// Hardware sprites test flag

#define VDPVAR_FULL_DUPLEX			0x0101	// Full duplex UART comms flag
#define TESTFLAG_VDPP_BUFFERSIZE	0x0102	// Buffer size on MOS for VDP protocol packets
#define TESTFLAG_ECHO				0x0110	// Echo back received data, for redirect/spool
// #define TESTFLAG_ECHO_SETTINGS	0x0111	// Settings for what will be echo'd
#define VDPVAR_SYSTEM_BEGIN			0x0200	// General system settings start at 0x0200
#define VDPVAR_SYSTEM_END			0x02FF	// General system settings end
#define VDPVAR_RTC_YEAR				0x0200	// RTC year is 4 digits
#define VDPVAR_RTC_MONTH			0x0201	// RTC month is 1-12
#define VDPVAR_RTC_DAY				0x0202	// RTC day is 1-31
#define VDPVAR_RTC_HOUR				0x0203	// RTC hour is 0-23
#define VDPVAR_RTC_MINUTE			0x0204	// RTC minute is 0-59
#define VDPVAR_RTC_SECOND			0x0205	// RTC second is 0-59
#define VDPVAR_RTC_MILLIS			0x0206	// RTC millisecond is 0-999
#define VDPVAR_RTC_WEEKDAY			0x0207	// RTC weekday is 0-6
#define VDPVAR_RTC_YEARDAY			0x0208	// RTC day of year is 0-366
#define VDPVAR_FREEPSRAM_LOW		0x0210	// Free PSRAM low bytes
#define VDPVAR_FREEPSRAM_HIGH		0x0211	// Free PSRAM high bytes
#define VDPVAR_BUFFERS_USED			0x0212	// Number of buffers used
#define VDPVAR_KEYBOARD_LAYOUT		0x0220	// Keyboard layout
#define VDPVAR_KEYBOARD_CTRL_KEYS	0x0221	// Control keys on/off
#define VDPVAR_KEYBOARD_REP_DELAY	0x0222	// Keyboard repeat delay (milliseconds)
#define VDPVAR_KEYBOARD_REP_RATE	0x0223	// Keyboard repeat rate (characters per second)
#define VDPVAR_KEYBOARD_LED			0x0224	// Keyboard LED status (bitmask, combined)
#define VDPVAR_KEYBOARD_LED_NUM		0x0225	// Keyboard Num Lock LED status (1=on, 0=off)
#define VDPVAR_KEYBOARD_LED_CAPS	0x0226	// Keyboard Caps Lock LED status (1=on, 0=off)
#define VDPVAR_KEYBOARD_LED_SCROLL	0x0227	// Keyboard Scroll Lock LED status (1=on, 0=off)
#define VDPVAR_CONTEXT_ID			0x0230	// Current active context ID
#define VDPVAR_MOUSE_CURSOR			0x0240	// Mouse cursor ID
#define VDPVAR_MOUSE_ENABLED		0x0241	// Mouse enabled/disabled
#define VDPVAR_MOUSE_XPOS			0x0242	// Mouse X position (pixel coords)
#define VDPVAR_MOUSE_YPOS			0x0243	// Mouse Y position (pixel coords)
#define VDPVAR_MOUSE_BUTTONS		0x0244	// Mouse button state (bitmask)
#define VDPVAR_MOUSE_WHEEL			0x0245	// Mouse wheel
#define VDPVAR_MOUSE_SAMPLERATE		0x0246	// Mouse sample rate
#define VDPVAR_MOUSE_RESOLUTION		0x0247	// Mouse resolution
#define VDPVAR_MOUSE_SCALING		0x0248	// Mouse scaling
#define VDPVAR_MOUSE_ACCELERATION	0x0249	// Mouse acceleration
#define VDPVAR_MOUSE_WHEELACC		0x024A	// Mouse wheel acceleration
#define VDPVAR_MOUSE_VISIBLE		0x024B	// Mouse cursor visible (1) or hidden (0)
// Flags 0x024C-0x024F reserved for mouse area
#define VDPVAR_MOUSE_XPOS_OS		0x0250	// Mouse X position (OS coords)
#define VDPVAR_MOUSE_YPOS_OS		0x0251	// Mouse Y position (OS coords)
#define VDPVAR_MOUSE_DELTAX			0x0252	// Mouse delta X (pixel coords)
#define VDPVAR_MOUSE_DELTAY			0x0253	// Mouse delta Y (pixel coords)
#define VDPVAR_MOUSE_DELTAX_OS		0x0254	// Mouse delta X (OS coords)
#define VDPVAR_MOUSE_DELTAY_OS		0x0255	// Mouse delta Y (OS coords)
#define VDPVAR_KEYEVENT_KEYCODE		0x0260	// Keycode (includes modifications) (upper byte raw ASCII from VirtualKeyItem)
#define VDPVAR_KEYEVENT_VK			0x0261	// FabGL Virtual keycode
#define VDPVAR_KEYEVENT_DOWN		0x0262	// Key down flag (1) or up (0)
#define VDPVAR_KEYEVENT_MODIFIERS	0x0263	// Key modifiers byte (Shift, Ctrl, Alt, etc)
#define VDPVAR_KEYEVENT_CTRL		0x0264	// CTRL key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_SHIFT		0x0265	// SHIFT key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_LALT		0x0266	// LEFT ALT key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_RALT		0x0267	// RIGHT ALT key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_CAPSLOCK	0x0268	// CAPSLOCK key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_NUMLOCK		0x0269	// NUMLOCK key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_SCROLLLOCK	0x026A	// SCROLLLOCK key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_GUI			0x026B	// GUI key state (1=pressed, 0=not pressed)
#define VDPVAR_KEYEVENT_SCANCODE1	0x026C	// Key scancode bytes 1 and 2
#define VDPVAR_KEYEVENT_SCANCODE2	0x026D	// Key scancode bytes 3 and 4
#define VDPVAR_KEYEVENT_SCANCODE3	0x026E	// Key scancode bytes 5 and 6
#define VDPVAR_KEYEVENT_SCANCODE4	0x026F	// Key scancode bytes 7 and 8
#define TESTFLAG_TILE_ENGINE		0x0300	// Tile engine flag (layers commands)
#define VDPVAR_COPPER				0x0310	// Copper feature flag
#define VDPVAR_AUTO_HW_SPRITES		0x0400	// Auto hardware sprites flag
#define VDPVAR_LAST_CHARACTER_READ	0x0500	// Last character read from screen
#define VDPVAR_LAST_COLOUR_RED		0x0510	// Last colour read (red component)
#define VDPVAR_LAST_COLOUR_GREEN	0x0511	// Last colour read (green component)
#define VDPVAR_LAST_COLOUR_BLUE		0x0512	// Last colour read (blue component)
#define VDPVAR_LAST_COLOUR_LOGICAL	0x0513	// Last colour read (logical colour / palette index)
#define VDPVAR_LAST_COLOUR_PHYSICAL	0x0514	// Last colour read (physical colour / RGB222 value)
#define VDPVAR_LAST_COLOUR_X		0x0515	// Last colour read (X coordinate)
#define VDPVAR_LAST_COLOUR_Y		0x0516	// Last colour read (Y coordinate)
#define VDPVAR_KEYMAP_START			0x0800	// Keymap start (block of 256)
#define VDPVAR_VDU_VARIABLES_START	0x1000	// VDU variables start at 0x1000
#define VDPVAR_VDU_VARIABLES_END	0x1FFF	// VDU variables end
#define VDPVAR_VDU_VARIABLES_MASK	0x0FFF	// VDU variables mask

// VDU variables (add 0x1000 to these for the actual VDP variable numbers)
#define VDU_VAR_PALETTE				0x0200	// Palette variables start (block of 64)
#define VDU_VAR_PALETTE_END			0x023F	// Palette variables end
#define VDU_VAR_CHARMAPPING			0x0300	// Character to bitmap mapping start (block of 256)
#define VDU_VAR_CHARMAPPING_END		0x03FF	// Character to bitmap mapping end

#define LOGICAL_SCRW				1280	// As per the BBC Micro standard
#define LOGICAL_SCRH				1024

// Function Prototypes
//
void debug_log(const char *format, ...);

void force_debug_log(const char *format, ...);

// Terminal states
//
enum class TerminalState {
	Disabled,
	Disabling,
	Enabling,
	Enabled,
	Suspending,
	Suspended,
	Resuming
};

// VDU command processor states
//
enum class VDUProcessorState {
	Active,					// Available to process commands
	WaitingForFrames,		// Waiting for a number of frames to be processed
	PagedModePaused,		// Paused in paged mode
	CtrlShiftPaused			// Paused in Ctrl+Shift mode
};

// Paged mode states
//
enum class PagedMode : uint8_t {
	Disabled = 0,
	Enabled = 1,
	TempEnabled_Disabled = 2,
	TempEnabled_Enabled = 3,
};

// Additional modelines
//
#ifndef VGA_640x240_60Hz
#define VGA_640x240_60Hz	"\"640x240@60Hz\" 25.175 640 656 752 800 240 245 246 262 -HSync -VSync DoubleScan"
#endif
