#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#define _GNU_SOURCE
#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8196
#define NUM_FILES  128
#define MAX_FILE_SIZE 10240000 //10 Megabytes

uint8_t blocks[NUM_BLOCKS][BLOCK_SIZE];

//max 20 character diskImage name->
//This is a global variable that refers to the current disk image in use------------>
char diskImage[20] = "";

struct Directory_Entry
{
	uint8_t valid;
	char filename[255];
	uint32_t inode;
};


struct Inode
{
	uint8_t attribs;
	/*
	Attribs-------------->
	0 -> not hidden, not read-only
	1 -> not hidden, read-only
	2 -> hidden, not read-only
	3 -> hidden, read-only
	*/
	uint8_t valid;
	uint32_t size;
	uint32_t blocks[1250];
	//time_t time; //Time created
};


uint8_t *freeBlockList;
uint8_t *freeInodeList;
struct Directory_Entry *dir;
FILE* fd;
struct Inode *inodes;

void initializeDirectory()
{
	int i;
	for(i=0;i<NUM_FILES;i++)
	{
		dir[i].valid = 0;
		dir[i].inode = -1;

		memset (dir[i].filename,0,32);
		//memset (dir[i].filename,0,255);
	}
}


void initializeBlockList()
{
	int i;
	for(i=0;i<NUM_BLOCKS;i++)
	{
		//1 = free, 0 = used
		freeBlockList[i] = 1;
	}
}


void initializeInodeList()
{
	int i;
	for(i=0;i<NUM_FILES;i++)
	{
		//1 = free, 0 = used
		freeInodeList[i] = 1;
	}
}

void initializeInodes()
{
	int i;
	int j;
	for(i=0;i<NUM_FILES;i++)
	{
		inodes[i].valid = 0;
		inodes[i].size = 0;
		inodes[i].attribs = 0;
		for(j=0;j<1250;j++)
		{
			inodes[i].blocks[j] = -1;
		}
		//memset (inodes[i].time,0,20);	
	}
}



//How much space is free:
int df()
{
	int i;
	int free_space = 0;
	for(i=132;i<NUM_BLOCKS;i++)
	{
		if(freeBlockList[i]) free_space += BLOCK_SIZE; 
	}
	return free_space;
}


int findFreeInode()
{
	int i;
	int ret = -1;
	for(i=0;i<NUM_FILES;i++)
	{
		if (inodes[i].valid==0)
		{
			inodes[i].valid = 1;
			return i;
		}
	}

	return ret;
}

int findFreeBlock()
{
	int i;
	int ret = -1;

	for(i=132;i<NUM_BLOCKS;i++)
	{
		if(freeBlockList[i] == 1)
		{
			freeBlockList[i] = 0;
			return i;
		}
	}

	return ret;
}

int findDirectoryEntry(char* filename)
{
	//Check for existing entry
	int i;
	int ret = -1;
	for(i=0;i<NUM_FILES;i++)
	{
		if (strcmp(filename,dir[i].filename) == 0)
		{
			return i;
		}
	}

	for(i=0;i<NUM_FILES;i++)
	{
		if (dir[i].valid == 0)
		{
			//dir[i].valid = 1;
			return i;
		}
	}

	return ret;
}

int findDirIdx(char* filename)
{
	//Check where directory is
	//Return -1 if Directory doesn't exist
	int i;
	int ret = -1;
	for(i=0;i<NUM_FILES;i++)
	{
		if (strcmp(filename,dir[i].filename) == 0)
		{
			return i;
		}
	}
	return ret;
}

//Debug function to print all blocks
void Dbg_print_block()
{
	int i,j;
	for (i=0;i<NUM_BLOCKS;i++) 
	{
		for (j=0;j<BLOCK_SIZE;j++) 
		{
		printf("%c",blocks[i][j]);
		}
	}
}


