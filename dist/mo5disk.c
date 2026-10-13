#include <cmoc.h>

#define EOF_MARKER 0xC0   // Values greater than 0xC0 indicate End of File
#define SECTOR_SIZE 256

unsigned char sectorBuffer[SECTOR_SIZE];
unsigned char fatBuffer[SECTOR_SIZE]; // Holds Track 20, Sector 1

// Low-level sector reader using SWI 2
unsigned char readSector(unsigned char drive, unsigned char track, unsigned char sector, void *buffer)
{
    unsigned char errorFlag = 0;
    asm {
        PSHS  U
        LDA   :drive
        LDB   :track
        LDX   :buffer
        CLRA
        LDB   :sector
        TFR   D,Y
        LDA   #$02          ; Read Command
        LDB   #$01          ; 1 Sector
        SWI
        FCB   $02
        BCC   .ok
        STB   :errorFlag
.ok:
        PULS  U
    }
    return errorFlag;
}


/*
 * Scan Track 20 (Sectors 2-16) to find the file name matching target.
 * Note: target must be 11 chars (8 for filename, 3 for extension) exactly.
 * Example target: "MYPROG  BIN"
 */
unsigned char findFileSector(const char *targetName)
{
    unsigned char currentSector;
    unsigned char entryIndex;
    unsigned char charIndex;
    unsigned char isMatch;
    unsigned int  entryOffset;
    
    // Scan all directory sectors on Track 20
    for (currentSector = 2; currentSector <= 16; currentSector++)
    {
        // Query Thomson system firmware to fill our RAM buffer
        if (readThomsonSector(0, 20, currentSector, sectorBuffer) != 0) {
            continue; // Hardware error reading this directory block
        }
        
        // Loop through the 8 directory entries contained within the 256-byte cache
        for (entryIndex = 0; entryIndet + 15]; // Byte 15 = Sector
                return 1; // Success flag
            }
        }
    }
    
    return 0; // File was not found in directoryx < 8; entryIndex++)
        {
            entryOffset = entryIndex * 32;
            
            // Check status byte: 0xFF indicates a deleted or empty record
            if (sectorBuffer[entryOffset] == 0xFF) {
                continue;
            }
            
            // Verify if filename and extension characters match
            isMatch = 1;
            for (charIndex = 0; charIndex < 11; charIndex++) 
            {
                // Offsets 1 to 11 hold the 'FILENAMEEXT' pattern
                if (sectorBuffer[entryOffset + 1 + charIndex] != targetName[charIndex]) {
                    isMatch = 0;
                    break;
                }
            }
            
            // We found our target match!
            if (isMatch) {
                fileStartTrack  = sectorBuffer[entryOffset + 14]; // Byte 14 = Track
                filSetartSector = sectorBuffer[entryOffse
}

// Global configuration variables for the disk layout
// These depend on how you formatted the disk in fdfs
unsigned char sectorsPerCluster = 2; // Standard for double-density DKDOS

void streamLargeFile(const char *targetName)
{
    unsigned char currentTrack = 0;
    unsigned char currentSector = 0;
    unsigned char currentCluster = 0;
    unsigned long bytesRead = 0;
    
    // 1. Load the FAT into RAM memory immediately
    if (readSector(0, 20, 1, fatBuffer) != 0) {
        printf("Error: Failed to read FAT table.\n");
        return;
    }

    // 2. Locate the starting Track and Sector from Directory (Track 20, Sector 2)
    // (Assuming you used the findFileSector function from earlier)
    // Let's assume it found starting track/sector and calculated initial cluster
    
    printf("Streaming 122KB File...\n");

    while (bytesRead < 122000)
    {
        // Read the current raw physical sector
        if (readSector(0, currentTrack, currentSector, sectorBuffer) == 0)
        {
            // Process your 256 bytes here!
            // e.g., pipe to video RAM, audio registers, or bank-switched RAM
            bytesRead += SECTOR_SIZE;
        }
        else
        {
            printf("Disk read error at T:%d S:%d\n", currentTrack, currentSector);
            break;
        }

        // Advance to the next sector inside the cluster
        currentSector++;
        
        // Have we exhausted this cluster? Time to look at the FAT chain
        // Note: Thomson sectors typically wrap based on layout geometry
        if (currentSector > 16) // Example boundary for 16 sectors per track
        {
            currentSector = 1;
            currentTrack++;
        }
        
        // Every time you pass a cluster allocation boundary:
        // currentCluster = fatBuffer[currentCluster];
        // if (currentCluster >= EOF_MARKER) break;
    }
    
    printf("Finished. Total bytes streamed: %ld\n", bytesRead);
}
