// File: cloud_storage.c
#include "../include/cloud_storage.h"
#include <stdio.h>
#include <curl/curl.h>

#include <string.h>

OAuthTokens tokens;

int read_tokens_from_file(const char *filename, OAuthTokens *tokens)
{
    printf("cloud_storage: Reading tokens from file\n");

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Unable to open the token file.\n");
        return -1;
    }

    char line[2048];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *token, *value;
        token = strtok(line, "=");
        value = strtok(NULL, "\n");
        if (token && value)
        {
            if (strcmp(token, "access_token") == 0)
            {
                strncpy(tokens->access_token, value, sizeof(tokens->access_token) - 1);
            }
            else if (strcmp(token, "refresh_token") == 0)
            {
                strncpy(tokens->refresh_token, value, sizeof(tokens->refresh_token) - 1);
            }
            else if (strcmp(token, "token_uri") == 0)
            {
                strncpy(tokens->token_uri, value, sizeof(tokens->token_uri) - 1);
            }
            else if (strcmp(token, "client_id") == 0)
            {
                strncpy(tokens->client_id, value, sizeof(tokens->client_id) - 1);
            }
            else if (strcmp(token, "client_secret") == 0)
            {
                strncpy(tokens->client_secret, value, sizeof(tokens->client_secret) - 1);
            }
        }
    }
    fclose(file);
    return 0;
}

// Function to URL encode a string
char *url_encode(char *str)
{
    char *encoded = curl_easy_escape(NULL, str, 0);
    return encoded; // Note: This should be freed with curl_free() after usage
}

// Callback function for writing downloaded data to file
static size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// Helper function to add headers with the access token
void add_auth_header(CURL *curl, OAuthTokens *creds)
{
    struct curl_slist *headers = NULL;
    char auth_header[3000];
    char refresh_token_header[3000];
    char client_id_header[3000];
    char client_secret_header[3000];
    char token_uri_header[3000];

    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", creds->access_token);
    snprintf(refresh_token_header, sizeof(refresh_token_header), "X-Refresh-Token: %s", creds->refresh_token);
    snprintf(client_id_header, sizeof(client_id_header), "X-Client-ID: %s", creds->client_id);
    snprintf(client_secret_header, sizeof(client_secret_header), "X-Client-Secret: %s", creds->client_secret);
    snprintf(token_uri_header, sizeof(token_uri_header), "X-Token-URI: %s", creds->token_uri);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, refresh_token_header);
    headers = curl_slist_append(headers, client_id_header);
    headers = curl_slist_append(headers, client_secret_header);
    headers = curl_slist_append(headers, token_uri_header);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
}

int upload_file_to_folder(const char *folder_name, const char *filename, OAuthTokens *tokens)
{
    printf("cloud_storage: Uploading file %s to folder %s\n", filename, folder_name);
    CURL *curl;
    CURLcode res;
    FILE *file;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl)
    {
        file = fopen(filename, "rb");
        if (!file)
        {
            curl_easy_cleanup(curl);
            return -1; // Cannot open file
        }

        // Prepare form data
        curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file",
                     CURLFORM_FILE, filename, CURLFORM_END);
        curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "folder_name",
                     CURLFORM_COPYCONTENTS, folder_name, CURLFORM_END);
        curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file_name",
                     CURLFORM_COPYCONTENTS, filename, CURLFORM_END);

        // Set the URL and multipart form data
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/upload_by_folder_name");
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

        // Add Authorization header
        add_auth_header(curl, tokens);

        // Perform the file upload
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        // Cleanup
        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        fclose(file);
    }
    curl_global_cleanup();
    return (int)res;
}

int download_file_from_folder(const char *folder_name, const char *filename, OAuthTokens *tokens)
{
    printf("cloud_storage: Downloading file %s from folder %s\n", filename, folder_name);

    CURL *curl;
    CURLcode res;
    FILE *file;
    char url[1024]; // Adjust size as necessary
    char filepath[512];

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    // URL encode folder_name and filename
    char *encoded_folder_name = curl_easy_escape(curl, folder_name, 0);
    char *encoded_filename = curl_easy_escape(curl, filename, 0);

    snprintf(url, sizeof(url), "http://localhost:8080/download_by_folder_name?folder_name=%s&filename=%s", encoded_folder_name, encoded_filename);
    snprintf(filepath, sizeof(filepath), "%s", filename); // Modify '/path/to/save/' as needed

    if (curl)
    {
        file = fopen(filepath, "wb");
        if (!file)
        {
            fprintf(stderr, "Failed to open file for writing.\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            curl_free(encoded_folder_name);
            curl_free(encoded_filename);
            return -1; // Cannot open file
        }

        // Prepare and set the authorization headers
        add_auth_header(curl, tokens);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        // Perform the file download
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        // Cleanup
        curl_easy_cleanup(curl);
        fclose(file);
        curl_free(encoded_folder_name);
        curl_free(encoded_filename);
    }
    else
    {
        fprintf(stderr, "Failed to initialize CURL.\n");
    }
    curl_global_cleanup();
    return (int)res;
}