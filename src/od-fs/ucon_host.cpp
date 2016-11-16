/*
* UAE - The Un*x Amiga Emulator
*
* ucon.resource host code
*
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "uae.h"
#include "options.h"
#include "uae/memory.h"
#include "custom.h"
#include "newcpu.h"
#include "traps.h"
#include "autoconf.h"
#include "execlib.h"
#include "uaeresource.h"
#include "native2amiga.h"
#include "ucon.h"

//#define UCON_DEBUG
#ifdef UCON_DEBUG
#define DEBUG_LOG(format, ...) \
    do { \
        printf("ucon_host: " format, ##__VA_ARGS__); \
    } while(0)
#else
#define DEBUG_LOG(format, ...)
#endif

static uae_u32 ucon_task, read_sig_mask;

/* reader thread state */
static uae_thread_id reader_thread_id;
static uae_sem_t reader_sem;
static int stop_pipe[2];

static int stay;
static int init_done;
static uae_u8 *read_buf;
static uae_u32 read_maxlen;
static uae_u32 read_resptr;

static int read_fd = STDIN_FILENO;
static int write_fd = STDOUT_FILENO;

/* reader thread */
static void *reader_thread(void *arg)
{
    /* setup read fd set */
    fd_set read_fds;

    /* max fd num for select */
    int nfds = read_fd;
    if(stop_pipe[0] > nfds) {
        nfds = stop_pipe[0];
    }
    nfds++;

    /* main loop */
    DEBUG_LOG("t: reader thread begin\n");
    stay = 1;
    while(stay) {
        /* wait for next read packet */
        DEBUG_LOG("t: wait\n");
        uae_sem_wait(&reader_sem);
        if(!stay) {
            /* thread needs to shutdown */
            break;
        }

        /* prepare set */
        FD_ZERO(&read_fds);
        FD_SET(read_fd, &read_fds);
        FD_SET(stop_pipe[0], &read_fds);

        /* select read */
        DEBUG_LOG("t: select\n");
        int res = select(nfds, &read_fds, NULL, NULL, NULL);
        if(res > 0) {
            /* stop pipe triggered abort */
            if(FD_ISSET(stop_pipe[0], &read_fds)) {
                DEBUG_LOG("t: got stop pipe\n");
                break;
            }
            /* regular read */
            if(FD_ISSET(read_fd, &read_fds)) {
                /* perform read */
                DEBUG_LOG("t: read begin: read_buf=%p maxlen=%d\n", read_buf, read_maxlen);
                ssize_t result = read(STDIN_FILENO, read_buf, read_maxlen);

                /* store result length in ucon handler and send signal to handler task */
                uae_u32 res = (uae_u32)result;
                DEBUG_LOG("t: post signal: res=%08x -> addr=%08x\n", res, read_resptr);
                put_long(read_resptr, res);
                uae_Signal(ucon_task, read_sig_mask);
            }
        }
    }
    DEBUG_LOG("t: reader thread end\n");
    return 0;
}

int ucon_host_init(uae_u32 ucon_task_, uae_u32 read_sig_mask_)
{
    ucon_task = ucon_task_;
    read_sig_mask = read_sig_mask_;

    /* remove old thread first */
    if(init_done) {
        DEBUG_LOG("init: cleanup first\n");
        ucon_host_exit();
    }

    /* init sem */
    if(uae_sem_init (&reader_sem, 0, 0)) {
        DEBUG_LOG("init: sem failed!\n");
        return 1;
    }

    /* stop pipe */
    if(pipe(stop_pipe)) {
        DEBUG_LOG("init: pipe failed!\n");
        uae_sem_destroy(&reader_sem);
        return 1;
    }

    /* start thread */
    if(uae_start_thread("ucon_reader", reader_thread, NULL, &reader_thread_id) == BAD_THREAD) {
        DEBUG_LOG("init: thread failed!\n");
        uae_sem_destroy(&reader_sem);
        close(stop_pipe[0]);
        close(stop_pipe[1]);
        return 1;
    }

    init_done = 1;
    DEBUG_LOG("init: done\n");
    return 0;
}

void ucon_host_exit(void)
{
    if(!init_done) {
        return;
    }

    DEBUG_LOG("exit: shutdown thread\n");

    /* clear flag */
    stay = 0;
    /* trigger reader sem */
    uae_sem_post(&reader_sem);
    /* trigger stop pipe */
    char dummy = 0;
    write(stop_pipe[1], &dummy, 1);
    /* wait for shutdown */
    uae_wait_thread(reader_thread_id);

    uae_sem_destroy(&reader_sem);

    close(stop_pipe[0]);
    close(stop_pipe[1]);

    init_done = 0;

    DEBUG_LOG("exit: done\n");
}

void ucon_host_cleanup(void)
{
    ucon_host_exit();
}

void ucon_host_read(uae_u8 *ptr, uae_u32 length, uae_u32 res_ptr)
{
    /* report to reader thread */
    read_buf = ptr;
    read_maxlen = length;
    read_resptr = res_ptr;
    uae_sem_post(&reader_sem);
}

void ucon_host_write(uae_u8 *ptr, uae_u32 length)
{
    write(write_fd, ptr, length);
}
