#include "rtmidi_c.h"
#include <soundio/soundio.h>

#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
//#define M_PI 3.14159265358979323846264338327950288
// static float seconds_offset = 0.0f;

struct ADSREnv
{
  float attack;
  float decay;
  float sustain_level;

  float released;
  float release;
};

bool adsr_done(struct ADSREnv *env, float offset) {
  return env->released >= 0.0 && offset - env->released >= env->release;
}

float adsr(struct ADSREnv *env, float offset)
{
  if (offset <= env->attack)
  {
    return offset / env->attack;
  }

  if (offset <= env->decay)
  {
    return env->sustain_level + (1 - env->sustain_level) * (1 - offset / env->decay);
  }

  if (env->released < 0.0) {
    return env->sustain_level;
  }

  offset -= env->released;

  if (offset <= env->release) {
    return env->sustain_level * (1 - offset / env->release);
  }

  return 0.0;
}
  static float relative_mod = 1.0;

struct Voice
{
  float amp;
  float phase;
  float modphase;
  float modvalue;
  float basefreq;
  float curr_basefreq;
  float elapsed_time;
  struct ADSREnv envelope;
};

struct Voice new_voice(float freq)
{
  struct Voice v = {
      .phase = 0.0f,
      .amp = 1.0f,
      .modphase = 0.0f,
      .modvalue = 0.0f,
      .basefreq = freq,
      .curr_basefreq = freq,
      // .relative_mod = 1.0f,
      .elapsed_time = 0.0f,
      .envelope = {
        .attack = 0.1,
        .decay = 0.2,
        .release = 2.0,
        .sustain_level = 0.6,
        .released = -1.0,
      }
  };
  return v;
}

float sample_voice(struct Voice *v, float dt)
{
  v->curr_basefreq += (v->basefreq - v->curr_basefreq) * 0.001;

  v->modphase += 2 * M_PI * dt * relative_mod * v->curr_basefreq;
  v->modphase = fmodf(v->modphase, 2 * M_PI);
  v->modvalue = sinf(v->modphase);

  v->phase += 2 * M_PI * v->curr_basefreq * dt;
  v->phase = fmodf(v->phase, 2 * M_PI);

  float sample = v->amp * 0.3 * sinf(v->phase + v->modvalue) * adsr(&v->envelope, v->elapsed_time);
  v->elapsed_time += dt;
  return sample;
}

static const int NUM_VOICES = 100;
struct Voice* voices[100] = {NULL};

float sample(double dt)
{
  float sample = 0.0;
  for (int i = 0; i < NUM_VOICES; i++)
  {
    if (voices[i] != NULL)
    {
      // printf("voice %d active", i);
      sample += sample_voice(voices[i], dt);
      if(adsr_done(&voices[i]->envelope, voices[i]->elapsed_time)) {
        printf("Freeing voice %d\n",i);
        free(voices[i]);
        voices[i] = NULL;
      }
    }
  }

  return sample;
}

