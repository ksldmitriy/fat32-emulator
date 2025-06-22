#include "fat32.h"

#include <stdio.h>
#include <endian.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <libgen.h>

#define FILE_SIZE (20 * 1024 * 1024)

#define SECTOR_SIZE 512
#define SECTORS_PER_CLUSTER 8
#define NUM_FATS 1
#define ROOT_DIR_CLUSTER 2

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20

typedef struct {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t byts_per_sec;
    uint8_t sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t num_fats;
    uint16_t root_ent_cnt;
    uint16_t tot_sec16;
    uint8_t media;
    uint16_t fat_sz16;
    uint16_t sec_per_trk;
    uint16_t num_heads;
    uint32_t hidd_sec;
    uint32_t tot_sec32;

    uint32_t fat_sz32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_clus;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t reserved[12];
    uint8_t drv_num;
    uint8_t reserved1;
    uint8_t boot_sig;
    uint32_t vol_id;
    uint8_t vol_lab[11];
    uint8_t fil_sys_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint32_t lead_sig;
    uint8_t Reserved1[480];
    uint32_t struc_sig;
    uint32_t free_count;
    uint32_t nxt_free;
    uint8_t Reserved2[12];
    uint32_t trail_sig;
} __attribute__((packed)) fat32_fs_info_t;

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

static uint16_t get_fat_date() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    uint16_t year = tm->tm_year + 1900;
    uint16_t month = tm->tm_mon + 1;
    uint16_t day = tm->tm_mday;

    return ((year - 1980) << 9) | (month << 5) | day;
}

static uint16_t get_fat_time() {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    uint16_t hour = tm->tm_hour;
    uint16_t minute = tm->tm_min;
    uint16_t second = tm->tm_sec / 2;

    return (hour << 11) | (minute << 5) | second;
}

