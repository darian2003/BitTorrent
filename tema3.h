#ifndef TEMA3_H
#define TEMA3_H

#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 33
#define MAX_CHUNKS 100
#define MAX_CLIENTS 10
#define INITIALISATION_TAG 0
#define INFO_TAG 1
#define REQUEST_TAG 2
#define PARTIAL_FINALIZATION_TAG 3
#define ACTUALIZATION_TAG 4
#define TOTAL_FINALIZATION_TAG 5
#define REQUEST_PEER_TAG 6
#define REPLY_PEER_TAG 7
#define UPLOAD_THREAD_TAG 8

int downloaded[MAX_FILES];

typedef struct {
    char filename[MAX_FILENAME];
    int num_chunks;
    int file_size;
    char chunks[MAX_CHUNKS][HASH_SIZE];
} file_info;

typedef struct{
    int num_owned_files;
    file_info owned_files[MAX_FILES];
    int num_desired_files;
    file_info desired_files[MAX_FILES];
} peer_info;

extern peer_info info;

typedef struct {
    int owner_rank;
    file_info file_info;
} client_struct;

extern client_struct **client_structs;

typedef struct {
    int num_owned_files;
    file_info owned_files[MAX_FILES];
} initialisation_msg;

typedef struct {
    int num_chunks;
    char chunks[MAX_CHUNKS][HASH_SIZE];
} owner_info;

typedef struct {
    char filename[MAX_FILENAME];
    int file_size;
    owner_info owners[MAX_CLIENTS];
} swarm;

peer_info read_file(const char *filename);
void printFile(file_info file);
void printSwarm(client_struct **swarm_pool);
void create_MPI_owner_type(MPI_Datatype *owner_info_type);
void create_MPI_file_type(MPI_Datatype *file_info_type);
void create_MPI_client_struct_type(MPI_Datatype *client_struct_type, MPI_Datatype *file_info_type);
void printDesiredFiles(peer_info info);
void copyFileInfo(file_info *destination, file_info *source);
void printClientSwarm(client_struct *client_structs);
void saveFile(int rank, file_info info);
void requestDesiredFiles(int rank, MPI_Datatype file_info_type);
void *download_thread_func(void *arg);
void *upload_thread_func(void *arg);
void addToSwarm(client_struct **swarm_pool, file_info *file, int rank);
void tracker(int numtasks, int rank);
void peer(int numtasks, int rank);
void solveRequest(int rank);
void uploadFilesToTracker(MPI_Datatype file_info_type, int tag);
void requestDesiredFilesFromTracker(int rank, MPI_Datatype *file_info_type);

#endif /* TEMA3_H */
