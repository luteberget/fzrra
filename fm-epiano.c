#include <soundio/soundio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float PI = 3.1415926535f;

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

//float op1_phase = 0.0;
//float operator1(float dt) {
//  // TODO note pitch input
//  float note_pitch = 440.0f;
//   
//  // basic frequency + fine + detune, 
//  float coarse = 1.0; // 0.5, 1, 2, ..., 31.
//  float fine = 0.0;   // 0.0, 0.99,
//  float detune_level = 3.0;
//  float detune_amount = 7.2 * detune_level; // ??? 
//  float freq = note_pitch * (coarse + fine + detune_amount);
//
//
//  // keyboard level scaling 
//  // is DISABLED on most of the e.piano1 operators  
//
//  // keyboard rate scaling (?) 
//  //  it increases the rate of the envelope based on keyboard midinote.
//  //  (increases "qr" from the envelope function's rate constant)
//  //  see dexed/ dx7voice.cc and env.cc
//  // MIGHT be necessary for e.piano1
//  float rate_scaling = 0.0;
//
//  // amplitude modulation sensitivity is DISABLED on e.piano1
//  //  this is a factor on the the LFO amp.mod. depth
//
//  // key velocity scaling  / key velocity sensitivity
//  //   it increases the constant output amplitude of the envelope based on keyboard velocity.
//
//
//
//  // envelope
//  float env_level = get_level(params, dt, rate_scale);
//
//  // output level
//  float output_level = 1.0;
//  
//  op1_phase += 2 * PI * freq; // no mod on op1
//  return output_level * env_level * sinf(op1_phase);
//}

float clamp(float x) { return fmin(1.0, fmax(0.0, x)); }
float dbamp(float db) { return powf(10., db * .05f); }
float level_to_amp(float level) { return level > 0 ? dbamp(-90.65*(1 - level)) : 0; }

typedef struct { float levels[4]; float rates[4]; int stage; float level; bool is_released; } envelope;
typedef struct { float freq; float amp; envelope amp_env; float phase; } operator;
typedef struct { operator ops[6]; float timestep; float feedback_amp; float feedback_buf; } voice;

//float velocity_sensitivity(float sensitivity, float velocity) {
//  // velocity in [0,1], sensitivity in [0,1].
//
//  // rescale and use approximation of velocity_data table from dexed
//  float clamped_vel = velocity * 127.0;
//  float vel_value = 60.0 * log((clamped_vel/2.0)+1) - 239.0;
//  float scaled_vel = (sensitivity * 7 + 1) * vel_value * 2;
//  // TODO simplify
//  return scaled_vel / 128.0 / 32.0;
//}

float velocity_sensitivity(float sens, float vel) {
  return (sens + 1.0/7.0) * (log((vel * 64.0) + 1.0) - 239.0 / 64.0) / 4.0;
}

bool outed = false;
void envelope_advance(float dt, envelope* env) {
  if(env->stage >= 3 && !env->is_released) { /*printf("sustain\n");*/ return; } // sustaining, no change in level.
  bool is_attack = env->levels[(env->stage + 3)%4] < env->levels[env->stage];
  if (env->stage < 3 && ((env->level <= env->levels[env->stage]) ^ is_attack || env->level == env->levels[env->stage])) { 
    env->stage = (env->stage + 1) % 4;
    if(!outed && env->stage == 2) { printf("stage 2\n"); outed = true; }
    return envelope_advance(dt, env); 
  }
  float const_rate = dt *  0.28 / 90.65 * powf(2.0f, 16.0f * env->rates[env->stage]);
  float rate = const_rate * (is_attack ? (17.0 - 16.0*(3840.0/4096.0)*env->level) : -1.0);
  //printf("attacK %d, const_rate=%g, rate=%g\n", is_attack, const_rate, rate);
  env->level += rate;
  if((env->level <= env->levels[env->stage]) ^is_attack) env->level = env->levels[env->stage];
}

