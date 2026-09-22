#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "Arduino.h"
#include "SPI.h"

// Arduino normally generates these sketch prototypes.
void interrupt_routine();
void handle_rx();
void handle_tx();
void setColor(int red, int green, int blue);

#include DRIVER_SOURCE
#include SKETCH_SOURCE

#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error( \
        std::string(__func__) + ": " + #condition); \
} while (false)

struct Access {
    bool long_address;
    bool write;
    unsigned address;
    byte value;
};

struct Mock {
    std::array<byte, 64> short_registers{};
    std::array<byte, 1024> long_registers{};
    std::vector<Access> accesses;
    std::vector<std::array<int, 3>> colors;
    std::vector<unsigned long> delays;
    std::array<int, 3> color{};
    std::vector<byte> command;
    bool selected = false;
    std::function<void(const Access&)> on_access;
    unsigned atomic_depth = 0;
    unsigned atomic_entries = 0;
    std::function<void()> on_atomic_enter;
    std::function<void()> on_atomic_exit;
    std::function<void()> on_pwm;
    bool snapshot_only = false;
};

Mock& mock() {
    static Mock value;
    return value;
}

uint8_t DDRC = 0, PINC = 0xff, SREG = 0x80;
SPIClass SPI;

uint8_t mock_atomic_enter() {
    const uint8_t saved = SREG;
    noInterrupts();
    ++mock().atomic_depth;
    ++mock().atomic_entries;
    if (mock().on_atomic_enter) mock().on_atomic_enter();
    return saved;
}

void mock_atomic_exit(uint8_t saved) {
    CHECK(!(SREG & 0x80));
    CHECK(mock().atomic_depth > 0);
    --mock().atomic_depth;
    SREG = saved;
    if (mock().on_atomic_exit) mock().on_atomic_exit();
}

void pinMode(int, int) {}
void attachInterrupt(int, void (*)(), int) {}
void noInterrupts() { SREG &= ~0x80; }
void interrupts() { SREG |= 0x80; }
void delay(unsigned long milliseconds) { mock().delays.push_back(milliseconds); }
void SoftPWMBegin() {}
void SoftPWMSetFadeTime(int, int, int) {}
void SoftPWMSet(int pin, int value) {
    CHECK(mock().atomic_depth == 0);
    if (mock().on_pwm) mock().on_pwm();
    CHECK(pin == redPin || pin == greenPin);
    mock().color[pin == redPin ? 0 : 1] = value;
}
void analogWrite(int pin, int value) {
    CHECK(mock().atomic_depth == 0);
    if (mock().on_pwm) mock().on_pwm();
    CHECK(pin == bluePin);
    mock().color[2] = value;
    mock().colors.push_back(mock().color);
}

void digitalWrite(int pin, int value) {
    if (pin != pin_cs) return;
    auto& state = mock();
    if (value == LOW) {
        CHECK(!state.selected);
        state.selected = true;
        state.command.clear();
    } else {
        CHECK(state.selected);
        CHECK(state.command.size() == ((state.command[0] & 0x80) ? 3u : 2u));
        state.selected = false;
    }
}

byte SPIClass::transfer(byte value) {
    auto& state = mock();
    CHECK(state.selected);
    CHECK(!state.snapshot_only || state.atomic_depth == 0);
    state.command.push_back(value);
    const byte first = state.command[0];
    const bool is_long = first & 0x80;
    if (state.command.size() < (is_long ? 3u : 2u)) return 0;
    const unsigned address = is_long
        ? ((first & 0x7f) << 3) | (state.command[1] >> 5)
        : (first >> 1) & 0x3f;
    const bool write = is_long ? state.command[1] & 0x10 : first & 1;
    byte& reg = is_long ? state.long_registers.at(address) : state.short_registers.at(address);
    const byte result = reg;
    const Access access{is_long, write, address, write ? value : result};
    state.accesses.push_back(access);
    if (state.on_access) state.on_access(access);
    if (write) reg = value;
    else if (!is_long && address == MRF_INTSTAT) reg = 0;
    return result;
}

