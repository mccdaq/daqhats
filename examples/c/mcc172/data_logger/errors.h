#ifndef ERRORS_H_INCLUDED
#define ERRORS_H_INCLUDED

// Logger application error codes
#define NO_HAT_DEVICES_FOUND        -100
#define UNABLE_TO_OPEN_FILE         -101
#define MAXIMUM_FILE_SIZE_EXCEEDED  -102
#define THREAD_ERROR                -103
#define UNKNOWN_ERROR               -999

// Status errors
#define HW_OVERRUN                  -200
#define BUFFER_OVERRUN              -201

gboolean show_error(int* error_code_ptr);
void show_error_in_main_thread(int error_code);
#endif // ERRORS_H_INCLUDED

