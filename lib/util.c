/*
*   util.c
*   author Measurement Computing Corp.
*   brief This file contains utility functions for the MCC HATs.
*
*   date 06/29/2018
*/
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "daqhats.h"
#include "util.h"
#include "gpio.h"

// *****************************************************************************
// Constants

//#define USE_SEMAPHORES

#define LOCK_RETRY_TIME_S       5       // 5 seconds
#define LOCK_RETRY_TIME         (LOCK_RETRY_TIME_S*SEC)

// Raspberry Pi HAT eeprom constants

// Atom types
#define ATOM_INVALID_TYPE 	0x0000
#define ATOM_VENDOR_TYPE    0x0001
#define ATOM_GPIO_TYPE	    0x0002
#define ATOM_DT_TYPE 		0x0003
#define ATOM_CUSTOM_TYPE	0x0004
#define ATOM_HINVALID_TYPE  0xffff

#define ATOM_VENDOR_NUM     0x0000
#define ATOM_GPIO_NUM       0x0001
#define ATOM_DT_NUM         0x0002

// minimal sizes of data structures
#define HEADER_SIZE     12
#define ATOM_SIZE       10
#define VENDOR_SIZE     22
#define GPIO_SIZE       30
#define CRC_SIZE        2

#define GPIO_MIN        2
#define GPIO_COUNT      28

#define SIGNATURE       0x69502D52  // "R-Pi" in ASCII
#define FORMAT_VERSION  0x01

// Board address GPIO pin numbers
#define ADDR0_GPIO              12
#define ADDR1_GPIO              13
#define ADDR2_GPIO              26

#define IRQ_GPIO                21

// EEPROM header structure
struct _Header
{
    uint32_t signature;
    unsigned char ver;
    unsigned char res;
    uint16_t numatoms;
    uint32_t eeplen;
};

// Atom structure
struct _Atom
{
    uint16_t type;
    uint16_t count;
    uint32_t dlen;
    char* data;
    uint16_t crc16;
};

// Vendor info atom data
struct _VendorInfo
{
    uint32_t serial_1; //least significant
    uint32_t serial_2;
    uint32_t serial_3;
    uint32_t serial_4; //most significant
    uint16_t pid;
    uint16_t pver;
    unsigned char vslen;
    unsigned char pslen;
    char* vstr;
    char* pstr;
};

#ifdef USE_SEMAPHORES
// named semaphores
static const char* const INIT_MUTEX = "/mcc_daqhats_init_mutex";
// shared memory
static const char* const SHARED_DATA = "/mcc_daqhats_mutex_shm";
#else
// lock files for synchronization
static const char* const SPI_LOCKFILE = "/tmp/.mcc_spi_lockfile";

static const char* const BOARD_LOCKFILES[] =
{
    "/tmp/.mcc_hat_lockfile_0",
    "/tmp/.mcc_hat_lockfile_1",
    "/tmp/.mcc_hat_lockfile_2",
    "/tmp/.mcc_hat_lockfile_3",
    "/tmp/.mcc_hat_lockfile_4",
    "/tmp/.mcc_hat_lockfile_5",
    "/tmp/.mcc_hat_lockfile_6",
    "/tmp/.mcc_hat_lockfile_7",
};
#endif

static const char* const HAT_SETTINGS_DIR = "/etc/mcc/hats";
static const char* const SYS_HAT_DIR = "/proc/device-tree/hat";
static const char* const VENDOR_NAME = "Measurement Computing Corp.";

static const char* UNDEFINED_ERROR_MESSAGE =
    "An unknown error occurred.";

static const char* HAT_ERROR_MESSAGES[] =
{
    "Success.",
    "An incorrect parameter was passed to the function.",
    "The device is busy.",
    "There was a timeout accessing a resource.",
    "There was a timeout while obtaining a resource lock.",
    "The device at the specified address is not the correct type.",
    "A needed resource was not available.",
    "Could not communicate with the device."
};

// *****************************************************************************
// Variables
static bool _address_initialized = false;
#ifdef USE_SEMAPHORES
static int shm_fd = -1;
typedef struct
{
    pthread_mutex_t spi_mutex;
    pthread_mutex_t board_mutex[MAX_NUMBER_HATS];
} shared_data_struct;

