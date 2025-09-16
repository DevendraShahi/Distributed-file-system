#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>

// Defining port numbers for each server

// Main/C server port
#define S1_PORT 4301

// PDF server port
#define S2_PORT 4302

// Text server port
#define S3_PORT 4303

// ZIP server port
#define S4_PORT 4304

// Defining buffer and path sizes
#define MAX_PATH 1024
#define BUFFER_SIZE 4096

/* DIRECTORY MANAGEMENT FUNCTIONS */

// Creating server directories if they don't exist
int initialize_server_directories()
{
    printf("\n[S1] === INITIALIZING SERVER DIRECTORIES ===\n");

    // Listing all directories to create
    const char *directories[] = {"S1", "S1/temp", "S2", "S3", "S4"};
    // Calculating number of directories
    int dir_count = sizeof(directories) / sizeof(directories[0]);

    // Looping through each directory
    for (int i = 0; i < dir_count; i++)
    {
        // Creating stat structure to check directory
        struct stat st = {0};
        // Checking if directory already exists
        if (stat(directories[i], &st) == 0 && S_ISDIR(st.st_mode))
        {
            printf("[S1] Directory already exists: %s\n", directories[i]);
            // Skipping if directory exists
            continue;
        }
        else
        {
            // Creating directory with permissions 755
            if (mkdir(directories[i], 0755) == 0)
            {
                printf("[S1] Success: Created directory: %s\n", directories[i]);
            }
            else
            {
                printf("[S1] Failed to create directory: %s\n", directories[i]);
            }
        }
    }

    printf("[S1] === DIRECTORY INITIALIZATION COMPLETE ===\n\n");
    return 0;
}

// Creating full directory path recursively
int create_full_directories(char *full_path)
{
    // Storing temporary path for building
    char temp_path[MAX_PATH];
    // Storing pointer for path splitting
    char *token;
    // Storing copy of original path
    char path_copy[MAX_PATH];

    printf("[S1] Creating directory structure: %s\n", full_path);

    // Copying the full path to avoid modifying original
    strcpy(path_copy, full_path);
    // Initializing temporary path as empty
    strcpy(temp_path, "");

    // Splitting path by forward slash
    token = strtok(path_copy, "/");

    // Processing each path component
    while (token != NULL)
    {
        // Adding slash if temp_path is not empty
        if (strlen(temp_path) > 0)
        {
            // Concatenating slash to path
            strcat(temp_path, "/");
        }
        // Appending current token to temp path
        strcat(temp_path, token);

        // Attempting to create directory
        if (mkdir(temp_path, 0755) == -1)
        {
            // Checking if error is directory already exists
            if (errno != EEXIST)
            {
                printf("[S1] Failed to create directory: %s\n", temp_path);
                return -1;
            }
            else
            {
                printf("[S1] Directory exists: %s\n", temp_path);
            }
        }
        else
        {
            printf("[S1] Created directory: %s\n", temp_path);
        }

        // Getting next path component
        token = strtok(NULL, "/");
    }

    printf("[S1] Directory structure created successfully\n");
    return 0;
}

// Converting client path format to server path format
void convert_path_for_server(const char *s1_path, const char *server_prefix, char *result_path, int max_size)
{
    // Initializing result path
    result_path[0] = '\0';

    printf("[S1] Converting path '%s' for server '%s'\n", s1_path, server_prefix);

    // Checking if path is empty
    if (s1_path == NULL || strlen(s1_path) == 0)
    {
        // Using server root for empty path
        snprintf(result_path, max_size, "%s", server_prefix);
        printf("[S1] Empty path converted to: '%s'\n", result_path);
        return;
    }

    // Converting different S1 path formats
    if (strncmp(s1_path, "~/S1/", 5) == 0)
    {
        // Converting ~/S1/test to S2/test
        snprintf(result_path, max_size, "%s/%s", server_prefix, s1_path + 5);
    }
    else if (strcmp(s1_path, "~/S1") == 0)
    {
        // Converting ~/S1 to S2
        snprintf(result_path, max_size, "%s", server_prefix);
    }
    else if (strncmp(s1_path, "~S1/", 4) == 0)
    {
        // Converting ~S1/test to S2/test
        snprintf(result_path, max_size, "%s/%s", server_prefix, s1_path + 4);
    }
    else if (strcmp(s1_path, "~S1") == 0)
    {
        // Converting ~S1 to S2
        snprintf(result_path, max_size, "%s", server_prefix);
    }
    else if (s1_path[0] == '/')
    {
        // Converting absolute path /test to S2/test
        snprintf(result_path, max_size, "%s%s", server_prefix, s1_path);
    }
    else
    {
        // Converting relative path test to S2/test
        snprintf(result_path, max_size, "%s/%s", server_prefix, s1_path);
    }

    printf("[S1] Path converted to: '%s'\n", result_path);
}

/* FILE TRANSFER FUNCTIONS */

// Sending file to another server (S2/S3/S4) or client
int send_file_to_S1(int socket, const char *full_path)
{
    // Creating buffer for reading file chunks
    char buffer[BUFFER_SIZE];
    // Creating file pointer for reading
    FILE *file;
    // Storing file size
    long file_size;
    // Tracking total bytes sent
    long total_sent = 0;

    printf("[S1] Preparing to send file: %s\n", full_path);

    // Opening file for reading in binary mode
    file = fopen(full_path, "rb");
    // Checking if file exists
    if (file == NULL)
    {
        printf("[S1] File not found: %s\n", full_path);
        return -1;
    }

    // Moving to end of file to get size
    fseek(file, 0, SEEK_END);
    // Getting current position (file size)
    file_size = ftell(file);
    // Moving back to beginning
    fseek(file, 0, SEEK_SET);

    printf("[S1] File size: %ld bytes\n", file_size);

    // Sending file size first
    if (send(socket, &file_size, sizeof(file_size), 0) == -1)
    {
        printf("[S1] Failed to send file size\n");
        fclose(file);
        return -1;
    }
    printf("[S1] File size sent\n");

    // Sending file data in chunks
    printf("[S1] Starting file transfer...\n");
    while (total_sent < file_size)
    {
        // Reading chunk from file
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        // Checking if read was successful
        if (bytes_read <= 0)
        {
            printf("[S1] Failed to read from file\n");
            fclose(file);
            return -1;
        }

        // Sending chunk over socket
        int bytes_sent = send(socket, buffer, bytes_read, 0);
        // Checking if send was successful
        if (bytes_sent <= 0)
        {
            printf("[S1] Failed to send file data\n");
            fclose(file);
            return -1;
        }

        // Updating total bytes sent
        total_sent += bytes_sent;

        // Showing progress for larger files
        if (file_size > 10000)
        {
            printf("[S1] Progress: %ld/%ld bytes (%.1f%%)\r",
                   total_sent, file_size, (total_sent * 100.0) / file_size);
            fflush(stdout);
        }
    }

    // Closing file
    fclose(file);
    printf("\n[S1] File sent successfully\n");
    return 0;
}

// Receiving file from client
int receive_file_from_client(int client_socket, const char *filename)
{
    // Building complete path for temporary storage
    char full_path[MAX_PATH];
    // Creating buffer for file data chunks
    char buffer[BUFFER_SIZE];
    // Creating file pointer for writing
    FILE *file;
    // Storing incoming file size
    long file_size;
    // Tracking total bytes received
    long total_received = 0;

    printf("[S1] Receiving file from client: %s\n", filename);

    // Adding small delay for synchronization
    usleep(50000);

    // Attempting to receive file size with retries
    int attempts = 0;
    int size_received = 0;

    // Retrying up to 3 times
    while (attempts < 3 && size_received != sizeof(file_size))
    {
        // Receiving file size
        size_received = recv(client_socket, &file_size, sizeof(file_size), 0);
        // Breaking if successful
        if (size_received == sizeof(file_size))
        {
            break;
        }

        // Incrementing attempt counter
        attempts++;
        printf("[S1] Retry %d: Attempting to receive file size\n", attempts);
        // Adding delay before retry
        usleep(100000);
    }

    // Checking if file size received successfully
    if (size_received != sizeof(file_size))
    {
        printf("[S1] Failed to receive file size after %d attempts\n", attempts);
        return -1;
    }

    // Validating file size is reasonable
    if (file_size <= 0 || file_size > 100000000)
    {
        printf("[S1] Invalid file size: %ld bytes\n", file_size);
        return -1;
    }

    printf("[S1] File size: %ld bytes\n", file_size);

    // Building path for temporary storage
    snprintf(full_path, sizeof(full_path), "S1/temp/%s", filename);

    // Creating temp directory if needed
    create_full_directories("S1/temp");

    // Opening file for writing
    file = fopen(full_path, "wb");
    // Checking if file creation successful
    if (file == NULL)
    {
        printf("[S1] Failed to create temporary file: %s\n", full_path);
        return -1;
    }

    printf("[S1] Starting file reception...\n");
    // Receiving file data in chunks
    while (total_received < file_size)
    {
        // Calculating bytes remaining
        long remaining = file_size - total_received;
        // Determining how much to receive this chunk
        int to_receive = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;

        // Receiving data chunk
        int bytes_received = recv(client_socket, buffer, to_receive, 0);
        // Checking if receive successful
        if (bytes_received <= 0)
        {
            printf("[S1] Failed to receive file data (got %d bytes)\n", bytes_received);
            fclose(file);
            return -1;
        }

        // Writing received data to file
        int bytes_written = fwrite(buffer, 1, bytes_received, file);
        // Checking if all data written
        if (bytes_written != bytes_received)
        {
            printf("[S1] Failed to write complete data to file\n");
            fclose(file);
            return -1;
        }

        // Updating total received
        total_received += bytes_received;

        // Showing progress for larger files
        if (file_size > 10000)
        {
            printf("[S1] Progress: %ld/%ld bytes (%.1f%%)\r",
                   total_received, file_size, (total_received * 100.0) / file_size);
            fflush(stdout);
        }
    }

    // Closing file
    fclose(file);
    printf("\n[S1] File received and stored temporarily: %s\n", full_path);

    // Verifying file was written correctly
    FILE *verify = fopen(full_path, "rb");
    if (verify != NULL)
    {
        // Getting actual file size
        fseek(verify, 0, SEEK_END);
        long actual_size = ftell(verify);
        fclose(verify);

        // Comparing with expected size
        if (actual_size == file_size)
        {
            printf("[S1] File size verification: OK (%ld bytes)\n", actual_size);
        }
        else
        {
            printf("[S1] File size verification: FAILED (expected %ld, got %ld)\n",
                   file_size, actual_size);
            return -1;
        }
    }

    return 0;
}

