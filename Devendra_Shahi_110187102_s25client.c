#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

// Server connection details
#define S1_PORT 4301
#define BUFFER_SIZE 4096
#define MAX_PATH 1024

/*=== HELPER FUNCTIONS ===*/

// Displaying help information for available commands
void display_help()
{
    printf("\n========== S25 Distributed File System - Available Commands ==========\n\n");

    printf("UPLOADF - Upload files to server\n");
    printf("  Command: uploadf file1 [file2] [file3] destination_path\n");

    printf("DOWNLF - Download files from server\n");
    printf("  Command: downlf filepath1 [filepath2]\n");

    printf("REMOVEF - Delete files from server\n");
    printf("  Command: removef filepath1 [filepath2]\n");

    printf("DOWNLTAR - Download tar archive of specific file type\n");
    printf("  Command: downltar filetype\n");

    printf("DISPFNAMES - Display files in directory\n");
    printf("  Command: dispfnames pathname\n");

    printf("TEST - Test server connectivity\n");
    printf("  Command: TEST\n");
    printf("  - Test connection to server\n\n");

    printf("HELP - Display this help message\n");
    printf("  Command: help\n\n");

    printf("QUIT - Exit the client\n");
    printf("  Command: quit\n\n");

    printf("======================================================================\n");
}

/*=== FILE TRANSFER FUNCTIONS ===*/

// Sending file to S1 server
int send_file_to_server(int s1_socket, const char *filename)
{
    // Creating file pointer for reading
    FILE *file;
    // Creating buffer for file data
    char buffer[BUFFER_SIZE];
    // Storing file size
    long file_size;
    // Tracking total bytes sent
    long total_sent = 0;

    printf("[CLIENT] Sending file: %s\n", filename);

    // Opening file for reading in binary mode
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        printf("[CLIENT] ERROR: Cannot open file %s\n", filename);
        return -1;
    }

    // Moving to end of file to get size
    fseek(file, 0, SEEK_END);
    // Getting current position (file size)
    file_size = ftell(file);
    // Moving back to beginning
    fseek(file, 0, SEEK_SET);

    printf("[CLIENT] File size: %ld bytes\n", file_size);

    // Sending file size first to server
    if (send(s1_socket, &file_size, sizeof(file_size), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send file size\n");
        fclose(file);
        return -1;
    }

    printf("[CLIENT] File size sent successfully\n");

    // Adding small delay to ensure file size is processed
    usleep(50000); // 50ms

    // Sending file data in chunks
    while (total_sent < file_size)
    {
        // Reading chunk from file
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (bytes_read <= 0)
        {
            printf("[CLIENT] ERROR: Error reading file\n");
            fclose(file);
            return -1;
        }

        // Sending chunk to server
        int bytes_sent = send(s1_socket, buffer, bytes_read, 0);
        if (bytes_sent <= 0)
        {
            printf("[CLIENT] ERROR: Error sending file data\n");
            fclose(file);
            return -1;
        }

        // Updating total sent
        total_sent += bytes_sent;

        // Showing progress for larger files
        if (file_size > 10000)
        {
            printf("[CLIENT] Sent %ld/%ld bytes (%.1f%%)\r", total_sent, file_size,
                   (total_sent * 100.0) / file_size);
            fflush(stdout);
        }
    }

    // Closing file
    fclose(file);

    if (file_size > 10000)
    {
        // New line after progress display
        printf("\n");
    }

    printf("[CLIENT] File %s sent successfully\n", filename);
    return 0;
}

