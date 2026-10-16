use std::time::Duration;

pub const DOT: Duration = Duration::from_millis(100);
pub const DASH: Duration = Duration::from_millis(300);
pub const DORK: Duration = Duration::from_millis(150);
pub const DROOL: Duration = Duration::from_secs(1);
pub const DAB: Duration = Duration::from_millis(50);
pub const SLP: Duration = Duration::from_millis(40);
pub const SLO: Duration = Duration::from_millis(750);
pub const FAST: Duration = Duration::from_millis(375);
pub const PHISH: Duration = Duration::from_millis(50);
pub const FOURPHISH: Duration = Duration::from_secs(2);

/// Map a RON wait name (`"slp"`, `"dab"`, …) onto the C-era timing constants.
#[must_use]
pub fn duration_for(name: &str) -> Option<Duration> {
    Some(match name.to_ascii_lowercase().as_str() {
        "dot" => DOT,
        "dash" => DASH,
        "dork" => DORK,
        "drool" => DROOL,
        "dab" => DAB,
        "slp" => SLP,
        "slo" => SLO,
        "fast" => FAST,
        "phish" => PHISH,
        "fourphish" => FOURPHISH,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn named_waits_match_constants() {
        assert_eq!(duration_for("slp"), Some(SLP));
        assert_eq!(duration_for("SLP"), Some(SLP));
        assert_eq!(duration_for("fourphish"), Some(FOURPHISH));
        assert_eq!(duration_for("nope"), None);
    }
}