int create_fat32_file(const char *filepath) {
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam");
        return -1;
    }

    uint32_t total_sectors = FILE_SIZE / SECTOR_SIZE;
    uint32_t fat_size_sectors = (total_sectors / SECTORS_PER_CLUSTER + 2) * 4 / SECTOR_SIZE + 1;
    uint32_t reserved_sectors = 32;
    uint32_t data_start_sector = reserved_sectors + (NUM_FATS * fat_size_sectors);
    uint32_t data_sectors = total_sectors - data_start_sector;
    uint32_t cluster_count = data_sectors / SECTORS_PER_CLUSTER;

    fat32_bpb_t bpb = {0};

    bpb.jmp_boot[0] = 0xEB;
    bpb.jmp_boot[1] = 0x58;
    bpb.jmp_boot[2] = 0x90;

    memcpy(bpb.oem_name, "MSWIN4.1", 8);

    bpb.byts_per_sec = htole16(SECTOR_SIZE);
    bpb.sec_per_clus = SECTORS_PER_CLUSTER;
    bpb.rsvd_sec_cnt = htole16(reserved_sectors);
    bpb.num_fats = NUM_FATS;
    bpb.root_ent_cnt = 0;
    bpb.tot_sec16 = 0;
    bpb.media = 0xF8;
    bpb.fat_sz16 = 0;
    bpb.sec_per_trk = htole16(63);
    bpb.num_heads = htole16(255);
    bpb.hidd_sec = 0;
    bpb.tot_sec32 = htole32(total_sectors);

    bpb.fat_sz32 = htole32(fat_size_sectors);
    bpb.ext_flags = 0;
    bpb.fs_ver = 0;
    bpb.root_clus = htole32(ROOT_DIR_CLUSTER);
    bpb.fs_info = htole16(1);
    bpb.bk_boot_sec = htole16(6);

    bpb.drv_num = 0x80;
    bpb.boot_sig = 0x29;
    bpb.vol_id = htole32(0x12345678);
    memcpy(bpb.vol_lab, "NO NAME    ", 11);
    memcpy(bpb.fil_sys_type, "FAT32   ", 8);

    if (fwrite(&bpb, sizeof(bpb), 1, file) != 1) {
        fprintf(stderr, "failed to write boot sector");
        fclose(file);
        return -1;
    }

    uint8_t padding[SECTOR_SIZE - sizeof(bpb) - 2] = {0};
    fwrite(padding, sizeof(padding), 1, file);
    uint16_t boot_signature = htole16(0xAA55);
    fwrite(&boot_signature, sizeof(boot_signature), 1, file);

    fat32_fs_info_t fs_info = {0};
    fs_info.lead_sig = htole32(0x41615252);
    fs_info.struc_sig = htole32(0x61417272);
    fs_info.free_count = htole32(cluster_count - 1);
    fs_info.nxt_free = htole32(3);
    fs_info.trail_sig = htole32(0xAA550000);

    fwrite(&fs_info, sizeof(fs_info), 1, file);

    uint8_t fs_info_padding[SECTOR_SIZE - sizeof(fs_info)] = {0};
    fwrite(fs_info_padding, sizeof(fs_info_padding), 1, file);

    fseek(file, reserved_sectors * SECTOR_SIZE, SEEK_SET);

    uint32_t *fat = calloc(fat_size_sectors * SECTOR_SIZE / 4, sizeof(uint32_t));
    if (!fat) {
        fprintf(stderr, "failed to allocate fat");
        fclose(file);
        return -1;
    }

    fat[0] = htole32(0x0FFFFFF8);
    fat[1] = htole32(0x0FFFFFFF);
    fat[2] = htole32(0x0FFFFFFF);

    for (int i = 0; i < NUM_FATS; i++) {
        if (fwrite(fat, fat_size_sectors * SECTOR_SIZE, 1, file) != 1) {
            fprintf(stderr, "failed to write fat");
            free(fat);
            fclose(file);
            return -1;
        }
    }

    free(fat);

    fseek(file, data_start_sector * SECTOR_SIZE, SEEK_SET);

    uint8_t root_cluster[SECTORS_PER_CLUSTER * SECTOR_SIZE] = {0};

    fat32_dir_entry_t dot_entry = {0};
    memset(dot_entry.name, ' ', 11);
    dot_entry.name[0] = '.';
    dot_entry.attr = ATTR_DIRECTORY;
    dot_entry.crt_date = get_fat_date();
    dot_entry.crt_time = get_fat_time();
    dot_entry.lst_acc_date = dot_entry.crt_date;
    dot_entry.wrt_date = dot_entry.crt_date;
    dot_entry.wrt_time = dot_entry.crt_time;
    dot_entry.fst_clus_hi = htole16(ROOT_DIR_CLUSTER >> 16);
    dot_entry.fst_clus_lo = htole16(ROOT_DIR_CLUSTER & 0xFFFF);
    dot_entry.file_size = 0;

    memcpy(root_cluster, &dot_entry, sizeof(dot_entry));
    fwrite(root_cluster, sizeof(root_cluster), 1, file);

    fseek(file, FILE_SIZE - 1, SEEK_SET);
    fputc(0, file);

    fclose(file);
    return 0;
}

static uint32_t read_fat_entry(FILE *file, uint32_t cluster, uint32_t fat_start_sector) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / SECTOR_SIZE);
    uint32_t fat_entry_offset = fat_offset % SECTOR_SIZE;

    fseek(file, fat_sector * SECTOR_SIZE + fat_entry_offset, SEEK_SET);
    uint32_t fat_entry;
    fread(&fat_entry, sizeof(fat_entry), 1, file);
    return le32toh(fat_entry) & 0x0FFFFFFF;
}

static void write_fat_entry(FILE *file,
                            uint32_t cluster,
                            uint32_t value,
                            uint32_t fat_start_sector,
                            uint8_t num_fats) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector_offset = fat_offset / SECTOR_SIZE;
    uint32_t fat_entry_offset = fat_offset % SECTOR_SIZE;

    value = htole32(value & 0x0FFFFFFF);

    for (int i = 0; i < num_fats; i++) {
        uint32_t fat_sector =
            fat_start_sector + (i * (fat_start_sector / num_fats)) + fat_sector_offset;
        fseek(file, fat_sector * SECTOR_SIZE + fat_entry_offset, SEEK_SET);
        fwrite(&value, sizeof(value), 1, file);
    }
}

