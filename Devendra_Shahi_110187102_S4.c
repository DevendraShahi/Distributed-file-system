#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>

// Port number for S4 ZIP server
#define PORT 4304
#define MAX_PATH 1024
#define BUFFER_SIZE 4096

/*=== DIRECTORY MANAGEMENT FUNCTIONS ===*/

// Creating full directory path recursively
int create_full_directories(char *full_path)
{
    // Creating temporary path for building step by step
    char temp_path[MAX_PATH];
    // Creating pointer for splitting path
    char *token;
    // Creating copy of original path
    char path_copy[MAX_PATH];

    printf("[S4] Creating directory structure: %s\n", full_path);

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

        printf("[S4] Creating directory: %s\n", temp_path);

        // Attempting to create directory
        if (mkdir(temp_path, 0755) == -1)
        {
            // Checking if error is directory already exists
            if (errno != EEXIST)
            {
                printf("[S4] ERROR: Failed to create directory: %s\n", temp_path);
                return -1;
            }
            else
            {
                printf("[S4] Directory already exists: %s\n", temp_path);
            }
        }
        else
        {
            printf("[S4] Successfully created: %s\n", temp_path);
        }

        // Getting next path component
        token = strtok(NULL, "/");
    }

    printf("[S4] All directories created successfully\n");
    return 0;
}

/*=== FILE LISTING FUNCTIONS ===*/

// Sending file list to S1 server
int send_filelist_to_S1(int s1_socket, const char *directory_path, const char *file_extension)
{
    // Creating directory pointer
    DIR *dir;
    // Creating directory entry structure
    struct dirent *entry;
    // Creating buffer to collect all file names
    char file_list[4096] = "";
    // Creating temporary buffer for each file name
    char temp_name[256];

    printf("[S4] Collecting file list from: %s\n", directory_path);

    // Opening directory
    dir = opendir(directory_path);
    if (dir == NULL)
    {
        printf("[S4] Cannot open directory: %s\n", directory_path);
        // Sending "No files" message to S1
        send(s1_socket, "No files", 8, 0);
        return -1;
    }

    // Reading directory entries and collecting filenames
    while ((entry = readdir(dir)) != NULL)
    {
        // Skipping current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Filtering by extension if specified
        if (file_extension != NULL && strstr(entry->d_name, file_extension) == NULL)
        {
            continue;
        }

        // Formatting filename with newline
        snprintf(temp_name, sizeof(temp_name), "%s\n", entry->d_name);
        // Adding filename to list
        strcat(file_list, temp_name);
    }

    // Closing directory
    closedir(dir);

    // Sending file list to S1
    if (strlen(file_list) == 0)
    {
        // Sending "No files" if list is empty
        send(s1_socket, "No files", 8, 0);
    }
    else
    {
        // Sending the complete file list
        send(s1_socket, file_list, strlen(file_list), 0);
        printf("[S4] File list sent to S1\n");
    }

    return 0;
}

/*=== FILE DELETION FUNCTIONS ===*/

// Deleting file from filesystem
int delete_file(const char *full_path)
{
    printf("[S4] Attempting to delete file: %s\n", full_path);

    // Checking if file exists
    FILE *file = fopen(full_path, "r");
    if (file == NULL)
    {
        printf("[S4] ERROR: File not found: %s\n", full_path);
        return -1;
    }
    // Closing file handle
    fclose(file);

    // Removing the file
    if (remove(full_path) == 0)
    {
        printf("[S4] File deleted successfully: %s\n", full_path);
        return 0;
    }
    else
    {
        printf("[S4] ERROR: Failed to delete file: %s\n", full_path);
        return -1;
    }
}

/*=== FILE TRANSFER FUNCTIONS ===*/

// Sending file to S1 server
int send_file_to_S1(int s1_socket, const char *full_path)
{
    // Creating buffer for reading file data chunks
    char buffer[BUFFER_SIZE];
    // Creating file pointer for reading
    FILE *file;
    // Storing file size
    long file_size;
    // Tracking total bytes sent
    long total_sent = 0;

    printf("[S4] Preparing to send file: %s\n", full_path);

    // Opening file for reading in binary mode
    file = fopen(full_path, "rb");
    if (file == NULL)
    {
        printf("[S4] ERROR: File not found: %s\n", full_path);
        return -1;
    }

    // Moving to end of file to get size
    fseek(file, 0, SEEK_END);
    // Getting current position (file size)
    file_size = ftell(file);
    // Moving back to beginning
    fseek(file, 0, SEEK_SET);

    printf("[S4] File size: %ld bytes\n", file_size);

    // Sending file size to S1 first
    if (send(s1_socket, &file_size, sizeof(file_size), 0) == -1)
    {
        printf("[S4] ERROR: Failed to send file size to S1\n");
        fclose(file);
        return -1;
    }
    printf("[S4] File size sent to S1\n");

    printf("[S4] Starting file transfer\n");
    // Sending file data in chunks
    while (total_sent < file_size)
    {
        // Reading data chunk from file
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read <= 0)
        {
            printf("[S4] ERROR: Failed to read from file\n");
            fclose(file);
            return -1;
        }

        // Sending chunk to S1
        int bytes_sent = send(s1_socket, buffer, bytes_read, 0);
        if (bytes_sent <= 0)
        {
            printf("[S4] ERROR: Failed to send file data to S1\n");
            fclose(file);
            return -1;
        }

        // Updating total bytes sent
        total_sent += bytes_sent;
    }

    // Closing file
    fclose(file);
    printf("[S4] File sent successfully\n");
    return 0;
}

