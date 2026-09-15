pub mod patterns;
pub mod routines;
pub mod term;
pub mod timing;
pub mod xbee;

use patterns::DARK;
use routines::{Action, App};
use term::Terminal;
use xbee::Xbee;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // 1. Enumerate FTDI devices matching FT232 (VID 0x0403, PID 0x6001)
    let devices = Xbee::list()?;

    // 2. Open first device, configure 57600 baud 8N1
    let first_dev = devices.into_iter().next().expect("devices list was non-empty");
    let xbee = Xbee::open(first_dev).await?;

    // 3. Initialize terminal raw mode and Ratatui backend
    let mut term = Terminal::new()?;

    // 4. Initialize stateful application
    let mut app = App::new(xbee);

    // Initial UI render
    let _ = term.draw(&app.ui_state);

    // 5. Main event loop
    loop {
        let key = Terminal::poll_key();

        let action = app.handle_key(key, &mut term).await;

        // In C, a dark packet is broadcast after every command / idle tick
        app.send(&DARK, &mut term).await;

        if matches!(action, Action::Quit) {
            break;
        }
    }

    // 6. Clean shutdown: restore terminal and close USB device
    drop(term);
    app.xbee.close().await;
    println!("End of program.");

    Ok(())
}
