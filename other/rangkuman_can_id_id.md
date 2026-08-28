# Mapping CAN ID Votol dan BMS

## Ringkasan Semua CAN ID

| CAN ID | Tipe | Arah / kelompok | Fungsi |
|---|---|---|---|
| `0x3FF` | Standard 11-bit | Display -> Controller | Request/poll live data |
| `0x3FE` | Standard 11-bit | Controller -> Display | Response live data 3 frame |
| `0x9026105A` | Extended 29-bit | Display -> Controller | Speed dan odometer |
| `0x90262001` | Extended 29-bit | VCU -> Controller | VCU/controller frame |
| `0x90261022` | Extended 29-bit | Controller -> Display | RPM, speed kalkulasi, voltage, current |
| `0x90261023` | Extended 29-bit | Controller -> Status | Suhu controller dan fault 8-bit |
| `0x0A010810` | Extended 29-bit | Controller | Mode, RPM, speed kalkulasi, suhu controller, suhu motor |
| `0x0A6D0D09` | Extended 29-bit | BMS | Pack voltage, current, power, remaining capacity, full capacity |
| `0x0E6C0D09` | Extended 29-bit | BMS | 5 sensor suhu baterai |
| `0x0A6E0D09` | Extended 29-bit | BMS | SOC, SOH, cycle count |
| `0x0A6F0D09` | Extended 29-bit | BMS | Highest, lowest, average cell voltage |
| `0x0A700D09` | Extended 29-bit | BMS | Max/min battery temperature |
| `0x0A730D09` | Extended 29-bit | BMS | Balance mode, balance status, balance bitmask |
| `0x0E640D09` | Extended 29-bit | BMS cell | Cell voltage 1-4 |
| `0x0E650D09` | Extended 29-bit | BMS cell | Cell voltage 5-8 |
| `0x0E660D09` | Extended 29-bit | BMS cell | Cell voltage 9-12 |
| `0x0E670D09` | Extended 29-bit | BMS cell | Cell voltage 13-16 |
| `0x0E680D09` | Extended 29-bit | BMS cell | Cell voltage 17-20 |
| `0x0E690D09` | Extended 29-bit | BMS cell | Cell voltage 21-23 |
| `0x1810D0F3` | Extended 29-bit | Charger | Charger voltage, current, status |
| `0x1811D0F3` | Extended 29-bit | Charger | Charger voltage, current, status |
| `0x0AB40D09` | Extended 29-bit | BMS | Charging flag |
| `0x0A750D09` | Extended 29-bit | BMS | Hardware version ASCII |
| `0x0A760D09` | Extended 29-bit | BMS | Firmware version ASCII |
| `0x10261041` | Extended 29-bit | Charger / injector | Original charger detection dan injector frame |

## CAN Standard 11-bit

### `0x3FF`: Request / Poll Live Data

Direction:

```text
display/perangkat pembaca -> controller
```

Payload:

```text
Frame A: 09 55 AA AA 00 AA 00 00
Frame B: 00 18 AA 05 D2 00 20 33
```

Fungsi:

- Meminta controller mengirim live data.
- Response live data dikirim lewat `0x3FE`.
- Payload dikirim sebagai dua frame 8 byte berurutan.

### `0x3FE`: Response Live Data

Direction:

```text
controller -> display/perangkat pembaca
```

Satu update terdiri dari 3 frame `0x3FE`, masing-masing 8 byte:

```text
B0..B7   = frame 0
B8..B15  = frame 1
B16..B23 = frame 2
```

Contoh:

```text
Frame 0: 09 55 AA AA 00 00 00 01
Frame 1: 27 00 01 00 00 00 00 84
Frame 2: 00 00 4A F0 00 00 01 07
```

Mapping byte:

| Byte global | Frame byte | Fungsi | Decode |
|---:|---|---|---|
| `B0..B3` | frame0 byte0..3 | Header | Contoh `09 55 AA AA` |
| `B4..B6` | frame0 byte4..6 | Reserved | Belum didekode |
| `B7..B8` | frame0 byte7 + frame1 byte0 | Battery voltage | `((B7 << 8) | B8) / 10` V |
| `B9..B10` | frame1 byte1..2 | Battery current | `int16((B9 << 8) | B10) / 10` A |
| `B11` | frame1 byte3 | Reserved | Belum didekode |
| `B12..B15` | frame1 byte4..7 | Fault code | Big-endian `u32` |
| `B16..B17` | frame2 byte0..1 | RPM | `(B16 << 8) | B17` |
| `B18` | frame2 byte2 | Controller temp | `B18 - 50` C |
| `B19` | frame2 byte3 | External/motor temp | `B19 - 50` C |
| `B20..B21` | frame2 byte4..5 | Status | Belum didekode |
| `B22` | frame2 byte6 | Status/gear bitfield | Lihat tabel `B22` |
| `B23` | frame2 byte7 | Controller state | Lihat tabel state |

