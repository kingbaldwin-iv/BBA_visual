#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "raylib.h"
#include <libwebsockets.h>
#include <signal.h>
#include <limits.h>

#define TWHITE CLITERAL(Color) {255, 255, 255, 100}
#define SWIDTH 1200
#define SHEIGHT 800
#define MARGIN 20
#define BUFF_LEN 2000
#define FPS 100
static int interrupt_signal = 0;
static struct lws * web_socket = NULL;

typedef struct BBA {
    float bid;
    float ask;
}
BBA;
static BBA pass = {
    0.0f,
    0.0f
};
static void signal_handler(int sig) {
    interrupt_signal = 1;
}
typedef struct circ_buffer {
    float vals[BUFF_LEN];
    int start;
    int end;
    float max;
    float min;
    char full;
}
circ_buffer;
void append(circ_buffer * cb, float val) {
    cb -> end = (cb -> end + 1) % BUFF_LEN;
    cb -> vals[cb -> end] = val;
    if (cb -> full) cb -> start = (cb -> start + 1) % BUFF_LEN;
    if (cb -> end == BUFF_LEN - 1) cb -> full = 1;
    if (val < cb -> min) cb -> min = val;
    if (val > cb -> max) cb -> max = val;
}
float extract_price(const char * json, char is_bid) {
    const char * key = is_bid ? "\"b\":\"" : "\"a\":\"";
    char * start = strstr(json, key);
    if (!start) return 0.0f;
    start += strlen(key);
    char * end = strchr(start, '"');
    if (!end) return 0.0f;
    size_t len = end - start;
    char number[64] = {
        0
    };
    if (len >= sizeof(number)) return 0.0f;
    strncpy(number, start, len);
    number[len] = '\0';
    return (float) atof(number);
}

static int callback_binance(struct lws * wsi, enum lws_callback_reasons reason, void * user, void * in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf("Connected\n");
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        pass.bid = extract_price((const char * ) in, 1);
        pass.ask = extract_price((const char * ) in, 0);

        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf("Connection error\n");
        web_socket = NULL;
        break;

    case LWS_CALLBACK_CLOSED:
        printf("Connection closed\n");
        web_socket = NULL;
        break;

    default:
        break;
    }
    return 0;
}

void gen_plot_canvas(int screenWidth, int screenHeight) {
    DrawRectangleLines(MARGIN, MARGIN, screenWidth - MARGIN * 2 - 50.0f, screenHeight - MARGIN * 2, TWHITE);
}

int main(void) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return EXIT_FAILURE;
    }

    if (pid == 0) { // Child process

        close(pipe_fd[1]); // Close write end
        InitWindow(SWIDTH, SHEIGHT, "BBA Visual");
        SetTargetFPS(FPS);
        BBA received;
        circ_buffer bids;
        bids.start = 0;
        bids.end = -1;
        bids.max = INT_MIN;
        bids.min = INT_MAX;
        bids.full = 0;
        circ_buffer asks;
        asks.start = 0;
        asks.end = -1;
        asks.max = INT_MIN;
        asks.min = INT_MAX;
        asks.full = 0;
        const float x_delta = (float)(SWIDTH - 2 * MARGIN - 50.0f) / BUFF_LEN;
        const int x_load = 2 * MARGIN;
        const int y_load = SHEIGHT / 2 - MARGIN / 2;
        const int x_load_end = SWIDTH - 50 - 4 * MARGIN;
        const float canv_height = (float)(SHEIGHT - 2 * MARGIN);
        const float canv_bot_y = SHEIGHT - MARGIN;
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(BLACK);

            ssize_t result = read(pipe_fd[0], & received, sizeof(received));
            if (result <= 0) {
                break;
            }
            float cur_x = SWIDTH - (float) MARGIN - 50.0f;
            append( & bids, received.bid);
            append( & asks, received.ask);
            float max = asks.max;
            float min = bids.min;
            float y_delta = canv_height / (max - min);
            if (!bids.full) {
                int prog = (int)(((float) bids.end) / BUFF_LEN * x_load_end);
                DrawRectangleLines(x_load, y_load, x_load_end, MARGIN, WHITE);
                DrawRectangle(x_load, y_load, prog, MARGIN, WHITE);
            } else {
                int it = bids.end;
                char con[64];
                snprintf(con, sizeof(con), "%f", bids.vals[it]);
                float pos_t = canv_bot_y - (bids.vals[it] - min) * y_delta;
                DrawTextEx(GetFontDefault(), con, (Vector2) {
                    cur_x,
                    pos_t
                }, 10, 1, WHITE);
                pos_t = canv_bot_y - (asks.vals[it] - min) * y_delta;
                snprintf(con, sizeof(con), "%f", asks.vals[it]);
                DrawTextEx(GetFontDefault(), con, (Vector2) {
                    cur_x,
                    pos_t
                }, 10, 1, WHITE);
                int n = -1;
                for (int i = 0; i < BUFF_LEN - 1; i++) {
                    it = (it + BUFF_LEN - 1) % BUFF_LEN;
                    n = (it + 1) % BUFF_LEN;
                    Vector2 snd = {
                        cur_x,
                        canv_bot_y - (bids.vals[n] - min) * y_delta
                    };
                    Vector2 snd2 = {
                        cur_x,
                        canv_bot_y - (asks.vals[n] - min) * y_delta
                    };
                    cur_x -= x_delta;
                    Vector2 fst = {
                        cur_x,
                        canv_bot_y - (bids.vals[it] - min) * y_delta
                    };
                    Vector2 fst2 = {
                        cur_x,
                        canv_bot_y - (asks.vals[it] - min) * y_delta
                    };
                    DrawLineV(fst, snd, GREEN);
                    DrawLineV(fst2, snd2, RED);
                }
            }
            gen_plot_canvas(SWIDTH, SHEIGHT);
            EndDrawing();
        }

        close(pipe_fd[0]);
        CloseWindow();
    } else {
        close(pipe_fd[0]);
        signal(SIGINT, signal_handler);

        struct lws_context_creation_info info = {
            0
        };
        info.port = CONTEXT_PORT_NO_LISTEN;
        info.protocols = (struct lws_protocols[]) {
            {
                "binance-ws",
                callback_binance,
                0,
                0
            }, {
                NULL,
                NULL,
                0,
                0
            }
        };
        info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

        struct lws_context * context = lws_create_context( & info);
        if (!context) {
            printf("Failed to create context\n");
            return -1;
        }

        struct lws_client_connect_info ccinfo = {
            0
        };
        ccinfo.context = context;
        ccinfo.address = "stream.binance.com";
        ccinfo.port = 9443;
        ccinfo.path = "/ws/solusdt@bookTicker";
        ccinfo.host = ccinfo.address;
        ccinfo.origin = ccinfo.address;
        ccinfo.protocol = "binance-ws";
        ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
        web_socket = lws_client_connect_via_info( & ccinfo);

        while (!interrupt_signal) {
            lws_service(context, 100);

            if (pass.bid > 0.0f)
                write(pipe_fd[1], & pass, sizeof(pass));

        }

        close(pipe_fd[1]); // Close write end
    }

    return 0;
}
