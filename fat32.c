/*>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
Author:     Sami Byaruhanga 
Purpose:    Checking filesystem info, list, get
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>*/



/* STEPS TO SOLVE PROGRAM *******************************************
 0. Read usr cmd + file > CHECK MBR & FAT(reserved region/ cluster 0/BPB) > Ensure valid 
 1. print drive Info
 2. list contents 
 3. fetch file
*********************************************************************/



//INCLUDES **********************************************************
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     //for strcpy i.e., copying file names into filesToBeRead
#include <ctype.h>      //so as to use to lower
#include <fcntl.h>      //open, and the O_stuff
#include <unistd.h>     // write
#include <stdbool.h>    //bool
#include <stdint.h>     //using unsigned ints
#include <sys/types.h>
//************************************************* INCLUDES END HERE



//STRUCTS & DEFINES *************************************************
#define _FILE_OFFSET_BITS 64    //textbook provided:  make the off_t 64 bits wide instead of 32 bits.
#define BS_OEMName_LENGTH 8
#define BS_VolLab_LENGTH 11
#define BS_FilSysType_LENGTH 8 

#pragma pack(push, 1)       //same as #pragma pack(push) #pragma pack(1)
struct fat32BS_struct {
	char BS_jmpBoot[3];                 //Switched this from uint8_t
	char BS_OEMName[BS_OEMName_LENGTH]; //Switched this from uint8_t
	uint16_t BPB_BytesPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
	uint32_t BPB_FATSz32;
	uint16_t BPB_ExtFlags;
	uint8_t BPB_FSVerLow;
	uint8_t BPB_FSVerHigh;
	uint32_t BPB_RootClus;
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	char BPB_reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	char BS_VolLab[BS_VolLab_LENGTH];
	char BS_FilSysType[BS_FilSysType_LENGTH];
	char BS_CodeReserved[420];
	uint8_t BS_SigA;
	uint8_t BS_SigB;
};
typedef struct fat32BS_struct fat32BS;
#pragma pack(pop)

#pragma pack(push, 1)       //same as #pragma pack(push) #pragma pack(1)
struct FSInfo{
    uint32_t lead_sig;
    uint8_t reserved1[480];
    uint32_t signature;
    uint32_t free_count;
    uint32_t next_free;
    uint8_t reserved2[12];
    uint32_t trail_signature;
};
typedef struct FSInfo fat32FSInfo; 
#pragma pack(pop)

#pragma pack(push, 1)
struct DirInfo {
    unsigned char dir_name[11]; 
    uint8_t dir_attr;     
    uint8_t dir_ntres;          
    uint8_t dir_crt_time_tenth;
    uint16_t dir_crt_time;
    uint16_t dir_crt_date;
    uint16_t dir_last_access_time;
    uint16_t dir_first_cluster_hi;
    uint16_t dir_wrt_time;
    uint16_t dir_wrt_date;
    uint16_t dir_first_cluster_lo;
    uint32_t dir_file_size;
};
typedef struct DirInfo fat32Dir; 
#pragma pack(pop)


#define BITMASK 0x0FFFFFFFF
#define EOC 0x0FFFFFF8  // page 18 changed based off handount
#define BAD_CLUSTER 0x0FFFFFF7
#define LEAD_SIGNATURE 0x41615252
#define SIGNATURE 0x61417272
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID
//************************************************** STRUCTS END HERE



//GLOBAL VARIABLES **************************************************
int fd, totalSectorCount;
uint32_t fsInfoOffset, fat0, fat1, startingAddy;
uint32_t firstDataSector;   //page 14 of fatSpecs manual
uint32_t RootDirSectors;    //page 14 of fatSpecs manual
fat32BS* bootSector;
fat32FSInfo* fsInfo;
//*******************************************************************

