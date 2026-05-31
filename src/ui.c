#include "apostrophe.h"
#include "apostrophe_widgets.h"
#include "moonlight.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────── */

static void show_error(const char *message)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 1,
    };
    ap_confirm_result result;
    (void)ap_confirmation(&opts, &result);
}

static void show_info(const char *message)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_A, .label = "OK", .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 1,
    };
    ap_confirm_result result;
    (void)ap_confirmation(&opts, &result);
}

static bool show_confirm(const char *message, const char *confirm_label)
{
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Cancel" },
        { .button = AP_BTN_A, .label = confirm_label, .is_confirm = true },
    };
    ap_message_opts opts = {
        .message = message,
        .footer = footer,
        .footer_count = 2,
    };
    ap_confirm_result result = {0};
    int rc = ap_confirmation(&opts, &result);
    return rc == AP_OK && result.confirmed;
}

static const ml_server *active_server(const ml_config *cfg)
{
    if (cfg->active_server >= 0 && cfg->active_server < cfg->server_count)
        return &cfg->servers[cfg->active_server];
    return NULL;
}

/* ── Main menu ───────────────────────────────────────────────── */

main_action show_main_menu(void)
{
    ml_config cfg = load_config();
    const ml_server *srv = active_server(&cfg);

    ap_list_item items[] = {
        { .label = "Stream",  .trailing_text = srv ? srv->name : NULL },
        AP_LIST_ITEM("Pair",     NULL),
        AP_LIST_ITEM("Servers",  NULL),
        AP_LIST_ITEM("Settings", NULL),
    };
    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Quit" },
        { .button = AP_BTN_A, .label = "Select", .is_confirm = true },
    };

    char help[256];
    if (srv)
        snprintf(help, sizeof(help), "Active server: %s (%s)", srv->name, srv->host);
    else
        snprintf(help, sizeof(help), "No server configured. Go to Servers to add one.");

    ap_list_opts opts = ap_list_default_opts("Moonlight", items, 4);
    opts.footer = footer;
    opts.footer_count = 2;
    opts.help_text = help;

    ap_list_result result = {0};
    int rc = ap_list(&opts, &result);
    if (rc != AP_OK)
        return MAIN_ACTION_QUIT;

    switch (result.selected_index) {
    case 0: return MAIN_ACTION_STREAM;
    case 1: return MAIN_ACTION_PAIR;
    case 2: return MAIN_ACTION_SERVERS;
    case 3: return MAIN_ACTION_SETTINGS;
    default: return MAIN_ACTION_QUIT;
    }
}

/* ── Pair flow ───────────────────────────────────────────────── */

typedef struct {
    char  host[ML_MAX_IP];
    char  pin[16];
    int   result;
    int  *interrupt;
    char *dynamic_msg;
    char  msg_buf[256];
} pair_worker_data;

static int pair_worker(void *userdata)
{
    pair_worker_data *d = (pair_worker_data *)userdata;
    d->result = ml_pair(d->host, d->pin, sizeof(d->pin),
                        d->interrupt, &d->dynamic_msg,
                        d->msg_buf, sizeof(d->msg_buf));
    return d->result;
}

void pair_flow(void)
{
    ml_config cfg = load_config();
    const ml_server *srv = active_server(&cfg);
    if (!srv) {
        show_error("No server configured.\n\nGo to Servers to add one.");
        return;
    }

    int interrupt = 0;
    pair_worker_data wd = {0};
    snprintf(wd.host, sizeof(wd.host), "%s", srv->host);
    wd.interrupt = &interrupt;
    snprintf(wd.msg_buf, sizeof(wd.msg_buf), "Pairing with %s", srv->name);
    wd.dynamic_msg = wd.msg_buf;

    ap_process_opts proc = {
        .message          = " ",
        .interrupt_signal = &interrupt,
        .interrupt_button = AP_BTN_B,
        .dynamic_message  = &wd.dynamic_msg,
        .message_lines    = 1,
    };
    int rc = ap_process_message(&proc, pair_worker, &wd);

    if (interrupt) return;

    if (rc == AP_OK && wd.result == 0)
        show_info("Paired successfully!");
    else
        show_error("Pairing failed.\n\nMake sure Sunshine is running\nand reachable.");
}

/* ── Stream flow ─────────────────────────────────────────────── */

typedef struct {
    char host[ML_MAX_IP];
    ml_app apps[ML_MAX_APPS];
    int  count;
    int *interrupt;
} list_worker_data;

