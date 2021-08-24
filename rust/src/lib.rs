use std::{f32::consts::TAU, time::Duration};

const SAMPLE_RATE: u32 = miniaudio::SAMPLE_RATE_48000;

pub fn play(mut f: impl FnMut(&mut miniaudio::FramesMut) + Send + 'static) {
    let mut device_config = miniaudio::DeviceConfig::new(miniaudio::DeviceType::Playback);
    device_config
        .playback_mut()
        .set_format(miniaudio::Format::F32);
    device_config.playback_mut().set_channels(2);
    device_config.set_sample_rate(SAMPLE_RATE);

    device_config.set_stop_callback(|_device| {
        println!("Device Stopped.");
    });

    let mut device =
        miniaudio::Device::new(None, &device_config).expect("failed to open playback device");
    device.set_data_callback(move |_device, output, _input| {
        f(output);
    });

    device.start().expect("failed to start device");

    println!("Device Backend: {:?}", device.context().backend());
    std::thread::sleep(Duration::from_secs(10));
    println!("Shutting Down...");
}

pub struct MySine {
    wave: miniaudio::Waveform,
}

impl MySine {
    pub fn new(a: f64, f: f64) -> Self {
        let wave = miniaudio::Waveform::new(&miniaudio::WaveformConfig::new(
            miniaudio::Format::F32,
            2,
            SAMPLE_RATE,
            miniaudio::WaveformType::Sawtooth,
            a,
            f,
        ));
        MySine { wave }
    }

    pub fn emit(&mut self, output: &mut miniaudio::FramesMut) {
        self.wave.read_pcm_frames(output);
    }
}

pub struct Const(pub f32);
impl Source for Const {
    fn output(&mut self) -> f32 {
        self.0
    }
}

trait Source {
    fn output(&mut self) -> f32;
}

struct Sin<F, A> {
    freq: F,
    amp: A,
}

impl<F: Source, A: Source> Sin<F, A> {
    pub fn new(freq: F, amp: A) -> Self {
        Self { freq, amp }
    }
}

impl<F: Source, A: Source> Source for Sin<F, A> {
    fn output(&mut self) -> f32 {
        self.amp.output() * (TAU * self.freq.output()).sin()
    }
}
