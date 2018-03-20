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
	if(!bDirectoryLoaded){
		eDisk_ReadSector(Buff,255);//    read disk sector 255 and populate Directory and FAT
		for(uint8_t i = 0; i< 255; ++i){
			Directory[i]= Buff[i];
		}
			for(uint8_t i = 0; i< 255; ++i){
			FAT[i]= Buff[i+256];
		}
 bDirectoryLoaded=1;
	}
return;
}

// Return the index of the last sector in the file
// associated with a given starting sector.
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t lastsector(uint8_t start){
	if (start == 255) return 255;
  
	uint8_t i = start;
	while (i != 255) {
		i = FAT[start];
		if (i == 255) {
			return start;
		} else {
			start = i;
		}
	}	
	return 255; // error
	/*uint8_t i = start;
  while(FAT[i]!=255){
		while (i != 255) {
		i = FAT[start];
		if (i == 255) {
			return start;
		} else {
			start = i;
		}
	}
  return 254;*/
}

// Return the index of the first free sector.
// Note: This function will loop forever without returning
// if a file has no end or if (Directory[255] != 255)
// (i.e. the FAT is corrupted).

uint8_t endsector=0, maxendsector=-1;	//this way they can be observed and debugged
uint8_t findfreesector(void){
int16_t fs = -1;
	uint8_t i = 0;
	int16_t ls = (int16_t) lastsector(Directory[i]);
	
	while (ls != 255) {
		fs = max(fs, ls);
		i++;
		ls = lastsector(Directory[i]);
	}
	
	return fs + 1;/*
for(uint8_t i = 0; i<255; ++i){
	endsector=lastsector(Directory[i]);
	if( endsector== 255) break;
	maxendsector=max(endsector, maxendsector);
}
return maxendsector+1;*/
   
}

// Append a sector index 'n' at the end of file 'num'.
// This helper function is part of OS_File_Append(), which
// should have already verified that there is free space,
// so it always returns 0 (successful).
// Note: This function will loop forever without returning
// if the file has no end (i.e. the FAT is corrupted).
uint8_t appendfat(uint8_t num, uint8_t n){
	uint8_t m = 0;
	uint8_t sect = Directory[num];
	
	if (sect == 255) { // file is new, empty
		Directory[num] = n;
		return 0;
	} else {
		while (1) {
			m = FAT[sect]; // next sector
			if (m == 255) { // eof
				FAT[sect] = n;
				return 0;
			} else {
				sect = m; // skip to next sector
			}
		}
	}
  /*if (Directory[num]==255){Directory[num]=n;return 0;}

  uint8_t i = 0;
  while(1){
	if(FAT[i]==255){

	FAT[i]=n;
	
	return 0;} 
	
	i=FAT[i];
	}*/
}

//********OS_File_New*************
// Returns a file number of a new file for writing
// Inputs: none
// Outputs: number of a new file
// Errors: return 255 on failure or disk full
uint8_t OS_File_New(void){
	MountDirectory();
	 if(lastsector(Directory[254])!=255) return 255;
	uint8_t i =0; 
	while(Directory[i]!= 255&&i<255) ++i;
	return i;
}

//********OS_File_Size*************
// Check the size of this file
// Inputs:  num, 8-bit file number, 0 to 254
// Outputs: 0 if empty, otherwise the number of sectors
// Errors:  none
uint8_t OS_File_Size(uint8_t num){
  if(Directory[num] == 255) return 0;
	uint8_t i = Directory[num], count =1;
  while(FAT[i]!=255){
		 i= FAT[i];
		count++;
	}
  return count; // replace this line
}

//********OS_File_Append*************
// Save 512 bytes into the file
// Inputs:  num, 8-bit file number, 0 to 254
//          buf, pointer to 512 bytes of data
// Outputs: 0 if successful
// Errors:  255 on failure or disk full
uint8_t OS_File_Append(uint8_t num, uint8_t buf[512]){
	MountDirectory();
	uint8_t next = findfreesector();
	if(next==255) return 255;
	
	appendfat(num, next);
	eDisk_WriteSector(buf, next);
  
  return 0; // replace this line
}

//********OS_File_Read*************
// Read 512 bytes from the file
// Inputs:  num, 8-bit file number, 0 to 254
//          location, logical address, 0 to 254
//          buf, pointer to 512 empty spaces in RAM
// Outputs: 0 if successful
// Errors:  255 on failure because no data
uint8_t OS_File_Read(uint8_t num, uint8_t location,
                     uint8_t buf[512]){
	uint8_t count = 0;
	uint8_t sect = Directory[num];
											 
	if (sect == 255) return 255; // error, no data
											 
	while (count != location) { // traverse FAT to find sector
		if (FAT[sect] == 255) return 255;
		sect = FAT[sect];
		count++;
	}
	/*if(OS_File_Size(num)==0) return 255;
	uint8_t sector=Directory[num];
	for(uint8_t i = 0; i< location; i++){
		sector= FAT[sector];//incriment through path location number of times
	}*/
  eDisk_ReadSector(buf, sect);
  return 0; 
}

//********OS_File_Flush*************
// Update working buffers onto the disk
// Power can be removed after calling flush
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Flush(void){
		for(uint8_t i = 0; i< 255; ++i){
			 Buff[i]=Directory[i];
			Buff[i+256]=FAT[i];
		}


  return eDisk_WriteSector(Buff, 255) >0 ?255:0; 
}

//********OS_File_Format*************
// Erase all files and all data
// Inputs:  none
// Outputs: 0 if success
// Errors:  255 on disk write failure
uint8_t OS_File_Format(void){
	enum DRESULT x = eDisk_Format();// call eDiskFormat
	bDirectoryLoaded=0;// clear bDirectoryLoaded to zero

  return x>0? 255:0; 
}
