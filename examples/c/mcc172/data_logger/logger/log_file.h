#ifndef LOG_FILE_H_INCLUDED
#define LOG_FILE_H_INCLUDED

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "globals.h"

extern FILE* log_file_ptr;

extern void get_path_and_filename(char* full_path, char* path,
    char* filename);
extern char* choose_log_file(GtkWidget *parent_window,
    char* default_file_name);
extern FILE* open_log_file (char* filename);
extern int write_log_file(FILE* log_file_ptr, double* read_buf,
    int numberOfSamplesPerChannel, int numberOfChannels);
extern int init_log_file(FILE* log_file_ptr, uint8_t current_channel_mask);

#endif // LOG_FILE_H_INCLUDED
