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
#define UPDATE_TAG 9

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

peer_info info;

typedef struct {
    int owner_rank;
    file_info file_info;
} client_struct;

client_struct **client_structs;



peer_info read_file(const char *filename) {
    FILE *file;
    peer_info info;

    char *path_to_file = calloc(100, sizeof(char));
    sprintf(path_to_file, "%s", filename);

    // Open the file for reading
    file = fopen(path_to_file, "r");

    if (file == NULL) {
        printf("Error opening the file.\n");
        exit(1);
    }

    // Read the number of owned files
    fscanf(file, "%d", &info.num_owned_files);

    // Read files one by one.
    for (int i = 0; i < info.num_owned_files; i++) {
        int num_segments;
        fscanf(file, "%s %d", info.owned_files[i].filename, &num_segments);
        info.owned_files[i].num_chunks = num_segments;
        info.owned_files[i].file_size = num_segments;

        for (int j = 0; j < num_segments; j++) {
            fscanf(file, "%s", info.owned_files[i].chunks[j]);
        }
    }

    // Read the number of desired files
    fscanf(file, "%d", &info.num_desired_files);

    for (int i = 0; i < info.num_desired_files; i++) {
        fscanf(file, "%s", info.desired_files[i].filename);
        info.desired_files[i].num_chunks = 0;
        info.desired_files[i].file_size = -1;
    }

    // Close the file
    fclose(file);

    return info;
}

void printFile(file_info file) {
    printf("Filename: %s\n", file.filename);
    printf("Num chunks: %d\n", file.num_chunks);
    for (int i = 0; i < file.num_chunks; i++) {
        printf("Chunk %d: %s\n", i, file.chunks[i]);
    }
}

void printSwarm(client_struct **swarm_pool) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(swarm_pool[i][0].file_info.filename, "") == 0) {
            break;
        }
        printf("Filename: %s\n", swarm_pool[i][0].file_info.filename);
        printf("File size: %d\n", swarm_pool[i][0].file_info.file_size);
        printf("Num chunks: %d\n", swarm_pool[i][0].file_info.num_chunks);
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (swarm_pool[i][j].owner_rank == 0) {
                break;
            }
            printf("Owner rank: %d\n", swarm_pool[i][j].owner_rank);
            printf("Chunks:\n");
            for (int k = 0; k < swarm_pool[i][j].file_info.num_chunks; k++) {
                printf("%s\n", swarm_pool[i][j].file_info.chunks[k]);
            }
        }
    }
}

void create_MPI_file_type(MPI_Datatype *file_info_type) {
    
    int lengths[4] = { MAX_FILENAME, 1, 1, MAX_CHUNKS * HASH_SIZE };

    MPI_Aint displacements[4];
    file_info dummy_msg;
    MPI_Aint base_address;
    MPI_Get_address(&dummy_msg, &base_address);
    MPI_Get_address(&dummy_msg.filename, &displacements[0]);
    MPI_Get_address(&dummy_msg.num_chunks, &displacements[1]);
    MPI_Get_address(&dummy_msg.file_size, &displacements[2]);
    MPI_Get_address(&dummy_msg.chunks, &displacements[3]);

    displacements[0] = MPI_Aint_diff(displacements[0], base_address);
    displacements[1] = MPI_Aint_diff(displacements[1], base_address);
    displacements[2] = MPI_Aint_diff(displacements[2], base_address);
    displacements[3] = MPI_Aint_diff(displacements[3], base_address);

    MPI_Datatype types[4] = { MPI_CHAR, MPI_INT, MPI_INT, MPI_CHAR };
    MPI_Type_create_struct(4, lengths, displacements, types, file_info_type);
    MPI_Type_commit(file_info_type);
}

file_info copyFileInfo(file_info source) {

    file_info destination;
    // Copy each field from source to destination
    strcpy(destination.filename, source.filename);
    destination.num_chunks = source.num_chunks;
    destination.file_size = source.file_size;

    // Copy each chunk
    for (int i = 0; i < source.num_chunks; ++i) {
        strcpy(destination.chunks[i], source.chunks[i]);
    }
    return destination;
}

