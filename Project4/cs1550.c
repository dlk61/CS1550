/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nIndexBlock;				//where the index block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int) - sizeof(long)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	long lastAllocatedBlock; //The number of the last allocated block
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int) - sizeof(long)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How many entries can one index block hold?
#define	MAX_ENTRIES_IN_INDEX_BLOCK (BLOCK_SIZE/sizeof(long))

struct cs1550_index_block
{
      //All the space in the index block can be used for index entries.
			// Each index entry is a data block number.
      long entries[MAX_ENTRIES_IN_INDEX_BLOCK];
};

typedef struct cs1550_index_block cs1550_index_block;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res;
	int i;
	int path_amt = 0;
	memset(stbuf, 0, sizeof(struct stat));


	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	}
	else {
		// Parse the path
		char sub_directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		path_amt = sscanf(path, "/%[^/]/%[^.].%s", sub_directory, filename, extension);

		struct cs1550_root_directory root_dir;
		FILE * fp;
		//open .disk for binary read/write
		fp = fopen(".disk", "rb+");
		//Read 1 item whose size is sizeof(struct cs1550_root_directory) to root_dir
		fread(&root_dir, sizeof(struct cs1550_root_directory), 1, fp);

		// Iterate through directories array in root
		i = 0;
		int d_index;
		for (d_index = 0; (d_index < root_dir.nDirectories) && !i; d_index++) {

			// If sub directory found
			if (strncmp(root_dir.directories[d_index].dname, sub_directory, MAX_FILENAME + 1) == 0) {

				// Directory
				if (path_amt == 1) {

					stbuf->st_mode = S_IFDIR | 0755;
		        	stbuf->st_nlink = 2;
		        	res = 0; //no error
					break;
				}
				// File
				else {
					struct cs1550_directory_entry sub_dir; 
					long block = root_dir.directories[d_index].nStartBlock;
					fseek(fp, block * BLOCK_SIZE, SEEK_SET);
					fread(&sub_dir, sizeof(struct cs1550_directory_entry), 1, fp);

				
					int f_index;
					for (f_index = 0; f_index < sub_dir.nFiles; f_index++) {
						// If file found
						if ((strncmp(sub_dir.files[f_index].fname, filename, MAX_FILENAME + 1) == 0) && (strncmp(sub_dir.files[f_index].fext, extension, MAX_EXTENSION + 1) == 0)) {
							//regular file, probably want to be read and write
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 1; //file links
							stbuf->st_size = sub_dir.files[f_index].fsize; //file size - make sure you replace with real size!
							res = 0; // no error
							i = 1;
							f_index = f_index - 1;
							break;
						}
					}
					// Not found
					if ((f_index) == sub_dir.nFiles) {
						res = -ENOENT;
						i = 1;
						break;
					}
				}

			}
				
		}

		// If directory not found
		if (path_amt == 1) {
			if (d_index == root_dir.nDirectories) 
				res = -ENOENT;
		}
		else {
			if ((d_index - 1) == root_dir.nDirectories) 
				res = -ENOENT;
		}


		fclose(fp);

	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	int res = 0;

	//the filler function allows us to add entries to the listing
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	struct cs1550_root_directory root_dir;
	FILE * fp;
	//open .disk for binary read/write
	fp = fopen(".disk", "rb+");
	//Read 1 item whose size is sizeof(struct cs1550_root_directory) to root_dir
	fread(&root_dir, sizeof(struct cs1550_root_directory), 1, fp);

	// If path is root
	if (strcmp(path, "/") == 0) {
		// Iterate through directories array in root and add to buffer
		int d_index;
		for (d_index = 0; d_index < root_dir.nDirectories; d_index++) 
			filler(buf, root_dir.directories[d_index].dname, NULL, 0);

	}
	else {
		// Parse path
		char sub_directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
		int path_amt = sscanf(path, "/%[^/]/%[^.].%s", sub_directory, filename, extension);

		if (path_amt == 1) {
			// Iterate through directories array in root
			int d_index;
			for (d_index = 0; d_index < root_dir.nDirectories; d_index++) {
				if (strncmp(root_dir.directories[d_index].dname, sub_directory, MAX_FILENAME + 1) == 0) {

					struct cs1550_directory_entry sub_dir; 
					long block = root_dir.directories[d_index].nStartBlock;
					fseek(fp, block * BLOCK_SIZE, SEEK_SET);
					fread(&sub_dir, sizeof(struct cs1550_directory_entry), 1, fp);

					// Iterate through files in subdirectory and add to buffer
					int f_index;
					for (f_index = 0; f_index < sub_dir.nFiles; f_index++) {
						char filename[MAX_FILENAME + MAX_EXTENSION + 1];
						strncpy(filename, sub_dir.files[f_index].fname, MAX_FILENAME + 1);
						strcat(filename, ".");
						strncat(filename, sub_dir.files[f_index].fext, MAX_EXTENSION + 1);
						filler(buf, filename, NULL, 0);
					}
					break;
				}
					
			}
			if (d_index == root_dir.nDirectories) 
				res = -ENOENT;  // Not found

			
		}
		// Trying to read from file, not directory
		else 
			res = -ENOENT;  // Not found
			
	}
			
	fclose(fp);
	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	char sub_directory[MAX_FILENAME + 1];

	char* target = strdup(path);
	char* token;

	// Parse the path
	token = strtok(target, "/");
	int i = 0;
	while (token != NULL) {
		if (i > 0)  // Directory is not under root dir only
			return -EPERM;
		if (strlen(token) > MAX_FILENAME ) // name is beyond 8 chars
			return -ENAMETOOLONG;
		strncpy(sub_directory, token, MAX_FILENAME + 1);
		token = strtok(NULL, "/");
		i++;
	}
	
	struct cs1550_root_directory root_dir;
	FILE * fp;
	//open .disk for binary read/write
	fp = fopen(".disk", "rb+");
	//Read 1 item whose size is sizeof(struct cs1550_root_directory) to root_dir
	fread(&root_dir, sizeof(struct cs1550_root_directory), 1, fp);

	// Iterate through root array to see if directory already exists
	int d_index;
	for (d_index = 0; d_index < root_dir.nDirectories; d_index++) {
		if (strncmp(root_dir.directories[d_index].dname, sub_directory, MAX_FILENAME + 1) == 0) {
			fclose(fp);
			return -EEXIST; // directory already exists
		}
	}
	
	// If it doesn't already exist, allocate
	if (root_dir.nDirectories < MAX_DIRS_IN_ROOT) {

		strncpy(root_dir.directories[root_dir.nDirectories].dname, sub_directory, MAX_FILENAME + 1);
		root_dir.directories[root_dir.nDirectories].nStartBlock = root_dir.lastAllocatedBlock + 1;
		root_dir.nDirectories++;
		root_dir.lastAllocatedBlock++;
		fseek(fp, 0, SEEK_SET);
		fwrite(&root_dir, sizeof(struct cs1550_root_directory), 1, fp); // write to disk
	}
	
	fclose(fp);
	return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{

	(void) mode;
	(void) dev;
	(void) path;

	// If path is root
	if (strcmp(path, "/") == 0) 
		return EPERM;

	char sub_directory[MAX_FILENAME + 1], filename[MAX_FILENAME + 1], extension[MAX_EXTENSION + 1];
	char* token;

	char* target = strdup(path);
	const char delimeters[] = "/ .";

	// Parse the path
	token = strtok(target, delimeters);
	int i = 0;
	int path_amt = 0;

	while (token != NULL) {

		if (i == 0) {
			path_amt = 1;
			if (strlen(token) > MAX_FILENAME)
				return -ENAMETOOLONG;
			strncpy(sub_directory, token, MAX_FILENAME + 1);
		}
		else if (i == 1) {
			path_amt = 2;
			if (strlen(token) > MAX_FILENAME)
				return -ENAMETOOLONG;
			strncpy(filename, token, MAX_FILENAME + 1);
		}
		else if (i == 2) {
			path_amt = 3;
			if (strlen(token) > MAX_EXTENSION)
				return -ENAMETOOLONG;
			strncpy(extension, token, MAX_EXTENSION + 1);
		}
		else {
			path_amt = -1;
			return -EPERM;
		}

		token = strtok(NULL, delimeters);
		i++;
	}

	// file is trying to be created in root dir
	if (path_amt < 3 && path_amt > 0) 
		return -EPERM;

	struct cs1550_root_directory root_dir;
	FILE * fp;
	//open .disk for binary read/write
	fp = fopen(".disk", "rb+");
	//Read 1 item whose size is sizeof(struct cs1550_root_directory) to root_dir
	fread(&root_dir, sizeof(struct cs1550_root_directory), 1, fp);

	// Iterate through root array to see if directory exists
	int d_index;
	for (d_index = 0; d_index < root_dir.nDirectories; d_index++) {

		// If directory exists
		if (strncmp(root_dir.directories[d_index].dname, sub_directory, MAX_FILENAME + 1) == 0) {

			struct cs1550_directory_entry sub_dir; 
			long block = root_dir.directories[d_index].nStartBlock;
			fseek(fp, block * BLOCK_SIZE, SEEK_SET);
			fread(&sub_dir, sizeof(struct cs1550_directory_entry), 1, fp);

			int f_index;
			for (f_index = 0; f_index < sub_dir.nFiles; f_index++) {
				// If file exists
				if ((strncmp(sub_dir.files[f_index].fname, filename, MAX_FILENAME + 1) == 0) && (strncmp(sub_dir.files[f_index].fext, extension, MAX_EXTENSION + 1) == 0)) {
					fclose(fp);
					return -EEXIST; // file already exists
				}
			}

			// If sub directory has less than the max, allocate
			if (sub_dir.nFiles < MAX_FILES_IN_DIR) {

				strncpy(sub_dir.files[sub_dir.nFiles].fname, filename, MAX_FILENAME + 1);			//update name
				strncpy(sub_dir.files[sub_dir.nFiles].fext, extension, MAX_EXTENSION + 1);			//update extension
				sub_dir.files[sub_dir.nFiles].nIndexBlock = root_dir.lastAllocatedBlock + 1;       //update index block
				sub_dir.files[sub_dir.nFiles].fsize = 0;
				sub_dir.nFiles++;																   //increase number of files in sub directory
				root_dir.lastAllocatedBlock++;													   //increase the last allocated block

				struct cs1550_index_block idx;
				fseek(fp, root_dir.lastAllocatedBlock * BLOCK_SIZE, SEEK_SET);
				//Read 1 item whose size is sizeof(struct cs1550_root_directory) to root_dir
				fread(&idx, sizeof(struct cs1550_index_block), 1, fp);

				idx.entries[0] = root_dir.lastAllocatedBlock + 1;
				idx.entries[1] = -1;

				fseek(fp, root_dir.lastAllocatedBlock * BLOCK_SIZE, SEEK_SET);
				fwrite(&idx, sizeof(cs1550_index_block), 1, fp);

				root_dir.lastAllocatedBlock++;

				fseek(fp, 0, SEEK_SET);
				fwrite(&root_dir, sizeof(struct cs1550_root_directory), 1, fp);


				fseek(fp, root_dir.directories[d_index].nStartBlock * BLOCK_SIZE, SEEK_SET);
				fwrite(&sub_dir, sizeof(struct cs1550_directory_entry), 1, fp);

			}
			break;
		}
	}

	fclose(fp);
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	size = 0;

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	return size;
}

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}

/* Thanks to Mohammad Hasanzadeh Mofrad (@moh18) for these
   two functions */
static void * cs1550_init(struct fuse_conn_info* fi)
{
	  (void) fi;
    printf("We're all gonna live from here ....\n");
		return NULL;
}

static void cs1550_destroy(void* args)
{
		(void) args;
    printf("... and die like a boss here\n");
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
		.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
		.mknod	= cs1550_mknod,
		.unlink = cs1550_unlink,
		.truncate = cs1550_truncate,
		.flush = cs1550_flush,
		.open	= cs1550_open,
		.init = cs1550_init,
    .destroy = cs1550_destroy,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