void reset_receiver() {
    mock() = Mock{};
    *mrf.get_rxinfo() = rx_info_t{};
    *mrf.get_txinfo() = tx_info_t{};
    std::fill(mrf.get_rxbuf(), mrf.get_rxbuf() + 127, 0);
    flag_got_rx = flag_got_tx = 0;
    mrf.set_bufferPHY(false);
    SREG = 0x80;
    PINC = 0xff;
    idVal = startId = 0;
}

std::vector<byte> payload(unsigned size, byte seed = 1) {
    std::vector<byte> data(size);
    for (unsigned i = 0; i < size; ++i) data[i] = static_cast<byte>(seed + i * 7);
    return data;
}

void load_packet(const std::vector<byte>& data) {
    auto& state = mock();
    state.long_registers.fill(0xee);
    const unsigned length = data.size() + 11;
    state.long_registers[0x300] = length;
    for (unsigned i = 0; i < data.size(); ++i) state.long_registers[0x30a + i] = data[i];
    state.long_registers[0x301 + length] = 0xa7;
    state.long_registers[0x302 + length] = 0x53;
    state.short_registers[MRF_INTSTAT] = MRF_I_RXIF;
    state.accesses.clear();
}

unsigned fifo_reads() {
    return std::count_if(mock().accesses.begin(), mock().accesses.end(), [](const Access& access) {
        return access.long_address && !access.write && access.address >= 0x300;
    });
}

void check_payload(const std::vector<byte>& data) {
    CHECK(mrf.rx_datalength() == static_cast<int>(data.size()));
    CHECK(std::equal(data.begin(), data.end(), mrf.get_rxinfo()->rx_data));
    CHECK(mrf.get_rxinfo()->lqi == 0xa7);
    CHECK(mrf.get_rxinfo()->rssi == 0x53);
}

void setup_disables_raw_buffering() {
    reset_receiver();
    mrf.set_bufferPHY(true);
    setup();
    CHECK(!mrf.get_bufferPHY());
    CHECK(redPin == 3 && greenPin == 4 && bluePin == 5);
}

void raw_buffering_equivalence() {
    const auto data = payload(96);
    unsigned reads[2]{};
    for (int raw = 0; raw < 2; ++raw) {
        reset_receiver();
        mrf.set_bufferPHY(raw);
        load_packet(data);
        mrf.interrupt_handler();
        check_payload(data);
        reads[raw] = fifo_reads();
        for (unsigned i = 0; i < 107; ++i) {
            CHECK(mrf.get_rxbuf()[i] == (raw ? mock().long_registers[0x301 + i] : 0));
        }
    }
    CHECK(reads[0] == 99);
    CHECK(reads[1] == 206);
    std::cout << "  96-byte payload: FIFO reads " << reads[1] << " -> " << reads[0] << '\n';
}

void check_receive_accesses(unsigned length, bool raw, bool valid, bool tx = false) {
    CHECK(fifo_reads() == (valid ? 3 + length - 11 + (raw ? length : 0) : 1));
    unsigned flushes = 0, disables = 0, enables = 0, tx_reads = 0;
    for (const auto& access : mock().accesses) {
        if (access.long_address) {
            CHECK(!access.write);
            CHECK(access.address >= 0x300);
            if (!valid) CHECK(access.address == 0x300);
            else {
                CHECK(access.address <= 0x302 + length);
                CHECK(raw || access.address == 0x300 ||
                    (access.address >= 0x30a && access.address < 0x30a + length - 11) ||
                    access.address == 0x301 + length || access.address == 0x302 + length);
            }
        } else if (access.write) {
            CHECK(access.address == MRF_BBREG1 || access.address == MRF_RXFLUSH);
            if (access.address == MRF_RXFLUSH) {
                CHECK(access.value == 1);
                CHECK(disables == 1 && enables == 0);
                ++flushes;
            } else if (access.value == 4) ++disables;
            else {
                CHECK(access.value == 0 && disables == 1);
                ++enables;
            }
        } else {
            CHECK(access.address == MRF_INTSTAT || access.address == MRF_TXSTAT);
            if (access.address == MRF_TXSTAT) ++tx_reads;
        }
    }
    CHECK(disables == 1 && enables == 1);
    CHECK(flushes == (valid ? 0u : 1u));
    CHECK(tx_reads == (tx ? 1u : 0u));
    CHECK(mock().short_registers[MRF_BBREG1] == 0);
    CHECK(!mock().selected);
}

