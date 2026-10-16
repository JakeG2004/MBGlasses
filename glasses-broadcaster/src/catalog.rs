use std::collections::HashMap;
use std::fs;
use std::path::Path;
use std::time::Duration;

use anyhow::{bail, Context, Result};
use serde::{Deserialize, Serialize};

use crate::patterns::{Packet, PACKET_LEN};
use crate::timing;

/// Named 96-byte RGB frame stored on disk.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Frame {
    pub name: String,
    pub rgb: Vec<u8>,
}

/// Related packets kept together in one `.frame` file.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FrameSet(pub HashMap<String, Vec<u8>>);

#[derive(Debug, Deserialize)]
#[serde(untagged)]
enum FrameFile {
    Single(Frame),
    Set(FrameSet),
}

/// A cue loaded from a `.show` file. Hotkey, dashboard name, and category live here.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct Show {
    pub key: char,
    pub name: String,
    pub category: String,
    #[serde(default)]
    pub desc: String,
    #[serde(default)]
    pub looping: bool,
    pub program: Vec<Op>,
}

/// Engine opcodes. Procedural effects stay tiny instead of pre-expanded timelines.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum Op {
    Hold { frame: String, wait: Wait },
    LoadWorking { frame: String },
    HoldWorking { wait: Wait },
    Rotate13,
    MarqueeLeft,
    MarqueeRight,
}

/// Named C-era timing (`"slp"`) or an explicit millisecond delay.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum Wait {
    Named(String),
    Ms(u64),
}

impl Wait {
    /// Resolve this wait to a duration using [`timing`] names when needed.
    pub fn duration(&self) -> Result<Duration> {
        match self {
            Self::Ms(ms) => Ok(Duration::from_millis(*ms)),
            Self::Named(name) => timing::duration_for(name)
                .with_context(|| format!("unknown wait name `{name}`")),
        }
    }
}

impl Op {
    fn wait(&self) -> Option<&Wait> {
        match self {
            Self::Hold { wait, .. } | Self::HoldWorking { wait } => Some(wait),
            Self::LoadWorking { .. }
            | Self::Rotate13
            | Self::MarqueeLeft
            | Self::MarqueeRight => None,
        }
    }

    fn frame_name(&self) -> Option<&str> {
        match self {
            Self::Hold { frame, .. } | Self::LoadWorking { frame } => Some(frame.as_str()),
            Self::HoldWorking { .. }
            | Self::Rotate13
            | Self::MarqueeLeft
            | Self::MarqueeRight => None,
        }
    }
}

/// Validated in-memory show catalog: frames by name, shows by hotkey.
#[derive(Debug, Clone, Default)]
pub struct Catalog {
    frames: HashMap<String, Packet>,
    shows: HashMap<char, Show>,
}

impl Catalog {
    /// Merge frames and shows, rejecting duplicate hotkeys and unknown frame refs.
    pub fn new(frames: HashMap<String, Packet>, shows: Vec<Show>) -> Result<Self> {
        let mut by_key: HashMap<char, Show> = HashMap::with_capacity(shows.len());
        for show in shows {
            if let Some(prev) = by_key.get(&show.key) {
                bail!(
                    "duplicate hotkey '{}': '{}' and '{}'",
                    show.key,
                    prev.name,
                    show.name
                );
            }
            for op in &show.program {
                if let Some(frame) = op.frame_name() {
                    if !frames.contains_key(frame) {
                        bail!(
                            "show '{}' references unknown frame `{frame}`",
                            show.name
                        );
                    }
                }
                if let Some(Wait::Named(name)) = op.wait() {
                    if timing::duration_for(name).is_none() {
                        bail!("show '{}' has unknown wait name `{name}`", show.name);
                    }
                }
            }
            by_key.insert(show.key, show);
        }
        Ok(Self {
            frames,
            shows: by_key,
        })
    }

    /// Load `frames/` and `shows/` under `data_dir` (default `data`).
    pub fn load(data_dir: impl AsRef<Path>) -> Result<Self> {
        let data_dir = data_dir.as_ref();
        let frames = load_frames(&data_dir.join("frames"))?;
        let shows = load_shows(&data_dir.join("shows"))?;
        Self::new(frames, shows)
    }

    #[must_use]
    pub fn frame(&self, name: &str) -> Option<&Packet> {
        self.frames.get(name)
    }

    #[must_use]
    pub fn show_for_key(&self, key: char) -> Option<&Show> {
        self.shows.get(&key)
    }

    #[must_use]
    pub fn shows(&self) -> impl Iterator<Item=&Show> {
        self.shows.values()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.shows.is_empty()
    }
}

impl Frame {
    /// Convert RGB bytes into a wire packet.
    pub fn to_packet(&self) -> Result<Packet> {
        vec_to_packet(&self.name, &self.rgb)
    }
}