static shared_data_struct* shared_data = NULL;
#else
static int spi_lockfile;
static int board_lockfiles[MAX_NUMBER_HATS];
static pthread_mutex_t spi_mutex;
static pthread_mutex_t board_mutex[MAX_NUMBER_HATS];
#endif

// *****************************************************************************
// Local Functions

/******************************************************************************
  Initializes the GPIO pins used for board addressing.
 *****************************************************************************/
void _address_init(void)
{
    if (!_address_initialized)
    {
        gpio_init();
        _address_initialized = true;
    }
}


void _lock_init(void)
{
    int i;

#ifdef USE_SEMAPHORES
    // Use a named semaphore to synchronize shared memory init.  The semaphore could get hung
    // if a process is killed while it is open, so we only use it for a brief time during init.
    sem_t* mutex = sem_open(INIT_MUTEX, O_CREAT, 0666, 1);
    if (SEM_FAILED == mutex)
    {
        printf("_lock_init: mutex sem_open failed, errno %d\n", errno);
        return;
    }

    // acquire lock
    sem_wait(mutex);

    // Use pthread_mutexes in shared memory for the SPI and board access synchronization. Set them
    // to process shared and robust so they can be shared between different processes and can be
    // cleaned up if the owning process dies without releasing it.  A posix named semaphore would
    // be left locked if the owning process dies without releasing it.

    // Open shared memory.  Try to open in exclusive mode first to see if already exists.
    int mode = 0666;
    // When creating it will use the umask of the program, so override those so the resulting
    // file can be shared with different users, root, etc.
    mode_t old_umask = umask(0);
    shm_fd = shm_open(SHARED_DATA, O_CREAT | O_RDWR | O_EXCL, mode);
    umask(old_umask);

    if (shm_fd < 0)
    {
        // It already exists, so don't initialize it.
        printf("_lock_init: shm exists\n");
        shm_fd = shm_open(SHARED_DATA, O_RDWR, mode);
        if (shm_fd < 0)
        {
            printf("_lock_init: shm_open failed, errno %d\n", errno);
            sem_post(mutex);
            sem_close(mutex);
            sem_unlink(INIT_MUTEX);
            return;
        }

        // mmap the data
        shared_data = (shared_data_struct*)mmap(NULL,
            sizeof(shared_data_struct),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            shm_fd,
            0);
        if (MAP_FAILED == shared_data)
        {
            printf("_lock_init: shared_data mmap failed\n");
            close(shm_fd);
            shm_fd = -1;
            sem_post(mutex);
            sem_close(mutex);
            sem_unlink(INIT_MUTEX);
            return;
        }
    }
    else
    {
        // It did not exist so we need to initialize it.
        printf("_lock_init: shm does not exist\n");
        if (ftruncate(shm_fd, sizeof(shared_data_struct)) == -1)
        {
            printf("_lock_init: ftruncate failed\n");
            close(shm_fd);
            shm_fd = -1;
            sem_post(mutex);
            sem_close(mutex);
            sem_unlink(INIT_MUTEX);
            return;
        }
        shared_data = (shared_data_struct*)mmap(NULL,
            sizeof(shared_data_struct),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            shm_fd,
            0);
        if (MAP_FAILED == shared_data)
        {
            printf("_lock_init: shared_data mmap failed\n");
            close(shm_fd);
            shm_fd = -1;
            sem_post(mutex);
            sem_close(mutex);
            sem_unlink(INIT_MUTEX);
            return ;
        }

        // init the mutexes
        pthread_mutexattr_t mattr;
        pthread_mutexattr_init(&mattr);
        // set to process shared
        pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        // set to robust
        pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
        // apply to all mutexes
        pthread_mutex_init(&shared_data->spi_mutex, &mattr);
        for (i = 0; i < MAX_NUMBER_HATS; i++)
        {
            pthread_mutex_init(&shared_data->board_mutex[i], &mattr);
        }
        pthread_mutexattr_destroy(&mattr);
    }

    // release the init mutex
    sem_post(mutex);
    sem_close(mutex);
    sem_unlink(INIT_MUTEX);
#else
    // set umask so we can set permission to 0666; otherwise, if run as root it
    // will leave lockfiles that normal users cannot open
    mode_t mask = umask(0111);

    // init spi lockfile
    spi_lockfile = open(SPI_LOCKFILE,
        O_CREAT     |   // create file if it does not exist
        O_WRONLY    |   // open for write access only
        O_CLOEXEC,      // close on execute
        S_IRUSR     |   // user permission: read/write
        S_IWUSR     |
        S_IRGRP     |   // group permission: read/write
        S_IWGRP     |
        S_IROTH     |   // other permission: read/write
        S_IWOTH);

    // revert umask
    umask(mask);

    // Multiple threads in the same process will share the above file
    // descriptor, so flock() will not work. Use a mutex for this scenario.
    pthread_mutex_init(&spi_mutex, NULL);

    for (i = 0; i < MAX_NUMBER_HATS; i++)
    {
        pthread_mutex_init(&board_mutex[i], NULL);
    }
#endif
}

