#include <napp.h>

NAPP_APPLICATION("NAI", "Offline NasuaOS assistant", true, {}, false, "System");

static constexpr uint32_t MAX_INPUT_LENGTH = 160;
static constexpr uint32_t MAX_TURNS = 10;
static constexpr uint32_t MAX_ANSWER_LENGTH = 384;
static constexpr uint32_t CHAR_WIDTH = 9;
static constexpr uint32_t LINE_HEIGHT = 15;
static constexpr uint32_t COLOR_BACKGROUND = 0xFF11151D;
static constexpr uint32_t COLOR_PANEL = 0xFF1A1F2C;
static constexpr uint32_t COLOR_TEXT = 0xFFE1E7EF;
static constexpr uint32_t COLOR_MUTED = 0xFF9AA4B2;
static constexpr uint32_t COLOR_ACCENT = 0xFF59E81B;
static constexpr uint32_t COLOR_USER = 0xFF70C8FF;

struct chat_turn
{
    char question[MAX_INPUT_LENGTH];
    char answer[MAX_ANSWER_LENGTH];
};

struct nai_state
{
    const napp_api* api;
    char input[MAX_INPUT_LENGTH];
    uint32_t input_length;
    chat_turn turns[MAX_TURNS];
    uint32_t turn_count;
};

static nai_state state{};

static uint32_t text_length(const char* text)
{
    uint32_t length = 0;
    while (text[length] != '\0')
    {
        ++length;
    }
    return length;
}

static char lower_ascii(char character)
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character - 'A' + 'a')
        : character;
}

static bool contains(const char* text, const char* phrase)
{
    if (text == nullptr || phrase == nullptr || phrase[0] == '\0')
    {
        return false;
    }

    for (uint32_t start = 0; text[start] != '\0'; ++start)
    {
        uint32_t offset = 0;
        while (phrase[offset] != '\0' &&
               text[start + offset] != '\0' &&
               lower_ascii(text[start + offset]) == lower_ascii(phrase[offset]))
        {
            ++offset;
        }
        if (phrase[offset] == '\0')
        {
            return true;
        }
    }
    return false;
}

