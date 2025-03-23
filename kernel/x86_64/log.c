#include <log.h>
#include <string.h>

static char tty_buffer[TTY_MAX_ROWS][TTY_ROW_MAX_LEN];
static int current_row = 0;
static int current_col = 0;

void log_init(void) {
    memset(tty_buffer, 0, sizeof(tty_buffer));
    current_row = 0;
    current_col = 0;
}

static void newline(void) {
    current_row++;
    if (current_row >= TTY_MAX_ROWS) {
        // Scroll up by shifting rows upward.
        for (int i = 1; i < TTY_MAX_ROWS; i++) {
            memcpy(tty_buffer[i - 1], tty_buffer[i], TTY_ROW_MAX_LEN);
        }
        current_row = TTY_MAX_ROWS - 1;
        memset(tty_buffer[current_row], 0, TTY_ROW_MAX_LEN);
    }
    current_col = 0;
}

void log_print_char(char ch) {
    if (ch == '\n') {
        newline();
    } else if (ch == '\r') {
        current_col = 0;
    } else if (ch == '\t') {
        // Expand tab to four spaces.
        for (int i = 0; i < 4; i++) {
            log_print_char(' ');
        }
    } else if (ch == '\b') {
        if (current_col > 0) {
            current_col--;
            tty_buffer[current_row][current_col] = ' ';
        }
    } else {
        if (current_col < TTY_ROW_MAX_LEN - 1) {
            tty_buffer[current_row][current_col++] = ch;
        } else {
            newline();
            tty_buffer[current_row][current_col++] = ch;
        }
    }
}

void log_print_str(const char *str) {
    while (*str) {
        log_print_char(*str++);
    }
}

const char* log_get_row(int index) {
    if (index < 0 || index >= TTY_MAX_ROWS)
        return 0;
    return tty_buffer[index];
}

int log_get_row_count(void) {
    return current_row + 1;
}
