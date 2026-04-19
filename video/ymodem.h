#ifndef YMODEM_H
#define YMODEM_H

#include <stdlib.h>
#include <string.h>
#include "CRC16.h"
#include "CRC32.h"

// YMODEM protocol constants
#define YMODEM_MAX_NAME_LENGTH         100
#define YMODEM_BLOCK_SEQ_INDEX         1
#define YMODEM_BLOCK_SEQ_COMP_INDEX    2
#define YMODEM_BLOCK_HEADER            3
#define YMODEM_BLOCK_TRAILER           2
#define YMODEM_BLOCK_OVERHEAD          (YMODEM_BLOCK_HEADER + YMODEM_BLOCK_TRAILER)
#define YMODEM_BLOCKSIZE_128           128
#define YMODEM_BLOCKSIZE_1K            1024
#define YMODEM_MOS_BLOCK               1024
#define YMODEM_FILESIZEDATA_LENGTH     16
#define YMODEM_MAXFILES                128
#define YMODEM_SOH                     0x01  // 128 byte data block
#define YMODEM_STX                     0x02  // 1024 byte data block
#define YMODEM_EOT                     0x04
#define YMODEM_ACK                     0x06
#define YMODEM_NAK                     0x15
#define YMODEM_CAN                     0x18
#define YMODEM_DEFCRC16                0x43
#define YMODEM_TIMEOUT                 1200
#define YMODEM_FLUSHTIME               200
#define YMODEM_MAX_ERRORS              32
#define YMODEM_MAX_RETRY               3

// Global variables
extern HardwareSerial DBGSerial;
bool                  ymodem_session_aborted;
static uint8_t        ymodem_fullblockbuffer[1+ YMODEM_BLOCKSIZE_1K + YMODEM_BLOCK_OVERHEAD];  // header + seq + ~seq + data + CRC
static uint8_t        ymodem_tmpbuffer[YMODEM_BLOCKSIZE_1K];                                   // padded block buffer
static uint8_t        ymodem_block0[YMODEM_BLOCKSIZE_128];

typedef struct {
  char *buffer;
  char *bufptr;
  char *filename;
  size_t filesize;
  size_t received;
} ymodem_fileinfo_t;

typedef struct{
  uint8_t  *data = ymodem_fullblockbuffer;
  uint8_t   blocktype;
  uint8_t   blocknumber;
  uint32_t  length;
  uint32_t  filesize;
  char      filename[YMODEM_MAX_NAME_LENGTH];
  bool      timed_out;
  bool      end_of_batch;
  bool      crc_verified;
  bool      correct_blocknumber;
} ymodem_block_t;

// Read a single byte from the external serial port, until timeout
static bool serialRx_byte_t (uint8_t *c, uint32_t timeout_ms) {
  uint32_t timeReceived = millis();

  while(millis() - timeReceived < timeout_ms) {
    if(DBGSerial.available()) {
      *c = DBGSerial.read();
      return true;
    }    
  }
  return false;
}

static void send_ack (void) {
  DBGSerial.write(YMODEM_ACK);
}
static void send_nak (void) {
  DBGSerial.write(YMODEM_NAK);
}
static void send_reqcrc (void) {
  DBGSerial.write(YMODEM_DEFCRC16);
}
static void send_abort (void) {
  DBGSerial.write(YMODEM_CAN);
  DBGSerial.write(YMODEM_CAN);
}

// Eat all uart RX during a specific time period
static void uart_flush(void) {
  uint32_t timeReceived = millis();

  while(millis() - timeReceived < YMODEM_FLUSHTIME) {
    if(DBGSerial.available()) {
      DBGSerial.read();
    }    
  }
  return;
}

static int io_write(const uint8_t *data, int len) {
  while(len) {
    DBGSerial.write(*data++);
    len--;
  }
  return len;
}

static void wipe32chars_restartline(void) {
  printFmt("\r                                \r");
}

void VDUStreamProcessor::sendKeycodeUINT32_T(uint32_t value) {
  uint8_t packet[] = {0, 0};
  uint8_t shiftbits = 0;

  for(int i = 0; i < 4; i++) {
    packet[0] = ((uint8_t)(value >> shiftbits) & 0xFF);
    shiftbits += 8;
    send_packet(PACKET_KEYCODE, sizeof(packet), packet);
  }
}

