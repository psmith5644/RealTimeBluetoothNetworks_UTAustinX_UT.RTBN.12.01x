// eFile.c
// Runs on either TM4C123 or MSP432
// High-level implementation of the file system implementation.
// Daniel and Jonathan Valvano
// August 29, 2016
#include <stdint.h>
#include "eDisk.h"

uint8_t Buff[512]; // temporary buffer used during file I/O
uint8_t Directory[256], FAT[256];
int32_t bDirectoryLoaded =0; // 0 means disk on ROM is complete, 1 means RAM version active

// Return the larger of two integers.
int16_t max(int16_t a, int16_t b){
  if(a > b){
    return a;
  }
  return b;
}

//*****MountDirectory******
// if directory and FAT are not loaded in RAM,
// bring it into RAM from disk
void MountDirectory(void){ 
// if bDirectoryLoaded is 0, 
//    read disk sector 255 and populate Directory and FAT
//    set bDirectoryLoaded=1
// if bDirectoryLoaded is 1, simply return

  if (bDirectoryLoaded != 0) {
    return;
  }

  eDisk_ReadSector(Buff, 255);

  for (int i = 0; i < 256; i++) {
    Directory[i] = Buff[i];
  }

  for (int i = 0; i < 256; i++) {
    FAT[i] = Buff[i+256];
  }

  bDirectoryLoaded = 1;
}

// Return the index of the last sector in the file
// associated with a given starting sector.
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t lastsector(uint8_t start){
  uint8_t current_sector = start;

  while (FAT[current_sector] != 255) {
    current_sector = FAT[current_sector];
  } 

  return current_sector;
}

// Return the index of the first free sector.
// Note: This function will loop forever without returning
// if a file has no end or if (Directory[255] != 255)
// (i.e. the FAT is corrupted).
uint8_t findfreesector(void){
  uint8_t i = 0;
  int16_t last_used_sector = -1;

  while (Directory[i] != 255) {
    int16_t last_sector = lastsector(Directory[i]);
    last_used_sector = max(last_sector, last_used_sector);
    i++;
  }
  return last_used_sector + 1;
}

// Append a sector index 'n' at the end of file 'num'.
// This helper function is part of OS_File_Append(), which
// should have already verified that there is free space,
// so it always returns 0 (successful).
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t appendfat(uint8_t num, uint8_t n){

  if (Directory[num] == 255) {
    Directory[num] = n;
    return 0;
  }

  uint8_t last_sector = lastsector(Directory[num]);
  FAT[last_sector] = n;
  return 0;
}

//********OS_File_New*************
// Returns a file number of a new file for writing
// Inputs: none
// Outputs: number of a new file
// Errors: return 255 on failure or disk full
uint8_t OS_File_New(void){
  MountDirectory();
  uint8_t i = 0;
  while (i < 255 && Directory[i] != 255) {
    i++;
  }  

  return i;
}

//********OS_File_Size*************
// Check the size of this file
// Inputs:  num, 8-bit file number, 0 to 254
// Outputs: 0 if empty, otherwise the number of sectors
// Errors:  none
uint8_t OS_File_Size(uint8_t num){
  MountDirectory();
  if (Directory[num] == 255) {
    return 0;
  }

  uint8_t curr_sector = Directory[num];
  uint8_t num_sectors = 1;
	while (FAT[curr_sector] != 255) {
    num_sectors++;
    curr_sector = FAT[curr_sector];
  }

  return num_sectors;
}

//********OS_File_Append*************
// Save 512 bytes into the file
// Inputs:  num, 8-bit file number, 0 to 254
//          buf, pointer to 512 bytes of data
// Outputs: 0 if successful
// Errors:  255 on failure or disk full
uint8_t OS_File_Append(uint8_t num, uint8_t buf[512]){
  MountDirectory();
  if (num >= 255) {
    return 255;
  }

  uint8_t free_sector = findfreesector();
  appendfat(num, free_sector);

  uint32_t error = eDisk_WriteSector(buf, free_sector);
  if (error) {
    return 255;
  }

  return 0;
}

//********OS_File_Read*************
// Read 512 bytes from the file
// Inputs:  num, 8-bit file number, 0 to 254
//          location, logical address, 0 to 254
//          buf, pointer to 512 empty spaces in RAM
// Outputs: 0 if successful
// Errors:  255 on failure because no data
uint8_t OS_File_Read(uint8_t num, uint8_t location,
                     uint8_t buf[512]) {
  MountDirectory();
  uint8_t sector = Directory[num];

  for (uint8_t i = 1; i <= location; i++) {
    sector = FAT[sector];
  }

  if (sector == 255) {
    return 255;
  }

  uint8_t error = eDisk_ReadSector(buf, sector);
  if (error) {
    return 255;
  }

  return 0;
}

//********OS_File_Flush*************
// Update working buffers onto the disk
// Power can be removed after calling flush
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Flush(void){
  MountDirectory();
  for (uint32_t i = 0; i < 256; i++) {
    Buff[i] = Directory[i];
  }

  for (uint32_t i = 0; i < 256; i++) {
    Buff[i+256] = FAT[i];
  }

  uint32_t error = eDisk_WriteSector(Buff, 255);
  if (error) {
    return 255;
  }

  return 0;
}

//********OS_File_Format*************
// Erase all files and all data
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Format(void){
  // call eDiskFormat
  // clear bDirectoryLoaded to zero
  MountDirectory();
  uint8_t error = eDisk_Format();
  if (error) {
    return 255;
  }

  bDirectoryLoaded = 0; 

  return 0;
}
