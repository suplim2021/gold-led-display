# เครื่องแสดงราคาทองคำไทย — ESP32 + MAX7219 (32×8)

แสดงราคา **ทองคำแท่ง 96.5% (บาทต่อทองหนัก 1 บาท)** บนจอ LED Dot Matrix สีแดง 4-in-1
ดึงข้อมูลเองผ่าน Wi-Fi ทุก ~60 วินาที ไม่ต้องเปิดคอมพิวเตอร์ค้างไว้

ข้อมูล: [thaigoldtoday.com](https://www.thaigoldtoday.com) (อ้างอิงสมาคมค้าทองคำ — เว็บขอให้ระบุที่มาเมื่อนำไปใช้)

![ตัวอย่างหน้าจอ](docs/preview.png)

## สถานะ

| ส่วน | สถานะ |
|---|---|
| Firmware คอมไพล์สำหรับ ESP32 (`pio run -e esp32dev`) | ✅ ผ่าน |
| Unit tests บน PC (`pio test -e native`) — 53 กรณี | ✅ ผ่าน |
| ตรวจ API จริง (endpoint, JSON schema, HTTPS) | ✅ ตรวจแล้ว 2026-11-05 |
| ตัวจำลองจอ 32×8 | ✅ |
| อัปโหลดเข้า ESP32 จริง / Wi-Fi / TLS บนบอร์ด | ⏳ รอฮาร์ดแวร์ |
| การต่อสาย, ไฟเลี้ยง, การแสดงผลบนจอจริง, ทิศทางภาพ | ⏳ รอฮาร์ดแวร์ |

## โครงสร้าง

```text
gold-led-display/
├── platformio.ini              env: esp32dev (firmware), native (tests)
├── include/gold_config.example.h   ค่าตั้ง (คัดลอกเป็น gold_config.h)
├── lib/GoldCore/src/           logic ที่ไม่ขึ้นกับฮาร์ดแวร์ (ทดสอบบน PC ได้)
│   ├── Font.*                  ฟอนต์ 4×6 และ 3×6
│   ├── DisplayRenderer.*       จัดวางตัวเลขกึ่งกลาง / ตัวบ่งชี้
│   ├── GoldPriceParser.*       แปลง JSON จาก API
│   ├── GoldPriceProvider.h     interface แหล่งข้อมูล (เปลี่ยนแหล่งได้)
│   ├── PriceTracker.*          ความสดของข้อมูล / backoff
│   └── ScreenSelector.*        เลือกหน้าจอ Mode A/B/C
├── src/                        ส่วน ESP32
│   ├── main.cpp
│   ├── GoldPriceClient.*       HTTPS client (WiFiClientSecure + root CA)
│   ├── WiFiManager.*           เชื่อมต่อใหม่แบบ backoff
│   ├── LedMatrix.*             ส่งภาพไป MD_MAX72XX
│   ├── PriceStore.*            บันทึกราคาล่าสุดใน NVS
│   └── RootCA.h
├── test/                       Unity tests (display, parser, runtime)
├── tools/preview/              ตัวจำลองจอ 32×8 (ASCII + HTML)
└── docs/WIRING.md              ผังต่อสาย (แบบ 5 เส้น และแบบ HCT125)
```

## เริ่มใช้งาน

1. ติดตั้ง [PlatformIO](https://platformio.org/install) (VS Code extension หรือ `pip install platformio`)
2. คัดลอกค่าตั้ง แล้วใส่ชื่อ/รหัส Wi-Fi (2.4 GHz):
   ```bash
   cp include/gold_config.example.h include/gold_config.h
   ```
3. ต่อสายตาม [docs/WIRING.md](docs/WIRING.md)
4. Build + อัปโหลดผ่าน USB-C:
   ```bash
   pio run -e esp32dev -t upload
   ```
5. ดู log:
   ```bash
   pio device monitor
   ```

> ไฟล์ `include/gold_config.h` อยู่ใน `.gitignore` — รหัส Wi-Fi จะไม่ถูก commit
> (ชื่อไม่ใช้ `config.h` เพราะชนกับ header ใน ESP32 SDK)

## การทดสอบบน PC (ไม่ต้องมีฮาร์ดแวร์)

```bash
pio test -e native
```

ครอบคลุม: ฟอนต์ 0–9 ทั้งสองขนาด, 5/6 หลัก, กึ่งกลาง, ขอบบน/ล่าง 1 แถว, ตรวจ overflow,
JSON ถูก/ผิด/ว่าง, ราคาหาย, HTTP error, timeout, comma ในราคา, schema เปลี่ยน, timestamp เก่า/อนาคต,
Wi-Fi หลุด/กลับมา, API ล่ม, รีสตาร์ทแล้วกู้ราคาเดิม, NTP ไม่ sync, millis() วนรอบ, Mode B

ตัวจำลองจอ (ใช้โค้ดเดียวกับ firmware):

```bash
tools/preview/run.sh                     # ชุดตัวอย่าง -> tools/preview/out/preview.html
tools/preview/run.sh 65950 100000        # ราคาที่ต้องการ
curl -s https://www.thaigoldtoday.com/api/gold-price -o /tmp/g.json && tools/preview/run.sh --json /tmp/g.json
```

## การแสดงผล

ฟอนต์ bitmap ออกแบบเฉพาะ 32×8 ไม่มี comma, ตัวเลขอยู่แถว 1–6 (เว้นแถว 0 และ 7)

| ราคา | ฟอนต์ | ความกว้าง | ขอบซ้าย/ขวา |
|---|---|---|---|
| 5 หลัก เช่น 65950 | 4×6, เว้น 1 | 24 | 4 / 4 |
| 6 หลัก เช่น 100000 | 3×6, เว้น 1 | 23 | 4 / 5 |

### Mode A — ราคาขายออก (ค่าเริ่มต้น)
ตัวเลขล้วน ไม่มีสัญลักษณ์ใด ๆ

### Mode B — สลับขายออก / รับซื้อ ทุก 5 วินาที
ตัวเลขล้วนแยกไม่ออกว่าเป็นราคาไหน จึงมีจุด 2 ดวงที่คอลัมน์ซ้ายสุด:
- **จุดมุมซ้ายบน** = ราคา **ขายออก** (ราคาสูงกว่า → อยู่บน)
- **จุดมุมซ้ายล่าง** = ราคา **รับซื้อ**

เปิดได้ 2 วิธี: ตั้ง `DISPLAY_MODE_DEFAULT 1` หรือกดปุ่ม **BOOT** บนบอร์ด (สลับ A↔B และจำค่าไว้)

### Mode C — ข้อมูลเก่า / ขัดข้อง
- ราคาล่าสุดยังแสดงอยู่ แต่มี **เส้นประใต้ตัวเลข (แถวล่างสุด)** = ราคานี้ *ไม่ใช่ราคาปัจจุบันที่ยืนยันแล้ว*
- ทุก 10 วินาที จะขึ้นรหัสสถานะ 2 วินาที:

| รหัส | ความหมาย |
|---|---|
| `E1` | ไม่มี Wi-Fi |
| `E2` | API ไม่ตอบ / HTTP error / timeout / TLS ล้มเหลว |
| `E3` | ข้อมูลผิดรูปแบบ (JSON เสีย, ฟิลด์หาย, schema เปลี่ยน, ราคาผิดปกติ) |
| `E4` | เวลาประกาศเก่ากว่า 4 วัน หรืออยู่ในอนาคต |
| `----` | กำลังเริ่มต้น ยังไม่มีข้อมูล |

ราคาจะถือว่า "สด" (ไม่มีเส้นประ) ก็ต่อเมื่อ: ดึงสำเร็จในรอบการเปิดเครื่องนี้, ภายใน 3 นาทีล่าสุด,
Wi-Fi ยังต่ออยู่ และ (ถ้านาฬิกา sync แล้ว) เวลาประกาศไม่เก่าเกิน 4 วัน
— ราคาที่กู้จาก flash หลังไฟดับ **จะมีเส้นประเสมอ** จนกว่าจะดึงใหม่สำเร็จ
(เผื่อ 4 วันเพราะสมาคมฯ ไม่ประกาศราคาวันหยุด)

## API ที่ใช้ (ตรวจสอบแล้ว)

`GET https://www.thaigoldtoday.com/api/gold-price` — ฟรี ไม่ต้องใช้ key, cache 60 วินาที

ฟิลด์ที่ใช้ (ยืนยันจากเอกสารและ response จริง 2026-11-05):

| ฟิลด์ | ใช้เป็น |
|---|---|
| `current.sellBar` | ราคาขายออกทองแท่ง (บังคับ) |
| `current.buyBar` | ราคารับซื้อทองแท่ง (บังคับ) |
| `current.updatedAt` | เวลาประกาศ ISO-8601 UTC (บังคับ) |
| `current.change` | การเปลี่ยนแปลง (ไม่บังคับ, แสดงใน log) |

หมายเหตุ: เอกสารระบุ `updateRound` แต่ response จริงไม่มี จึงไม่ใช้ฟิลด์นี้
Response จริงที่บันทึกไว้อยู่ที่ `test/fixtures/live_response.h`

ถ้าต้องเปลี่ยนแหล่งข้อมูล: สร้าง class ใหม่ที่ implement `gold::GoldPriceProvider`
(ดูตัวอย่าง `ThaiGoldTodayClient`) แล้วเปลี่ยนตัวแปร `provider` ใน `main.cpp` — ส่วนแสดงผลไม่ต้องแก้

## พฤติกรรม Firmware

1. เปิดจอ แสดง `----` (ความสว่างตาม `DISPLAY_INTENSITY`, ค่าเริ่มต้น 1)
2. อ่านค่าตั้ง + ราคาล่าสุดจาก NVS (แสดงพร้อมเส้นประ)
3. เชื่อม Wi-Fi แบบไม่ block, หลุดแล้วต่อใหม่แบบ backoff 10 วิ → สูงสุด 2 นาที (ไม่รีบูตเอง)
4. NTP (UTC) ทำงานเบื้องหลัง — ถ้า sync ไม่ได้ ข้ามการตรวจเวลาประกาศแต่ยังตรวจอายุการดึงข้อมูล
5. ดึงราคาทันทีที่ต่อ Wi-Fi ได้ แล้วทุก 60 วินาที
6. ผิดพลาด → retry 15 วิ, 30 วิ, 1 นาที … สูงสุด 5 นาที (เก็บราคาเดิมไว้)
7. เขียน NVS **เฉพาะเมื่อราคา/เวลาประกาศเปลี่ยน**
8. HTTPS ตรวจใบรับรองด้วย root CA ที่ฝังไว้ (ISRG X1/X2, GTS R1/R4)

## ค่าที่ปรับได้ (`include/gold_config.h`)

| ค่า | ค่าเริ่มต้น | ความหมาย |
|---|---|---|
| `DISPLAY_INTENSITY` | 1 | ความสว่าง 0–15 |
| `DISPLAY_HARDWARE_TYPE` | `FC16_HW` | ถ้าภาพเพี้ยน ลอง `GENERIC_HW`, `PAROLA_HW`, `ICSTATION_HW` |
| `DISPLAY_MIRROR_X` / `DISPLAY_FLIP_Y` | 0 | ภาพกลับซ้าย-ขวา / บน-ล่าง |
| `DISPLAY_CS_PIN` | 5 | ขา CS |
| `DISPLAY_MODE_DEFAULT` | 0 | 0 = Mode A, 1 = Mode B |
| `POLL_INTERVAL_MS` | 60000 | รอบดึงราคา (อย่าต่ำกว่า 60 วิ) |
| `STALE_AFTER_MS` | 180000 | ไม่ได้ข้อมูลใหม่นานเท่านี้ → เส้นประ |
| `MAX_PRICE_AGE_SEC` | 4 วัน | เวลาประกาศเก่ากว่านี้ → E4 |
| `GOLD_API_ALLOW_INSECURE_TLS` | 0 | 1 = ข้ามการตรวจใบรับรอง (ใช้เฉพาะเมื่อ TLS ใช้ไม่ได้จริง ๆ) |

## สิ่งที่ต้องทดสอบบนฮาร์ดแวร์ (Phase 2–3)

- [ ] ภาพไม่กลับด้าน/ไม่สลับโมดูล (ปรับ `DISPLAY_HARDWARE_TYPE`, `MIRROR_X`, `FLIP_Y`)
- [ ] ไฟเลี้ยงพอ, ไม่ร้อน, ไม่รีสตาร์ทเอง (ดู [WIRING.md](docs/WIRING.md))
- [ ] การต่อตรง 3.3V เสถียรหรือไม่ (ถ้าไม่ → HCT125)
- [ ] TLS handshake กับ thaigoldtoday.com บน ESP32 สำเร็จ (log `[api] buy=... sell=...`)
  ถ้าได้ `E2` ตลอดพร้อม log transport error ให้ตรวจ root CA ก่อนพิจารณา `GOLD_API_ALLOW_INSECURE_TLS`
- [ ] ถอด router → `E1` + เส้นประ, เสียบกลับ → ราคาสด
- [ ] ถอดปลั๊กแล้วเสียบใหม่ → ราคาเดิมขึ้นพร้อมเส้นประ จนดึงใหม่ได้
- [ ] รันต่อเนื่อง 24 ชม.