/// Parse a single `Frame(...)` document.
pub fn parse_frame(text: &str) -> Result<Frame> {
    ron::from_str(text).context("parsing Frame RON")
}

/// Parse a `FrameSet({ ... })` document.
pub fn parse_frame_set(text: &str) -> Result<FrameSet> {
    ron::from_str(text).context("parsing FrameSet RON")
}

/// Parse a `Show(...)` document.
pub fn parse_show(text: &str) -> Result<Show> {
    ron::from_str(text).context("parsing Show RON")
}

fn parse_frame_file(text: &str) -> Result<FrameFile> {
    if let Ok(frame) = ron::from_str::<Frame>(text) {
        return Ok(FrameFile::Single(frame));
    }
    if let Ok(set) = ron::from_str::<FrameSet>(text) {
        return Ok(FrameFile::Set(set));
    }
    let frame_err = ron::from_str::<Frame>(text).expect_err("Frame parse already failed");
    bail!("not a Frame ({frame_err}) or FrameSet")
}

fn vec_to_packet(name: &str, rgb: &[u8]) -> Result<Packet> {
    if rgb.len() != PACKET_LEN {
        bail!("frame `{name}` has {} bytes, expected {PACKET_LEN}", rgb.len());
    }
    let mut packet = [0u8; PACKET_LEN];
    packet.copy_from_slice(rgb);
    Ok(packet)
}

fn load_frames(dir: &Path) -> Result<HashMap<String, Packet>> {
    let mut frames = HashMap::new();
    if !dir.is_dir() {
        bail!("frames directory not found: {}", dir.display());
    }
    let mut entries: Vec<_> = fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .collect::<std::io::Result<Vec<_>>>()
        .with_context(|| format!("reading {}", dir.display()))?;
    entries.sort_by_key(fs::DirEntry::path);
    for entry in entries {
        let path = entry.path();
        if path.extension().and_then(|ext| ext.to_str()) != Some("frame") {
            continue;
        }
        let text = fs::read_to_string(&path)
            .with_context(|| format!("reading {}", path.display()))?;
        let file = parse_frame_file(&text)
            .with_context(|| format!("parsing {}", path.display()))?;
        match file {
            FrameFile::Single(frame) => {
                insert_frame(&mut frames, frame.name, &frame.rgb, &path)?;
            }
            FrameFile::Set(FrameSet(set)) => {
                for (name, rgb) in set {
                    insert_frame(&mut frames, name, &rgb, &path)?;
                }
            }
        }
    }
    Ok(frames)
}

fn insert_frame(
    frames: &mut HashMap<String, Packet>,
    name: String,
    rgb: &[u8],
    path: &Path,
) -> Result<()> {
    let packet = vec_to_packet(&name, rgb)
        .with_context(|| format!("in {}", path.display()))?;
    if frames.insert(name.clone(), packet).is_some() {
        bail!(
            "duplicate frame name `{name}` (from {})",
            path.display()
        );
    }
    Ok(())
}