//PUT IMPLEMENTATION------------------------->
int put (char *filename)
{
	struct stat buf;
	int ret;

	ret = stat(filename,&buf);

	if (ret == -1)
	{
		printf("File does not exist\n");
		return -1;
	}

	int size = buf.st_size; //Size of file to be input

	if (size>MAX_FILE_SIZE)
	{
		printf("File size too big\n");
		return -1;
	}

	if(size>df())
	{
		printf("File exceeds remaining disk space\n");
		return -1;	
	}

	int directoryIndex = findDirectoryEntry(filename);
	int free_block;
	int inodeIndex;

	
	//IF file already Exists----------->
	if (dir[directoryIndex].valid)
	{
		//Choice to overwrite---------->
		char OWchoice;
		printf("File %s already exists. Do you want to overwrite?(y/n) ?",filename);
		scanf("%c",&OWchoice);
		if(OWchoice == 'n') 
		{
			printf("No changes made to file.\n");
			return 1;
		}
		else
		{
			//Use the same inode and blocks
			inodeIndex = dir[directoryIndex].inode;
			free_block = inodes[inodeIndex].blocks[0];
		}

	}

	//If file doesn't exist already on FS-------->
	else
	{
		inodeIndex = findFreeInode();
		free_block = findFreeBlock();
	}

    printf("Free block = %d\n",free_block);

    //Set the time------------------>
    //inodes[inodeIndex].time = buf.st_mtime;
	//Set the filename----------->
	strncpy(dir[directoryIndex].filename,filename,strlen(filename));


	//Set Inode and validity for directory:
	dir[directoryIndex].inode = inodeIndex;
	dir[directoryIndex].valid = 1;

	//Set Inode:
	inodes[inodeIndex].attribs = 0; //Default value of not hidden and not read-only
	inodes[inodeIndex].size = size; //Size of fiel
 
    // Open the input file read-only 
    FILE *ifp = fopen ( filename, "r" ); 
    printf("Reading %d bytes from %s\n", size, filename );

	// Save off the size of the input file since we'll use it in a couple of places and 
    // also initialize our index variables to zero. 
    int copy_size   = size;


    // We want to copy and write in chunks of BLOCK_SIZE. So to do this 
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int offset      = 0;               

    // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
    // memory pool. Why? We are simulating the way the file system stores file data in
    // blocks of space on the disk. block_index will keep us pointing to the area of
    // the area that we will read from or write to.

    int block_index = free_block;
    int inode_block_idx = 0;
    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size > 0 )
    {
    	inodes[inodeIndex].blocks[inode_block_idx] = block_index;
    	//block_index = findFreeBlock();

		// Index into the input file by offset number of bytes.  Initially offset is set to
		// zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
		// then increase the offset by BLOCK_SIZE and continue the process.  This will
		// make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
		fseek( ifp, offset, SEEK_SET );

		// Read BLOCK_SIZE number of bytes from the input file and store them in our
		// data array. 
		int bytes  = fread( blocks[block_index], BLOCK_SIZE, 1, ifp );

		// If bytes == 0 and we haven't reached the end of the file then something is 
		// wrong. If 0 is returned and we also have the EOF flag set then that is OK.
		// It means we've reached the end of our input file.
		if( bytes == 0 && !feof( ifp ) )
		{
			printf("An error occured reading from the input file.\n");
			return -1;
		}

		// Clear the EOF file flag.
		clearerr( ifp );

		// Reduce copy_size by the BLOCK_SIZE bytes.
		copy_size -= BLOCK_SIZE;

		// Increase the offset into our input file by BLOCK_SIZE.  This will allow
		// the fseek at the top of the loop to position us to the correct spot.
		offset    += BLOCK_SIZE;


		//Mark block as used:
		freeBlockList[block_index] = 0;

		// Increment the index into the block array 
		block_index ++;
		inode_block_idx++;
	}

	fclose( ifp );
return 1;
}


