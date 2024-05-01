#ifndef CLOUD_STORAGE_H
#define CLOUD_STORAGE_H

// Function to upload a file to Google Drive
int upload_file(const char *filename);

// Function to download a file from Google Drive
int download_file(const char *file_id);

#endif // CLOUD_STORAGE_H