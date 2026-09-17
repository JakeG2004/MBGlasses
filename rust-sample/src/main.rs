pub mod patterns;
pub mod routines;
pub mod term;
pub mod timing;
pub mod xbee;

use patterns::DARK;
use routines::{Action, App};
use term::Terminal;
use xbee::{Xbee, XbeeHandle, SimXbee};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let (simulate, log_path) = parse_args();

    let xbee = if simulate {
        eprintln!("Running in --simulate mode");
        XbeeHandle::Simulated(SimXbee::new(log_path.as_deref())?)
    } else {
    
        // 1. Enumerate FTDI devices matching FT232 (VID 0x0403, PID 0x6001)
        let devices = Xbee::list()?;

        // 2. Open first device, configure 57600 baud 8N1
        let first_dev = devices.into_iter().next().expect("devices list was non-empty");
        XbeeHandle::Real(Xbee::open(first_dev).await?)
    };

    // 3. Initialize terminal raw mode and Ratatui backend
    let mut term = Terminal::new()?;

    // 4. Initialize stateful application
    let mut app = App::new(xbee);
    app.ui_state.simulated = simulate;

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

fn parse_args() -> (bool, Option<std::path::PathBuf>) {
    let args: Vec<String> = std::env::args().collect();
    let simulate = args.iter().any(|a| a == "--simulate" || a == "-s");
    let log_path = args
        .iter()
        .position(|a| a == "--log" || a == "-l")
        .and_then(|i| args.get(i + 1))
        .map(std::path::PathBuf::from);
    (simulate, log_path)
}