// Receiving file from other servers (S2/S3/S4)
int receive_file_from_S1(int server_socket, const char *filename, const char *dest_path)
{
    // Building complete path for file storage
    char full_path[MAX_PATH];
    // Creating buffer for file data
    char buffer[BUFFER_SIZE];
    // Creating file pointer for writing
    FILE *file;
    // Storing file size
    long file_size;
    // Tracking bytes received
    long total_received = 0;

    printf("[S1] Receiving file from server: %s\n", filename);

    // Receiving file size from server
    if (recv(server_socket, &file_size, sizeof(file_size), 0) <= 0)
    {
        printf("[S1] Failed to receive file size from server\n");
        return -1;
    }

    printf("[S1] File size: %ld bytes\n", file_size);

    // Creating destination directory if needed
    char dest_path_copy[MAX_PATH];
    strcpy(dest_path_copy, dest_path);
    if (create_full_directories(dest_path_copy) == -1)
    {
        printf("[S1] Failed to create destination directory: %s\n", dest_path);
        return -1;
    }

    // Building complete file path
    snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);
    printf("[S1] Saving to: %s\n", full_path);

    // Opening file for writing
    file = fopen(full_path, "wb");
    if (file == NULL)
    {
        printf("[S1] Failed to create file: %s\n", full_path);
        return -1;
    }

    printf("[S1] Starting file transfer from server...\n");
    // Receiving file data in chunks
    while (total_received < file_size)
    {
        // Receiving data chunk
        int bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("[S1] Failed to receive file data from server\n");
            fclose(file);
            return -1;
        }

        // Writing data to file
        int bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received)
        {
            printf("[S1] Error writing data to file\n");
            fclose(file);
            return -1;
        }

        // Updating total received
        total_received += bytes_received;
    }

    // Closing file
    fclose(file);
    printf("[S1] File received successfully from server: %s\n", full_path);
    return 0;
}

/*SERVER CONNECTION FUNCTIONS */

// Connecting to S2 (PDF server)
int connect_to_s2()
{
    printf("[S1] Establishing connection to S2 (PDF server)...\n");

    // Creating socket for S2 connection
    int s2_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Checking if socket creation successful
    if (s2_socket == -1)
    {
        printf("[S1] Failed to create socket for S2\n");
        return -1;
    }

    // Setting up S2 address structure
    // Initializing to zero
    struct sockaddr_in s2_addr = {0};
    // Setting IPv4 protocol
    s2_addr.sin_family = AF_INET;
    // Converting port to network format
    s2_addr.sin_port = htons(S2_PORT);
    // Setting IP
    s2_addr.sin_addr.s_addr = INADDR_ANY;

    // Attempting to connect to S2
    if (connect(s2_socket, (struct sockaddr *)&s2_addr, sizeof(s2_addr)) == -1)
    {
        printf("[S1] Failed to connect to S2 server\n");
        // Closing socket before returning
        close(s2_socket);
        return -1;
    }

    printf("[S1] Successfully connected to S2 server\n");
    return s2_socket;
}

// Connecting to S3 (Text server)
int connect_to_s3()
{
    printf("[S1] Establishing connection to S3 (Text server)...\n");

    // Creating socket for S3 connection
    int s3_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Checking if socket creation successful
    if (s3_socket == -1)
    {
        printf("[S1] Failed to create socket for S3\n");
        return -1;
    }

    // Setting up S3 address structure
    struct sockaddr_in s3_addr = {0};
    s3_addr.sin_family = AF_INET;
    s3_addr.sin_port = htons(S3_PORT);
    s3_addr.sin_addr.s_addr = INADDR_ANY;

    // Attempting to connect to S3
    if (connect(s3_socket, (struct sockaddr *)&s3_addr, sizeof(s3_addr)) == -1)
    {
        printf("[S1] Failed to connect to S3 server\n");
        // Closing socket before returning
        close(s3_socket);
        return -1;
    }

    printf("[S1] Successfully connected to S3 server\n");
    return s3_socket;
}

// Connecting to S4 (ZIP server)
int connect_to_s4()
{
    printf("[S1] Establishing connection to S4 (ZIP server)...\n");

    // Creating socket for S4 connection
    int s4_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Checking if socket creation successful
    if (s4_socket == -1)
    {
        printf("[S1] Failed to create socket for S4\n");
        return -1;
    }

    // Setting up S4 address structure
    struct sockaddr_in s4_addr = {0};
    s4_addr.sin_family = AF_INET;
    s4_addr.sin_port = htons(S4_PORT);
    s4_addr.sin_addr.s_addr = INADDR_ANY;

    // Attempting to connect to S4
    if (connect(s4_socket, (struct sockaddr *)&s4_addr, sizeof(s4_addr)) == -1)
    {
        printf("[S1] Failed to connect to S4 server\n");
        // Closing socket before returning
        close(s4_socket);
        return -1;
    }

    printf("[S1] Successfully connected to S4 server\n");
    return s4_socket;
}

// Sending file to other servers (S2/S3/S4)
int send_file_to_server(int server_socket, const char *filename, const char *dest_path)
{
    // Creating command buffer
    char command[1024];
    // Building temp file path
    char temp_file_path[MAX_PATH];
    // Creating response buffer
    char response[256];

    printf("[S1] Sending file to server: %s\n", filename);

    // Building STORE command for server
    snprintf(command, sizeof(command), "STORE %s %s", filename, dest_path);
    printf("[S1] Sending command: %s\n", command);

    // Sending command to server
    if (send(server_socket, command, strlen(command), 0) == -1)
    {
        printf("[S1] Failed to send STORE command\n");
        return -1;
    }

    // Receiving response from server
    int bytes_received = recv(server_socket, response, sizeof(response) - 1, 0);
    if (bytes_received <= 0)
    {
        printf("[S1] No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes_received] = '\0';

    printf("[S1] Server response: %s\n", response);

    // Checking if server is ready to receive file
    if (strncmp(response, "READY", 5) != 0)
    {
        printf("[S1] Server not ready, received: %s\n", response);
        return -1;
    }

    // Building temp file path
    snprintf(temp_file_path, sizeof(temp_file_path), "S1/temp/%s", filename);
    printf("[S1] Sending file data from: %s\n", temp_file_path);

    // Sending file data to server using existing function
    if (send_file_to_S1(server_socket, temp_file_path) == -1)
    {
        printf("[S1] Failed to send file data to server\n");
        return -1;
    }

    // Receiving final response from server
    bytes_received = recv(server_socket, response, sizeof(response) - 1, 0);
    if (bytes_received > 0)
    {
        // Adding null terminator
        response[bytes_received] = '\0';
        printf("[S1] Final server response: %s\n", response);

        // Checking if transfer successful
        if (strncmp(response, "SUCCESS", 7) == 0)
        {
            printf("[S1] File transfer completed successfully\n");
            return 0;
        }
        else
        {
            printf("[S1] Server reported error: %s\n", response);
            return -1;
        }
    }
    else
    {
        printf("[S1] No final response from server\n");
        return -1;
    }
}

/*FILE MANAGEMENT FUNCTIONS*/

// Storing C file locally in S1
int store_file_in_S1(const char *filename, const char *dest_path)
{
    // Building temporary file path
    char temp_path[MAX_PATH];
    // Building final file path
    char final_path[MAX_PATH];
    // Creating buffer for file copying
    char buffer[BUFFER_SIZE];
    // Creating file pointers for source and destination
    FILE *temp_file, *final_file;
    // Storing bytes read in each chunk
    int bytes_read;

    printf("[S1] Storing C file locally: %s\n", filename);

    // Creating destination directory if needed
    char dest_path_copy[MAX_PATH];
    strcpy(dest_path_copy, dest_path);
    if (create_full_directories(dest_path_copy) == -1)
    {
        printf("[S1] Failed to create destination directory: %s\n", dest_path);
        return -1;
    }

    // Building paths for temporary and final locations
    snprintf(temp_path, sizeof(temp_path), "S1/temp/%s", filename);
    snprintf(final_path, sizeof(final_path), "%s/%s", dest_path, filename);

    printf("[S1] Moving file from %s to %s\n", temp_path, final_path);

    // Opening temporary file for reading
    temp_file = fopen(temp_path, "rb");
    if (temp_file == NULL)
    {
        printf("[S1] Cannot open temp file: %s\n", temp_path);
        return -1;
    }

    // Opening final file for writing
    final_file = fopen(final_path, "wb");
    if (final_file == NULL)
    {
        printf("[S1] Cannot create final file: %s\n", final_path);
        // Closing temp file before returning
        fclose(temp_file);
        return -1;
    }

    printf("[S1] Copying file data...\n");
    // Copying file data from temp to final location
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, temp_file)) > 0)
    {
        // Writing chunk to final file
        int bytes_written = fwrite(buffer, 1, bytes_read, final_file);
        // Checking if all bytes written successfully
        if (bytes_written != bytes_read)
        {
            printf("[S1] Failed to write data to final file\n");
            fclose(temp_file);
            fclose(final_file);
            return -1;
        }
    }

    // Closing both files
    fclose(temp_file);
    fclose(final_file);

    // Deleting temporary file for cleanup
    printf("[S1] Cleaning up temporary file: %s\n", temp_path);
    if (remove(temp_path) == 0)
    {
        printf("[S1] Temporary file deleted successfully\n");
    }
    else
    {
        printf("[S1] Warning: Failed to delete temporary file\n");
        // Don't return error - file was still copied successfully
    }

    printf("[S1] C file stored locally: %s\n", final_path);
    return 0;
}

// Deleting file (used for C files in S1)
int delete_file(const char *full_path)
{
    printf("[S1] Attempting to delete file: %s\n", full_path);

    // Checking if file exists
    FILE *file = fopen(full_path, "r");
    if (file == NULL)
    {
        printf("[S1] File not found: %s\n", full_path);
        return -1;
    }
    // Closing file handle
    fclose(file);

    // Removing the file
    if (remove(full_path) == 0)
    {
        printf("[S1] File deleted successfully: %s\n", full_path);
        return 0;
    }
    else
    {
        printf("[S1] Failed to delete file: %s\n", full_path);
        return -1;
    }
}

