use std::time::Duration;

use crate::patterns::*;
use crate::term::{LoopStatus, Terminal, UiState};
use crate::timing::*;
use crate::xbee::Xbee;

pub enum Action {
    Continue,
    Quit,
}

pub struct App {
    pub xbee: Xbee,
    pub snowman1: Packet,
    pub snowman2: Packet,
    pub tree1: Packet,
    pub tree2: Packet,
    pub sorc: Packet,
    pub ui_state: UiState,
}

impl App {
    pub fn new(xbee: Xbee) -> Self {
        Self {
            xbee,
            snowman1: SNOWMAN1,
            snowman2: SNOWMAN2,
            tree1: TREE1,
            tree2: TREE2,
            sorc: [0; PACKET_LEN],
            ui_state: UiState::default(),
        }
    }

    /// Transmits a packet, updates UI state, and redraws the terminal.
    pub async fn send(&mut self, packet: &Packet, term: &mut Terminal) {
        self.ui_state.last_packet = *packet;
        self.ui_state.packet_count = self.ui_state.packet_count.saturating_add(1);

        if let Err(e) = self.xbee.send(packet).await {
            let msg = format!("USB write error: {e}");
            self.ui_state.is_error = true;
            self.ui_state.status_message = Some(msg);
        } else if self.ui_state.is_error {
            self.ui_state.is_error = false;
            self.ui_state.status_message = Some("Broadcasting OK".to_string());
        }

        let _ = term.draw(&self.ui_state);
    }

    /// Helper to transmit a packet and sleep for the given duration.
    pub async fn flash(&mut self, packet: &Packet, duration: Duration, term: &mut Terminal) {
        self.send(packet, term).await;
        tokio::time::sleep(duration).await;
    }