fn load_shows(dir: &Path) -> Result<Vec<Show>> {
    if !dir.is_dir() {
        bail!("shows directory not found: {}", dir.display());
    }
    let mut shows = Vec::new();
    let mut entries: Vec<_> = fs::read_dir(dir)
        .with_context(|| format!("reading {}", dir.display()))?
        .collect::<std::io::Result<Vec<_>>>()
        .with_context(|| format!("reading {}", dir.display()))?;
    entries.sort_by_key(fs::DirEntry::path);
    for entry in entries {
        let path = entry.path();
        if path.extension().and_then(|ext| ext.to_str()) != Some("show") {
            continue;
        }
        let text = fs::read_to_string(&path)
            .with_context(|| format!("reading {}", path.display()))?;
        let show = parse_show(&text)
            .with_context(|| format!("parsing {}", path.display()))?;
        shows.push(show);
    }
    Ok(shows)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::patterns::{rainbow_drift_packet, DARK, FULL_RGB, WHITE};
    use crate::timing::SLP;

    const WHITE_FRAME_RON: &str = r#"
Frame(
    name: "white",
    rgb: [
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
        255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
    ],
)
"#;

    const SHOW_RON: &str = r#"
Show(
    key: 'w',
    name: "White Flash",
    category: "Flashes & Solids",
    desc: "fixture one-shot",
    looping: false,
    program: [
        Hold(frame: "white", wait: "slp"),
        Hold(frame: "dark", wait: 40),
    ],
)
"#;

    fn solid(name: &str, packet: Packet) -> (String, Packet) {
        (name.to_owned(), packet)
    }

    fn hold_show(key: char, name: &str, frame: &str) -> Show {
        Show {
            key,
            name: name.to_owned(),
            category: "Flashes & Solids".to_owned(),
            desc: String::new(),
            looping: false,
            program: vec![Op::Hold {
                frame: frame.to_owned(),
                wait: Wait::Named("slp".to_owned()),
            }],
        }
    }

    #[test]
    fn parse_frame_and_looping_show_from_ron() {
        let frame = parse_frame(WHITE_FRAME_RON).expect("frame");
        assert_eq!(frame.name, "white");
        assert_eq!(frame.to_packet().unwrap(), WHITE);

        let set = parse_frame_set(
            r#"
FrameSet({
    "dark": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
})
"#,
        )
            .expect("frameset");
        assert_eq!(set.0.get("dark").map(Vec::len), Some(PACKET_LEN));

        let show = parse_show(SHOW_RON).expect("show");
        assert_eq!(show.key, 'w');
        assert!(!show.looping);
        assert_eq!(
            show.program[0],
            Op::Hold {
                frame: "white".to_owned(),
                wait: Wait::Named("slp".to_owned()),
            }
        );
        assert_eq!(
            show.program[1],
            Op::Hold {
                frame: "dark".to_owned(),
                wait: Wait::Ms(40),
            }
        );
        assert_eq!(Wait::Named("slp".to_owned()).duration().unwrap(), SLP);

        let looping = parse_show(
            r#"
Show(
    key: 'c',
    name: "Christmas Sparkle",
    category: "Twinkles & Sparkles",
    looping: true,
    program: [
        Hold(frame: "white", wait: "slp"),
        Rotate13,
        MarqueeLeft,
    ],
)
"#,
        )
            .expect("looping show");
        assert!(looping.looping);
        assert_eq!(looping.program[1], Op::Rotate13);
    }

    #[test]
    fn load_repo_fixtures() {
        let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("data");
        let catalog = Catalog::load(&dir).expect("load data/");
        assert_eq!(catalog.frame("white"), Some(&WHITE));
        assert_eq!(catalog.frame("dark"), Some(&DARK));
        assert_eq!(catalog.frame("full_rgb"), Some(&FULL_RGB));
        let show = catalog.show_for_key('w').expect("white flash");
        assert_eq!(show.name, "White Flash");
        assert!(catalog.show_for_key('c').is_some());
        assert!(catalog.show_for_key('[').is_some());

        assert_eq!(catalog.frame("rd_000"), Some(&rainbow_drift_packet(0)));
        assert_eq!(catalog.frame("rd_090"), Some(&rainbow_drift_packet(90)));
        assert_eq!(catalog.frame("rd_179"), Some(&rainbow_drift_packet(179)));

        let drift = catalog.show_for_key('u').expect("rainbow drift");
        assert_eq!(drift.name, "Rainbow Drift");
        assert!(drift.looping);
        assert_eq!(drift.program.len(), 180);
        for (i, op) in drift.program.iter().enumerate() {
            match op {
                Op::Hold { frame, wait } => {
                    assert_eq!(frame, &format!("rd_{i:03}"));
                    assert_ne!(frame.as_str(), "black", "{frame}");
                    assert_ne!(frame.as_str(), "dark", "{frame}");
                    assert_eq!(wait, &Wait::Named("slp".to_owned()));
                }
                other => panic!("expected Hold, got {other:?}"),
            }
        }

        let test_rgb = catalog.show_for_key('t').expect("test rgb");
        assert_eq!(test_rgb.name, "Test RGB");
        let test_frames: Vec<&str> = test_rgb
            .program
            .iter()
            .filter_map(|op| match op {
                Op::Hold { frame, .. } => Some(frame.as_str()),
                _ => None,
            })
            .collect();
        assert!(test_frames.contains(&"black"), "{test_frames:?}");
        assert!(test_frames.contains(&"dark"), "{test_frames:?}");
    }

    #[test]
    fn reject_duplicate_hotkeys() {
        let frames = HashMap::from([solid("white", WHITE)]);
        let err = Catalog::new(
            frames,
            vec![
                hold_show('w', "White A", "white"),
                hold_show('w', "White B", "white"),
            ],
        )
            .expect_err("duplicates");
        let msg = format!("{err:#}");
        assert!(msg.contains("duplicate hotkey 'w'"), "{msg}");
        assert!(msg.contains("White A"), "{msg}");
        assert!(msg.contains("White B"), "{msg}");
    }

    #[test]
    fn reject_unknown_frame_names() {
        let frames = HashMap::from([solid("white", WHITE)]);
        let err = Catalog::new(frames, vec![hold_show('x', "Missing", "nope")])
            .expect_err("unknown frame");
        let msg = format!("{err:#}");
        assert!(msg.contains("unknown frame `nope`"), "{msg}");
    }
}
