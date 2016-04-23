/*
 * Rut
 *
 * Rig Utilities
 *
 * Copyright (C) 2016  Robert Bragg
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <rut-config.h>

#include <stdint.h>
#include <stdbool.h>

#include <alsa/asoundlib.h>

#include <clib.h>

#include "rut-shell.h"
#include "rut-alsa-shell.h"

#include <math.h>

void
rut_debug_generate_sine_audio(rut_shell_t *shell, rut_audio_chunk_t *chunk)
{
    rut_channel_layout_t *layouts = chunk->channels;
    int n_layouts = chunk->n_channels;
    int n_frames = chunk->n_frames;
    static double phase = 0;
    double phase_len = 2.0 * M_PI;
    double sine_freq_hz = 300;
    double step = phase_len * sine_freq_hz / (double)shell->pcm_freq;

    for (int i = 0; i < n_frames; i++) {
        int val = sin(phase) * INT16_MAX;

        for (int c = 0; c < n_layouts; c++) {
            int16_t *sample = (int16_t *)(chunk->data +
                                          layouts[c].offset +
                                          layouts[c].stride * i);
            *sample = val;
        }

        phase += step;
        if (phase >= phase_len)
            phase -= phase_len;
    }
}

static void
pcm_dispatch_cb(void *user_data, int fd, int revents)
{
    rut_shell_t *shell = user_data;
    int n_pollfds = shell->pcm_n_pollfds;
    struct pollfd *pollfds = shell->pcm_pollfds;
    unsigned short pcm_revents = 0;

    for (int i = 0; i < n_pollfds; i++) {
        if (pollfds[i].fd == fd)
            pollfds[i].revents = revents;
        else
            pollfds[i].revents = 0;
    }

    snd_pcm_poll_descriptors_revents(shell->pcm,
                                     pollfds,
                                     n_pollfds,
                                     &pcm_revents);
    if (pcm_revents & POLLOUT) {
        rut_audio_chunk_t *chunk =
            c_list_first(&shell->pcm_chunk_queue, rut_audio_chunk_t, link);
            //rut_audio_chunk_new(shell);

        snd_pcm_status(shell->pcm, shell->pcm_status);

        if (chunk) {
            int ret;

            c_list_remove(&chunk->link);

            /* Audio chunk that eventually get queued for output are
             * assumed to be be layed out according to the output
             * device...
             */
            c_warn_if_fail(chunk->channels != shell->pcm_channel_layouts);
            c_warn_if_fail(chunk->n_channels != shell->pcm_n_channels);

            do {
                //rut_debug_generate_sine_audio(shell, chunk);
                ret = snd_pcm_writei(shell->pcm,
                                     chunk->data,
                                     chunk->n_frames);
                switch (ret) {
                case -EBADF:
                    c_warning("PCM not in running state, ready accept write");
                    break;
                case -EPIPE:
                    c_warning("Audio underrun");
                    /* FIXME: in this case it's likely we also need to
                     * discard some number of out-of-date queued
                     * chunks.
                     */
                    break;
                case -ESTRPIPE:
                    c_warning("PCM stream suspended, not ready to accept write");
                    break;
                }

                if (ret < 0)
                    ret = snd_pcm_recover(shell->pcm, ret, false /* no output */);
                else {
                    /* TODO: record status information in chunk for
                     * debug feedback */
                }

                /* TODO: pool/reuse audio chunks */
                rut_object_unref(chunk);

            } while (ret == -EINTR); /* not obvious if EINTR is handled
                                        for us by Alsa */
        }
    }
}

static void
rut_alsa_shell_audio_chunk_init(rut_shell_t *shell, rut_audio_chunk_t *chunk)
{
    chunk->n_channels = shell->pcm_n_channels;
    chunk->channels = shell->pcm_channel_layouts;

    chunk->n_frames = shell->pcm_period_n_frames;
    chunk->data_len = chunk->n_frames * chunk->n_channels * 2; /* assumes 16bit format a.t.m */
    chunk->data = c_malloc0(chunk->data_len);
}

