#include <soundio/soundio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct ADSREnv {
  float attack;
  float decay;
  float sustain;
  float release;
  float sustain_level;
};

float adsr(struct ADSREnv* env, float offset) {
  if(offset <= env->attack) {
    return offset / env->attack;
  } 
  offset -= env->attack;

  if (offset <= env->decay) {
    return env->sustain_level + (1 - env->sustain_level)*(1 - offset/env->decay);
  }
  offset -= env->decay;

  if (offset <= env->sustain) {
    return env->sustain_level;
  }
  offset -= env->sustain;

  if (offset <= env->release) {
    return env->sustain_level * (1 - offset/env->release);
  }

  return 0.0;
}

static const float PI = 3.1415926535f;
static float seconds_offset = 0.0f;

static struct ADSREnv envelope = { 
  .attack = 0.025, 
  .decay = 0.05, 
  .sustain = 0.0, 
  .release = 0.1, 
  .sustain_level = 0.5  };

static void write_callback(struct SoundIoOutStream *outstream,
        int frame_count_min, int frame_count_max)
{
    const struct SoundIoChannelLayout *layout = &outstream->layout;
    float float_sample_rate = outstream->sample_rate;
    float seconds_per_frame = 1.0f / float_sample_rate;
    struct SoundIoChannelArea *areas;
    int frames_left = frame_count_max;
    int err;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        if (!frame_count)
            break;

        float pitch = 440.0f;
        float pitch1 = pitch * 2.0f * PI;
        float pitch2 = pitch1 * powf(2, 3/12.0);
        float pitch3 = pitch1 * powf(2, 7/12.0);
        float pitch4 = pitch1 * powf(2, 10/12.0);
        float pitch5 = pitch1 * powf(2, 12/12.0);
        for (int frame = 0; frame < frame_count; frame += 1) {
            float t = seconds_offset + frame * seconds_per_frame;
	    float chord = sinf(t*pitch1) + 
	                  sinf(t*pitch2) + 
	                  sinf(t*pitch3) + 
	                  sinf(t*pitch4) + 
	                  sinf(t*pitch5);
            float sample = 0.1 * adsr(&envelope, t) * chord;
            //printf("env at %g -> %g\n", t, adsr(&envelope,t));
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                float *ptr = (float*)(areas[channel].ptr + areas[channel].step * frame);
                *ptr = sample;
            }
        }
        seconds_offset = fmodf(seconds_offset + seconds_per_frame * frame_count, 1.0f);

        if ((err = soundio_outstream_end_write(outstream))) {
            fprintf(stderr, "%s\n", soundio_strerror(err));
            exit(1);
        }

        frames_left -= frame_count;
    }
}

int main(int argc, char **argv) {
    int err;
    struct SoundIo *soundio = soundio_create();
    if (!soundio) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    if ((err = soundio_connect(soundio))) {
        fprintf(stderr, "error connecting: %s", soundio_strerror(err));
        return 1;
    }

    soundio_flush_events(soundio);

    int default_out_device_index = soundio_default_output_device_index(soundio);
    if (default_out_device_index < 0) {
        fprintf(stderr, "no output device found");
        return 1;
    }

    struct SoundIoDevice *device = soundio_get_output_device(soundio, default_out_device_index);
    if (!device) {
        fprintf(stderr, "out of memory");
        return 1;
    }

    fprintf(stderr, "Output device: %s\n", device->name);

    struct SoundIoOutStream *outstream = soundio_outstream_create(device);
    outstream->format = SoundIoFormatFloat32NE;
    outstream->write_callback = write_callback;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
        return 1;
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s", soundio_strerror(err));
        return 1;
    }

    for (;;)
        soundio_wait_events(soundio);

    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
    soundio_destroy(soundio);
    return 0;
}
