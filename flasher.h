
/*
    flasher.h - Copyright (C) 2002 Murray Nesbitt (websrc@nesbitt.ca)

    This program is protected and licensed under the following terms and
    conditions: 1) it may not be redistributed in binary form without the
    explicit permission of the author; 2) when redistributed in source
    form, in whole or in part, this complete copyright statement must
    remain intact.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <syslog.h>
#include <string.h> /* strtok() */

/* Time in seconds between updates. */
#define SLEEP	60
/* Delay between flashes in microseconds. */
#define FLASH_DELAY	200000
/* How long the flash lasts in microseconds. */
#define FLASH_ONTIME	200000
/* Interval between flash groups in seconds. */
#define FLASH_SLEEP 2

/* You shouldn't need to change anything beyond this point. */

#if defined(__linux__)
  #include <sys/kd.h>
  #define GET_LED_STATE KDGETLED
  #define SET_LED_STATE KDSETLED
  #define GET_KB_STATE KDGKBLED

#elif defined(__FreeBSD__)
  #include <sys/kbio.h>
  #define GET_LED_STATE KDGETLED
  #define SET_LED_STATE KDSETLED
  #define GET_KB_STATE KDGKBSTATE

#elif defined(__sun)
  #include <sys/kbio.h>
  #include <sys/kbd.h>
  #define GET_LED_STATE KIOCGLED
  #define SET_LED_STATE KIOCSLED
  #define LED_SCR LED_SCROLL_LOCK
  #define LED_NUM LED_NUM_LOCK
  #define LED_CAP LED_CAPS_LOCK

#elif defined(__SCO__)
  #include <sys/vtkd.h>
  #define GET_LED_STATE KDGETLED
  #define SET_LED_STATE KDSETLED
  #define LED_SCR 0x1
  #define LED_NUM 0x2
  #define LED_CAP 0x4

#else
  #error Platform not supported.
#endif

#if defined(__sun)
  /* A few things that Solaris does differently than everyone else. */
  typedef char ioctl_arg_type;
  #define CONSOLE_DEVICE	"/dev/kbd"
  #include <sys/param.h>
  #define CWD_SIZE MAXPATHLEN
#else
  typedef int ioctl_arg_type;
  #define CONSOLE_DEVICE	"/dev/console"
  #define CWD_SIZE 0
#endif

#ifndef LED_MASK
  #define LED_MASK	(LED_CAP | LED_NUM | LED_SCR)
#endif

#ifndef _PATH_VARRUN
  #define _PATH_VARRUN	"/var/run/"
#endif

#define LOCK_FILE       _PATH_VARRUN "flasher.pid"

#define LED_ON	1
#define LED_OFF	0

#define SIGNAL(s, handler)      { \
    sa.sa_handler = handler; \
    if (sigaction(s, &sa, NULL) < 0) { \
        syslog(LOG_ERR, "Couldn't establish signal handler (%d): %m", s); \
        exit(EXIT_FAILURE); \
    } \
}

extern int LEDstate(int led, int state);
extern void set_sig_handlers(void);
extern void check_lock(void);
extern void daemonize(void);
extern void open_console(char *terminal);
extern int console_opened(void);
extern char *absolute_path(char *file);

typedef struct file {
    char *path;
    time_t time;
    int writes;
} file;