static uint32_t find_free_cluster(FILE *file, uint32_t fat_start_sector, uint32_t cluster_count) {
    for (uint32_t cluster = 2; cluster < cluster_count + 2; cluster++) {
        if (read_fat_entry(file, cluster, fat_start_sector) == 0) {
            return cluster;
        }
    }
    return 0;
}

static void name_to_83(const char *name, uint8_t *dest) {
    memset(dest, ' ', 11);

    int name_len = strlen(name);
    int dot_pos = -1;

    for (int i = name_len - 1; i >= 0; i--) {
        if (name[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    int name_part_len = (dot_pos == -1) ? name_len : dot_pos;
    for (int i = 0; i < name_part_len && i < 8; i++) {
        dest[i] = toupper(name[i]);
    }

    if (dot_pos != -1) {
        int ext_len = name_len - dot_pos - 1;
        for (int i = 0; i < ext_len && i < 3; i++) {
            dest[8 + i] = toupper(name[dot_pos + 1 + i]);
        }
    }
}

static uint32_t find_dir_entry_cluster(FILE *file,
                                       uint32_t start_cluster,
                                       const char *name,
                                       fat32_bpb_t *bpb,
                                       uint32_t data_start_sector,
                                       uint32_t fat_start_sector) {
    uint8_t target_name[11];
    name_to_83(name, target_name);

    uint32_t cluster = start_cluster;
    uint32_t cluster_size = le16toh(bpb->byts_per_sec) * bpb->sec_per_clus;

    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start_sector + (cluster - 2) * bpb->sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);

        fat32_dir_entry_t entry;
        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fread(&entry, sizeof(entry), 1, file);

            if (entry.name[0] == 0)
                return 0;
            if (entry.name[0] == 0xE5)
                continue;

            if (memcmp(entry.name, target_name, 11) == 0 && (entry.attr & ATTR_DIRECTORY)) {
                return (le16toh(entry.fst_clus_hi) << 16) | le16toh(entry.fst_clus_lo);
            }
        }

        cluster = read_fat_entry(file, cluster, fat_start_sector);
    }

    return 0;
}

static int find_dir_entry(FILE *file,
                          uint32_t start_cluster,
                          const char *name,
                          fat32_bpb_t *bpb,
                          uint32_t data_start_sector,
                          uint32_t fat_start_sector) {
    return find_dir_entry_cluster(file,
                                  start_cluster,
                                  name,
                                  bpb,
                                  data_start_sector,
                                  fat_start_sector) != 0
               ? 1
               : -1;
}

static uint32_t find_parent_cluster(FILE *file,
                                    const char *path,
                                    fat32_bpb_t *bpb,
                                    uint32_t data_start_sector,
                                    uint32_t fat_start_sector) {
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0 || strlen(path) == 0) {
        return le32toh(bpb->root_clus);
    }

    char *path_copy = strdup(path);
    if (!path_copy)
        return 0;

    int len = strlen(path_copy);
    while (len > 1 && (path_copy[len - 1] == '/' || path_copy[len - 1] == '\\')) {
        path_copy[len - 1] = '\0';
        len--;
    }

    char *last_slash = strrchr(path_copy, '/');
    char *last_backslash = strrchr(path_copy, '\\');
    char *separator = (last_slash > last_backslash) ? last_slash : last_backslash;

    uint32_t parent_cluster;

    if (!separator) {
        parent_cluster = le32toh(bpb->root_clus);
    } else if (separator == path_copy) {
        parent_cluster = le32toh(bpb->root_clus);
    } else {
        *separator = '\0';
        parent_cluster =
            find_parent_cluster(file, path_copy, bpb, data_start_sector, fat_start_sector);
        *separator = '/';

        if (parent_cluster == 0) {
            free(path_copy);
            return 0;
        }

        char *parent_name = separator + 1;
        char *parent_end = strrchr(path_copy, '/');
        if (!parent_end)
            parent_end = strrchr(path_copy, '\\');
        if (parent_end && parent_end != separator) {
            *separator = '\0';
            parent_name = strrchr(path_copy, '/');
            if (!parent_name)
                parent_name = strrchr(path_copy, '\\');
            if (parent_name)
                parent_name++;
            else
                parent_name = path_copy;
        }
    }

    free(path_copy);
    return parent_cluster;
}

