#ifndef ERRORS_H_INCLUDED
#define ERRORS_H_INCLUDED

// Logger application error codes
#define NO_HAT_DEVICES_FOUND        -100
#define UNABLE_TO_OPEN_FILE         -101
#define MAXIMUM_FILE_SIZE_EXCEEDED  -102
#define UNKNOWN_ERROR               -999


// Status errors
#define HW_OVERRUN                  -200
#define BUFFER_OVERRUN              -201

extern const char* get_mcc118_error_message(int error_code);
extern gboolean show_error(const char* errmsg);
extern void show_mcc118_error_main_thread(gpointer error_code);
extern void show_mcc118_error(int error_code);

#endif // ERRORS_H_INCLUDED