void _lock_fini(void)
{
    int i;

#ifdef USE_SEMAPHORES
    if (NULL == shared_data)
    {
        return;
    }
    pthread_mutex_unlock(&shared_data->spi_mutex);
    for (i = 0; i < MAX_NUMBER_HATS; i++)
    {
        pthread_mutex_unlock(&shared_data->board_mutex[i]);
    }
    munmap(shared_data, sizeof(shared_data_struct));
    shared_data = NULL;
    if (shm_fd > -1)
    {
        // close the shared memory
        close(shm_fd);
        shm_fd = -1;
    }
#else
    close(spi_lockfile);
    pthread_mutex_destroy(&spi_mutex);
    for (i = 0; i < MAX_NUMBER_HATS; i++)
    {
        pthread_mutex_destroy(&board_mutex[i]);
    }
#endif
}

// *****************************************************************************
// Global Functions

// library constructor / destructor
void __attribute__ ((constructor)) init(void)
{
    // initialization
    _address_init();

    _lock_init();
}

void __attribute__ ((destructor)) fini(void)
{
    // cleanup
    gpio_close();

    _lock_fini();
}


/******************************************************************************
  Sets the specified address on the GPIO address pins.
 *****************************************************************************/
void _set_address(uint8_t address)
{
    if (address < MAX_NUMBER_HATS)
    {
        gpio_set_output(ADDR0_GPIO, address & 0x01);
        gpio_set_output(ADDR1_GPIO, address & 0x02);
        gpio_set_output(ADDR2_GPIO, address & 0x04);
    }
}

void _free_address(void)
{
    gpio_release(ADDR0_GPIO);
    gpio_release(ADDR1_GPIO);
    gpio_release(ADDR2_GPIO);
}

/******************************************************************************
  Returns the absolute difference in microseconds between two struct timeval
  values.
 *****************************************************************************/
uint32_t _difftime_us(struct timespec* start, struct timespec* end)
{
    int32_t diff;

    if (!start || !end)
        return 0;

    diff = (end->tv_sec*1e6 + end->tv_nsec/1000) - (start->tv_sec*1e6 +
        start->tv_nsec/1000);
    if (diff < 0)
        return (uint32_t)-diff;
    else
        return (uint32_t)diff;
}

uint32_t _difftime_ms(struct timespec* start, struct timespec* end)
{
    // return absolute difference in milliseconds
    int32_t diff;

    if (!start || !end)
        return 0;

    diff = (end->tv_sec*1e3 + end->tv_nsec/1e6) - (start->tv_sec*1e3 +
        start->tv_nsec/1e6);
    if (diff < 0)
        return (uint32_t)-diff;
    else
        return (uint32_t)diff;
}

