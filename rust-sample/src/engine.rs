use std::time::Instant;

use anyhow::{bail, Context, Result};

use crate::catalog::{Catalog, Op, Show, Wait};
use crate::patterns::{marquee_left, marquee_right, rotate13, Packet, DARK};

/// Full brightness is a no-op multiply on emit.
pub const BRIGHTNESS_MAX: u8 = 255;
/// Operator `<` / `>` step.
pub const BRIGHTNESS_STEP: u8 = 16;

/// Packet the sequencer should send, plus when the next opcode may run.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Step {
    pub packet: Packet,
    pub deadline: Instant,
}

/// Interprets a show program without talking to USB or the TUI.
#[derive(Debug, Clone)]
pub struct Player {
    program: Vec<Op>,
    pc: usize,
    looping: bool,
    working: Packet,
    brightness: u8,
    deadline: Option<Instant>,
    running: bool,
    show_name: String,
    loop_pass: usize,
}

impl Default for Player {
    fn default() -> Self {
        Self {
            program: Vec::new(),
            pc: 0,
            looping: false,
            working: DARK,
            brightness: BRIGHTNESS_MAX,
            deadline: None,
            running: false,
            show_name: "IDLE".to_owned(),
            loop_pass: 0,
        }
    }
}

impl Player {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    #[must_use]
    pub fn is_running(&self) -> bool {
        self.running
    }

    #[must_use]
    pub fn is_looping(&self) -> bool {
        self.running && self.looping
    }

    #[must_use]
    pub fn show_name(&self) -> &str {
        &self.show_name
    }

    #[must_use]
    pub fn deadline(&self) -> Option<Instant> {
        self.deadline
    }

    #[must_use]
    pub fn brightness(&self) -> u8 {
        self.brightness
    }

    /// Load `show` and run until the first emit (or finish).
    pub fn start(
        &mut self,
        show: &Show,
        catalog: &Catalog,
        now: Instant,
    ) -> Result<Option<Step>> {
        self.program.clone_from(&show.program);
        self.pc = 0;
        self.looping = show.looping;
        self.working = DARK;
        self.running = true;
        self.show_name.clone_from(&show.name);
        self.deadline = None;
        self.loop_pass = 0;
        self.execute_until_wait(catalog, now)
    }

    /// Stop immediately; the sequencer may send an explicit dark packet.
    pub fn stop(&mut self) {
        self.program.clear();
        self.pc = 0;
        self.looping = false;
        self.running = false;
        self.deadline = None;
        self.show_name = "IDLE".to_owned();
    }

    /// Advance when `now` has reached the current deadline.
    pub fn advance(&mut self, catalog: &Catalog, now: Instant) -> Result<Option<Step>> {
        if !self.running {
            return Ok(None);
        }
        if let Some(deadline) = self.deadline {
            if now < deadline {
                return Ok(None);
            }
        }
        self.execute_until_wait(catalog, now)
    }

    pub fn dimmer(&mut self) {
        self.brightness = self.brightness.saturating_sub(BRIGHTNESS_STEP);
    }

    pub fn brighter(&mut self) {
        self.brightness = self.brightness.saturating_add(BRIGHTNESS_STEP);
    }