voice new_note_voice(float timestep, int midinote /* [midi] */, float velocity /* [[0,1]] */) {
  float freq = 440.0f / 32.0f * powf(2, (((float)(midinote - 9)) / 12.0f));
  printf("frequency %g\n", freq);
  voice v = { .timestep = timestep, .feedback_amp = 6.0f/7.0f, .feedback_buf = 0.0, .ops = {

    // TODO patch: the whole LFO thing
    // TODO patch: pitch envelope

    // TODO operator: AM sensitivity (factor for adding LFO amplitude depth to operator amp)
    // TODO operator: keyboard level scaling (add amplitude based on midinote)
    // TODO operator: keyboard rate scaling (modify frequency based on midinote)

    // OP 0 "pling"
    //{ .freq = 1.00f * freq + 3.0f*7.2f /* (1 + 3*detuned) */ /* [Hz] */ , 
    { .freq = 1.00f *freq + 3.0f / 32.0 /* (1 + 3*detuned) */ /* [Hz] */ , 
      .amp =  level_to_amp( clamp(1.0 + /* keyboard sensitivity: */ velocity_sensitivity(2.0/7.0, velocity))),
      .amp_env = { .levels = { 1.0, 0.75, 0.0, 0.0 }, .rates = { 0.96, 0.25, 0.25, 0.67 } } },
    // OP 1 [modulates->0]
    { .freq = 14.0f * freq, 
       .amp =  level_to_amp( clamp (0.78 + velocity_sensitivity(1.0, velocity))),
      .amp_env = { .levels = { 1.0, 0.75, 0.0, 0.0 }, .rates = { 0.95, 0.50, 0.35, 0.78 } } },

    // OP 2
    { .freq = 1.0 * freq,
      .amp =  level_to_amp( clamp(1.0 + /* keyboard sensitivity: */ velocity_sensitivity(2.0 / 7.0, velocity))),
      .amp_env = { .levels = { 1.0, 0.95, 0.0, 0.0 }, .rates = { 0.95, 0.2, 0.2, 0.5 } } },
    // OP 3 [modulates->2]
    { .freq = 1.00f * freq /* (1 + 3*detuned) */ /* [Hz] */ , 
      .amp =  level_to_amp( clamp(0.79 + /* keyboard sensitivity: */ velocity_sensitivity(6.0/7.0, velocity))),
      .amp_env = { .levels = { 1.0, 0.95, 0.0, 0.0 }, .rates = { 0.95, 0.29, 0.20, 0.50 } } },

    // OP 4
    { .freq = 1.00f * freq + -0.0/5.0,
      .amp =  level_to_amp( clamp(1.0 + /* keyboard sensitivity: */ velocity_sensitivity(0.0, velocity))),
      .amp_env = { .levels = { 1.0, 0.95, 0.0, 0.0 }, .rates = { 0.95, 0.20, 0.20, 0.50 } } },
    // OP 5
    { .freq = 1.00f * freq + 0.0/5.0 /* (1 + 3*detuned) */ /* [Hz] */ , 
      .amp =  level_to_amp( clamp(0.69 + /* keyboard sensitivity: */ velocity_sensitivity(6.0/7.0, velocity))),
      .amp_env = { .levels = { 0.99, 0.95, 0.0, 0.0 }, .rates = { 0.95, 0.29, 0.20, 0.50 } } },
    }};
  return v;
}