static int list_worker(void *userdata)
{
    list_worker_data *d = (list_worker_data *)userdata;
    d->count = ml_list_apps(d->host, d->apps, ML_MAX_APPS, d->interrupt);
    return (d->count >= 0) ? 0 : -1;
}

void stream_flow(void)
{
    ml_config cfg = load_config();
    const ml_server *srv = active_server(&cfg);
    if (!srv) {
        show_error("No server configured.\n\nGo to Servers to add one.");
        return;
    }

    int interrupt = 0;
    list_worker_data wd = {0};
    snprintf(wd.host, sizeof(wd.host), "%s", srv->host);
    wd.interrupt = &interrupt;

    char msg[256];
    snprintf(msg, sizeof(msg), "Querying apps on %s", srv->name);
    ap_process_opts proc = {
        .message          = msg,
        .interrupt_signal = &interrupt,
        .interrupt_button = AP_BTN_B,
    };
    int rc = ap_process_message(&proc, list_worker, &wd);

    if (interrupt) return;

    if (rc != AP_OK || wd.count <= 0) {
        show_error("Could not retrieve app list.\n\nMake sure the host is paired\nand reachable.");
        return;
    }

    ap_list_item *items = calloc(wd.count, sizeof(ap_list_item));
    if (!items) return;
    for (int i = 0; i < wd.count; i++)
        items[i].label = wd.apps[i].name;

    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
        { .button = AP_BTN_A, .label = "Launch", .is_confirm = true },
    };

    char title[ML_MAX_NAME + 16];
    snprintf(title, sizeof(title), "%s", srv->name);

    ap_list_opts opts = ap_list_default_opts(title, items, wd.count);
    opts.footer = footer;
    opts.footer_count = 2;

    ap_list_result result = {0};
    rc = ap_list(&opts, &result);

    if (rc == AP_OK && result.selected_index >= 0 &&
        result.selected_index < wd.count) {
        const char *app_name = wd.apps[result.selected_index].name;
        ap_log("ui: launching stream for app: %s", app_name);

        ap_quit();
        int stream_rc = ml_stream(&cfg, app_name);

        ap_config acfg = {0};
        acfg.window_title = "Moonlight";
        acfg.font_path    = AP_PLATFORM_IS_DEVICE ? NULL
                            : "third_party/apostrophe/res/font.ttf";
        acfg.log_path     = ap_resolve_log_path("moonlight");
        acfg.is_nextui    = AP_PLATFORM_IS_DEVICE;
        acfg.cpu_speed    = AP_CPU_SPEED_MENU;
        ap_init(&acfg);

        if (stream_rc != 0) {
            char errmsg[128];
            snprintf(errmsg, sizeof(errmsg), "Stream ended with error (%d).", stream_rc);
            show_error(errmsg);
        }
    }

    free(items);
}

/* ── Servers flow ────────────────────────────────────────────── */

static void add_server(ml_config *cfg)
{
    if (cfg->server_count >= ML_MAX_SERVERS) {
        show_error("Maximum number of servers reached.");
        return;
    }

    ap_keyboard_result name_result;
    int rc = ap_keyboard("", "Server name", AP_KB_GENERAL, &name_result);
    if (rc != AP_OK || !name_result.text[0])
        return;

    ap_keyboard_result host_result;
    rc = ap_url_keyboard("", "IP address (e.g. 192.168.1.100)", NULL, &host_result);
    if (rc != AP_OK || !host_result.text[0])
        return;

    int idx = cfg->server_count;
    snprintf(cfg->servers[idx].name, ML_MAX_NAME, "%s", name_result.text);
    snprintf(cfg->servers[idx].host, ML_MAX_IP, "%s", host_result.text);
    cfg->server_count++;

    if (cfg->active_server < 0)
        cfg->active_server = idx;

    save_config(cfg);

    char msg[256];
    snprintf(msg, sizeof(msg), "Added %s (%s)",
             cfg->servers[idx].name, cfg->servers[idx].host);
    show_info(msg);
}

static void edit_server(ml_config *cfg, int idx)
{
    if (idx < 0 || idx >= cfg->server_count) return;

    ml_server *srv = &cfg->servers[idx];

    ap_keyboard_result name_result;
    int rc = ap_keyboard(srv->name, "Server name", AP_KB_GENERAL, &name_result);
    if (rc != AP_OK || !name_result.text[0])
        return;

    ap_keyboard_result host_result;
    rc = ap_url_keyboard(srv->host, "IP address (e.g. 192.168.1.100)", NULL, &host_result);
    if (rc != AP_OK || !host_result.text[0])
        return;

    snprintf(srv->name, ML_MAX_NAME, "%s", name_result.text);
    snprintf(srv->host, ML_MAX_IP, "%s", host_result.text);
    save_config(cfg);
}

