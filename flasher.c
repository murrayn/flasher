/*
    flasher.c - Copyright (C) 2002 Murray Nesbitt (websrc@nesbitt.ca)

    This program is protected and licensed under the following terms and
    conditions: 1) it may not be redistributed in binary form without the
    explicit permission of the author; 2) when redistributed in source
    form, in whole or in part, this complete copyright statement must
    remain intact.
*/

#include "flasher.h"

/* One of these per LED. */
static struct LED {
    int led;
    int flash_state;
    int num_files;
    struct file *files;
} LED[3];

#define FLASH_ANY (LED[0].flash_state || LED[1].flash_state || \
                   LED[2].flash_state)

char *progname;
static int preferred_sleep_time = SLEEP;

void usage(void)
{
    printf("Usage: %s [-k tty] [-u #] -{c|n|s} file1:...:fileN -{c|n|s} file1:...\n",
        progname); 
    exit(EXIT_FAILURE);
}

/* Monitors writes and reads to the files, and takes care of 
   flashing the LEDs. */
void *monitor(void) {
    int i, j;
    struct stat buf;
    struct LED *this;
    int current_sleep_time = preferred_sleep_time;

    while(1) {
        sleep(current_sleep_time);
        for (i = 0; i < 3; i++) {
            this = &LED[i];

            for (j = 0; j < this->num_files; j++) {
                if (stat(this->files[j].path, &buf) == -1)
                    /* File still doesn't exist, but user has been warned. */
                    continue;
                if (buf.st_mtime > this->files[j].time) {
                    /* File has been changed since we last looked. */
                    this->files[j].time = buf.st_mtime;
                    this->files[j].writes++;
                    this->flash_state++;
                    current_sleep_time = FLASH_SLEEP;
                }
                else if (buf.st_atime > this->files[j].time && 
                    this->flash_state) {
                    /* File has been read since we last looked. */
                    this->files[j].time = buf.st_atime;
                    this->flash_state -= this->files[j].writes;
                    this->files[j].writes = 0;
                    if (!FLASH_ANY) {
                        /* All files read -- back to regular sleep interval. */
                        current_sleep_time = preferred_sleep_time;
                    }

                }
            }
        }

        if (!FLASH_ANY)
            continue;
    
        for (i = 0; i < 3; i++) {
            this = &LED[i];
            for (j = 0; j < this->flash_state; j++) {
                usleep(FLASH_DELAY);
                LEDstate(this->led, LED_ON);
                usleep(FLASH_ONTIME);
                LEDstate(this->led, LED_OFF);
            }
        }
    }
}

/* Associates one or more (colon-separated) filenames with a 
   particular LED.  */
void setup_LED(int led, char *filenames)
{
    char *file;
    int num_files = 0;
    struct stat buf;
    struct LED *this;
    int leds[] = { LED_CAP, LED_NUM, LED_SCR };

    this = &LED[led];
    this->led = leds[led];

    if (((file = strtok(filenames, ":")) == NULL) || !filenames) {
        usage(); /* exits */
    }

    /* Allocate storage for the first file. */
    if ((this->files = malloc(sizeof(struct file))) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    do {
        file = absolute_path(file);
        if (stat(file, &buf) == -1) {
            if (errno == ENOENT) {
                /* If the file doesn't yet exist, that's OK, but warn. */
                fprintf(stderr, "%s: warning: %s does not exist.\n",
                    progname, file);
                buf.st_mtime = 0;
            }
            else {
                perror(file);
                exit(EXIT_FAILURE);
            }
        }
        if (num_files && (this->files = realloc(this->files, 
                (num_files + 1) * sizeof(struct file))) == NULL) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
        }
        this->num_files++;
        this->files[num_files].path = file;
        this->files[num_files].time = buf.st_mtime;
        this->files[num_files].writes = 0;
        num_files++;
    } while ((file = strtok(NULL, ":")) != NULL);
}

int main(int argc, char **argv)
{
    int i = 1;

    progname = argv[0];

    if (geteuid() != 0) {
        fprintf(stderr, "%s: must be run as root.\n", progname);
        exit(EXIT_FAILURE);
    }

    /* Ensure we aren't already running. */
    check_lock();

    /* option handling */
    while (argv[i] && argv[i][0] == '-') {
        switch(argv[i][1]) {
            case 'c':
                if (LED[0].led)
                    usage(); /* exits */
                setup_LED(0, argv[++i]);
                break;
            case 'k':
                open_console(argv[++i]);
                break;
            case 'n':
                if (LED[1].led)
                    usage(); /* exits */
                setup_LED(1, argv[++i]);
                break;
            case 's':
                if (LED[2].led)
                    usage(); /* exits */
                setup_LED(2, argv[++i]);
                break;
            case 'u':
                if ((preferred_sleep_time = atoi(argv[++i])) == 0)
                    usage(); /* exits */
                break;
            default:
                usage(); /* exits */
        }
        i++;
    }

    /* Nothing to do. */
    if (!(LED[0].led || LED[1].led || LED[2].led)) {
        usage(); /* exits */
    }

    if (!console_opened())
        open_console(NULL);

    /* Initialize signal handlers for removing the lock file
       and restoring LED states at exit. */
    set_sig_handlers();
    /* Become a daemon, and create our lock file. */
    daemonize();

    monitor();

    return 0;  /* not reached. */
}