static uint32_t resolve_path_to_cluster(FILE *file,
                                        const char *path,
                                        fat32_bpb_t *bpb,
                                        uint32_t data_start_sector,
                                        uint32_t fat_start_sector) {

    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0 || strlen(path) == 0) {
        return le32toh(bpb->root_clus);
    }

    uint32_t current_cluster = le32toh(bpb->root_clus);

    char *path_copy = strdup(path);
    if (!path_copy)
        return 0;

    char *p = path_copy;
    while (*p == '/' || *p == '\\')
        p++;

    int len = strlen(p);
    while (len > 0 && (p[len - 1] == '/' || p[len - 1] == '\\')) {
        p[len - 1] = '\0';
        len--;
    }

    if (strlen(p) == 0) {
        free(path_copy);
        return le32toh(bpb->root_clus);
    }

    char *token = strtok(p, "/\\");
    while (token != NULL) {
        uint32_t next_cluster = find_dir_entry_cluster(file,
                                                       current_cluster,
                                                       token,
                                                       bpb,
                                                       data_start_sector,
                                                       fat_start_sector);
        if (next_cluster == 0) {

            free(path_copy);
            return 0;
        }
        current_cluster = next_cluster;
        token = strtok(NULL, "/\\");
    }

    free(path_copy);
    return current_cluster;
}

