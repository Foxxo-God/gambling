#include <stdint.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
#define RGB15(r, g, b) ((uint16_t)(((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

#define KEY_A      (1u << 0)
#define KEY_B      (1u << 1)
#define KEY_SELECT (1u << 2)
#define KEY_START  (1u << 3)
#define KEY_RIGHT  (1u << 4)
#define KEY_LEFT   (1u << 5)
#define KEY_UP     (1u << 6)
#define KEY_DOWN   (1u << 7)
#define KEY_R      (1u << 8)
#define KEY_L      (1u << 9)

#define COLOR_BG RGB15(6, 13, 14)
#define COLOR_PANEL RGB15(10, 21, 19)
#define COLOR_GREEN RGB15(9, 25, 17)
#define COLOR_MINT RGB15(20, 29, 19)
#define COLOR_GOLD RGB15(31, 24, 12)
#define COLOR_RED RGB15(29, 9, 8)
#define COLOR_WHITE RGB15(31, 30, 25)
#define COLOR_DIM RGB15(17, 23, 20)
#define COLOR_BLACK RGB15(2, 4, 4)

#define REG_DISPCNT (*(volatile uint16_t *)0x04000000)
#define REG_VCOUNT (*(volatile uint16_t *)0x04000006)
#define REG_KEYINPUT (*(volatile uint16_t *)0x04000130)
#define VRAM ((volatile uint16_t *)0x06000000)

typedef struct { uint8_t rank, suit; } Card;

static const uint8_t font[36][5] = {
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4},
    {3,4,5,5,3}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2}, {6,5,6,4,4}, {2,5,5,3,1}, {6,5,6,5,5},
    {3,4,2,1,6}, {7,2,2,2,2}, {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7}, {7,5,5,5,7}, {2,6,2,2,7}, {6,1,7,4,7}, {6,1,3,1,6},
    {5,5,7,1,1}, {7,4,6,1,6}, {3,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,6}
};

static uint32_t credits;
static uint32_t wager;
static uint32_t random_state;
static uint32_t frame_count;
static uint16_t previous_keys;
static uint8_t screen;
static uint8_t menu_choice;
static uint8_t roulette_number;
static uint8_t roulette_number_bet;
static uint8_t roulette_bet_black;
static uint8_t roulette_result_valid;
static uint8_t roulette_bet_number;
static uint8_t dice_target;
static uint8_t dice_first;
static uint8_t dice_second;
static uint8_t slot_symbols[3];
static uint8_t blackjack_active;
static uint8_t blackjack_done;
static uint8_t player_count;
static uint8_t dealer_count;
static Card player_hand[12];
static Card dealer_hand[12];
static char status_line[32];

static void put_pixel(int x, int y, uint16_t color) {
    if ((unsigned)x < SCREEN_WIDTH && (unsigned)y < SCREEN_HEIGHT)
        VRAM[y * SCREEN_WIDTH + x] = color;
}

static void fill_rect(int x, int y, int width, int height, uint16_t color) {
    int row, column;
    for (row = 0; row < height; ++row)
        for (column = 0; column < width; ++column)
            put_pixel(x + column, y + row, color);
}

static void draw_rect(int x, int y, int width, int height, uint16_t color) {
    int i;
    for (i = 0; i < width; ++i) {
        put_pixel(x + i, y, color);
        put_pixel(x + i, y + height - 1, color);
    }
    for (i = 0; i < height; ++i) {
        put_pixel(x, y + i, color);
        put_pixel(x + width - 1, y + i, color);
    }
}

static void draw_circle(int center_x, int center_y, int radius, uint16_t color) {
    int x = radius, y = 0, error = 1 - radius;
    while (x >= y) {
        put_pixel(center_x + x, center_y + y, color);
        put_pixel(center_x + y, center_y + x, color);
        put_pixel(center_x - y, center_y + x, color);
        put_pixel(center_x - x, center_y + y, color);
        put_pixel(center_x - x, center_y - y, color);
        put_pixel(center_x - y, center_y - x, color);
        put_pixel(center_x + y, center_y - x, color);
        put_pixel(center_x + x, center_y - y, color);
        ++y;
        if (error < 0) error += 2 * y + 1;
        else { --x; error += 2 * (y - x) + 1; }
    }
}

static uint8_t glyph_index(char character) {
    if (character >= 'a' && character <= 'z') character = (char)(character - 'a' + 'A');
    if (character >= 'A' && character <= 'Z') return (uint8_t)(character - 'A');
    if (character >= '0' && character <= '9') return (uint8_t)(character - '0' + 26);
    return 0xff;
}