//FUNCTIONS FOR READING EACH HEADER SECTION *************************
//-------------------------------------------------------
// checkConsistent
// PURPOSE: To check the mbr and fat ensure consistent
// INPUT PARAMETERS: bootsector, fs information
// NOTE: Borrowed from lab4 code
//------------------------------------------
void checkConsistent(fat32BS* bootSector,fat32FSInfo* fsInfo){
    printf("----------------------------\n");
    printf("Checking if constistent\n");
    //Checking mbr -------------------------
   if (bootSector->BS_jmpBoot[0] != '\xEB' && bootSector->BS_jmpBoot[0] != '\xE9') {
        printf("Inconsistent file system: BS_jmpBoot must be 0xEB or 0xE9 is %x \n", bootSector->BS_jmpBoot[0]);
        close(fd);
        exit(1);
    }

    if (bootSector->BPB_FATSz32 == 0) {
        printf("Inconsistent file system: BPB_FATSz32 must be non 0 is %x \n", bootSector->BPB_FATSz32);
        close(fd);
        exit(1);
    }

    if (bootSector->BPB_RootClus < 2) {
        printf("Inconsistent file system: BPB_RootClus must be >= 2 is %x \n", bootSector->BPB_RootClus);
        close(fd);
        exit(1);
    }

    if (bootSector->BPB_TotSec32 <= 65525) {
        printf("Inconsistent file system: BPB_TotSec32 must be > 65525 is %d \n", bootSector->BPB_TotSec32);
        close(fd);
        exit(1);
    }

    for (uint16_t i = 0; i < 12; i++) {
        if (bootSector->BPB_reserved[i] != 0) {
            printf("Inconsistent file system: BPB_reserved must be 0s");
            close(fd);
            exit(1);
        }
    }
    printf("  MBR appears to be consistent.\n");
	//--------------------------------------

	//Checking fat and fsinfo --------------
    startingAddy = (bootSector->BPB_RsvdSecCnt) * (bootSector->BPB_BytesPerSec);
    lseek(fd, startingAddy, SEEK_SET);
    read(fd, &fat0, sizeof(fat0));
    read(fd, &fat1, sizeof(fat1));
    if(bootSector->BPB_FSInfo){
        fsInfoOffset = bootSector->BPB_BytesPerSec;   
    }else{
        fsInfoOffset = (bootSector->BPB_FSInfo -1) * (bootSector->BPB_BytesPerSec);   
    }

    if (fat0 != 0x0FFFFFF8){//E0C
        printf("Inconsistent file system: FAT entry 0 should be 0x0FFFFFF8 but is 0x%x\n", fat0);
        close(fd);
        exit(1);
    }
    if ((fat1 & 0x0FFFFFFF) != 0x0FFFFFFF){//E0C
        printf("Inconsistent file system: FAT entry 1 should have lower 28 bits as 0x0FFFFFFF but is 0x%x\n", fat1);
        close(fd);
        exit(1);
    }

	if(bootSector->BPB_FSInfo != 0xFFFF){
        lseek(fd, fsInfoOffset, SEEK_SET);       //FAT: Set for reading lead_sig
        read(fd, &fsInfo ->lead_sig, 4);
        // lseek(fd, 484, SEEK_SET); //NOT READING SIGNATURE HERE FAT: Set for reading signature
        lseek(fd, fsInfoOffset + 484, SEEK_SET); //FAT: Set for reading signature
        read(fd, &fsInfo -> signature,4);
        read(fd, &fsInfo -> free_count,4);       //FAT: Free count righ under signature
    }
    
    if(fsInfo->signature != SIGNATURE){
        printf("incorrect signature: must be =>%x but is %x \n",SIGNATURE, fsInfo->signature);
        close(fd);
        exit(1);
    }    
    
    if(fsInfo->lead_sig != LEAD_SIGNATURE){
        printf("incorrect lead signature: must be =>%x but is %x \n",LEAD_SIGNATURE, fsInfo->lead_sig);
        close(fd);
        exit(1);
    }    
    printf("  FAT appears to be consistent.\n");
    //--------------------------------------
  
    //Initialize global variables ----------
    totalSectorCount = (bootSector->BPB_SecPerClus * bootSector->BPB_BytesPerSec) / sizeof(fat32Dir);
    firstDataSector = bootSector->BPB_RsvdSecCnt + (bootSector->BPB_NumFATs * bootSector->BPB_FATSz32) + RootDirSectors; //page14
    //--------------------------------------

    // //CHECKING: REMOVE BEFOR SUBMISSION ----
    // printf("fat0 => 0x%x\n", fat0);
    // printf("fat1 => 0x%x\n\n", fat1);
    // printf("fsInfoOffset => %x \n",fsInfoOffset);
    // printf("BPB_BytesPerSec => %x \n",bootSector->BPB_BytesPerSec);
    // printf("BPB_RsvdSecCnt => %x \n",bootSector->BPB_RsvdSecCnt);
    // printf("BPB_FSInfo => %x \n\n",bootSector->BPB_FSInfo);
    // printf("lead_sig => %x \n",fsInfo ->lead_sig);
    // printf("signature => %x \n",fsInfo -> signature);
    // printf("freecount (hex)=> %x \n",fsInfo -> free_count);
    // printf("freecount (int)=> %d \n",fsInfo -> free_count);
    // //--------------------------------------
    printf("Done checking\n");
    printf("----------------------------\n");
}
//-------------------------------------------------------