//List Functionality------------------------->
void list(int choice)
{	
	//choice 0 = print all
	//choice 1 = print non-hidden
	int i;
	int filecount = 0;
		for(i=0;i<NUM_FILES;i++)
		{
			
			if(dir[i].valid)
			{
			
				int inode_idx = dir[i].inode;
				if(choice == 0)
				//this choice is for all files
				{
					printf("filename:%s size:%d bytes.\n",dir[i].filename,inodes[inode_idx].size);
					filecount++;
				}

				else
				{
				//this choice is for non-hidden files
					if(inodes[inode_idx].attribs == 0 || inodes[inode_idx].attribs == 1)
					{
						printf("filename:%s size:%d bytes.\n",dir[i].filename,inodes[inode_idx].size);
						filecount++;
					}
				}
			}
		}
	if(filecount == 0) printf("No files found.\n");
} 

//Delete Implementation
void del(char* filename)
{
	int dirIDx = findDirIdx(filename);

    //Check if file exists in File system-------------->
    if (dirIDx == -1)
    {
    	printf("No file with name %s exists in FS.\n",filename);
    	return;
    }
    int inodeIndex = dir[dirIDx].inode;


    //Set Everything about the directory to initital values---------->
    dir[dirIDx].valid = 0;
	dir[dirIDx].inode = -1;
	memset (dir[dirIDx].filename,0,32);

	//Set everything about inodes to initial values-------------->
	inodes[inodeIndex].valid = 0;
	inodes[inodeIndex].size = 0;
	inodes[inodeIndex].attribs = 0;
	//Reset the time created---------->
	

	//Set all the blocks and the inode to free---------->
	freeInodeList[inodeIndex] = 1;
	int j;

	//Traverse the block list in inodes and set every block to free
	for(j=0;j<1250;j++)
	{
		if(inodes[inodeIndex].blocks[j]==-1) 
		{
			break;
		}
		else
		{
			freeBlockList[inodes[inodeIndex].blocks[j]] = 1;
		}
	}


	for(j=0;j<1250;j++)
	{
		inodes[inodeIndex].blocks[j] = -1;
	}

	printf("Successfully deleted %s from the File System.\n",filename);

}

void attribSet(char* attrib, char* filename)
{
	int dirIDx = findDirIdx(filename);
	printf("Attribute to add: %s\n",attrib);
    //Check if file exists in File system-------------->
    /*
	Attribs-------------->
	0 -> not hidden, not read-only
	1 -> not hidden, read-only
	2 -> hidden, not read-only
	3 -> hidden, read-only
	*/
    if (dirIDx == -1)
    {
    	printf("No file with name %s exists in FS.\n",filename);
    	return;
    }
    int inodeIndex = dir[dirIDx].inode;

    //Preserve the other attribute for file and only change one attribute
    if(strcmp(attrib,"+h") == 0)
    {
    	if(inodes[inodeIndex].attribs == 0) inodes[inodeIndex].attribs = 2;
    	if(inodes[inodeIndex].attribs == 1) inodes[inodeIndex].attribs = 3;
    	printf("Successfully made %s hidden.\n",filename);
    }
    else if(strcmp(attrib,"-h") == 0)
    {
    	if(inodes[inodeIndex].attribs == 2) inodes[inodeIndex].attribs = 0;
    	if(inodes[inodeIndex].attribs == 3) inodes[inodeIndex].attribs = 1;
    	printf("Successfully made %s not hidden.\n",filename);
    }
    else if(strcmp(attrib,"+r") == 0)
    {
    	if(inodes[inodeIndex].attribs == 0) inodes[inodeIndex].attribs = 1;
    	if(inodes[inodeIndex].attribs == 2) inodes[inodeIndex].attribs = 3;
    	printf("Successfully made %s read-only.\n",filename);
    }
    else if(strcmp(attrib,"-r") == 0)
    {
    	if(inodes[inodeIndex].attribs == 1) inodes[inodeIndex].attribs = 0;
    	if(inodes[inodeIndex].attribs == 3) inodes[inodeIndex].attribs = 2;
    	printf("Successfully made %s not read-only.\n",filename);
    }
    else{
    	printf("Wrong attribute +h = hidden, -h = unhidden +r = read-only -r = not read-only.\n");
    }
}

