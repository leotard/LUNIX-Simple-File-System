#include "sfs_api.h"
#include <math.h>

#define MAX_FILES 199
#define BLOCK_SIZE 1024
#define MAX_BLOCKS 1024
#define MAX_FD_ENTRIES 32
#define MAX_FILENAME_LENGTH 10
#define MAX_INODE_BLOCKS 14

typedef struct inode{
    int size;
    int direct[MAX_INODE_BLOCKS];
    int indirect;
}inode_t;

typedef struct superblock{
    int magic;
    int bsize;
    int file_system_size;
    int numOf_inodes;
    inode_t root;
    inode_t shadow[4];
    int castshadow;
}superblock_t;

typedef struct dir_entry{
    char file_name[MAX_FILENAME_LENGTH];
    int inode_num;
}dir_entry_t;

typedef struct fd{
    int pwrite;
    int pread;
    int inode_num;
}fd_t;

inode_t root;
superblock_t superblock;
inode_t directory_inode;

int free_blocks_total = MAX_BLOCKS - 3;
short FBM[MAX_BLOCKS]={1};
short WM[MAX_BLOCKS]={1};

inode_t inodes_file[MAX_FILES];
int inode_count=1;
dir_entry_t dir[MAX_FILES-1];

fd_t fd_table[MAX_FD_ENTRIES];
int openFiles_num=0;
short free_fd[MAX_FD_ENTRIES]={1};




int remove_dir_entry(char* fileTo_remove){

    int inode_num;
    int max_index = (directory_inode.size);
    if (max_index==0){
        return -1;
    }
    int foundFile=0;
    for (int i=0; i<max_index;i++){
        if(foundFile==0 && (strcmp(dir[i].file_name, fileTo_remove)!=0)){
            inode_num= dir[i].inode_num;
            foundFile=1;
        }

        if (foundFile==1){
            if(i<max_index-1)
                dir[i]=dir[i+1];
            else if(i==max_index-1){
                *(dir[i].file_name)='\0';
                dir[i].inode_num=-1;
            }
        }
    }

    if (foundFile==1){
        directory_inode.size= directory_inode.size-1;
        return inode_num;
    }else
        return -1;
}


