#include "pomodoro_chime.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <stdint.h>

static const char *TAG = "pomo_chime";

#define SAMPLE_RATE   16000
#define CHUNK_SAMPLES 256
#define VOLUME_PCT    75

static TaskHandle_t s_task;
static volatile bool s_pending;
static bool s_audio_ready;

static void write_square(int16_t *buf, int n, int *phase, int period, int16_t amp)
{
    for (int i = 0; i < n; i++) {
        buf[i] = (*phase < period / 2) ? amp : (int16_t)(-amp);
        if (++(*phase) >= period) *phase = 0;
    }
}

static void write_silence(int16_t *buf, int n)
{
    for (int i = 0; i < n; i++) buf[i] = 0;
}

static void play_tone_ms(int16_t *buf, int hz, int ms, int16_t amp)
{
    if (hz <= 0 || ms <= 0) return;
    int period = SAMPLE_RATE / hz;
    if (period < 2) period = 2;
    int total = SAMPLE_RATE * ms / 1000;
    int phase = 0;
    while (total > 0) {
        int n = total < CHUNK_SAMPLES ? total : CHUNK_SAMPLES;
        write_square(buf, n, &phase, period, amp);
        bsp_audio_write(buf, (size_t)n * sizeof(int16_t));
        total -= n;
    }
}

static void play_silence_ms(int16_t *buf, int ms)
{
    int total = SAMPLE_RATE * ms / 1000;
    while (total > 0) {
        int n = total < CHUNK_SAMPLES ? total : CHUNK_SAMPLES;
        write_silence(buf, n);
        bsp_audio_write(buf, (size_t)n * sizeof(int16_t));
        total -= n;
    }
}

static void play_chime(void)
{
    if (!s_audio_ready) {
        if (bsp_audio_init() != ESP_OK) {
            ESP_LOGW(TAG, "audio init failed; skip chime");
            return;
        }
        s_audio_ready = true;
    }

    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGW(TAG, "set_format failed; skip chime");
        return;
    }
    bsp_audio_set_volume(VOLUME_PCT);

    int16_t *buf = malloc(CHUNK_SAMPLES * sizeof(int16_t));
    if (!buf) {
        ESP_LOGW(TAG, "oom; skip chime");
        return;
    }

    /* Short ding-dong: A5 then D6-ish, soft square. */
    play_tone_ms(buf, 880, 140, 5000);
    play_silence_ms(buf, 70);
    play_tone_ms(buf, 1175, 220, 5500);
    play_silence_ms(buf, 40);

    free(buf);
    ESP_LOGI(TAG, "chime done");
}

static void chime_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (s_pending) {
            s_pending = false;
            play_chime();
        } else {
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

void pomodoro_chime_notify(bool muted)
{
    if (muted) {
        ESP_LOGI(TAG, "muted; skip");
        return;
    }
    if (!s_task) {
        BaseType_t ok = xTaskCreate(chime_task, "pomo_chime", 3072, NULL, 4, &s_task);
        if (ok != pdPASS) {
            ESP_LOGW(TAG, "task create failed");
            s_task = NULL;
            return;
        }
    }
    s_pending = true;
}