    fn execute_until_wait(
        &mut self,
        catalog: &Catalog,
        now: Instant,
    ) -> Result<Option<Step>> {
        if self.program.is_empty() {
            self.finish_idle();
            return Ok(None);
        }

        let limit = self.program.len().saturating_mul(2).max(1);
        for _ in 0..limit {
            if self.pc >= self.program.len() {
                if self.looping {
                    self.pc = 0;
                    self.loop_pass = self.loop_pass.saturating_add(1);
                    continue;
                }
                self.finish_idle();
                return Ok(None);
            }

            let op = self.program[self.pc].clone();
            match op {
                Op::Hold { frame, wait } => {
                    let packet = self.emit_frame(catalog, &frame)?;
                    self.pc += 1;
                    return Ok(Some(self.hold(packet, &wait, now)?));
                }
                Op::LoadWorking { frame } => {
                    self.pc += 1;
                    if self.loop_pass == 0 {
                        self.working = *catalog
                            .frame(&frame)
                            .with_context(|| format!("unknown frame `{frame}`"))?;
                    }
                }
                Op::HoldWorking { wait } => {
                    let packet = scale_rgb(&self.working, self.brightness);
                    self.pc += 1;
                    return Ok(Some(self.hold(packet, &wait, now)?));
                }
                Op::Rotate13 => {
                    rotate13(&mut self.working);
                    self.pc += 1;
                }
                Op::MarqueeLeft => {
                    marquee_left(&mut self.working);
                    self.pc += 1;
                }
                Op::MarqueeRight => {
                    marquee_right(&mut self.working);
                    self.pc += 1;
                }
            }
        }

        bail!(
            "show '{}' never emits a packet (program is only transforms?)",
            self.show_name
        );
    }

    fn finish_idle(&mut self) {
        self.running = false;
        self.deadline = None;
        self.show_name = "IDLE".to_owned();
    }

    fn emit_frame(&self, catalog: &Catalog, frame: &str) -> Result<Packet> {
        let packet = catalog
            .frame(frame)
            .with_context(|| format!("unknown frame `{frame}`"))?;
        Ok(scale_rgb(packet, self.brightness))
    }

    fn hold(&mut self, packet: Packet, wait: &Wait, now: Instant) -> Result<Step> {
        let deadline = now + wait.duration()?;
        self.deadline = Some(deadline);
        Ok(Step { packet, deadline })
    }
}