// Receiving file from S1 server
int receive_file_from_S1(int s1_socket, const char *filename, const char *filepath)
{
    // Creating complete path including filename
    char full_path[MAX_PATH];
    // Creating buffer for reading file data chunks
    char buffer[BUFFER_SIZE];
    // Creating file pointer for writing
    FILE *file;
    // Storing incoming file size
    long file_size;
    // Tracking total bytes received
    long total_received = 0;

    printf("[S4] Preparing to receive file: %s\n", filename);

    // Receiving file size from S1
    if (recv(s1_socket, &file_size, sizeof(file_size), 0) <= 0)
    {
        printf("[S4] ERROR: Failed to receive file size\n");
        return -1;
    }
    printf("[S4] File size: %ld bytes\n", file_size);

    // Creating mutable copy of filepath for directory creation
    char filepath_copy[MAX_PATH];
    strcpy(filepath_copy, filepath);

    // Creating the directory structure if it doesn't exist
    if (create_full_directories(filepath_copy) == -1)
    {
        printf("[S4] ERROR: Failed to create directory structure\n");
        return -1;
    }

    // Building complete path for file storage
    snprintf(full_path, sizeof(full_path), "%s/%s", filepath, filename);
    printf("[S4] Saving file to: %s\n", full_path);

    // Opening file for writing in binary mode
    file = fopen(full_path, "wb");
    if (file == NULL)
    {
        printf("[S4] ERROR: Failed to create file %s\n", full_path);
        return -1;
    }

    printf("[S4] Starting file transfer\n");
    // Receiving file data in chunks
    while (total_received < file_size)
    {
        // Receiving data chunk from S1
        int bytes_received = recv(s1_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            printf("[S4] ERROR: Failed to receive file data\n");
            fclose(file);
            return -1;
        }

        // Writing received data to file
        int bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received)
        {
            printf("[S4] ERROR: Failed to write data to file\n");
            fclose(file);
            return -1;
        }

        // Updating total received
        total_received += bytes_received;
    }

    // Closing file
    fclose(file);
    printf("[S4] File received successfully: %s\n", full_path);
    return 0;
}

/*=== MAIN FUNCTION ===*/