int fat32_mkdir(const char *filesystem_path, const char *path) {
    FILE *file = fopen(filesystem_path, "r+b");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam\n");
        return -1;
    }

    fat32_bpb_t bpb;
    fseek(file, 0, SEEK_SET);
    if (fread(&bpb, sizeof(bpb), 1, file) != 1) {
        fprintf(stderr, "failed to read BPB");
        fclose(file);
        return -1;
    }

    uint32_t fat_start_sector = le16toh(bpb.rsvd_sec_cnt);
    uint32_t fat_size_sectors = le32toh(bpb.fat_sz32);
    uint32_t data_start_sector = fat_start_sector + (bpb.num_fats * fat_size_sectors);
    uint32_t cluster_size = le16toh(bpb.byts_per_sec) * bpb.sec_per_clus;
    uint32_t total_clusters = (le32toh(bpb.tot_sec32) - data_start_sector) / bpb.sec_per_clus;

    char *path_copy = strdup(path);
    if (!path_copy) {
        fprintf(stderr, "failed to allocate memory\n");
        return -1;
    }
    char *dir_name = strdup(basename(path_copy));
    if (!dir_name) {
        fprintf(stderr, "failed to allocate memory\n");
        free(path_copy);
        return -1;
    }
    strcpy(path_copy, path);
    char *parent_path = strdup(dirname(path_copy));
    if (!parent_path) {
        fprintf(stderr, "failed to allocate memory\n");
        free(dir_name);
        free(path_copy);
        return -1;
    }
    free(path_copy);

    uint32_t parent_cluster =
        resolve_path_to_cluster(file, parent_path, &bpb, data_start_sector, fat_start_sector);
    if (parent_cluster == 0) {
        fprintf(stderr, "parent directory not found: %s\n", parent_path);
        free(dir_name);
        free(parent_path);
        fclose(file);
        return -1;
    }
    free(parent_path);

    if (find_dir_entry(file, parent_cluster, dir_name, &bpb, data_start_sector, fat_start_sector) >
        0) {
        fprintf(stderr, "directory already exists\n");
        free(dir_name);
        fclose(file);
        return -1;
    }

    uint32_t new_cluster = find_free_cluster(file, fat_start_sector, total_clusters);
    if (new_cluster == 0) {
        fprintf(stderr, "no free clusters available\n");
        free(dir_name);
        fclose(file);
        return -1;
    }

    write_fat_entry(file, new_cluster, 0x0FFFFFFF, fat_start_sector, bpb.num_fats);

    uint32_t new_dir_sector = data_start_sector + (new_cluster - 2) * bpb.sec_per_clus;
    fseek(file, new_dir_sector * SECTOR_SIZE, SEEK_SET);

    fat32_dir_entry_t dot_entry = {0};
    memcpy(dot_entry.name, ".          ", 11);
    dot_entry.attr = ATTR_DIRECTORY;
    dot_entry.fst_clus_hi = htole16((new_cluster >> 16) & 0xFFFF);
    dot_entry.fst_clus_lo = htole16(new_cluster & 0xFFFF);
    dot_entry.crt_date = dot_entry.wrt_date = dot_entry.lst_acc_date = htole16(get_fat_date());
    dot_entry.crt_time = dot_entry.wrt_time = htole16(get_fat_time());
    fwrite(&dot_entry, sizeof(dot_entry), 1, file);

    fat32_dir_entry_t dotdot_entry = {0};
    memcpy(dotdot_entry.name, "..         ", 11);
    dotdot_entry.attr = ATTR_DIRECTORY;
    dotdot_entry.fst_clus_hi = htole16((parent_cluster >> 16) & 0xFFFF);
    dotdot_entry.fst_clus_lo = htole16(parent_cluster & 0xFFFF);
    dotdot_entry.crt_date = dotdot_entry.wrt_date = dotdot_entry.lst_acc_date =
        htole16(get_fat_date());
    dotdot_entry.crt_time = dotdot_entry.wrt_time = htole16(get_fat_time());
    fwrite(&dotdot_entry, sizeof(dotdot_entry), 1, file);

    uint8_t *zero_buf = calloc(cluster_size - 2, sizeof(fat32_dir_entry_t));
    if (!zero_buf) {
        fprintf(stderr, "failed to allocate memory\n");
        free(dir_name);
        return -1;
    }
    fwrite(zero_buf, sizeof(zero_buf), 1, file);

    uint32_t cluster = parent_cluster;
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start_sector + (cluster - 2) * bpb.sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);

        fat32_dir_entry_t entry;
        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            long pos = ftell(file);
            fread(&entry, sizeof(entry), 1, file);

            if (entry.name[0] == 0 || entry.name[0] == 0xE5) {
                fseek(file, pos, SEEK_SET);

                fat32_dir_entry_t new_entry = {0};
                name_to_83(dir_name, new_entry.name);
                new_entry.attr = ATTR_DIRECTORY;
                new_entry.fst_clus_hi = htole16((new_cluster >> 16) & 0xFFFF);
                new_entry.fst_clus_lo = htole16(new_cluster & 0xFFFF);
                new_entry.crt_date = new_entry.wrt_date = new_entry.lst_acc_date =
                    htole16(get_fat_date());
                new_entry.crt_time = new_entry.wrt_time = htole16(get_fat_time());

                fwrite(&new_entry, sizeof(new_entry), 1, file);

                free(dir_name);
                fclose(file);
                return 0;
            }
        }

        cluster = read_fat_entry(file, cluster, fat_start_sector);
    }

    fprintf(stderr, "parent directory is full\n");
    free(dir_name);
    fclose(file);
    return -1;
}

