#include "terminal.h"
#include "editor.h"
#include "eko.h"

int main(int argc, char **argv) {
    file_name = argc >= 2 ? argv[1] : NULL;

    config_load();
    syntax_load_external();
    enable_raw_mode();
    editor_init();
    if (file_name)
        editor_open();

    editor_set_status_message("^S=save ^F=find ^R=replace ^O=open ^Q=quit");

    while (1) {
        editor_refresh_screen();
        editor_process_keypress();
    }
    return 0;
}