// Receiving file from S1 server
// Fixed receive_file_from_server function
int receive_file_from_server(int s1_socket, const char *filename)
{
    // Creating file pointer for writing
    FILE *file;
    // Creating buffer for file data
    char buffer[BUFFER_SIZE];
    // Storing file size
    long file_size;
    // Tracking total bytes received
    long total_received = 0;

    printf("[CLIENT] Receiving file: %s\n", filename);

    // Receiving file size first from server
    if (recv(s1_socket, &file_size, sizeof(file_size), 0) <= 0)
    {
        printf("[CLIENT] ERROR: Failed to receive file size\n");
        return -1;
    }

    printf("[CLIENT] File size: %ld bytes\n", file_size);

    // Validate file size
    if (file_size <= 0)
    {
        printf("[CLIENT] ERROR: Invalid file size: %ld\n", file_size);
        return -1;
    }

    // Opening file for writing in binary mode
    file = fopen(filename, "wb");
    if (file == NULL)
    {
        printf("[CLIENT] ERROR: Cannot create file %s\n", filename);
        perror("[CLIENT] fopen error");
        return -1;
    }

    printf("[CLIENT] Successfully opened file for writing: %s\n", filename);

    // Receiving file data in chunks
    while (total_received < file_size)
    {
        // Calculate how many bytes to request this time
        long remaining = file_size - total_received;
        int bytes_to_receive = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : (int)remaining;

        // Receiving data chunk from server
        int bytes_received = recv(s1_socket, buffer, bytes_to_receive, 0);
        if (bytes_received <= 0)
        {
            printf("[CLIENT] ERROR: Error receiving file data (received %d bytes)\n", bytes_received);
            fclose(file);
            // Remove partially created file
            remove(filename);
            return -1;
        }

        // Writing received data to file
        int bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written != bytes_received)
        {
            printf("[CLIENT] ERROR: Error writing to file (wrote %d, expected %d)\n",
                   bytes_written, bytes_received);
            fclose(file);
            // Remove partially created file
            remove(filename);
            return -1;
        }

        // Updating total received
        total_received += bytes_received;

        // Showing progress for larger files
        if (file_size > 10000)
        {
            printf("[CLIENT] Received %ld/%ld bytes (%.1f%%)\r", total_received, file_size,
                   (total_received * 100.0) / file_size);
            fflush(stdout);
        }
    }

    // Closing file
    fclose(file);

    if (file_size > 10000)
    {
        printf("\n"); // New line after progress display
    }

    // Verify the file was actually created and has correct size
    FILE *verify = fopen(filename, "rb");
    if (verify == NULL)
    {
        printf("[CLIENT] ERROR: File was not created: %s\n", filename);
        return -1;
    }

    // Check file size
    fseek(verify, 0, SEEK_END);
    long actual_size = ftell(verify);
    fclose(verify);

    if (actual_size != file_size)
    {
        printf("[CLIENT] ERROR: File size mismatch (expected %ld, got %ld)\n",
               file_size, actual_size);
        remove(filename);
        return -1;
    }

    printf("[CLIENT] File %s received successfully (%ld bytes)\n", filename, actual_size);
    return 0;
}

