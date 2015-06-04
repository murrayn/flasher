
/*
    util.c - Copyright (C) 2002 Murray Nesbitt (websrc@nesbitt.ca)

    This program is protected and licensed under the following terms and
    conditions: 1) it may not be redistributed in binary form without the
    explicit permission of the author; 2) when redistributed in source
    form, in whole or in part, this complete copyright statement must
    remain intact.
*/

#include "flasher.h"

static int console;  /* console file descriptor */
extern char *progname;

/* Set the LED state, on or off. */
int LEDstate(int led, int state)
{
    ioctl_arg_type LEDs;
    int rc = 0;

    if (ioctl(console, GET_LED_STATE, &LEDs) < 0) {
        syslog(LOG_ERR, "ioctl() get failed: %m");
        rc = -1;
    }
    else {
        LEDs = (state ? LEDs|led : (LEDs & ~led)) & LED_MASK;
#if defined(__sun)
        /* Solaris requires a pointer to the LED arg. */
        if (ioctl(console, SET_LED_STATE, &LEDs) < 0) {
#else
        if (ioctl(console, SET_LED_STATE, LEDs) < 0) {
#endif
            syslog(LOG_ERR, "ioctl() set failed: %m");
            rc = -1;
        }
    }
    return rc;
}

/* Build an absolute path to file, if necessary. */
char *absolute_path(char *file)
{
    static char *cwd;
    char *path;

    if (!cwd)
        cwd = getcwd(NULL, CWD_SIZE);

    if (file[0] != '/') {
        if ((path = malloc(strlen(cwd) + strlen(file) + 2)) == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        sprintf(path, "%s/%s", cwd, file);
        fprintf(stderr, "%s: warning: converting '%s' to " \
            "absolute pathname '%s'.\n", progname, file, path);
        file = path;
    }
    return file;
}

void open_console(char *terminal) {

    if (!terminal)
        terminal = CONSOLE_DEVICE;

    if ((console = open(terminal, O_RDONLY|O_NOCTTY)) < 0) {
        perror(terminal);
        exit(EXIT_FAILURE);
    }
}

int console_opened(void)
{
    return console;
}

/* Set the program up to run as a daemon. */
void daemonize(void)
{
    pid_t pid;
    int i;
    FILE *f;

    if ((pid = fork()) == 0) {
        /* Since we are running as a daemon, change the current working
           directory to the root.  Otherwise, we might at some point
           prevent a mounted filesystem from being unmounted. */
        chdir("/");

        /* Start a new session. */
        setsid();

        /* Close all open descriptors, except the console. */
        for (i = 0; i < console; i++)
            close(i);

        /* There's now no controlling terminal, so any output must go
           to syslog. */
        openlog(progname, LOG_PID, LOG_USER);
        syslog(LOG_INFO, "daemon started");
    }
    else if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else {
        close(console);
        fprintf(stderr, "%s daemon started, PID %d.\n", progname, (int)pid);

        /* Write the pid to the lock file. */
        if ((f = fopen(LOCK_FILE, "w")) == NULL) {
            fprintf(stderr, "%s: warning: could not open '%s' for writing\n",
            progname, LOCK_FILE);
        }
        else {
            fprintf(f, "%d\n", (int)pid);
            fclose(f);
        }
        exit(EXIT_SUCCESS);
    }
}

/* Make sure our lock file gets removed on exit, and set
   the LEDs to match the current keyboard state. */
static void cleanup(int sig) {
#if defined(GET_KB_STATE)
    ioctl_arg_type LEDs;

    if (ioctl(console, GET_KB_STATE, &LEDs) != -1) {
        LEDstate(LED_CAP, LEDs & LED_CAP ? LED_ON : LED_OFF);
        LEDstate(LED_NUM, LEDs & LED_NUM ? LED_ON : LED_OFF);
        LEDstate(LED_SCR, LEDs & LED_SCR ? LED_ON : LED_OFF);
    }
#endif
    closelog();
    unlink(LOCK_FILE);
    exit(EXIT_FAILURE);
}

void set_sig_handlers(void)
{
    sigset_t mask;
    struct sigaction sa;

    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sa.sa_mask = mask;
    sa.sa_flags = 0;
    SIGNAL(SIGHUP, cleanup);
    SIGNAL(SIGINT, cleanup);
    SIGNAL(SIGTERM, cleanup);
}

/* Make sure another instance isn't already running. */
void check_lock(void)
{
    FILE *f;
    pid_t pid;

    if ((f = fopen(LOCK_FILE, "r")) == NULL) {
        return;
    }

    fscanf(f, "%d\n", (int *)&pid);
    fclose(f);
    
    if ((kill(pid, 0) == -1) && (errno == ESRCH)) {
        /* Was running, but appears to be gone now. */
        unlink(LOCK_FILE);
    }
    else {
        fprintf(stderr, "%s appears to be running (PID: %d). " \
        "If this is not the case, delete '%s' and try again.\n",
            progname, (int)pid, LOCK_FILE);
        exit(EXIT_FAILURE);
    }
}