/******************************************************************************
  Control access to the SPI bus by multiple processes.

  There can be multiple boards in a system with multiple processes
  communicating with the boards, and all will use a single SPI port.
  Keep the SPI port locked to a single process for the duration of
  the transaction with a lock file.  All MCC HAT libraries will use
  this same lock file. This avoids the issue with named semaphores
  where the semaphore could be stuck at 0 if a process receives
  SIGKILL before incrementing the semaphore.  If the process dies the
  file handle is automatically released.

  The flock() mechanism does not work for multiple threads within the same
  process - the same file descriptor is shared among all the threads so once one
  of them has a lock then flock() will return successfully for any other thread
  that requests the lock.  We use a pthread_mutex to control cross-thread
  locking.

  Return: int, file descriptor (RESULT_TIMEOUT for time out obtaining lock)
 *****************************************************************************/
int _obtain_lock(void)
{
#ifdef USE_SEMAPHORES
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += LOCK_RETRY_TIME_S;

    int s = pthread_mutex_timedlock(&shared_data->spi_mutex, &ts);

    switch (s)
    {
    case 0:
        // success
        return RESULT_SUCCESS;
    case EOWNERDEAD:
        // The process that was holding the mutex died.  Assume we can recover it.
        printf("_obtain_lock: inconsistent spi_mutex reported\n");
        pthread_mutex_consistent(&shared_data->spi_mutex);
        return RESULT_SUCCESS;
        break;
    case ETIMEDOUT:
        // Timeout
        return RESULT_TIMEOUT;
    default:
        printf("_obtain_lock error %d\n", s);
        return RESULT_TIMEOUT;
    }

#else
    bool locked;
    struct timespec start_time;
    struct timespec current_time;
    int test;

    // Block until lock obtained, but allow context switching with usleep().
    // Time out after 5 seconds
    locked = false;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    do
    {
        // the file was opened, now lock it so no other process can open it
        if ((test = flock(spi_lockfile, LOCK_EX | LOCK_NB)) == 0)
        {
            locked = true;
        }
        else
        {
            // could not get a lock, so wait and retry
            usleep(10);
            clock_gettime(CLOCK_MONOTONIC, &current_time);
        }
    } while (!locked &&
        (_difftime_us(&start_time, &current_time) < LOCK_RETRY_TIME));

    if (!locked)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_TIMEOUT;
    }

    // file locking will not work for multiple threads in the same process, so
    // use a mutex as well
    pthread_mutex_lock(&spi_mutex);

    return spi_lockfile;
#endif
}

/******************************************************************************
  Use lock files to control access to the HAT boards by multiple processes.

  Not used by all board types, just when there is a lengthy process involving
  a board resource that cannot be interrupted. For example, setting up an
  MCC 134 ADC conversion then waiting for the results (~50ms); use a lock to
  prevent another process from changing the ADC config.

  The flock() mechanism does not work for multiple threads within the same
  process - the same file descriptor is shared among all the threads so once one
  of them has a lock then flock() will return successfully for any other thread
  that requests the lock.  We use a pthread_mutex to control cross-thread
  locking.

  Return: int status
 *****************************************************************************/