uint32_t VDUStreamProcessor::receiveKeycodeUINT32() {
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        result |= uint32_t(readByte_b()) << (i * 8);
    }
    return result;
}


void VDUStreamProcessor::sendKeycodeBytestream(const char *ptr, uint32_t length) {
  uint8_t packet[] = {0, 0};
  uint8_t shiftbits = 0;

  for(int i = 0; i < length; i++) {
    packet[0] = *ptr++;
    send_packet(PACKET_KEYCODE, sizeof(packet), packet);
  }
}

void VDUStreamProcessor::receiveKeycodeBytestream(char *ptr, uint32_t length) {
  for(int i = 0; i < length; i++) {
    *ptr++ = readByte_b();
  }
}

static bool is_end_of_batch(ymodem_block_t *block) {
  return ((block->length > YMODEM_BLOCK_HEADER) && (block->blocknumber == 0) && (block->data[YMODEM_BLOCK_HEADER] == 0));
}

static void get_block(ymodem_block_t *block, uint8_t blocknumber) {
  auto kb = getKeyboard();
	fabgl::VirtualKeyItem item;
  CRC16 crc16result(0x1021); // Ymodem uses CRC-16-CCITT polynomial
  int bytecount, block_size;
  char file_length_data[YMODEM_BLOCKSIZE_128];
  uint8_t input_byte;
  uint8_t *data_start = block->data;
  uint8_t *data = block->data;

  block->length = 0;
  block->timed_out = false;
  block->crc_verified = false;
  block->correct_blocknumber = false;
  block->blocknumber = 0;
  block->end_of_batch = false;

  if (kb->getNextVirtualKey(&item, 0)) {
		if(item.down) {
			if(item.ASCII == 0x1B) {
				ymodem_session_aborted = true;
				return;
			}
		}
	}

  if (serialRx_byte_t(&input_byte, YMODEM_TIMEOUT) == false) {
    block->timed_out = true;
    return;
  }
  block->length = 1;
  block->blocktype = input_byte;

  switch (input_byte) {
    case YMODEM_SOH:
      block_size = YMODEM_BLOCKSIZE_128;
      bytecount = YMODEM_BLOCKSIZE_128 + YMODEM_BLOCK_OVERHEAD - 1;
  		break;
    case YMODEM_STX:
      block_size = YMODEM_BLOCKSIZE_1K;
      bytecount = YMODEM_BLOCKSIZE_1K + YMODEM_BLOCK_OVERHEAD - 1;
      break;
    case YMODEM_EOT:
    case YMODEM_CAN:
    default:
      return;
  }

  *data++ = input_byte;
  for (int i = 0; i < bytecount; i++) {
	  if (serialRx_byte_t(&input_byte, YMODEM_TIMEOUT) == false) {
      block->timed_out = true;
      block->end_of_batch = is_end_of_batch(block);
      return; // block incomplete
    }
    block->length++;
	  *data++ = input_byte;
  }
  // complete block
  
  // check crc
  crc16result.restart();
  crc16result.add(&data_start[YMODEM_BLOCK_HEADER], block_size + YMODEM_BLOCK_TRAILER);
  block->crc_verified = (crc16result.calc() == 0);

  // check blocknumber
  block->blocknumber = block->data[YMODEM_BLOCK_SEQ_INDEX];
  block->correct_blocknumber = (block->blocknumber == blocknumber);
  block->correct_blocknumber = block->correct_blocknumber && (block->data[YMODEM_BLOCK_SEQ_COMP_INDEX] == (255 - block->blocknumber));
  
  size_t i;
  uint8_t *tmp;
  // parse header filename
  for (i = 0, tmp = block->data + YMODEM_BLOCK_HEADER; ((*tmp != 0) && (i < YMODEM_MAX_NAME_LENGTH)); i++) block->filename[i] = *tmp++;
  block->filename[i] = 0;

  // parse header filesize
  while((*tmp !=0) && (i < block->length)) tmp++;
  for (i = 0, tmp++; (*tmp != ' ') && (i < YMODEM_FILESIZEDATA_LENGTH);) file_length_data[i++] = *tmp++;
  file_length_data[i] = 0;
  if (strlen(file_length_data) > 0) block->filesize = strtol(file_length_data, NULL, 10);
  else block->filesize = 0;

  // check end-of-batch
  block->end_of_batch = is_end_of_batch(block);

  return;  
}

