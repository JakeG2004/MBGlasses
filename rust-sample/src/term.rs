use std::io::{stdout, Stdout};
use std::time::Duration;

use anyhow::Result;
use crossterm::cursor::{Hide, Show};
use crossterm::event::{self, Event, KeyCode, KeyEventKind, KeyModifiers};
use crossterm::execute;
use crossterm::terminal::{
    disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use ratatui::backend::CrosstermBackend;
use ratatui::layout::{Alignment, Constraint, Direction, Layout, Rect};
use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, BorderType, Borders, Paragraph, Wrap};
use ratatui::Frame;
use ratatui::Terminal as RatatuiTerminal;

use crate::patterns::PACKET_LEN;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LoopStatus {
    Idle,
    Looping,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandInfo {
    pub key: char,
    pub name: &'static str,
    pub desc: &'static str,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RoutineCategory {
    pub title: &'static str,
    pub commands: Vec<CommandInfo>,
}

pub fn get_categories() -> Vec<RoutineCategory> {
    vec![
        RoutineCategory {
            title: "Flashes & Solids",
            commands: vec![
                CommandInfo { key: 'r', name: "Red Flash", desc: "Solid red pulse" },
                CommandInfo { key: 'e', name: "Green Flash", desc: "Solid green pulse" },
                CommandInfo { key: 'b', name: "Blue Flash", desc: "Solid blue pulse" },
                CommandInfo { key: 'g', name: "Gold Flash", desc: "Gold/yellow pulse" },
                CommandInfo { key: 'w', name: "White Flash", desc: "White pulse" },
                CommandInfo { key: 'm', name: "Magenta Flash", desc: "Magenta pulse" },
                CommandInfo { key: 'y', name: "Yellow Flash", desc: "Yellow pulse" },
                CommandInfo { key: 'k', name: "Cyan Flash", desc: "Cyan pulse" },
                CommandInfo { key: 'd', name: "Dark", desc: "All channels off" },
                CommandInfo { key: 'n', name: "Gold Loop", desc: "Continuous gold hold" },
                CommandInfo { key: 'o', name: "White Loop", desc: "Continuous white hold" },
            ],
        },
        RoutineCategory {
            title: "Twinkles & Sparkles",
            commands: vec![
                CommandInfo { key: 'a', name: "Asterion Twinkle", desc: "12-step twinkle loop" },
                CommandInfo { key: 'c', name: "Christmas Sparkle", desc: "4-phase sparkle loop" },
                CommandInfo { key: 'q', name: "Sparkle Cycle", desc: "Rainbow sparkle loop" },
                CommandInfo { key: 'x', name: "Christmas Solid", desc: "Solid holiday hold" },
                CommandInfo { key: 'z', name: "Slow Twinkle", desc: "Slow holiday shimmer" },
                CommandInfo { key: 'f', name: "Twink 8", desc: "Twinkle step 8 pulse" },
                CommandInfo { key: 'h', name: "Twink 9", desc: "Twinkle step 9 pulse" },
                CommandInfo { key: 'j', name: "Twink 10", desc: "Twinkle step 10 pulse" },
                CommandInfo { key: 'l', name: "Twink 11", desc: "Twinkle step 11 pulse" },
            ],
        },
        RoutineCategory {
            title: "Animations & Marquees",
            commands: vec![
                CommandInfo { key: 's', name: "Rainbow Short", desc: "4-color rainbow pass" },
                CommandInfo { key: 'p', name: "Rainbow Med", desc: "Multi-stage rainbow sweep" },
                CommandInfo { key: '[', name: "Marquee Left", desc: "Scrolling left loop" },
                CommandInfo { key: ']', name: "Marquee Right", desc: "Scrolling right loop" },
                CommandInfo { key: '6', name: "Falldown", desc: "Cascading channel drop" },
                CommandInfo { key: '7', name: "Channel Stagger", desc: "Continuous channel stagger" },
            ],
        },
        RoutineCategory {
            title: "Holiday & Specials",
            commands: vec![
                CommandInfo { key: '1', name: "Snowman 1", desc: "Snowman rotation loop" },
                CommandInfo { key: '2', name: "Snowman 2", desc: "Snowman alternate loop" },
                CommandInfo { key: '3', name: "Tree 1", desc: "Gold/Red tree sparkle loop" },
                CommandInfo { key: '4', name: "Tree 2", desc: "White/Red/Gold tree loop" },
                CommandInfo { key: '5', name: "Idaho Spelloff", desc: "V-A-N-D-A-L-S sequence" },
                CommandInfo { key: 't', name: "Test RGB", desc: "RGB channel test loop" },
            ],
        },
    ]
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UiState {
    pub current_routine: String,
    pub loop_status: LoopStatus,
    pub last_key: Option<char>,
    pub packet_count: u64,
    pub last_packet: [u8; PACKET_LEN],
    pub status_message: Option<String>,
    pub is_error: bool,
    pub device_connected: bool,
    pub simulated: bool,
}

impl Default for UiState {
    fn default() -> Self {
        Self {
            current_routine: "IDLE".to_string(),
            loop_status: LoopStatus::Idle,
            last_key: None,
            packet_count: 0,
            last_packet: [0; PACKET_LEN],
            status_message: Some("System Ready - Listening for commands".to_string()),
            is_error: false,
            device_connected: true,
            simulated: false, // Default is real ftdi
        }
    }
}

pub struct Terminal {
    inner: RatatuiTerminal<CrosstermBackend<Stdout>>,
}

impl Terminal {
    /// Initializes terminal raw mode and alternate screen with Ratatui backend (RAII).
    pub fn new() -> Result<Self> {
        enable_raw_mode()?;
        let mut stdout = stdout();
        execute!(stdout, EnterAlternateScreen, Hide)?;
        let backend = CrosstermBackend::new(stdout);
        let inner = RatatuiTerminal::new(backend)?;
        Ok(Self { inner })
    }

    /// Draws the complete UI frame using the provided UiState snapshot.
    pub fn draw(&mut self, state: &UiState) -> Result<()> {
        self.inner.draw(|frame| {
            render_ui(frame, state);
        })?;
        Ok(())
    }

    /// Non-blocking keyboard poll. Maps Ctrl-C to `Some('.')`.
    pub fn poll_key() -> Option<char> {
        if let Ok(true) = event::poll(Duration::ZERO)
            && let Ok(Event::Key(key_event)) = event::read()
            && key_event.kind == KeyEventKind::Press
        {
            if key_event.modifiers.contains(KeyModifiers::CONTROL)
                && key_event.code == KeyCode::Char('c')
            {
                return Some('.');
            }
            if let KeyCode::Char(c) = key_event.code {
                return Some(c);
            }
        }
        None
    }
}

pub fn render_ui(frame: &mut Frame, state: &UiState) {
    let size = frame.area();

    // Main vertical layout
    let main_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(3),  // Header
            Constraint::Min(12),    // Command cards grid
            Constraint::Length(7),  // Status & Visualizer panel
            Constraint::Length(1),  // Footer keybar
        ])
        .split(size);

    render_header(frame, main_chunks[0], state);
    render_command_grid(frame, main_chunks[1]);
    render_status_and_preview(frame, main_chunks[2], state);
    render_footer(frame, main_chunks[3]);
}