//GET IMPLEMENTATION-------------->
void get(char* filename)
{
	//Open the filename file 
	int dirIDx = findDirIdx(filename);

    //Check if file exists in File system-------------->
    if (dirIDx == -1)
    {
    	printf("No file with name %s exists in FS.\n",filename);
    	return;
    }

	FILE *ofp;
    ofp = fopen(filename, "w");

    if( ofp == NULL )
    {
      printf("Could not open output file: %s\n", filename );
      perror("Opening output file returned");
      return -1;
    }

    // Initialize our offsets and pointers just we did above when reading from the file.
    int i;
    
    //Find the inode for the directory
    int inodeIndex = dir[dirIDx].inode;


    //START WRITING FROM FIRST BLOCK
    int block_index = inodes[inodeIndex].blocks[0];
    int copy_size   = inodes[inodeIndex].size;
    int offset      = 0;

    printf("Writing %d bytes to %s\n", copy_size, filename );
    while(copy_size > 0)
    { 

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( blocks[block_index], num_bytes, 1, ofp ); 

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      block_index ++;

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( ofp, offset, SEEK_SET );
    }

    // Close the output file, we're done. 
    fclose( ofp );

}

void getandCopy(char* infilename,char* outfilename)
{
	int i;
    int dirIDx = findDirIdx(infilename);


    //Check if file exists in File system-------------->
    if (dirIDx == -1)
    {
    	printf("No file with name %s exists in FS.\n",infilename);
    	return;
    }

	//Check validity of output file----------------------->
	FILE *ofp;
    ofp = fopen(outfilename, "w");

    if( ofp == NULL )
    {
      printf("Could not open output file: %s\n", outfilename );
      perror("Opening output file returned");
      return -1;
    }

    // Initialize our offsets and pointers just we did above when reading from the file.
    //Find the inode for the directory
    int inodeIndex = dir[dirIDx].inode;

    //START WRITING FROM FIRST BLOCK
    int block_index = inodes[inodeIndex].blocks[0];
    int copy_size   = inodes[inodeIndex].size;
    int offset      = 0;

    printf("Writing %d bytes to %s\n", copy_size, outfilename );
    while(copy_size > 0)
    { 

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( blocks[block_index], num_bytes, 1, ofp ); 

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      block_index ++;

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( ofp, offset, SEEK_SET );
    }

    // Close the output file, we're done. 
    fclose( ofp );

}

//CLoses current file system--------------->
void closeDiskImage()
{
	//TODO-----------PUT FILES FROM RAM TO Disk Image
	FILE *ofp;
    ofp = fopen(diskImage, "w");

    int size = NUM_BLOCKS * BLOCK_SIZE; //Read all the file system
    int block_index = 0; //Start from the first block
    int offset = 0;

	while(size > 0)
    { 

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( size < BLOCK_SIZE )
      {
        num_bytes = size;
      }
      else 
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( blocks[block_index], num_bytes, 1, ofp ); 

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      block_index ++;

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( ofp, offset, SEEK_SET );
    }

	printf("Successfully closed File System %s.\n",diskImage);
	memset (diskImage,0,32);
}

