#include <stdio.h>
#include <string.h>

#include "common.h"

#define USERS_FILE "data/users.txt"

int user_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    char line[128];
    char file_user[MAX_USERNAME];
    char file_pass[MAX_PASSWORD];

    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }

        if (sscanf(line, "%49s %49s", file_user, file_pass) == 2 &&
            strcmp(file_user, username) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int login_user(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    char line[128];
    char file_user[MAX_USERNAME];
    char file_pass[MAX_PASSWORD];

    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }

        if (sscanf(line, "%49s %49s", file_user, file_pass) == 2 &&
            strcmp(file_user, username) == 0 &&
            strcmp(file_pass, password) == 0) {
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
