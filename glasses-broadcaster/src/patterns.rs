pub const CHANNELS: usize = 32;
pub const PACKET_LEN: usize = CHANNELS * 3; // 96
pub type Packet = [u8; PACKET_LEN];

pub const DARK: Packet = [0; PACKET_LEN];

/// Warm white used by tests and the default fixture frame.
pub const WHITE: Packet = [
    255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255,
    255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255,
    215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215,
    255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255,
    255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255, 215, 255, 255,
    215,
];

/// Full-intensity RGB white `(255,255,255)` on every channel.
/// Distinct from warm [`WHITE`] `(255,255,215)`.
pub const FULL_RGB: Packet = [255; PACKET_LEN];

/// Convert HSV to RGB. `hue` is degrees in `0..=360` (360 wraps to 0).
#[must_use]
pub fn hsv_to_rgb(hue: u16, saturation: u8, value: u8) -> [u8; 3] {
    let hue = hue % 360;
    if saturation == 0 {
        return [value, value, value];
    }
    let region = hue / 60;
    let remainder = u32::from(hue % 60);
    let saturation = u32::from(saturation);
    let value = u32::from(value);
    let p_val = value * (255 - saturation) / 255;
    let q_val = value * (255 - saturation * remainder / 60) / 255;
    let t_val = value * (255 - saturation * (60 - remainder) / 60) / 255;
    let byte = |x: u32| u8::try_from(x).unwrap_or(u8::MAX);
    let (v_byte, p_byte, q_byte, t_byte) = (byte(value), byte(p_val), byte(q_val), byte(t_val));
    match region {
        0 => [v_byte, t_byte, p_byte],
        1 => [q_byte, v_byte, p_byte],
        2 => [p_byte, v_byte, t_byte],
        3 => [p_byte, q_byte, v_byte],
        4 => [t_byte, p_byte, v_byte],
        _ => [v_byte, p_byte, q_byte],
    }
}

/// Spatial rainbow at 2° phase `step` (`0..180`).
///
/// Channel `i` uses hue `(step * 2 + i * 360 / 32) % 360` at full saturation/value.
#[must_use]
pub fn rainbow_drift_packet(step: u16) -> Packet {
    let mut packet = [0u8; PACKET_LEN];
    for i in 0..CHANNELS {
        let hue = (step * 2 + u16::try_from(i).unwrap_or(0) * 360 / 32) % 360;
        let rgb = hsv_to_rgb(hue, 255, 255);
        let offset = i * 3;
        packet[offset] = rgb[0];
        packet[offset + 1] = rgb[1];
        packet[offset + 2] = rgb[2];
    }
    packet
}

/// Exact port of `rotate13` from `glasses.c`.
///
/// Rotates channels 0..12 and 16..28:
/// - Channel 0 is saved and written into channel 13.
/// - Channels 1..12 are shifted left into channels 0..11.
/// - Channels 17..28 are shifted left into channels 16..27 (overwriting channel 16, leaving channel 28 unchanged).
/// - Channels 14, 15, and 29..31 are untouched.
pub fn rotate13(arr: &mut Packet) {
    let r = arr[0];
    let g = arr[1];
    let b = arr[2];

    let nrot = 13;
    for i in 3..nrot * 3 {
        let j = i + 48;
        arr[i - 3] = arr[i];
        arr[j - 3] = arr[j];
    }

    arr[nrot * 3] = r;
    arr[nrot * 3 + 1] = g;
    arr[nrot * 3 + 2] = b;
}

/// Marquee shift left by one 3-byte RGB channel (`[` key).
pub fn marquee_left(arr: &mut Packet) {
    arr.rotate_left(3);
}

