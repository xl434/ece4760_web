/**
 * ECE 4760 Final Project: Guitar Looper
 * Demo Date: 12/6/2023
 * Authors: Alina Wang(lw584), Xiaoyu Liang(xl434), Yaqi Gao(yg298)
 *
 *
 *
 * HARDWARE CONNECTIONS
 * SERIAL
 * - GPIO 0 (TX)    -->     UART RX (white)
 * - GPIO 1 (RX)    -->     UART TX (green)
 * - RP2040 GND     -->     UART GND
 *
 * DAC
 * - GPIO 5 (CS)    -->     CS
 * - GPIO 6 (SCK)   -->     SCK
 * - GPIO 7 (MOSI)  -->     SDI
 * - GPIO 8 (LDAC)  -->     LDAC
 * - RP2040 GND     -->     VSS
 * - 3.3v           -->     VCC
 *                          VOUT_A  --> audio jack right
 *                          VOUT_B  --> audio jack middle
 *
 * ADC
 * - GPIO 26 (ADC 0)  -->   AMPLIFIER VOUT
 *
 * TRACK CONTROL BUTTONS
 * - GPIO 2   -- play/record
 * - GPIO 3   -- pause
 * - GPIO 15  -- clean
 *
 * TRACK STATUS INDICATORS
 * - GPIO 27 -- 330 ohm --> RED LED (recording)
 * - GPIO 28 -- 330 ohm --> GREEN LED (playing)
 * - GPIO 4  -- 330 ohm --> YELLOW LED (new track)
 *
 * FRAM
 * - VBUS (pin40) --> FRAM Vin
 * - GND          --> FRAM GND
 *                    FRAM 3v3 --> FRAM WP, FRAM HOLD
 * - GPIO 10 --> FRAM SCK
 * - GPIO 11 --> FRAM MOSI
 * - GPIO 12 --> FRAM MISO
 * - GPIO 13 --> FRAM CS
 *
 */

// c libraries
#include <stdio.h>
#include <math.h>
#include <string.h>

// pico libraries
#include "pico/stdlib.h"
#include "pico/multicore.h"

// hardware libraries
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

// Include protothreads
#include "pt_cornell_rp2040_v1.h"

// custom library
#include "pin_out.h"

// ==================FRAM=====================

// chip select macros
#define CS_on gpio_put(FRAM_CS, 0)
#define CS_off gpio_put(FRAM_CS, 1)

// FRAM commands
#define fram_write_cmd (2)
#define fram_read_cmd (3)
#define fram_we_cmd (6)
#define fram_wd_cmd (4)
#define fram_write_status_cmd (1)
#define fram_read_status_cmd (5)

