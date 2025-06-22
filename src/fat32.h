#ifndef FAT32_FAT32_H
#define FAT32_FAT32_H

int create_fat32_file(const char *filepath);
int fat32_mkdir(const char *filesystem_path, const char *path);
int fat32_touch(const char *filesystem_path, const char *path);
int fat32_ls(const char *filesystem_path, const char *path);
int fat32_is_directory(const char *filesystem_path, const char *path);
int fat32_exists(const char *filesystem_path, const char *path);

#endif