/// Marquee shift right by one 3-byte RGB channel (`]` key).
pub fn marquee_right(arr: &mut Packet) {
    arr.rotate_right(3);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_packet_lengths() {
        assert_eq!(DARK.len(), PACKET_LEN);
        assert_eq!(WHITE.len(), PACKET_LEN);
        assert_eq!(FULL_RGB.len(), PACKET_LEN);
        assert_eq!(PACKET_LEN, 96);
    }

    #[test]
    fn test_spot_values() {
        assert_eq!(&DARK[0..3], &[0, 0, 0]);
        assert_eq!(&WHITE[0..3], &[255, 255, 215]);
        assert_eq!(&FULL_RGB[0..3], &[255, 255, 255]);
        assert_eq!(&FULL_RGB[PACKET_LEN - 3..], &[255, 255, 255]);
        assert!(FULL_RGB.iter().all(|&b| b == 255));
    }

    #[test]
    fn test_hsv_primary_hues() {
        assert_eq!(hsv_to_rgb(0, 255, 255), [255, 0, 0]);
        assert_eq!(hsv_to_rgb(120, 255, 255), [0, 255, 0]);
        assert_eq!(hsv_to_rgb(240, 255, 255), [0, 0, 255]);
        assert_eq!(hsv_to_rgb(60, 255, 255), [255, 255, 0]);
        assert_eq!(hsv_to_rgb(360, 255, 255), [255, 0, 0]);
    }

    #[test]
    fn test_rainbow_drift_packet() {
        let pkt = rainbow_drift_packet(0);
        assert_eq!(pkt.len(), PACKET_LEN);
        assert_eq!(&pkt[0..3], &[255, 0, 0]);
        assert_eq!(&pkt[16 * 3..16 * 3 + 3], &hsv_to_rgb(180, 255, 255));
        assert_eq!(&pkt[16 * 3..16 * 3 + 3], &[0, 255, 255]);

        let last = rainbow_drift_packet(179);
        assert_eq!(last.len(), PACKET_LEN);
        assert_eq!(&last[0..3], &hsv_to_rgb(358, 255, 255));
    }

    #[test]
    fn test_rotate13() {
        let mut pkt = [0u8; 96];
        for i in 0..32 {
            pkt[i * 3] = u8::try_from(i).unwrap();
            pkt[i * 3 + 1] = u8::try_from(i + 100).unwrap();
            pkt[i * 3 + 2] = u8::try_from(i + 200).unwrap();
        }
        let original = pkt;
        rotate13(&mut pkt);

        for ch in 0..12 {
            assert_eq!(
                &pkt[ch * 3..ch * 3 + 3],
                &original[(ch + 1) * 3..(ch + 1) * 3 + 3]
            );
        }
        assert_eq!(&pkt[12 * 3..12 * 3 + 3], &original[12 * 3..12 * 3 + 3]);
        assert_eq!(&pkt[13 * 3..13 * 3 + 3], &original[0..3]);
        assert_eq!(&pkt[14 * 3..14 * 3 + 3], &original[14 * 3..14 * 3 + 3]);
        assert_eq!(&pkt[15 * 3..15 * 3 + 3], &original[15 * 3..15 * 3 + 3]);
        for ch in 16..28 {
            assert_eq!(
                &pkt[ch * 3..ch * 3 + 3],
                &original[(ch + 1) * 3..(ch + 1) * 3 + 3]
            );
        }
        assert_eq!(&pkt[28 * 3..28 * 3 + 3], &original[28 * 3..28 * 3 + 3]);
        for ch in 29..32 {
            assert_eq!(&pkt[ch * 3..ch * 3 + 3], &original[ch * 3..ch * 3 + 3]);
        }
    }

    #[test]
    fn test_marquee() {
        let mut pkt = [0u8; PACKET_LEN];
        for i in 0..CHANNELS {
            pkt[i * 3] = u8::try_from(i).unwrap();
            pkt[i * 3 + 1] = 10;
            pkt[i * 3 + 2] = 20;
        }
        let original = pkt;
        marquee_left(&mut pkt);
        assert_ne!(pkt, original);
        marquee_right(&mut pkt);
        assert_eq!(pkt, original);

        for _ in 0..32 {
            marquee_left(&mut pkt);
        }
        assert_eq!(pkt, original);
    }
}