int fat32_touch(const char *filesystem_path, const char *path) {
    FILE *file = fopen(filesystem_path, "r+b");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam\n");
        return -1;
    }

    fat32_bpb_t bpb;
    fread(&bpb, sizeof(bpb), 1, file);

    uint32_t fat_start = le16toh(bpb.rsvd_sec_cnt);
    uint32_t fat_size = le32toh(bpb.fat_sz32);
    uint32_t data_start = fat_start + bpb.num_fats * fat_size;
    uint32_t total_clusters = (le32toh(bpb.tot_sec32) - data_start) / bpb.sec_per_clus;

    char *path_copy = strdup(path);
    char *file_name = strdup(basename(path_copy));
    strcpy(path_copy, path);
    char *dir_path = dirname(path_copy);

    uint32_t dir_cluster = resolve_path_to_cluster(file, dir_path, &bpb, data_start, fat_start);
    if (dir_cluster == 0) {
        fprintf(stderr, "Directory not found: %s\n", dir_path);
        free(file_name);
        free(path_copy);
        fclose(file);
        return -1;
    }

    if (find_dir_entry(file, dir_cluster, file_name, &bpb, data_start, fat_start) > 0) {
        fprintf(stderr, "File already exists: %s\n", file_name);
        free(file_name);
        free(path_copy);
        fclose(file);
        return 0;
    }

    uint32_t cluster_size = le16toh(bpb.byts_per_sec) * bpb.sec_per_clus;
    uint32_t cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start + (cluster - 2) * bpb.sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);
        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            long pos = ftell(file);
            fat32_dir_entry_t entry;
            fread(&entry, sizeof(entry), 1, file);
            if (entry.name[0] == 0 || entry.name[0] == 0xE5) {
                fseek(file, pos, SEEK_SET);
                fat32_dir_entry_t new_entry = {0};
                name_to_83(file_name, new_entry.name);
                new_entry.attr = ATTR_ARCHIVE;
                new_entry.crt_date = new_entry.wrt_date = new_entry.lst_acc_date =
                    htole16(get_fat_date());
                new_entry.crt_time = new_entry.wrt_time = htole16(get_fat_time());
                new_entry.file_size = 0;
                fwrite(&new_entry, sizeof(new_entry), 1, file);
                free(file_name);
                free(path_copy);
                fclose(file);
                return 0;
            }
        }
        cluster = read_fat_entry(file, cluster, fat_start);
    }

    fprintf(stderr, "Directory is full\n");
    free(file_name);
    free(path_copy);
    fclose(file);
    return -1;
}

int fat32_ls(const char *filesystem_path, const char *path) {
    FILE *file = fopen(filesystem_path, "rb");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam\n");
        return -1;
    }

    fat32_bpb_t bpb;
    fread(&bpb, sizeof(bpb), 1, file);

    uint32_t fat_start = le16toh(bpb.rsvd_sec_cnt);
    uint32_t fat_size = le32toh(bpb.fat_sz32);
    uint32_t data_start = fat_start + bpb.num_fats * fat_size;
    uint32_t cluster_size = le16toh(bpb.byts_per_sec) * bpb.sec_per_clus;

    uint32_t dir_cluster = resolve_path_to_cluster(file, path, &bpb, data_start, fat_start);
    if (dir_cluster == 0) {
        fprintf(stderr, "Directory not found: %s\n", path);
        fclose(file);
        return -1;
    }

    while (dir_cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start + (dir_cluster - 2) * bpb.sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);
        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t entry;
            fread(&entry, sizeof(entry), 1, file);
            if (entry.name[0] == 0)
                break;
            if (entry.name[0] == 0xE5)
                continue;

            char name[12] = {0};
            memcpy(name, entry.name, 11);
            for (int j = 10; j >= 0; j--) {
                if (name[j] != ' ')
                    break;
                name[j] = '\0';
            }

            printf("%s ", name);
        }

        putchar('\n');

        dir_cluster = read_fat_entry(file, dir_cluster, fat_start);
    }

    fclose(file);
    return 0;
}

