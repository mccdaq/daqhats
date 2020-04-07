#ifndef LOG_FILE_H_INCLUDED
#define LOG_FILE_H_INCLUDED

#include <sys/stat.h>
#include <string.h>
#include "data_logger.h"

FILE* log_file_ptr;
char csv_filename[512];

char* choose_log_file(GtkWidget *parent_window, char* default_file_name);
FILE* open_log_file (char* filename);
int write_log_file(FILE* log_file_ptr, double* read_buf,
    int numberOfSamplesPerChannel, int numberOfChannels);
int init_log_file(FILE* log_file_ptr);

#endif // LOG_FILE_H_INCLUDED