static void draw_char(int x, int y, char character, uint16_t color) {
    uint8_t index = glyph_index(character);
    uint8_t row;
    if (index != 0xff) {
        for (row = 0; row < 5; ++row) {
            uint8_t bits = font[index][row];
            uint8_t column;
            for (column = 0; column < 3; ++column)
                if (bits & (1u << (2 - column))) put_pixel(x + column, y + row, color);
        }
    } else if (character == ':') {
        put_pixel(x + 1, y + 1, color); put_pixel(x + 1, y + 4, color);
    } else if (character == '.') put_pixel(x + 1, y + 4, color);
    else if (character == '-') fill_rect(x, y + 2, 3, 1, color);
    else if (character == '/') { put_pixel(x + 2, y, color); put_pixel(x + 1, y + 2, color); put_pixel(x, y + 4, color); }
    else if (character == '+') { fill_rect(x + 1, y + 1, 1, 3, color); fill_rect(x, y + 2, 3, 1, color); }
    else if (character == '?') { put_pixel(x, y, color); put_pixel(x + 1, y, color); put_pixel(x + 2, y + 1, color); put_pixel(x + 1, y + 2, color); put_pixel(x + 1, y + 4, color); }
}

static void draw_text(int x, int y, const char *text, uint16_t color) {
    while (*text) { draw_char(x, y, *text++, color); x += 4; }
}

static void draw_centered(int y, const char *text, uint16_t color) {
    int length = 0;
    const char *cursor = text;
    while (*cursor++) ++length;
    draw_text((SCREEN_WIDTH - length * 4) / 2, y, text, color);
}

static void draw_number(int x, int y, uint32_t value, uint16_t color) {
    static const uint32_t powers[] = {1000000,100000,10000,1000,100,10,1};
    uint8_t started = 0;
    uint8_t i;
    for (i = 0; i < 7; ++i) {
        uint8_t digit = 0;
        while (value >= powers[i]) { value -= powers[i]; ++digit; }
        if (digit || started || powers[i] == 1) {
            draw_char(x, y, (char)('0' + digit), color);
            x += 4;
            started = 1;
        }
    }
}

static void draw_header(const char *title) {
    fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    fill_rect(0, 0, SCREEN_WIDTH, 18, COLOR_GREEN);
    draw_text(8, 6, title, COLOR_GOLD);
    draw_text(143, 6, "CR", COLOR_MINT);
    draw_number(155, 6, credits, COLOR_WHITE);
    fill_rect(7, 21, 226, 1, COLOR_DIM);
}

static void draw_footer(const char *left, const char *right) {
    fill_rect(0, 146, SCREEN_WIDTH, 14, COLOR_GREEN);
    draw_text(6, 151, left, COLOR_MINT);
    draw_text(137, 151, right, COLOR_GOLD);
}

static void draw_wager(void) {
    draw_text(8, 30, "WAGER", COLOR_MINT);
    draw_number(36, 30, wager, COLOR_GOLD);
    draw_text(63, 30, "L/R CHANGE", COLOR_DIM);
}