//-------------------------------------------------------
// printInfo
// PURPOSE: Prints the drive information
// INPUT PARAMETERS: none
// NOTE: Borrowed some from lab4
//------------------------------------------
void printInfo(){ 
	//Drive Name ---------------------------
	char *name = (char *)malloc(sizeof(char) * 12);
	for(int i=0; i<11;i++){
		name[i] = bootSector -> BS_VolLab[i];
	}
	name[11] = '\0';
	//--------------------------------------

    // Calculate the total number of clusters and the number of clusters used by the root directory
    uint32_t totalClusters = (bootSector -> BPB_TotSec32 - bootSector -> BPB_RsvdSecCnt - (bootSector->BPB_NumFATs * bootSector->BPB_FATSz32)) / bootSector->BPB_SecPerClus;
    RootDirSectors = ((bootSector->BPB_RootEntCnt * 32) + (bootSector->BPB_BytesPerSec - 1)) / bootSector->BPB_BytesPerSec; //page14
    uint32_t freeCount = 0;
    for (uint32_t i = 0; i < totalClusters + 2; i++){//rootdir usually has 2cluster
        uint32_t fatEntryOffset = startingAddy + i * sizeof(uint32_t);
        lseek(fd, fatEntryOffset, SEEK_SET);       //FAT: Set for reading lead_sig
        uint32_t fatEntry;
        read(fd, &fatEntry, sizeof(uint32_t));
        if ((fatEntry & 0x0FFFFFFF) == 0x00000000){
            freeCount++;
        }
    }	
    //--------------------------------------

	//Free space calculations --------------
	uint32_t clusterSizeBytes = bootSector->BPB_SecPerClus * bootSector->BPB_BytesPerSec;
	uint32_t freeSpaceBytes = freeCount * clusterSizeBytes;
	//-------------------------------------

	//USABLE space calculations ------------
	uint32_t test = bootSector->BPB_TotSec32 - (bootSector->BPB_RsvdSecCnt + (bootSector->BPB_NumFATs*bootSector->BPB_FATSz32)) +RootDirSectors;
	uint32_t totalSize = test * bootSector->BPB_BytesPerSec;
	uint32_t usableSpaceBytes = totalSize - freeSpaceBytes;
	//-------------------------------------

	//Clean print---------------------------
	printf("  DRIVE OEM NAME: %s\n",bootSector->BS_OEMName);
	printf("  DRIVE VOL NAME: %s\n",name);
    printf("  DRIVE Total size: %.2f KB (or %d bytes)\n",(float)totalSize/1024, totalSize);
	printf("  FREE SPACE: %.2f KB (or %d bytes)\n", (float)freeSpaceBytes/1024, freeSpaceBytes);
	printf("  USABLE SPACE: %.2f KB (or %d bytes)\n", (float)usableSpaceBytes/1024, usableSpaceBytes);
	printf("  CLUSTER SIZE: %.2f KB (or %d bytes) \n", (float)clusterSizeBytes/1024, clusterSizeBytes);
	printf("  CLUSTER SIZE: %d (IN NUMBER OF SECTORS)\n", bootSector->BPB_SecPerClus);
    //-----------------------------------------------
	printf("\nDone printing drive info\n");
	free(name);
	//--------------------------------------
}
//-------------------------------------------------------