// === read commands ===
void fram_read_bytes(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *buf, uint8_t len)
{
  CS_on;
  uint8_t cmdbuf[4] = {
      fram_read_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  spi_write_blocking(spi, cmdbuf, 4);
  spi_read_blocking(spi, 0, buf, len);
  CS_off;
}

void fram_read_int(spi_inst_t *spi, uint cs_pin, uint32_t addr, int32_t *buf, uint8_t len)
{
  CS_on;
  uint8_t cmdbuf[4] = {
      fram_read_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  //
  uint8_t intbuf[4 * len];
  //
  spi_write_blocking(spi, cmdbuf, 4);
  spi_read_blocking(spi, 0, intbuf, 4 * len);
  memcpy(buf, intbuf, 4 * len);
  CS_off;
}

void fram_read(spi_inst_t *spi, uint cs_pin, uint32_t addr, void *buf, uint8_t len)
{
  CS_on;
  uint8_t cmdbuf[4] = {
      fram_read_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  //
  spi_write_blocking(spi, cmdbuf, 4);
  spi_read_blocking(spi, 0, (uint8_t *)buf, len);
  //
  CS_off;
}

// === write control ===
void fram_write_enable(spi_inst_t *spi, uint cs_pin)
{
  CS_on;
  uint8_t cmd = fram_we_cmd;
  spi_write_blocking(spi, &cmd, 1);
  CS_off;
}

void fram_write_disable(spi_inst_t *spi, uint cs_pin)
{
  CS_on;
  uint8_t cmd = fram_wd_cmd;
  spi_write_blocking(spi, &cmd, 1);
  CS_off;
}

// === write commands ===
void fram_write_bytes(spi_inst_t *spi, uint cs_pin, uint32_t addr, uint8_t *data, uint8_t len)
{
  uint8_t cmdbuf[4] = {
      fram_write_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  fram_write_enable(FRAM_SPI_PORT, cs_pin);
  CS_on;
  spi_write_blocking(spi, cmdbuf, 4);
  spi_write_blocking(spi, data, len);
  CS_off;
  fram_write_disable(FRAM_SPI_PORT, cs_pin);
}

void fram_write_int(spi_inst_t *spi, uint cs_pin, uint32_t addr, int32_t *data, uint8_t len)
{
  uint8_t cmdbuf[4] = {
      fram_write_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  uint8_t intbuf[4 * len];
  memcpy(intbuf, data, 4 * len);
  //
  fram_write_enable(FRAM_SPI_PORT, cs_pin);
  CS_on;
  spi_write_blocking(spi, cmdbuf, 4);
  spi_write_blocking(spi, intbuf, 4 * len);
  CS_off;
  fram_write_disable(FRAM_SPI_PORT, cs_pin);
}

void fram_write(spi_inst_t *spi, uint cs_pin, uint32_t addr, void *data, uint8_t len)
{
  uint8_t cmdbuf[4] = {
      fram_write_cmd,
      addr >> 16,
      addr >> 8,
      addr};
  //
  fram_write_enable(FRAM_SPI_PORT, cs_pin);
  CS_on;
  spi_write_blocking(spi, cmdbuf, 4);
  spi_write_blocking(spi, (uint8_t *)data, len);
  CS_off;
  fram_write_disable(FRAM_SPI_PORT, cs_pin);
}

/// ===  status reg commands ===
// for lock/unloc commands
void fram_write_status(spi_inst_t *spi, uint cs_pin, uint8_t *data)
{
  uint8_t cmd = fram_write_status_cmd;
  fram_write_enable(FRAM_SPI_PORT, cs_pin);
  CS_on;
  spi_write_blocking(spi, &cmd, 1);
  spi_write_blocking(spi, data, 1);
  CS_off;
  fram_write_disable(FRAM_SPI_PORT, cs_pin);
}

char fram_read_status(spi_inst_t *spi, uint cs_pin)
{
  uint8_t cmd = fram_read_status_cmd;
  uint8_t buf;
  CS_on;
  //
  spi_write_blocking(spi, &cmd, 1);
  spi_read_blocking(spi, 0, &buf, 1);
  //
  CS_off;
  return buf;
}

#define volume_size 524288
#define volume_max_file_count 100
#define volume_header_size sizeof(int) * 3
#define volume_max_open_file_count 10
// an arbitrary integer for checking that the volume exists
#define volume_exists 4760
// start of the volume index
#define volume_file_index_base_location (volume_header_size + 1)

// file directory item
#define file_name_max_size 16
// 3 ints plus the file name
#define volume_file_index_entry_size (sizeof(int) * 3 + file_name_max_size)
#define volume_file_index_size (volume_file_index_entry_size * volume_max_file_count)
// the beginning of the actual data for the first file
#define volume_zero_file_location (volume_file_index_size + volume_header_size + 1)

// volume header:
// these items are written to  FRAM on volume creation
// these items are read from  FRAM on file creation
// vol_exists contains '4760' as a way to see if the volume already exists
struct vol_head
{
  int vol_exists_number;
  // init to volume_size - (vol header size + file directory size)
  int vol_free_space;
  // actually defined files
  int vol_file_count;
} volume_head;

// this struct is written to fram on file creation
// this struct is read during file opening
// this struct is writtein during file closeing
struct file_index
{
  char file_name[file_name_max_size];
  int file_begin_location;
  int file_max_size;
  int file_current_size;
} file_index_item;

// data for one open file
struct open_file_item
{
  int file_begin_location;
  int file_current_size; //(when opened)
  int file_max_size;
  int file_read_location;  //(init to 0 offset when opened)
  int file_write_location; // (init to offset file_size+1)
  int index_location;      // position of file in volume index
  int valid;               // true while the file is actually open
};
// all of the open files
struct open_file_item open_files[volume_max_open_file_count];

// initialize spi for fram
void fram_spi_init()
{
  spi_init(FRAM_SPI_PORT, 20000000);

  // Map SPI signals to GPIO ports
  // gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(FRAM_SCK, GPIO_FUNC_SPI);
  gpio_set_function(FRAM_MISO, GPIO_FUNC_SPI);
  gpio_set_function(FRAM_MOSI, GPIO_FUNC_SPI);
  // toggle CS manually rather than use framed CS
  gpio_init(FRAM_CS);
  // turn off chip sel
  gpio_put(FRAM_CS, true);
  gpio_set_dir(FRAM_CS, GPIO_OUT);
}

// === Create the volume structure
// EVERY program should start by calling this with force=false
// force=true means always build it
// force=false means dont overwrite existing structure
void volume_create(int force)
{
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol if force is false
  if (volume_head.vol_exists_number != volume_exists || force == true)
  {
    // build a new volume header
    volume_head.vol_exists_number = volume_exists;
    volume_head.vol_free_space = volume_size - (volume_header_size + volume_file_index_size);
    volume_head.vol_file_count = -1;
    // load it to fram
    fram_write(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  }
  // error check
  else if (volume_head.vol_exists_number = volume_exists && force == false)
  {
    printf("Volume create not done -- volume exists\n\r");
  }
}

// Return the size of free storage currently available for files
int volume_free(void)
{
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol
  if (volume_head.vol_exists_number == volume_exists)
  {
    return volume_head.vol_free_space;
  }
  else
  {
    return -1;
  }
}

// volume_file_count();
// Return the size of free storage currently available for files
int volume_file_count(void)
{
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol if force is false
  if (volume_head.vol_exists_number == volume_exists)
  {
    return volume_head.vol_file_count;
  }
  else
  {
    return -1;
  }
}

// Create a file with a name and a maximum size it can grow to
// returns a file count
int fCreate(char *fileName, uint32_t maxSize)
{
  // read the volume info and check for a volume
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol and check for avail space
  if ((volume_head.vol_exists_number == volume_exists) && (volume_head.vol_free_space > maxSize))
  {
    //
    volume_head.vol_file_count++;
    // build new file descriptor
    strncpy(file_index_item.file_name, fileName, 16);
    file_index_item.file_begin_location = volume_size - volume_head.vol_free_space;
    file_index_item.file_max_size = maxSize;
    file_index_item.file_current_size = -1;
    // write it
    int index_item_loc = volume_file_index_base_location + volume_head.vol_file_count * sizeof(file_index_item);
    fram_write(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));

    // write new volume header
    // modify volume file count, free space
    volume_head.vol_free_space -= maxSize;

    fram_write(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
    // testing
    // printf("fcreat: file count %d index loc %d name %s\n\r", volume_head.vol_file_count, index_item_loc, file_index_item.file_name) ;
    return volume_head.vol_file_count;
  }
  else
  {
    printf("fCreate: No volume, or not enough space\n\r");
    return -1;
  }
}

// file_open_index = fOpen(char* fileName);
// Open an existing file,  return negative if the file does not exist
int fOpen(char *fileName, int unit_number)
{
  // get volume header for number of files
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol and check for avail space
  if ((volume_head.vol_exists_number == volume_exists))
  {
    int n_files = volume_head.vol_file_count;
    for (int i = 0; i <= n_files; i++)
    {
      int index_item_loc = volume_file_index_base_location + i * sizeof(file_index_item);
      fram_read(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));
      // testing
      // printf("fopen: search# %d index loc %d name %s\n\r", i, index_item_loc, file_index_item.file_name) ;
      if (strcmp(fileName, file_index_item.file_name) == 0)
      {
        // new location
        // open_file_count++ ;
        // testing
        // printf("fopen file found: file name %s\n\r", file_index_item.file_name) ;
        // build open block
        open_files[unit_number].file_begin_location = file_index_item.file_begin_location;
        open_files[unit_number].file_current_size = file_index_item.file_current_size; //(when opened)
        open_files[unit_number].file_max_size = file_index_item.file_max_size;
        open_files[unit_number].file_read_location = 0;                                       //(init to 0 offset when opened)
        open_files[unit_number].file_write_location = file_index_item.file_current_size += 1; // (init to offset file_size+1)
        open_files[unit_number].index_location = i;                                           // index of file index entry on fram
        open_files[unit_number].valid = true;                                                 // index of file index entry on fram

        // return index to block
        return unit_number;
      }
    }
  }
  else
  {
    return -1;
  }
}

// fExists just checks for a given file name and returns the file index if true and -1 if false
int fExists(char *fileName)
{
  // get volume header for number of files
  // read a volume header (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol and check for avail space
  if ((volume_head.vol_exists_number == volume_exists))
  {
    int n_files = volume_head.vol_file_count;
    for (int i = 0; i <= n_files; i++)
    {
      int index_item_loc = volume_file_index_base_location + i * sizeof(file_index_item);
      fram_read(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));
      // testing
      // printf("fopen: search# %d index loc %d name %s\n\r", i, index_item_loc, file_index_item.file_name) ;
      if (strcmp(fileName, file_index_item.file_name) == 0)
      {
        // return index to file block
        return i;
      }
    }
    return -1;
  }
  else
  {
    return -1;
  }
}

// fClose(open_block_index) -- copies open file block back to file index location on fram
// nulls the open_block
// any write operation is NOT finalized until the file is closed
void fClose(int open_block_index)
{
  // mark open block invalid
  open_files[open_block_index].valid = false;
  // get current block from fram
  int index_item_loc = volume_file_index_base_location + open_files[open_block_index].index_location * sizeof(file_index_item);
  fram_read(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));
  // modify length,
  file_index_item.file_current_size = open_files[open_block_index].file_current_size;
  // testing
  // printf("fClose: size %d \n\r", open_files[open_block_index].file_current_size) ;
  // write it back
  fram_write(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));
}

// num_bytes = fRead(open_block_index, output_pointer, num_bytes)
// file must be open so that it has a unit_number
// get the open_block contents from the unit_number
// -- reads number of bytes at current read pointer with check for file size
// -- updates open_block read pointer
int fRead(int open_block_index, void *buf, uint32_t num_bytes)
{
  // is the file acetually open
  if (open_files[open_block_index].valid == false)
  {
    printf("fread: File not open\n\r");
    return -1;
  }
  // need to check for file size
  // testing
  // printf("loccation and size %d %d\n\r", open_files[open_block_index].file_read_location, open_files[open_block_index].file_current_size);
  if (num_bytes + open_files[open_block_index].file_read_location <= open_files[open_block_index].file_current_size + 1)
  {
    // get absolute fram address
    int addr = open_files[open_block_index].file_begin_location + open_files[open_block_index].file_read_location;
    uint8_t cmdbuf[4] = {
        fram_read_cmd,
        addr >> 16,
        addr >> 8,
        addr};
    //
    CS_on;
    spi_write_blocking(FRAM_SPI_PORT, cmdbuf, 4);
    spi_read_blocking(FRAM_SPI_PORT, 0, (uint8_t *)buf, num_bytes);
    CS_off;
    // update the open file block -- write location -- file size
    open_files[open_block_index].file_read_location += num_bytes;
    return num_bytes;
  }
  else
  {
    printf("fRead: not enough data\n\r");
    return -1;
  }
}

// num_bytes = fWrite(open_block_index, input_pointer, num_bytes)
// file must be open so that it has a unit_number
// get the open_block contents from the unit_number
// -- writes at end of file pointer with check for max_size overflow
// -- updates open_block write pointer and file size
int fWrite(int open_block_index, void *data, uint32_t num_bytes)
{
  // is the file actually open
  if (open_files[open_block_index].valid == false)
  {
    printf("fwrite: File not open\n\r");
    return -1;
  }
  //
  char lock_status = fram_read_status(FRAM_SPI_PORT, FRAM_CS);
  if (lock_status == 0x0c)
  {
    printf("fwrite: Volume is not writable (locked)\n\r");
    return -1;
  }
  //
  // check for valid write size
  if (num_bytes + open_files[open_block_index].file_current_size <= open_files[open_block_index].file_max_size)
  {
    // call the write
    // void fram_write(spi_inst_t *spi, uint cs_pin, uint32_t addr, void * data, uint8_t len)
    int addr = open_files[open_block_index].file_begin_location + open_files[open_block_index].file_write_location;
    // tesing
    // printf("fWrite: addr %d\n\r", addr);
    // fram_write(SPI_PORT, PIN_CS, fram_address, (void *) input_pointer, num_bytes) ;
    uint8_t cmdbuf[4] = {
        fram_write_cmd,
        addr >> 16,
        addr >> 8,
        addr};
    //
    fram_write_enable(FRAM_SPI_PORT, FRAM_CS);
    CS_on;
    spi_write_blocking(FRAM_SPI_PORT, cmdbuf, 4);
    spi_write_blocking(FRAM_SPI_PORT, (uint8_t *)data, num_bytes);
    CS_off;
    fram_write_disable(FRAM_SPI_PORT, FRAM_CS);

    // update the open file block -- write location -- file size
    open_files[open_block_index].file_write_location += num_bytes;
    open_files[open_block_index].file_current_size += num_bytes;
    return num_bytes;
  }
  else
  {
    printf("fWrite: attempt to write past max size\n\r");
    return -1;
  }
}

// fReadAt(file_open_index,  fileOffset, *buffer, count); // Read in data after seeking to a file position
int fReadAt(int open_block_index, void *buf, uint32_t offset, uint32_t num_bytes)
{
  // is the file acetually open
  if (open_files[open_block_index].valid == false)
  {
    printf("freadAt: File not open\n\r");
    return -1;
  }
  // need to check for file size
  // testing
  // printf("loccation and size %d %d\n\r", open_files[open_block_index].file_read_location, open_files[open_block_index].file_current_size);
  if (num_bytes + offset <= open_files[open_block_index].file_current_size + 1)
  {
    // get absolute fram address
    int addr = open_files[open_block_index].file_begin_location + offset;
    uint8_t cmdbuf[4] = {
        fram_read_cmd,
        addr >> 16,
        addr >> 8,
        addr};
    //
    CS_on;
    spi_write_blocking(FRAM_SPI_PORT, cmdbuf, 4);
    spi_read_blocking(FRAM_SPI_PORT, 0, (uint8_t *)buf, num_bytes);
    CS_off;
    // update the open file block -- write location -- file size
    // no location update for absolute read address
    // open_files[open_block_index].file_read_location += num_bytes ;
    return num_bytes;
  }
  else
  {
    printf("fReadAt: not enough data\n\r");
    return -1;
  }
}

// NEED
// fWriteAt(file_open_index, fileOffset, *buffer, count); // Write out data after seeking to a file position
int fWriteAt(int open_block_index, void *data, uint32_t location, uint32_t num_bytes)
{
  // is the file actually open
  if (open_files[open_block_index].valid == false)
  {
    printf("fwriteAt: File not open\n\r");
    return -1;
  }

  char lock_status = fram_read_status(FRAM_SPI_PORT, FRAM_CS);
  if (lock_status == 0x0c)
  {
    printf("fwriteAt: Volume is not writable (locked)\n\r");
    return -1;
  }

  // check for valid write size
  if (num_bytes + location <= open_files[open_block_index].file_current_size + 1)
  {
    // call the write
    // void fram_write(spi_inst_t *spi, uint cs_pin, uint32_t addr, void * data, uint8_t len)
    int addr = open_files[open_block_index].file_begin_location + location;
    // tesing
    // printf("fWrite: addr %d\n\r", addr);
    // fram_write(SPI_PORT, PIN_CS, fram_address, (void *) input_pointer, num_bytes) ;
    uint8_t cmdbuf[4] = {
        fram_write_cmd,
        addr >> 16,
        addr >> 8,
        addr};
    //
    fram_write_enable(FRAM_SPI_PORT, FRAM_CS);
    CS_on;
    spi_write_blocking(FRAM_SPI_PORT, cmdbuf, 4);
    spi_write_blocking(FRAM_SPI_PORT, (uint8_t *)data, num_bytes);
    CS_off;
    fram_write_disable(FRAM_SPI_PORT, FRAM_CS);
    // update the open file block -- write location -- file size
    // open_files[open_block_index].file_write_location += num_bytes ;
    // if(num_bytes+location > open_files[open_block_index].file_current_size){
    //   open_files[open_block_index].file_current_size = num_bytes + location ;
    //}
    return num_bytes;
  }
  else
  {
    printf("fWriteAt: attempt to write past current size\n\r");
    return -1;
  }
}

void volume_file_dir(void)
{
  // read a volume directory f (if any)
  fram_read(FRAM_SPI_PORT, FRAM_CS, 0, &volume_head, sizeof(volume_head));
  // check for existing vol if force is false
  if (volume_head.vol_exists_number == volume_exists)
  {
    printf("free bytes=%-6d     # files=%-2d \n\r", volume_free(), volume_file_count() + 1);
    char lock_status = fram_read_status(FRAM_SPI_PORT, FRAM_CS);
    if (lock_status == 0)
      printf("Volume is writable (unlocked)\n\r");
    else if (lock_status == 0x0c)
      printf("Volume is not writable (locked)\n\r");
    printf("*** fram files ***\n\r");
    printf("#  name            size   max_size\n\r");
    int n_files = volume_head.vol_file_count;
    for (int i = 0; i <= n_files; i++)
    {
      int index_item_loc = volume_file_index_base_location + i * sizeof(file_index_item);
      fram_read(FRAM_SPI_PORT, FRAM_CS, index_item_loc, &file_index_item, sizeof(file_index_item));
      // testing
      // printf("fopen: search# %d index loc %d name %s\n\r", i, index_item_loc, file_index_item.file_name) ;
      printf("%-2d %-15s %-6d %-6d\n\r", i, file_index_item.file_name, file_index_item.file_current_size, file_index_item.file_max_size);
    }
    printf("*** open files ***\n\r");
    printf(("ID cur_size read_loc write_loc\n\r"));
    for (int i = 0; i < volume_max_open_file_count; i++)
    {
      if (open_files[i].valid == true)
      {
        printf("%-2d %-8d %-8d %-6d\n\r", i,
               open_files[i].file_current_size, open_files[i].file_read_location, open_files[i].file_write_location);
      }
    }
  }
  else
  {
    printf("No directory\n\r");
  }
}

// ===============END FRAM=====================

// ==================DAC=======================

// DAC parameters
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

// data for the spi port
uint16_t DAC_data;

void dac_init()
{
  spi_init(DAC_SPI_PORT, 20000000);
  // Format (channel, data bits per transfer, polarity, phase, order)
  spi_set_format(DAC_SPI_PORT, 16, 0, 0, 0);
  // Map SPI signals to GPIO ports
  // gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(DAC_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(DAC_PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(DAC_PIN_CS, GPIO_FUNC_SPI);
}
// ================END DAC====================

// ===========================================
// ADC setup
// ===========================================
// setup ADC
void ADC_setup(void)
{
  adc_init();
  // Make sure GPIO is high-impedance, no pullups etc
  // microphone
  adc_gpio_init(26);
  // y
  // adc_gpio_init(27);
}

// char file_name[16] = "track";

volatile int recording, playing;
volatile int first_record = true;
volatile int blink_red = false;

// ==========================================
//           set up timer ISR
// ==========================================
// 1/Fs in microseconds
// 8 KHz
volatile int alarm_period = 125;
// !! DO NOT USE alarm 0
// This low-level setup is ocnsiderably faster to execute
// than the hogh-level callback

#define ALARM_NUM 1
#define ALARM_IRQ TIMER_IRQ_1

volatile int track_length;
volatile int track_length_counter;
volatile int track_position;
#define FRAM_BUFFER_SIZE 80000
char fram_buffer[FRAM_BUFFER_SIZE];

static void alarm_irq(void)
{
  // Clear the alarm irq
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
  // arm the next interrupt
  // Write the lower 32 bits of the target time to the alarm to arm it
  timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + alarm_period;

  if (!first_record && track_position > track_length)
  {
    track_position = 0;
  }

  if (first_record && track_position > FRAM_BUFFER_SIZE)
  {
    recording = false;
    first_record = false;
    playing = true;
    track_position = 0;
    track_length = track_length_counter;
  }

  // if first record, increment counter for track length
  if (first_record && recording)
  {
    track_length_counter++;
  }

  if (recording && ((!first_record && track_position < track_length) || (first_record && track_position < FRAM_BUFFER_SIZE)))
  {
    int adc_data = adc_read() >> 3;

    if (first_record)
    {
      fram_buffer[track_position] = adc_data;
    }
    else
    {
      int buffer_data_read = fram_buffer[track_position];
      fram_buffer[track_position] = (buffer_data_read >> 1) + (adc_data >> 1);
    }
  }

  if (playing && track_position < track_length)
  {
    // read from buffer
    int buffer_data = fram_buffer[track_position] << 3; //<<3;
    // low pass
    int DAC_data_low_passed = DAC_data_low_passed + ((buffer_data - DAC_data_low_passed) >> 2);

    // DAC_data = (DAC_config_chan_A | (DAC_data_low_passed & 0xff0));
    DAC_data = (DAC_config_chan_A | (buffer_data & 0xff0));
    // DAC_data = (DAC_config_chan_A | ((fram_buffer[track_position] << 3) & 0xff0));
    // send to dac
    spi_write16_blocking(DAC_SPI_PORT, &DAC_data, 1);
  }

  if (recording || playing)
  {
    track_position++;
  }
}

// set up the timer alarm ISR
static void alarm_in_us(uint32_t delay_us)
{
  // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  // Set irq handler for alarm irq
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  // Enable the alarm irq
  irq_set_enabled(ALARM_IRQ, true);
  // Enable interrupt in block and at processor
  // Alarm is only 32 bits
  uint64_t target = timer_hw->timerawl + delay_us;
  // Write the lower 32 bits of the target time to the alarm which
  // will arm it
  timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
}

void button_callback(uint gpio, uint32_t evt)
{
  gpio_set_irq_enabled(RECORD_BUTTON, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  gpio_set_irq_enabled(PLAY_BUTTON, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
  gpio_set_irq_enabled(CLEAR_BUTTON, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

  // printf("\n\ngpio: %d\nplay: %d, record: %d", gpio, playing, recording);

  if (gpio == RECORD_BUTTON)
  {
    if (first_record && !recording)
    {
      blink_red = true;
    }
    else
    {
      if (recording && first_record)
      {
        playing = true;
        first_record = false;
        recording = false;
        track_length = track_length_counter;
        track_position = 0;
      }
      else
      {
        recording = !recording;
        if (recording && !first_record)
        {
          playing = true;
        }
      }
    }
  }

  else if (gpio == PLAY_BUTTON)
  {
    if (!first_record)
    {
      playing = !playing;
    }

    if (!playing)
    {
      recording = false; // if pause, recording ends
    }
  }

  else if (gpio == CLEAR_BUTTON)
  {
    playing = false;
    recording = false;
    first_record = true;
    blink_red = false;
    // clear the buffer
    for (int i = 0; i < FRAM_BUFFER_SIZE; i++)
    {
      fram_buffer[i] = 0;
    }
    // track_length_counter, reset track position
    track_length_counter = 0;
    track_position = 0;
  }

  // printf("\nplay: %d, record: %d", playing, recording);
  gpio_set_irq_enabled(RECORD_BUTTON, GPIO_IRQ_EDGE_RISE, true);
  gpio_set_irq_enabled(PLAY_BUTTON, GPIO_IRQ_EDGE_RISE, true);
  gpio_set_irq_enabled(CLEAR_BUTTON, GPIO_IRQ_EDGE_RISE, true);
}

// This thread runs on core 0
static PT_THREAD(led_thread(struct pt *pt))
{
  // Indicate thread beginning
  PT_BEGIN(pt);
  gpio_put(YELLOW_LED, first_record);
  while (!blink_red)
  {
    PT_YIELD_usec(30000);
  }
  // Blink the red LED to signal the guitarist recording starting soon
  for (int i = 0; i < 6; i++)
  {
    gpio_put(RED_LED, !gpio_get(RED_LED));
    sleep_ms(500);
  }

  recording = true;
  playing = false;
  blink_red = false;

  // create file to put track

  while (1)
  {
    if (blink_red)
    {
      for (int i = 0; i < 6; i++)
      {
        gpio_put(RED_LED, !gpio_get(RED_LED));
        sleep_ms(500);
      }
      blink_red = false;
      recording = true;
      playing = false;
    }
    gpio_put(RED_LED, recording);
    gpio_put(GREEN_LED, playing);
    gpio_put(YELLOW_LED, first_record);

    PT_YIELD_usec(30000);
  }
  // Indicate thread end
  PT_END(pt);
}

volatile int track_lengths[5] = {0, 0, 0, 0, 0};

// serial thread for saving the recorded track or play a saved track
static PT_THREAD(serial_thread(struct pt *pt))
{
  PT_BEGIN(pt);
  PT_YIELD_usec(1000000);

  sprintf(pt_serial_out_buffer, "\nTo save track: save\n\r");
  serial_write;
  sprintf(pt_serial_out_buffer, "To play a saved track: play\n\r");
  serial_write;

  int track_num;
  char track_file_name[16];
  while (1)
  {
    // volume_file_dir();
    // print prompt
    sprintf(pt_serial_out_buffer, "cmd >");
    serial_write;
    serial_read;

    if (strcmp(pt_serial_in_buffer, "save") == 0)
    {
      recording = false;

      sprintf(pt_serial_out_buffer, "Enter track number [1-5]: ");
      serial_write;
      serial_read;

      sscanf(pt_serial_in_buffer, "%d", &track_num);
      sscanf(pt_serial_in_buffer, "%s", track_file_name);

      track_lengths[track_num - 1] = track_length;
      fOpen(track_file_name, 9);
      fWrite(9, &fram_buffer, FRAM_BUFFER_SIZE);
      fClose(9);
      track_position = 0;
    }

    // user can choose which saved tracks to play back
    if (strcmp(pt_serial_in_buffer, "play") == 0)
    {
      first_record = false;
      recording = false;
      playing = false;

      sprintf(pt_serial_out_buffer, "Enter track number [1-5]: ");
      serial_write;
      serial_read;
      sscanf(pt_serial_in_buffer, "%d", &track_num);
      sscanf(pt_serial_in_buffer, "%s", track_file_name);

      if (track_lengths[track_num - 1] == 0)
      {
        sprintf(pt_serial_out_buffer, "Nothing saved here yet");
        serial_write;
      }
      else
      {
        fOpen(track_file_name, 9);
        fRead(9, &fram_buffer, FRAM_BUFFER_SIZE);
        fClose(9);
        track_position = 0;
        track_length = track_lengths[track_num - 1];
      }
      playing = true;
    }
  }
  PT_END(pt);
} // serial thread

int main()
{
  // Initialize stdio
  stdio_init_all();

  printf("Welcome!\n");

  gpio_pull_up(RECORD_BUTTON);
  gpio_set_irq_enabled_with_callback(RECORD_BUTTON, GPIO_IRQ_EDGE_RISE, true, &button_callback);

  gpio_pull_up(PLAY_BUTTON);
  gpio_set_irq_enabled_with_callback(PLAY_BUTTON, GPIO_IRQ_EDGE_RISE, true, &button_callback);

  gpio_pull_up(CLEAR_BUTTON);
  gpio_set_irq_enabled_with_callback(CLEAR_BUTTON, GPIO_IRQ_EDGE_RISE, true, &button_callback);

  gpio_init(GREEN_LED);
  gpio_set_dir(GREEN_LED, GPIO_OUT);
  gpio_put(GREEN_LED, 0);

  gpio_init(RED_LED);
  gpio_set_dir(RED_LED, GPIO_OUT);
  gpio_put(RED_LED, 0);

  gpio_init(YELLOW_LED);
  gpio_set_dir(YELLOW_LED, GPIO_OUT);
  gpio_put(YELLOW_LED, 0);

  fram_spi_init();

  dac_init();
  ADC_setup();
  adc_select_input(0);

  volume_create(true);

  fCreate("1", FRAM_BUFFER_SIZE);
  fCreate("2", FRAM_BUFFER_SIZE);
  fCreate("3", FRAM_BUFFER_SIZE);
  fCreate("4", FRAM_BUFFER_SIZE);
  fCreate("5", FRAM_BUFFER_SIZE);

  // timer interrupt for sampling and playing
  alarm_in_us(alarm_period);

  pt_add_thread(led_thread);
  pt_add_thread(serial_thread);
  pt_schedule_start;
}
