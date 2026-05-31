#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ML_MAX_NAME 256
#define ML_MAX_IP    64

typedef struct { char name[ML_MAX_NAME]; } ml_app;

static int parse_app_line(const char *line, char *out, int out_size)
{
    int app_num;
    char app_name[ML_MAX_NAME];
    char *first_dot = strchr(line, '.');
    if (first_dot && first_dot[1] == ' ' &&
        sscanf(line, "%d. %255[^\n]", &app_num, app_name) == 2 && app_name[0]) {
        snprintf(out, out_size, "%s", app_name);
        return app_num;
    }
    return -1;
}

static void parse_pin(const char *line, char *pin_out, int pin_size)
{
    const char *pin_marker = strstr(line, "PIN");
    if (!pin_marker) return;
    const char *digit = pin_marker;
    while (*digit && (*digit < '0' || *digit > '9')) digit++;
    if (!*digit) return;
    const char *end = digit;
    while (*end >= '0' && *end <= '9') end++;
    int len = (int)(end - digit);
    if (len > 0 && len < pin_size) {
        memcpy(pin_out, digit, len);
        pin_out[len] = '\0';
    }
}

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %s... ", #name); \
} while(0)

#define PASS() do { tests_passed++; printf("ok\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

static void test_parse_app_lines(void)
{
    char name[ML_MAX_NAME];

    TEST(numbered_app);
    assert(parse_app_line("1. Desktop", name, sizeof(name)) == 1);
    assert(strcmp(name, "Desktop") == 0);
    PASS();

    TEST(multi_word_app);
    assert(parse_app_line("2. Steam Big Picture", name, sizeof(name)) == 2);
    assert(strcmp(name, "Steam Big Picture") == 0);
    PASS();

    TEST(high_number);
    assert(parse_app_line("15. My Game", name, sizeof(name)) == 15);
    assert(strcmp(name, "My Game") == 0);
    PASS();

    TEST(skip_status_line);
    assert(parse_app_line("Connecting to 192.168.0.252...", name, sizeof(name)) == -1);
    PASS();

    TEST(skip_ip_line);
    assert(parse_app_line("192.168.0.252", name, sizeof(name)) == -1);
    PASS();

    TEST(skip_empty);
    assert(parse_app_line("", name, sizeof(name)) == -1);
    PASS();

    TEST(skip_error_msg);
    assert(parse_app_line("You must pair with the PC first", name, sizeof(name)) == -1);
    PASS();

    TEST(skip_dot_in_text);
    assert(parse_app_line("Can't connect to server 192.168.0.1", name, sizeof(name)) == -1);
    PASS();
}

static void test_parse_pin(void)
{
    char pin[16];

    TEST(standard_pin);
    pin[0] = '\0';
    parse_pin("Please enter the following PIN on the target PC: 1234", pin, sizeof(pin));
    assert(strcmp(pin, "1234") == 0);
    PASS();

    TEST(short_pin);
    pin[0] = '\0';
    parse_pin("PIN: 42", pin, sizeof(pin));
    assert(strcmp(pin, "42") == 0);
    PASS();

    TEST(long_pin);
    pin[0] = '\0';
    parse_pin("Enter PIN 98765 on host", pin, sizeof(pin));
    assert(strcmp(pin, "98765") == 0);
    PASS();

    TEST(no_pin);
    pin[0] = '\0';
    parse_pin("Connecting to server...", pin, sizeof(pin));
    assert(pin[0] == '\0');
    PASS();

    TEST(no_digits_after_pin);
    pin[0] = '\0';
    parse_pin("PIN on target", pin, sizeof(pin));
    assert(pin[0] == '\0');
    PASS();
}

static void test_config_format(void)
{
    TEST(server_key_parse);
    int idx = -1;
    char field[32] = {0};
    assert(sscanf("server.0.name", "server.%d.%31s", &idx, field) == 2);
    assert(idx == 0);
    assert(strcmp(field, "name") == 0);
    PASS();

    TEST(server_key_host);
    idx = -1;
    field[0] = '\0';
    assert(sscanf("server.3.host", "server.%d.%31s", &idx, field) == 2);
    assert(idx == 3);
    assert(strcmp(field, "host") == 0);
    PASS();

    TEST(non_server_key);
    idx = -1;
    assert(sscanf("width", "server.%d.%31s", &idx, field) != 2);
    PASS();
}

int main(void)
{
    printf("Running parser tests:\n");
    test_parse_app_lines();
    test_parse_pin();
    test_config_format();
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