int _obtain_board_lock(uint8_t address)
{
#ifdef USE_SEMAPHORES
    if (address >= MAX_NUMBER_HATS)
    {
        printf("_obtain_board_lock: Invalid board address %d\n", address);
        return RESULT_BAD_PARAMETER;
    }

    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += LOCK_RETRY_TIME_S;

    int s = pthread_mutex_timedlock(&shared_data->board_mutex[address], &ts);

    switch (s)
    {
    case 0:
        // success
        return RESULT_SUCCESS;
    case EOWNERDEAD:
        // The process that was holding the mutex died.  Assume we can recover it.
        printf("_obtain_board_lock %d: inconsistent spi_mutex reported\n", address);
        pthread_mutex_consistent(&shared_data->spi_mutex);
        return RESULT_SUCCESS;
        break;
    case ETIMEDOUT:
        // Timeout
        return RESULT_TIMEOUT;
    default:
        printf("_obtain_board_lock %d error %d\n", address, s);
        return RESULT_TIMEOUT;
    }

#else
    bool locked;
    int lock_fd;
    struct timespec start_time;
    struct timespec current_time;
    char* filename;
    mode_t mask;

    if (address >= MAX_NUMBER_HATS)
    {
        return RESULT_BAD_PARAMETER;
    }

    filename = (char*)BOARD_LOCKFILES[address];

    // Block until lock obtained, but allow context switching with usleep().
    // Time out after 5 seconds
    locked = false;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    // set umask so we can set permission to 0666; otherwise, if run as root it
    // will leave lockfiles that normal users cannot open
    mask = umask(0111);
    do
    {
        lock_fd = open(filename,
            O_CREAT     |   // create file if it does not exist
            O_WRONLY    |   // open for write access only
            O_CLOEXEC,      // close on execute
            S_IRUSR     |   // user permission: read/write
            S_IWUSR     |
            S_IRGRP     |   // group permission: read/write
            S_IWGRP     |
            S_IROTH     |   // other permission: read/write
            S_IWOTH);

        if (lock_fd != -1)
        {
            // the file was opened, now lock it so no other process can open it
            if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0)
            {
                locked = true;
            }
            else
            {
                // could not get a lock, so wait and retry
                close(lock_fd);
                usleep(10);
                clock_gettime(CLOCK_MONOTONIC, &current_time);
            }
        }
        else
        {
            // could not open the lock file, so wait and retry
            usleep(10);
            clock_gettime(CLOCK_MONOTONIC, &current_time);
        }
    } while (!locked &&
        (_difftime_us(&start_time, &current_time) < LOCK_RETRY_TIME));

    // revert umask
    umask(mask);

    if (!locked)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_TIMEOUT;
    }

    board_lockfiles[address] = lock_fd;

    // file locking will not work for multiple threads in the same process, so
    // use a mutex as well
    pthread_mutex_lock(&board_mutex[address]);

    return RESULT_SUCCESS;
#endif
}

/******************************************************************************
  Release a previously obtained SPI lock.
 *****************************************************************************/
void _release_lock(int lock_fd)
{
#ifdef USE_SEMAPHORES
    if (NULL == shared_data)
    {
        return;
    }

    (void)(lock_fd);

    pthread_mutex_unlock(&shared_data->spi_mutex);
#else
    flock(lock_fd, LOCK_UN);
    pthread_mutex_unlock(&spi_mutex);
#endif
}


/******************************************************************************
  Release a previously obtained board lock.
 *****************************************************************************/
void _release_board_lock(uint8_t address)
{
    if (address >= MAX_NUMBER_HATS)
    {
        printf("_release_board_lock: Invalid board address %d\n", address);
        return;
    }

#ifdef USE_SEMAPHORES
    pthread_mutex_unlock(&shared_data->board_mutex[address]);
#else
    flock(board_lockfiles[address], LOCK_UN);
    close(board_lockfiles[address]);
    pthread_mutex_unlock(&board_mutex[address]);
#endif
}


/******************************************************************************
  List HAT boards attached to the Pi.
 *****************************************************************************/
