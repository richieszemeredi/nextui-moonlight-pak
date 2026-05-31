#include "apostrophe.h"
#include "moonlight.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void get_config_path(char *out, int out_size)
{
    const char *shared = getenv("SHARED_USERDATA_PATH");
    if (shared && shared[0]) {
        snprintf(out, out_size, "%s/moonlight", shared);
    } else if (access("/mnt/SDCARD", F_OK) == 0) {
        snprintf(out, out_size, "/mnt/SDCARD/.userdata/shared/moonlight");
    } else {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(out, out_size, "%s/.userdata/shared/moonlight", home);
    }
}

static void ensure_config_dir(void)
{
    char dir[ML_MAX_PATH];
    get_config_path(dir, sizeof(dir));
    mkdir(dir, 0755);
}

static void get_config_file(char *out, int out_size)
{
    char dir[ML_MAX_PATH];
    get_config_path(dir, sizeof(dir));
    snprintf(out, out_size, "%s/config.txt", dir);
}

ml_config load_config(void)
{
    ml_config cfg = {
        .width         = 1024,
        .height        = 768,
        .fps           = 60,
        .bitrate       = 20000,
        .active_server = -1,
        .server_count  = 0,
    };

    char path[ML_MAX_PATH];
    get_config_file(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return cfg;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcmp(key, "width") == 0)
            cfg.width = atoi(val);
        else if (strcmp(key, "height") == 0)
            cfg.height = atoi(val);
        else if (strcmp(key, "fps") == 0)
            cfg.fps = atoi(val);
        else if (strcmp(key, "bitrate") == 0)
            cfg.bitrate = atoi(val);
        else if (strcmp(key, "active_server") == 0)
            cfg.active_server = atoi(val);
        else if (strncmp(key, "server.", 7) == 0) {
            int idx = -1;
            char field[32] = {0};
            if (sscanf(key, "server.%d.%31s", &idx, field) == 2 &&
                idx >= 0 && idx < ML_MAX_SERVERS) {
                if (idx >= cfg.server_count)
                    cfg.server_count = idx + 1;
                if (strcmp(field, "name") == 0)
                    snprintf(cfg.servers[idx].name, sizeof(cfg.servers[idx].name), "%s", val);
                else if (strcmp(field, "host") == 0)
                    snprintf(cfg.servers[idx].host, sizeof(cfg.servers[idx].host), "%s", val);
            }
        }
    }

    fclose(f);

    if (cfg.active_server >= cfg.server_count)
        cfg.active_server = cfg.server_count > 0 ? 0 : -1;

    return cfg;
}

int save_config(const ml_config *cfg)
{
    ensure_config_dir();

    char path[ML_MAX_PATH];
    get_config_file(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) {
        ap_log("config: failed to write %s", path);
        return -1;
    }

    fprintf(f, "width=%d\n", cfg->width);
    fprintf(f, "height=%d\n", cfg->height);
    fprintf(f, "fps=%d\n", cfg->fps);
    fprintf(f, "bitrate=%d\n", cfg->bitrate);
    fprintf(f, "active_server=%d\n", cfg->active_server);

    for (int i = 0; i < cfg->server_count; i++) {
        fprintf(f, "server.%d.name=%s\n", i, cfg->servers[i].name);
        fprintf(f, "server.%d.host=%s\n", i, cfg->servers[i].host);
    }

    fclose(f);
    ap_log("config: saved to %s (%d servers, active=%d)",
           path, cfg->server_count, cfg->active_server);
    return 0;
}