//OPEN DISK IMAGE TO READ FROM-------------->
void openDiskImage(char* filename)
{
	
	struct stat buf;
	int ret;

	ret = stat(filename,&buf);

	if (ret == -1)
	{
		printf("File System Image does not exist\n");
		return -1;
	}
	FILE *ifp = fopen ( filename, "r" ); 

	int copy_size = buf.st_size; //Find disk size
	int block_index = 0; //STart from first block
	int offset = 0;

	while( copy_size > 0 )
    {
		// Index into the input file by offset number of bytes.  Initially offset is set to
		// zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
		// then increase the offset by BLOCK_SIZE and continue the process.  This will
		// make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
		fseek( ifp, offset, SEEK_SET );

		// Read BLOCK_SIZE number of bytes from the input file and store them in our
		// data array. 
		int bytes  = fread( blocks[block_index], BLOCK_SIZE, 1, ifp );

		// If bytes == 0 and we haven't reached the end of the file then something is 
		// wrong. If 0 is returned and we also have the EOF flag set then that is OK.
		// It means we've reached the end of our input file.
		if( bytes == 0 && !feof( ifp ) )
		{
			printf("An error occured reading from the input file.\n");
			return -1;
		}

		// Clear the EOF file flag.
		clearerr( ifp );

		// Reduce copy_size by the BLOCK_SIZE bytes.
		copy_size -= BLOCK_SIZE;

		// Increase the offset into our input file by BLOCK_SIZE.  This will allow
		// the fseek at the top of the loop to position us to the correct spot.
		offset    += BLOCK_SIZE;

		// Increment the index into the block array 
		block_index ++;
	}

	fclose( ifp );
	strncpy(diskImage,filename,strlen(filename));

	printf("%s opened successfully for I/O.\n",diskImage);
}

//Create File System------------------>
void createfs(char* filename)
{
	//Create your file------------------>
	dir = (struct Directory_Entry*) &blocks[0];
	freeInodeList = (uint8_t*) &blocks[7];
	freeBlockList = (uint8_t*) &blocks[8];
	inodes = (struct Inode*) &blocks[9];

	//Initialize Everything
	initializeDirectory();
	initializeInodeList();
	initializeBlockList();
	initializeInodes();

	fd = fopen(filename,"w");
	memset(&blocks[10],0,(NUM_BLOCKS - 10) * BLOCK_SIZE);
	fwrite(&blocks[0],NUM_BLOCKS,BLOCK_SIZE,fd);
	fclose(fd);
	printf("File system %s created successfully.\n",filename);
	strncpy(diskImage,filename,strlen(filename));
}