int hat_list(uint16_t filter_id, struct HatInfo* pList)
{
    uint8_t address;
    char filename[256];
    char temp[256];
    char* ptr;
    int eeprom_fd;
    uint8_t count;
    struct _Header header;
    struct _Atom atom;
    struct _VendorInfo vinf;
    uint16_t id;
    uint16_t pver;

    count = 0;

    // EEPROM 0 will always use the built-in OS support to prevent caching info
    // for a single board that is swapped out (such as during manufacturing
    // test.)

    sprintf(filename, "%s/vendor", SYS_HAT_DIR);
    // open vendor file and compare against VENDOR_NAME
    eeprom_fd = -1;
    if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
        (read(eeprom_fd, &temp, 256) > 0) &&
        (strcmp(temp, VENDOR_NAME) == 0))
    {
        // match, so read the pid
        close(eeprom_fd);
        eeprom_fd = -1;
        sprintf(filename, "%s/product_id", SYS_HAT_DIR);
        if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
            (read(eeprom_fd, &temp, 256) > 0))
        {
            // convert string to int
            id = (uint16_t)strtoul(temp, &ptr, 16);
            if ((filter_id == 0) ||
                (filter_id == id))
            {
                // pid matches, save the info
                close(eeprom_fd);
                eeprom_fd = -1;
                count = 1;
                if (pList != NULL)
                {
                    pList[0].address = 0;
                    pList[0].id = id;

                    // read the version
                    sprintf(filename, "%s/product_ver", SYS_HAT_DIR);
                    if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                        (read(eeprom_fd, &temp, 256) > 0))
                    {
                        pver = (uint16_t)strtoul(temp, &ptr, 16);
                        pList[0].version = pver;

                        // read the product string
                        close(eeprom_fd);
                        eeprom_fd = -1;
                        sprintf(filename, "%s/product", SYS_HAT_DIR);
                        if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                            (read(eeprom_fd, &temp, 256) > 0))
                        {
                            strcpy(pList[0].product_name, temp);
                        }
                    }
                }
            }
        }
    }
    if (eeprom_fd != -1)
        close(eeprom_fd);

    // Boards 1-7 will be supported with the read_eeproms utility that copies
    // the EEPROM contents to /etc/mcc/hats
    for (address = 1; address < MAX_NUMBER_HATS; address++)
    {
        // look for HAT eeprom.bin files in /etc/mcc/hats
        sprintf(filename, "%s/eeprom_%d.bin", HAT_SETTINGS_DIR, address);

        // open file
        if ((eeprom_fd = open(filename, O_RDONLY)) == -1)
            continue;

        // read hat header
        if (read(eeprom_fd, &header, HEADER_SIZE) != HEADER_SIZE)
        {
            close(eeprom_fd);
            continue;
        }

        // check the header
        if ((header.signature != SIGNATURE) ||
            (header.ver != FORMAT_VERSION) ||
            (header.numatoms < 1))
        {
            close(eeprom_fd);
            continue;
        }

        // header is valid, read the vendor atom
        if (read(eeprom_fd, &atom, ATOM_SIZE-CRC_SIZE) != ATOM_SIZE-CRC_SIZE)
        {
            close(eeprom_fd);
            continue;
        }

        // vendor atom must be the first atom
        if (atom.type != ATOM_VENDOR_TYPE)
        {
            close(eeprom_fd);
            continue;
        }

        // read the vendor info
        if (read(eeprom_fd, &vinf, VENDOR_SIZE) != VENDOR_SIZE)
        {
            close(eeprom_fd);
            continue;
        }

        // allocate and read the vendor and product strings
        vinf.vstr = (char*)malloc(vinf.vslen+1);
        vinf.pstr = (char*)malloc(vinf.pslen+1);
        if (read(eeprom_fd, vinf.vstr, vinf.vslen) != vinf.vslen)
        {
            free(vinf.vstr);
            free(vinf.pstr);
            close(eeprom_fd);
            continue;
        }
        vinf.vstr[vinf.vslen] = '\0';
        if (read(eeprom_fd, vinf.pstr, vinf.pslen) != vinf.pslen)
        {
            free(vinf.vstr);
            free(vinf.pstr);
            close(eeprom_fd);
            continue;
        }
        vinf.pstr[vinf.pslen] = '\0';

        // compare the vendor string to the desired string
        if ((strcmp(vinf.vstr, VENDOR_NAME) == 0) &&
            (filter_id == 0 ? true : (vinf.pid == filter_id)))
        {
            // vendor is MCC, save the entry if it matches the PID filter
            if (pList != NULL)
            {
                pList[count].address = address;
                pList[count].id = vinf.pid;
                pList[count].version = vinf.pver;
                strncpy(pList[count].product_name, vinf.pstr, 256-1);
            }
            count++;
        }

        free(vinf.vstr);
        free(vinf.pstr);
        close(eeprom_fd);
    }

    return count;
}

/******************************************************************************
  Return factory data for a specific HAT board as a jSON string.
 *****************************************************************************/