fn render_header(frame: &mut Frame, area: Rect, state: &UiState) {
    let header_chunks = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage(50),
            Constraint::Percentage(50),
        ])
        .split(area);

    let title_line = Line::from(vec![
        Span::styled("⚡ ", Style::default().fg(Color::Yellow)),
        Span::styled(
            "Ben's Halftime Light Show Toolkit",
            Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD),
        ),
    ]);

    let title_para = Paragraph::new(title_line)
        .block(
            Block::default()
                .borders(Borders::ALL)
                .border_type(BorderType::Rounded)
                .border_style(Style::default().fg(Color::Cyan)),
        )
        .alignment(Alignment::Left);
    frame.render_widget(title_para, header_chunks[0]);

    let (conn_text, conn_color) = if state.simulated {
        ("◆ SIMULATED", Color::Magenta)
    } else if state.device_connected {
        ("● ONLINE", Color::Green)
    } else {
        ("○ OFFLINE", Color::Red)
    };

    let status_line = Line::from(vec![
        Span::styled("PORT: ", Style::default().fg(Color::DarkGray)),
        Span::styled("FTDI 57600 8N1 ", Style::default().fg(Color::White)),
        Span::styled("| ", Style::default().fg(Color::DarkGray)),
        Span::styled("STATUS: ", Style::default().fg(Color::DarkGray)),
        Span::styled(conn_text, Style::default().fg(conn_color).add_modifier(Modifier::BOLD)),
    ]);

    let status_para = Paragraph::new(status_line)
        .block(
            Block::default()
                .borders(Borders::ALL)
                .border_type(BorderType::Rounded)
                .border_style(Style::default().fg(Color::Cyan)),
        )
        .alignment(Alignment::Right);
    frame.render_widget(status_para, header_chunks[1]);
}