/// Multiply RGB by `brightness / 255`. `0` is dark; `255` is identity.
#[must_use]
pub fn scale_rgb(packet: &Packet, brightness: u8) -> Packet {
    if brightness == 0 {
        return DARK;
    }
    if brightness == BRIGHTNESS_MAX {
        return *packet;
    }
    let mut out = *packet;
    for byte in &mut out {
        *byte = u8::try_from(u16::from(*byte) * u16::from(brightness) / 255)
            .unwrap_or(u8::MAX);
    }
    out
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::time::Duration;

    use super::*;
    use crate::catalog::{Show, Wait};
    use crate::patterns::{marquee_left, rotate13, CHANNELS, WHITE};

    fn catalog_with(frames: &[(&str, Packet)]) -> Catalog {
        let map = frames
            .iter()
            .map(|(name, pkt)| ((*name).to_owned(), *pkt))
            .collect::<HashMap<_, _>>();
        Catalog::new(map, Vec::new()).expect("catalog")
    }

    fn show(looping: bool, program: Vec<Op>) -> Show {
        Show {
            key: 't',
            name: "Test".to_owned(),
            category: "Tests".to_owned(),
            desc: String::new(),
            looping,
            program,
        }
    }

    fn hold(frame: &str, wait: Wait) -> Op {
        Op::Hold {
            frame: frame.to_owned(),
            wait,
        }
    }

    #[test]
    fn one_shot_hold_sequence_emits_and_finishes() {
        let catalog = catalog_with(&[("white", WHITE), ("dark", DARK)]);
        let cue = show(
            false,
            vec![
                hold("white", Wait::Named("slp".to_owned())),
                hold("dark", Wait::Ms(40)),
            ],
        );
        let mut player = Player::new();
        let t0 = Instant::now();

        let first = player.start(&cue, &catalog, t0).unwrap().expect("white");
        assert_eq!(first.packet, WHITE);
        assert_eq!(first.deadline, t0 + Duration::from_millis(40));
        assert!(player.is_running());

        assert!(player.advance(&catalog, t0 + Duration::from_millis(39)).unwrap().is_none());

        let second = player
            .advance(&catalog, first.deadline)
            .unwrap()
            .expect("dark");
        assert_eq!(second.packet, DARK);

        assert!(player.advance(&catalog, second.deadline).unwrap().is_none());
        assert!(!player.is_running());
        assert_eq!(player.show_name(), "IDLE");
    }

    #[test]
    fn looping_repeats_until_stop() {
        let catalog = catalog_with(&[("white", WHITE)]);
        let cue = show(true, vec![hold("white", Wait::Ms(10))]);
        let mut player = Player::new();
        let t0 = Instant::now();

        let first = player.start(&cue, &catalog, t0).unwrap().expect("first");
        assert!(player.is_looping());
        let second = player
            .advance(&catalog, first.deadline)
            .unwrap()
            .expect("repeat");
        assert_eq!(second.packet, WHITE);

        player.stop();
        assert!(!player.is_running());
        assert!(player.advance(&catalog, second.deadline).unwrap().is_none());
    }

    #[test]
    fn rotate_and_marquee_use_working_buffer() {
        let mut seed = DARK;
        seed[0..3].copy_from_slice(&[1, 2, 3]);
        seed[3..6].copy_from_slice(&[4, 5, 6]);
        let catalog = catalog_with(&[("seed", seed)]);

        let rotate_show = show(
            false,
            vec![
                Op::LoadWorking {
                    frame: "seed".to_owned(),
                },
                Op::Rotate13,
                Op::HoldWorking { wait: Wait::Ms(5) },
            ],
        );
        let mut player = Player::new();
        let t0 = Instant::now();
        let step = player
            .start(&rotate_show, &catalog, t0)
            .unwrap()
            .expect("rotated");
        let mut expected = seed;
        rotate13(&mut expected);
        assert_eq!(step.packet, expected);

        let marquee_show = show(
            false,
            vec![
                Op::LoadWorking {
                    frame: "seed".to_owned(),
                },
                Op::MarqueeLeft,
                Op::HoldWorking { wait: Wait::Ms(5) },
                Op::MarqueeRight,
                Op::HoldWorking { wait: Wait::Ms(5) },
            ],
        );
        let mut player = Player::new();
        let left = player
            .start(&marquee_show, &catalog, t0)
            .unwrap()
            .expect("left");
        let mut left_expected = seed;
        marquee_left(&mut left_expected);
        assert_eq!(left.packet, left_expected);

        let right = player
            .advance(&catalog, left.deadline)
            .unwrap()
            .expect("right");
        assert_eq!(right.packet, seed);
    }

    #[test]
    fn looping_does_not_reload_working_buffer() {
        let mut seed = DARK;
        for i in 0..CHANNELS {
            seed[i * 3] = u8::try_from(i).unwrap_or(u8::MAX);
        }
        let catalog = catalog_with(&[("seed", seed)]);
        let cue = show(
            true,
            vec![
                Op::LoadWorking {
                    frame: "seed".to_owned(),
                },
                Op::MarqueeLeft,
                Op::HoldWorking { wait: Wait::Ms(1) },
            ],
        );
        let mut player = Player::new();
        let t0 = Instant::now();
        let first = player.start(&cue, &catalog, t0).unwrap().expect("first");
        let mut once = seed;
        marquee_left(&mut once);
        assert_eq!(first.packet, once);

        let second = player
            .advance(&catalog, first.deadline)
            .unwrap()
            .expect("second");
        let mut twice = once;
        marquee_left(&mut twice);
        assert_eq!(second.packet, twice);
    }

    #[test]
    fn brightness_scales_on_emit() {
        let catalog = catalog_with(&[("white", WHITE)]);
        let cue = show(false, vec![hold("white", Wait::Ms(1))]);
        let mut player = Player::new();
        let t0 = Instant::now();

        player.brightness = 0;
        let dark = player.start(&cue, &catalog, t0).unwrap().expect("dark");
        assert_eq!(dark.packet, DARK);

        player.brightness = BRIGHTNESS_MAX;
        let ident = player.start(&cue, &catalog, t0).unwrap().expect("ident");
        assert_eq!(ident.packet, WHITE);

        let half = scale_rgb(&WHITE, 128);
        assert_ne!(half, WHITE);
        assert_ne!(half, DARK);
        assert_eq!(half[0], (255u16 * 128 / 255) as u8);
        assert_eq!(half[CHANNELS * 3 - 1], (215u16 * 128 / 255) as u8);
    }
}