void first_and_alternating_packets() {
    for (bool raw : {false, true}) {
        reset_receiver();
        mrf.set_bufferPHY(raw);
        for (unsigned size : {3, 96, 1, 116, 0, 48, 2}) {
            const auto data = payload(size, size + 1);
            const rx_info_t before = *mrf.get_rxinfo();
            flag_got_rx = 0;
            load_packet(data);
            mock().on_access = [](const Access& access) {
                CHECK(!(SREG & 0x80));
                if (access.long_address) CHECK(flag_got_rx == 0);
            };
            mrf.interrupt_handler();
            CHECK(flag_got_rx == 1);
            check_payload(data);
            CHECK(std::equal(before.rx_data + size, before.rx_data + 116,
                             mrf.get_rxinfo()->rx_data + size));
            check_receive_accesses(size + 11, raw, true);
        }
    }
}

void fifo_length_boundaries() {
    for (bool raw : {false, true}) {
        for (unsigned length : {0, 10, 11, 127, 128, 255}) {
            for (byte pending : {0, 1}) {
                reset_receiver();
                mrf.set_bufferPHY(raw);
                load_packet(payload(48));
                mrf.interrupt_handler();
                const rx_info_t previous = *mrf.get_rxinfo();
                std::array<byte, 127> previous_raw;
                std::copy(mrf.get_rxbuf(), mrf.get_rxbuf() + 127, previous_raw.begin());
                const bool valid = length >= 11 && length <= 127;
                const auto data = payload(valid ? length - 11 : 0, 0x51);
                load_packet(data);
                mock().long_registers[0x300] = length;
                flag_got_rx = pending;
                mock().on_access = [pending](const Access& access) {
                    CHECK(!(SREG & 0x80));
                    if (access.long_address) CHECK(flag_got_rx == pending);
                };
                mrf.interrupt_handler();
                check_receive_accesses(length, raw, valid);
                if (valid) {
                    check_payload(data);
                    CHECK(flag_got_rx == 1);
                } else {
                    CHECK(std::memcmp(&previous, mrf.get_rxinfo(), sizeof(previous)) == 0);
                    CHECK(std::equal(previous_raw.begin(), previous_raw.end(), mrf.get_rxbuf()));
                    CHECK(flag_got_rx == pending);
                }
                unsigned callbacks = 0;
                // check_flags takes plain functions, so count through the mock trace.
                mock().colors.clear();
                mrf.check_flags([] { mock().colors.push_back({1, 2, 3}); }, [] {});
                callbacks = mock().colors.size();
                CHECK(callbacks == (valid || pending ? 1u : 0u));
                CHECK(flag_got_rx == 0);
                mock().on_access = nullptr;
                load_packet(payload(5, 0x73));
                mrf.interrupt_handler();
                check_payload(payload(5, 0x73));
                CHECK(flag_got_rx == 1);
            }
        }
    }
}