fn render_command_grid(frame: &mut Frame, area: Rect) {
    let categories = get_categories();
    let grid_rows = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(area);

    let top_cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(grid_rows[0]);

    let bottom_cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(grid_rows[1]);

    let slots = [top_cols[0], top_cols[1], bottom_cols[0], bottom_cols[1]];
    let border_colors = [Color::Magenta, Color::Blue, Color::Green, Color::Yellow];

    for (i, cat) in categories.iter().enumerate() {
        if i >= slots.len() {
            break;
        }

        let mut lines = Vec::new();
        for cmd in &cat.commands {
            lines.push(Line::from(vec![
                Span::styled(
                    format!("[{}] ", cmd.key),
                    Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD),
                ),
                Span::styled(cmd.name, Style::default().fg(Color::White)),
                Span::styled(format!(" - {}", cmd.desc), Style::default().fg(Color::DarkGray)),
            ]));
        }

        let card = Paragraph::new(lines)
            .block(
                Block::default()
                    .title(format!(" {} ", cat.title))
                    .title_style(
                        Style::default()
                            .fg(border_colors[i])
                            .add_modifier(Modifier::BOLD),
                    )
                    .borders(Borders::ALL)
                    .border_type(BorderType::Rounded)
                    .border_style(Style::default().fg(border_colors[i])),
            )
            .wrap(Wrap { trim: true });

        frame.render_widget(card, slots[i]);
    }
}

fn render_status_and_preview(frame: &mut Frame, area: Rect, state: &UiState) {
    let block = Block::default()
        .title(" System Status & Channel Visualizer ")
        .title_style(Style::default().fg(Color::White).add_modifier(Modifier::BOLD))
        .borders(Borders::ALL)
        .border_type(BorderType::Rounded)
        .border_style(Style::default().fg(Color::DarkGray));

    let inner_area = block.inner(area);
    frame.render_widget(block, area);

    let (loop_text, loop_style) = match state.loop_status {
        LoopStatus::Idle => (
            "[IDLE]",
            Style::default().fg(Color::DarkGray),
        ),
        LoopStatus::Looping => (
            "[LOOPING]",
            Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD),
        ),
    };

    let key_display = match state.last_key {
        Some(k) => format!("'{}'", k),
        None => "None".to_string(),
    };

    let line1 = Line::from(vec![
        Span::styled("ROUTINE: ", Style::default().fg(Color::DarkGray)),
        Span::styled(
            &state.current_routine,
            Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD),
        ),
        Span::raw("  "),
        Span::styled(loop_text, loop_style),
        Span::raw("   "),
        Span::styled("LAST KEY: ", Style::default().fg(Color::DarkGray)),
        Span::styled(key_display, Style::default().fg(Color::White)),
        Span::raw("   "),
        Span::styled("PACKETS: ", Style::default().fg(Color::DarkGray)),
        Span::styled(
            format!("{}", state.packet_count),
            Style::default().fg(Color::Magenta).add_modifier(Modifier::BOLD),
        ),
    ]);

    // Visualizer line for 32 RGB channels
    let mut vis_spans = vec![
        Span::styled("CHANNELS (1-32): ", Style::default().fg(Color::DarkGray)),
    ];
    for ch in 0..32 {
        let r = state.last_packet[ch * 3];
        let g = state.last_packet[ch * 3 + 1];
        let b = state.last_packet[ch * 3 + 2];
        if r == 0 && g == 0 && b == 0 {
            vis_spans.push(Span::styled("·", Style::default().fg(Color::DarkGray)));
        } else {
            vis_spans.push(Span::styled("█", Style::default().fg(Color::Rgb(r, g, b))));
        }
        if ch < 31 {
            vis_spans.push(Span::raw(" "));
        }
    }
    let line2 = Line::from(vis_spans);

    // Message line
    let line3 = if state.is_error {
        Line::from(vec![
            Span::styled("ERROR: ", Style::default().fg(Color::Red).add_modifier(Modifier::BOLD)),
            Span::styled(
                state.status_message.as_deref().unwrap_or("Hardware Error"),
                Style::default().fg(Color::Red),
            ),
        ])
    } else {
        Line::from(vec![
            Span::styled("LOG: ", Style::default().fg(Color::DarkGray)),
            Span::styled(
                state.status_message.as_deref().unwrap_or("System Idle"),
                Style::default().fg(Color::Green),
            ),
        ])
    };

    let content_lines = vec![line1, line2, line3];
    let para = Paragraph::new(content_lines);
    frame.render_widget(para, inner_area);
}

