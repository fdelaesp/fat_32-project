#ifndef COMMANDS_H
#define COMMANDS_H

#include "fat32.h"

/* Command functions */
void cmd_info(FileSystem *fs);
void cmd_ls(FileSystem *fs);
void cmd_cd(FileSystem *fs, const char *dirname);
void cmd_mkdir(FileSystem *fs, const char *dirname);
void cmd_creat(FileSystem *fs, const char *filename);
void cmd_open(FileSystem *fs, const char *filename, const char *mode);
void cmd_close(FileSystem *fs, const char *filename);
void cmd_lsof(FileSystem *fs);
void cmd_lseek(FileSystem *fs, const char *filename, uint32_t offset);
void cmd_read(FileSystem *fs, const char *filename, uint32_t size);
void cmd_write(FileSystem *fs, const char *filename, const char *string);
void cmd_mv(FileSystem *fs, const char *source, const char *dest);
void cmd_rm(FileSystem *fs, const char *filename);
void cmd_rmdir(FileSystem *fs, const char *dirname);

/* Helper functions */
OpenFile *find_open_file(FileSystem *fs, const char *filename);
int add_open_file(FileSystem *fs, const char *filename, const char *mode,
                  const char *path, uint32_t first_cluster, uint32_t size);
void remove_open_file(FileSystem *fs, const char *filename);

#endif