void interrupt_state_and_tx_completion() {
    for (byte entry : {0x25, 0xa5}) {
        for (bool raw : {false, true}) {
            for (bool valid : {false, true}) {
                for (byte status : {0, MRF_I_RXIF, MRF_I_TXNIF, MRF_I_RXIF | MRF_I_TXNIF}) {
                    reset_receiver();
                    mrf.set_bufferPHY(raw);
                    load_packet(payload(3));
                    if (!valid) mock().long_registers[0x300] = 255;
                    mock().short_registers[MRF_INTSTAT] = status;
                    mock().short_registers[MRF_TXSTAT] = 0xa1;
                    SREG = entry;
                    mock().on_access = [](const Access&) { CHECK(!(SREG & 0x80)); };
                    mrf.interrupt_handler();
                    CHECK(SREG == entry);
                    CHECK(mock().atomic_depth == 0);
                    CHECK(flag_got_rx == ((status & MRF_I_RXIF) && valid ? 1 : 0));
                    CHECK(flag_got_tx == (status & MRF_I_TXNIF ? 1 : 0));
                    if (status & MRF_I_RXIF) {
                        check_receive_accesses(valid ? 14 : 255, raw, valid, status & MRF_I_TXNIF);
                    }
                    if (status & MRF_I_TXNIF) {
                        CHECK(mrf.get_txinfo()->tx_ok == 0);
                        CHECK(mrf.get_txinfo()->retries == 2);
                        CHECK(mrf.get_txinfo()->channel_busy == 1);
                        mock().colors.clear();
                        mrf.check_flags([] {}, [] { mock().colors.push_back({4, 5, 6}); });
                        CHECK(mock().colors.size() == 1);
                        CHECK(flag_got_tx == 0);
                    }
                }
            }
        }
    }
}

void pending_notification_does_not_wrap() {
    reset_receiver();
    for (unsigned i = 0; i < 300; ++i) {
        const auto data = payload(3, i);
        load_packet(data);
        mrf.interrupt_handler();
        CHECK(flag_got_rx != 0);
        check_payload(data);
    }
    mrf.check_flags([] {}, [] {});
    CHECK(flag_got_rx == 0);
}

void receive(const std::vector<byte>& data) {
    load_packet(data);
    mrf.interrupt_handler();
}

void apply_snapshot() {
    const byte entry = SREG;
    const unsigned entries = mock().atomic_entries;
    mock().snapshot_only = true;
    mock().accesses.clear();
    handle_rx();
    mock().snapshot_only = false;
    CHECK(SREG == entry);
    CHECK(mock().atomic_entries == entries + 1);
    CHECK(mock().atomic_depth == 0);
    CHECK(mock().accesses.size() == 1);
    const auto& flush = mock().accesses[0];
    CHECK(!flush.long_address && flush.write && flush.address == MRF_RXFLUSH && flush.value == 1);
}

void rgb_length_boundaries_and_ids() {
    for (unsigned id = 0; id < 16; ++id) {
        for (byte entry : {0x25, 0xa5}) {
            reset_receiver();
            PINC = static_cast<byte>(~id);
            setup();
            CHECK(idVal == static_cast<int>(id));
            CHECK(startId == static_cast<int>(id * 3));
            mock().colors.clear();
            SREG = entry;
            const auto complete = payload(startId + 3);
            receive(complete);
            apply_snapshot();
            const std::array<int, 3> expected{{complete[startId], complete[startId + 1], complete[startId + 2]}};
            CHECK(mock().color == expected);
            CHECK(mock().colors.size() == 1);
            for (unsigned size : {0u, static_cast<unsigned>(startId + 1),
                                  static_cast<unsigned>(startId + 2)}) {
                receive(payload(size, 0xc1));
                apply_snapshot();
                CHECK(mock().colors.size() == 1);
                CHECK(mock().color == expected);
            }
            receive(complete);
            mock().colors.clear();
            mock().delays.clear();
            loop();
            if (RECEIVER_NEW && id == 15) {
                const std::vector<std::array<int, 3>> demo{
                    {255, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0},
                    {0, 0, 255}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}
                };
                CHECK(mock().colors == demo);
                CHECK(mock().delays == std::vector<unsigned long>({250, 100, 250, 100, 250, 100, 450, 100}));
                CHECK(flag_got_rx == 1);
            } else {
                CHECK(mock().colors.size() == 1);
                CHECK(mock().color == expected);
                CHECK(mock().delays.empty());
                CHECK(flag_got_rx == 0);
            }
        }
    }
}