Controller state `B23`:

| Nilai | State | Arti |
|---:|---|---|
| `0` | `IDLE` | Idle |
| `1` | `INIT` | Inisialisasi |
| `2` | `START` | Start |
| `3` | `RUN` | Berjalan normal |
| `4` | `STOP` | Stop |
| `5` | `BRAKE` | Pengereman |
| `6` | `WAIT` | Menunggu |
| `7` | `FAULT` | Fault aktif |

Status/gear bitfield `B22`:

| Bit | Fungsi |
|---:|---|
| `bit0..bit1` | Gear/mode: `0=L`, `1=M`, `2=H`, `3=S` |
| `bit2` | Reverse |
| `bit3` | Park |
| `bit4` | Brake |
| `bit5` | Antitheft |
| `bit6` | Side stand |
| `bit7` | Regen |

Fault code `B12..B15`:

| Mask | Nama | Arti |
|---|---|---|
| `0x00000001` | `EBrakeOn` | Brake aktif |
| `0x00000002` | `OverCurrent` | Hardware overcurrent |
| `0x00000004` | `UnderVoltage` | Tegangan terlalu rendah |
| `0x00000008` | `ThrottleHallError` | Error Hall throttle |
| `0x00000010` | `OverVoltage` | Tegangan terlalu tinggi |
| `0x00000020` | `McuError` | Error controller/MCU |
| `0x00000040` | `MotorBlock` | Motor block error |
| `0x00000080` | `FootplateErr` | Error throttle/footplate |
| `0x00000100` | `SpeedControl` | Runaway/speed control error |
| `0x00000200` | `WritingEeprom` | Menulis EEPROM |
| `0x00000800` | `StartUpFailure` | Startup failure |
| `0x00001000` | `Overheat` | Controller overheat |
| `0x00002000` | `OverCurrent1` | Software overcurrent |
| `0x00004000` | `AcceleratePadalErr` | Throttle/pedal failure |
| `0x00008000` | `Ics1Err` | Current sensor error 1 |
| `0x00010000` | `Ics2Err` | Current sensor error 2 |
| `0x00020000` | `BreakErr` | Brake failure |
| `0x00040000` | `HallSelError` | Hall error |
| `0x00080000` | `MosfetDriverFault` | Driver failure |
| `0x00100000` | `MosfetHighShort` | MOS tube short circuit |
| `0x00200000` | `PhaseOpen` | Phase wire open |
| `0x00400000` | `PhaseShort` | Phase wire short |
| `0x00800000` | `McuChipError` | Controller/chip failure |
| `0x01000000` | `PreChargeError` | Pre-charge failure |
| `0x08000000` | `MotorOverheat` | Motor overheat |
| `0x80000000` | `SocZeroError` | SOC 0 error |

## CAN Extended 29-bit: Votol Controller

### `0x9026105A`: Display Controller

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..1` | Odometer | Little-endian `uint16 * 1000` meter |
| `5` | Speed | Nilai langsung, km/h |

### `0x90262001`: VCU Controller

| Byte | Fungsi | Decode |
|---:|---|---|
| - | VCU/controller frame | Layout belum tersedia |

### `0x90261022`: Controller Display

| Byte | Fungsi | Decode |
|---:|---|---|
| `2..3` | RPM | Little-endian `uint16` |
| `4..5` | Battery voltage | Little-endian `uint16 * 100` mV |
| `6..7` | Battery current | Little-endian `int16 * 100` mA |

Speed:

```text
speed_kmh = rpm * 0.0783744
```

### `0x90261023`: Controller Status

| Byte | Fungsi | Decode |
|---:|---|---|
| `0` | Controller temperature | `int8`, C |
| `6` | Fault code | Bitfield 8-bit |

Fault bitfield:

| Mask | Arti |
|---|---|
| `0x01` | Motor stalled |
| `0x02` | Hall sensor abnormal |
| `0x04` | Throttle abnormal |
| `0x08` | Power-on self-check error |
| `0x10` | Brake aktif saat power-on |
| `0x20` | Over-temperature |
| `0x40` | Internal 15V abnormal / controller overtemperature |

### `0x0A010810`: Controller Basic

| Byte | Fungsi | Decode |
|---:|---|---|
| `1` | Mode kendaraan | Lihat tabel mode |
| `2..3` | RPM | Little-endian: `B2 | (B3 << 8)` |
| `4` | Controller temp | C |
| `5` | Motor temp | C |

Speed:

```text
speed_kmh = rpm * 0.1033
```

Mode byte:

| Nilai | Mode |
|---|---|
| `0x00`, `0x04` | `PARK` |
| `0x61` | `CHARGING` |
| `0x70` | `DRIVE` |
| `0x50`, `0xF0`, `0x30`, `0xF8`, `0x56` | `REVERSE` |
| `0x72`, `0xB2` | `BRAKE` |
| `0xB0` | `SPORT` |
| `0x78`, `0x7A`, `0x7C`, `0x08` | `STAND` |

## CAN Extended 29-bit: BMS

### `0x0A6D0D09`: Pack Voltage, Current, Capacity

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..1` | Pack voltage | Big-endian `uint16 * 0.1` V |
| `2..3` | Pack current | Big-endian `int16 * 0.1` A |
| `4..5` | Remaining capacity | Big-endian `uint16 * 0.1` Ah |
| `6..7` | Full capacity | Big-endian `uint16 * 0.1` Ah |

