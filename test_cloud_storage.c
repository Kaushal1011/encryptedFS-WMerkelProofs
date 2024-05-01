#include "include/cloud_storage.h"
#include <stdio.h>

int main() {
    const char *upload_filename = "test_upload.txt";
    const char *download_file_id = "file_id_here"; // You need to know the file ID

    // Test upload
    printf("Testing file upload...\n");
    if (upload_file(upload_filename) == 0) {
        printf("Upload successful!\n");
    } else {
        printf("Upload failed!\n");
    }

    // // Test download
    // printf("Testing file download...\n");
    // if (download_file(download_file_id) == 0) {
    //     printf("Download successful!\n");
    // } else {
    //     printf("Download failed!\n");
    // }

    return 0;
}
