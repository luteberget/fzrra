#include <soundio/soundio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ADSREnv {
  float attack;
  float decay;
  float sustain;
  float release;
  float sustain_level;
};

float adsr(struct ADSREnv *env, float offset) {
  if (offset <= env->attack) {
    return offset / env->attack;
  }
  offset -= env->attack;

  if (offset <= env->decay) {
    return env->sustain_level +
           (1 - env->sustain_level) * (1 - offset / env->decay);
  }
  offset -= env->decay;

  if (offset <= env->sustain) {
    return env->sustain_level;
  }
  offset -= env->sustain;

  if (offset <= env->release) {
    return env->sustain_level * (1 - offset / env->release);
  }

  return 0.0;
}

static const float PI = 3.1415926535f;
static float seconds_offset = 0.0f;
static char *line = NULL;
static ssize_t length = 0;

static struct ADSREnv envelope = {.attack = 0.1,
                                  .decay = 0.1,
                                  .sustain = 0.8,
                                  .release = 0.2,
                                  .sustain_level = 0.5};
static struct ADSREnv envelope2 = {.attack = 0.1,
                                  .decay = 0.1,
                                  .sustain = 0.8,
                                  .release = 0.2,
                                  .sustain_level = 0.2};


float saw(float t) {
  // periodic at 2*pi
  return fmodf((-PI + t) / PI, 2 * PI);
}

float charpitch(char c) {
  float a = 440.0f;
  if(c == 'C') return a * powf(2.0, -21.0 / 12.0);
  if(c == 'D') return a * powf(2.0, -19.0 / 12.0);
  if(c == 'E') return a * powf(2.0, -17.0 / 12.0);
  if(c == 'F') return a * powf(2.0, -16.0 / 12.0);
  if(c == 'G') return a * powf(2.0, -14.0 / 12.0);
  if(c == 'A') return a * powf(2.0, -12.0 / 12.0);
  if(c == 'H') return a * powf(2.0, -10.0 / 12.0);
  if(c == 'c') return a * powf(2.0, -9.0 / 12.0);
  if(c == 'd') return a * powf(2.0, -7.0 / 12.0);
  if(c == 'e') return a * powf(2.0, -6.0 / 12.0);
  if(c == 'f') return a * powf(2.0, -4.0 / 12.0);
  if(c == 'g') return a * powf(2.0, -2.0 / 12.0);
  if(c == 'a') return a * powf(2.0, 0.0 / 12.0);
  if(c == 'h') return a * powf(2.0, 2.0 / 12.0);
  if(c == 'x') return a * powf(2.0, 3.0 / 12.0);
  return -1.0f;
}

static char prev = '\0';
float last_rel_t = 0.0;
float phase_o1 = 0.0;
float phase_o2 = 0.0;
float phase_o3 = 0.0;
float get_sample_at(float t) {
  float bpm = 0.85; 
  float relative_t = fmod(t, 1.0 / (float)bpm);

  if(relative_t < last_rel_t) { // TRIGGER
    phase_o1 = 0.0;
    phase_o2 = 0.0;
    phase_o3 = 0.0;
  }
  // TODO this stuff is not sample accurate, some rounding in the fmod relative_time.
  float dt = last_rel_t - relative_t;
  last_rel_t = relative_t;

  float env = adsr(&envelope, relative_t);
  float env2 = adsr(&envelope2, relative_t);
  size_t idx = ((int) (t * bpm)) % length;
  char c = line[idx];

  if(c != prev) {
    //printf("pitch %c %g.\n", c, charpitch(c));
    prev = c;
  }

  float pitch = charpitch(c);
  if(pitch > 0) {

    float pitch_o1 = powf(pitch, 1);
    float mod_o1 = 8.0 * env2 * pitch_o1 * sinf( 2 * PI * 2 * pitch_o1 * relative_t);
    phase_o1 += 2 * PI * dt * (pitch_o1 + mod_o1);
    phase_o1 = fmod(phase_o1, 2 * PI);
    float signal_o1 = sinf(phase_o1);

    float pitch_o2 = pitch * powf(2.0, 3.0 / 12.0);
    float mod_o2 = 8.0 * env2 * pitch_o2 * sinf( 2 * PI * 2 * pitch_o2 * relative_t);
    phase_o2 += 2 * PI * dt * (pitch_o2 + mod_o2);
    phase_o2 = fmod(phase_o2, 2 * PI);
    float signal_o2 = sinf(phase_o2);

    float pitch_o3 = pitch * powf(2.0, 7.0 / 12.0);
    float mod_o3 = 8.0 * env2 * pitch_o3 * sinf( 2 * PI * 2 * pitch_o3 * relative_t);
    phase_o3 += 2 * PI * dt * (pitch_o3 + mod_o3);
    phase_o3 = fmod(phase_o3, 2 * PI);
    float signal_o3 = sinf(phase_o3);

    return 0.1 * env * (signal_o1 + signal_o2 + signal_o3);
  } else {
    return 0.0;
  }
}

static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max) {
  const struct SoundIoChannelLayout *layout = &outstream->layout;
  float float_sample_rate = outstream->sample_rate;
  float seconds_per_frame = 1.0f / float_sample_rate;
  struct SoundIoChannelArea *areas;
  int frames_left = frame_count_max;
  int err;

  while (frames_left > 0) {
    int frame_count = frames_left;

    if ((err =
             soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }
    //printf("frame_count before %d, after %d.\n", frames_left, frame_count);

    if (!frame_count)
      break;

    //printf("frame count %d.\n", frame_count);
    for (int frame = 0; frame < frame_count; frame += 1) {
      float t = seconds_offset + frame * seconds_per_frame;
      //printf("t = %g.\n", t);
      float sample = get_sample_at(t);
      for (int channel = 0; channel < layout->channel_count; channel += 1) {
        float *ptr =
            (float *)(areas[channel].ptr + areas[channel].step * frame);
        *ptr = sample;
      }
    }
    seconds_offset += frame_count * seconds_per_frame;

    if ((err = soundio_outstream_end_write(outstream))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    frames_left -= frame_count;
  }
  //printf("Write callback finished.\n");
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

  struct SoundIoDevice *device =
      soundio_get_output_device(soundio, default_out_device_index);
  if (!device) {
    fprintf(stderr, "out of memory");
    return 1;
  }

  //fprintf(stderr, "Output device: %s\n", device->name);

  struct SoundIoOutStream *outstream = soundio_outstream_create(device);
  outstream->format = SoundIoFormatFloat32NE;
  outstream->write_callback = write_callback;

  if ((err = soundio_outstream_open(outstream))) {
    fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
    return 1;
  }

  if (outstream->layout_error)
    fprintf(stderr, "unable to set channel layout: %s\n",
            soundio_strerror(outstream->layout_error));

  //size_t zero = 0;
  //printf("Melodi: ");
  //length = getline(&line, &zero, stdin);
  //line[length - 1] = '\0';
  //length -= 1;
  //if (length == -1) {
  //  fprintf(stderr, "unable to read input.\n");
  //  return 1;
  //}
  line = "cccceeddccccggddccccGGAH";
  length = 24;


  if ((err = soundio_outstream_start(outstream))) {
    fprintf(stderr, "unable to start device: %s", soundio_strerror(err));
    return 1;
  }

  //printf("read %s.\n", line);

  for (;;)
    soundio_wait_events(soundio);

  soundio_outstream_destroy(outstream);
  soundio_device_unref(device);
  soundio_destroy(soundio);
  return 0;
}