static uint32_t rng_next(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint8_t rng_below(uint8_t limit) {
    uint16_t value = (uint16_t)(rng_next() & 0xff);
    while (value >= limit) value -= limit;
    return (uint8_t)value;
}

static uint8_t place_wager(void) {
    if (credits < 5 || wager < 5) {
        status_line[0] = 'N'; status_line[1] = 'E'; status_line[2] = 'E'; status_line[3] = 'D';
        status_line[4] = ' '; status_line[5] = '5'; status_line[6] = ' '; status_line[7] = 'C';
        status_line[8] = 'R'; status_line[9] = 'E'; status_line[10] = 'D'; status_line[11] = 'I';
        status_line[12] = 'T'; status_line[13] = 'S'; status_line[14] = 0;
        return 0;
    }
    if (wager > credits) wager = credits;
    credits -= wager;
    return 1;
}

static void change_wager(uint16_t keys) {
    if ((keys & KEY_L) && wager > 5) wager -= 5;
    if (keys & KEY_R) wager += 5;
    if (wager > credits && credits >= 5) wager = credits - (credits % 5);
    if (wager < 5) wager = 5;
}

static uint8_t is_red(uint8_t number) {
    static const uint8_t reds[] = {1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    uint8_t i;
    for (i = 0; i < 18; ++i) if (reds[i] == number) return 1;
    return 0;
}

static void draw_wheel(void) {
    static const int8_t spoke_x[] = {0,10,18,24,27,24,18,10,0,-10,-18,-24,-27,-24,-18,-10};
    static const int8_t spoke_y[] = {-27,-24,-18,-10,0,10,18,24,27,24,18,10,0,-10,-18,-24};
    uint8_t i;
    draw_circle(120, 89, 40, COLOR_GOLD);
    draw_circle(120, 89, 36, COLOR_WHITE);
    draw_circle(120, 89, 25, COLOR_GOLD);
    for (i = 0; i < 16; ++i) {
        int j;
        int x0 = 120 + spoke_x[i], y0 = 89 + spoke_y[i];
        int x1 = 120 - spoke_x[i], y1 = 89 - spoke_y[i];
        int dx = x1 > x0 ? x1 - x0 : x0 - x1;
        int sx = x0 < x1 ? 1 : -1;
        int dy = y1 > y0 ? y0 - y1 : y1 - y0;
        int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            put_pixel(x0, y0, i & 1 ? COLOR_RED : COLOR_BLACK);
            if (x0 == x1 && y0 == y1) break;
            if (2 * error >= dy) { error += dy; x0 += sx; }
            if (2 * error <= dx) { error += dx; y0 += sy; }
        }
    }
    fill_rect(111, 80, 18, 18, COLOR_GOLD);
    fill_rect(115, 84, 10, 10, COLOR_GREEN);
    if (roulette_result_valid) {
        draw_number(117, 86, roulette_number, COLOR_WHITE);
    } else draw_text(116, 86, "?", COLOR_WHITE);
}

static void draw_roulette(void) {
    draw_header("ROULETTE"); draw_wager(); draw_wheel();
    if (roulette_result_valid) {
        draw_text(72, 128, "RESULT", COLOR_MINT);
        draw_number(100, 128, roulette_number, COLOR_GOLD);
        draw_text(114, 128, roulette_number == 0 ? "GREEN" : is_red(roulette_number) ? "RED" : "BLACK", COLOR_WHITE);
    } else if (roulette_number_bet) {
        draw_text(79, 128, "BET NUMBER", COLOR_MINT);
        draw_number(125, 128, roulette_bet_number, COLOR_GOLD);
    } else draw_centered(128, roulette_bet_black ? "BET: BLACK" : "BET: RED", COLOR_GOLD);
    draw_centered(138, status_line, COLOR_WHITE);
    draw_footer("A SPIN", "B BET TYPE");
}

static uint8_t card_points(Card card) {
    if (card.rank == 1) return 11;
    return card.rank > 10 ? 10 : card.rank;
}

static uint8_t hand_points(const Card *hand, uint8_t count) {
    uint16_t total = 0;
    uint8_t aces = 0, i;
    for (i = 0; i < count; ++i) { total += card_points(hand[i]); if (hand[i].rank == 1) ++aces; }
    while (total > 21 && aces) { total -= 10; --aces; }
    return (uint8_t)total;
}

static Card draw_card(void) {
    Card card;
    card.rank = (uint8_t)(rng_below(13) + 1);
    card.suit = rng_below(4);
    return card;
}

static void draw_card_row(int x, int y, const Card *hand, uint8_t count, uint8_t hide_second) {
    static const char ranks[] = "A23456789TJQK";
    static const char suits[] = "SHDC";
    uint8_t i;
    for (i = 0; i < count; ++i) {
        int card_x = x + i * 25;
        fill_rect(card_x, y, 21, 30, hide_second && i == 1 ? COLOR_GREEN : COLOR_WHITE);
        draw_rect(card_x, y, 21, 30, COLOR_GOLD);
        if (hide_second && i == 1) draw_text(card_x + 8, y + 12, "?", COLOR_GOLD);
        else {
            draw_char(card_x + 3, y + 4, ranks[hand[i].rank - 1], COLOR_BLACK);
            draw_char(card_x + 3, y + 13, suits[hand[i].suit], hand[i].suit == 1 || hand[i].suit == 2 ? COLOR_RED : COLOR_GREEN);
        }
    }
}

static void finish_blackjack(void) {
    uint8_t player = hand_points(player_hand, player_count);
    uint8_t dealer = hand_points(dealer_hand, dealer_count);
    uint8_t natural = player_count == 2 && player == 21;
    uint8_t dealer_natural = dealer_count == 2 && dealer == 21;
    blackjack_done = 1;
    if (player > 21) {
        const char text[] = "BUST - DEALER WINS"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else if (natural && dealer_natural) {
        credits += wager;
        const char text[] = "PUSH - WAGER BACK"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else if (dealer_natural) {
        const char text[] = "DEALER BLACKJACK"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else if (player == dealer) {
        credits += wager;
        const char text[] = "PUSH - WAGER BACK"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else if (natural) {
        credits += wager + wager * 3 / 2;
        const char text[] = "BLACKJACK! +3:2"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else if (dealer > 21 || player > dealer) {
        credits += wager * 2;
        const char text[] = "YOU WIN - +1:1"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    } else {
        const char text[] = "DEALER WINS"; uint8_t i;
        for (i = 0; i < sizeof(text); ++i) status_line[i] = text[i];
    }
}

static void start_blackjack(void) {
    if (!place_wager()) return;
    player_count = 2; dealer_count = 2; blackjack_active = 1; blackjack_done = 0;
    player_hand[0] = draw_card(); player_hand[1] = draw_card();
    dealer_hand[0] = draw_card(); dealer_hand[1] = draw_card();
    status_line[0] = 'H'; status_line[1] = 'I'; status_line[2] = 'T'; status_line[3] = ' '; status_line[4] = 'A';
    status_line[5] = ' '; status_line[6] = ' '; status_line[7] = 'B'; status_line[8] = ' '; status_line[9] = 'S'; status_line[10] = 'T';
    status_line[11] = 'A'; status_line[12] = 'N'; status_line[13] = 'D'; status_line[14] = 0;
    if (hand_points(player_hand, player_count) == 21) finish_blackjack();
}

static void hit_blackjack(void) {
    if (player_count < 12) player_hand[player_count++] = draw_card();
    if (hand_points(player_hand, player_count) >= 21) {
        if (hand_points(player_hand, player_count) == 21)
            while (hand_points(dealer_hand, dealer_count) < 17 && dealer_count < 12)
                dealer_hand[dealer_count++] = draw_card();
        finish_blackjack();
    }
}

static void stand_blackjack(void) {
    while (hand_points(dealer_hand, dealer_count) < 17 && dealer_count < 12)
        dealer_hand[dealer_count++] = draw_card();
    finish_blackjack();
}

static void draw_blackjack(void) {
    uint8_t shown_dealer = blackjack_done ? dealer_count : (dealer_count ? 2 : 0);
    draw_header("BLACKJACK"); draw_wager();
    draw_text(8, 42, "DEALER", COLOR_MINT);
    if (dealer_count) {
        draw_card_row(68, 37, dealer_hand, shown_dealer, !blackjack_done);
        draw_number(204, 48, blackjack_done ? hand_points(dealer_hand, dealer_count) : card_points(dealer_hand[0]), COLOR_GOLD);
        if (!blackjack_done) draw_text(216, 48, "?", COLOR_GOLD);
    }
    draw_text(8, 81, "PLAYER", COLOR_MINT);
    if (player_count) {
        draw_card_row(68, 76, player_hand, player_count, 0);
        draw_number(204, 86, hand_points(player_hand, player_count), COLOR_GOLD);
    } else draw_text(8, 101, "PRESS A TO DEAL", COLOR_WHITE);
    draw_centered(123, status_line, COLOR_WHITE);
    draw_footer(blackjack_active && !blackjack_done ? "A HIT" : "A DEAL", blackjack_active && !blackjack_done ? "B STAND" : "SELECT MENU");
}

static void draw_slots(void) {
    static const char slot_glyphs[] = "7CHLM";
    draw_header("LUCKY SLOTS"); draw_wager();
    fill_rect(37, 54, 166, 55, COLOR_GOLD);
    draw_rect(37, 54, 166, 55, COLOR_WHITE);
    draw_rect(42, 59, 156, 45, COLOR_BLACK);
    fill_rect(47, 64, 43, 35, COLOR_WHITE);
    fill_rect(98, 64, 43, 35, COLOR_WHITE);
    fill_rect(149, 64, 43, 35, COLOR_WHITE);
    draw_char(66, 79, slot_glyphs[slot_symbols[0]], COLOR_RED);
    draw_char(117, 79, slot_glyphs[slot_symbols[1]], COLOR_GREEN);
    draw_char(168, 79, slot_glyphs[slot_symbols[2]], COLOR_RED);
    draw_centered(121, "TRIPLE 7 PAYS 18X", COLOR_GOLD);
    draw_centered(134, status_line, COLOR_WHITE);
    draw_footer("A SPIN", "PAIR 2X / TRIPLE 6X");
}

static void draw_die(int x, int y, uint8_t value) {
    int positions[6][2] = {{3,3},{13,13},{13,3},{3,13},{8,8},{3,8}};
    uint8_t i;
    fill_rect(x, y, 20, 20, COLOR_WHITE); draw_rect(x, y, 20, 20, COLOR_GOLD);
    for (i = 0; i < value && i < 6; ++i)
        fill_rect(x + positions[i][0], y + positions[i][1], 3, 3, COLOR_GREEN);
}

static const uint8_t dice_gross[] = {0,0,36,18,12,9,7,6,7,9,12,18,36};

static void play_dice(void) {
    uint8_t total;
    dice_first = (uint8_t)(rng_below(6) + 1);
    dice_second = (uint8_t)(rng_below(6) + 1);
    total = dice_first + dice_second;
    if (total == dice_target) {
        credits += wager * dice_gross[total];
        status_line[0] = 'H'; status_line[1] = 'I'; status_line[2] = 'T'; status_line[3] = '!'; status_line[4] = 0;
    } else {
        status_line[0] = 'N'; status_line[1] = 'O'; status_line[2] = ' '; status_line[3] = 'H'; status_line[4] = 'I';
        status_line[5] = 'T'; status_line[6] = 0;
    }
    draw_number(111, 97, total, COLOR_GOLD);
}

static void draw_dice(void) {
    draw_header("LUCKY DICE"); draw_wager();
    draw_die(85, 65, dice_first); draw_die(135, 65, dice_second);
    draw_centered(104, "TARGET", COLOR_MINT);
    draw_number(118, 114, dice_target, COLOR_GOLD);
    draw_centered(133, status_line, COLOR_WHITE);
    draw_footer("A ROLL", "LEFT/RIGHT TARGET");
}

static const char *game_titles[] = {"ROULETTE", "BLACKJACK", "SLOTS", "DICE"};
static const char *game_subtitles[] = {"EUROPEAN 0-36", "BEAT THE DEALER", "MATCH SYMBOLS", "PICK A TOTAL"};

static void draw_menu(void) {
    uint8_t i;
    draw_header("LUCKY CIRCUIT");
    draw_centered(25, "POCKET ARCADE", COLOR_MINT);
    for (i = 0; i < 4; ++i) {
        int x = 16 + (i % 2) * 108;
        int y = 48 + (i / 2) * 40;
        uint16_t fill = i == menu_choice ? COLOR_GOLD : COLOR_PANEL;
        uint16_t text = i == menu_choice ? COLOR_BLACK : COLOR_WHITE;
        fill_rect(x, y, 98, 33, fill); draw_rect(x, y, 98, 33, COLOR_MINT);
        draw_text(x + 6, y + 6, game_titles[i], text);
        draw_text(x + 6, y + 19, game_subtitles[i], i == menu_choice ? COLOR_GREEN : COLOR_MINT);
    }
    draw_centered(133, "A CHOOSE  UP/DOWN/LEFT/RIGHT", COLOR_GOLD);
    draw_footer("PLAY CREDITS ONLY", "NO CASH VALUE");
}

static void draw_game(void) {
    if (screen == 0) draw_menu();
    else if (screen == 1) draw_roulette();
    else if (screen == 2) draw_blackjack();
    else if (screen == 3) draw_slots();
    else draw_dice();
}

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) { }
    while (REG_VCOUNT < 160) { }
    ++frame_count;
}

static void spin_roulette(void) {
    uint8_t result, color_win;
    if (!place_wager()) return;
    result = rng_below(37);
    roulette_number = result;
    roulette_result_valid = 1;
    color_win = result != 0 && is_red(result) != roulette_bet_black;
    if ((roulette_number_bet && result == roulette_bet_number) || (!roulette_number_bet && color_win)) {
        credits += wager * (roulette_number_bet ? 36 : 2);
        status_line[0] = 'W'; status_line[1] = 'I'; status_line[2] = 'N'; status_line[3] = '!'; status_line[4] = 0;
    } else {
        status_line[0] = 'N'; status_line[1] = 'O'; status_line[2] = ' '; status_line[3] = 'W'; status_line[4] = 'I';
        status_line[5] = 'N'; status_line[6] = 0;
    }
}

static void spin_slots(void) {
    uint8_t a, b, c, payout = 0;
    if (!place_wager()) return;
    a = rng_below(5); b = rng_below(5); c = rng_below(5);
    slot_symbols[0] = a; slot_symbols[1] = b; slot_symbols[2] = c;
    if (a == b && b == c) payout = a == 0 ? 18 : 6;
    else if (a == b || b == c || a == c) payout = 2;
    if (payout) {
        credits += wager * payout;
        status_line[0] = 'W'; status_line[1] = 'I'; status_line[2] = 'N'; status_line[3] = '!'; status_line[4] = 0;
    } else {
        status_line[0] = 'N'; status_line[1] = 'O'; status_line[2] = ' '; status_line[3] = 'M'; status_line[4] = 'A';
        status_line[5] = 'T'; status_line[6] = 'C'; status_line[7] = 'H'; status_line[8] = 0;
    }
}

static void select_game(void) {
    screen = (uint8_t)(menu_choice + 1);
    blackjack_active = 0; blackjack_done = 0; player_count = 0; dealer_count = 0;
    roulette_number_bet = 0; roulette_bet_black = 0; roulette_result_valid = 0;
    roulette_number = 0; roulette_bet_number = 17;
    status_line[0] = 'P'; status_line[1] = 'R'; status_line[2] = 'E'; status_line[3] = 'S'; status_line[4] = 'S';
    status_line[5] = ' '; status_line[6] = 'A'; status_line[7] = ' '; status_line[8] = 'T'; status_line[9] = 'O';
    status_line[10] = ' '; status_line[11] = 'P'; status_line[12] = 'L'; status_line[13] = 'A'; status_line[14] = 'Y'; status_line[15] = 0;
}

static void handle_menu(uint16_t keys) {
    if ((keys & KEY_UP) && menu_choice >= 2) menu_choice -= 2;
    if ((keys & KEY_DOWN) && menu_choice < 2) menu_choice += 2;
    if ((keys & KEY_LEFT) && (menu_choice & 1)) --menu_choice;
    if ((keys & KEY_RIGHT) && !(menu_choice & 1)) ++menu_choice;
    if (keys & KEY_A) select_game();
}

static void handle_roulette(uint16_t keys) {
    if (keys & KEY_B) roulette_number_bet = !roulette_number_bet;
    if (keys & KEY_LEFT) roulette_bet_black = 0;
    if (keys & KEY_RIGHT) roulette_bet_black = 1;
    if (keys & KEY_UP) roulette_bet_number = (uint8_t)((roulette_bet_number + 1) % 37);
    if (keys & KEY_DOWN) roulette_bet_number = (uint8_t)((roulette_bet_number + 36) % 37);
    if (keys & KEY_A) spin_roulette();
}

static void handle_blackjack(uint16_t keys) {
    if (keys & KEY_A) {
        if (!blackjack_active || blackjack_done) start_blackjack();
        else hit_blackjack();
    }
    if ((keys & KEY_B) && blackjack_active && !blackjack_done) stand_blackjack();
}

static void handle_dice(uint16_t keys) {
    if (keys & KEY_LEFT) { if (dice_target > 2) --dice_target; }
    if (keys & KEY_RIGHT) { if (dice_target < 12) ++dice_target; }
    if ((keys & KEY_A) && place_wager()) play_dice();
}

int main(void) {
    static const char welcome[] = "PRESS A TO PLAY";
    uint8_t i;
    credits = 1000;
    wager = 25;
    random_state = 0x4c55434b;
    roulette_bet_number = 17;
    dice_target = 7;
    dice_first = 5;
    dice_second = 2;
    slot_symbols[0] = 0; slot_symbols[1] = 1; slot_symbols[2] = 2;
    for (i = 0; welcome[i]; ++i) status_line[i] = welcome[i];
    status_line[i] = 0;
    REG_DISPCNT = 0x0403;
    random_state ^= (uint32_t)(uintptr_t)&REG_VCOUNT;
    draw_game();
    for (;;) {
        uint16_t keys;
        uint16_t pressed;
        wait_vblank();
        keys = (uint16_t)(~REG_KEYINPUT) & 0x03ff;
        pressed = keys & (uint16_t)~previous_keys;
        previous_keys = keys;
        if (pressed & KEY_SELECT) screen = 0;
        if (screen == 0) handle_menu(pressed);
        else {
            if (pressed & (KEY_L | KEY_R)) change_wager(pressed);
            if (screen == 1) handle_roulette(pressed);
            else if (screen == 2) handle_blackjack(pressed);
            else if (screen == 3 && pressed & KEY_A) spin_slots();
            else if (screen == 4) handle_dice(pressed);
        }
        if (pressed) draw_game();
    }
    return 0;
}