int ni = 0;
float voice_sample(voice* v) {
//printf("getting sample %d\n", ni++);
  // sample each operator
  float out[6];

  double feedback_scaling = 6.4;

  for(int i = 0; i < 5; i++) out[i] = v->ops[i].amp * level_to_amp(v->ops[i].amp_env.level) * sinf(v->ops[i].phase);

  out[5] = v->ops[5].amp * level_to_amp(v->ops[5].amp_env.level) * sinf(v->ops[5].phase + feedback_scaling * v->feedback_buf);
  v->feedback_buf = out[5];
printf("amp=%g\tleevel=%g\tphase=%g,freq=%g,sample=%g\n", v->ops[5].amp, v->ops[5].amp_env.level, v->ops[5].phase, v->ops[5].freq,out[5]);


//printf(" sinf ops[0]: %g,  v.ops[0].amp_env.level: %g \n", sinf(v->ops[0].phase), v->ops[0].amp_env.level);
//printf(" sinf ops[1]: %g,  v.ops[1].amp_env.level: %g \n", sinf(v->ops[1].phase), v->ops[1].amp_env.level);

//for(int sens_i = 0; sens_i <= 10; sens_i++) {
//for(int vel_i = 0; vel_i <= 10; vel_i++) {
//  float vel = (float)vel_i / 10.0;
//  float sens = (float)sens_i / 10.0;
//  float delta = velocity_sensitivity(sens,vel) - velocity_sensitivity_simplified(sens,vel);
//  printf("velocity_sensitivity(%g,%g)=%g  delta=%g\n", sens, vel, velocity_sensitivity(sens, vel));
//}
//}

  //double feedback_scaling = 6.0/7.0 / (2 * PI) *0.18 / 99.0 * 50;

  // run the algorithm (advancing oscillators in preparation for next sample)
  v->ops[0].phase += 2 * PI * v->timestep * v->ops[0].freq + out[1];// + 0.01*out[1];
  v->ops[1].phase += 2 * PI * v->timestep * v->ops[1].freq;
  v->ops[2].phase += 2 * PI * v->timestep * v->ops[2].freq + out[3] /* modulation */;
  v->ops[3].phase += 2 * PI * v->timestep * v->ops[3].freq;
  v->ops[4].phase += 2 * PI * v->timestep * v->ops[4].freq + out[5]; //+ out[5] /* modulation */);
  v->ops[5].phase += 2 * PI * v->timestep * v->ops[5].freq;// + feedback_scaling*out[5] /* feedback */;

  // clamp phase angles in [0,2pi]
  for(int i = 0; i < 6; i++) while(v->ops[i].phase > 2*PI) v->ops[i].phase -= 2*PI;

  // advance the amplitude envelope generators
  for(int i = 0; i < 6; i++) envelope_advance(v->timestep, &v->ops[i].amp_env);

//printf("0 amp: %g\n", v->ops[0].amp);
  // return the mixed sample
  return (out[4] + out[4])/2.0; ;// / (v->ops[0].amp + v->ops[2].amp) ;
  //return ( out[0] + out[2] + out[4] ) / 3.0f /* normalize amplitude for 3 carriers */;
}

void release_note(voice* v) {
  for(int i = 0; i < 6; i++) {
    v->ops[i].amp_env.is_released = true;
    v->ops[i].amp_env.stage = 3;
  }
}

voice the_voice;
bool inited = false;

static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max) {
  const struct SoundIoChannelLayout *layout = &outstream->layout;
  float float_sample_rate = outstream->sample_rate;
  float seconds_per_frame = 1.0f / float_sample_rate;
  struct SoundIoChannelArea *areas;
  int frames_left = frame_count_max;
  int err;

  if(!inited) {
    the_voice = new_note_voice(seconds_per_frame, 64, 0.99);
    inited = true;
  }

  while (frames_left > 0) {
    int frame_count = frames_left;

    if ((err =
             soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    if (!frame_count)
      break;

    for (int frame = 0; frame < frame_count; frame += 1) {
      float sample = voice_sample(&the_voice);
      //printf("final sample: %g\n", sample);
      for (int channel = 0; channel < layout->channel_count; channel += 1) {
        float *ptr =
            (float *)(areas[channel].ptr + areas[channel].step * frame);
        *ptr = sample;
      }
    }

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
  struct SoundIoDevice *device =
      soundio_get_output_device(soundio, default_out_device_index);
  if (!device) {
    fprintf(stderr, "out of memory");
    return 1;
  }
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
