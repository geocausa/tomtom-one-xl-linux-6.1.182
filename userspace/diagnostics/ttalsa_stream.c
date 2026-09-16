#define _POSIX_C_SOURCE 200809L
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define APP_RATE 44100U
#define CHANNELS 2U
#define TEST_SECONDS 30U
#define BUFFER_US 100000U
#define PERIOD_US 20000U

static long long us_diff(const struct timespec *a, const struct timespec *b) {
    return (long long)(b->tv_sec - a->tv_sec) * 1000000LL +
           (long long)(b->tv_nsec - a->tv_nsec) / 1000LL;
}

static int setup_pcm(snd_pcm_t **out, snd_pcm_uframes_t *buffer_frames,
                     snd_pcm_uframes_t *period_frames) {
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *hw;
    snd_pcm_sw_params_t *sw;
    unsigned int rate = APP_RATE;
    unsigned int buffer_time = BUFFER_US;
    unsigned int period_time = PERIOD_US;
    int dir = 0;
    int err;

    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_sw_params_alloca(&sw);

    if ((err = snd_pcm_open(&pcm, "ttbt", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_access(pcm, hw,
                    SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_format(pcm, hw,
                    SND_PCM_FORMAT_S16_LE)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_channels(pcm, hw, CHANNELS)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, &dir)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_buffer_time_near(
                    pcm, hw, &buffer_time, &dir)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params_set_period_time_near(
                    pcm, hw, &period_time, &dir)) < 0)
        goto fail;
    if ((err = snd_pcm_hw_params(pcm, hw)) < 0)
        goto fail;
    if ((err = snd_pcm_get_params(pcm, buffer_frames, period_frames)) < 0)
        goto fail;

    if ((err = snd_pcm_sw_params_current(pcm, sw)) < 0)
        goto fail;

    snd_pcm_uframes_t threshold =
        (*buffer_frames / *period_frames) * *period_frames;
    if ((err = snd_pcm_sw_params_set_start_threshold(pcm, sw, threshold)) < 0)
        goto fail;
    if ((err = snd_pcm_sw_params_set_avail_min(pcm, sw, *period_frames)) < 0)
        goto fail;
    if ((err = snd_pcm_sw_params(pcm, sw)) < 0)
        goto fail;
    if ((err = snd_pcm_prepare(pcm)) < 0)
        goto fail;

    printf("PCM opened: app_rate=%u actual_app_rate=%u buffer_us=%u period_us=%u "
           "buffer_frames=%lu period_frames=%lu\n",
           APP_RATE, rate, buffer_time, period_time,
           (unsigned long)*buffer_frames, (unsigned long)*period_frames);

    *out = pcm;
    return 0;

fail:
    fprintf(stderr, "ALSA setup failed: %s\n", snd_strerror(err));
    if (pcm != NULL)
        snd_pcm_close(pcm);
    return -1;
}

int main(void) {
    snd_pcm_t *pcm = NULL;
    snd_pcm_uframes_t buffer_frames = 0, period_frames = 0;

    if (setup_pcm(&pcm, &buffer_frames, &period_frames) != 0)
        return 2;

    size_t samples = (size_t)period_frames * CHANNELS;
    int16_t *buf = calloc(samples, sizeof(*buf));
    if (buf == NULL) {
        fprintf(stderr, "allocation failed\n");
        snd_pcm_close(pcm);
        return 3;
    }

    uint32_t phase = 0;
    const uint32_t phase_step =
        (uint32_t)(((uint64_t)440U << 32) / APP_RATE);
    const uint64_t target_frames = (uint64_t)APP_RATE * TEST_SECONDS;
    uint64_t frames_done = 0;
    unsigned int xruns = 0;
    unsigned int short_writes = 0;
    long long max_write_us = 0;
    long long writes_over_30ms = 0;
    long long writes_over_50ms = 0;

    struct timespec start, finish;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (frames_done < target_frames) {
        snd_pcm_uframes_t frames = period_frames;
        if (target_frames - frames_done < frames)
            frames = (snd_pcm_uframes_t)(target_frames - frames_done);

        for (snd_pcm_uframes_t i = 0; i < frames; i++) {
            uint32_t p = phase >> 16;
            int32_t tri = p < 32768U ?
                (int32_t)p * 2 - 32768 :
                98303 - (int32_t)p * 2;
            int16_t s = (int16_t)(tri / 3);
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
            phase += phase_step;
        }

        snd_pcm_uframes_t off = 0;
        while (off < frames) {
            struct timespec a, b;
            clock_gettime(CLOCK_MONOTONIC, &a);
            snd_pcm_sframes_t n = snd_pcm_writei(
                pcm, buf + off * CHANNELS, frames - off);
            clock_gettime(CLOCK_MONOTONIC, &b);

            long long dt = us_diff(&a, &b);
            if (dt > max_write_us)
                max_write_us = dt;
            if (dt > 30000)
                writes_over_30ms++;
            if (dt > 50000)
                writes_over_50ms++;

            if (n == -EPIPE) {
                xruns++;
                fprintf(stderr, "XRUN at app_frame=%llu\n",
                    (unsigned long long)(frames_done + off));
                if (snd_pcm_prepare(pcm) < 0)
                    goto write_fail;
                continue;
            }
            if (n < 0) {
                n = snd_pcm_recover(pcm, (int)n, 1);
                if (n < 0)
                    goto write_fail;
                continue;
            }
            if ((snd_pcm_uframes_t)n < frames - off)
                short_writes++;
            off += (snd_pcm_uframes_t)n;
        }

        frames_done += frames;
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);
    long long feed_us = us_diff(&start, &finish);

    struct timespec da, db;
    clock_gettime(CLOCK_MONOTONIC, &da);
    int drain_rc = snd_pcm_drain(pcm);
    clock_gettime(CLOCK_MONOTONIC, &db);
    long long drain_us = us_diff(&da, &db);

    printf("RESULT feed_frames=%llu nominal_seconds=%u feed_wall_us=%lld "
           "xruns=%u short_writes=%u max_write_us=%lld "
           "writes_gt30ms=%lld writes_gt50ms=%lld drain_rc=%d drain_us=%lld\n",
           (unsigned long long)frames_done, TEST_SECONDS, feed_us,
           xruns, short_writes, max_write_us,
           writes_over_30ms, writes_over_50ms, drain_rc, drain_us);

    free(buf);
    snd_pcm_close(pcm);
    return drain_rc < 0 ? 5 : 0;

write_fail:
    fprintf(stderr, "ALSA write failed at app_frame=%llu: %s\n",
            (unsigned long long)frames_done, snd_strerror(errno));
    free(buf);
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    return 4;
}