class MOS_YmodemSession {
  public:
    MOS_YmodemSession(VDUStreamProcessor* obj);
   ~MOS_YmodemSession();
    bool open(void);
    void close(const char *message);
    
    void debug(void);

    bool addFile(const char* filename, size_t filesize);
    bool addData(const uint8_t *data, size_t length);
    bool writeFiles(void); // Sends all stored files to the YMODEM utility
    bool readFiles(void); // Reads all files from the YMODEM utility to memory
    size_t getFilecount(void);
    size_t getFilesize(void);

    size_t getFilesize(size_t index);
    const char *getFilename(size_t index);
    const char *getFiledata(size_t index);

  private:
  VDUStreamProcessor *vsp;
  void readData(size_t length); // reads data from the YMODEM utility

  size_t _filecount;
  ymodem_fileinfo_t *files;
};

const char * MOS_YmodemSession::getFiledata(size_t index) {
  if(index >= _filecount) return NULL;
  return files[index].buffer;
}

size_t MOS_YmodemSession::getFilesize(unsigned index) {
  if(index >= _filecount) return 0;

  return files[index].filesize;
}

const char * MOS_YmodemSession::getFilename(unsigned index) {
  if(index >= _filecount) return NULL;

  return files[index].filename;
}

void MOS_YmodemSession::debug(void) {
  CRC32 crc;

  printFmt("Current session data:\r\n");
  for(int i = 0; i < _filecount; i++) {
    crc.restart();
    crc.add((const uint8_t*)(files[i].buffer), files[i].filesize);
    printFmt("%s (0x%08X) %d bytes\r\n", files[i].filename, crc.calc(), files[i].filesize);
  }
}

