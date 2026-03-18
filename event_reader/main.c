#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/input.h>
#include "input_reader.h"

static const char* get_key_name(uint16_t code) {
    switch (code) {
        case KEY_ESC: return "ESC";
        case KEY_1: return "1";
        case KEY_2: return "2";
        case KEY_3: return "3";
        case KEY_4: return "4";
        case KEY_5: return "5";
        case KEY_6: return "6";
        case KEY_7: return "7";
        case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_0: return "0";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_TAB: return "TAB";
        case KEY_Q: return "Q";
        case KEY_W: return "W";
        case KEY_E: return "E";
        case KEY_R: return "R";
        case KEY_T: return "T";
        case KEY_Y: return "Y";
        case KEY_U: return "U";
        case KEY_I: return "I";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_A: return "A";
        case KEY_S: return "S";
        case KEY_D: return "D";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_Z: return "Z";
        case KEY_X: return "X";
        case KEY_C: return "C";
        case KEY_V: return "V";
        case KEY_B: return "B";
        case KEY_N: return "N";
        case KEY_M: return "M";
        case KEY_COMMA: return ",";
        case KEY_DOT: return ".";
        case KEY_SLASH: return "/";
        case KEY_LEFTCTRL: return "LEFTCTRL";
        case KEY_LEFTSHIFT: return "LEFTSHIFT";
        case KEY_LEFTALT: return "LEFTALT";
        case KEY_RIGHTCTRL: return "RIGHTCTRL";
        case KEY_RIGHTSHIFT: return "RIGHTSHIFT";
        case KEY_RIGHTALT: return "RIGHTALT";
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        default: return "UNKNOWN";
    }
}

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    input_reader_t reader;

    printf("Initializing input reader...\n");
    if (input_reader_init(&reader) < 0) {
        fprintf(stderr, "No keyboard devices found!\n");
        return 1;
    }

    printf("Found %d keyboard device(s):\n", (int)reader.count);
    for (int i = 0; i < (int)reader.count; i++) {
        const char *name = input_reader_get_keyboard_name(&reader, i);
        printf("  %d: %s\n", i, name ? name : "unknown");
    }

    printf("\nPress Ctrl+C or ESC to exit\n\n");

    uint16_t key_code;
    while (running) {
        int ret = input_reader_read_key(&reader, &key_code);
        if (ret > 0) {
            if (key_code == KEY_ESC) {
                printf("ESC\n");
                break;
            }
            const char *name = get_key_name(key_code);
            printf("%s\n", name ? name : "UNKNOWN");
            fflush(stdout);
        }
    }

    input_reader_cleanup(&reader);
    printf("Goodbye!\n");
    return 0;
}