fn render_footer(frame: &mut Frame, area: Rect) {
    let keybar = Line::from(vec![
        Span::styled(" [,] ", Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD)),
        Span::styled("Stop Loop", Style::default().fg(Color::White)),
        Span::raw("   "),
        Span::styled(" [.] / [Ctrl-C] ", Style::default().fg(Color::Red).add_modifier(Modifier::BOLD)),
        Span::styled("Quit", Style::default().fg(Color::White)),
        Span::raw("   "),
        Span::styled(" [<] ", Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD)),
        Span::styled("Dim", Style::default().fg(Color::DarkGray)),
        Span::raw("   "),
        Span::styled(" [>] ", Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD)),
        Span::styled("Brighten", Style::default().fg(Color::DarkGray)),
    ]);

    let footer_para = Paragraph::new(keybar).alignment(Alignment::Center);
    frame.render_widget(footer_para, area);
}

impl Drop for Terminal {
    fn drop(&mut self) {
        let _ = execute!(self.inner.backend_mut(), Show, LeaveAlternateScreen);
        let _ = disable_raw_mode();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ratatui::backend::TestBackend;
    use ratatui::Terminal as RatatuiTerminal;

    #[test]
    fn test_default_ui_state() {
        let state = UiState::default();
        assert_eq!(state.current_routine, "IDLE");
        assert_eq!(state.loop_status, LoopStatus::Idle);
        assert_eq!(state.packet_count, 0);
        assert!(!state.is_error);
        assert!(state.device_connected);
    }

    #[test]
    fn test_categories_and_commands() {
        let categories = get_categories();
        assert_eq!(categories.len(), 4);
        assert_eq!(categories[0].title, "Flashes & Solids");
        assert_eq!(categories[1].title, "Twinkles & Sparkles");
        assert_eq!(categories[2].title, "Animations & Marquees");
        assert_eq!(categories[3].title, "Holiday & Specials");

        let total_commands: usize = categories.iter().map(|c| c.commands.len()).sum();
        assert!(total_commands >= 25);
    }

    #[test]
    fn test_render_default_dashboard() {
        let backend = TestBackend::new(120, 30);
        let mut terminal = RatatuiTerminal::new(backend).unwrap();
        let state = UiState::default();

        terminal
            .draw(|f| {
                render_ui(f, &state);
            })
            .unwrap();

        let buffer = terminal.backend().buffer();
        let content = format!("{buffer:?}");
        assert!(content.contains("Ben's Halftime Light Show Toolkit"));
        assert!(content.contains("ONLINE"));
        assert!(content.contains("[IDLE]"));
    }

    #[test]
    fn test_render_looping_and_error_state() {
        let backend = TestBackend::new(120, 30);
        let mut terminal = RatatuiTerminal::new(backend).unwrap();
        let mut state = UiState::default();
        state.current_routine = "Christmas Sparkle".to_string();
        state.loop_status = LoopStatus::Looping;
        state.last_key = Some('c');
        state.packet_count = 42;
        state.is_error = true;
        state.status_message = Some("USB timeout writing endpoint".to_string());
        state.last_packet[0] = 255;
        state.last_packet[1] = 0;
        state.last_packet[2] = 0;

        terminal
            .draw(|f| {
                render_ui(f, &state);
            })
            .unwrap();

        let buffer = terminal.backend().buffer();
        let content = format!("{buffer:?}");
        assert!(content.contains("Christmas Sparkle"));
        assert!(content.contains("[LOOPING]"));
        assert!(content.contains("ERROR"));
        assert!(content.contains("USB timeout writing endpoint"));
    }

    #[test]
    fn test_render_constrained_dimensions() {
        let backend = TestBackend::new(40, 15);
        let mut terminal = RatatuiTerminal::new(backend).unwrap();
        let state = UiState::default();

        // Ensure small dimensions render without panicking
        let result = terminal.draw(|f| {
            render_ui(f, &state);
        });
        assert!(result.is_ok());
    }
}