static void delete_server(ml_config *cfg, int idx)
{
    if (idx < 0 || idx >= cfg->server_count) return;

    char msg[256];
    snprintf(msg, sizeof(msg), "Remove %s (%s)?",
             cfg->servers[idx].name, cfg->servers[idx].host);
    if (!show_confirm(msg, "Remove")) return;

    for (int i = idx; i < cfg->server_count - 1; i++)
        cfg->servers[i] = cfg->servers[i + 1];
    cfg->server_count--;

    if (cfg->active_server == idx)
        cfg->active_server = cfg->server_count > 0 ? 0 : -1;
    else if (cfg->active_server > idx)
        cfg->active_server--;

    save_config(cfg);
}

static int show_server_detail(ml_config *cfg, int idx)
{
    if (idx < 0 || idx >= cfg->server_count) return 0;

    ml_server *srv = &cfg->servers[idx];
    bool is_active = (idx == cfg->active_server);

    (void)is_active;

    ap_list_item action_items[] = {
        AP_LIST_ITEM("Edit",   NULL),
        AP_LIST_ITEM("Delete", NULL),
    };

    ap_footer_item footer[] = {
        { .button = AP_BTN_B, .label = "Back" },
        { .button = AP_BTN_A, .label = "Select", .is_confirm = true },
    };

    ap_list_opts opts = ap_list_default_opts(srv->name, action_items, 2);
    opts.footer = footer;
    opts.footer_count = 2;

    ap_list_result result = {0};
    int rc = ap_list(&opts, &result);
    if (rc != AP_OK) return 0;

    if (result.selected_index == 0) {
        edit_server(cfg, idx);
        return 1;
    } else if (result.selected_index == 1) {
        delete_server(cfg, idx);
        return 2;
    }

    return 0;
}

void servers_flow(void)
{
    int last_index = 0;
    int last_visible = 0;

    for (;;) {
        ml_config cfg = load_config();

        int item_count = cfg.server_count;
        ap_list_item *items = NULL;
        char (*labels)[ML_MAX_NAME + ML_MAX_IP + 16] = NULL;

        if (item_count > 0) {
            items = calloc(item_count, sizeof(ap_list_item));
            labels = calloc(item_count, sizeof(*labels));
            if (!items || !labels) {
                free(items);
                free(labels);
                return;
            }

            for (int i = 0; i < item_count; i++) {
                if (i == cfg.active_server)
                    snprintf(labels[i], sizeof(labels[i]),
                             "\xE2\x97\x8F %s", cfg.servers[i].name);
                else
                    snprintf(labels[i], sizeof(labels[i]),
                             "  %s", cfg.servers[i].name);
                items[i].label = labels[i];
                items[i].trailing_text = cfg.servers[i].host;
            }
        }

        ap_footer_item footer_with_servers[] = {
            { .button = AP_BTN_B, .label = "Back" },
            { .button = AP_BTN_A, .label = "Open", .is_confirm = true },
            { .button = AP_BTN_X, .label = "Add" },
            { .button = AP_BTN_SELECT, .label = "Activate" },
        };
        ap_footer_item footer_empty[] = {
            { .button = AP_BTN_B, .label = "Back" },
            { .button = AP_BTN_X, .label = "Add" },
        };

        ap_list_result result = {0};
        int rc;

        if (item_count > 0) {
            ap_list_opts opts = ap_list_default_opts("Servers", items, item_count);
            opts.footer = footer_with_servers;
            opts.footer_count = 4;
            opts.action_button = AP_BTN_X;
            opts.secondary_action_button = AP_BTN_SELECT;
            opts.initial_index = last_index < item_count ? last_index : 0;
            opts.visible_start_index = last_visible;

            rc = ap_list(&opts, &result);
            last_index = result.selected_index >= 0 ? result.selected_index : last_index;
            last_visible = result.visible_start_index;
        } else {
            ap_list_item empty_items[] = {
                AP_LIST_ITEM("No servers. Press X to add.", NULL),
            };
            ap_list_opts opts = ap_list_default_opts("Servers", empty_items, 1);
            opts.footer = footer_empty;
            opts.footer_count = 2;
            opts.action_button = AP_BTN_X;

            rc = ap_list(&opts, &result);
        }

        free(items);
        free(labels);

        if (rc == AP_CANCELLED) return;
        if (rc != AP_OK) return;

        if (result.action == AP_ACTION_TRIGGERED) {
            ml_config fresh = load_config();
            add_server(&fresh);
            continue;
        }

        if (result.action == AP_ACTION_SECONDARY_TRIGGERED &&
            result.selected_index >= 0 && result.selected_index < item_count) {
            ml_config fresh = load_config();
            fresh.active_server = result.selected_index;
            save_config(&fresh);
            continue;
        }

        if (result.action == AP_ACTION_SELECTED &&
            result.selected_index >= 0 && result.selected_index < item_count) {
            ml_config fresh = load_config();
            int action = show_server_detail(&fresh, result.selected_index);
            if (action == 2 && last_index >= fresh.server_count && fresh.server_count > 0)
                last_index = fresh.server_count - 1;
            continue;
        }
    }
}

