// cloud_storage.c

#include "../include/cloud_storage.h"
#include <stdio.h>
#include <curl/curl.h>

// Callback function for reading the file to upload
static size_t read_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t retcode = fread(ptr, size, nmemb, stream);
    return retcode;
}

// Function to upload a file to Google Drive
int upload_file(const char *filename) {
    CURL *curl;
    CURLcode res;
    FILE *file;
    struct curl_slist *headers = NULL;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        file = fopen(filename, "rb");
        if (!file) {
            return -1; // Cannot open file
        }

        form = curl_mime_init(curl);
        field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, filename);

        // Set the URL that is about to receive our POST
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/upload");

        // What we are sending
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        // Send it!
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        // Cleanup
        curl_easy_cleanup(curl);
        curl_mime_free(form);
        fclose(file);
    }
    curl_global_cleanup();
    return (int)res;
}

// Callback function for writing downloaded data to file
static size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// Function to download a file from Google Drive
int download_file(const char *file_id) {
    CURL *curl;
    CURLcode res;
    FILE *file;
    char url[256];

    snprintf(url, sizeof(url), "http://localhost:8080/download/%s", file_id);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        file = fopen("downloaded_file.bin", "wb");
        if (!file) {
            return -1; // Cannot open file
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        // Perform the request
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        // Cleanup
        curl_easy_cleanup(curl);
        fclose(file);
    }
    curl_global_cleanup();
    return (int)res;
}
