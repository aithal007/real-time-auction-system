#include <stdio.h>
#include <string.h>

#include "common.h"

#define USERS_FILE "data/users.txt"

int user_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    char file_user[MAX_USERNAME];
    char file_pass[MAX_PASSWORD];

    if (!fp) {
        return 0;
    }

    while (fscanf(fp, "%49s %49s", file_user, file_pass) == 2) {
        if (strcmp(file_user, username) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int login_user(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    char file_user[MAX_USERNAME];
    char file_pass[MAX_PASSWORD];

    if (!fp) {
        return 0;
    }

    while (fscanf(fp, "%49s %49s", file_user, file_pass) == 2) {
        if (strcmp(file_user, username) == 0 && strcmp(file_pass, password) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void register_user(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "a");

    if (!fp) {
        return;
    }

    fprintf(fp, "%s %s\n", username, password);
    fclose(fp);
}