int _hat_info(uint8_t address, struct HatInfo* entry, char* pData,
    uint16_t* pSize)
{
    bool found_custom;
    bool found_vendor;
    bool error;
    char filename[256];
    char temp[256];
    uint8_t atom_num;
    uint16_t custom_size;
    int eeprom_fd;
    struct _Header header;
    struct _Atom atom;
    struct _VendorInfo vinf;
    struct stat filestat;

    if (address >= MAX_NUMBER_HATS)
    {
        return RESULT_BAD_PARAMETER;
    }

    found_vendor = false;

    if (address == 0)
    {
        // use the info in /proc/device-tree/hat if possible
        sprintf(filename, "%s/vendor", SYS_HAT_DIR);
        // open vendor file and compare against VENDOR_NAME
        eeprom_fd = -1;
        if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
            (read(eeprom_fd, temp, 256) > 0) &&
            (strcmp(temp, VENDOR_NAME) == 0))
        {
            // name matches
            found_vendor = true;
            close(eeprom_fd);
            eeprom_fd = -1;

            if (entry)
            {
                entry->address = 0;

                // read the device info
                sprintf(filename, "%s/product_id", SYS_HAT_DIR);
                if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                    (read(eeprom_fd, temp, 256) > 0))
                {
                    // convert string to int
                    entry->id = (uint16_t)strtoul(temp, NULL, 16);
                    close(eeprom_fd);
                    eeprom_fd = -1;
                }

                sprintf(filename, "%s/product_ver", SYS_HAT_DIR);
                if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                    (read(eeprom_fd, temp, 256) > 0))
                {
                    entry->version = (uint16_t)strtoul(temp, NULL, 16);
                    close(eeprom_fd);
                    eeprom_fd = -1;
                }

                sprintf(filename, "%s/product", SYS_HAT_DIR);
                if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                    (read(eeprom_fd, temp, 256) > 0))
                {
                    strcpy(entry->product_name, temp);
                    close(eeprom_fd);
                    eeprom_fd = -1;
                }
            }

            // get the custom size
            custom_size = 0;
            sprintf(filename, "%s/custom_0", SYS_HAT_DIR);
            if (stat(filename, &filestat) == 0)
            {
                custom_size = filestat.st_size;

                // read the file?
                if (pData)
                {
                    if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
                        (read(eeprom_fd, pData, custom_size) > 0))
                    {
                        close(eeprom_fd);
                        eeprom_fd = -1;
                        pData[custom_size] = '\0';  // null terminate the string
                    }
                }
            }

            if (pSize)
            {
                *pSize = custom_size + 1;   // add room for null
            }
        }
        if (eeprom_fd != -1)
            close(eeprom_fd);
    }

    if (!found_vendor)
    {
        // try the files in /etc/mcc/hats
        found_custom = false;
        sprintf(filename, "%s/eeprom_%d.bin", HAT_SETTINGS_DIR, address);
        eeprom_fd = -1;
        if (((eeprom_fd = open(filename, O_RDONLY)) != -1) &&
            (read(eeprom_fd, &header, HEADER_SIZE) == HEADER_SIZE) &&
            (header.signature == SIGNATURE) &&
            (header.ver == FORMAT_VERSION) &&
            (header.numatoms > 1))
        {
            // header is valid, read atoms
            atom_num = 0;
            error = false;
            while (!(found_custom && found_vendor) &&
                (atom_num < header.numatoms) &&
                !error)
            {
                if (read(eeprom_fd, &atom, ATOM_SIZE-CRC_SIZE) ==
                    ATOM_SIZE-CRC_SIZE)
                {
                    // process the atom by type
                    if (atom.type == ATOM_VENDOR_TYPE)
                    {
                        // get the vendor info
                        if (read(eeprom_fd, &vinf, VENDOR_SIZE) == VENDOR_SIZE)
                        {
                            vinf.vstr = (char*)malloc(vinf.vslen+1);
                            vinf.pstr = (char*)malloc(vinf.pslen+1);
                            if (read(eeprom_fd, vinf.vstr, vinf.vslen) !=
                                vinf.vslen)
                            {
                                free(vinf.vstr);
                                free(vinf.pstr);
                                error = true;
                                continue;
                            }
                            vinf.vstr[vinf.vslen] = '\0';
                            if (read(eeprom_fd, vinf.pstr, vinf.pslen) !=
                                vinf.pslen)
                            {
                                free(vinf.vstr);
                                free(vinf.pstr);
                                error = true;
                                continue;
                            }
                            vinf.pstr[vinf.pslen] = '\0';
                            lseek(eeprom_fd, CRC_SIZE, SEEK_CUR);

                            if (strcmp(vinf.vstr, VENDOR_NAME) == 0)
                            {
                                // valid vendor, save the info
                                if (entry != NULL)
                                {
                                    entry->address = address;
                                    entry->id = vinf.pid;
                                    entry->version = vinf.pver;
                                    strncpy(entry->product_name, vinf.pstr,
                                        256-1);
                                }
                                found_vendor = true;
                            }
                            else
                            {
                                // invalid vendor
                                error = true;
                            }
                            free(vinf.vstr);
                            free(vinf.pstr);
                        }
                        else
                        {
                            error = true;
                        }
                        atom_num++;
                    }
                    else if (atom.type == ATOM_CUSTOM_TYPE)
                    {
                        // found the custom info
                        found_custom = true;
                    }
                    else
                    {
                        // skip
                        lseek(eeprom_fd, atom.dlen, SEEK_CUR);
                        atom_num++;
                    }
                }
                else
                {
                    error = true;
                }
            }
        }
        else
        {
            // no board info found
            return RESULT_BAD_PARAMETER;
        }

        custom_size = 0;
        if (found_custom)
        {
            // read the JSON custom data
            custom_size = atom.dlen - CRC_SIZE;
            if (pData)
            {
                if (read(eeprom_fd, pData, custom_size) != custom_size)
                {
                    custom_size = 0;
                }
                pData[custom_size] = '\0';  // add null termination
            }
        }
        if (pSize)
        {
            *pSize = custom_size + 1;   // add room for null
        }
    }

    return RESULT_SUCCESS;
}