static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max)
{
  //printf("Please write %d -- %d frame_count\n", frame_count_min, frame_count_max);
  const struct SoundIoChannelLayout *layout = &outstream->layout;
  float float_sample_rate = outstream->sample_rate;
  float seconds_per_frame = 1.0f / float_sample_rate;
  struct SoundIoChannelArea *areas;
  int frames_left = 1000000;
  if (frame_count_max < frames_left)
  {
    frames_left = frame_count_max;
  }
  if (frame_count_min > frames_left)
  {
    frames_left = frame_count_min;
  }

  int err;

  while (frames_left > 0)
  {
    int frame_count = frames_left;

    if ((err =
             soundio_outstream_begin_write(outstream, &areas, &frame_count)))
    {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    if (!frame_count)
      break;

    for (int frame = 0; frame < frame_count; frame += 1)
    {

      // long double t = seconds_offset + frame * seconds_per_frame;

      for (int channel = 0; channel < layout->channel_count; channel += 1)
      {
        float *ptr =
            (float *)(areas[channel].ptr + areas[channel].step * frame);
        *ptr = sample(seconds_per_frame);
      }
    }

    // seconds_offset = seconds_offset + seconds_per_frame * frame_count;

    if ((err = soundio_outstream_end_write(outstream)))
    {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    frames_left -= frame_count;
  }
}

void midi_msg_in_cb(double timestamp, const unsigned char *message, size_t message_size, void *user_data)
{

  if (message_size == 3)
  {
    if (message[0] == 144)
    {
      // NOTE ON
      char note = message[1];
      char velocity = message[2];
      printf(" NOTEON note %d vel %d \n", note, velocity);
      float frequency = powf(2.0, (note - 69.0) / 12.0) * 440.0;

      for(int i = 0; i < NUM_VOICES; i++) {
        if(voices[i] == NULL) {
          printf("Adding voice %d\n", i);
          voices[i] = malloc(sizeof(struct Voice));
          *voices[i] = new_voice(frequency);
          voices[i]->amp = (float)velocity / 127.0f;
          break;
        }
      }
    }
    else if (message[0] == 128)
    {
      // NOTE OFF
      char note = message[1];
      char velocity = message[2];
      float frequency = powf(2.0, (note - 69.0) / 12.0) * 440.0;
      printf(" NOTEOFF note %d vel %d \n", note, velocity);
      for(int i = 0; i < NUM_VOICES; i++) {
        if(voices[i] != NULL && voices[i]->basefreq == frequency && voices[i]->envelope.released < 0.0) {
          voices[i]->envelope.released = voices[i]->elapsed_time;
        }
      }
    }
    else if (message[0] == 176)
    {
      // KNOB
      char note = message[1];
      char velocity = message[2];
      printf(" KNOB note %d vel %d \n", note, velocity);

      relative_mod = ((float)velocity / 127.0) * 3.0 + 0.5;
    }
    else
    {
      printf("Unknown midi msg");
      for (int i = 0; i < message_size; i++)
      {
        printf(" %d ", message[i]);
      }
      printf("\n");
    }
  }
  else
  {
    printf("Unknown midi msg.\n");
  }
}

int main(int argc, char **argv)
{
  int err;
  struct SoundIo *soundio = soundio_create();
  if (!soundio)
  {
    fprintf(stderr, "out of memory\n");
    return 1;
  }

  if ((err = soundio_connect(soundio)))
  {
    fprintf(stderr, "error connecting: %s", soundio_strerror(err));
    return 1;
  }

  soundio_flush_events(soundio);

  int default_out_device_index = soundio_default_output_device_index(soundio);
  if (default_out_device_index < 0)
  {
    fprintf(stderr, "no output device found");
    return 1;
  }

  struct SoundIoDevice *device =
      soundio_get_output_device(soundio, default_out_device_index);
  if (!device)
  {
    fprintf(stderr, "out of memory");
    return 1;
  }

  fprintf(stderr, "Output device: %s\n", device->name);

  struct SoundIoOutStream *outstream = soundio_outstream_create(device);
  outstream->format = SoundIoFormatFloat32NE;
  outstream->write_callback = write_callback;
  printf("Default software latency %g\n", outstream->software_latency);
  outstream->software_latency = 0.001;

  if ((err = soundio_outstream_open(outstream)))
  {
    fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
    return 1;
  }
  printf("Default software latency %g\n", outstream->software_latency);

  if (outstream->layout_error)
    fprintf(stderr, "unable to set channel layout: %s\n",
            soundio_strerror(outstream->layout_error));

  if ((err = soundio_outstream_start(outstream)))
  {
    fprintf(stderr, "unable to start device: %s", soundio_strerror(err));
    return 1;
  }

  // INIT MIDI
  struct RtMidiWrapper *midiin = rtmidi_in_create_default();

  ////unsigned char buf[1024];
  ////int buf_len = sizeof(buf);
  ////unsigned int ports = rtmidi_get_port_count(midiin);
  ////printf("port count: %d\n", ports);
  ////for(int i = 0; i < ports; i++) {
  ////        rtmidi_get_port_name(midiin, i, buf, &buf_len);
  ////        printf("name: %s\n", buf);
  ////}

  ////printf("Opening 2\n");

  rtmidi_open_port(midiin, 2, "midi2osc");
  rtmidi_in_set_callback(midiin, &midi_msg_in_cb, NULL);

  for (;;)
  {
    soundio_wait_events(soundio);
  }

  soundio_outstream_destroy(outstream);
  soundio_device_unref(device);
  soundio_destroy(soundio);
  rtmidi_in_free(midiin);
}
