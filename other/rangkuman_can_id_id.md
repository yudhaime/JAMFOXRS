# Mapping CAN ID Votol dan BMS

## Ringkasan Semua CAN ID

| CAN ID | Tipe | Arah / kelompok | Fungsi |
|---|---|---|---|
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

## CAN Extended 29-bit: Votol Controller

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