//HELPER FUNCTIONS ::::::::::::::::::::::::::::::::::::::
//-------------------------------------------------------
// helperFirstSectorofCluster
// PURPOSE: Returns the clusters first sector number
// INPUT PARAMETERS: Bootsector, clusterNumer
// RESOURCE: page 14 of fatSpecs manual
//------------------------------------------
uint32_t helperFirstSectorofCluster(fat32BS *bootSector, uint32_t N){
    return ((N - 2) * bootSector->BPB_SecPerClus) + firstDataSector;
}
//-------------------------------------------------------
// helperIsValid
// PURPOSE: Checks if the directory is valid
// INPUT PARAMETERS: directory information
//------------------------------------------
bool helperIsValid(fat32Dir* dirInfo) {
    return (dirInfo->dir_attr == 0x10 || dirInfo->dir_attr == 0x20) && (dirInfo->dir_name[0] != '.');
}
//-------------------------------------------------------
// helperExtractName
// PURPOSE: returns the directory or file name
// INPUT PARAMETERS: directory name
//------------------------------------------
char* helperExtractName(unsigned char name[11]) {
    char* temp = (char*)malloc(sizeof(char) * 12);
    int i = 0;
    for (; i < 8 && name[i] != ' '; i++) {
        temp[i] = name[i];
    }
    temp[i] = '\0';

    if (name[8] != ' ') {
        int n = strlen(temp);
        temp[n++] = '.';
        for (int a = 8; a < 11; a++) {
            temp[n++] = name[a];
        }
        temp[n] = '\0';
    }
    return temp;
}
//-------------------------------------------------------
// helperNextCluster
// PURPOSE: returns the next cluster
// INPUT PARAMETERS: file, bootsector and cluster number
// RESOURCE: page 15 of fatSpecs manual
//------------------------------------------
uint32_t helperNextCluster(int fd, fat32BS* bootSector, uint32_t N) {
    uint32_t fatSecNumber = bootSector->BPB_RsvdSecCnt + ((N*4) / bootSector->BPB_BytesPerSec);
    uint32_t fatEntryOffset = (N*4) % bootSector->BPB_BytesPerSec;
    uint32_t nextCluster;
    lseek(fd, (bootSector->BPB_BytesPerSec * fatSecNumber) + fatEntryOffset, SEEK_SET);
    read(fd, &nextCluster, sizeof(nextCluster));
    return nextCluster;
}
//-------------------------------------------------------
// helpertextbookBeforeListPrint
// PURPOSE: Prints contents had on the demo video before list called
//------------------------------------------
void helpertextbookBeforeListPrint(){
    //Drive Name ---------------------------
	char *name = (char *)malloc(sizeof(char) * 12);
	for(int i=0; i<11;i++){
		name[i] = bootSector -> BS_VolLab[i];
	}
	name[11] = '\0';
	//Print the content ---------------------
    printf("  Drive name: %s\n",bootSector->BS_OEMName);
    printf("  Drive has %u Bytes per sector and %d sectors per cluster\n", bootSector->BPB_BytesPerSec, bootSector->BPB_SecPerClus);
    printf("  fat0 => 0x%x should be => 0xfffffff8\n", fat0);
    printf("  fat1 => 0x%x should be => 0xffffffff\n", fat1);
    printf("  Searching for root cluster at %d\n", bootSector->BPB_RootClus);
    printf("  Sector %d address is => %d\n", bootSector->BPB_RootClus, firstDataSector);
    printf("  Volume ID: %s\n",name);
    printf("\n");
    //---------------------------------------
}
//------------------------------------------------------
// helperWantReadFiles
// PURPOSE: ask user if want me to read it and print on screen
//------------------------------------------
bool helperWantReadFiles(){
    char size[2];
    printf("  Do you want to print here on screen (to view) while I move it into outputs? answer (y/n): ");
    fgets(size,2,stdin);
    if(size[0] == 'y'){
        return true;
    }
    return false;
}
//-------------------------------------------------------
// helperReadFileContent
// PURPOSE: read file contents and print them on screen
// INPUT PARAMETERS: cluster numbers
//------------------------------------------
void helperReadFileContent(uint32_t clusterNumber) {
    //Get the fileOffset -------------------
    uint32_t firstDataSector = helperFirstSectorofCluster(bootSector, clusterNumber);
    uint32_t fileOffset = firstDataSector * bootSector->BPB_BytesPerSec;
    //--------------------------------------
    
    //--------------------------------------
    //Go through all sectors ----------------
    for (int i = 0; i < totalSectorCount; i++) {
        //Get that specific directory -------
        fat32Dir* dirInfo = (fat32Dir *)malloc(sizeof(fat32Dir));
        lseek(fd, fileOffset + (sizeof(fat32Dir) * i), SEEK_SET);
        read(fd, dirInfo, sizeof(fat32Dir));
        //-----------------------------------
        //Print File content ----------------
        printf("%s", dirInfo->dir_name);
        //-----------------------------------
    }
    //--------------------------------------

    //Get next cluster & check its not done --
    uint32_t nextCluster = helperNextCluster(fd, bootSector, clusterNumber);
    nextCluster = nextCluster & BITMASK;
    if (nextCluster != EOC && nextCluster != BITMASK) {
        helperReadFileContent(nextCluster);
    }
    //--------------------------------------
}
//::::::::::::::::::::::::::::::::END OF HELPER FUNCTIONS