/******************************************************************************
  Return an error description string.
 *****************************************************************************/
const char* hat_error_message(int result)
{
    switch (result)
    {
    case RESULT_SUCCESS:
        return HAT_ERROR_MESSAGES[0];
    case RESULT_BAD_PARAMETER:
        return HAT_ERROR_MESSAGES[1];
    case RESULT_BUSY:
        return HAT_ERROR_MESSAGES[2];
    case RESULT_TIMEOUT:
        return HAT_ERROR_MESSAGES[3];
    case RESULT_LOCK_TIMEOUT:
        return HAT_ERROR_MESSAGES[4];
    case RESULT_INVALID_DEVICE:
        return HAT_ERROR_MESSAGES[5];
    case RESULT_RESOURCE_UNAVAIL:
        return HAT_ERROR_MESSAGES[6];
    case RESULT_COMMS_FAILURE:
        return HAT_ERROR_MESSAGES[7];
    case RESULT_UNDEFINED:
    default:
        return UNDEFINED_ERROR_MESSAGE;
    }
}

/******************************************************************************
  Return the interrupt status.
 *****************************************************************************/
int hat_interrupt_state(void)
{
    gpio_input(IRQ_GPIO);
    int val = gpio_read(IRQ_GPIO);
    gpio_release(IRQ_GPIO);
    if (val == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/******************************************************************************
  Wait for an interrupt to occur, with timeout.
 *****************************************************************************/
int hat_wait_for_interrupt(int timeout)
{
    // wait for IRQ_GPIO to go low, with timeout
    switch (gpio_wait_for_low(IRQ_GPIO, timeout))
    {
    case -1:    // error
        return RESULT_UNDEFINED;
    case 0:     // timeout
        return RESULT_TIMEOUT;
    default:    // success
        return RESULT_SUCCESS;
    }
}

/******************************************************************************
  Create an interrupt handler that calls the user-provided callback function.
 *****************************************************************************/
int hat_interrupt_callback_enable(void (*function)(void*), void* data)
{
    switch (gpio_interrupt_callback(IRQ_GPIO, 0, function, data))
    {
    case -1:    // error
        return RESULT_UNDEFINED;
    case 0:
    default:
        return RESULT_SUCCESS;
    }
}

/******************************************************************************
  Disable an interrupt callback.
 *****************************************************************************/
int hat_interrupt_callback_disable(void)
{
    switch (gpio_interrupt_callback(IRQ_GPIO, 3, NULL, NULL))
    {
    case -1:    // error
        return RESULT_UNDEFINED;
    case 0:
    default:
        return RESULT_SUCCESS;
    }
}
