// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader()
{
	numBytes = -1;
	numSectors = -1;
    numLists = -1;
	memset(dataSectorLists, -1, sizeof(dataSectorLists));
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	// nothing to do now
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    numSectors = divRoundUp(fileSize, SectorSize);
    numLists = divRoundUp(numSectors, SectorNumPerList);
    // if not enough space
    if (freeMap->NumClear() < numSectors)
        return FALSE;
    int nowNumSectors = 0;
    for (int i = 0; i < numLists; i++, nowNumSectors += SectorNumPerList) {
        // find free sector for sector list
        dataSectorLists[i] = freeMap->FindAndSet();
        ASSERT(dataSectorLists[i] >= 0);
        // find lastSectorNum for this loop
        int lastSectorNum;
        if (nowNumSectors + SectorNumPerList > numSectors) lastSectorNum = numSectors;
        else lastSectorNum = nowNumSectors + SectorNumPerList;
        // buffer to write to disk
        int *buf = new int[SectorNumPerList];
        memset(buf, 0, sizeof(int) * SectorNumPerList);
        for (int j = 0; j < lastSectorNum - nowNumSectors; j++) {
            // for each entry in the list, find a free space
            buf[j] = freeMap->FindAndSet();
            ASSERT(buf[j] >= 0);
        }
        // write to disk
        kernel->synchDisk->WriteSector(dataSectorLists[i], (char *) buf);
        delete [] buf;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(PersistentBitmap *freeMap)
{
    int nowNumSectors = 0;
    for (int i = 0; i < numLists; i++, nowNumSectors += SectorNumPerList) {
        // last Sector in this loop
        int lastSectorNum;
        if (nowNumSectors + SectorNumPerList > numSectors) lastSectorNum = numSectors;
        else lastSectorNum = nowNumSectors + SectorNumPerList;
        // buf to write in
        int *buf = new int[SectorNumPerList];
        kernel->synchDisk->ReadSector(dataSectorLists[i], (char *) buf);
        for (int j = 0; j < lastSectorNum - nowNumSectors; j++) {
            // check if buf[j] is marked and clear it
            ASSERT(freeMap->Test((int) buf[j]));
            freeMap->Clear((int) buf[j]);
        }
        delete [] buf;
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);
	
	/*
		MP4 Hint:
		After you add some in-core informations, you will need to rebuild the header's structure
	*/
	
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this); 
	
	/*
		MP4 Hint:
		After you add some in-core informations, you may not want to write all fields into disk.
		Use this instead:
		char buf[SectorSize];
		memcpy(buf + offset, &dataToBeWritten, sizeof(dataToBeWritten));
		...
	*/
	
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    // calculate number of sectors
    int sectorIdx = offset / SectorSize;
    // calculate where is it stored
    int listIdx = sectorIdx / SectorNumPerList, idxInList = sectorIdx % SectorNumPerList;
    // buf to read in
    int *buf = new int[SectorNumPerList];
    kernel->synchDisk->ReadSector(dataSectorLists[listIdx], (char *) buf);
    // get the SectorNum
    int retVal = buf[idxInList];
    delete [] buf;
    return retVal;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    printf("FileHeader contents.  File size: %d.  List blocks:\n", numBytes);
    for (int i = 0; i < numLists; i++)
        printf("%d ", dataSectorLists[i]);
    puts("");
    int nowNumSectors = 0, nowNumBytes = 0;
    for (int i = 0; i < numLists; i++, nowNumSectors += SectorNumPerList) {
        // last Sector in this loop
        int lastSectorNum;
        if (nowNumSectors + SectorNumPerList > numSectors) lastSectorNum = numSectors;
        else lastSectorNum = nowNumSectors + SectorNumPerList;
        // buf to write in
        int *buf = new int[SectorNumPerList];
        kernel->synchDisk->ReadSector(dataSectorLists[i], (char *) buf);
        printf("File contents in list %d, Sector %d:\n", i, dataSectorLists[i]);
        for (int j = 0; j < lastSectorNum - nowNumSectors; j++) {
            // check if buf[j] is marked and clear it
            char *data = new char[SectorSize];
            // read the data the idx in the list points to
            kernel->synchDisk->ReadSector(buf[j], (char *) data);
            // print it as original version
            for (int k = 0; (k < SectorSize) && (nowNumBytes < numBytes); k++, nowNumBytes++) {
                if ('\040' <= data[k] && data[k] <= '\176')
                    printf("%c", data[k]);
                else
                    printf("\\%x", (unsigned char) data[k]);
            }
            puts("");
            delete [] data;
        }
        delete [] buf;
    }
}
