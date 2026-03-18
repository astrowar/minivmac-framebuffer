#ifndef INPUT_READER_H
#define INPUT_READER_H

#include <stdint.h>

#define MAX_KEYBOARDS 16
#define MAX_NAME_LEN 1024

typedef struct {
    int fd;
    char name[MAX_NAME_LEN];
    char path[MAX_NAME_LEN];
} keyboard_device_t;

typedef struct {
    keyboard_device_t keyboards[MAX_KEYBOARDS];
    int count;
} input_reader_t;

/* Initialize and detect all keyboard devices */
int input_reader_init(input_reader_t *reader);

/* Read a single key press event from all keyboards */
/* Returns: >0 if key pressed, 0 if timeout, -1 on error */
int input_reader_read_key(input_reader_t *reader, uint16_t *key_code);

/* Get the name of a keyboard by index */
const char* input_reader_get_keyboard_name(input_reader_t *reader, int index);

/* Get the number of detected keyboards */
int input_reader_get_count(input_reader_t *reader);

/* Cleanup and close all devices */
void input_reader_cleanup(input_reader_t *reader);

#endif /* INPUT_READER_H */