void printClientSwarm(client_struct *client_structs) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_structs[i].owner_rank == 0) {
            printf("Am ajuns la final\n");
            break;
        }
        printf("Owner rank: %d\n", client_structs[i].owner_rank);
        printf("Filename: %s\n", client_structs[i].file_info.filename);
        printf("Num chunks: %d\n", client_structs[i].file_info.num_chunks);
        for (int j = 0; j < client_structs[i].file_info.num_chunks; j++) {
            printf("Chunk %d: %s\n", j, client_structs[i].file_info.chunks[j]);
        }
    }
}

void saveFile(int rank, file_info info) {
    // Create a file named filename and save the chunks in it
    FILE *file;

    // Open the file for writing
    char filename[MAX_FILENAME + 10];
    memset(filename, '\0', sizeof(filename));
    sprintf(filename, "client%d_%s", rank, info.filename);
    file = fopen(filename, "ab");

    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Write the chunks to the file
    for (int i = 0; i < info.num_chunks; ++i) {
        fprintf(file, "%s", info.chunks[i]);
        if (i != info.num_chunks - 1) {
            fprintf(file, "\n");
        }
    }

    // Close the file
    fclose(file);

    printf("File saved successfully.\n");
}

void *download_thread_func(void *arg)
{
    int rank = *(int*) arg;
    int ack, desired_files_count = 0;
    int steps = 10;

    // Whenever a file is downloaded, mark it as downloaded.
    int downloaded[MAX_FILES];
    for (int i = 0; i < MAX_FILES; i++) {
        downloaded[i] = 0;
    }

    // Read the file
    char filename[MAX_FILENAME];
    sprintf(filename, "in%d.txt", rank);
    memset(&info, '\0', sizeof(info));
    info = read_file(filename);
    desired_files_count = info.num_desired_files;

    // Create personal data type
    MPI_Datatype file_info_type;
    create_MPI_file_type(&file_info_type);

    // Send initialisation message to tracker with the number of owned files.
    int num_owned_files = info.num_owned_files;
    MPI_Send(&num_owned_files, 1, MPI_INT, TRACKER_RANK, INITIALISATION_TAG, MPI_COMM_WORLD);

    // Send the owned files one by one.
    for (int i = 0; i < info.num_owned_files; i++) {      
        MPI_Send(&info.owned_files[i], 1, file_info_type, TRACKER_RANK, INFO_TAG, MPI_COMM_WORLD);
    }

    // Wait for the tracker to send the ack
    MPI_Recv(&ack, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Create the swarm pool
    client_structs = (client_struct**)calloc(info.num_desired_files, sizeof(client_struct*));
    for (int i = 0; i < info.num_desired_files; i++) {
        client_structs[i] = (client_struct*)calloc(MAX_CLIENTS, sizeof(client_struct));
    }

    // Start the routine. Request files from the tracker and then ask other peers for the missing chunks.
    // Every 10 steps, update the tracker with my info.
    while(desired_files_count > 0) {
        if (steps >= 10) {
            // Reset steps
            steps = 0;

            // Tell the tracker how many files tp expect.
            MPI_Send(&desired_files_count, 1, MPI_INT, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD);

            // Now send the name of the desired files.
            for (int i = 0; i < info.num_desired_files; i++) {
                // Check if the file is already downloaded
                if (downloaded[i] == 1) {
                    continue;
                }

                char filename[MAX_FILENAME];
                memset(filename, '\0', sizeof(filename));
                client_struct recv_file;
                int file_size;
                strncpy(filename, info.desired_files[i].filename, MAX_FILENAME-1);

                // Send the filename and wait for the file's swarm
                MPI_Send(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD);
               
                // The Tracker tells us how many files to expect.
                int number_of_files;
                MPI_Recv(&number_of_files, 1, MPI_INT, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                // For every file, update our swarm pool.
                for (int k = 0; k < number_of_files; k++) {

                    // Receive file.
                    MPI_Recv(&file_size, 1, MPI_INT, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&recv_file.owner_rank, 1, MPI_INT, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Recv(&recv_file.file_info, 1, file_info_type, TRACKER_RANK, REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    info.desired_files[i].file_size = file_size;
            
                    // Add file_info to swarm pool.
                    int found = 0;
                    for (int j = 0; j < info.num_desired_files; j++) {

                        // Check if a swarm for this file already exists.
                        if (strcmp(client_structs[j][0].file_info.filename, "") == 0) {
                            client_structs[j][0].file_info = copyFileInfo(recv_file.file_info);
                            client_structs[j][0].owner_rank = recv_file.owner_rank;
                            break;
                        } else if (strcmp(client_structs[j][0].file_info.filename, filename) == 0) {

                            // Found the swarm for this file.
                            for (int k = 0; k < MAX_CLIENTS; k++) {

                                // We reached the end of the swarm. Add a new entry at the end of it.
                                if (client_structs[j][k].owner_rank == 0) {
                                    client_structs[j][k].file_info = recv_file.file_info;
                                    client_structs[j][k].owner_rank = recv_file.owner_rank;
                                    found = 1;
                                    break;
                                }

                                // This client is already in the swarm. Update his info.
                                if (recv_file.owner_rank == client_structs[j][k].owner_rank) {
                                    client_structs[j][k].file_info = recv_file.file_info;
                                    client_structs[j][k].owner_rank = recv_file.owner_rank;
                                    found = 1;
                                    break;
                                }
                            }
                            if (found == 1)
                                break;
                        }
                    }
                }
            
            }

            // Update tracker with my info.
            // Tell the tracker how many files to expect.
            int num_owned_files = info.num_owned_files;
            MPI_Send(&num_owned_files, 1, MPI_INT, TRACKER_RANK, UPDATE_TAG, MPI_COMM_WORLD);

            // Now send the owned files one by one.
            for (int i = 0; i < info.num_owned_files; i++) {      
                MPI_Send(&info.owned_files[i], 1, file_info_type, TRACKER_RANK, UPDATE_TAG, MPI_COMM_WORLD);
            }
        }


        // Now ask other peers for the missing chunks.
        for (int i = 0; i < info.num_desired_files; i++) {

            // If the file is already downloaded, skip it.
            if (downloaded[i] == 1) {
                continue;
            }

            int desired_file_index = i;
            int owned_file_index = -1;
            char filename[MAX_FILENAME];
            memset(filename, '\0', sizeof(filename));
            strncpy(filename, info.desired_files[desired_file_index].filename, MAX_FILENAME-1);

            // Find out what is the next chunk that I need.
            int chunk_index = -1;
            for (int j = 0; j < info.num_owned_files; j++) {
                if (strcmp(info.owned_files[j].filename, filename) == 0) {
                    chunk_index = info.owned_files[j].num_chunks;
                    owned_file_index = j;
                }
            }

            // I don't have any chunks from this file. We start from the beginning.
            if (chunk_index == -1) {
                chunk_index = 0;
                info.owned_files[info.num_owned_files] = copyFileInfo(info.desired_files[desired_file_index]);
                info.num_owned_files++;
                owned_file_index = info.num_owned_files - 1;
            }

            // Run a random algorithm to choose a peer from the swarm.
            // Find the peer. Also check if he's got the chunk I'm interesed in.
            client_struct peer;
            for (int j = 0; j < MAX_FILES; j++) {
                if (strcmp(client_structs[j][0].file_info.filename, filename) == 0) {

                    // Found the file in the swarm pool. Let's see who has the chunk.
                    int has_the_chunk[MAX_CLIENTS] = { 0 };
                    int arr_len = 0;
                    
                    // Check if the peer has the chunk.
                    for (int k = 0; k < MAX_CLIENTS; k++) {
                        
                        // Check if we reached the end of the swarm.
                        if (strcmp(client_structs[j][k].file_info.filename, "") == 0) {
                            break;
                        }
                        if (client_structs[j][k].file_info.num_chunks > chunk_index) {
                            has_the_chunk[arr_len] = k;
                            arr_len++;
                        }
                    }
                    
                    // Now choose a random peer from the ones that have the chunk.
                    int random_index = rand() % arr_len;
                    peer = client_structs[j][has_the_chunk[random_index]];
                    break;
                }
            }
            
            // A random peer has been chosen. Now send him a request for the chunk.
            MPI_Send(&rank, 1, MPI_INT, peer.owner_rank, UPLOAD_THREAD_TAG, MPI_COMM_WORLD);
            MPI_Send(&filename, MAX_FILENAME, MPI_CHAR, peer.owner_rank, REQUEST_PEER_TAG, MPI_COMM_WORLD);
            MPI_Send(peer.file_info.chunks[chunk_index], HASH_SIZE, MPI_CHAR, peer.owner_rank, REQUEST_PEER_TAG, MPI_COMM_WORLD);

            // Wait for the peer to send us the chunk.
            MPI_Recv(&ack, 1, MPI_INT, peer.owner_rank, REPLY_PEER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // We received the "chunk". Now save it.
            if (ack == 1) {
                info.owned_files[owned_file_index].num_chunks++;
                strcpy(info.owned_files[owned_file_index].chunks[chunk_index], peer.file_info.chunks[chunk_index]);

                // Check if we downloaded the entire file.
                if (info.owned_files[owned_file_index].num_chunks == info.owned_files[owned_file_index].file_size) {
                    downloaded[i] = 1;
                    desired_files_count--;

                    // Send partial finalization tag to tracker.
                    MPI_Send(&ack, 1, MPI_INT, TRACKER_RANK, PARTIAL_FINALIZATION_TAG, MPI_COMM_WORLD);
                    saveFile(rank, info.owned_files[owned_file_index]);
                }
            } else {
                printf("Error. We did not received ack.\n");
            }
            steps++;
            break;
        }

    }

    // Send total finalization tag to tracker.
    MPI_Send(&ack, 1, MPI_INT, TRACKER_RANK, TOTAL_FINALIZATION_TAG, MPI_COMM_WORLD);
    return NULL;
}

void *upload_thread_func(void *arg)
{
    int rank = *(int*) arg;

    // Loop ends when the tracker sends the total finalization signal.
    while (1) {
        int found = 0;
        int data;
        char filename[MAX_FILENAME], hash[HASH_SIZE];
        memset(filename, '\0', sizeof(filename));
        memset(hash, '\0', sizeof(hash));
        MPI_Status status;

        // Receive data meant for the upload thread sent by anyone.
        MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, UPLOAD_THREAD_TAG, MPI_COMM_WORLD, &status);

        // Check if the tracker sent the total finalization signal.
        if (status.MPI_SOURCE == TRACKER_RANK) {
            printf("CLIENT %d: SHUTTING DOWN UPLOAD THREAD\n", rank);
            break;
        }

        // Receive the filename and hash from the peer.
        MPI_Recv(&filename, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, REQUEST_PEER_TAG, MPI_COMM_WORLD, &status);
        MPI_Recv(&hash, HASH_SIZE, MPI_CHAR, status.MPI_SOURCE, REQUEST_PEER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Search for the file in the peer's owned files.
        for (int i = 0; i < MAX_FILES; i++) {

            file_info file = info.owned_files[i];
            if (strcmp(file.filename, "") == 0)  {
                continue;
            }

            // Found the file. Now search for the hash.
            if (strcmp(file.filename, filename) == 0) {

                for (int j = 0; j < file.num_chunks; j++) {

                    // Found the hash. Send ack to the peer.
                    if (strncmp(hash, file.chunks[j], HASH_SIZE) == 0) {
                        int ack = 1;
                        MPI_Send(&ack, 1, MPI_INT, status.MPI_SOURCE, REPLY_PEER_TAG, MPI_COMM_WORLD);
                        found = 1;
                        break;
                    }
                }
                if (found)
                    break;
            }
        }

    }

    return NULL;
}

void addToSwarm(client_struct **swarm_pool, file_info file, int rank) {

    // If a swarm for this file already exists, try to update it with information from this peer.
    // If not, find the first empty spot in the swarm pool and create a swarm for this file at that position.
    for (int i = 0; i < MAX_FILES; i++) {

        // Found the swarm for this file.
        if (strcmp(swarm_pool[i][0].file_info.filename, file.filename) == 0) {
            for (int j = 0; j < MAX_CLIENTS; j++) {

                // Found an empty spot in the swarm.
                if (swarm_pool[i][j].owner_rank == 0) {      
                    swarm_pool[i][j].owner_rank = rank;
                    swarm_pool[i][j].file_info = file;
                    break;
                }
                // Found the peer in the swarm. Update his info.
                if (swarm_pool[i][j].owner_rank == rank) {
                    swarm_pool[i][j].file_info = file;
                    break;
                }
            }
            break;
        }

        // File does not exist in the swarm pool. Create a new swarm for it.
        if (strcmp(swarm_pool[i][0].file_info.filename, "") == 0) {
            swarm_pool[i][0].owner_rank = rank;
            swarm_pool[i][0].file_info = file;
            break;
        }
    }
   
}

void tracker(int numtasks, int rank) {

    int received_data;
    client_struct **swarm_pool = (client_struct**)calloc(MAX_FILES, sizeof(client_struct*));
    for (int i = 0; i < MAX_FILES; i++) {
        swarm_pool[i] = (client_struct*)calloc(MAX_CLIENTS, sizeof(client_struct));
    }


    // Create personal data type. We will use it to send structs.
    MPI_Datatype file_info_type;
    create_MPI_file_type(&file_info_type);

    // Receive initialisation messages from peers.
    for (int i = 1; i < numtasks; i++) {
        int num_owned_files;
        MPI_Recv(&num_owned_files, 1, MPI_INT, i, INITIALISATION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // For every received file, add it to the swarm pool.
        for (int j = 0; j < num_owned_files; j++) {
            file_info info;
            MPI_Recv(&info, 1, file_info_type, i, INFO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            addToSwarm(swarm_pool, info, i);
        }
    }
    int ack = 1;

    // Send ack to all peers. Now they can start requesting files between each other.
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ack, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    int remaining_clients = numtasks - 1;

    // Tracker routine ends when all peers send the total finalization tag.
    while (remaining_clients > 0) {

        // Awaiting requests from peers.
        MPI_Status status;
        MPI_Recv(&received_data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int source = status.MPI_SOURCE;
        int tag = status.MPI_TAG;

        switch (tag) {
            case REQUEST_TAG :

                // For every received file, send the file swarm back to the peer.
                for (int awaiting_files = 0; awaiting_files < received_data; awaiting_files++) {

                    char filename[MAX_FILENAME];
                    MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, source, REQUEST_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    // Search for the file in the swarm pool.
                    for (int i = 0; i < MAX_FILES; i++) {

                        // Found the file in the swarm pool.
                        if (strcmp(swarm_pool[i][0].file_info.filename, filename) == 0) {
                            
                            // Count the number of owners in this swarm. Send it to the peer so that he knows how many files to expect.
                            int number_of_owners = 0;
                            while (strcmp(swarm_pool[i][number_of_owners].file_info.filename, "") != 0) {
                                number_of_owners++;
                            }
                            MPI_Send(&number_of_owners, 1, MPI_INT, source, REQUEST_TAG, MPI_COMM_WORLD);

                            // Now send each owner of this file one by one.
                            int j = 0;
                            while (j < number_of_owners) {

                                // Send the file and its owner to the peer.
                                int file_size = swarm_pool[i][j].file_info.file_size;
                                MPI_Send(&file_size, 1, MPI_INT, source, REQUEST_TAG, MPI_COMM_WORLD);
                                MPI_Send(&swarm_pool[i][j].owner_rank, 1, MPI_INT, source, REQUEST_TAG, MPI_COMM_WORLD);
                                MPI_Send(&swarm_pool[i][j].file_info, 1, file_info_type, source, REQUEST_TAG, MPI_COMM_WORLD);
                                j++;
                            } 
                            break;
                        }
                    }
                }
                break;

            // A peer has sent its updated files to the tracker. Every peer does this once every 10 steps.
            case UPDATE_TAG :

                // Update the swarm pool with the new info.
                for (int j = 0; j < received_data; j++) {
                    file_info info;
                    MPI_Recv(&info, 1, file_info_type, source, UPDATE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    addToSwarm(swarm_pool, info, source);
                }  
                break;   

            // A peer has entirely downloaded a file.   
            case PARTIAL_FINALIZATION_TAG :
                printf("File download completed by %d\n", source);
                break;

            // A peer has entirely downloaded all of its files.
            case TOTAL_FINALIZATION_TAG :
                printf("Download completed entirely by %d\n", source);
                remaining_clients--;
                break;

        }
    }

    // Send shutdown signdal to all upload threads of all peers.
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(&ack, 1, MPI_INT, i, UPLOAD_THREAD_TAG, MPI_COMM_WORLD);
    }


}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &rank);
    if (r) {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}