/* ── Settings ────────────────────────────────────────────────── */

void show_settings(void)
{
    ml_config cfg = load_config();

    ap_option fps_opts[] = {
        { .label = "30",  .value = "30"  },
        { .label = "60",  .value = "60"  },
        { .label = "120", .value = "120" },
    };
    int fps_sel = 1;
    if (cfg.fps == 30) fps_sel = 0;
    else if (cfg.fps == 120) fps_sel = 2;

    ap_option res_opts[] = {
        { .label = "1024x768 (Brick native)", .value = "1024x768" },
        { .label = "1280x720 (720p)",         .value = "1280x720" },
        { .label = "640x480 (low)",            .value = "640x480"  },
    };
    int res_sel = 0;
    if (cfg.width == 1280 && cfg.height == 720) res_sel = 1;
    else if (cfg.width == 640 && cfg.height == 480) res_sel = 2;

    ap_option bitrate_opts[] = {
        { .label = "1 Mbps",  .value = "1000"  },
        { .label = "2 Mbps",  .value = "2000"  },
        { .label = "3 Mbps",  .value = "3000"  },
        { .label = "5 Mbps",  .value = "5000"  },
        { .label = "8 Mbps",  .value = "8000"  },
        { .label = "10 Mbps", .value = "10000" },
        { .label = "15 Mbps", .value = "15000" },
        { .label = "20 Mbps", .value = "20000" },
        { .label = "30 Mbps", .value = "30000" },
    };
    int br_sel = 7;
    for (int i = 0; i < 9; i++) {
        if (cfg.bitrate <= atoi(bitrate_opts[i].value)) {
            br_sel = i;
            break;
        }
    }

    ap_options_item items[] = {
        {
            .label = "Resolution",
            .type  = AP_OPT_STANDARD,
            .options = res_opts,
            .option_count = 3,
            .selected_option = res_sel,
        },
        {
            .label = "FPS",
            .type  = AP_OPT_STANDARD,
            .options = fps_opts,
            .option_count = 3,
            .selected_option = fps_sel,
        },
        {
            .label = "Bitrate",
            .type  = AP_OPT_STANDARD,
            .options = bitrate_opts,
            .option_count = 9,
            .selected_option = br_sel,
        },
    };

    ap_footer_item footer[] = {
        { .button = AP_BTN_B,    .label = "Back" },
        { .button = AP_BTN_LEFT, .label = "Change", .button_text = "←/→" },
        { .button = AP_BTN_A,    .label = "Save", .is_confirm = true },
    };

    ap_options_list_opts opts = {
        .title = "Settings",
        .items = items,
        .item_count = 3,
        .footer = footer,
        .footer_count = 3,
        .confirm_button = AP_BTN_A,
    };

    ap_options_list_result result = {0};
    int rc = ap_options_list(&opts, &result);
    if (rc != AP_OK) return;

    const char *res_val = res_opts[items[0].selected_option].value;
    if (strcmp(res_val, "1024x768") == 0) {
        cfg.width = 1024; cfg.height = 768;
    } else if (strcmp(res_val, "1280x720") == 0) {
        cfg.width = 1280; cfg.height = 720;
    } else if (strcmp(res_val, "640x480") == 0) {
        cfg.width = 640; cfg.height = 480;
    }

    cfg.fps = atoi(fps_opts[items[1].selected_option].value);
    cfg.bitrate = atoi(bitrate_opts[items[2].selected_option].value);

    ap_log("ui: saving settings res=%dx%d fps=%d bitrate=%d",
           cfg.width, cfg.height, cfg.fps, cfg.bitrate);

    if (save_config(&cfg) != 0)
        show_error("Could not save settings.");
}
