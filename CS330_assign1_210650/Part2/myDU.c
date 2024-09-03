#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>

struct VisitedDirectories {
    char path[PATH_MAX];
};

int size(const char *path, struct VisitedDirectories *visitedDirs, int *visitedDirsCount);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        perror("Unable to execute");
        return 1;
    }
    const char *root_path = argv[1]; 
    struct VisitedDirectories visitedDirs[100]; 
    int visitedDirsCount = 0;
    int total_size = size(root_path, visitedDirs, &visitedDirsCount);
    if (total_size >= 0) {
        printf("%d\n", total_size);
    } else {
        perror("Unable to execute");
        return 1;
    }
    return 0;
}
int size(const char *path, struct VisitedDirectories *visitedDirs, int *visitedDirsCount) {
    for (int i = 0; i < *visitedDirsCount; i++) {
        if (strcmp(path, visitedDirs[i].path) == 0) {
            return 0; 
        }
    }

    strcpy(visitedDirs[*visitedDirsCount].path, path);
    (*visitedDirsCount)++;

    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }
    int total_size = 0;
    int fd[2];
    struct dirent *entry;
    int i = 0;
    struct stat s;
    if (!(stat(path, &s) == 0)) {
        perror("Unable to execute");
        closedir(dir);
        return -1;
    }
    total_size += s.st_size;
    while ((entry = readdir(dir)) != NULL) {
        if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
            continue;

        char entry_path[PATH_MAX];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
        struct stat st;

        if (!(stat(entry_path, &st) == 0)) {
            perror("Unable to execute");
            closedir(dir);
            return -1;
        }

        if (S_ISLNK(st.st_mode)) {
       
            char link_target[PATH_MAX];
            ssize_t link_size = readlink(entry_path, link_target, sizeof(link_target) - 1);
            if (link_size == -1) {
                perror("Unable to execute");
                closedir(dir);
                return -1;
            }
            link_target[link_size] = '\0';

            struct stat target_st;
            if (!(stat(link_target, &target_st) == 0)) {
                perror("Unable to execute");
                closedir(dir);
                return -1;
            }
            if (S_ISDIR(target_st.st_mode)) {
                int sdir_size = size(link_target, visitedDirs, visitedDirsCount);
                total_size += sdir_size;
            } else if (S_ISREG(target_st.st_mode)) {
                total_size += target_st.st_size;
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (pipe(fd) == -1) {
                perror("Unable to execute");
                closedir(dir);
                return -1;
            }
            pid_t pid = fork();
            if (pid == -1) {
                perror("Unable to execute");
                closedir(dir);
                return -1;
            }
            if (pid == 0) {
                close(fd[0]);
                int sdir_size = size(entry_path, visitedDirs, visitedDirsCount);
                write(fd[1], &sdir_size, sizeof(int));
                close(fd[1]);
                exit(0);
            } else {
                close(fd[1]);
                int sub_dir_size;
                read(fd[0], &sub_dir_size, sizeof(int));
                close(fd[0]);
                waitpid(pid, NULL, 0);
                total_size += sub_dir_size;
            }
        } else if (S_ISREG(st.st_mode)) {
            total_size += st.st_size;
        }
    }
    closedir(dir);
    return total_size;
}
