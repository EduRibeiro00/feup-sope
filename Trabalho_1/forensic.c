#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h> 
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <signal.h> 

#include "others.c"
#include "forensicAux.c"

void analiseFile(char* file) {

    blocksigint(); // blocks SIGINT while analyzing a file

    struct stat stat_entry;
    char outputString[1000]; // string that will have all the information in the end, and that is going to be printed
    outputString[0] = '\0';

    if(lstat(file, &stat_entry) == -1){
        perror("Stat error");
        exit(3);
    }

    if(S_ISREG(stat_entry.st_mode)){ // if regular file

        if (boolArray[OUT]) {

            if (boolArray[LOG])
                addSentSignalToLog("SIGUSR2");

            kill(parentPid, SIGUSR2);
        }

        strcat(outputString, file); // appends file name

        char buffer[MAX_SIZE] = "";
        commandToString(buffer, MAX_SIZE, "file", file); // calls the "file" command, and extracts its result
        strcat(outputString, ",");
        strcat(outputString, firstCharAfterSpace(buffer)); // appends file type

        sprintf(buffer, "%d", (int) stat_entry.st_size);
        strcat(outputString, ",");
        strcat(outputString, buffer); // appends file size

        getFileAcess(buffer, MAX_SIZE, stat_entry.st_mode);
        strcat(outputString, ",");
        strcat(outputString, buffer); // appends file access

        struct tm *ts;

        ts = localtime(&stat_entry.st_atime);
	    strftime(buffer, MAX_SIZE, "%Y-%m-%dT%H:%M:%S", ts);
	    strcat(outputString, ",");
        strcat(outputString, buffer); // appends file access date

	    ts = localtime(&stat_entry.st_mtime);
	    strftime(buffer, MAX_SIZE, "%Y-%m-%dT%H:%M:%S", ts);
	    strcat(outputString, ",");
        strcat(outputString, buffer); // appends file modification date


        if(boolArray[MD5]){ // if md5 option selected
            commandToString(buffer, MAX_SIZE, "md5sum", file); // calls the "md5" command, and extracts its result
            strcat(outputString, ",");
            strcat(outputString, strtok(buffer, " ")); // apends md5 hash code
        }

        if(boolArray[SHA1]){ // if sha1 option selected
            commandToString(buffer, MAX_SIZE, "sha1sum", file); // calls the "sha1" command, and extracts its result
            strcat(outputString, ",");
            strcat(outputString, strtok(buffer, " ")); // apends sha1 hash code
        }

        if(boolArray[SHA256]){ // if sha256 option selected
            commandToString(buffer, MAX_SIZE, "sha256sum", file); // calls the "sha256" command, and extracts its result
            strcat(outputString, ",");
            strcat(outputString, strtok(buffer, " ")); // apends sha256 hash code
        }

        strcat(outputString, "\n");
        write(fdOut, outputString, strlen(outputString)); // writes the output

        if(boolArray[LOG])
            addFileToLog(file); // add file name to the log file
    }



    unblocksigint(); // operation is over; unblocks sigint

}

// -------------------------

void analiseDir(char* subDir, char* baseDir){

    if (boolArray[OUT]) {
        if (boolArray[LOG])
            addSentSignalToLog("SIGUSR1");

        kill(parentPid, SIGUSR1);
    }

    struct dirent* dentry;

    char directory[MAX_SIZE];
    strcpy(directory, baseDir);

    if(strlen(subDir) != 0) {
        strcat(directory, "/");
        strcat(directory, subDir);
    }

    if((dir = opendir(directory)) == NULL){ // open the directory
        perror(directory);
        exit(4);
    }

    if(boolArray[LOG])
        addDirToLog(directory); // add directory name to the log file

    if(boolArray[REC]) {

        // search for subdirectories
        while((dentry = readdir(dir)) != NULL){

            // only want to search for subdirectories
            if((strcmp(dentry->d_name, ".") == 0) || (strcmp(dentry->d_name, "..") == 0))
                continue;

            char newDir[MAX_SIZE];
            sprintf(newDir, "%s/%s", directory, dentry->d_name);

            if(checkFileType(newDir) == 2){ // if it is a directory

                pid_t pid;

                if((pid = fork()) == -1) {
                    perror("fork");
                    exit(4);
                }                
                else if(pid == 0) { // child process

                    if(strlen(subDir) != 0)
                        strcat(subDir, "/");

                    strcat(subDir, dentry->d_name);

                    analiseDir(subDir, baseDir); // calls recursively the program, but on the subdirectory
                    exit(0);
                }
            }
        }

        rewinddir(dir); // resets the directory, so we can iterate through it again
        

        if(chdir(baseDir) != 0){ // change to the main directory
            perror("chdir");
            exit(4);
        }

        char name[MAX_SIZE];

        // now, searching only for regular files
        while((dentry = readdir(dir)) != NULL) {
            
            if(strlen(subDir) != 0)
                sprintf(name, "%s/%s", subDir, dentry->d_name);
            else sprintf(name, "%s", dentry->d_name);


            analiseFile(name); // analise it (only if it is regular)
        }

        // wait for children, and "release" them
        int status;
        while(wait(&status) != -1) {
            if(WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Child did not end properly!");
                exit(4);
            }
        }

    }
    else {

        if(chdir(directory) != 0){ // change to that directory
            perror("chdir");
            exit(4);
        }

        while((dentry = readdir(dir)) != NULL){ // for every file contained in the directory...                
            analiseFile(dentry->d_name); // analise it (only if it is regular)  
        }
    }

    if(closedir(dir) != 0) {
        perror("closedir");
        exit(4);
    }
    
}

