// 32x8 pixel preview — renders screens with the SAME code as the firmware
// (lib/GoldCore) and prints them as ASCII art; optionally writes an HTML page
// that looks like the red LED matrix.
//
// Build & run:   tools/preview/run.sh                 (demo set)
//                tools/preview/run.sh 65950 100000    (your own prices)
//                tools/preview/run.sh --json file.json (parse an API response)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "DisplayRenderer.h"
#include "GoldPriceParser.h"
#include "ScreenSelector.h"

using namespace gold;

struct Item {
    std::string title;
    Frame frame;
    bool ok;
};

static void printAscii(const Item& it) {
    printf("%s%s\n", it.title.c_str(), it.ok ? "" : "   [RENDER FAILED / OVERFLOW]");
    printf("  +--------------------------------+\n");
    for (int y = 0; y < Frame::kHeight; ++y) {
        printf("  |");
        for (int x = 0; x < Frame::kWidth; ++x) putchar(it.frame.get(x, y) ? '#' : '.');
        printf("|\n");
    }
    printf("  +--------------------------------+\n\n");
}

static std::string htmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '&') o += "&amp;";
        else o += c;
    }
    return o;
}

static void writeHtml(const std::vector<Item>& items, const char* path) {
    std::ofstream f(path);
    f << "<!doctype html><html lang=\"th\"><head><meta charset=\"utf-8\">"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         "<title>LED 32x8 Preview</title><style>"
         "body{background:#1b1b1b;color:#ddd;font-family:system-ui,sans-serif;margin:16px}"
         ".card{margin:0 0 20px}.t{font-size:14px;margin:0 0 6px}"
         ".m{display:inline-grid;grid-template-columns:repeat(32,1fr);gap:2px;padding:8px;"
         "background:#000;border-radius:6px;width:min(100%,520px);box-sizing:border-box}"
         ".p{aspect-ratio:1;border-radius:50%;background:#2a0505}"
         ".on{background:#ff2a1a;box-shadow:0 0 6px #ff3b2a}"
         ".bad{color:#ff8080}</style></head><body><h1>MAX7219 32x8 preview</h1>";
    for (const Item& it : items) {
        f << "<div class=\"card\"><p class=\"t" << (it.ok ? "" : " bad") << "\">"
          << htmlEscape(it.title) << (it.ok ? "" : " — RENDER FAILED") << "</p><div class=\"m\">";
        for (int y = 0; y < Frame::kHeight; ++y)
            for (int x = 0; x < Frame::kWidth; ++x)
                f << "<div class=\"p" << (it.frame.get(x, y) ? " on" : "") << "\"></div>";
        f << "</div></div>";
    }
    f << "</body></html>\n";
}

static Item priceItem(const std::string& title, int32_t price, PriceMarker m = PriceMarker::None,
                      bool stale = false) {
    Item it;
    it.title = title;
    it.ok = renderPrice(it.frame, price, m, stale).ok;
    return it;
}

static Item screenItem(const std::string& title, const Screen& s) {
    Item it;
    it.title = title;
    it.ok = renderScreen(it.frame, s);
    return it;
}

int main(int argc, char** argv) {
    std::vector<Item> items;
    const char* htmlPath = nullptr;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--html") && i + 1 < argc) {
            htmlPath = argv[++i];
        } else {
            args.push_back(argv[i]);
        }
    }

    if (args.size() >= 2 && args[0] == "--json") {
        std::ifstream in(args[1]);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string body = ss.str();
        GoldPrice p;
        const ParseError e = parseThaiGoldToday(body.data(), body.size(), p);
        if (e != ParseError::Ok) {
            fprintf(stderr, "parse error: %s\n", parseErrorName(e));
            return 1;
        }
        printf("parsed: buyBar=%d sellBar=%d change=%d updatedAt(epoch)=%lld\n\n", p.buyBar,
               p.sellBar, p.change, (long long)p.updatedAtEpoch);
        items.push_back(priceItem("Mode A — sell " + std::to_string(p.sellBar), p.sellBar));
        items.push_back(priceItem("Mode B — sell", p.sellBar, PriceMarker::Sell));
        items.push_back(priceItem("Mode B — buy", p.buyBar, PriceMarker::Buy));
    } else if (!args.empty()) {
        for (const std::string& a : args) {
            items.push_back(priceItem("price " + a, (int32_t)atol(a.c_str())));
        }
    } else {
        // Demo set (example prices only — NOT real/live data)
        items.push_back(priceItem("Mode A — 5 digits (65950)", 65950));
        items.push_back(priceItem("Mode A — 6 digits (100000)", 100000));
        items.push_back(priceItem("Mode B — sell (marker top-left)", 65950, PriceMarker::Sell));
        items.push_back(priceItem("Mode B — buy (marker bottom-left)", 65750, PriceMarker::Buy));
        items.push_back(priceItem("Mode C — stale price (dotted underline)", 65950,
                                  PriceMarker::None, true));
        items.push_back(priceItem("Mode C — stale 6 digits", 100000, PriceMarker::None, true));
        Screen s;
        items.push_back(screenItem("Booting / waiting for first price", s));
        s.kind = ScreenKind::Status;
        const char* names[] = {"", "E1 — no Wi-Fi", "E2 — API/HTTP error or timeout",
                               "E3 — bad data / schema changed", "E4 — price timestamp too old"};
        for (int i = 1; i <= 4; ++i) {
            s.fault = (Fault)i;
            items.push_back(screenItem(names[i], s));
        }
        items.push_back(priceItem("4 digits (1234) - auto centred", 1234));
        items.push_back(priceItem("All digits 4x6: 56789", 56789));
        items.push_back(priceItem("All digits 3x6: 123456", 123456));
        items.push_back(priceItem("All digits 3x6: 789000", 789000));
        items.push_back(priceItem("Overflow check: 1000000 (must fail)", 1000000));
    }

    for (const Item& it : items) printAscii(it);
    if (htmlPath) {
        writeHtml(items, htmlPath);
        printf("HTML written to %s\n", htmlPath);
    }
    return 0;
}