int main()
{
	//Initialize everything for operation------------------>
	dir = (struct Directory_Entry*) &blocks[0];
	freeInodeList = (uint8_t*) &blocks[7];
	freeBlockList = (uint8_t*) &blocks[8];
	inodes = (struct Inode*) &blocks[9];

	initializeDirectory();
	initializeInodeList();
	initializeBlockList();
	initializeInodes();

	/*createfs("disk.image");

	//printf("%d",df());*/

	char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
	
	while( 1 )
	{
	    // Print out the mfs prompt
		printf ("mfs> ");

	    // Read the command from the commandline.  The
	    // maximum command that will be read is MAX_COMMAND_SIZE
	    // This while command will wait here until the user
	    // inputs something since fgets returns NULL when there
	    // is no input
	    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

	    // Parse input 
	    char *token[MAX_NUM_ARGUMENTS];

	    int   token_count = 0;                                 
	                                                           
	    // Pointer to point to the token
	    // parsed by strsep
	    char *arg_ptr;                                         
	                                                           
	    char *working_str  = strdup( cmd_str );                

	    // we are going to move the working_str pointer so
	    // keep track of its original value so we can deallocate
	    // the correct amount at the end
	    char *working_root = working_str;

	    // Tokenize the input stringswith whitespace used as the delimiter
	    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
	    {
			token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
			if( strlen( token[token_count] ) == 0 )
			{
			    token[token_count] = NULL;
			}
			token_count++;
	    }

	    /*int token_index  = 0;
	    for( token_index = 0; token_index < token_count; token_index ++ ) 
	    {
	     	printf("token[%d] = %s\n", token_index, token[token_index] );  
	    }*/
	    if(token[0] == '\0')
	    {
	    	continue;
	    }

	  	//FOR PUT---------------------->
	  	else if(strcmp(token[0],"put")==0)
	    {
	    	//Check if only has two arguments------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 3) 
	    	{
	    		printf("Wrong use of put command. Invalid no. of arguments\n");
	    	}

	    	else
	    	{
	    		put(token[1]);
	    	}
	    }

	    else if(strcmp(token[0],"del")==0)
	    {
	    	//Check if only has two arguments------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 3) 
	    	{
	    		printf("Wrong use of del command. Invalid no. of arguments\n");
	    	}

	    	else
	    	{
	    		del(token[1]);
	    	}
	    }

	    //FOR GET----------------------------->
	    else if(strcmp(token[0],"get")==0)
	    {
	    	//Check if only has two or, three arguments------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 3 && token_count != 4) 
	    	{
	    		printf("Wrong use of get command. Invalid no. of arguments\n");
	    	}

	    	else if (token_count == 3) 
	    	{
	    		get(token[1]);
	    	}

	    	else if (token_count == 4) 
	    	{
	    		getandCopy(token[1],token[2]);
	    	}
	    }

	    //For List-------------------------->
	    else if(strcmp(token[0],"list")==0)
	    {
	    	//Check if there is a file in the system
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	//Check if only has one or two arguments
	    	
	    	if(token_count == 2) list(1); //Prints non-hidden files

	    	else if(strcmp(token[1],"-h")==0) list(0); //Prints all files in system

	    	else
	    	{
	    		printf("Invalid use of list command, Try list or list -h.\n");
	    	}

	    }

	    //FOR df---------------------->
	  	else if(strcmp(token[0],"df")==0)
	    {
	    	//Check if only has one argument------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 2) 
	    	{
	    		printf("Wrong use of df command. df takes no arguments\n");
	    	}

	    	else
	    	{
	    		printf("You have %d bytes space free.\n",df());
	    	}
	    }

	    //For attrib-------------------->
	    else if(strcmp(token[0],"attrib")==0)
	    {
	    	//Check if only has three arguments------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 4) 
	    	{
	    		printf("Wrong use of attrib command. Attrib takes in one attribute and one filename\n");
	    	}

	    	else
	    	{
	    		attribSet(token[1],token[2]);
	    	}
	    }

		//For creating file system-------------------->
	    else if(strcmp(token[0],"createfs")==0)
	    {
	    	//Check if only has two arguments------------>
	    	if(token_count != 3) 
	    	{
	    		printf("Wrong use of createfs command. Invalid no. of arguments\n");
	    	}

	    	else
	    	{
	    		createfs(token[1]);
	    	}
	    }

	    else if(strcmp(token[0],"close")==0)
	    {
	    	//Check if only has one argument------------>
	    	if(strlen(diskImage) == 0)
	    	{
	    		printf("No disk image open. Please use createfs or, open command before using this command\n");
	    		continue;
	    	}
	    	if(token_count != 2) 
	    	{
	    		printf("Wrong use of close command. Close doesn't take any arguments.\n");
	    	}

	    	else
	    	{
	    		closeDiskImage();
	    	}
	    }

	    else if(strcmp(token[0],"open")==0)
	    {
	    	//Check if only has two arguments------------>
	    	if(token_count != 3) 
	    	{
	    		printf("Wrong use of open command. Open takes in one argument for diskImage.\n");
	    	}

	    	else
	    	{
	    		openDiskImage(token[1]);
	    	}
	    }

	    else if(strcmp(token[0],"printblock")==0)
	    {
	    	Dbg_print_block();
	    }

	    else if((strcmp(token[0],"exit")==0)||(strcmp(token[0],"quit")==0)) 
	    {
	    	return 0;
	    }

	    else
	    {
	    	printf("INVALID COMMAND.\n");
	    	continue;
	    }

	    // Now print the tokenized input as a debug check
	    // \TODO Remove this code and replace with your shell functionality

	    
	    free( working_root );

	    

	}
	  return 0;
}	

