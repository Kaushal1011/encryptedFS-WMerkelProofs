#ifndef CLOUD_STORAGE_H
#define CLOUD_STORAGE_H

typedef struct
{
    char access_token[2048];
    char refresh_token[2048];
    char token_uri[2048];
    char client_id[2048];
    char client_secret[2048];
} OAuthTokens;

extern OAuthTokens tokens;

int upload_file_to_folder(const char *folder_name, const char *filename, OAuthTokens *tokens);

int download_file_from_folder(const char *folder_name, const char *filename, OAuthTokens *tokens);

int read_tokens_from_file(const char *filename, OAuthTokens *tokens);

#endif // CLOUD_STORAGE_H