// Main function - S4 ZIP server entry point
int main()
{
    // Printing startup message
    printf("\n========================================\n");
    printf("S4 - ZIP File Server\n");
    printf("Starting on port %d\n", PORT);
    printf("========================================\n");

    // Creating socket for S4 server
    printf("[S4] Creating server socket\n");
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        printf("[S4] ERROR: Failed to create socket\n");
        exit(1);
    }

    // Setting socket option to reuse address
    printf("[S4] Setting socket options\n");
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        printf("[S4] WARNING: Failed to set socket options\n");
    }

    // Creating address structure for S4
    printf("[S4] Configuring server address\n");
    struct sockaddr_in addr = {0};
    // Setting protocol family to IPv4
    addr.sin_family = AF_INET;
    // Setting to accept connections from any IP address
    addr.sin_addr.s_addr = INADDR_ANY;
    // Converting port number to network format
    addr.sin_port = htons(PORT);

    // Binding socket to address
    printf("[S4] Binding socket to address\n");
    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("[S4] ERROR: Failed to bind socket\n");
        close(server_socket);
        exit(1);
    }

    // Starting to listen for S1 connections
    printf("[S4] Starting to listen for connections\n");
    if (listen(server_socket, 1) == -1)
    {
        printf("[S4] ERROR: Failed to listen on socket\n");
        close(server_socket);
        exit(1);
    }

    printf("\n[S4] Server is ready and waiting for S1 connections\n");
    printf("[S4] Listening on port %d\n", PORT);

    // Main server loop - keep accepting S1 connections
    while (1)
    {
        printf("[S4] Waiting for S1 connection\n");

        // Accepting connection from S1
        int s1_socket = accept(server_socket, NULL, NULL);
        if (s1_socket == -1)
        {
            printf("[S4] ERROR: Failed to accept connection\n");
            continue;
        }

        printf("[S4] S1 connected successfully\n");

        // Creating buffer to store commands from S1
        char command[1024];

        // Inner loop - keep processing commands from S1
        while (1)
        {
            // Clearing command buffer
            memset(command, 0, sizeof(command));

            // Receiving command from S1
            int bytes = recv(s1_socket, command, sizeof(command) - 1, 0);

            // Checking if S1 disconnected
            if (bytes <= 0)
            {
                printf("[S4] S1 disconnected\n");
                break;
            }

            // Adding null terminator to command
            command[bytes] = '\0';
            printf("\n[S4] Processing command: %s\n", command);

            /*=== STORE COMMAND PROCESSING ===*/
            if (strncmp(command, "STORE", 5) == 0)
            {
                // Declaring variables for filename and filepath
                char filename[256], filepath[MAX_PATH];

                // Parsing command to extract filename and filepath
                if (sscanf(command, "STORE %s %s", filename, filepath) == 2)
                {
                    printf("[S4] Storing %s in %s\n", filename, filepath);

                    // Sending ready status to S1
                    send(s1_socket, "READY", 5, 0);

                    // Receiving file from S1
                    if (receive_file_from_S1(s1_socket, filename, filepath) == 0)
                    {
                        // Sending success status to S1
                        send(s1_socket, "SUCCESS", 7, 0);
                        printf("[S4] File stored successfully\n");
                    }
                    else
                    {
                        // Sending error status to S1
                        send(s1_socket, "ERROR", 5, 0);
                        printf("[S4] ERROR: Failed to store file\n");
                    }
                }
                else
                {
                    printf("[S4] ERROR: Invalid STORE command format\n");
                    // Sending format error status to S1
                    send(s1_socket, "FORMAT ERROR", 12, 0);
                }
            }

            /*=== RETRIEVE COMMAND PROCESSING ===*/
            else if (strncmp(command, "RETRIEVE", 8) == 0)
            {
                // Declaring variable for filepath
                char filepath[MAX_PATH];

                // Parsing command to extract filepath
                if (sscanf(command, "RETRIEVE %s", filepath) == 1)
                {
                    printf("[S4] Retrieving from filepath: %s\n", filepath);

                    // Sending file to S1
                    if (send_file_to_S1(s1_socket, filepath) == 0)
                    {
                        printf("[S4] File sent successfully\n");
                    }
                    else
                    {
                        // Sending error status to S1
                        send(s1_socket, "ERROR", 5, 0);
                        printf("[S4] ERROR: Failed to send file\n");
                    }
                }
                else
                {
                    printf("[S4] ERROR: Invalid RETRIEVE command format\n");
                    // Sending format error status to S1
                    send(s1_socket, "FORMAT ERROR", 12, 0);
                }
            }

            /*=== DELETE COMMAND PROCESSING ===*/
            else if (strncmp(command, "DELETE", 6) == 0)
            {
                // Declaring variable for filepath
                char filepath[MAX_PATH];

                // Parsing command to extract filepath
                if (sscanf(command, "DELETE %s", filepath) == 1)
                {
                    printf("[S4] Attempting to delete file: %s\n", filepath);

                    // Deleting file
                    if (delete_file(filepath) == 0)
                    {
                        // Sending success status to S1
                        send(s1_socket, "SUCCESS", 7, 0);
                        printf("[S4] File deleted successfully\n");
                    }
                    else
                    {
                        // Sending error status to S1
                        send(s1_socket, "ERROR", 5, 0);
                        printf("[S4] ERROR: Unable to delete file\n");
                    }
                }
                else
                {
                    printf("[S4] ERROR: Invalid DELETE command format\n");
                    // Sending format error status to S1
                    send(s1_socket, "FORMAT ERROR", 12, 0);
                }
            }

            /*=== LIST COMMAND PROCESSING ===*/
            else if (strncmp(command, "LIST", 4) == 0)
            {
                // Declaring variable for directory path
                char directory_path[MAX_PATH];

                // Parsing command to extract directory path
                if (sscanf(command, "LIST %s", directory_path) == 1)
                {
                    printf("[S4] Preparing list for directory path: %s\n", directory_path);

                    // Sending file list to S1
                    if (send_filelist_to_S1(s1_socket, directory_path, ".zip") == 0)
                    {
                        printf("[S4] List of files sent successfully\n");
                    }
                    else
                    {
                        printf("[S4] ERROR: Failed to send list of files\n");
                    }
                }
                else
                {
                    printf("[S4] ERROR: Invalid LIST command format\n");
                    // Sending format error status to S1
                    send(s1_socket, "FORMAT ERROR", 12, 0);
                }
            }

            /*=== TEST COMMAND PROCESSING ===*/
            else if (strncmp(command, "TEST", 4) == 0)
            {
                printf("[S4] Received test command\n");
                // Sending working response to S1
                send(s1_socket, "S4_OK", 5, 0);
                printf("[S4] Sent OK response to S1\n");
            }

            /*=== UNKNOWN COMMAND HANDLING ===*/
            else
            {
                printf("[S4] WARNING: Unknown command received: %s\n", command);
                // Sending generic OK response
                send(s1_socket, "OK", 2, 0);
            }
        }

        // Closing connection to S1
        close(s1_socket);
        printf("[S4] Connection to S1 closed\n");
    }

    // This code never executes due to infinite loop
    printf("[S4] Server shutting down\n");
    close(server_socket);
    return 0;
}