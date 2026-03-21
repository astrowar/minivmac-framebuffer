#include "input_reader.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/select.h>
#include <linux/input.h>
#include "OSGLUFB_Common.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_EVENTS 32

int input_reader_init(input_reader_t *reader) {
    if (!reader) {
        return -1;
    }

    memset(reader, 0, sizeof(input_reader_t));

    /* Check if we can access /dev/input/event0 */
    int test_fd = open("/dev/input/event0", O_RDONLY);
    if (test_fd < 0) {
        if (errno == ENOENT) {
            /* File does not exist */
            log_printf("\n");
            log_printf("ERROR: /dev/input/event0 does not exist\n");
            log_printf("Input devices are not available on this system.\n");
            log_printf("\n");
            return -1;
        } else if (errno == EACCES) {
            /* Permission denied - user is not in the input group */
            log_printf("\n");
            log_printf("ERROR: Permission denied accessing /dev/input/event0\n");
            log_printf("The current user does not have permission to access input devices.\n");
            log_printf("\n");
            log_printf("To fix this, add your user to the 'input' group:\n");
            log_printf("    sudo usermod -aG input %s\n", getenv("USER") ? getenv("USER") : "your_user");
            log_printf("\n");
            log_printf("Then log out and log back in (or reboot) for the changes to take effect.\n");
            log_printf("\n");
            return -1;
        } else {
            /* Other error */
            log_printf("\n");
            log_printf("ERROR: Cannot access /dev/input/event0: %s\n", strerror(errno));
            log_printf("\n");
            return -1;
        }
    }
    close(test_fd);

    DIR *input_dir = opendir("/sys/class/input");
    if (!input_dir) {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(input_dir)) != NULL && reader->count < MAX_KEYBOARDS) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        int num = atoi(entry->d_name + 5);
        if (num < 0 || num >= MAX_EVENTS) {
            continue;
        }

        char dev_path[MAX_NAME_LEN];
        char sys_path[MAX_NAME_LEN];
        char name[MAX_NAME_LEN] = {0};
        char full_path[MAX_NAME_LEN];

        snprintf(dev_path, sizeof(dev_path), "/dev/input/event%d", num);
        snprintf(sys_path, sizeof(sys_path), "/sys/class/input/%s", entry->d_name);

        /* Get device name */
        snprintf(full_path, sizeof(full_path), "%s/device/name", sys_path);
        FILE *f = fopen(full_path, "r");
        if (f) {
            if (fgets(name, sizeof(name), f) != NULL) {
                name[strcspn(name, "\n")] = 0;
            }
            fclose(f);
        }

        /* Skip non-keyboard devices */
        if (strstr(name, "ALSA") != NULL || strstr(name, "HDA") != NULL ||
            strstr(name, "Video") != NULL || strstr(name, "Power Button") != NULL ||
            strstr(name, "Lid") != NULL || strstr(name, "Dell") != NULL ||
            strstr(name, "Intel") != NULL || strstr(name, "PS/2") != NULL ||
            strstr(name, "Power") != NULL || strstr(name, "Button") != NULL ||
            strstr(name, "sof-hda-dsp") != NULL) {
            continue;
        }

        /* Open device */
        int fd = open(dev_path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        reader->keyboards[reader->count].fd = fd;
        reader->keyboards[reader->count].name[0] = '\0';
        if (name[0] != '\0') {
            strncpy(reader->keyboards[reader->count].name, name, MAX_NAME_LEN - 1);
        }
        reader->keyboards[reader->count].path[0] = '\0';
        strncpy(reader->keyboards[reader->count].path, dev_path, MAX_NAME_LEN - 1);
        reader->count++;
    }

    closedir(input_dir);
    return reader->count > 0 ? 0 : -1;
}

int input_reader_read_key(input_reader_t *reader, uint16_t *key_code) {
    if (reader == NULL || key_code == NULL || reader->count <= 0) {
        return 0;
    }

    fd_set readfds;
    int maxfd = -1;

    FD_ZERO(&readfds);

    for (int i = 0; i < reader->count; i++) {
        int fd = reader->keyboards[i].fd;
        if (fd >= 0) {
            FD_SET(fd, &readfds);
            if (fd > maxfd) {
                maxfd = fd;
            }
        }
    }

    if (maxfd < 0) {
        return 0;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
    if (ready <= 0) {
        return 0;
    }

    for (int i = 0; i < reader->count; i++) {
        int fd = reader->keyboards[i].fd;
        if (fd < 0 || !FD_ISSET(fd, &readfds)) {
            continue;
        }

        while (1) {
            struct input_event ev;
            ssize_t n = read(fd, &ev, sizeof(ev));

            if (n == (ssize_t)sizeof(ev)) {
                if ((ev.type == EV_KEY) && (ev.value == 1)) {
                    *key_code = (uint16_t)ev.code;
                    return 1;
                }
            } else if ((n < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                break;
            } else {
                break;
            }
        }
    }

    return 0;
}

const char* input_reader_get_keyboard_name(input_reader_t *reader, int index) {
    if (reader == NULL || index < 0 || index >= reader->count) {
        return NULL;
    }
    return reader->keyboards[index].name;
}

int input_reader_get_count(input_reader_t *reader) {
    if (reader == NULL) {
        return 0;
    }
    return reader->count;
}

void input_reader_cleanup(input_reader_t *reader) {
    if (reader == NULL) {
        return;
    }
    for (int i = 0; i < reader->count; i++) {
        if (reader->keyboards[i].fd >= 0) {
            close(reader->keyboards[i].fd);
            reader->keyboards[i].fd = -1;
        }
    }
    reader->count = 0;
}

#pragma GCC diagnostic pop