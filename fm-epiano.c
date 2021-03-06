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

static struct ADSREnv envelope = {.attack = 0.001,
                                  .decay = 0.05,
                                  .sustain = 0.2,
                                  .release = 0.05,
                                  .sustain_level = 0.5};

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

//      | Filename: benson-sysex/rom1a.syx
//      | Voice #: 11
//      | Name: E.PIANO 1 
//      | Algorithm: 5     // 2 -> 1, 4 -> 3, 6* -> 5   (6 has feedback to itself)
//      | Feedback: 6      // level of feedback (though it is also operator number 6)
// OFF/*| LFO  
//      |   Wave: Sine
//      |   Speed: 34
//      |   Delay: 33
//      |   Pitch Mod Depth: 0
//      |   AM Depth: 0
//      |   Sync: Off
//      |   Pitch Modulation Sensitivity: 3
//    */| Oscillator Key Sync: Off
// OFF  | Pitch Envelope Generator
//      |   Rate 1: 94
//      |   Rate 2: 67
//      |   Rate 3: 95
//      |   Rate 4: 60
//      |   Level 1: 50
//      |   Level 2: 50
//      |   Level 3: 50
//      |   Level 4: 50
// OFF  | Transpose: C3

float algorithm(float t) {
  float o2 = operator2(t);
  float o1 = operator1(t, o2); // o2 modulates o1
  
  // ...
  return o1 + o3 + o5;
}

// Operator: 1
//   AM Sensitivity: 0
//   Oscillator Mode: Frequency (Ratio)
//   Frequency: 1
//   Detune: 3
//   Envelope Generator
//     Rate 1: 96
//     Rate 2: 25
//     Rate 3: 25
//     Rate 4: 67
//     Level 1: 99
//     Level 2: 75
//     Level 3: 0
//     Level 4: 0
//   Keyboard Level Scaling
//     Breakpoint: A-1
//     Left Curve: -LIN
//     Right Curve: -LIN
//     Left Depth: 0
//     Right Depth: 0
//   Keyboard Rate Scaling: 3
//   Output Level: 99
//   Key Velocity Sensitivity: 2
// 

float op1_phase = 0.0;
float operator1(float dt) {
  // TODO note pitch input
  float note_pitch = 440.0f;
   
  // basic frequency + fine + detune, 
  float coarse = 1.0; // 0.5, 1, 2, ..., 31.
  float fine = 0.0;   // 0.0, 0.99,
  float detune_level = 3.0;
  float detune_amount = 7.2 * detune_level; // ??? 
  float freq = note_pitch * (coarse + fine + detune_amount);


  // keyboard level scaling 
  // is DISABLED on most of the e.piano1 operators  

  // keyboard rate scaling (?) 
  //  it increases the rate of the envelope based on keyboard midinote.
  //  (increases "qr" from the envelope function's rate constant)
  //  see dexed/ dx7voice.cc and env.cc
  // MIGHT be necessary for e.piano1
  float rate_scaling = 0.0;

  // amplitude modulation sensitivity is DISABLED on e.piano1
  //  this is a factor on the the LFO amp.mod. depth

  // key velocity scaling  / key velocity sensitivity
  //   it increases the constant output amplitude of the envelope based on keyboard velocity.



  // envelope
  float env_level = get_level(params, dt, rate_scale);

  // output level
  float output_level = 1.0;
  
  op1_phase += 2 * PI * freq; // no mod on op1
  return output_level * env_level * sinf(op1_phase);
}


static char prev = '\0';
float last_rel_t = 0.0;
float phase = 0.0;
float get_sample_at(float t) {
  int bpm = 2; 
  float relative_t = fmod(t, 1.0 / (float)bpm);

  if(relative_t < last_rel_t) { // TRIGGER
    phase = 0.0;
  }
  // TODO this stuff is not sample accurate, some rounding in the fmod relative_time.
  float dt = last_rel_t - relative_t;
  last_rel_t = relative_t;

  float env = 0.5 * adsr(&envelope, relative_t);
  size_t idx = ((int) (t * bpm)) % length;
  char c = line[idx];

  if(c != prev) {
    //printf("pitch %c %g.\n", c, charpitch(c));
    prev = c;
  }

  float pitch = charpitch(c)/8.0;
  if(pitch > 0) {

    float mod1 = 2.0 * pitch * sinf( 2 * PI * 1.0 * pitch * relative_t);

    float mod2_env = 8 * pitch * (1 - relative_t / 0.1) ;
    if(mod2_env < 0.0) mod2_env = 0.0;
    float mod2 = mod2_env * sinf( 2 * PI * 5.0 * pitch * relative_t);

    phase += 2 * PI * dt * (pitch + mod1 + mod2);
    phase = fmod(phase, 2 * PI);

    float signal = sinf(phase);
    return env * signal;
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
