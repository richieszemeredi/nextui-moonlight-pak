#ifndef MOONLIGHT_H
#define MOONLIGHT_H

#include <stdbool.h>

#define ML_MAX_PATH     1024
#define ML_MAX_NAME      256
#define ML_MAX_IP         64
#define ML_MAX_APPS       64
#define ML_MAX_SERVERS    16

typedef struct {
    char name[ML_MAX_NAME];
} ml_app;

typedef struct {
    char name[ML_MAX_NAME];
    char host[ML_MAX_IP];
} ml_server;

typedef struct {
    int  width;
    int  height;
    int  fps;
    int  bitrate;
    int  active_server;
    int  server_count;
    ml_server servers[ML_MAX_SERVERS];
} ml_config;

/* ── config.c ────────────────────────────────────────────────── */

void      get_config_path(char *out, int out_size);
ml_config load_config(void);
int       save_config(const ml_config *cfg);

/* ── moonlight_cli.c — CLI wrappers ──────────────────────────── */

int  ml_pair(const char *host, char *pin_out, int pin_size,
             int *interrupt, char **dynamic_msg, char *msg_buf, int msg_buf_size);
int  ml_list_apps(const char *host, ml_app *apps, int max_apps, int *interrupt);
int  ml_stream(const ml_config *cfg, const char *app_name);

/* ── ui.c ────────────────────────────────────────────────────── */

typedef enum {
    MAIN_ACTION_QUIT = 0,
    MAIN_ACTION_STREAM,
    MAIN_ACTION_PAIR,
    MAIN_ACTION_SERVERS,
    MAIN_ACTION_SETTINGS,
} main_action;

main_action show_main_menu(void);
void        stream_flow(void);
void        pair_flow(void);
void        servers_flow(void);
void        show_settings(void);

#endif /* MOONLIGHT_H */
