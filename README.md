Distributed File System using Socket Programming (COMP-8567, Summer 2025)

This project implements a distributed file system with four servers (S1–S4) and a client (s25client) using TCP sockets in C. Clients interact only with the main server (S1), while S1 transparently delegates storage and retrieval of different file types across S2, S3, and S4.

Key features include:
File upload/download: Seamlessly handles .c, .pdf, .txt, and .zip files with automatic distribution across servers.
Remote commands: uploadf, downlf, removef, downltar, dispfnames.
Transparency: Clients are unaware of backend distribution — all interactions appear to happen with S1.
Concurrency: Each client request is served in a dedicated process via fork().
File aggregation: On-demand tar creation and consolidated file listings across all servers.