/*=== DOWNLTAR COMMAND HANDLER ===*/
int handle_downltar(int s1_socket, char *command)
{
    // Creating variables for command parsing
    char filetype[10];
    // Creating response buffer
    char response[1024];
    // Creating tar filename buffer
    char tar_filename[256];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing downltar command\n");

    // Parsing command to extract filetype
    if (sscanf(command, "downltar %s", filetype) != 1)
    {
        printf("[CLIENT] ERROR: Invalid downltar format\n");
        printf("[CLIENT] Command: downltar filetype\n");
        return -1;
    }

    printf("[CLIENT] Requesting tar file for filetype: %s\n", filetype);

    // Validate filetype
    if (strcmp(filetype, ".c") != 0 && strcmp(filetype, ".pdf") != 0 && strcmp(filetype, ".txt") != 0)
    {
        printf("[CLIENT] ERROR: Invalid filetype. Supported: .c, .pdf, .txt\n");
        return -1;
    }

    // Sending command to server
    printf("[CLIENT] Sending command to server\n");
    if (send(s1_socket, command, strlen(command), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send command\n");
        return -1;
    }

    // Waiting for response from server
    printf("[CLIENT] Waiting for server response\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes <= 0)
    {
        printf("[CLIENT] ERROR: No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes] = '\0';

    printf("[CLIENT] Server response: %s\n", response);

    // Checking server response
    if (strncmp(response, "TAR_READY", 9) == 0)
    {
        printf("[CLIENT] Server created tar file, downloading\n");

        // Creating filename for the tar file based on filetype
        if (strcmp(filetype, ".c") == 0)
        {
            strcpy(tar_filename, "cfiles.tar");
        }
        else if (strcmp(filetype, ".pdf") == 0)
        {
            strcpy(tar_filename, "pdffiles.tar");
        }
        else if (strcmp(filetype, ".txt") == 0)
        {
            strcpy(tar_filename, "txtfiles.tar");
        }
        else
        {
            strcpy(tar_filename, "files.tar");
        }

        printf("[CLIENT] Downloading tar file as: %s\n", tar_filename);

        // Check if file already exists and remove it
        if (access(tar_filename, F_OK) == 0)
        {
            printf("[CLIENT] Removing existing file: %s\n", tar_filename);
            if (remove(tar_filename) != 0)
            {
                printf("[CLIENT] WARNING: Failed to remove existing file\n");
            }
        }

        // Receiving the tar file using existing function
        if (receive_file_from_server(s1_socket, tar_filename) == 0)
        {
            // Verify the file actually exists after download
            if (access(tar_filename, F_OK) == 0)
            {
                struct stat st;
                if (stat(tar_filename, &st) == 0)
                {
                    printf("[CLIENT] Tar file downloaded successfully: %s (%lld bytes)\n",
                           tar_filename, (long long)st.st_size);

                    // Additional verification - check if it's a valid tar file
                    if (st.st_size > 0)
                    {
                        printf("[CLIENT] File appears to be valid (non-zero size)\n");
                        return 0;
                    }
                    else
                    {
                        printf("[CLIENT] WARNING: Downloaded file is empty\n");
                        remove(tar_filename);
                        return -1;
                    }
                }
                else
                {
                    printf("[CLIENT] ERROR: Cannot get file statistics\n");
                    return -1;
                }
            }
            else
            {
                printf("[CLIENT] ERROR: File was not created on disk: %s\n", tar_filename);
                return -1;
            }
        }
        else
        {
            printf("[CLIENT] ERROR: Failed to download tar file\n");
            return -1;
        }
    }
    else if (strstr(response, "TAR_ERROR") != NULL)
    {
        printf("[CLIENT] ERROR: Server failed to create tar file: %s\n", response);
        return -1;
    }
    else if (strstr(response, "INVALID_TYPE") != NULL)
    {
        printf("[CLIENT] ERROR: Invalid file type: %s\n", filetype);
        return -1;
    }
    else if (strstr(response, "FORMAT_ERROR") != NULL)
    {
        printf("[CLIENT] ERROR: Invalid command format\n");
        return -1;
    }
    else if (strstr(response, "ZIP file not supported") != NULL)
    {
        printf("[CLIENT] ERROR: ZIP files are not supported for tar operations\n");
        return -1;
    }
    else
    {
        printf("[CLIENT] ERROR: Unexpected server response: %s\n", response);
        return -1;
    }
}

/*=== UPLOADF COMMAND HANDLER ===*/

// Handling uploadf command
int handle_uploadf(int s1_socket, char *command)
{
    // Creating variables for command parsing
    char cmd[20], file1[256], file2[256], file3[256], dest_path[512];
    // Creating response buffer
    char response[1024];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing uploadf command\n");

    // Parsing command to extract files and destination
    int argc = sscanf(command, "%s %s %s %s %s", cmd, file1, file2, file3, dest_path);

    // Validating command format
    if (argc < 3)
    {
        printf("[CLIENT] ERROR: Invalid uploadf format: Require at least 3 arguments.\n");
        printf("[CLIENT] Command: uploadf file1 [file2] [file3] dest_path\n");
        return -1;
    }

    if (argc > 5)
    {
        printf("[CLIENT] ERROR: Invalid uploadf format: More than 5 arguments.\n");
        printf("[CLIENT] Command: uploadf file1 [file2] [file3] dest_path\n");
        return -1;
    }

    // Calculating number of files to upload
    int file_count = argc - 2; // Subtract command and dest_path
    printf("[CLIENT] Uploading %d files to %s\n", file_count, dest_path);

    // Displaying files to be uploaded
    printf("[CLIENT] Files to upload:\n");
    if (file_count >= 1)
        printf("[CLIENT]   1. %s\n", file1);
    if (file_count >= 2)
        printf("[CLIENT]   2. %s\n", file2);
    if (file_count >= 3)
        printf("[CLIENT]   3. %s\n", file3);

    // Sending command to server
    printf("[CLIENT] Sending command to server\n");
    if (send(s1_socket, command, strlen(command), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send command\n");
        return -1;
    }

    // Waiting for READY response from server
    printf("[CLIENT] Waiting for server response\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes <= 0)
    {
        printf("[CLIENT] ERROR: No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes] = '\0';

    // Checking if server is ready
    if (strncmp(response, "READY", 5) != 0)
    {
        printf("[CLIENT] ERROR: Server response: %s\n", response);
        return -1;
    }

    printf("[CLIENT] Server ready to receive files\n");

    // Creating array of filenames for easier processing
    char *filenames[3] = {file1, file2, file3};

    // Sending each file with delays between transfers
    for (int i = 0; i < file_count; i++)
    {
        printf("\n[CLIENT] === Sending file %d/%d: %s ===\n", i + 1, file_count, filenames[i]);

        // Checking if file exists before sending
        FILE *check_file = fopen(filenames[i], "r");
        if (check_file == NULL)
        {
            printf("[CLIENT] ERROR: File not found: %s\n", filenames[i]);
            return -1;
        }
        fclose(check_file);

        // Sending file to server
        if (send_file_to_server(s1_socket, filenames[i]) != 0)
        {
            printf("[CLIENT] ERROR: Failed to send file: %s\n", filenames[i]);
            return -1;
        }

        printf("[CLIENT] File %s sent successfully\n", filenames[i]);

        // Adding delay between files to ensure server processes each file completely
        if (i < file_count - 1)
        {
            printf("[CLIENT] Waiting before sending next file\n");
            usleep(200000); // 200ms delay between files
        }
    }

    // Waiting for final response from server
    printf("\n[CLIENT] Waiting for final server response\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes > 0)
    {
        // Adding null terminator to response
        response[bytes] = '\0';
        printf("[CLIENT] Upload result: %s\n", response);

        // Checking if upload was successful
        if (strstr(response, "SUCCESS") != NULL)
        {
            printf("[CLIENT] All files uploaded successfully\n");
            return 0;
        }
        else if (strstr(response, "PARTIAL") != NULL)
        {
            printf("[CLIENT] Some files uploaded successfully\n");
            return 0;
        }
        else
        {
            printf("[CLIENT] Upload failed\n");
            return -1;
        }
    }
    else
    {
        printf("[CLIENT] ERROR: No final response from server\n");
        return -1;
    }
}

/*=== DOWNLF COMMAND HANDLER ===*/
// Handling downlf command
int handle_downlf(int s1_socket, char *command)
{
    // Creating response buffer
    char response[1024];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing downlf command\n");
    printf("[CLIENT] Downloading files from S1\n");

    // Sending command to server
    printf("[CLIENT] Sending command to server\n");
    if (send(s1_socket, command, strlen(command), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send command\n");
        return -1;
    }

    // Waiting for response from server
    printf("[CLIENT] Waiting for server response\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes <= 0)
    {
        printf("[CLIENT] ERROR: No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes] = '\0';

    printf("[CLIENT] Server response: %s\n", response);

    // Checking if server is ready to send files
    if (strncmp(response, "READY", 5) == 0)
    {
        // Extracting file count from server response
        int file_count;
        sscanf(response, "READY %d", &file_count);
        printf("[CLIENT] Server will send %d files\n", file_count);

        // Receiving each file from server
        for (int i = 0; i < file_count; i++)
        {
            printf("\n[CLIENT] === Receiving file %d/%d ===\n", i + 1, file_count);

            // Receiving filename with null terminator
            char filename[257];
            memset(filename, 0, sizeof(filename));

            printf("[CLIENT] Receiving filename\n");
            // Receiving filename character by character until null terminator
            int filename_pos = 0;
            while (filename_pos < 256)
            {
                char c;
                // Receiving one character at a time
                bytes = recv(s1_socket, &c, 1, 0);
                if (bytes != 1)
                {
                    printf("[CLIENT] ERROR: Failed to receive filename character at position %d\n", filename_pos);
                    return -1;
                }

                // Storing character in filename
                filename[filename_pos] = c;

                // Checking if we found null terminator
                if (c == '\0')
                {
                    // Filename is complete
                    break;
                }

                // Moving to next position
                filename_pos++;
            }

            // Checking if filename is too long
            if (filename_pos >= 256)
            {
                printf("[CLIENT] ERROR: Filename too long or no null terminator found\n");
                return -1;
            }

            printf("[CLIENT] Received filename: '%s' (length: %d)\n", filename, (int)strlen(filename));

            // Receiving the file using existing function
            if (receive_file_from_server(s1_socket, filename) != 0)
            {
                printf("[CLIENT] ERROR: Failed to receive file: %s\n", filename);
                return -1;
            }

            printf("[CLIENT] Successfully downloaded: %s\n", filename);
        }

        printf("\n[CLIENT] Download result: SUCCESS - %d file(s) downloaded successfully\n", file_count);
        return 0;
    }
    else if (strstr(response, "ERROR") != NULL)
    {
        printf("[CLIENT] Download failed: %s\n", response);
        return -1;
    }
    else
    {
        printf("[CLIENT] ERROR: Unexpected server response: %s\n", response);
        return -1;
    }
}

/*=== DISPFNAMES COMMAND HANDLER ===*/

// Handling dispfnames command
int handle_dispfnames(int s1_socket, char *command)
{
    // Creating response buffer (larger for file lists)
    char response[8192];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing dispfnames command\n");

    // Sending command to server
    printf("[CLIENT] Sending command to server\n");
    if (send(s1_socket, command, strlen(command), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send command\n");
        return -1;
    }

    // Receiving file list from server
    printf("[CLIENT] Receiving file list from server\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes <= 0)
    {
        printf("[CLIENT] ERROR: No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes] = '\0';

    // Displaying file list to user
    printf("\n========== Files in directory ==========\n");
    if (strstr(response, "No files found") != NULL)
    {
        printf("No files found in the specified directory\n");
    }
    else if (strstr(response, "ERROR") != NULL)
    {
        printf("Error: %s\n", response);
    }
    else
    {
        // Counting number of files
        int file_count = 0;
        char *temp_response = strdup(response);
        char *line = strtok(temp_response, "\n");
        while (line != NULL)
        {
            if (strlen(line) > 0)
            {
                file_count++;
            }
            line = strtok(NULL, "\n");
        }
        free(temp_response);

        printf("Found %d files:\n", file_count);
        printf("%s", response);
    }
    printf("========================================\n");

    return 0;
}

/*=== REMOVEF COMMAND HANDLER ===*/

// Handling removef command
int handle_removef(int s1_socket, char *command)
{
    // Creating response buffer
    char response[1024];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing removef command\n");

    // Parsing command to count files for deletion
    char cmd[20], file1[512], file2[512];
    int argc = sscanf(command, "%s %s %s", cmd, file1, file2);

    // Calculating number of files to delete
    int file_count = argc - 1; // Subtract command name

    printf("[CLIENT] Requesting deletion of %d file(s)\n", file_count);
    if (file_count >= 1)
        printf("[CLIENT]   - %s\n", file1);
    if (file_count >= 2)
        printf("[CLIENT]   - %s\n", file2);

    // Sending command to server
    printf("[CLIENT] Sending command to server\n");
    if (send(s1_socket, command, strlen(command), 0) == -1)
    {
        printf("[CLIENT] ERROR: Failed to send command\n");
        return -1;
    }

    // Waiting for response from server
    printf("[CLIENT] Waiting for server response\n");
    bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
    if (bytes <= 0)
    {
        printf("[CLIENT] ERROR: No response from server\n");
        return -1;
    }
    // Adding null terminator to response
    response[bytes] = '\0';

    printf("[CLIENT] Delete result: %s\n", response);

    // Analyzing server response
    if (strstr(response, "SUCCESS: All files deleted") != NULL)
    {
        printf("[CLIENT] All files deleted successfully\n");
        return 0;
    }
    else if (strstr(response, "PARTIAL SUCCESS") != NULL)
    {
        printf("[CLIENT] Some files deleted successfully\n");
        return 0;
    }
    else if (strstr(response, "ERROR") != NULL)
    {
        printf("[CLIENT] Deletion failed\n");
        return -1;
    }
    else
    {
        printf("[CLIENT] Unexpected server response\n");
        return -1;
    }
}

/*=== TEST COMMAND HANDLER ===*/

// Handling test command
int handle_test(int s1_socket, char *command)
{
    // Creating response buffer
    char response[1024];
    // Storing bytes received
    int bytes;

    printf("[CLIENT] Processing test command\n");
    printf("[CLIENT] Testing server connectivity\n");

    // Sending test command to server
    if (send(s1_socket, command, strlen(command), 0) != -1)
    {
        // Receiving response from server
        bytes = recv(s1_socket, response, sizeof(response) - 1, 0);
        if (bytes > 0)
        {
            // Adding null terminator to response
            response[bytes] = '\0';
            printf("[CLIENT] Test result: %s\n", response);

            // Analyzing test result
            if (strstr(response, "OK") != NULL)
            {
                printf("[CLIENT] Server connectivity test passed\n");
                return 0;
            }
            else if (strstr(response, "ERROR") != NULL)
            {
                printf("[CLIENT] Server connectivity test failed\n");
                return -1;
            }
            else
            {
                printf("[CLIENT] Server responded with unknown status\n");
                return -1;
            }
        }
        else
        {
            printf("[CLIENT] ERROR: No response from server\n");
            return -1;
        }
    }
    else
    {
        printf("[CLIENT] ERROR: Failed to send test command\n");
        return -1;
    }
}

/*=== MAIN FUNCTION ===*/

int main()
{
    // Printing client startup banner
    printf("\n============================================================\n");
    printf("S25 Distributed File System Client\n");
    printf("Connecting to S1 server on port %d\n", S1_PORT);
    printf("============================================================\n");

    // Creating socket for server connection
    printf("[CLIENT] Creating socket\n");
    int s1_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (s1_socket == -1)
    {
        printf("[CLIENT] ERROR: Failed to create socket\n");
        exit(1);
    }

    // Setting up server address structure
    printf("[CLIENT] Setting up server address\n");
    struct sockaddr_in s1_addr = {0};
    // Setting protocol family to IPv4
    s1_addr.sin_family = AF_INET;
    // Setting server port
    s1_addr.sin_port = htons(S1_PORT);
    // Setting server IP address (localhost)
    s1_addr.sin_addr.s_addr = INADDR_ANY;

    // Connecting to S1 server
    printf("[CLIENT] Connecting to S1 server\n");
    if (connect(s1_socket, (struct sockaddr *)&s1_addr, sizeof(s1_addr)) == -1)
    {
        printf("[CLIENT] ERROR: Failed to connect to S1 server\n");
        printf("[CLIENT] Make sure S1 server is running on port %d\n", S1_PORT);
        // Closing socket before exit
        close(s1_socket);
        exit(1);
    }

    printf("[CLIENT] Successfully connected to S1 server\n");

    // Receiving welcome message from server
    char welcome[1024];
    int bytes = recv(s1_socket, welcome, sizeof(welcome) - 1, 0);
    if (bytes > 0)
    {
        // Adding null terminator to welcome message
        welcome[bytes] = '\0';
        printf("[CLIENT] Server says: %s\n", welcome);
    }

    // Displaying initial instructions
    printf("\n[CLIENT] Type 'help' for available commands or 'quit' to exit\n");

    // Creating command buffer
    char command[1024];
    // Creating response buffer
    char response[1024];

    // Main client interaction loop
    while (1)
    {
        // Displaying prompt
        printf("\ns25client$ ");
        fflush(stdout);

        // Reading command from user
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("\n[CLIENT] Input error, exiting\n");
            break;
        }

        // Removing newline character from command
        command[strcspn(command, "\n")] = '\0';

        // Checking for exit command
        if (strcmp(command, "quit") == 0)
        {
            printf("[CLIENT] Goodbye!\n");
            break;
        }

        // Checking for empty command
        if (strlen(command) == 0)
        {
            continue;
        }

        // Checking for help command
        if (strcmp(command, "help") == 0)
        {
            display_help();
            continue;
        }

        printf("\n[CLIENT] Processing command: %s\n", command);

        // Routing commands to appropriate handlers
        if (strncmp(command, "uploadf", 7) == 0)
        {
            printf("[CLIENT] Executing uploadf command\n");
            if (handle_uploadf(s1_socket, command) == 0)
            {
                printf("[CLIENT] UPLOADF command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] UPLOADF command failed\n");
            }
        }
        else if (strncmp(command, "downlf", 6) == 0)
        {
            printf("[CLIENT] Executing downlf command\n");
            if (handle_downlf(s1_socket, command) == 0)
            {
                printf("[CLIENT] DOWNLF command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] DOWNLF command failed\n");
            }
        }
        else if (strncmp(command, "downltar", 8) == 0)
        {
            printf("[CLIENT] Executing downltar command\n");
            if (handle_downltar(s1_socket, command) == 0)
            {
                printf("[CLIENT] DOWNLTAR command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] DOWNLTAR command failed\n");
            }
        }
        else if (strncmp(command, "dispfnames", 10) == 0)
        {
            printf("[CLIENT] Executing dispfnames command\n");
            if (handle_dispfnames(s1_socket, command) == 0)
            {
                printf("[CLIENT] DISPFNAMES command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] DISPFNAMES command failed\n");
            }
        }
        else if (strncmp(command, "removef", 7) == 0)
        {
            printf("[CLIENT] Executing removef command\n");
            if (handle_removef(s1_socket, command) == 0)
            {
                printf("[CLIENT] REMOVEF command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] REMOVEF command failed\n");
            }
        }
        else if (strncmp(command, "TEST", 4) == 0)
        {
            printf("[CLIENT] Executing test command\n");
            if (handle_test(s1_socket, command) == 0)
            {
                printf("[CLIENT] TEST command completed successfully\n");
            }
            else
            {
                printf("[CLIENT] TEST command failed\n");
            }
        }
        else
        {
            printf("[CLIENT] ERROR: Unknown command: %s\n", command);
            printf("[CLIENT] Type 'help' for available commands\n");
        }
    }

    // Closing connection to server
    printf("\n[CLIENT] Closing connection to server\n");
    close(s1_socket);
    printf("[CLIENT] Connection closed\n");
    printf("[CLIENT] Client terminated\n");

    return 0;
}