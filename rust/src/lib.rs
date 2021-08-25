use std::{f32::consts::TAU, time::Duration};

const SAMPLE_RATE: u32 = miniaudio::SAMPLE_RATE_48000;
const PER_SAMPLE_RATE: f32 = 1.0 / miniaudio::SAMPLE_RATE_48000 as f32;

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

// pub struct MySine {
//     wave: miniaudio::Waveform,
// }

// impl MySine {
//     pub fn new(a: f64, f: f64) -> Self {
//         let wave = miniaudio::Waveform::new(&miniaudio::WaveformConfig::new(
//             miniaudio::Format::F32,
//             2,
//             SAMPLE_RATE,
//             miniaudio::WaveformType::Sawtooth,
//             a,
//             f,
//         ));
//         MySine { wave }
//     }

//     pub fn emit(&mut self, output: &mut miniaudio::FramesMut) {
//         self.wave.read_pcm_frames(output);
//     }
// }

pub struct Const(pub f32);
impl Source for Const {
    fn output(&mut self) -> f32 {
        self.0
    }
}

pub fn zero() -> Const {
    Const(0.)
}

// pub struct Z;
// impl Source for Z {
//     fn output(&mut self) -> f32 {
//         0.
//     }
// }

pub struct Add<A,B> {
    a  :A, b :B
}

pub struct Id<A>(pub A);

impl<A :Source> Source for Id<A> {
    fn output(&mut self) -> f32 {
        self.0.output()
    }
}

impl<A , B> std::ops::Add<A> for Id<B> where A: Source, B: Source {
    type Output = Add<A,B>;

    fn add(self, rhs: A) -> Self::Output {
        Add { a: rhs, b: self.0}
    }
}

impl<A :Source, B :Source> Source for Add<A,B> {
    fn output(&mut self) -> f32 {
        self.a.output() + self.b.output()
    }
}

impl<A :Source> std::ops::Add<A> for Const {
    type Output = Add<Const, A>;

    fn add(self, rhs: A) -> Self::Output {
        Add { a :self, b: rhs}
    }
}


impl std::ops::Add<f32> for Const {
    type Output = Add<Const, Const>;

    fn add(self, rhs: f32) -> Self::Output {
        Add { a :Const(rhs), b: self}
    }
}

impl std::ops::Add<Const> for f32 {
    type Output = Add<Const, Const>;

    fn add(self, rhs: Const) -> Self::Output {
        Add { a :Const(self), b: rhs}
    }
}

pub trait Source {
    fn output(&mut self) -> f32;
    fn emit(&mut self, output: &mut miniaudio::FramesMut) {
        for s in output.frames_mut() {
            let sample = self.output();
            s[0] = sample;
            s[1] = sample;
        }
    }
}

pub struct Sin<F, A> {
    freq: F,
    amp: A,
    phase: f32,
}

impl<F: Source, A: Source> Sin<F, A> {
    pub fn new(freq: F, amp: A) -> Self {
        Self {
            freq,
            amp,
            phase: 0.0,
        }
    }
}

impl<F: Source, A: Source> Source for Sin<F, A> {
    fn output(&mut self) -> f32 {
        let incr = TAU * self.freq.output() * PER_SAMPLE_RATE;
        self.phase += incr;
        self.phase %= TAU;
        self.amp.output() * (self.phase).sin()
    }
}