// Getting list of C files from local S1 directory
int get_local_c_files(const char *directory_path, char *file_list, int max_size)
{
    // Creating directory pointer
    DIR *dir;
    // Creating directory entry pointer
    struct dirent *entry;
    // Creating temporary name buffer
    char temp_name[256];
    // Tracking total length of file list
    int total_length = 0;

    printf("[S1] Getting local C files from: %s\n", directory_path);

    // Initializing file list as empty
    file_list[0] = '\0';

    // Opening directory
    dir = opendir(directory_path);
    if (dir == NULL)
    {
        printf("[S1] Cannot open directory: %s\n", directory_path);
        return -1;
    }

    // Reading directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // Skipping current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Filtering by .c extension
        if (strstr(entry->d_name, ".c") != NULL)
        {
            // Formatting filename with newline
            snprintf(temp_name, sizeof(temp_name), "%s\n", entry->d_name);

            // Checking if adding this file would exceed buffer size
            if (total_length + strlen(temp_name) < max_size)
            {
                // Adding filename to list
                strcat(file_list, temp_name);
                total_length += strlen(temp_name);
                printf("[S1] Found C file: %s", temp_name);
            }
            else
            {
                printf("[S1] Warning: Buffer full, skipping remaining files\n");
                break;
            }
        }
    }

    // Closing directory
    closedir(dir);
    printf("[S1] Local C file listing complete\n");
    return 0;
}

// Getting file list from other servers (S2/S3/S4)
int get_files_from_server(int server_socket, const char *directory_path, char *file_list, int max_size)
{
    // Creating command buffer
    char command[1024];
    // Creating response buffer
    char response[4096];
    // Storing bytes received
    int bytes_received;

    printf("[S1] Requesting file list from server for: %s\n", directory_path);

    // Building LIST command
    snprintf(command, sizeof(command), "LIST %s", directory_path);

    // Sending LIST command to server
    if (send(server_socket, command, strlen(command), 0) == -1)
    {
        printf("[S1] Failed to send LIST command\n");
        return -1;
    }

    // Receiving file list from server
    bytes_received = recv(server_socket, response, sizeof(response) - 1, 0);
    if (bytes_received <= 0)
    {
        printf("[S1] Failed to receive file list from server\n");
        return -1;
    }

    // Adding null terminator
    response[bytes_received] = '\0';
    printf("[S1] Received file list from server:\n%s", response);

    // Checking if no files found
    if (strncmp(response, "No files", 8) == 0)
    {
        printf("[S1] No files found on server\n");
        // Setting empty list
        file_list[0] = '\0';
        return 0;
    }

    // Copying response to file list
    if (strlen(response) < max_size)
    {
        strcpy(file_list, response);
    }
    else
    {
        // Truncating if response too long
        strncpy(file_list, response, max_size - 1);
        file_list[max_size - 1] = '\0';
        printf("[S1] Warning: File list truncated\n");
    }

    printf("[S1] File list received from server\n");
    return 0;
}

// Sorting file list alphabetically
void sort_file_list(char file_list[][256], int count)
{
    // Creating temporary buffer for swapping
    char temp[256];

    printf("[S1] Sorting %d files alphabetically\n", count);

    // Using bubble sort algorithm
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = 0; j < count - i - 1; j++)
        {
            // Comparing adjacent elements
            if (strcmp(file_list[j], file_list[j + 1]) > 0)
            {
                // Swapping elements if out of order
                strcpy(temp, file_list[j]);
                strcpy(file_list[j], file_list[j + 1]);
                strcpy(file_list[j + 1], temp);
            }
        }
    }

    printf("[S1] File list sorted successfully\n");
}

/* TAR FILE FUNCTIONS */

// Creating tar file for C files locally
int create_local_tar_c_files(const char *tar_filepath)
{
    // Declaring a command string
    char command[2048];
    // Declaring a file pointer for checking size later
    FILE *tar_file;
    // Declaring a variable to store return code of system()
    int result_code;

    // Making sure S1 directory exists
    create_full_directories("S1");

    // Printing what we are creating
    printf("[S1] Creating tar file for C files: %s\n", tar_filepath);

    // Trying a fast way first (may fail on macOS because -printf is GNU-only)
    // Building the command (creating a tar at the exact path we received)
    snprintf(
        command,
        sizeof(command),
        // Changing directory to S1 then adding relative paths so the tar has clean paths
        // Using GNU find's -printf when available
        "sh -c \"cd S1 2>/dev/null && files=$(find . -name '*.c' -type f -printf '%%P\\n' 2>/dev/null); "
        "if [ -n \\\"$files\\\" ]; then tar -cf '%s' -C S1 $files 2>/dev/null; else exit 99; fi\"",
        tar_filepath);

    // Running the fast path
    result_code = system(command);

    // Checking if fast path failed (non-zero) or special 99 (no files)
    if (result_code != 0)
    {
        // Explaining why we are switching
        printf("[S1] WARNING: First tar method failed or not supported, trying portable method\n");

        // Building portable command (works on macOS/BSD and Linux)
        // Finding files then piping list to tar with --files-from
        snprintf(
            command,
            sizeof(command),
            "find S1 -name '*.c' -type f 2>/dev/null | tar -cf '%s' --files-from=- 2>/dev/null",
            tar_filepath);

        // Running the portable path
        result_code = system(command);
        // Checking error again
        if (result_code != 0)
        {
            // Printing error and returning
            printf("[S1] ERROR: Both tar methods failed\n");
            return -1;
        }
    }

    // Opening the tar file to verify it exists
    tar_file = fopen(tar_filepath, "rb");
    // Checking if open failed
    if (tar_file == NULL)
    {
        // Printing error and returning
        printf("[S1] ERROR: Tar file was not created: %s\n", tar_filepath);
        return -1;
    }

    // Moving to end of file to get size
    fseek(tar_file, 0, SEEK_END);
    // Getting size in bytes
    long size = ftell(tar_file);
    // Closing the file
    fclose(tar_file);

    // Checking if size is zero
    if (size == 0)
    {
        // Printing error
        printf("[S1] ERROR: Tar file is empty: %s\n", tar_filepath);
        // Removing the empty file
        remove(tar_filepath);
        // Returning error
        return -1;
    }

    // Printing success message
    printf("[S1] TAR file created successfully: %s (size: %ld bytes)\n", tar_filepath, size);
    // Returning success
    return 0;
}

// Sending tar file to client
int send_tar_file_to_client(int client_socket, const char *tar_file_path)
{
    // Creating file pointer for tar file
    FILE *tar_file;
    // Creating buffer for file data
    char buffer[BUFFER_SIZE];
    // Storing file size
    long file_size;
    // Tracking bytes sent
    long total_sent = 0;

    printf("[S1] Sending tar file to client: %s\n", tar_file_path);

    // Opening tar file for reading
    tar_file = fopen(tar_file_path, "rb");
    if (tar_file == NULL)
    {
        printf("[S1] Cannot open tar file: %s\n", tar_file_path);
        return -1;
    }

    // Getting tar file size
    fseek(tar_file, 0, SEEK_END);
    file_size = ftell(tar_file);
    fseek(tar_file, 0, SEEK_SET);

    printf("[S1] Tar file size: %ld bytes\n", file_size);

    // Sending file size to client first
    if (send(client_socket, &file_size, sizeof(file_size), 0) == -1)
    {
        printf("[S1] Failed to send tar file size\n");
        fclose(tar_file);
        return -1;
    }

    printf("[S1] Starting tar file transfer...\n");
    // Sending tar file data in chunks
    while (total_sent < file_size)
    {
        // Reading chunk from tar file
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, tar_file);
        if (bytes_read <= 0)
        {
            printf("[S1] Failed to read tar file\n");
            fclose(tar_file);
            return -1;
        }

        // Sending chunk to client
        int bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent <= 0)
        {
            printf("[S1] Failed to send tar data\n");
            fclose(tar_file);
            return -1;
        }

        // Updating total sent
        total_sent += bytes_sent;

        // Showing progress
        printf("[S1] Tar transfer progress: %ld/%ld bytes (%.1f%%)\r",
               total_sent, file_size, (total_sent * 100.0) / file_size);
        fflush(stdout);
    }

    // Closing tar file
    fclose(tar_file);
    printf("\n[S1] Tar file sent to client successfully\n");
    return 0;
}

/* CLIENT PROCESSING FUNCTION */