//-------------------------------------------------------
// listContent
// PURPOSE: Prints all the files and directory on drive
// INPUT PARAMETERS: clusterNumbers and count
//------------------------------------------
void listContents(uint32_t clusterNumber, uint32_t count) {
    //Get the fileOffset -------------------
    uint32_t firstDataSector = helperFirstSectorofCluster(bootSector, clusterNumber);
    uint32_t fileOffset = firstDataSector * bootSector->BPB_BytesPerSec;
    //--------------------------------------
    
    //--------------------------------------
    //Go through all sectors ----------------
    for (int i = 0; i < totalSectorCount; i++) {
        //Get that specific directory -------
        fat32Dir* dirInfo = (fat32Dir *)malloc(sizeof(fat32Dir));
        lseek(fd, fileOffset + (sizeof(fat32Dir) * i), SEEK_SET);
        read(fd, dirInfo, sizeof(fat32Dir));
        //-----------------------------------

        //print directory and file names----
        if (helperIsValid(dirInfo)) {
            if (dirInfo->dir_name[0] != 0xE5 && (dirInfo->dir_attr & ATTR_DIRECTORY)) {//name is not free and is a directory
                for (int j = 0; j < (int)count; j++) {
                    printf("-");
                }
                printf("%s\n", helperExtractName(dirInfo->dir_name));
                uint32_t nextCluster = dirInfo->dir_first_cluster_hi << 0x10 | dirInfo->dir_first_cluster_lo;
                listContents(nextCluster, count + 1);
            } else if (dirInfo->dir_name[0] != 0xE5) {//name not free, meaning file is left
                for (int j = 0; j < (int)count; j++) {
                    printf("-");
                }
                printf("File: %s\n", helperExtractName(dirInfo->dir_name));
            }
        }
        //-----------------------------------
    }
    //--------------------------------------

    //Get next cluster & check its not done --
    uint32_t nextCluster = helperNextCluster(fd, bootSector, clusterNumber);
    nextCluster = nextCluster & BITMASK;
    if (nextCluster != EOC && nextCluster != BITMASK) {
        listContents(nextCluster, count);
    }
    //--------------------------------------
}
//-------------------------------------------------------



