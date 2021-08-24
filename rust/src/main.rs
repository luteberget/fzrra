use rustaudio::*;

fn main() {
    let mut sine = MySine::new(0.1, 440.0);
    play(move |output| {
        sine.emit(output);
    });
}