// Processing client requests in child process
void prcclient(int client_socket)
{
    printf("\n[S1] New client connected - starting service\n");

    // Creating command buffer
    char command[1024];
    // Creating response buffer
    char response[1024];

    // Sending welcome message to client
    char welcome[] = "Welcome to S1 server.";
    send(client_socket, welcome, strlen(welcome), 0);
    printf("[S1] Welcome message sent to client\n");

    // Starting infinite loop to process client commands
    while (1)
    {
        // Clearing command buffer
        memset(command, 0, sizeof(command));

        // Receiving command from client
        int bytes = recv(client_socket, command, sizeof(command) - 1, 0);

        // Checking if client disconnected
        if (bytes <= 0)
        {
            printf("[S1] Client disconnected - ending session\n");
            break;
        }

        // Adding null terminator to command
        command[bytes] = '\0';
        printf("\n[S1] Processing command: %s\n", command);

        /*=== UPLOADF COMMAND PROCESSING ===*/
        if (strncmp(command, "uploadf", 7) == 0)
        {
            printf("[S1] Processing uploadf command\n");

            // Creating command copy for parsing
            char command_copy[1024];
            strcpy(command_copy, command);

            // Creating token array for parsed arguments
            char *tokens[6];
            int token_count = 0;

            // Parsing command using strtok
            char *token = strtok(command_copy, " ");
            while (token != NULL && token_count < 6)
            {
                // Allocating memory for token
                tokens[token_count] = malloc(strlen(token) + 1);
                strcpy(tokens[token_count], token);
                token_count++;
                token = strtok(NULL, " ");
            }

            printf("[S1] Parsed %d arguments from command\n", token_count);
            for (int i = 0; i < token_count; i++)
            {
                printf("[S1] Argument[%d]: '%s'\n", i, tokens[i]);
            }

            // Validating command structure
            if (token_count < 3)
            {
                send(client_socket, "ERROR: Not enough arguments. Command: uploadf file1 [file2] [file3] dest_path", 75, 0);
                printf("[S1] ERROR: Invalid uploadf - need at least 3 arguments\n");
                // Freeing allocated memory
                for (int i = 0; i < token_count; i++)
                    free(tokens[i]);
                continue;
            }

            if (token_count > 5)
            {
                send(client_socket, "ERROR: Too many arguments. Max 3 files allowed", 47, 0);
                printf("[S1] ERROR: Invalid uploadf - too many arguments\n");
                // Freeing allocated memory
                for (int i = 0; i < token_count; i++)
                    free(tokens[i]);
                continue;
            }

            // Extracting file count and destination
            // Total tokens - command - dest_path
            int file_count = token_count - 2;
            char destination_path[512];
            // Last token is dest_path
            strcpy(destination_path, tokens[token_count - 1]);

            // Extracting filenames
            char filenames[3][256];
            memset(filenames, 0, sizeof(filenames));
            for (int i = 0; i < file_count && i < 3; i++)
            {
                // Skip command token
                strcpy(filenames[i], tokens[i + 1]);
            }

            printf("[S1] File count: %d\n", file_count);
            printf("[S1] Destination path: '%s'\n", destination_path);
            for (int i = 0; i < file_count; i++)
            {
                printf("[S1] File[%d]: '%s'\n", i, filenames[i]);
            }

            // Freeing token memory
            for (int i = 0; i < token_count; i++)
                free(tokens[i]);

            // Sending ready response to client
            send(client_socket, "READY", 5, 0);
            printf("[S1] Sent READY signal to client\n");

            // Receiving files from client
            int files_received = 0;
            printf("[S1] Starting file reception phase\n");

            for (int i = 0; i < file_count; i++)
            {
                printf("[S1] Receiving file %d/%d: %s\n", i + 1, file_count, filenames[i]);

                // Receiving file from client
                if (receive_file_from_client(client_socket, filenames[i]) == 0)
                {
                    files_received++;
                    printf("[S1] Successfully received: %s\n", filenames[i]);

                    // Adding delay between files for synchronization
                    if (i < file_count - 1)
                    {
                        printf("[S1] Waiting before next file\n");
                        // 100ms delay
                        usleep(100000);
                    }
                }
                else
                {
                    printf("[S1] ERROR: Failed to receive: %s\n", filenames[i]);
                    break;
                }
            }

            // Checking if all files received successfully
            if (files_received != file_count)
            {
                send(client_socket, "ERROR: Failed to receive all files", 34, 0);
                printf("[S1] ERROR: Upload failed - only received %d/%d files\n", files_received, file_count);
                continue;
            }

            printf("[S1] All files received successfully\n");
            printf("[S1] Starting file distribution phase\n");

            // Distributing files to appropriate servers
            int success_count = 0;
            for (int i = 0; i < file_count; i++)
            {
                char *filename = filenames[i];
                char server_path[512];
                char temp_file_path[512];

                printf("\n[S1] Processing file: %s\n", filename);
                printf("[S1] Source destination: '%s'\n", destination_path);
                snprintf(temp_file_path, sizeof(temp_file_path), "S1/temp/%s", filename);

                // Routing based on file extension
                if (strstr(filename, ".pdf") != NULL)
                {
                    printf("[S1] Routing PDF file to S2 server\n");
                    // Converting path for S2
                    convert_path_for_server(destination_path, "S2", server_path, sizeof(server_path));

                    // Connecting to S2
                    int s2_socket = connect_to_s2();
                    if (s2_socket != -1)
                    {
                        // Sending file to S2
                        if (send_file_to_server(s2_socket, filename, server_path) == 0)
                        {
                            success_count++;
                            printf("[S1] SUCCESS: PDF file sent to S2\n");
                            // Cleaning up temp file
                            remove(temp_file_path);
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send PDF to S2\n");
                        }
                        close(s2_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S2 server\n");
                    }
                }
                else if (strstr(filename, ".txt") != NULL)
                {
                    printf("[S1] Routing TXT file to S3 server\n");
                    // Converting path for S3
                    convert_path_for_server(destination_path, "S3", server_path, sizeof(server_path));

                    // Connecting to S3
                    int s3_socket = connect_to_s3();
                    if (s3_socket != -1)
                    {
                        // Sending file to S3
                        if (send_file_to_server(s3_socket, filename, server_path) == 0)
                        {
                            success_count++;
                            printf("[S1] SUCCESS: TXT file sent to S3\n");
                            // Cleaning up temp file
                            remove(temp_file_path);
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send TXT to S3\n");
                        }
                        close(s3_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S3 server\n");
                    }
                }
                else if (strstr(filename, ".zip") != NULL)
                {
                    printf("[S1] Routing ZIP file to S4 server\n");
                    // Converting path for S4
                    convert_path_for_server(destination_path, "S4", server_path, sizeof(server_path));

                    // Connecting to S4
                    int s4_socket = connect_to_s4();
                    if (s4_socket != -1)
                    {
                        // Sending file to S4
                        if (send_file_to_server(s4_socket, filename, server_path) == 0)
                        {
                            success_count++;
                            printf("[S1] SUCCESS: ZIP file sent to S4\n");
                            // Cleaning up temp file
                            remove(temp_file_path);
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send ZIP to S4\n");
                        }
                        close(s4_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S4 server\n");
                    }
                }
                else if (strstr(filename, ".c") != NULL)
                {
                    printf("[S1] Storing C file locally in S1\n");
                    // Converting path for local storage
                    convert_path_for_server(destination_path, "S1", server_path, sizeof(server_path));

                    // Storing file locally
                    if (store_file_in_S1(filename, server_path) == 0)
                    {
                        success_count++;
                        printf("[S1] SUCCESS: C file stored locally\n");
                    }
                    else
                    {
                        printf("[S1] ERROR: Failed to store C file locally\n");
                    }
                }
                else
                {
                    printf("[S1] WARNING: Unknown file type - skipping: %s\n", filename);
                }

                printf("[S1] End processing file: %s\n", filename);
            }

            // Sending final response to client
            printf("\n[S1] Distribution summary: %d/%d files successful\n", success_count, file_count);

            if (success_count == file_count)
            {
                send(client_socket, "SUCCESS: All files uploaded and distributed successfully", 56, 0);
                printf("[S1] SUCCESS: All files uploaded successfully\n");
            }
            else if (success_count > 0)
            {
                char partial_msg[256];
                snprintf(partial_msg, sizeof(partial_msg),
                         "PARTIAL SUCCESS: %d/%d files uploaded successfully",
                         success_count, file_count);
                send(client_socket, partial_msg, strlen(partial_msg), 0);
                printf("[S1] PARTIAL: Some files uploaded successfully\n");
            }
            else
            {
                send(client_socket, "ERROR: Failed to distribute any files", 37, 0);
                printf("[S1] ERROR: All uploads failed\n");
            }

            printf("[S1] UPLOADF command processing complete\n");
        }

        /*=== DOWNLF COMMAND PROCESSING ===*/
        else if (strncmp(command, "downlf", 6) == 0)
        {
            printf("[S1] Processing downlf command\n");

            // Parsing command arguments
            char cmd[20], file1_path[512], file2_path[512], extra_arg[512];
            int argc = sscanf(command, "%s %s %s %s", cmd, file1_path, file2_path, extra_arg);

            printf("[S1] Parsed %d arguments from downlf command\n", argc);

            // Validating argument count
            if (argc < 2)
            {
                send(client_socket, "ERROR: Not enough arguments. Command: downlf filepath1 [filepath2]", 64, 0);
                printf("[S1] ERROR: Invalid downlf - not enough arguments\n");
                continue;
            }

            if (argc > 3)
            {
                send(client_socket, "ERROR: Too many arguments. Command: downlf filepath1 [filepath2] (max 2 files)", 76, 0);
                printf("[S1] ERROR: Invalid downlf - too many arguments\n");
                continue;
            }

            // Determining number of files to download
            int file_count = argc - 1; // Subtract command name
            char *file_paths[2] = {file1_path, file2_path};

            printf("[S1] Download request for %d files\n", file_count);

            // Retrieving each file from appropriate servers
            int success_count = 0;
            for (int i = 0; i < file_count; i++)
            {
                char *full_path = file_paths[i];
                char filename[256];
                char directory_path[512];
                char server_path[512];

                printf("[S1] Processing download %d/%d: %s\n", i + 1, file_count, full_path);

                // Extracting filename from full path
                char *last_slash = strrchr(full_path, '/');
                if (last_slash == NULL)
                {
                    printf("[S1] ERROR: Invalid file path format: %s\n", full_path);
                    continue;
                }
                strcpy(filename, last_slash + 1);

                // Extracting directory path
                int dir_length = last_slash - full_path;
                strncpy(directory_path, full_path, dir_length);
                directory_path[dir_length] = '\0';

                printf("[S1] Filename: %s, Directory: %s\n", filename, directory_path);

                // Routing to appropriate server based on file extension
                if (strstr(filename, ".pdf") != NULL)
                {
                    printf("[S1] Retrieving PDF file from S2\n");

                    // Converting ~S1/abcd to S2/abcd
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S2");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S2");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path);
                    }

                    // Connecting to S2
                    int s2_socket = connect_to_s2();
                    if (s2_socket != -1)
                    {
                        char retrieve_command[1024];
                        snprintf(retrieve_command, sizeof(retrieve_command), "RETRIEVE %s/%s", server_path, filename);
                        printf("[S1] Sending to S2: %s\n", retrieve_command);

                        // Sending retrieve command to S2
                        if (send(s2_socket, retrieve_command, strlen(retrieve_command), 0) != -1)
                        {
                            // Receiving file from S2 and saving to temp
                            if (receive_file_from_S1(s2_socket, filename, "S1/temp") == 0)
                            {
                                success_count++;
                                printf("[S1] Successfully retrieved %s from S2\n", filename);
                            }
                            else
                            {
                                printf("[S1] ERROR: Failed to receive %s from S2\n", filename);
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send RETRIEVE command to S2\n");
                        }
                        close(s2_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S2\n");
                    }
                }
                else if (strstr(filename, ".txt") != NULL)
                {
                    printf("[S1] Retrieving TXT file from S3\n");

                    // Converting path for S3
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S3");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S3");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path);
                    }

                    // Connecting to S3
                    int s3_socket = connect_to_s3();
                    if (s3_socket != -1)
                    {
                        char retrieve_command[1024];
                        snprintf(retrieve_command, sizeof(retrieve_command), "RETRIEVE %s/%s", server_path, filename);
                        printf("[S1] Sending to S3: %s\n", retrieve_command);

                        // Sending retrieve command to S3
                        if (send(s3_socket, retrieve_command, strlen(retrieve_command), 0) != -1)
                        {
                            // Receiving file from S3 and saving to temp
                            if (receive_file_from_S1(s3_socket, filename, "S1/temp") == 0)
                            {
                                success_count++;
                                printf("[S1] Successfully retrieved %s from S3\n", filename);
                            }
                            else
                            {
                                printf("[S1] ERROR: Failed to receive %s from S3\n", filename);
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send RETRIEVE command to S3\n");
                        }
                        close(s3_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S3\n");
                    }
                }
                else if (strstr(filename, ".zip") != NULL)
                {
                    printf("[S1] Retrieving ZIP file from S4\n");

                    // Converting path for S4
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S4");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S4");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path);
                    }

                    // Connecting to S4
                    int s4_socket = connect_to_s4();
                    if (s4_socket != -1)
                    {
                        char retrieve_command[1024];
                        snprintf(retrieve_command, sizeof(retrieve_command), "RETRIEVE %s/%s", server_path, filename);
                        printf("[S1] Sending to S4: %s\n", retrieve_command);

                        // Sending retrieve command to S4
                        if (send(s4_socket, retrieve_command, strlen(retrieve_command), 0) != -1)
                        {
                            // Receiving file from S4 and saving to temp
                            if (receive_file_from_S1(s4_socket, filename, "S1/temp") == 0)
                            {
                                success_count++;
                                printf("[S1] Successfully retrieved %s from S4\n", filename);
                            }
                            else
                            {
                                printf("[S1] ERROR: Failed to receive %s from S4\n", filename);
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send RETRIEVE command to S4\n");
                        }
                        close(s4_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S4\n");
                    }
                }
                else if (strstr(filename, ".c") != NULL)
                {
                    printf("[S1] Retrieving C file from local S1 storage\n");

                    // Converting path for local S1 storage
                    char local_directory[MAX_PATH];
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        // ~/S1/program -> S1/program
                        snprintf(local_directory, sizeof(local_directory), "S1/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        // ~/S1 -> S1
                        strcpy(local_directory, "S1");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        // ~S1/program -> S1/program
                        snprintf(local_directory, sizeof(local_directory), "S1/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        // ~S1 -> S1
                        strcpy(local_directory, "S1");
                    }
                    else
                    {
                        // Relative path -> S1/relative_path
                        snprintf(local_directory, sizeof(local_directory), "S1/%s", directory_path);
                    }

                    // Copying from local S1 storage to temp
                    char local_path[MAX_PATH];
                    char temp_path[MAX_PATH];
                    snprintf(local_path, sizeof(local_path), "%s/%s", local_directory, filename);
                    snprintf(temp_path, sizeof(temp_path), "S1/temp/%s", filename);

                    printf("[S1] Copying from %s to %s\n", local_path, temp_path);

                    // Opening source file for reading
                    FILE *src = fopen(local_path, "rb");
                    if (src != NULL)
                    {
                        // Creating temp directory if needed
                        create_full_directories("S1/temp");

                        // Opening destination file for writing
                        FILE *dst = fopen(temp_path, "wb");
                        if (dst != NULL)
                        {
                            // Copying file data
                            char buffer[BUFFER_SIZE];
                            int bytes_read;
                            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0)
                            {
                                fwrite(buffer, 1, bytes_read, dst);
                            }
                            fclose(dst);
                            success_count++;
                            printf("[S1] Successfully copied local C file\n");
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to create temp file: %s\n", temp_path);
                        }
                        fclose(src);
                    }
                    else
                    {
                        printf("[S1] ERROR: Local C file not found: %s\n", local_path);
                    }
                }
                else
                {
                    printf("[S1] ERROR: Unknown file type: %s\n", filename);
                }
            }

            // Checking if any files were successfully retrieved
            if (success_count == 0)
            {
                send(client_socket, "ERROR: No files could be retrieved", 34, 0);
                printf("[S1] ERROR: Download failed - no files retrieved\n");
                continue;
            }

            // Sending client number of files we're sending
            char count_msg[64];
            snprintf(count_msg, sizeof(count_msg), "READY %d", success_count);
            send(client_socket, count_msg, strlen(count_msg), 0);
            printf("[S1] Told client we're sending %d files\n", success_count);

            // Adding delay to ensure client processes READY message
            sleep(1);

            // Sending each file to client
            for (int i = 0; i < file_count; i++)
            {
                char *full_path = file_paths[i];
                char filename[256];
                char temp_path[MAX_PATH];

                // Extracting filename
                char *last_slash = strrchr(full_path, '/');
                if (last_slash != NULL)
                {
                    strcpy(filename, last_slash + 1);
                    snprintf(temp_path, sizeof(temp_path), "S1/temp/%s", filename);

                    // Checking if file exists in temp
                    FILE *check_file = fopen(temp_path, "r");
                    if (check_file != NULL)
                    {
                        fclose(check_file);

                        printf("[S1] Sending %s to client\n", filename);

                        // Sending filename with null terminator
                        char filename_with_null[300];
                        snprintf(filename_with_null, sizeof(filename_with_null), "%s", filename);
                        int filename_len = strlen(filename_with_null) + 1; // +1 for null terminator

                        // Sending filename to client
                        if (send(client_socket, filename_with_null, filename_len, 0) == -1)
                        {
                            printf("[S1] ERROR: Failed to send filename\n");
                            continue;
                        }
                        printf("[S1] Sent filename: '%s' with null terminator\n", filename);

                        // Sending file data using existing function
                        if (send_file_to_S1(client_socket, temp_path) == 0)
                        {
                            printf("[S1] Successfully sent %s to client\n", filename);
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send %s to client\n", filename);
                        }

                        // Cleaning up temp file
                        remove(temp_path);
                        printf("[S1] Cleaned up temp file: %s\n", temp_path);
                    }
                    else
                    {
                        printf("[S1] ERROR: File not found in temp after retrieval: %s\n", temp_path);
                    }
                }
            }

            printf("[S1] DOWNLF command processing complete\n");
        }

        /*=== REMOVEF COMMAND PROCESSING ===*/
        else if (strncmp(command, "removef", 7) == 0)
        {
            printf("[S1] Processing removef command\n");

            // First, parsing ALL arguments to count them properly
            char cmd[20];
            char args[10][512];
            int total_args = 0;

            // Parsing the entire command to count all arguments
            char command_copy[1024];
            strcpy(command_copy, command);

            // Tokenizing command to extract all arguments
            char *token = strtok(command_copy, " ");
            while (token != NULL && total_args < 10)
            {
                if (total_args == 0)
                {
                    // First token is the command
                    strcpy(cmd, token);
                }
                else
                {
                    // Store file arguments
                    strcpy(args[total_args - 1], token);
                }
                total_args++;
                // Getting next token
                token = strtok(NULL, " ");
            }

            // Calculating file count (subtract command name)
            int file_count = total_args - 1;

            printf("[S1] Parsed %d total arguments, %d file arguments\n", total_args, file_count);

            // VALIDATION FIRST - before doing any work
            if (file_count < 1)
            {
                send(client_socket, "ERROR: Not enough arguments. Command: removef filepath1 [filepath2]", 65, 0);
                printf("[S1] ERROR: Invalid removef - not enough arguments\n");
                continue;
            }

            if (file_count > 2)
            {
                send(client_socket, "ERROR: Too many arguments. Max 2 files allowed", 47, 0);
                printf("[S1] ERROR: Invalid removef - too many arguments (%d files provided, max 2 allowed)\n", file_count);
                continue;
            }

            printf("[S1] Validation passed - processing %d file(s) for deletion\n", file_count);

            // Now extract the validated file paths
            char file1_path[512] = "";
            char file2_path[512] = "";

            if (file_count >= 1)
            {
                strcpy(file1_path, args[0]);
                printf("[S1] File 1 path: %s\n", file1_path);
            }
            if (file_count >= 2)
            {
                strcpy(file2_path, args[1]);
                printf("[S1] File 2 path: %s\n", file2_path);
            }

            // Processing each file and deleting from appropriate servers
            char *file_paths[2] = {file1_path, file2_path};
            int success_count = 0;

            for (int i = 0; i < file_count; i++)
            {
                char *full_path = file_paths[i];
                char filename[256];
                char directory_path[512];
                char server_path[512];
                char delete_command[1024];
                char response[256];

                // Skip empty paths (shouldn't happen with new validation, but safety check)
                if (strlen(full_path) == 0)
                {
                    continue;
                }

                printf("[S1] Processing deletion %d/%d: %s\n", i + 1, file_count, full_path);

                // Extracting filename from full path
                char *last_slash = strrchr(full_path, '/');
                if (last_slash == NULL)
                {
                    printf("[S1] ERROR: Invalid file path format: %s\n", full_path);
                    continue;
                }
                // Getting filename after last slash
                strcpy(filename, last_slash + 1);

                // Extracting directory path
                int dir_length = last_slash - full_path;
                strncpy(directory_path, full_path, dir_length);
                directory_path[dir_length] = '\0';

                printf("[S1] Filename: %s, Directory: %s\n", filename, directory_path);

                // Route to appropriate server based on file extension
                if (strstr(filename, ".pdf") != NULL)
                {
                    printf("[S1] Deleting PDF file from S2\n");

                    // Converting ~S1/project to S2/project
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S2");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S2");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S2/%s", directory_path);
                    }

                    // Connecting to S2
                    int s2_socket = connect_to_s2();
                    if (s2_socket != -1)
                    {
                        // Sending DELETE command to S2
                        snprintf(delete_command, sizeof(delete_command), "DELETE %s/%s", server_path, filename);
                        printf("[S1] Sending to S2: %s\n", delete_command);

                        // Sending delete command to S2
                        if (send(s2_socket, delete_command, strlen(delete_command), 0) != -1)
                        {
                            // Waiting for response from S2
                            int bytes = recv(s2_socket, response, sizeof(response) - 1, 0);
                            if (bytes > 0)
                            {
                                // Adding null terminator to response
                                response[bytes] = '\0';
                                printf("[S1] S2 response: %s\n", response);

                                // Checking if deletion was successful
                                if (strstr(response, "SUCCESS") != NULL)
                                {
                                    success_count++;
                                    printf("[S1] Successfully deleted %s from S2\n", filename);
                                }
                                else
                                {
                                    printf("[S1] ERROR: Failed to delete %s from S2\n", filename);
                                }
                            }
                            else
                            {
                                printf("[S1] ERROR: No response from S2\n");
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send DELETE command to S2\n");
                        }
                        // Closing connection to S2
                        close(s2_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S2\n");
                    }
                }
                else if (strstr(filename, ".txt") != NULL)
                {
                    printf("[S1] Deleting TXT file from S3\n");

                    // Converting ~S1/project to S3/project
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S3");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S3");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S3/%s", directory_path);
                    }

                    // Connecting to S3
                    int s3_socket = connect_to_s3();
                    if (s3_socket != -1)
                    {
                        // Sending DELETE command to S3
                        snprintf(delete_command, sizeof(delete_command), "DELETE %s/%s", server_path, filename);
                        printf("[S1] Sending to S3: %s\n", delete_command);

                        // Sending delete command to S3
                        if (send(s3_socket, delete_command, strlen(delete_command), 0) != -1)
                        {
                            // Waiting for response from S3
                            int bytes = recv(s3_socket, response, sizeof(response) - 1, 0);
                            if (bytes > 0)
                            {
                                // Adding null terminator to response
                                response[bytes] = '\0';
                                printf("[S1] S3 response: %s\n", response);

                                // Checking if deletion was successful
                                if (strstr(response, "SUCCESS") != NULL)
                                {
                                    success_count++;
                                    printf("[S1] Successfully deleted %s from S3\n", filename);
                                }
                                else
                                {
                                    printf("[S1] ERROR: Failed to delete %s from S3\n", filename);
                                }
                            }
                            else
                            {
                                printf("[S1] ERROR: No response from S3\n");
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send DELETE command to S3\n");
                        }
                        // Closing connection to S3
                        close(s3_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S3\n");
                    }
                }
                else if (strstr(filename, ".zip") != NULL)
                {
                    printf("[S1] Deleting ZIP file from S4\n");

                    // Converting ~S1/project to S4/project
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path + 5);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        strcpy(server_path, "S4");
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path + 4);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        strcpy(server_path, "S4");
                    }
                    else
                    {
                        snprintf(server_path, sizeof(server_path), "S4/%s", directory_path);
                    }

                    // Connecting to S4
                    int s4_socket = connect_to_s4();
                    if (s4_socket != -1)
                    {
                        // Sending DELETE command to S4
                        snprintf(delete_command, sizeof(delete_command), "DELETE %s/%s", server_path, filename);
                        printf("[S1] Sending to S4: %s\n", delete_command);

                        // Sending delete command to S4
                        if (send(s4_socket, delete_command, strlen(delete_command), 0) != -1)
                        {
                            // Waiting for response from S4
                            int bytes = recv(s4_socket, response, sizeof(response) - 1, 0);
                            if (bytes > 0)
                            {
                                // Adding null terminator to response
                                response[bytes] = '\0';
                                printf("[S1] S4 response: %s\n", response);

                                // Checking if deletion was successful
                                if (strstr(response, "SUCCESS") != NULL)
                                {
                                    success_count++;
                                    printf("[S1] Successfully deleted %s from S4\n", filename);
                                }
                                else
                                {
                                    printf("[S1] ERROR: Failed to delete %s from S4\n", filename);
                                }
                            }
                            else
                            {
                                printf("[S1] ERROR: No response from S4\n");
                            }
                        }
                        else
                        {
                            printf("[S1] ERROR: Failed to send DELETE command to S4\n");
                        }
                        // Closing connection to S4
                        close(s4_socket);
                    }
                    else
                    {
                        printf("[S1] ERROR: Cannot connect to S4\n");
                    }
                }
                else if (strstr(filename, ".c") != NULL)
                {
                    printf("[S1] Deleting C file from local S1 storage\n");

                    // For .c files, delete from local S1 storage
                    char local_path[MAX_PATH];

                    // Convert S1 client path format to actual local filesystem path
                    if (strncmp(directory_path, "~/S1/", 5) == 0)
                    {
                        // ~/S1/abcd -> S1/abcd
                        snprintf(local_path, sizeof(local_path), "S1/%s/%s", directory_path + 5, filename);
                    }
                    else if (strncmp(directory_path, "~/S1", 4) == 0)
                    {
                        // ~/S1 -> S1
                        snprintf(local_path, sizeof(local_path), "S1/%s", filename);
                    }
                    else if (strncmp(directory_path, "~S1/", 4) == 0)
                    {
                        // ~S1/abcd -> S1/abcd
                        snprintf(local_path, sizeof(local_path), "S1/%s/%s", directory_path + 4, filename);
                    }
                    else if (strncmp(directory_path, "~S1", 3) == 0)
                    {
                        // ~S1 -> S1
                        snprintf(local_path, sizeof(local_path), "S1/%s", filename);
                    }
                    else
                    {
                        // Relative path -> S1/path
                        snprintf(local_path, sizeof(local_path), "S1/%s/%s", directory_path, filename);
                    }

                    printf("[S1] Deleting local file: %s\n", local_path);

                    // Using existing delete_file function
                    if (delete_file(local_path) == 0)
                    {
                        success_count++;
                        printf("[S1] Successfully deleted local C file: %s\n", filename);
                    }
                    else
                    {
                        printf("[S1] ERROR: Failed to delete local C file: %s\n", filename);
                    }
                }
                else
                {
                    printf("[S1] ERROR: Unknown file type: %s\n", filename);
                }
            }

            // Sending final response to client
            printf("\n[S1] Deletion summary: %d/%d files successful\n", success_count, file_count);

            if (success_count == file_count)
            {
                // All files deleted successfully
                send(client_socket, "SUCCESS: All files deleted successfully", 39, 0);
                printf("[S1] SUCCESS: All deletions completed successfully\n");
            }
            else if (success_count > 0)
            {
                // Some files deleted, some failed
                char partial_msg[256];
                snprintf(partial_msg, sizeof(partial_msg),
                         "PARTIAL SUCCESS: %d/%d files deleted successfully",
                         success_count, file_count);
                send(client_socket, partial_msg, strlen(partial_msg), 0);
                printf("[S1] PARTIAL: Deletion partially completed\n");
            }
            else
            {
                // All files failed
                send(client_socket, "ERROR: Failed to delete any files", 33, 0);
                printf("[S1] ERROR: All deletions failed\n");
            }

            printf("[S1] REMOVEF command processing complete\n");
        }

        /*=== DISPFNAMES COMMAND PROCESSING ===*/
        else if (strncmp(command, "dispfnames", 10) == 0)
        {
            printf("[S1] Processing dispfnames command\n");

            char pathname[MAX_PATH];

            // Parsing the command to extract pathname
            if (sscanf(command, "dispfnames %s", pathname) == 1)
            {
                printf("[S1] Displaying files for pathname: %s\n", pathname);

                // Buffers for different file types
                char c_files[2048] = "";
                char pdf_files[2048] = "";
                char txt_files[2048] = "";
                char zip_files[2048] = "";
                char consolidated_list[8192] = "";

                // Arrays for sorting files
                char c_list[100][256];
                char pdf_list[100][256];
                char txt_list[100][256];
                char zip_list[100][256];
                int c_count = 0, pdf_count = 0, txt_count = 0, zip_count = 0;

                // Getting .c files from local S1 directory
                printf("[S1] Getting local C files\n");

                // Converting client path to local filesystem path
                char local_c_path[MAX_PATH];
                if (strncmp(pathname, "~/S1/", 5) == 0)
                {
                    // ~/S1/code -> S1/code
                    snprintf(local_c_path, sizeof(local_c_path), "S1/%s", pathname + 5);
                }
                else if (strncmp(pathname, "~/S1", 4) == 0)
                {
                    // ~/S1 -> S1
                    strcpy(local_c_path, "S1");
                }
                else if (strncmp(pathname, "~S1/", 4) == 0)
                {
                    // ~S1/code -> S1/code
                    snprintf(local_c_path, sizeof(local_c_path), "S1/%s", pathname + 4);
                }
                else if (strncmp(pathname, "~S1", 3) == 0)
                {
                    // ~S1 -> S1
                    strcpy(local_c_path, "S1");
                }
                else
                {
                    // Relative path -> S1/path
                    snprintf(local_c_path, sizeof(local_c_path), "S1/%s", pathname);
                }

                printf("[S1] Converted local path: %s\n", local_c_path);

                // Getting local C files
                if (get_local_c_files(local_c_path, c_files, sizeof(c_files)) == 0)
                {
                    // Parsing c_files into array for sorting
                    char c_files_copy[2048];
                    strcpy(c_files_copy, c_files);

                    // Tokenizing C files string
                    char *token = strtok(c_files_copy, "\n");
                    while (token != NULL && c_count < 100)
                    {
                        strcpy(c_list[c_count], token);
                        c_count++;
                        token = strtok(NULL, "\n");
                    }
                    // Sorting C files alphabetically
                    sort_file_list(c_list, c_count);
                    printf("[S1] Found %d C files\n", c_count);
                }
                else
                {
                    printf("[S1] ERROR: Failed to get local C files\n");
                }

                // Getting .pdf files from S2
                printf("[S1] Getting PDF files from S2\n");
                int s2_socket = connect_to_s2();
                if (s2_socket != -1)
                {
                    // Converting S1 path to S2 path
                    char s2_path[MAX_PATH];
                    if (strncmp(pathname, "~/S1/", 5) == 0)
                    {
                        // ~/S1/code -> S2/code
                        snprintf(s2_path, sizeof(s2_path), "S2/%s", pathname + 5);
                    }
                    else if (strncmp(pathname, "~/S1", 4) == 0)
                    {
                        // ~/S1 -> S2
                        strcpy(s2_path, "S2");
                    }
                    else if (strncmp(pathname, "~S1/", 4) == 0)
                    {
                        // ~S1/code -> S2/code
                        snprintf(s2_path, sizeof(s2_path), "S2/%s", pathname + 4);
                    }
                    else if (strncmp(pathname, "~S1", 3) == 0)
                    {
                        // ~S1 -> S2
                        strcpy(s2_path, "S2");
                    }
                    else
                    {
                        // Relative path -> S2/path
                        snprintf(s2_path, sizeof(s2_path), "S2/%s", pathname);
                    }

                    printf("[S1] Converted S2 path: %s\n", s2_path);

                    // Getting PDF files from S2
                    if (get_files_from_server(s2_socket, s2_path, pdf_files, sizeof(pdf_files)) == 0)
                    {
                        // Parsing pdf_files into array for sorting
                        char pdf_files_copy[2048];
                        strcpy(pdf_files_copy, pdf_files);

                        // Tokenizing PDF files string
                        char *token = strtok(pdf_files_copy, "\n");
                        while (token != NULL && pdf_count < 100)
                        {
                            strcpy(pdf_list[pdf_count], token);
                            pdf_count++;
                            token = strtok(NULL, "\n");
                        }
                        // Sorting PDF files alphabetically
                        sort_file_list(pdf_list, pdf_count);
                        printf("[S1] Found %d PDF files\n", pdf_count);
                    }
                    // Closing connection to S2
                    close(s2_socket);
                }
                else
                {
                    printf("[S1] ERROR: Failed to connect to S2\n");
                }

                // Getting .txt files from S3
                printf("[S1] Getting TXT files from S3\n");
                int s3_socket = connect_to_s3();
                if (s3_socket != -1)
                {
                    // Converting S1 path to S3 path
                    char s3_path[MAX_PATH];
                    if (strncmp(pathname, "~/S1/", 5) == 0)
                    {
                        // ~/S1/code -> S3/code
                        snprintf(s3_path, sizeof(s3_path), "S3/%s", pathname + 5);
                    }
                    else if (strncmp(pathname, "~/S1", 4) == 0)
                    {
                        // ~/S1 -> S3
                        strcpy(s3_path, "S3");
                    }
                    else if (strncmp(pathname, "~S1/", 4) == 0)
                    {
                        // ~S1/code -> S3/code
                        snprintf(s3_path, sizeof(s3_path), "S3/%s", pathname + 4);
                    }
                    else if (strncmp(pathname, "~S1", 3) == 0)
                    {
                        // ~S1 -> S3
                        strcpy(s3_path, "S3");
                    }
                    else
                    {
                        // Relative path -> S3/path
                        snprintf(s3_path, sizeof(s3_path), "S3/%s", pathname);
                    }

                    printf("[S1] Converted S3 path: %s\n", s3_path);

                    // Getting TXT files from S3
                    if (get_files_from_server(s3_socket, s3_path, txt_files, sizeof(txt_files)) == 0)
                    {
                        // Parsing txt_files into array for sorting
                        char txt_files_copy[2048];
                        strcpy(txt_files_copy, txt_files);

                        // Tokenizing TXT files string
                        char *token = strtok(txt_files_copy, "\n");
                        while (token != NULL && txt_count < 100)
                        {
                            strcpy(txt_list[txt_count], token);
                            txt_count++;
                            token = strtok(NULL, "\n");
                        }
                        // Sorting TXT files alphabetically
                        sort_file_list(txt_list, txt_count);
                        printf("[S1] Found %d TXT files\n", txt_count);
                    }
                    // Closing connection to S3
                    close(s3_socket);
                }
                else
                {
                    printf("[S1] ERROR: Failed to connect to S3\n");
                }

                // Getting .zip files from S4
                printf("[S1] Getting ZIP files from S4\n");
                int s4_socket = connect_to_s4();
                if (s4_socket != -1)
                {
                    // Converting S1 path to S4 path
                    char s4_path[MAX_PATH];
                    if (strncmp(pathname, "~/S1/", 5) == 0)
                    {
                        // ~/S1/code -> S4/code
                        snprintf(s4_path, sizeof(s4_path), "S4/%s", pathname + 5);
                    }
                    else if (strncmp(pathname, "~/S1", 4) == 0)
                    {
                        // ~/S1 -> S4
                        strcpy(s4_path, "S4");
                    }
                    else if (strncmp(pathname, "~S1/", 4) == 0)
                    {
                        // ~S1/code -> S4/code
                        snprintf(s4_path, sizeof(s4_path), "S4/%s", pathname + 4);
                    }
                    else if (strncmp(pathname, "~S1", 3) == 0)
                    {
                        // ~S1 -> S4
                        strcpy(s4_path, "S4");
                    }
                    else
                    {
                        // Relative path -> S4/path
                        snprintf(s4_path, sizeof(s4_path), "S4/%s", pathname);
                    }

                    printf("[S1] Converted S4 path: %s\n", s4_path);

                    // Getting ZIP files from S4
                    if (get_files_from_server(s4_socket, s4_path, zip_files, sizeof(zip_files)) == 0)
                    {
                        // Parsing zip_files into array for sorting
                        char zip_files_copy[2048];
                        strcpy(zip_files_copy, zip_files);

                        // Tokenizing ZIP files string
                        char *token = strtok(zip_files_copy, "\n");
                        while (token != NULL && zip_count < 100)
                        {
                            strcpy(zip_list[zip_count], token);
                            zip_count++;
                            token = strtok(NULL, "\n");
                        }
                        // Sorting ZIP files alphabetically
                        sort_file_list(zip_list, zip_count);
                        printf("[S1] Found %d ZIP files\n", zip_count);
                    }
                    // Closing connection to S4
                    close(s4_socket);
                }
                else
                {
                    printf("[S1] ERROR: Failed to connect to S4\n");
                }

                // Consolidating all file lists in order: .c, .pdf, .txt, .zip
                printf("[S1] Consolidating file lists\n");
                printf("[S1] Total files found - C:%d, PDF:%d, TXT:%d, ZIP:%d\n",
                       c_count, pdf_count, txt_count, zip_count);

                // Adding .c files
                for (int i = 0; i < c_count; i++)
                {
                    strcat(consolidated_list, c_list[i]);
                    strcat(consolidated_list, "\n");
                }

                // Adding .pdf files
                for (int i = 0; i < pdf_count; i++)
                {
                    strcat(consolidated_list, pdf_list[i]);
                    strcat(consolidated_list, "\n");
                }

                // Adding .txt files
                for (int i = 0; i < txt_count; i++)
                {
                    strcat(consolidated_list, txt_list[i]);
                    strcat(consolidated_list, "\n");
                }

                // Adding .zip files
                for (int i = 0; i < zip_count; i++)
                {
                    strcat(consolidated_list, zip_list[i]);
                    strcat(consolidated_list, "\n");
                }

                // Sending consolidated list to client
                if (strlen(consolidated_list) > 0)
                {
                    printf("[S1] Sending file list to client\n");
                    send(client_socket, consolidated_list, strlen(consolidated_list), 0);
                }
                else
                {
                    char no_files_msg[] = "No files found in the specified directory\n";
                    printf("[S1] No files found, sending message to client\n");
                    send(client_socket, no_files_msg, strlen(no_files_msg), 0);
                }

                printf("[S1] DISPFNAMES command completed\n");
            }
            else
            {
                // Invalid command format
                printf("[S1] ERROR: Invalid dispfnames command format\n");
                char error_msg[] = "ERROR: Invalid command format. Command: dispfnames pathname";
                send(client_socket, error_msg, strlen(error_msg), 0);
            }
        }

        /*=== TEST COMMAND PROCESSING ===*/
        else if (strncmp(command, "TEST", 4) == 0)
        {
            // Client is testing the connection
            printf("[S1] Processing TEST command\n");

            // Testing connections to other servers
            int s2_sock = connect_to_s2();
            int s3_sock = connect_to_s3();
            int s4_sock = connect_to_s4();

            // Checking if all servers are accessible
            if (s2_sock != -1 && s3_sock != -1 && s4_sock != -1)
            {
                strcpy(response, "S1 OK - All servers connected");
                // Closing the test connections
                close(s2_sock);
                close(s3_sock);
                close(s4_sock);
            }
            else
            {
                strcpy(response, "S1 ERROR - Some servers not available");
            }

            // Sending test response to client
            send(client_socket, response, strlen(response), 0);
            printf("[S1] TEST command completed\n");
        }

        /*=== DOWNLTAR COMMAND PROCESSING ===*/
        // Checking if the command is downltar
        else if (strncmp(command, "downltar", 8) == 0)
        {
            // Printing that we are starting downltar
            printf("[S1] Processing downltar command\n");

            // Declaring variables for parsing
            char cmd[20], filetype[10];
            // Parsing command into two parts (command and filetype)
            int argc = sscanf(command, "%s %s", cmd, filetype);

            // Printing how many arguments we got
            printf("[S1] Parsed %d arguments from downltar command\n", argc);

            // Checking if the format is correct (must be exactly 2 things)
            if (argc != 2)
            {
                // Telling client format is wrong
                send(client_socket, "FORMAT_ERROR: Command: downltar filetype", 38, 0);
                // Printing error info
                printf("[S1] ERROR: Invalid downltar - incorrect arguments\n");
                // Continuing to the next loop
                continue;
            }

            // Printing which filetype we are handling
            printf("[S1] Filetype requested: %s\n", filetype);

            // Validating the allowed types (.c, .pdf, .txt only)
            if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0)
            {
                // Checking if user asked for .zip which is not allowed here
                if (strcmp(filetype, ".zip") == 0)
                {
                    // Informing client that .zip is not supported for tar
                    send(client_socket, "ZIP file not supported for tar operations", 41, 0);
                    // Printing error
                    printf("[S1] ERROR: ZIP files not supported for tar operations\n");
                }
                else
                {
                    // Informing client that type is invalid
                    send(client_socket, "INVALID_TYPE: Only .c, .pdf, .txt supported", 44, 0);
                    // Printing error
                    printf("[S1] ERROR: Invalid filetype: %s\n", filetype);
                }
                // Skipping the rest for invalid type
                continue;
            }

            // Declaring holders for tar file info
            char tar_filename[256];
            char tar_path[MAX_PATH];
            // Setting flag for success or failure
            int tar_success = 0;

            // Handling based on the filetype
            if (strcmp(filetype, ".c") == 0)
            {
                // Saying we are creating tar for C files locally
                printf("[S1] Creating tar file for C files locally\n");

                // Making sure our temp folder exists
                create_full_directories("S1/temp");

                // Choosing a fixed tar name inside S1/temp
                strcpy(tar_filename, "S1/temp/cfiles.tar");

                // Removing old tar if it already exists
                if (access(tar_filename, F_OK) == 0)
                {
                    // Printing that we are cleaning old tar
                    printf("[S1] Removing existing tar file: %s\n", tar_filename);
                    // Deleting the old tar
                    remove(tar_filename);
                }

                // Creating the tar file at the fixed path (inside temp)
                if (create_local_tar_c_files(tar_filename) == 0)
                {
                    // Saving the path to send later
                    strcpy(tar_path, tar_filename);
                    // Marking success
                    tar_success = 1;
                    // Printing confirmation
                    printf("[S1] Successfully created C files tar: %s\n", tar_path);
                }
                else
                {
                    // Printing failure reason
                    printf("[S1] ERROR: Failed to create C files tar\n");
                }
            }
            else if (strcmp(filetype, ".pdf") == 0)
            {
                // Saying we are requesting tar file from S2
                printf("[S1] Requesting PDF tar file from S2\n");

                // Connecting to S2 server
                int s2_socket = connect_to_s2();
                // Checking connection success
                if (s2_socket != -1)
                {
                    // Declaring buffers for commands and responses
                    char createtar_command[1024];
                    char s2_response[1024];

                    // Asking S2 to create its tar under ~/S2
                    snprintf(createtar_command, sizeof(createtar_command), "CREATETAR ~/S2");
                    // Printing what we send
                    printf("[S1] Sending to S2: %s\n", createtar_command);

                    // Sending the CREATETAR request
                    if (send(s2_socket, createtar_command, strlen(createtar_command), 0) != -1)
                    {
                        // Receiving the response path from S2
                        int bytes_received = recv(s2_socket, s2_response, sizeof(s2_response) - 1, 0);
                        // Checking if we got something
                        if (bytes_received > 0)
                        {
                            // Terminating the string
                            s2_response[bytes_received] = '\0';
                            // Printing response
                            printf("[S1] S2 response: %s\n", s2_response);

                            // Checking if S2 returned a path starting with S2/
                            if (strncmp(s2_response, "S2/", 3) == 0)
                            {
                                // Building retrieve command with that path
                                char retrieve_command[1024];
                                snprintf(retrieve_command, sizeof(retrieve_command), "RETRIEVE %s", s2_response);
                                // Printing what we will retrieve
                                printf("[S1] Retrieving tar file: %s\n", retrieve_command);

                                // Sending retrieve request
                                if (send(s2_socket, retrieve_command, strlen(retrieve_command), 0) != -1)
                                {
                                    // Naming the local tar filename
                                    strcpy(tar_filename, "pdffiles.tar");
                                    // Receiving the tar into S1/temp
                                    if (receive_file_from_S1(s2_socket, "pdffiles.tar", "S1/temp") == 0)
                                    {
                                        // Building full path to the tar in temp
                                        snprintf(tar_path, sizeof(tar_path), "S1/temp/%s", tar_filename);
                                        // Marking success
                                        tar_success = 1;
                                        // Printing success
                                        printf("[S1] Successfully received PDF tar file from S2\n");
                                    }
                                    else
                                    {
                                        // Printing error if receive failed
                                        printf("[S1] ERROR: Failed to receive PDF tar file from S2\n");
                                    }
                                }
                                else
                                {
                                    // Printing error if retrieve command failed to send
                                    printf("[S1] ERROR: Failed to send RETRIEVE command to S2\n");
                                }
                            }
                            // Checking if S2 said it failed to create
                            else if (strstr(s2_response, "TAR_ERROR") != NULL)
                            {
                                // Printing that S2 failed
                                printf("[S1] ERROR: S2 failed to create PDF tar file\n");
                            }
                            else
                            {
                                // Printing unexpected response
                                printf("[S1] ERROR: Unexpected response from S2: %s\n", s2_response);
                            }
                        }
                        else
                        {
                            // Printing error when no response came
                            printf("[S1] ERROR: No response from S2 for CREATETAR\n");
                        }
                    }
                    else
                    {
                        // Printing send failure
                        printf("[S1] ERROR: Failed to send CREATETAR command to S2\n");
                    }

                    // Closing connection to S2
                    close(s2_socket);
                }
                else
                {
                    // Printing that we could not connect to S2
                    printf("[S1] ERROR: Cannot connect to S2 server\n");
                }
            }
            else if (strcmp(filetype, ".txt") == 0)
            {
                // Saying we are requesting tar file from S3
                printf("[S1] Requesting TXT tar file from S3\n");

                // Connecting to S3 server
                int s3_socket = connect_to_s3();
                // Checking connection success
                if (s3_socket != -1)
                {
                    // Declaring buffers for commands and responses
                    char createtar_command[1024];
                    char s3_response[1024];

                    // Asking S3 to create its tar under ~/S3
                    snprintf(createtar_command, sizeof(createtar_command), "CREATETAR ~/S3");
                    // Printing what we send
                    printf("[S1] Sending to S3: %s\n", createtar_command);

                    // Sending the CREATETAR request
                    if (send(s3_socket, createtar_command, strlen(createtar_command), 0) != -1)
                    {
                        // Receiving the response path from S3
                        int bytes_received = recv(s3_socket, s3_response, sizeof(s3_response) - 1, 0);
                        // Checking if we got something
                        if (bytes_received > 0)
                        {
                            // Terminating the string
                            s3_response[bytes_received] = '\0';
                            // Printing response
                            printf("[S1] S3 response: %s\n", s3_response);

                            // Checking if S3 returned a path starting with S3/
                            if (strncmp(s3_response, "S3/", 3) == 0)
                            {
                                // Building retrieve command with that path
                                char retrieve_command[1024];
                                snprintf(retrieve_command, sizeof(retrieve_command), "RETRIEVE %s", s3_response);
                                // Printing what we will retrieve
                                printf("[S1] Retrieving tar file: %s\n", retrieve_command);

                                // Sending retrieve request
                                if (send(s3_socket, retrieve_command, strlen(retrieve_command), 0) != -1)
                                {
                                    // Naming the local tar filename
                                    strcpy(tar_filename, "txtfiles.tar");
                                    // Receiving the tar into S1/temp
                                    if (receive_file_from_S1(s3_socket, "txtfiles.tar", "S1/temp") == 0)
                                    {
                                        // Building full path to the tar in temp
                                        snprintf(tar_path, sizeof(tar_path), "S1/temp/%s", tar_filename);
                                        // Marking success
                                        tar_success = 1;
                                        // Printing success
                                        printf("[S1] Successfully received TXT tar file from S3\n");
                                    }
                                    else
                                    {
                                        // Printing error if receive failed
                                        printf("[S1] ERROR: Failed to receive TXT tar file from S3\n");
                                    }
                                }
                                else
                                {
                                    // Printing error if retrieve command failed to send
                                    printf("[S1] ERROR: Failed to send RETRIEVE command to S3\n");
                                }
                            }
                            // Checking if S3 said it failed to create
                            else if (strstr(s3_response, "TAR_ERROR") != NULL)
                            {
                                // Printing that S3 failed
                                printf("[S1] ERROR: S3 failed to create TXT tar file\n");
                            }
                            else
                            {
                                // Printing unexpected response
                                printf("[S1] ERROR: Unexpected response from S3: %s\n", s3_response);
                            }
                        }
                        else
                        {
                            // Printing error when no response came
                            printf("[S1] ERROR: No response from S3 for CREATETAR\n");
                        }
                    }
                    else
                    {
                        // Printing send failure
                        printf("[S1] ERROR: Failed to send CREATETAR command to S3\n");
                    }

                    // Closing connection to S3
                    close(s3_socket);
                }
                else
                {
                    // Printing that we could not connect to S3
                    printf("[S1] ERROR: Cannot connect to S3 server\n");
                }
            }

            // Checking if a tar is ready to send
            if (tar_success)
            {
                // Informing client that tar is ready
                printf("[S1] Sending TAR_READY response to client\n");
                send(client_socket, "TAR_READY", 9, 0);

                // Waiting a short time so client can switch to receive mode
                usleep(500000); // 500 ms

                // Printing which tar we are sending
                printf("[S1] Sending tar file to client: %s\n", tar_path);
                // Sending the tar file
                if (send_tar_file_to_client(client_socket, tar_path) == 0)
                {
                    // Printing success
                    printf("[S1] Tar file sent successfully to client\n");

                    // Trying to delete local tar after sending
                    if (remove(tar_path) == 0)
                    {
                        // Printing cleanup success
                        printf("[S1] Cleaned up tar file: %s\n", tar_path);
                    }
                    else
                    {
                        // Printing cleanup warning
                        printf("[S1] Warning: Failed to clean up tar file: %s\n", tar_path);
                    }
                }
                else
                {
                    // Printing error if sending failed
                    printf("[S1] ERROR: Failed to send tar file to client\n");
                }
            }
            else
            {
                // Informing client that tar creation failed
                printf("[S1] ERROR: Tar file creation failed\n");
                send(client_socket, "TAR_ERROR: Failed to create tar file", 36, 0);
            }

            // Printing that we finished this command
            printf("[S1] DOWNLTAR command processing complete\n");
        }

        /*=== UNKNOWN COMMAND HANDLING ===*/
        else
        {
            // Unknown command received from client
            printf("[S1] ERROR: Unknown command from client: %s\n", command);
            strcpy(response, "ERROR: Unknown command");
            send(client_socket, response, strlen(response), 0);
        }
    }

    // Closing client connection
    close(client_socket);
    printf("[S1] Client connection closed\n");
}

