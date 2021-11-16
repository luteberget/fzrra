#include "rtmidi_c.h"
#include <soundio/soundio.h>

#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
//#define M_PI 3.14159265358979323846264338327950288
static float seconds_offset = 0.0f;

static float phase = 0.0f;
static float modphase = 0.0;
static float modvalue = 0.0;
static float basefreq = 330.0;
static float curr_basefreq = 330.0;

static float relative_mod = 1.0;

float saw(float t) { return -1.0 + 2.0 * fmodf(t / M_PI, 2 * M_PI); }

static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max) {
	//printf("Please write %d -- %d frame_count\n", frame_count_min, frame_count_max);
  const struct SoundIoChannelLayout *layout = &outstream->layout;
  float float_sample_rate = outstream->sample_rate;
  float seconds_per_frame = 1.0f / float_sample_rate;
  struct SoundIoChannelArea *areas;
  int frames_left = 1000000;
  if(frame_count_max < frames_left) {
	  frames_left = frame_count_max;
  }
  if(frame_count_min > frames_left) {
	  frames_left = frame_count_min;
  }


  int err;

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

      // long double t = seconds_offset + frame * seconds_per_frame;

      curr_basefreq += (basefreq - curr_basefreq)*0.001;

      modphase += 2 * M_PI * seconds_per_frame * relative_mod*curr_basefreq;
      modphase = fmodf(modphase, 2 * M_PI);
      modvalue = sinf(modphase);

      phase += 2 * M_PI * curr_basefreq * seconds_per_frame;
      phase = fmodf(phase, 2 * M_PI);

      float sample = 0.4 * sinf(phase + modvalue);

      for (int channel = 0; channel < layout->channel_count; channel += 1) {
        float *ptr =
            (float *)(areas[channel].ptr + areas[channel].step * frame);
        *ptr = sample;
      }
    }

    seconds_offset = seconds_offset + seconds_per_frame * frame_count;

    if ((err = soundio_outstream_end_write(outstream))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    frames_left -= frame_count;
  }
}

void midi_msg_in_cb(double timestamp, const unsigned char* message, size_t message_size, void* user_data) {


	if (message_size  == 3) {
		if (message[0] == 144) {
			// NOTE ON
			char note = message[1];
			char velocity = message[2];
			printf(" NOTEON note %d vel %d \n", note, velocity);

			float frequency = powf(2.0, (note - 69.0)/12.0)*440.0;
			basefreq = frequency;


		}
		else if (message[0] == 128) {
			// NOTE OFF
			char note = message[1];
			char velocity = message[2];
			printf(" NOTEOFF note %d vel %d \n", note, velocity);
		}
		else if (message[0] == 176) {
			// KNOB
			char note = message[1];
			char velocity = message[2];
			printf(" KNOB note %d vel %d \n", note, velocity);

			relative_mod = ((float)velocity / 127.0) * 3.0 + 0.5;


		} else {
			printf("Unknown midi msg");
			for(int i = 0; i < message_size; i++) {
				printf(" %d ", message[i]);
			}
			printf("\n");
		}
	} else {
			printf("Unknown midi msg.\n");
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

  fprintf(stderr, "Output device: %s\n", device->name);

  struct SoundIoOutStream *outstream = soundio_outstream_create(device);
  outstream->format = SoundIoFormatFloat32NE;
  outstream->write_callback = write_callback;
  printf("Default software latency %g\n", outstream->software_latency);
  outstream->software_latency = 0.001;

  if ((err = soundio_outstream_open(outstream))) {
    fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
    return 1;
  }
  printf("Default software latency %g\n", outstream->software_latency);

  if (outstream->layout_error)
    fprintf(stderr, "unable to set channel layout: %s\n",
            soundio_strerror(outstream->layout_error));

  if ((err = soundio_outstream_start(outstream))) {
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



  char buffer[256];
  for (;;) {
    fd_set selectset;
    struct timeval timeout = {1, 0}; // timeout of 1 secs.
    int ret;
    FD_ZERO(&selectset);
    FD_SET(0, &selectset);
    ret = select(1, &selectset, NULL, NULL, &timeout);
    if (ret == 0) {
      // timeout
    } else if (ret == -1) {
      printf("Read error\n");
      exit(1);
      //  //error
    } else {

      int n = read(0, buffer, sizeof(buffer));
      int i = 0;
      basefreq = strtof(buffer, NULL);
      printf("Setting basefreq\n");

      //printf("Clear: %d\n", soundio_outstream_clear_buffer(outstream));
    }
    soundio_flush_events(soundio);
  }

  soundio_outstream_destroy(outstream);
  soundio_device_unref(device);
  soundio_destroy(soundio);
    rtmidi_in_free(midiin);
}