MOS_YmodemSession::MOS_YmodemSession(VDUStreamProcessor* obj) { 
  _filecount = 0; 
  vsp = obj;
  files = (ymodem_fileinfo_t *)heap_caps_malloc(YMODEM_MAXFILES * sizeof(ymodem_fileinfo_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(!files) throw std::runtime_error("Failed to allocate PSRAM");
}

MOS_YmodemSession::~MOS_YmodemSession() {
  if(!files) return;
  
  for(int i = 0; i < _filecount; i++) {
    free(files[i].buffer);
    free(files[i].filename);
  }
  free(files);
}

size_t MOS_YmodemSession::getFilecount(void) {
  return _filecount;
}

bool MOS_YmodemSession::readFiles(void) {
  size_t count, filename_length, filesize, crcexpected;
  char filename[YMODEM_MAX_NAME_LENGTH + 1];
  CRC32 crc;
  printFmt("Reading file(s)...");

  while(1) {
    count = vsp->receiveKeycodeUINT32();
    if(count == 0) break;
    filename_length = vsp->receiveKeycodeUINT32();
    vsp->receiveKeycodeBytestream(filename, filename_length);
    filename[filename_length] = 0;
    filesize = vsp->receiveKeycodeUINT32();
    addFile(basename(filename), filesize);
    readData(filesize);
    crcexpected = vsp->receiveKeycodeUINT32();    
    crc.restart();
    crc.add((uint8_t*)(files[_filecount-1].buffer), filesize);
    if(crc.calc() != crcexpected) {
      printFmt("\r\nCRC32 error - %s\r\n", filename);
      return false;
    }
  }
  printFmt("\r\n");
  return true;
}

bool MOS_YmodemSession::writeFiles(void) {
  size_t write_len, remaining, totalbytes, progress;
  char *dataptr;
  CRC32 crc;

  if(_filecount == 0) return false;
  // Check if the last file is done. Delete it from writing if not.
  if(files[_filecount-1].filesize != files[_filecount-1].received) {
    _filecount--;
    free(files[_filecount].buffer);
    free(files[_filecount].filename);
  }
  if(_filecount == 0) return false; // might have deleted the last file previously


  switch(_filecount) {
    case 0: return true;
    case 1:
      wipe32chars_restartline();
      printFmt("\nWriting file - ");
      break;
    default:
      wipe32chars_restartline();
      printFmt("\nWriting %d files - ", _filecount);
  }

  totalbytes = 0;
  progress = 0;
  for(int i = 0; i < _filecount; i++) totalbytes += files[i].filesize;

  for(int i = 0; i < _filecount; i++) {
    ymodem_fileinfo_t &f = files[i];
    
    // Signal incoming file
    vsp->sendKeycodeByte(1, false);
    vsp->sendKeycodeUINT32_T(strlen(f.filename));
    vsp->sendKeycodeBytestream(f.filename, strlen(f.filename));
    vsp->sendKeycodeUINT32_T(f.filesize);

    while(vsp->readByte_b() != 'S');
    if(vsp->readByte_b() != '1') {
      printFmt("\r\nError writing '%s'\r\n", f.filename);
      return false;
    };
    
    // Send data in multiple blocks
    remaining = f.filesize;
    dataptr = f.buffer;
    while(remaining) {
      if(remaining > YMODEM_MOS_BLOCK) write_len = YMODEM_MOS_BLOCK;
      else write_len = remaining;

      vsp->sendKeycodeByte(2,false); // Signal data block
      vsp->sendKeycodeUINT32_T(write_len);
      vsp->sendKeycodeBytestream(dataptr, write_len); // Send payload
      while(vsp->readByte_b() != 'S');
      if(vsp->readByte_b() != '2') {
        printFmt("\r\nError writing data to '%s'\r\n",f.filename);
        return false;
      }
      progress += write_len;
      if(_filecount == 1) {
        printFmt("\rWriting file - %3.0f%%", ((float)progress / (float)totalbytes) * 100);
      }
      else {
        printFmt("\rWriting %d files - %3.0f%%", _filecount, ((float)progress / (float)totalbytes) * 100);
      }
      dataptr += write_len;
      remaining -= write_len;
    }
    crc.restart();
    crc.add((uint8_t *)f.buffer, f.filesize);

    // Remote check CRC32
    vsp->sendKeycodeByte(3, false);
    vsp->sendKeycodeUINT32_T(crc.calc());
    while(vsp->readByte_b() != 'S');
    if(vsp->readByte_b() != 'V') {
      printFmt("\r\nCRC32 error - %s\r\n", f.filename);
      return false;
    }
    // Remote close file
    vsp->sendKeycodeByte(4, false);
    while(vsp->readByte_b() != 'S');
    while(vsp->readByte_b() != '4');
  }
  return true;
}

size_t MOS_YmodemSession::getFilesize(void) {
  return files[_filecount - 1].filesize;
}

bool MOS_YmodemSession::open(void) {
  vsp->sendKeycodeByte('C',false);
  return true;
}

bool MOS_YmodemSession::addFile(const char* filename, size_t filesize) {
  ymodem_fileinfo_t &f = files[_filecount];

  if(_filecount == YMODEM_MAXFILES) return false;

  f.buffer = (char *)heap_caps_malloc(filesize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(f.buffer == NULL) return false;

  f.filename = (char *)heap_caps_malloc(strlen(filename) + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(f.filename == NULL) {
    free(f.buffer);
    return false;
  }

  strcpy(f.filename, filename);
  f.bufptr = f.buffer;
  f.filesize = filesize;
  f.received = 0;
  _filecount++;

  return true;
}

void MOS_YmodemSession::close(const char *message) {
  printFmt("%s", message);
  vsp->sendKeycodeByte(0, false); // Done
}

bool MOS_YmodemSession::addData(const uint8_t *data, size_t length) {
  ymodem_fileinfo_t &f = files[_filecount - 1];

  size_t used = f.bufptr - f.buffer;
  if (used + length > f.filesize) {
      return false;  // prevent heap corruption
  }

  memcpy(f.bufptr, data, length);
  f.bufptr += length;
  f.received += length;
  return true;
}

void MOS_YmodemSession::readData(size_t length) {
  ymodem_fileinfo_t &f = files[_filecount - 1];

  size_t used = f.bufptr - f.buffer;
  if (used + length > f.filesize) {
      return;  // prevent heap corruption
  }

  vsp->receiveKeycodeBytestream(f.bufptr, length);

  f.bufptr += length;
  f.received += length;
  return;
}

//---------------------------------------------------------------
// ymodem_block0 (filename + size) - 128 bytes
//---------------------------------------------------------------
static void make_ymodem_block0(uint8_t *buf, const char *filename, uint32_t filesize) {
    memset(buf, 0, 128);  // clear block

    size_t pos = 0;

    // --- Copy filename ---
    if(filename && filename[0]) {
        size_t flen = strlen(filename);
        if(flen > 100) flen = 100;  
        memcpy(buf + pos, filename, flen);
        pos += flen;
    }
    buf[pos++] = '\0';  // single null terminator after filename

    // --- Copy filesize in ASCII ---
    int n = snprintf((char*)(buf + pos), 128 - pos, "%lu", (unsigned long)filesize);
    pos += n;
    buf[pos++] = ' ';  // <--- Python expects a SPACE after filesize, not \0

    // --- Copy mode in proper octal ---
    const char *mode = "0600";  // leading 0 = octal
    size_t mlen = strlen(mode);
    memcpy(buf + pos, mode, mlen);
    pos += mlen;
    buf[pos++] = '\0';  // null terminator after mode

    // --- Remaining bytes zeroed by memset ---
}

//---------------------------------------------------------------
// Send block (128 or 1024 bytes)
// This only sends HEADER + seqnum + ~seqnum + data + CRC.
//---------------------------------------------------------------
static int send_block(uint8_t header, uint8_t block_num, const uint8_t *data, uint16_t data_len, uint16_t block_size) {
  int p = 0;

  // --- prepare padded block ---
  memset(ymodem_tmpbuffer, 0x1A, block_size);           // pad with CTRL-Z

  if (data_len > 0 && data_len <= block_size)
      memcpy(ymodem_tmpbuffer, data, data_len);         // copy actual data


  // --- compute CRC over full padded block ---
  CRC16 crc(0x1021);
  crc.restart();
  crc.add(ymodem_tmpbuffer, block_size);
  uint16_t crc_val = crc.calc();

  // --- send header ---
  ymodem_fullblockbuffer[p++] = header;       // SOH or STX
  ymodem_fullblockbuffer[p++] = block_num;
  ymodem_fullblockbuffer[p++] = 255 - block_num;

  // --- send data ---
  memcpy(&ymodem_fullblockbuffer[p], ymodem_tmpbuffer, block_size);
  p += block_size;

  // --- send CRC16 ---
  ymodem_fullblockbuffer[p++] = (crc_val >> 8) & 0xFF;  // high byte
  ymodem_fullblockbuffer[p++] = crc_val & 0xFF;         // low byte

  return io_write(ymodem_fullblockbuffer, p);
}

void VDUStreamProcessor::vdu_sys_ymodem_send(void) {
  MOS_YmodemSession session(this);
  auto kb = getKeyboard();
  fabgl::VirtualKeyItem item;
  uint8_t rx;
  uint32_t offset;
  uint8_t blocknumber;
  int retry;
  bool startup = true;

  ymodem_session_aborted = 0;

  uart_flush();
  if (!session.open()) return;
  if (!session.readFiles()) { session.close("File Error\r\n"); return; }

  printFmt("Waiting for receiver - VDP:%d 8N1 (YMODEM-1K)", SERIALBAUDRATE);

  // --- Wait for initial 'C' ---
  while(1) {
    if(serialRx_byte_t(&rx, 100) && rx == YMODEM_DEFCRC16) break;
    if (kb->getNextVirtualKey(&item, 0)) {
      if(item.down) {
        if(item.ASCII == 0x1B) {
          ymodem_session_aborted = true;
          session.close("\r\nAborted\r\n");
          return;
        }
      }
    }
  }
  printFmt("\r\nSending data\r\n\r\n");

  for (int filecounter = 0; filecounter < session.getFilecount(); filecounter++) {
    const char* filename = session.getFilename(filecounter);
    uint32_t filesize = session.getFilesize(filecounter);
    wipe32chars_restartline();
    printFmt("%d - %s\r\n", filecounter+1, filename);

    // --- Wait for 'C' to start subsequent block 0
    if(!startup) {
      for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
          if (serialRx_byte_t(&rx, YMODEM_TIMEOUT) && rx == YMODEM_DEFCRC16) break;
      }
      if (retry >= YMODEM_MAX_RETRY) { session.close("\r\nMax retries\r\n"); return; }
    }
    else startup = false;

    // --- Send ymodem_block0 ---
    make_ymodem_block0(ymodem_block0, filename, filesize);
    for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
        send_block(YMODEM_SOH, 0, ymodem_block0, 128, 128);
        if (serialRx_byte_t(&rx, YMODEM_TIMEOUT)) {
            if (rx == YMODEM_ACK) break;
            if (rx == YMODEM_CAN) { session.close("\r\nReceiver aborts\r\n"); return; }
        }
    }
    if (retry >= YMODEM_MAX_RETRY) { session.close("\r\nMax retries\r\n"); return; }

    // --- Wait for 'C' to start data blocks ---
    for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
        if (serialRx_byte_t(&rx, YMODEM_TIMEOUT) && rx == YMODEM_DEFCRC16) break;
    }
    if (retry >= YMODEM_MAX_RETRY) { session.close("\r\nMax retries\r\n"); return; }

    // --- Send file data ---
    // First send as many 1K (STX) blocks as possible.
    // Then send the remainder in 128-byte (SOH) blocks as older clients like rz expect.
    offset = 0;
    blocknumber = 1;

    // --- Send full 1K STX blocks ---
    while ((filesize - offset) >= YMODEM_BLOCKSIZE_1K) {
        for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
            send_block(YMODEM_STX,
                      blocknumber,
                      (uint8_t *)session.getFiledata(filecounter) + offset,
                      YMODEM_BLOCKSIZE_1K,
                      YMODEM_BLOCKSIZE_1K);

            if (serialRx_byte_t(&rx, YMODEM_TIMEOUT)) {
                if (rx == YMODEM_ACK) {
                    offset += YMODEM_BLOCKSIZE_1K;
                    blocknumber++;
                    break;
                }
                if (rx == YMODEM_CAN) {
                    session.close("Receiver aborts\r\n");
                    return;
                }
            }
        }
        if (retry >= YMODEM_MAX_RETRY) {
            session.close("\r\nMax retries\r\n");
            return;
        }
        printFmt("\r%d/%d", offset, filesize);
    }
    // --- Send remainder using 128-byte SOH blocks ---
    uint32_t remaining = filesize - offset;
    while (remaining > 0) {
        uint16_t chunk = (remaining > YMODEM_BLOCKSIZE_128)
                            ? YMODEM_BLOCKSIZE_128
                            : remaining;

        for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
            send_block(YMODEM_SOH,
                      blocknumber,
                      (uint8_t *)session.getFiledata(filecounter) + offset,
                      chunk,                     // actual data length
                      YMODEM_BLOCKSIZE_128);    // pad to 128 bytes

            if (serialRx_byte_t(&rx, YMODEM_TIMEOUT)) {
                if (rx == YMODEM_ACK) {
                    offset += chunk;
                    remaining -= chunk;
                    blocknumber++;
                    break;
                }
                if (rx == YMODEM_CAN) {
                    session.close("Receiver aborts\r\n");
                    return;
                }
            }
        }
        if (retry >= YMODEM_MAX_RETRY) {
            session.close("\r\nMax retries\r\n");
            return;
        }
        printFmt("\r%d/%d", offset, filesize);
    }

    // --- Send EOT ---
    uint8_t eot = YMODEM_EOT;
    for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
        io_write(&eot, 1);
        if (serialRx_byte_t(&rx, YMODEM_TIMEOUT) && rx == YMODEM_ACK) break;
    }
    if (retry >= YMODEM_MAX_RETRY) { session.close("\r\nMax retries\r\n"); return; }  
  }

  // --- Wait for final 'C' for ymodem_block0
  for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
      if (serialRx_byte_t(&rx, YMODEM_TIMEOUT) && rx == YMODEM_DEFCRC16) break;
  }
  if (retry >= YMODEM_MAX_RETRY) { session.close("\r\nMax retries\r\n"); return; }
  
  // --- Send final empty ymodem_block0 safely ---
  memset(ymodem_block0, 0, sizeof(ymodem_block0));
  for (retry = 0; retry < YMODEM_MAX_RETRY; retry++) {
      send_block(YMODEM_SOH, 0, ymodem_block0, 128, 128);  // send at least 1 zero byte
      if (serialRx_byte_t(&rx, YMODEM_TIMEOUT) && rx == YMODEM_ACK) break;
  }
  
  wipe32chars_restartline();
  session.close("\r\nDone\r\n");
}

