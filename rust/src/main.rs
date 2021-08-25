use rustaudio::*;

fn main() {
    let mut wave = Sin::new(
        Const(440.0)
            + (Id(Sin::new(zero() + 5.0, zero() + 100.0)) + Sin::new(zero() + 5.0, zero() + 100.0)),
        zero() + 0.1,
    );
    play(move |output| {
        wave.emit(output);
    });
}