/*=== MAIN FUNCTION ===*/

// Main function - this is where S1 server starts
int main()
{
    // Printing startup banner
    printf("S1 - This is server 1 -S1\n");
    printf("Starting on port %d\n", S1_PORT);

    // Initializing all server directories
    printf("[S1] Initializing server directories\n");
    initialize_server_directories();

    // Creating a socket for S1 to listen for client connections
    printf("[S1] Creating server socket\n");
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        printf("[S1] ERROR: Failed to create server socket\n");
        // Exiting with error code
        exit(1);
    }

    // Setting socket option to reuse address
    printf("[S1] Setting socket options\n");
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        printf("[S1] WARNING: Failed to set socket options\n");
    }

    // Creating address structure for S1 server
    printf("[S1] Configuring server address\n");
    struct sockaddr_in server_addr = {0};
    // Setting the protocol family to IPv4
    server_addr.sin_family = AF_INET;
    // Setting to accept connections from any IP address
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Converting port number to network format and assigning
    server_addr.sin_port = htons(S1_PORT);

    // Binding the socket to the address
    printf("[S1] Binding socket to address\n");
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        printf("[S1] ERROR: Failed to bind socket to address\n");
        printf("[S1] Make sure port %d is not already in use\n", S1_PORT);
        // Closing socket before exit
        close(server_socket);
        exit(1);
    }

    // Starting to listen for client connections
    // The parameter 5 means we can have up to 5 pending connections in queue
    printf("[S1] Starting to listen for connections\n");
    if (listen(server_socket, 5) == -1)
    {
        printf("[S1] ERROR: Failed to listen on socket\n");
        // Closing socket before exit
        close(server_socket);
        exit(1);
    }

    printf("\n[S1] Server is ready and waiting for clients\n");
    printf("[S1] Listening on port %d\n", S1_PORT);
    printf("[S1] Press Ctrl+C to stop server\n\n");

    // Main server loop - keep accepting client connections forever
    while (1)
    {
        printf("[S1] Waiting for client connection\n");

        // Waiting for a client to connect and accepting the connection
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1)
        {
            printf("[S1] ERROR: Failed to accept client connection\n");
            // Continue to next iteration to try again
            continue;
        }

        printf("[S1] New client connected successfully\n");

        // Creating a child process to handle this client
        // fork() creates an exact copy of the current process
        printf("[S1] Creating child process to handle client\n");
        pid_t child_pid = fork();

        if (child_pid == 0)
        {
            // This code runs in the child process
            printf("[S1] Child process started\n");

            // Child doesn't need the server socket, so closing it
            close(server_socket);

            // Processing client requests in child process
            prcclient(client_socket);

            printf("[S1] Child process ending\n");
            // Child process exits when client handling is complete
            exit(0);
        }
        else if (child_pid > 0)
        {
            // This code runs in the parent process
            printf("[S1] Created child process with PID: %d\n", child_pid);

            // Parent doesn't need the client socket, so closing it
            close(client_socket);

            // Parent continues to listen for more clients
            printf("[S1] Parent process continues listening\n");
        }
        else
        {
            // fork() failed
            printf("[S1] ERROR: Failed to create child process\n");
            printf("[S1] Closing client connection\n");
            // Closing client socket and continuing
            close(client_socket);
        }
    }

    // This code never executes because of the infinite loop above
    // But included for completeness
    printf("[S1] Server shutting down\n");
    close(server_socket);
    return 0;
}