bool
rut_alsa_shell_init(rut_shell_t *shell)
{
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *params;
    int ret;

    ret = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (ret < 0) {
        c_warning("Failed to open PCM device");
        pcm = NULL;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm, params);

#if C_BYTE_ORDER == C_LITTLE_ENDIAN
    ret = snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
#else
    ret = snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_BE);
#endif
    if (ret) {
        c_warning("Failed to set S16 format on PCM device");
        snd_pcm_close(pcm);
    }

    snd_pcm_status_malloc(&shell->pcm_status);

    int n_channels = 2;
    ret = snd_pcm_hw_params_set_channels(pcm, params, n_channels);
    if (ret) {
        c_warning("Failed to set up single channel on PCM device");
        snd_pcm_close(pcm);
    }
    unsigned int frame_size = n_channels * 2;

    /* XXX: not sure if we can we expect to basically always get
     * what we want here? */
    shell->pcm_freq = 22050;
    snd_pcm_hw_params_set_rate_near(pcm, params,
                                    &shell->pcm_freq,
                                    NULL /* direction */);

#warning "remove hacked audio latency"
    /* Corresponds to ~30ms... */
    shell->pcm_period_n_frames = (shell->pcm_freq * 300) / 1000;
    snd_pcm_hw_params_set_period_size_near(pcm, params,
                                           &shell->pcm_period_n_frames,
                                           NULL /* direction */);

    shell->pcm_buffer_n_frames = shell->pcm_period_n_frames * 2;
    snd_pcm_hw_params_set_buffer_size_near(pcm, params,
                                           &shell->pcm_buffer_n_frames);

    snd_pcm_hw_params(pcm, params);

    snd_pcm_sw_params_t *swparams;

    snd_pcm_sw_params_alloca(&swparams);
    snd_pcm_sw_params_current(pcm, swparams);

    //snd_pcm_sw_params_set_avail_min(pcm, swparams, shell->pcm_period_n_frames);
    snd_pcm_sw_params_set_avail_min(pcm, swparams, shell->pcm_buffer_n_frames);
    snd_pcm_sw_params_set_start_threshold(pcm, swparams, shell->pcm_period_n_frames);
    snd_pcm_sw_params_set_period_event(pcm, swparams, true);

    snd_pcm_sw_params(pcm, swparams);

    shell->pcm = pcm;
    shell->pcm_buf = c_malloc(shell->pcm_period_n_frames * frame_size);

    /* FIXME: very hacky... */
    rut_channel_layout_t *channels = c_new0(rut_channel_layout_t, n_channels);
    for (int i = 0; i < n_channels; i++) {
        channels[i].offset = 2 * i;
        channels[i].stride = 2 * n_channels;
        channels[i].format = RUT_CHANNEL_FORMAT_S16_SYS;
        channels[i].type = i;
    }
    shell->pcm_channel_layouts = channels;
    shell->pcm_n_channels = n_channels;

    int n_pollfds = snd_pcm_poll_descriptors_count(pcm);
    if (n_pollfds > 0) {
        shell->pcm_pollfds = c_new0(struct pollfd, n_pollfds);
        shell->pcm_event_sources = c_new0(rut_poll_source_t *, n_pollfds);

        ret = snd_pcm_poll_descriptors(pcm, shell->pcm_pollfds, n_pollfds);
        if (ret == n_pollfds) {
            for (int i = 0; i < n_pollfds; i++) {
                /* Not sure why Alsa tends to add POLLNVAL to events */
                shell->pcm_pollfds[i].events &= ~POLLNVAL;

                shell->pcm_event_sources[i] =
                    rut_poll_shell_add_fd(shell,
                                          shell->pcm_pollfds[i].fd,
                                          shell->pcm_pollfds[i].events,
                                          NULL, /* prepare */
                                          pcm_dispatch_cb,
                                          shell);
            }
            shell->pcm_n_pollfds = n_pollfds;
        } else {
            c_free(shell->pcm_pollfds);
            shell->pcm_pollfds = NULL;

            c_free(shell->pcm_event_sources);
            shell->pcm_event_sources = NULL;

            c_warning("Failed to query Alsa PCM file descriptors to poll");
        }
    } else {
        c_warning("Failed to query Alsa PCM file descriptor count");
    }

    shell->platform.audio_chunk_init = rut_alsa_shell_audio_chunk_init;

    return true;
}