static void copy_text(char* destination, uint32_t capacity, const char* source)
{
    if (capacity == 0)
    {
        return;
    }
    uint32_t index = 0;
    while (index + 1 < capacity && source[index] != '\0')
    {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void append_text(char* destination, uint32_t capacity, const char* source)
{
    const uint32_t start = text_length(destination);
    uint32_t index = 0;
    while (start + index + 1 < capacity && source[index] != '\0')
    {
        destination[start + index] = source[index];
        ++index;
    }
    destination[start + index] = '\0';
}

static void append_number(char* destination, uint32_t capacity, uint64_t value)
{
    char digits[24];
    uint32_t count = 0;
    do
    {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (value != 0 && count < sizeof(digits));

    while (count > 0)
    {
        const char digit[2] = {digits[--count], '\0'};
        append_text(destination, capacity, digit);
    }
}

static const char* answer_question(const char* question)
{
    static char answer[MAX_ANSWER_LENGTH];
    answer[0] = '\0';

    if (question[0] == '\0')
    {
        return "Wpisz pytanie, a potem nacisnij Enter.";
    }
    if (contains(question, "czesc") || contains(question, "witaj") ||
        contains(question, "hello") || contains(question, "hej"))
    {
        return "Czesc! Jestem NAI, offline'owym asystentem NasuaOS. Zapytaj mnie o system lub wpisz \"pomoc\".";
    }
    if (contains(question, "pomoc") || contains(question, "co potrafisz") ||
        contains(question, "jakie pytania"))
    {
        return "Moge opowiedziec o NasuaOS, aplikacjach, terminalu i budowaniu systemu. Odpowiadam tez na kilka podstawowych pytan ogolnych. Nie korzystam z internetu ani z pobranego modelu.";
    }
    if (contains(question, "kim jestes") || contains(question, "twoje imie") ||
        contains(question, "kto cie stworzyl"))
    {
        return "Jestem NAI (NasuaAI) - malym, lokalnym asystentem napisanym specjalnie dla NasuaOS. Moje odpowiedzi pochodza z regulek i wbudowanej wiedzy, a nie z wytrenowanego modelu jezykowego.";
    }
    if (contains(question, "pamiec") || contains(question, "ram"))
    {
        if (state.api != nullptr && state.api->abi_version >= 10 &&
            state.api->get_system_info != nullptr)
        {
            napp_system_info info{};
            if (state.api->get_system_info(&info))
            {
                append_text(answer, sizeof(answer), "Pamiec wykryta przez system: ");
                append_number(answer, sizeof(answer), info.memory_total_bytes / (1024 * 1024));
                append_text(answer, sizeof(answer), " MB.");
                return answer;
            }
        }
        return "Nie moge odczytac informacji o pamieci w tej wersji API NAPP.";
    }
    if (contains(question, "rozdzielczosc") || contains(question, "ekran"))
    {
        if (state.api != nullptr && state.api->abi_version >= 10 &&
            state.api->get_system_info != nullptr)
        {
            napp_system_info info{};
            if (state.api->get_system_info(&info))
            {
                append_text(answer, sizeof(answer), "Rozdzielczosc ekranu: ");
                append_number(answer, sizeof(answer), info.framebuffer_width);
                append_text(answer, sizeof(answer), " x ");
                append_number(answer, sizeof(answer), info.framebuffer_height);
                append_text(answer, sizeof(answer), ".");
                return answer;
            }
        }
        return "Nie moge odczytac rozdzielczosci w tej wersji API NAPP.";
    }
    if (contains(question, "architektura") || contains(question, "procesor"))
    {
        if (state.api != nullptr && state.api->abi_version >= 10 &&
            state.api->get_system_info != nullptr)
        {
            napp_system_info info{};
            if (state.api->get_system_info(&info))
            {
                append_text(answer, sizeof(answer), "Architektura systemu: ");
                append_text(answer, sizeof(answer), info.architecture);
                append_text(answer, sizeof(answer), ".");
                return answer;
            }
        }
        return "NasuaOS jest rozwijany m.in. dla x86_64. Biezacej architektury nie moge teraz odczytac.";
    }
    if (contains(question, "zbudow") || contains(question, "kompil") ||
        contains(question, "make all"))
    {
        return "Aby zbudowac NasuaOS w katalogu projektu, uruchom: make all. Do kompilacji potrzebne sa m.in. GCC/G++, Clang, LLD, NASM, Python 3, xorriso, mtools i unzip. Nie pobieram za Ciebie zadnych plikow.";
    }
    if (contains(question, "nasuaos") || contains(question, "jaki system") ||
        contains(question, "system operacyjny") || contains(question, "kernel"))
    {
        return "NasuaOS to system operacyjny rozwijany w tym projekcie. Zawiera wlasne jadro, srodowisko graficzne, terminal oraz aplikacje NAPP uruchamiane przez system.";
    }
    if (contains(question, "terminal") || contains(question, "komendy"))
    {
        return "Terminal NasuaOS pozwala uruchamiac polecenia systemowe. Wpisz \"help\" w terminalu, aby zobaczyc dostepne polecenia. NAI sam nie wykonuje wpisanych polecen.";
    }
    if (contains(question, "aplikacj"))
    {
        return "NasuaOS uruchamia aplikacje NAPP. W systemie znajdziesz m.in. terminal, ustawienia, kalkulator, edytor tekstu i menedzer zadan.";
    }
    if (contains(question, "doom") || contains(question, "freedoom"))
    {
        return "Aplikacja Doom korzysta z dolaczonego Freedoom Phase 1 (freedoom1.wad, licencja BSD-3-Clause). Dane gry sa juz w systemie i nie trzeba ich pobierac.";
    }
    if (contains(question, "sztuczna inteligencja") ||
        contains(question, "model jezykowy") || contains(question, "co to ai") ||
        contains(question, "jestes ai"))
    {
        return "NAI jest prostym asystentem regulowym: rozpoznaje wybrane slowa i udziela przygotowanych odpowiedzi. Nie jest duzym modelem jezykowym, nie uczy sie i nie zna faktow spoza wbudowanej wiedzy.";
    }
    if (contains(question, "komputer") || contains(question, "cpu"))
    {
        return "Komputer wykonuje programy przy pomocy procesora (CPU), pamieci operacyjnej (RAM) oraz urzadzen wejscia i wyjscia. System operacyjny zarzadza sprzetem i udostepnia uslugi aplikacjom.";
    }
    if (contains(question, "programow") || contains(question, "kod") ||
        contains(question, "c++"))
    {
        return "Program to zestaw instrukcji wykonywanych przez komputer. Jezyk C++ pozwala tworzyc m.in. aplikacje i systemy operacyjne; kod zrodlowy jest kompilowany do postaci wykonywalnej.";
    }
    if (contains(question, "co to jest internet") || contains(question, "co to internet"))
    {
        return "Internet to globalna siec polaczonych sieci komputerowych. Urzadzenia wymieniaja dane m.in. przy uzyciu protokolow IP, TCP i HTTP.";
    }
    if (contains(question, "slonce"))
    {
        return "Slonce jest gwiazda w centrum ukladu Slonecznego. Ziemia i pozostale planety kraza wokol niego.";
    }
    if (contains(question, "dziekuje") || contains(question, "dzieki"))
    {
        return "Nie ma za co! Jesli chcesz, zadaj kolejne pytanie.";
    }
    if (contains(question, "pogoda") || contains(question, "najnowsze") ||
        contains(question, "dzisiaj") || contains(question, "aktualn"))
    {
        return "Nie mam dostepu do internetu ani aktualnych danych. Dzialam lokalnie i odpowiadam tylko na podstawie wbudowanych regulek.";
    }

    return "Nie znam jeszcze odpowiedzi na to pytanie. Sprobuj zapytac o NasuaOS, terminal, budowanie systemu, pamiec, ekran lub wpisz \"pomoc\".";
}

static uint32_t wrapped_line_count(const char* text, uint32_t chars_per_line)
{
    const uint32_t length = text_length(text);
    return length == 0 ? 1 : (length + chars_per_line - 1) / chars_per_line;
}

static void draw_wrapped(const char* text, int x, int& y, int bottom, uint32_t color,
                         uint32_t chars_per_line, uint32_t skip_lines)
{
    uint32_t offset = 0;
    uint32_t line = 0;
    const uint32_t length = text_length(text);
    do
    {
        if (line >= skip_lines && y + static_cast<int>(LINE_HEIGHT) <= bottom)
        {
            char clipped[96];
            uint32_t count = 0;
            while (count < chars_per_line && offset + count < length &&
                   count + 1 < sizeof(clipped))
            {
                clipped[count] = text[offset + count];
                ++count;
            }
            clipped[count] = '\0';
            state.api->gui->draw_text(clipped, x, y, color);
            y += static_cast<int>(LINE_HEIGHT);
        }
        offset += chars_per_line;
        ++line;
    }
    while (offset < length);
}

static void draw_nai(napp_window* window)
{
    const int left = window->pos_x + 16;
    const int body_top = window->pos_y + window->title_height;
    const int body_bottom = body_top + window->height;
    const int input_y = body_bottom - 48;
    const int chat_top = body_top + 42;
    const int chat_bottom = input_y - 10;
    const int available_width = window->width - 38;
    const uint32_t chars_per_line = available_width > static_cast<int>(CHAR_WIDTH)
        ? static_cast<uint32_t>(available_width / CHAR_WIDTH)
        : 1;
    const uint32_t visible_lines = chat_bottom > chat_top
        ? static_cast<uint32_t>((chat_bottom - chat_top) / LINE_HEIGHT)
        : 0;

    state.api->gui->fill_block(window->pos_x, body_top, COLOR_BACKGROUND, window->width, window->height);
    state.api->gui->draw_text("NasuaAI - offline assistant", left, body_top + 12, COLOR_ACCENT);
    state.api->gui->draw_text("Wpisz pytanie i nacisnij Enter. Wpisz \"pomoc\", aby zobaczyc przyklady.",
                              left, body_top + 27, COLOR_MUTED);

    if (state.turn_count == 0)
    {
        state.api->gui->draw_text("Czesc! Jestem NAI. Zapytaj mnie o NasuaOS lub podstawy komputerow.",
                                  left, chat_top, COLOR_TEXT);
    }
    else
    {
        uint32_t total_lines = 0;
        for (uint32_t index = 0; index < state.turn_count; ++index)
        {
            total_lines += wrapped_line_count(state.turns[index].question, chars_per_line);
            total_lines += wrapped_line_count(state.turns[index].answer, chars_per_line);
        }
        uint32_t skip_lines = total_lines > visible_lines ? total_lines - visible_lines : 0;
        int y = chat_top;
        for (uint32_t index = 0; index < state.turn_count; ++index)
        {
            const uint32_t question_lines = wrapped_line_count(state.turns[index].question, chars_per_line);
            if (skip_lines >= question_lines)
            {
                skip_lines -= question_lines;
            }
            else
            {
                draw_wrapped(state.turns[index].question, left, y, chat_bottom, COLOR_USER,
                             chars_per_line, skip_lines);
                skip_lines = 0;
            }

            const uint32_t answer_lines = wrapped_line_count(state.turns[index].answer, chars_per_line);
            if (skip_lines >= answer_lines)
            {
                skip_lines -= answer_lines;
            }
            else
            {
                draw_wrapped(state.turns[index].answer, left, y, chat_bottom, COLOR_TEXT,
                             chars_per_line, skip_lines);
                skip_lines = 0;
            }
        }
    }

    state.api->gui->fill_block(left, input_y, COLOR_PANEL, window->width - 32, 34);
    char visible_input[68];
    copy_text(visible_input, sizeof(visible_input), state.input[0] == '\0' ? "Wpisz pytanie..." : state.input);
    state.api->gui->draw_text(visible_input, left + 8, input_y + 12,
                              state.input[0] == '\0' ? COLOR_MUTED : COLOR_TEXT);
    state.api->gui->fill_block(window->pos_x + window->width - 82, input_y + 3, COLOR_ACCENT, 62, 28);
    state.api->gui->draw_text("Wyslij", window->pos_x + window->width - 72, input_y + 13, COLOR_BACKGROUND);
}

static void submit_question()
{
    if (state.turn_count == MAX_TURNS)
    {
        for (uint32_t index = 1; index < MAX_TURNS; ++index)
        {
            state.turns[index - 1] = state.turns[index];
        }
        --state.turn_count;
    }

    chat_turn& turn = state.turns[state.turn_count++];
    copy_text(turn.question, sizeof(turn.question), "Ty: ");
    append_text(turn.question, sizeof(turn.question), state.input);
    copy_text(turn.answer, sizeof(turn.answer), "NAI: ");
    append_text(turn.answer, sizeof(turn.answer), answer_question(state.input));
    state.input[0] = '\0';
    state.input_length = 0;
}

static void nai_key(napp_window*, char key)
{
    if (key == '\n')
    {
        submit_question();
        return;
    }
    if (key == '\b')
    {
        if (state.input_length > 0)
        {
            state.input[--state.input_length] = '\0';
        }
        return;
    }
    if (key == NAPP_KEY_ESCAPE)
    {
        state.input[0] = '\0';
        state.input_length = 0;
        return;
    }
    if (key >= 32 && key <= 126 && state.input_length + 1 < sizeof(state.input))
    {
        state.input[state.input_length++] = key;
        state.input[state.input_length] = '\0';
    }
}

static void nai_mouse(napp_window* window, int mouse_x, int mouse_y)
{
    const int button_x = window->pos_x + window->width - 82;
    const int button_y = window->pos_y + window->title_height + window->height - 45;
    if (mouse_x >= button_x && mouse_x < button_x + 62 &&
        mouse_y >= button_y && mouse_y < button_y + 34)
    {
        submit_question();
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->gui == nullptr ||
        api->gui->open_window == nullptr || api->gui->fill_block == nullptr ||
        api->gui->draw_text == nullptr)
    {
        return 1;
    }

    state = {};
    state.api = api;
    const napp_window_config config =
    {
        "NasuaAI", 720, 500, true, true, nullptr,
        draw_nai, nai_key, nai_mouse, nullptr, nullptr, 0, nullptr, nullptr
    };
    return api->gui->open_window(&config) ? 0 : 1;
}