// -------------------------

void extractHOptions(char* string) {

    if(strcmp(string, "md5") == 0){
        boolArray[MD5] = true;
        return;
    }
    else if(strcmp(string, "sha1") == 0){
        boolArray[SHA1] = true;
        return;
    }
    else if(strcmp(string, "sha256") == 0){
        boolArray[SHA256] = true;
        return;
    }

    char comma[2] = ",";
    char* token;

    token = strtok(string, comma);

    while(token != NULL){

        // tries to extract the different options that are available for -h

        if(strcmp(token, "md5") == 0)
            boolArray[MD5] = true;
        else if(strcmp(token, "sha1") == 0)
            boolArray[SHA1] = true;
        else if(strcmp(token, "sha256") == 0)
            boolArray[SHA256] = true;
        else
            incUsage();

        token = strtok(NULL, comma);
    }

    if(!(boolArray[MD5] || boolArray[SHA1] || boolArray[SHA256]))
        incUsage();
}

// -------------------------

void extractOptions(int argc, char* argv[], int* fileOutIndex) {

    blocksigint(); // operation underway: blocks SIGINT while performing this function


    if(argc == 1) 
        incUsage(); // if no arguments are specified

    if((!strcmp(argv[argc - 1], "-r")) || (!strcmp(argv[argc - 1], "-v")) || (!strcmp(argv[argc - 1], "-o")) || (!strcmp(argv[argc - 1], "-h")))
        incUsage(); // if last argument is a flag
        

    for(int i = 0; i < NUM_OPTIONS; i++)
        boolArray[i] = false;


    bool filePresent = false; // in order to point out that the file/directory was specified in the command call
    char* fileOut, *fileLog; // names of the files to be used for output and log (if any)

    for(int k = 1; k < argc; k++){
                
        if(strcmp(argv[k], "-r") == 0){ // recursive option
            
            if(boolArray[REC])
                incUsage(); // if it was already activated
            
            boolArray[REC] = true;
    
        }
        else if(strcmp(argv[k], "-h") == 0){ // md5, sha1 and/or sha256 option

            if(boolArray[MD5] || boolArray[SHA1] || boolArray[SHA256])
                incUsage(); // if it was already activated

            char string[MAX_SIZE];
            strcpy(string, argv[k + 1]);

            extractHOptions(string);
            k++; // skips the next argument, which is part of the -h flag specification

        }
        else if(strcmp(argv[k], "-o") == 0){ // output option

            if(boolArray[OUT])
                incUsage(); // if it was already activated

            boolArray[OUT] = true;

            if((!strcmp(argv[k + 1], "-r")) || (!strcmp(argv[k + 1], "-v")) || (!strcmp(argv[k + 1], "-o")) || (!strcmp(argv[k + 1], "-h")))
                incUsage(); // if next argument is a flag
        
            fileOut = argv[k + 1]; // saves the file name
            *fileOutIndex = k + 1; // saves the index of the file
            k++; // skips the next argument, because it is the name of the output file
        
        }
        else if(strcmp(argv[k], "-v") == 0){ // log file option
            
            if(boolArray[LOG])
                incUsage(); // if it was already activated

            boolArray[LOG] = true;

            if(getenv("LOGFILENAME") == NULL){
                fprintf(stderr, "LOGFILENAME is not defined!\n\n");
                exit(2);
            }

            fileLog = getenv("LOGFILENAME"); // saves the file name

        }
        else if(k != argc - 1) // if it is not a flag, and if it is not the last argument, invalid option
            incUsage();
        
        else filePresent = true; // last argument; its the name of the file 
        
    }

    if(!filePresent)
        incUsage();

    if(boolArray[OUT]){
        if((fdOut = open(fileOut, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1){
            perror(fileOut);
            exit(2);
        }
    }
    else fdOut = STDOUT_FILENO;

    if(boolArray[LOG])
        if((fdLog = open(fileLog, O_WRONLY | O_CREAT | O_APPEND, 0644)) == -1){
            perror(fileLog);
            exit(2);
        }


    unblocksigint(); // unblocks sigint
}

// -------------------------

int main(int argc, char* argv[]) {

    if(gettimeofday(&startTime, NULL) != 0) { // extracts the initial time
        fprintf(stderr, "Time extraction failed!");
        exit(1);
    }

    equipHandlers(); // equips all the signal handlers that are necessary

    parentPid = getpid();

    printf("\n");
    
    char subDir[MAX_SIZE];
    subDir[0] = '\0';

    int fileOutIndex = -1;

    extractOptions(argc, argv, &fileOutIndex);

    if(boolArray[LOG])
        addCommandToLog(argv, argc); // adds the command to the log file

    char* inputName = argv[argc - 1]; // name of the file or directory

    // checks the type of the file argument passed
    switch(checkFileType(inputName)){

        case 1: // regular file
            analiseFile(inputName);            
            break;

        case 2: // directory
            analiseDir(subDir, inputName);      
            break;

        default: // other (no output)
            fprintf(stderr, "Invalid file passed!\n");
            break;
    }

    if(boolArray[OUT]) {
        printf("End: ");
        printf("%d/%d directories/files processed.\n", numDirs, numFiles);
        printf("Data saved on file %s.", argv[fileOutIndex]);
    }

    if(boolArray[LOG])
        printf("\nExecution records saved on file %s.", getenv("LOGFILENAME"));

    if(boolArray[OUT])
        if(close(fdOut) == -1){
            perror("close");
            exit(5);
        }

    if(boolArray[LOG])
        if(close(fdLog) == -1){
            perror("close");
            exit(5);
        }

    printf("\n");

    return 0;
}
