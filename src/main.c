#define AP_IMPLEMENTATION
#include "apostrophe.h"
#define AP_WIDGETS_IMPLEMENTATION
#include "apostrophe_widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moonlight.h"

static void run_app(void)
{
    for (;;) {
        main_action action = show_main_menu();
        switch (action) {
        case MAIN_ACTION_STREAM:   stream_flow();    break;
        case MAIN_ACTION_PAIR:     pair_flow();       break;
        case MAIN_ACTION_SERVERS:  servers_flow();    break;
        case MAIN_ACTION_SETTINGS: show_settings();   break;
        case MAIN_ACTION_QUIT:     return;
        }
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    ap_config cfg = {0};
    cfg.window_title = "Moonlight";
    cfg.font_path    = AP_PLATFORM_IS_DEVICE ? NULL
                       : "third_party/apostrophe/res/font.ttf";
    cfg.log_path     = ap_resolve_log_path("moonlight");
    cfg.is_nextui    = AP_PLATFORM_IS_DEVICE;
    cfg.cpu_speed    = AP_CPU_SPEED_MENU;

    if (ap_init(&cfg) != AP_OK) {
        fprintf(stderr, "Failed to initialise Apostrophe: %s\n",
                ap_get_error());
        return 1;
    }

    ap_log("moonlight-pak: startup platform=%s", AP_PLATFORM_NAME);

    run_app();

    ap_quit();
    return 0;
}