void repeated_colors_and_rejection() {
    const std::vector<std::array<int, 3>> colors{{127, 0, 255}, {255, 30, 0}};
    for (const auto& color : colors) {
        reset_receiver();
        const std::vector<byte> data{static_cast<byte>(color[0]), static_cast<byte>(color[1]),
                                     static_cast<byte>(color[2])};
        for (unsigned i = 0; i < 180; ++i) {
            receive(data);
            loop();
            CHECK(mock().color == color);
        }
        CHECK(mock().colors.size() == 180);
        for (unsigned length : {0, 10, 128, 255}) {
            load_packet({});
            mock().long_registers[0x300] = length;
            mrf.interrupt_handler();
            loop();
            CHECK(mock().colors.size() == 180);
            CHECK(mock().color == color);
        }
        receive({11, 22});
        loop();
        CHECK(mock().colors.size() == 180);
        CHECK(mock().color == color);
        receive(data);
        loop();
        CHECK(mock().colors.size() == 181);
        CHECK(mock().color == color);
    }
}

void packet_interleaving() {
    // Deliver a pending ISR at snapshot exit or between PWM setters. Either way,
    // the chosen color must remain entirely from the earlier packet.
    for (bool at_pwm : {false, true}) {
        for (bool complete : {false, true}) {
            reset_receiver();
            const std::vector<byte> next{11, 22, 33};
            receive(complete ? std::vector<byte>{127, 0, 255} : std::vector<byte>{127, 0});
            setColor(9, 8, 7);
            mock().colors.clear();
            bool pending = false, delivered = false;
            mock().snapshot_only = true;
            mock().on_atomic_enter = [&] {
                if (!delivered) {
                    CHECK(!(SREG & 0x80));
                    pending = true;
                }
            };
            const auto deliver = [&] {
                if (!pending || delivered || !(SREG & 0x80)) return;
                delivered = true;
                CHECK(mock().atomic_depth == 0);
                const bool snapshot_only = mock().snapshot_only;
                mock().snapshot_only = false;
                // Model AVR ISR entry/return, not a nested enabled interrupt.
                const byte saved = SREG;
                noInterrupts();
                receive(next);
                CHECK(!(SREG & 0x80));
                SREG = saved;
                mock().snapshot_only = snapshot_only;
            };
            mock().on_atomic_exit = [&] { if (!at_pwm) deliver(); };
            mock().on_pwm = [&] { if (at_pwm) deliver(); };
            handle_rx();
            CHECK(pending);
            CHECK(delivered == (!at_pwm || complete));
            const std::array<int, 3> expected = complete
                ? std::array<int, 3>{{127, 0, 255}} : std::array<int, 3>{{9, 8, 7}};
            CHECK(mock().color == expected);
            CHECK(mock().colors.size() == (complete ? 1u : 0u));
            // Incomplete packets never call a setter, so service that pending ISR now.
            deliver();
            CHECK(delivered);
            mock().on_atomic_enter = nullptr;
            mock().on_atomic_exit = nullptr;
            mock().on_pwm = nullptr;
            mock().snapshot_only = false;
            apply_snapshot();
            CHECK(mock().color == (std::array<int, 3>{{11, 22, 33}}));
        }
    }
}

int main() {
    int failures = 0;
    const std::pair<const char*, void (*)()> tests[] = {
        {"setup disables raw buffering", setup_disables_raw_buffering},
        {"raw buffering equivalence", raw_buffering_equivalence},
        {"first and alternating packets", first_and_alternating_packets},
        {"FIFO length boundaries", fifo_length_boundaries},
        {"interrupt state and TX completion", interrupt_state_and_tx_completion},
        {"pending notification does not wrap", pending_notification_does_not_wrap},
        {"RGB length boundaries and IDs", rgb_length_boundaries_and_ids},
        {"repeated colors and rejection", repeated_colors_and_rejection},
        {"packet interleaving", packet_interleaving},
    };
    for (const auto& test : tests) {
        try {
            test.second();
            std::cout << "PASS " << test.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
        }
    }
    return failures ? 1 : 0;
}