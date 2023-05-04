# FAT32 File-System-Analyzer 

**Author:** Sami Byaruhanga

**Program Files included:**  fat32.c, makefile, README.md

**Purpose:** Checking filesystem info, list, get

# Running Program
0. Navigate to the directory with the groam files included above
1. Run ```make'''
2. After running make, you can run info, list, get commands 
3. info use: ```./fat32 imagename info``` e.g., ```./fat32 imgs/test.img info```
3. list use: ```./fat32 imagename list``` e.g., ```./fat32 imgs/test.img list```
4. get  use: ```./fat32 imagename get path``` e.g., ```./fat32 imgs/test.img get FOLDER1/TEST.TXT```

### Notes on Get command
1. It is case sensitive so just print it as above i.e., directory and the file you want (ITS SHORT FILE NAME)
2. After you pass it you will be promoted if you want output on screen as its being fetched 
    PLEASE ANSWER with -> y/n
3. The contents will be found int the outputs folder


**"Always think what you do is easy and it shall be so" - Emile Coue**