void VDUStreamProcessor::vdu_sys_ymodem_receive(void) {
  MOS_YmodemSession session(this);
  bool session_done = false;
  bool receiving_data = false;
  size_t errors = 0,timeout_counter = 0;
  size_t offset = 0,write_len;
  uint8_t blocknumber = 0;
  uint8_t cancel_counter;
  ymodem_block_t block;

  ymodem_session_aborted = false;

  if(!session.open()) return;

  uart_flush();
  printFmt("Receiving data - VDP:%d 8N1 (YMODEM-1K)\r\n\r\n", SERIALBAUDRATE);
  send_reqcrc();

  while(!session_done && !ymodem_session_aborted) {
    get_block(&block, blocknumber);
    if(block.length == 0) {
      if(blocknumber && (++timeout_counter > (YMODEM_MAX_RETRY))) {
        printFmt("\r\nTimeout\r\n");
        ymodem_session_aborted = true;
      }
      else send_reqcrc();
      continue;
    }

    timeout_counter = 0;
    if(block.blocktype != YMODEM_CAN) cancel_counter = 0;

    switch(block.blocktype) {
      case YMODEM_SOH:
      case YMODEM_STX:
        // Check for 'empty' block 0 block, might be early timed out
        if(!receiving_data && block.end_of_batch) {
          session_done = true;
          send_ack();
          break;
        }
        // Check for corrupted, smaller than required blocks
        if(block.timed_out) {
          errors++;
          break;
        }
        if(block.crc_verified && (block.correct_blocknumber)) {
          send_ack();
          if((!receiving_data) && (block.blocknumber == 0)) {
            // Header block
            if(!session.addFile(block.filename, block.filesize)) {
              printFmt("\r\nError allocating memory\r\n");
              ymodem_session_aborted = true;
            }
            wipe32chars_restartline();
            printFmt("%d - %s\r\n", session.getFilecount(), block.filename);
            send_reqcrc();
            receiving_data = true;
            offset = 0;
          }
          else {
            // Data block
            offset += block.length - YMODEM_BLOCK_OVERHEAD;  // total bytes received
            if (offset > session.getFilesize()) {
              write_len = block.length - YMODEM_BLOCK_OVERHEAD - (offset - session.getFilesize());
              offset = session.getFilesize();
            }
            else write_len = block.length - YMODEM_BLOCK_OVERHEAD;
            session.addData(block.data + YMODEM_BLOCK_HEADER, write_len);
            printFmt("\r%d/%d", offset, session.getFilesize());
          }
          blocknumber++;
        }
        else send_nak();
        break;
      case YMODEM_EOT:
        send_ack();
        receiving_data = false;
        blocknumber = 0;
        offset = 0;
        send_reqcrc();
        break;
      case YMODEM_CAN:
        if(++cancel_counter > 1) {
          printFmt("\r\nRemote abort\r\n");
          ymodem_session_aborted = true;
        }
        break;
      default:
        errors++;
    }
    if(errors > YMODEM_MAX_ERRORS) {
      printFmt("\r\nMax errors\r\n");
      ymodem_session_aborted = true;
    }
  }
  if(ymodem_session_aborted) send_abort();
  session.writeFiles();
  session.close("\r\nDone\r\n");
  uart_flush();
}

#endif // YMODEM_H