int fat32_is_directory(const char *filesystem_path, const char *path) {
    FILE *file = fopen(filesystem_path, "rb");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam\n");
        return 0;
    }

    fat32_bpb_t bpb;
    if (fread(&bpb, sizeof(bpb), 1, file) != 1) {
        fprintf(stderr, "failed to read BPB");
        fclose(file);
        return 0;
    }

    uint32_t fat_start = le16toh(bpb.rsvd_sec_cnt);
    uint32_t fat_size = le32toh(bpb.fat_sz32);
    uint32_t data_start = fat_start + bpb.num_fats * fat_size;
    uint32_t cluster_size = le16toh(bpb.byts_per_sec) * bpb.sec_per_clus;

    // Special case: root
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0 || strlen(path) == 0) {
        fclose(file);
        return 1;
    }

    char *path_copy = strdup(path);
    char *entry_name = strdup(basename(path_copy));
    strcpy(path_copy, path);
    char *dir_path = dirname(path_copy);

    uint32_t dir_cluster = resolve_path_to_cluster(file, dir_path, &bpb, data_start, fat_start);
    if (dir_cluster == 0) {
        free(entry_name);
        free(path_copy);
        fclose(file);
        return 0;
    }

    uint8_t target_name[11];
    name_to_83(entry_name, target_name);

    while (dir_cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start + (dir_cluster - 2) * bpb.sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);

        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t entry;
            fread(&entry, sizeof(entry), 1, file);
            if (entry.name[0] == 0)
                break;
            if (entry.name[0] == 0xE5)
                continue;

            if (memcmp(entry.name, target_name, 11) == 0) {
                int result = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;
                free(entry_name);
                free(path_copy);
                fclose(file);
                return result;
            }
        }

        dir_cluster = read_fat_entry(file, dir_cluster, fat_start);
    }

    free(entry_name);
    free(path_copy);
    fclose(file);
    return 0;
}

int fat32_exists(const char *filesystem_path, const char *path) {
    FILE *file = fopen(filesystem_path, "rb");
    if (!file) {
        fprintf(stderr, "failed to open a filesysteam\n");
        return 0;
    }

    fat32_bpb_t bpb;
    if (fread(&bpb, sizeof(bpb), 1, file) != 1) {
        fprintf(stderr, "failed to read BPB");
        fclose(file);
        return 0;
    }

    uint32_t fat_start = le16toh(bpb.rsvd_sec_cnt);
    uint32_t fat_size = le32toh(bpb.fat_sz32);
    uint32_t data_start = fat_start + bpb.num_fats * fat_size;
    uint32_t cluster_size = le16toh(bpb.byts_per_sec) * bpb.sec_per_clus;

    // Special case: root
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0 || strlen(path) == 0) {
        fclose(file);
        return 1;
    }

    char *path_copy = strdup(path);
    char *entry_name = strdup(basename(path_copy));
    strcpy(path_copy, path);
    char *dir_path = dirname(path_copy);

    uint32_t dir_cluster = resolve_path_to_cluster(file, dir_path, &bpb, data_start, fat_start);
    if (dir_cluster == 0) {
        free(entry_name);
        free(path_copy);
        fclose(file);
        return 0;
    }

    uint8_t target_name[11];
    name_to_83(entry_name, target_name);

    while (dir_cluster < 0x0FFFFFF8) {
        uint32_t sector = data_start + (dir_cluster - 2) * bpb.sec_per_clus;
        fseek(file, sector * SECTOR_SIZE, SEEK_SET);

        for (int i = 0; i < cluster_size / sizeof(fat32_dir_entry_t); i++) {
            fat32_dir_entry_t entry;
            fread(&entry, sizeof(entry), 1, file);
            if (entry.name[0] == 0)
                break;
            if (entry.name[0] == 0xE5)
                continue;

            if (memcmp(entry.name, target_name, 11) == 0) {
                free(entry_name);
                free(path_copy);
                fclose(file);
                return 1;
            }
        }

        dir_cluster = read_fat_entry(file, dir_cluster, fat_start);
    }

    free(entry_name);
    free(path_copy);
    fclose(file);
    return 0;
}