Power:

```text
power_W = voltage_V * current_A
```

### `0x0E6C0D09`: Battery Temperature Sensors

| Byte | Fungsi | Decode |
|---:|---|---|
| `0` | Battery temp sensor 1 | C |
| `1` | Battery temp sensor 2 | C |
| `2` | Battery temp sensor 3 | C |
| `3` | Battery temp sensor 4 | C |
| `4` | Battery temp sensor 5 | C |

Average:

```text
battery_temp_avg = (B0 + B1 + B2 + B3 + B4) / 5
```

### `0x0A6E0D09`: SOC, SOH, Cycle Count

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..1` | SOC raw | Big-endian `uint16`, lookup/interpolasi 0-100% |
| `2..3` | SOH | Big-endian `uint16 * 0.1` %, max 100% |
| `4..5` | Cycle count | Big-endian `uint16` |

SOC:

```text
raw <= 0   -> 0%
raw >= 950 -> 100%
nilai lain -> lookup/interpolasi table SOC
```

### `0x0A6F0D09`: Cell Voltage Stats

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..1` | Highest cell voltage | Big-endian `uint16`, mV |
| `2` | Highest cell number | Nomor cell |
| `3..4` | Lowest cell voltage | Big-endian `uint16`, mV |
| `5` | Lowest cell number | Nomor cell |
| `6..7` | Average cell voltage | Big-endian `uint16`, mV |

Delta:

```text
cell_delta_mV = highest - lowest
```

### `0x0A700D09`: Temperature Stats

| Byte | Fungsi | Decode |
|---:|---|---|
| `0` | Max temp | C |
| `1` | Max temp sensor/cell number | Nomor sensor/cell |
| `4` | Min temp | C |
| `5` | Min temp sensor/cell number | Nomor sensor/cell |

### `0x0A730D09`: Balance Info

| Byte | Fungsi | Decode |
|---:|---|---|
| `0` | Balance mode | Raw |
| `1` | Balance status | Raw |
| `2` | Balance bitmask byte 0 | Bitmask cell |
| `3` | Balance bitmask byte 1 | Bitmask cell |
| `4` | Balance bitmask byte 2 | Bitmask cell |
| `5` | Balance bitmask byte 3 | Bitmask cell |

Mapping bitmask:

```text
byte_index = cell_index / 8
bit_index = cell_index % 8
cell_balancing = (balance_bits[byte_index] & (1 << bit_index)) != 0
```

### Individual Cell Voltage

Semua cell voltage dibaca big-endian `uint16` dalam mV.

| CAN ID | Cell | Layout |
|---|---|---|
| `0x0E640D09` | 1-4 | `B0..1`, `B2..3`, `B4..5`, `B6..7` |
| `0x0E650D09` | 5-8 | `B0..1`, `B2..3`, `B4..5`, `B6..7` |
| `0x0E660D09` | 9-12 | `B0..1`, `B2..3`, `B4..5`, `B6..7` |
| `0x0E670D09` | 13-16 | `B0..1`, `B2..3`, `B4..5`, `B6..7` |
| `0x0E680D09` | 17-20 | `B0..1`, `B2..3`, `B4..5`, `B6..7` |
| `0x0E690D09` | 21-23 | `B0..1`, `B2..3`, `B4..5`; slot ke-4 tidak dipakai |

### `0x0AB40D09`: Charging Flag

| Byte | Fungsi | Decode |
|---:|---|---|
| `0` | Charging flag | `0x01` = charging aktif |

### `0x0A750D09`: Hardware Version

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..5` | Hardware version | ASCII, contoh `H:v21` |

### `0x0A760D09`: Firmware Version

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..5` | Firmware version | ASCII, contoh `F:v23` |

## CAN Extended 29-bit: Charger

### `0x1810D0F3` dan `0x1811D0F3`: Charger Status

| Byte | Fungsi | Decode |
|---:|---|---|
| `0..1` | Charger voltage | Big-endian `uint16 * 0.1` V |
| `2..3` | Charger current | Big-endian `uint16 * 0.1` A |
| `4` | Charger status | Raw status byte |

### `0x10261041`: Original Charger Detection / Injector

Receive:

```text
CAN ID: 0x10261041
Fungsi: deteksi charger original
```

Transmit injector:

```text
CAN ID: 0x10261041
DLC: 8
Extended: yes
Payload: 05 0A 1C 00 00 00 00 00
```