int remove_inode(int inodeTo_remove){

    int defaultArray[]= {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    int inode_num = inodeTo_remove;
    int max_index = (root.size);
    if (max_index==0){
        return -1;
    }

    int foundFile=0;

    if(inodeTo_remove <max_index)
            foundFile=1;
    else
        return -1;

    int blockWritten= ceil(((double)inodes_file[inodeTo_remove].size)/(double)BLOCK_SIZE);

    for(int j = 0; j< blockWritten; j++){
        FBM[inodes_file[inodeTo_remove].direct[j]]=1;
        inodes_file[inodeTo_remove].direct[j]=-1;
    }

    for (int i=inodeTo_remove; i<max_index;i++){

        if (foundFile==1){
            if(i<max_index-1)
                inodes_file[i]=inodes_file[i+1];
            else if(i==max_index-1){
                inodes_file[i].indirect=-1;
            }
        }
    }

    if (foundFile==1){
        root.size= root.size-1;
        return 0;
    }else
        return -1;
}


int remove_fd_entry(int file_toClose){

   // if (openFiles_num==0){
       // return -1;
    //}

    int foundFile=0;
    if(file_toClose<MAX_FD_ENTRIES && free_fd[file_toClose]==0){
        foundFile=1;
        fd_table[file_toClose].inode_num=-1;
        fd_table[file_toClose].pread='\0';
        fd_table[file_toClose].pwrite='\0';
        free_fd[file_toClose]=1;
    }

    if (foundFile==1){
        openFiles_num= openFiles_num-1;
        printf("File '%d'closed openFiles_num: '%d'\n",file_toClose,openFiles_num);
        return 0;
    }else
        return -1;
}

int write_data(inode_t* inode_data, size_t buff_size, void* buffer, int starting_block){
    int numblocks_to_wr= ceil((double)buff_size/(double)BLOCK_SIZE);
    int inode_total_blocks = (starting_block + numblocks_to_wr);
    int block_to_free;


    for (int k=starting_block; k<MAX_INODE_BLOCKS;k++){
        if((inode_data->size)/BLOCK_SIZE > k){
            block_to_free = inode_data->direct[k];
            FBM[block_to_free]=1;
            free_blocks_total=free_blocks_total+1;
        }
    }

    if (numblocks_to_wr>(free_blocks_total)){
        printf("File too big, no more space available. Delete files.");
        return -1;
    }

    int j=1;
    int i=starting_block;
    if (inode_total_blocks<MAX_INODE_BLOCKS+1){
        while ( i<inode_total_blocks){

        //won't iterate throught 1st and last 2 blocks
        //since by design they are never free
            while(j<MAX_BLOCKS-2){

                if(FBM[j]==1){
                    write_blocks(j,1,buffer+(i*BLOCK_SIZE));
                    FBM[j]=0;
                    inode_data->direct[i]=j;
                    i++;
                    free_blocks_total=free_blocks_total-1;
                    break;
                }
                j++;
            }
        }
    }



    return 0;
}

//assume read starts at begining of the block
//but may end wherever in the final block
//the intuition with stating block is that, if it's >0
int read_data(inode_t* inode_data,size_t buff_size, void* buffer, int starting_block){

    int numblocks_to_read= ceil((double)buff_size/(double)BLOCK_SIZE);

    void* blockRead = (void*) malloc(BLOCK_SIZE);
    int blockToRead;
    //how much data of the last block has to be read to buffer
    int last_block_fraction;

    //checks if the file system is empty or not
    if(free_blocks_total==MAX_BLOCKS-3)
        return -1;






        int i = starting_block;
        //read every data block except last one
        for (i; i<numblocks_to_read-1;i++){
            blockToRead = inode_data->direct[i];
            read_blocks(blockToRead,1,blockRead);
            memcpy(buffer+(i*BLOCK_SIZE), blockRead, BLOCK_SIZE);
        }
        //reads only partial data needed from last block
        if(i==numblocks_to_read-1){

            blockToRead = inode_data->direct[i];
            read_blocks(blockToRead,1,blockRead);

            last_block_fraction = buff_size - (numblocks_to_read - 1)*BLOCK_SIZE;
            memcpy(buffer+(i*BLOCK_SIZE), blockRead, last_block_fraction);
        }

    return 0;

}

void mkssfs(int fresh){



    char* fileSystem_ = "file_system";

    //checks if it's a new file sytem or exsting one that needs to be opened
    if(fresh == 1){
        if(init_fresh_disk(fileSystem_,BLOCK_SIZE, MAX_BLOCKS)== -1 )
            printf("Could not create new file system\n");
        else{

            for(int i=0; i<MAX_FD_ENTRIES;i++)
                free_fd[i]=1;

            for(int i=0; i<MAX_BLOCKS;i++)
                WM[i]=1;

            for(int i=0; i<MAX_BLOCKS;i++)
                FBM[i]=1;

            inode_t shadow[4];

            //init directory file
            directory_inode.size=0;
            inodes_file[0]=directory_inode;
            size_t dir_size = sizeof(dir);

            write_data(&directory_inode, dir_size, dir,0);

            //initialing root inode
            root.size= 1;           //initialy root has only the direcotry inode in it
            root.indirect=-1;
            write_data(&root,sizeof(inodes_file),inodes_file,0);

            printf("\nroot_size: %d\n", (int)root.size);


            //initialising superblock
            superblock.magic = rand();
            superblock.bsize = BLOCK_SIZE;
            superblock.file_system_size = BLOCK_SIZE*MAX_BLOCKS;
            superblock.numOf_inodes = MAX_FILES + 1;
            superblock.root = root;
            //superblock.shadow = shadow;
            superblock.castshadow = -1;
            write_blocks(0,1,&superblock);

            //initialising Free Bit Map
            FBM[0]=1;FBM[1022]=1;FBM[1023]=1;
            write_blocks(1022,1,FBM);


            //@To-do initialise Write Mask here
            WM[0]=0;
            write_blocks(1023,1,WM);

            printf("New file system '%s' created\n",fileSystem_);
        }
    }else if(fresh == 0){
        if ((init_disk(fileSystem_,BLOCK_SIZE, MAX_BLOCKS))==-1)
            printf("Could not create existing file system '%s'\n",fileSystem_);
        else{

            void* buffer_sb;
            buffer_sb = (void*) malloc(BLOCK_SIZE);

            read_blocks(0,1,buffer_sb);
            superblock = (superblock_t)(*((superblock_t*)buffer_sb));
            root = superblock.root;
            //inode_t *inodesBuff = (inode_t*) malloc(MAX_FILES*sizeof(inode_t));
            read_data(&root, MAX_FILES*sizeof(inode_t), inodes_file, 0);

            directory_inode = inodes_file[0];
            //fills in directory entries
            read_data(&directory_inode, sizeof(dir), dir, 0);

            //checks free block and fills in the array;
            short *fbm_buff = (short*)malloc(1024*sizeof(short));
            read_blocks(MAX_BLOCKS-2,1,fbm_buff);
            for (int i=1; i<MAX_BLOCKS-2; i++){

                *(fbm_buff+i)=FBM[i];
                if((*(fbm_buff+i))==0)
                    free_blocks_total--;
            }
            free(fbm_buff);
            FBM[0]=1;FBM[1022]=1;FBM[1023]=1;

            printf("Existing file system '%s' created\n",fileSystem_);

            for(int i=0; i<MAX_FD_ENTRIES;i++)
                free_fd[i]=1;
        }
    }
}

int ssfs_get_next_file_name(char *fname){
    return 0;
}

int ssfs_get_file_size(char* path){
    return 0;
}


int ssfs_fopen(char *name){

    //int isFileIn_dir=0;
    int inode_found;

    //checks if name is too long
    if(strlen(name)>MAX_FILENAME_LENGTH){
        printf("name '%s' is too long, MAX characters for file name: 10", name);
        return -1;
    }

    int max_index = directory_inode.size;

    //if there's not entry in the directory, create new file
    if(max_index==0){

            //create new inode
            inode_t new_inode;
            //initialize new_inode
            new_inode.size = 0;
            inodes_file[max_index+1] = new_inode;
            root.size ++;

            //create new dir_entry
            dir_entry_t new_dir_entry;
            //intitialize entry
            strcpy(new_dir_entry.file_name, name);
            new_dir_entry.inode_num = 1;
            dir[max_index] = new_dir_entry;

            directory_inode.size=directory_inode.size+1;

            //create new fd entry
            fd_t new_fd;
            //initialize fd_entry
            new_fd.inode_num = 1;
            new_fd.pread=0;
            new_fd.pwrite=0;
            fd_table[0]= new_fd;
            free_fd[0]=0;

            printf("file '%s' opened\nFileID: %d\n", name, 0);
            openFiles_num++;

            return 0;
    }else{
        //first take a look in the directory to find the corresponding inode number of the file 'name'
        for (int j=0; j<(directory_inode.size); j++){
            //checks if file is in the directory, if it is none we create a new one and assign a fd number

            if ((strcmp(name, dir[j].file_name) != 0 && j == directory_inode.size-1)){
                //create file

                //create new inode
                inode_t new_inode;
                //initialize new_inode
                new_inode.size = 0;
                inodes_file[max_index+1] = new_inode;
                root.size ++;

                //create new dir_entry
                dir_entry_t new_dir_entry;
                //intitialize entry
                strcpy(new_dir_entry.file_name, name);
                new_dir_entry.inode_num = max_index+1;
                dir[max_index] = new_dir_entry;

                directory_inode.size = directory_inode.size+1;

                //create new fd entry
                fd_t new_fd;
                //initialize fd_entry
                new_fd.inode_num = max_index+1;
                new_fd.pread=0;
                new_fd.pwrite=0;

                int l;
                for(l=0;l<MAX_FD_ENTRIES;l++){
                    if(free_fd[l]==1){
                        fd_table[l]= new_fd;
                        free_fd[l]=0;
                        break;
                    }
                    //printf("Print L: '%d'  //  Free: %d  ", l, free_fd[l]);
                }

                printf("file '%s' opened\nFileID: %d\n", name, l);
                openFiles_num++;

                return l;

            }else if (strcmp(name, dir[j].file_name) == 0){
                inode_found = dir[j].inode_num;
                //printf("directoryP: %d\n",j);

                //if there's no open file, create one
                if (openFiles_num == 0){
                     //create new fd entry
                    fd_t new_fd;
                    //initialize new fd entry
                    new_fd.inode_num = inode_found; new_fd.pread=0; new_fd.pwrite = inodes_file[inode_found].size;
                    fd_table[0]=new_fd;
                    free_fd[0]=0;
                    printf("file '%s' opened from DIRECTORY\nFileID: %d\n", name, 0);
                    openFiles_num++;
                    return 0;
                //if there's one or more than one open file, check for entry that matched inode_found
                }else if(openFiles_num>0){

                    for (int i=0; i<MAX_FD_ENTRIES; i++){
                        if(free_fd[i]==0){
                            if((fd_table[i].inode_num)==inode_found){
                                //if file is found in fd table the return index
                                printf("file '%s' opened from FD_table\nFileID: %d\n", name, i);
                                return i;

                            }
                            //printf("%d  ///  %d  ///  %d  ///  %d\n",fd_table[i].inode_num,inode_found, i, free_fd[i]);

                        }
                    }

                    //if no match is found in fd table and openFiles_num is less than MAX_FD_ENTRIES create new entry
                    //otherwise ask use to close a file first
                    if(openFiles_num<MAX_FD_ENTRIES){
                        //create new fd entry
                        fd_t new_fd;
                        //initialize new fd entry
                        new_fd.inode_num = inode_found; new_fd.pread=0; new_fd.pwrite = inodes_file[inode_found].size;

                        //insert new fd entry in the table (first free available entry)
                        int l;
                        for(l=0;l<MAX_FD_ENTRIES;l++){
                            if(free_fd[l]==1){
                                fd_table[l]= new_fd;
                                free_fd[l]=0;
                                break;
                            }
                        }

                        printf("file '%s' opened from DIRECTORY\nFileID: %d\n", name, l);
                        openFiles_num++;
                        return l;

                    }else{
                        printf("too many open files, close a file first then try again");
                        return -1;
                    }

                }
            }
        }
    }
    //update values on the disk
    write_data(&directory_inode, sizeof(dir), dir,0);
    write_data(&root,sizeof(inodes_file),inodes_file,0);
    superblock.root=root;
    write_blocks(0,1,&superblock);
}


int ssfs_fclose(int fileID){

    //printf("fileID: %d\n", fileID);
    if(fileID<0){
        printf("ERROR: fileID is negative, it can only be positive\n");
        return -1;
    }


    if(remove_fd_entry(fileID)==0)
        return 0;
    else{
        printf("ERROR: Could not close the file, errornious fileID: %d\n", fileID);
        return -1;
    }

}

int ssfs_frseek(int fileID, int loc){

    if(fileID<0 || fileID>(MAX_FD_ENTRIES-1)){
        printf("EROR: No such fileID found : %d, should be positivea and less that %d\n",fileID,(MAX_FD_ENTRIES-1));
        return -1;
    }else if(free_fd[fileID] == 1){
        printf("No such fileID found : %d\n",fileID);
        return -1;
    }

    fd_t* fd_entry = &(fd_table[fileID]);
    int inode_num=fd_entry->inode_num;
    inode_t* inode_ReadSeek = &(inodes_file[inode_num]);

    if ((inode_ReadSeek->size)-1<loc || loc<0){
        printf("ReadSeek loc '%d' in FileID '%d' is out of range, size file is: %d\n",loc, fileID,inode_ReadSeek->size);
        return -1;
    }else{
        fd_entry->pread = loc;
        return 0;
    }
}

int ssfs_fwseek(int fileID, int loc){

    if(fileID<0 || fileID>(MAX_FD_ENTRIES-1)){
        printf("EROR: No such fileID found : %d, should be positivea and less that %d\n",fileID,(MAX_FD_ENTRIES-1));
        return -1;
    }else if(free_fd[fileID] == 1){
        printf("No such fileID found : %d\n",fileID);
        return -1;
    }

    fd_t* fd_entry = &(fd_table[fileID]);
    int inode_num=fd_entry->inode_num;
    inode_t* inode_WriteSeek = &(inodes_file[inode_num]);

    if ((inode_WriteSeek->size)-1<loc || loc<0){
        printf("WriteSeek loc '%d' in FileID '%d' is out of range, size file is: %d\n",loc, fileID,inode_WriteSeek->size);
        return -1;
    }else{
        fd_entry->pwrite = loc;
        return 0;
    }
}

int ssfs_fwrite(int fileID, char *buf, int length){


    if(fileID<0 || fileID>(MAX_FD_ENTRIES-1)){
        printf("EROR: No such fileID found : %d, should be positive\n",fileID);
        return -1;
    }


    if(free_fd[fileID] == 1){
        printf("No such fileID found : %d\n",fileID);
        return -1;
    }


    fd_t* fd_entry = &(fd_table[fileID]);
    int inode_num=fd_entry->inode_num;
    //int truncates
    int blocks_toSkip = (int)((fd_entry->pwrite)/BLOCK_SIZE);
    int remaining_bytes= (fd_entry->pwrite) - BLOCK_SIZE*blocks_toSkip;
    inode_t* inode_toWrite = &(inodes_file[inode_num]);



    if(inode_toWrite->size != blocks_toSkip*BLOCK_SIZE){
        char* wr_buff = (char*)malloc(BLOCK_SIZE);
        read_data(inode_toWrite,BLOCK_SIZE,wr_buff,blocks_toSkip);


        wr_buff = realloc(wr_buff,(remaining_bytes+length));
        //*(wr_buff+remaining_bytes)='\0';
        //strcat(wr_buff,buf);
        memcpy(wr_buff+remaining_bytes,buf,length);
        // int numblocks_to_write = (int)ceil(strlen(wr_buff)/BLOCK_SIZE);

        //for(int i=blocks_toSkip+1; i<(blocks_toSkip+numblocks_to_write)){
            write_data(inode_toWrite, (remaining_bytes+length), wr_buff, blocks_toSkip);

        //}
    }else{
        write_data(inode_toWrite, length, buf, blocks_toSkip);
    }


    fd_entry->pwrite = fd_entry->pwrite+length;
    inode_toWrite->size = fd_entry->pwrite;

    inodes_file[inode_num] = *inode_toWrite;

    //update values on the disk
    write_data(&root,sizeof(inodes_file),inodes_file,0);
    superblock.root=root;
    write_blocks(0,1,&superblock);
    write_blocks(1022,1,FBM);

    return length;
}

int ssfs_fread(int fileID, char *buf, int length){

    if(fileID<0 || fileID>(MAX_FD_ENTRIES-1)){
        printf("EROR: No such fileID found : %d, should be positivea and less that %d\n",fileID,(MAX_FD_ENTRIES-1));
        return -1;
    }else if(free_fd[fileID] == 1){
        printf("No such fileID found : %d\n",fileID);
        return -1;
    }

    fd_t* fd_entry = &(fd_table[fileID]);
    int inode_num=fd_entry->inode_num;
    //int truncates
    int blocks_toSkip = (int)((fd_entry->pread)/BLOCK_SIZE);
    int lastBlock_bytesToSkip= (fd_entry->pread) - BLOCK_SIZE*blocks_toSkip;
    int remaining_bytes =BLOCK_SIZE-lastBlock_bytesToSkip;

    inode_t* inode_toRead = &(inodes_file[inode_num]);

    //checks if the file size is smaller than the asked buffer size
    if(((inode_toRead->size) - (fd_entry->pread))<length){
        printf("lenght too big");
        return -1;
    }


    if(remaining_bytes<1024){

        char* read_buff = (char*)malloc(BLOCK_SIZE);
        read_data(inode_toRead,BLOCK_SIZE,read_buff,blocks_toSkip);
        printf("\n\nreadBuff: %s\n",read_buff);

        if (length>remaining_bytes)
            memcpy(buf,read_buff+lastBlock_bytesToSkip,remaining_bytes);
        else{
            memcpy(buf,(char*)read_buff+lastBlock_bytesToSkip,length);
            printf("\n\nbuf: %s\n",buf);
            return length;
        }

        free(read_buff);
        if(read_data(inode_toRead,(length - remaining_bytes),buf+remaining_bytes,blocks_toSkip+1) == 0)
            return length;
        else
            return -1;
    }else if(remaining_bytes==1024){

        if(read_data(inode_toRead,length,buf,blocks_toSkip) == 0){
            return length;
        }else
            return -1;
    }



    //read_buff = (char*)realloc(BLOCK_SIZE);


}

int ssfs_remove(char *file){

    int inode_num = remove_dir_entry(file);
    if(inode_num<1)
        return -1;

    int isRemoved = remove_inode(inode_num);

    if(isRemoved==-1)
        return -1;
    else if (isRemoved==0){

        //update values on the disk
        write_data(&directory_inode, sizeof(dir), dir,0);
        write_data(&root,sizeof(inodes_file),inodes_file,0);
        superblock.root=root;
        write_blocks(0,1,&superblock);
        write_blocks(1022,1,FBM);
        return 0;
    }


}