//------------------------------------------------------
// fetchFile
// PURPOSE: Fetch and return file from drive
// INPUT PARAMETERS: 
//------------------------------------------
bool fileExists = false; //this is for get file in main
bool fetchFile(uint32_t clusterNumber, char *fileName, uint32_t count){
    //Get the fileOffset -------------------
    uint32_t firstDataSector = helperFirstSectorofCluster(bootSector, clusterNumber);
    uint32_t fileOffset = firstDataSector * bootSector->BPB_BytesPerSec;
    bool done = false;
    //--------------------------------------
    
    //Go through all sectors ----------------
    for (int i = 0; i < totalSectorCount && !done; i++) {
        //Get that specific directory -------
        fat32Dir* dirInfo = (fat32Dir *)malloc(sizeof(fat32Dir));
        lseek(fd, fileOffset + (sizeof(fat32Dir) * i), SEEK_SET);
        read(fd, dirInfo, sizeof(fat32Dir));
        //-----------------------------------

        //print directory and file names-----
        if (helperIsValid(dirInfo)) {
            if (dirInfo->dir_name[0] != 0xE5 && (dirInfo->dir_attr & ATTR_DIRECTORY)) {//name is not free and is a directory
                //Checks if have directory --
                uint32_t nextCluster = dirInfo->dir_first_cluster_hi << 0x10 | dirInfo->dir_first_cluster_lo;
                fetchFile(nextCluster, fileName, count + 1);
                if(strcasecmp(helperExtractName(dirInfo->dir_name), fileName) == 0){
                    done = true;
                    printf("  Found Directory: %s\n",helperExtractName(dirInfo->dir_name));
                }
                //-----------------------------
            }
            else if (dirInfo->dir_name[0] != 0xE5) {//name not free, meaning file is left
                //Check if have file ----------
                if (strcasecmp(helperExtractName(dirInfo->dir_name), fileName) == 0) {
                    done = true;
                    printf("  Found File: %s\n", helperExtractName(dirInfo->dir_name));
                    fileExists = true;
    
                    //Ask user if want to display-
                    if(helperWantReadFiles()){
                        helperReadFileContent(dirInfo->dir_first_cluster_lo);
                    }
                    //----------------------------

                    //Copy to outputs -------------
                    printf("  Copying file contents to outputs directory...\n");
                    char outputFilePath[256];
                    snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s", "outputs", helperExtractName(dirInfo->dir_name));
                    int outputFd = open(outputFilePath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    if (outputFd == -1) {
                        printf("  Error opening file for writing.\n");
                        return done;
                    }
                    uint32_t fileSize = dirInfo->dir_file_size;
                    uint32_t remainingSize = fileSize;
                    uint32_t fileCluster = dirInfo->dir_first_cluster_lo;
                    uint32_t bytesRead = 0;
                    while (remainingSize > 0) {
                        uint32_t currentClusterSize = bootSector->BPB_BytesPerSec * bootSector->BPB_SecPerClus;
                        uint32_t bytesToRead = remainingSize < currentClusterSize ? remainingSize : currentClusterSize;
                        uint32_t fileOffset = helperFirstSectorofCluster(bootSector, fileCluster) * bootSector->BPB_BytesPerSec;
                        lseek(fd, fileOffset, SEEK_SET);
                        char buffer[bytesToRead];
                        bytesRead = read(fd, buffer, bytesToRead);
                        if (bytesRead != bytesToRead) {
                            printf("  Error reading file contents.\n");
                            close(outputFd);
                            remove(outputFilePath);
                            return done;
                        }
                        write(outputFd, buffer, bytesRead);
                        remainingSize -= bytesRead;
                        fileCluster = helperNextCluster(fd, bootSector, fileCluster) & BITMASK;
                        if (fileCluster == EOC || fileCluster == BITMASK) {
                            break;
                        }
                    }
                    close(outputFd);
                    printf("Copied successfully to: %s\n",outputFilePath);
                    //----------------------------
                }
                //--------------------------------
            }
        }
    }
    //--------------------------------------

    //Get next cluster & check its not done 
    uint32_t nextCluster = helperNextCluster(fd, bootSector, clusterNumber);
    nextCluster = nextCluster & BITMASK;
    if (nextCluster != EOC && nextCluster != BITMASK) {
        fetchFile(nextCluster, fileName, count);
    }
    //--------------------------------------
    return done;
}
// -------------------------------------------------------
// ************************************************* FUNCTIONS END HERE



//MAIN **************************************************************
int main(int argc, char*argv[]){
    printf("******* STARTING A3  *****\n");
    //Accept file name from user ----------------------
    if (argc <3){ 
        printf("Expected filename and instruction to be passed\n");
        printf("like this => ./my_a3 imgs/3430-good.img info\n");
        exit(1);
    }
    char *fileReading = argv[1]; // printf("Reading disk image => %s\n\n", fileReading);
    //--------------------------------------------------

    //Read file passed ---------------------------------
    fd = open(fileReading, O_RDONLY); 
    if(fd < 0){
        printf("Failed to open file\n");
        exit(1);
    }
    //--------------------------------------------------

    //Prep structs  ------------------------------------
    off_t start = 0x00;     //ensure we at right start
    bootSector = (fat32BS *)malloc(sizeof(fat32BS));
    lseek(fd,start,SEEK_SET);
    read(fd, bootSector, sizeof(fat32BS)); 
    fsInfo = (fat32FSInfo *)malloc(sizeof(fat32FSInfo));
    fat32Dir* dirInfo = (fat32Dir *)malloc(sizeof(fat32Dir));
    lseek(fd,start,SEEK_SET);
    read(fd, dirInfo, sizeof(fat32Dir)); 
    //--------------------------------------------------

    //Ensure consistent & initialize varaibles there ---
    checkConsistent(bootSector, fsInfo);
    //--------------------------------------------------

    //Handle user commands  ----------------------------
    if (strcmp(argv[2], "info") == 0){
        printf("\n----------------------------\n");
        printf("Hold as print %s Drive info ...\n\n",fileReading);
        printInfo();
        printf("----------------------------\n");
    }else if(strcmp(argv[2], "list") == 0){
        printf("\n----------------------------\n");
        printf("Listing all directories & files on drive %s:\n",fileReading);
        helpertextbookBeforeListPrint();
        printf("...STARTS HERE\n");
        listContents(bootSector->BPB_RootClus,0);
        printf("...ENDS HERE\n\nDone listing\n");
        printf("----------------------------\n");
    }else if(strcmp(argv[2], "get") == 0){
        printf("\n----------------------------\n");
        printf("Fetching & returning %s from drive %s:\n", argv[3], fileReading);
        //Calling fetch--------------------------------
        bool done;   
        char *input;
        input = strtok(argv[3], "/");
        while(input !=NULL){
            done = fetchFile(bootSector->BPB_RootClus, input, 0);
            input = strtok(NULL, "/");            
        }
        if(!fileExists){
            printf("Oops.. not valid file try again with valid file name\n");
        }
        //--------------------------------------------
        printf("\nDone fetching\n");
        printf("----------------------------\n");
    }else{
        printf("Please pass either info, list or get (with file path) in cmd\n");
        exit(1);
    }
    //--------------------------------------------------

    //Done  --------------------------------------------
    free(bootSector);
    free(fsInfo); 
    free(dirInfo);
    close(fd);
    printf("******** A3 DONE **********\n\n");
	return EXIT_SUCCESS;// return 0;
	//--------------------------------------------------
}
//**************************************************** MAIN ENDS HERE

