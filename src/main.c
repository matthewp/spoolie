#include <locale.h>
#include "ui.h"

int main(void) {
    /* Enable UTF-8 */
    setlocale(LC_ALL, "");

    ui_state_t state;
    ui_init(&state);

    /* Use timeout so getch doesn't block - allows polling for async ops */
    timeout(100);

    while (state.running) {
        ui_draw(&state);
        ui_poll(&state);
        int ch = getch();
        if (ch == ERR) {
            /* Timeout - no input, just continue loop */
            continue;
        } else if (ch == KEY_RESIZE) {
            /* ncurses handles SIGWINCH internally and returns KEY_RESIZE */
            ui_resize(&state);
        } else {
            ui_handle_input(&state, ch);
        }
    }

    ui_cleanup(&state);
    return 0;
}