    /// Handles a single key press matching the `switch(letter)` from `glasses.c`.
    pub async fn handle_key(&mut self, key: Option<char>, term: &mut Terminal) -> Action {
        if key.is_some() {
            self.ui_state.last_key = key;
        }

        match key {
            Some('b') => {
                // blue flash
                self.ui_state.current_routine = "Blue Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&BLUE, DAB, term).await;
            }
            Some('c') => {
                // christmas sparkle
                self.ui_state.current_routine = "Christmas Sparkle".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&X1, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&X2, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&X3, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&X4, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('d') => {
                // dark
                self.ui_state.current_routine = "Dark".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&DARK, DAB, term).await;
            }
            Some('e') => {
                // green flash
                self.ui_state.current_routine = "Green Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&GREEN, DAB, term).await;
            }
            Some('f') => {
                // twnk8 flash
                self.ui_state.current_routine = "Twink 8".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&TWNK8, DAB, term).await;
            }
            Some('g') => {
                // gold flash
                self.ui_state.current_routine = "Gold Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&GOLD, DAB, term).await;
            }
            Some('h') => {
                // twnk9 flash
                self.ui_state.current_routine = "Twink 9".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&TWNK9, DAB, term).await;
            }
            Some('j') => {
                // twnk10 flash
                self.ui_state.current_routine = "Twink 10".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&TWNK10, DAB, term).await;
            }
            Some('k') => {
                // cyan flash
                self.ui_state.current_routine = "Cyan Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&CYAN, DAB, term).await;
            }
            Some('l') => {
                // twnk11 flash
                self.ui_state.current_routine = "Twink 11".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&TWNK11, DAB, term).await;
            }
            Some('m') => {
                // magenta flash
                self.ui_state.current_routine = "Magenta Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&MAGENTA, DAB, term).await;
            }
            Some('n') => {
                // gold on
                self.ui_state.current_routine = "Gold Loop".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&GOLD, DORK, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('o') => {
                // white on
                self.ui_state.current_routine = "White Loop".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&WHITE, DOT, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('p') => {
                // rainbow med
                self.ui_state.current_routine = "Rainbow Med".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&RAIN5, DASH, term).await;
                self.flash(&RAIN6, DAB, term).await;
                self.flash(&RAIN6, DASH, term).await;
                self.flash(&RAIN7, DAB, term).await;
                self.flash(&RAIN7, DASH, term).await;
                self.flash(&RAIN8, DAB, term).await;
                self.flash(&RAIN8, DASH, term).await;
                self.flash(&DARK, DAB, term).await;
                self.send(&DARK, term).await;
            }
            Some('q') => {
                // rainbow cycle
                self.ui_state.current_routine = "Sparkle Cycle".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&RAIN1, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&RAIN2, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&RAIN3, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    self.flash(&RAIN4, SLP, term).await;
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('r') => {
                // red flash
                self.ui_state.current_routine = "Red Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&RED, DAB, term).await;
            }
            Some('s') => {
                // rainbow short
                self.ui_state.current_routine = "Rainbow Short".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&RAIN1, DASH, term).await;
                self.flash(&RAIN2, DASH, term).await;
                self.flash(&RAIN3, DASH, term).await;
                self.flash(&RAIN4, DASH, term).await;
            }
            Some('t') | Some('u') => {
                // test sequence
                self.ui_state.current_routine = "Test RGB".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&RED, DASH, term).await;
                    self.flash(&GREEN, DASH, term).await;
                    self.flash(&BLUE, DASH, term).await;
                    self.flash(&BLACK, DASH, term).await;
                    tokio::time::sleep(DASH).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('w') => {
                // white flash
                self.ui_state.current_routine = "White Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&WHITE, DAB, term).await;
            }
            Some('x') => {
                // christmas solid
                self.ui_state.current_routine = "Christmas Solid".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&X1, DROOL, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('y') => {
                // yellow flash
                self.ui_state.current_routine = "Yellow Flash".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&YELLOW, DAB, term).await;
            }
            Some('z') => {
                // slow christmas twinkle
                self.ui_state.current_routine = "Slow Twinkle".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&X1, DASH * 2, term).await;
                    self.flash(&X2, DASH * 2, term).await;
                    self.flash(&X3, DASH * 2, term).await;
                    self.flash(&X4, DASH * 2, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('a') => {
                // asterion / twinkle routine
                self.ui_state.current_routine = "Asterion Twinkle".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    self.flash(&TWNK1, DROOL, term).await;
                    self.flash(&TWNK2, DROOL, term).await;
                    self.flash(&TWNK3, DROOL, term).await;
                    self.flash(&TWNK4, DROOL, term).await;
                    self.flash(&TWNK5, DROOL, term).await;
                    self.flash(&TWNK6, DROOL, term).await;
                    self.flash(&TWNK7, DROOL, term).await;
                    self.flash(&TWNK8, DROOL, term).await;
                    self.flash(&TWNK9, DROOL, term).await;
                    self.flash(&TWNK10, DROOL, term).await;
                    self.flash(&TWNK11, DROOL, term).await;
                    self.flash(&TWNK12, DROOL, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('1') => {
                // snowman w/ white
                self.ui_state.current_routine = "Snowman 1".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    let p1 = self.snowman1;
                    self.flash(&p1, SLP, term).await;
                    rotate13(&mut self.snowman1);
                    self.flash(&DARK, SLP, term).await;
                    let p2 = self.snowman1;
                    self.flash(&p2, SLP, term).await;
                    rotate13(&mut self.snowman1);
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('2') => {
                // snowman w/ white1
                self.ui_state.current_routine = "Snowman 2".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    let p1 = self.snowman2;
                    self.flash(&p1, SLP, term).await;
                    rotate13(&mut self.snowman2);
                    self.flash(&DARK, SLP, term).await;
                    let p2 = self.snowman2;
                    self.flash(&p2, SLP, term).await;
                    rotate13(&mut self.snowman2);
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('3') => {
                // tree1 (gold sparkle, colorguard red)
                self.ui_state.current_routine = "Tree 1".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    let p1 = self.tree1;
                    self.flash(&p1, SLP, term).await;
                    rotate13(&mut self.tree1);
                    self.flash(&DARK, SLP, term).await;
                    let p2 = self.tree1;
                    self.flash(&p2, SLP, term).await;
                    rotate13(&mut self.tree1);
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('4') => {
                // tree2 (white sparkle, colorguard red/gold)
                self.ui_state.current_routine = "Tree 2".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    let p1 = self.tree2;
                    self.flash(&p1, SLP, term).await;
                    rotate13(&mut self.tree2);
                    self.flash(&DARK, SLP, term).await;
                    let p2 = self.tree2;
                    self.flash(&p2, SLP, term).await;
                    rotate13(&mut self.tree2);
                    self.flash(&DARK, SLP, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('5') => {
                // Idaho spelloff
                self.ui_state.current_routine = "Idaho Spelloff".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                self.flash(&VANDAL_I, SLO, term).await;
                self.flash(&VANDAL_D, SLO, term).await;
                self.flash(&VANDAL_A, FAST, term).await;
                self.flash(&VANDAL_H, FAST, term).await;
                self.flash(&VANDAL_O, SLO, term).await;
            }
            Some('6') => {
                // falldown vandals
                self.ui_state.current_routine = "Falldown".to_string();
                self.ui_state.loop_status = LoopStatus::Idle;
                for p in &FALLDOWN[0..=16] {
                    self.flash(p, PHISH, term).await;
                }
                self.flash(&FALLDOWN[16], FOURPHISH, term).await;
                for i in (1..=15).rev() {
                    self.flash(&FALLDOWN[i], PHISH, term).await;
                }
                self.send(&FALLDOWN[0], term).await;
            }
            Some('7') => {
                // channels go up 1 by 1
                self.ui_state.current_routine = "Channel Stagger".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                loop {
                    let myterm = Terminal::poll_key();
                    for p in &FALLDOWN[0..=16] {
                        self.flash(p, PHISH, term).await;
                    }
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('<') => {
                self.ui_state.status_message = Some("Getting dimmer... TBA".to_string());
                let _ = term.draw(&self.ui_state);
            }
            Some('>') => {
                self.ui_state.status_message = Some("Getting brighter... TBA".to_string());
                let _ = term.draw(&self.ui_state);
            }
            Some('[') => {
                // marquee left
                self.ui_state.current_routine = "Marquee Left".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                self.sorc = IG;
                loop {
                    let myterm = Terminal::poll_key();
                    marquee_left(&mut self.sorc);
                    let current = self.sorc;
                    self.flash(&current, DAB, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some(']') => {
                // marquee right
                self.ui_state.current_routine = "Marquee Right".to_string();
                self.ui_state.loop_status = LoopStatus::Looping;
                self.sorc = IG;
                loop {
                    let myterm = Terminal::poll_key();
                    marquee_right(&mut self.sorc);
                    let current = self.sorc;
                    self.flash(&current, DAB, term).await;
                    if myterm == Some(',') {
                        self.ui_state.loop_status = LoopStatus::Idle;
                        self.ui_state.current_routine = "IDLE".to_string();
                        let _ = term.draw(&self.ui_state);
                        break;
                    }
                }
            }
            Some('.') => {
                self.ui_state.status_message = Some("Shutting down...".to_string());
                let _ = term.draw(&self.ui_state);
                return Action::Quit;
            }
            _ => {
                // default: idle tick
                tokio::time::sleep(DAB).await;
            }
        }

        Action::Continue
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ui_state_initialization() {
        let state = UiState::default();
        assert_eq!(state.current_routine, "IDLE");
        assert_eq!(state.loop_status, LoopStatus::Idle);
        assert_eq!(state.last_key, None);
        assert_eq!(state.packet_count, 0);
        assert_eq!(state.last_packet, [0u8; PACKET_LEN]);
        assert_eq!(state.is_error, false);
    }
}
