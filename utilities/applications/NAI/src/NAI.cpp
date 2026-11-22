#include <napp.h>

NAPP_APPLICATION("NAI", "Offline NasuaOS assistant", true, {}, false, "System");

namespace
{
    constexpr char MODEL_PATH[] =
        "/share/models/nai/SmolLM2-135M-Instruct-IQ4_XS.gguf";
    constexpr uint32_t MODEL_SIZE = 90897760;
    constexpr uint32_t COLOR_BACKGROUND = 0xFF11151D;
    constexpr uint32_t COLOR_TEXT = 0xFFE1E7EF;
    constexpr uint32_t COLOR_MUTED = 0xFF9AA4B2;
    constexpr uint32_t COLOR_ACCENT = 0xFF59E81B;
    constexpr uint32_t COLOR_ERROR = 0xFFFF7777;
    constexpr uint32_t COLOR_PANEL = 0xFF1A202A;
    constexpr uint32_t COLOR_CURSOR = 0xFF70C8FF;
    constexpr uint32_t MAX_INPUT_LENGTH = 256;
    constexpr uint32_t MAX_MESSAGES = 8;
    constexpr uint32_t MAX_MESSAGE_LENGTH = 384;
    constexpr uint32_t NAI_OUTPUT_LENGTH = 512;

    enum class model_status
    {
        api_unavailable,
        not_installed,
        unreadable,
        invalid_size,
        invalid_format,
        installed
    };

    struct nai_state
    {
        const napp_api* api;
        model_status status;
        char input[MAX_INPUT_LENGTH];
        uint32_t input_length;
        uint32_t input_cursor;
        char messages[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
        uint32_t message_count;
        uint32_t message_head;
        bool shift_pressed;
        bool generating;
        uint32_t job_id;
        uint32_t thinking_slot;
        char output[NAI_OUTPUT_LENGTH];
    };

    nai_state state{};

    bool read_utf8_codepoint(const char* text, uint32_t* position,
                             uint32_t* codepoint)
    {
        const uint8_t first = static_cast<uint8_t>(text[*position]);
        if (first < 0x80)
        {
            *codepoint = first;
            ++*position;
            return true;
        }

        uint32_t continuation_count;
        uint32_t value;
        if ((first & 0xE0) == 0xC0)
        {
            continuation_count = 1;
            value = first & 0x1F;
        }
        else if ((first & 0xF0) == 0xE0)
        {
            continuation_count = 2;
            value = first & 0x0F;
        }
        else if ((first & 0xF8) == 0xF0)
        {
            continuation_count = 3;
            value = first & 0x07;
        }
        else
        {
            ++*position;
            return false;
        }

        for (uint32_t index = 1; index <= continuation_count; ++index)
        {
            const uint8_t next =
                static_cast<uint8_t>(text[*position + index]);
            if (next == '\0' || (next & 0xC0) != 0x80)
            {
                ++*position;
                return false;
            }
            value = (value << 6) | (next & 0x3F);
        }
        *position += continuation_count + 1;
        *codepoint = value;
        return true;
    }

    uint32_t display_ascii(uint32_t codepoint, char* output)
    {
        switch (codepoint)
        {
            case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
            case 0x00C4: case 0x00C5: case 0x00E0: case 0x00E1:
            case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
            case 0x0104: case 0x0105:
                output[0] = ((codepoint >= 0x00E0 && codepoint <= 0x00E5) ||
                             codepoint == 0x0105)
                                ? 'a' : 'A';
                return 1;
            case 0x00C7: case 0x00E7: case 0x0106: case 0x0107:
                output[0] = (codepoint == 0x00E7 || codepoint == 0x0107)
                                ? 'c' : 'C';
                return 1;
            case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
            case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
            case 0x0118: case 0x0119:
                output[0] = ((codepoint >= 0x00E8 && codepoint <= 0x00EB) ||
                             codepoint == 0x0119)
                                ? 'e' : 'E';
                return 1;
            case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
            case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
                output[0] = codepoint >= 0x00E0 ? 'i' : 'I';
                return 1;
            case 0x00D1: case 0x00F1: case 0x0143: case 0x0144:
                output[0] = (codepoint == 0x00F1 || codepoint == 0x0144)
                                ? 'n' : 'N';
                return 1;
            case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
            case 0x00D6: case 0x00D8: case 0x00F2: case 0x00F3:
            case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
                output[0] = codepoint >= 0x00F0 ? 'o' : 'O';
                return 1;
            case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
            case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
                output[0] = codepoint >= 0x00F0 ? 'u' : 'U';
                return 1;
            case 0x00DD: case 0x00FD: case 0x00FF:
                output[0] = (codepoint == 0x00DD) ? 'Y' : 'y';
                return 1;
            case 0x00DF:
                output[0] = 's';
                output[1] = 's';
                return 2;
            case 0x0141: case 0x0142:
                output[0] = codepoint == 0x0141 ? 'L' : 'l';
                return 1;
            case 0x015A: case 0x015B:
                output[0] = codepoint == 0x015A ? 'S' : 's';
                return 1;
            case 0x0179: case 0x017A: case 0x017B: case 0x017C:
                output[0] = (codepoint == 0x0179 || codepoint == 0x017B)
                                ? 'Z' : 'z';
                return 1;
            default:
                if (codepoint >= 0x0300 && codepoint <= 0x036F)
                {
                    return 0;
                }
                output[0] = '?';
                return 1;
        }
    }

    model_status inspect_model(const napp_api* api)
    {
        if (api == nullptr || api->abi_version < 7 ||
            api->clawfs_get_file_size == nullptr ||
            api->clawfs_read_file_range == nullptr)
        {
            return model_status::api_unavailable;
        }

        uint32_t size = 0;
        if (!api->clawfs_get_file_size(MODEL_PATH, &size))
        {
            return model_status::not_installed;
        }
        if (size != MODEL_SIZE)
        {
            return model_status::invalid_size;
        }

        char magic[4];
        if (!api->clawfs_read_file_range(MODEL_PATH, 0, magic, sizeof(magic)))
        {
            return model_status::unreadable;
        }
        if (magic[0] != 'G' || magic[1] != 'G' ||
            magic[2] != 'U' || magic[3] != 'F')
        {
            return model_status::invalid_format;
        }

        return model_status::installed;
    }

    void fill_message(uint32_t slot, const char* text)
    {
        if (text == nullptr || slot >= MAX_MESSAGES)
        {
            return;
        }

        uint32_t index = 0;
        uint32_t source = 0;
        while (index + 1 < MAX_MESSAGE_LENGTH && text[source] != '\0')
        {
            if (static_cast<uint8_t>(text[source]) < 0x80)
            {
                state.messages[slot][index++] = text[source++];
                continue;
            }

            uint32_t codepoint = 0;
            if (!read_utf8_codepoint(text, &source, &codepoint))
            {
                state.messages[slot][index++] = '?';
                continue;
            }

            char replacement[2];
            const uint32_t replacement_length =
                display_ascii(codepoint, replacement);
            for (uint32_t character = 0;
                 character < replacement_length &&
                 index + 1 < MAX_MESSAGE_LENGTH;
                 ++character)
            {
                state.messages[slot][index++] =
                    replacement[character];
            }
        }
        state.messages[slot][index] = '\0';
    }

    void add_message(const char* text)
    {
        if (text == nullptr)
        {
            return;
        }

        fill_message(state.message_head, text);
        state.message_head = (state.message_head + 1) % MAX_MESSAGES;
        if (state.message_count < MAX_MESSAGES)
        {
            ++state.message_count;
        }
    }

    bool message_is_thinking(uint32_t slot)
    {
        static const char thinking[] = "AI is thinking";
        if (slot >= MAX_MESSAGES)
        {
            return false;
        }
        for (uint32_t index = 0; thinking[index] != '\0'; ++index)
        {
            if (state.messages[slot][index] != thinking[index])
            {
                return false;
            }
        }
        return true;
    }

    void finish_thinking_message(const char* text)
    {
        if (message_is_thinking(state.thinking_slot))
        {
            fill_message(state.thinking_slot, text);
        }
        else
        {
            add_message(text);
        }
    }

    void nai_tick(napp_window*)
    {
        if (!state.generating || state.api == nullptr ||
            state.api->nai_generate_poll == nullptr)
        {
            return;
        }

        const int32_t status = state.api->nai_generate_poll(
            state.job_id, state.output, sizeof(state.output));
        if (status == 0)
        {
            return;
        }
        state.generating = false;

        if (status > 0)
        {
            char reply[MAX_MESSAGE_LENGTH];
            uint32_t length = 0;
            static const char prefix[] = "AI: ";
            for (uint32_t index = 0;
                 prefix[index] != '\0' && length + 1 < sizeof(reply);
                 ++index)
            {
                reply[length++] = prefix[index];
            }
            for (uint32_t index = 0;
                 state.output[index] != '\0' &&
                 length + 1 < sizeof(reply);
                 ++index)
            {
                reply[length++] = state.output[index];
            }
            reply[length] = '\0';
            finish_thinking_message(reply);
        }
        else
        {
            finish_thinking_message("AI: Generation failed.");
        }
    }

    void replace_input(const char* text)
    {
        if (text == nullptr)
        {
            return;
        }

        uint32_t index = 0;
        while (index + 1 < MAX_INPUT_LENGTH && text[index] != '\0')
        {
            state.input[index] = text[index];
            ++index;
        }
        state.input[index] = '\0';
        state.input_length = index;
        state.input_cursor = index;
    }

    bool next_wrapped_line(const char* text, uint32_t* position,
                           char* line, uint32_t max_chars)
    {
        if (text[*position] == '\0')
        {
            return false;
        }
        if (text[*position] == '\n')
        {
            line[0] = '\0';
            ++*position;
            return true;
        }

        const uint32_t start = *position;
        uint32_t length = 0;
        uint32_t last_space = UINT32_MAX;
        while (text[*position] != '\0' &&
               text[*position] != '\n' &&
               length < max_chars)
        {
            line[length] = text[*position];
            if (text[*position] == ' ')
            {
                last_space = length;
            }
            ++length;
            ++*position;
        }

        if (text[*position] == '\n')
        {
            ++*position;
        }
        else if (text[*position] != '\0' && last_space != UINT32_MAX)
        {
            length = last_space;
            *position = start + last_space + 1;
            while (text[*position] == ' ')
            {
                ++*position;
            }
        }

        while (length > 0 && line[length - 1] == ' ')
        {
            --length;
        }
        line[length] = '\0';
        return true;
    }

    uint32_t wrapped_line_count(const char* text, uint32_t max_chars)
    {
        uint32_t position = 0;
        uint32_t lines = 0;
        char line[MAX_MESSAGE_LENGTH];
        while (next_wrapped_line(text, &position, line, max_chars))
        {
            ++lines;
        }
        return lines;
    }

    void draw_nai(napp_window* window)
    {
        const int x = window->pos_x + 18;
        const int y = window->pos_y + window->title_height;
        const int content_height = window->height - window->title_height;
        if (content_height <= 0)
        {
            return;
        }
        state.api->gui->fill_block(window->pos_x, y, COLOR_BACKGROUND,
                                   window->width, content_height);
        state.api->gui->draw_text("NasuaAI - local model status", x, y + 18,
                                  COLOR_ACCENT);

        switch (state.status)
        {
            case model_status::api_unavailable:
                state.api->gui->draw_text("This NAPP API cannot read model files.",
                                          x, y + 58, COLOR_ERROR);
                break;
            case model_status::not_installed:
                state.api->gui->draw_text("The SmolLM2 model is missing or the ISO drive is unavailable.",
                                          x, y + 58, COLOR_ERROR);
                state.api->gui->draw_text("Rebuild the ISO with the model and boot from that ISO.",
                                          x, y + 82, COLOR_TEXT);
                break;
            case model_status::unreadable:
                state.api->gui->draw_text("The model exists but cannot be read.",
                                          x, y + 58, COLOR_ERROR);
                break;
            case model_status::invalid_size:
                state.api->gui->draw_text("The model size does not match the pinned artifact.",
                                          x, y + 58, COLOR_ERROR);
                break;
            case model_status::invalid_format:
                state.api->gui->draw_text("The file is not a GGUF model.",
                                          x, y + 58, COLOR_ERROR);
                break;
            case model_status::installed:
                state.api->gui->draw_text(
                    "SmolLM2 GGUF loaded; native local generation is enabled.",
                    x, y + 58, COLOR_TEXT);
                break;
        }

        state.api->gui->draw_text(
            "Native transformer inference runs inside NasuaOS.",
            x, y + 110, COLOR_TEXT);
        state.api->gui->draw_text(
            "NAI does not send prompts over a network or show canned answers.",
            x, y + 134, COLOR_MUTED);

        const int input_height = 42;
        const int input_y = y + content_height - input_height - 12;
        state.api->gui->fill_block(x - 4, input_y - 4, COLOR_PANEL,
                                   window->width - 36, input_height);
        state.api->gui->draw_text("Prompt:", x, input_y + 12, COLOR_ACCENT);

        char input_view[MAX_INPUT_LENGTH];
        uint32_t index = 0;
        while (state.input[index] != '\0' &&
               index + 1 < sizeof(input_view))
        {
            input_view[index] = state.input[index];
            ++index;
        }
        input_view[index] = '\0';
        state.api->gui->draw_text(index == 0 ? "Type a prompt..." : input_view,
                                  x + 72, input_y + 12,
                                  index == 0 ? COLOR_MUTED : COLOR_TEXT);

        if (state.api->get_ticks == nullptr ||
            ((state.api->get_ticks() / 50) % 2) == 0)
        {
            const int cursor_x = x + 72 + static_cast<int>(state.input_cursor * 9);
            state.api->gui->fill_block(cursor_x, input_y + 8,
                                       COLOR_CURSOR, 2, 16);
        }

        const uint32_t oldest = state.message_count == 0 ? 0 :
            (state.message_head + MAX_MESSAGES - state.message_count) % MAX_MESSAGES;
        const int chat_top = y + 160;
        const int chat_bottom = input_y - 8;
        const uint32_t line_height = 12;
        const uint32_t max_lines = chat_bottom > chat_top
            ? static_cast<uint32_t>(chat_bottom - chat_top) / line_height
            : 0;
        const uint32_t max_chars =
            static_cast<uint32_t>(window->width - 36) / 9;
        uint32_t total_lines = 0;
        for (uint32_t message = 0; message < state.message_count; ++message)
        {
            const uint32_t position =
                (oldest + message) % MAX_MESSAGES;
            total_lines += wrapped_line_count(
                state.messages[position], max_chars);
        }

        uint32_t lines_to_skip = total_lines > max_lines
            ? total_lines - max_lines
            : 0;
        int message_y = chat_top;
        for (uint32_t message = 0; message < state.message_count; ++message)
        {
            const uint32_t position =
                (oldest + message) % MAX_MESSAGES;
            uint32_t line_position = 0;
            char line[MAX_MESSAGE_LENGTH];
            while (next_wrapped_line(state.messages[position],
                                     &line_position, line, max_chars))
            {
                if (lines_to_skip > 0)
                {
                    --lines_to_skip;
                    continue;
                }
                if (message_y + 8 > chat_bottom)
                {
                    return;
                }
                state.api->gui->draw_text(line, x, message_y, COLOR_TEXT);
                message_y += static_cast<int>(line_height);
            }
        }
    }

    void nai_key_event(napp_window*, char key, bool pressed)
    {
        if (key == NAPP_KEY_SHIFT)
        {
            state.shift_pressed = pressed;
            return;
        }
        if (!pressed)
        {
            return;
        }

        if (key == '\n')
        {
            if (state.input_length == 0)
            {
                return;
            }

            char message[MAX_MESSAGE_LENGTH];
            message[0] = 'Y';
            message[1] = 'o';
            message[2] = 'u';
            message[3] = ':';
            message[4] = ' ';
            uint32_t length = 5;
            while (length + 1 < sizeof(message) &&
                   state.input[length - 5] != '\0')
            {
                message[length] = state.input[length - 5];
                ++length;
            }
            message[length] = '\0';
            add_message(message);
            if (state.status == model_status::installed &&
                state.api != nullptr &&
                !state.generating)
            {
                if (state.api->nai_generate_start != nullptr)
                {
                    const uint32_t job =
                        state.api->nai_generate_start(state.input, 48);
                    if (job != 0)
                    {
                        state.generating = true;
                        state.job_id = job;
                        add_message("AI is thinking");
                        state.thinking_slot =
                            (state.message_head + MAX_MESSAGES - 1) %
                            MAX_MESSAGES;
                    }
                    else
                    {
                        add_message("AI: Generation failed to start.");
                    }
                }
                else if (state.api->nai_generate != nullptr)
                {
                    state.generating = true;
                    state.output[0] = '\0';
                    if (state.api->nai_generate(state.input,
                                                state.output,
                                                sizeof(state.output),
                                                48))
                    {
                        char reply[MAX_MESSAGE_LENGTH];
                        reply[0] = 'A';
                        reply[1] = 'I';
                        reply[2] = ':';
                        reply[3] = ' ';
                        uint32_t length = 4;
                        while (length + 1 < sizeof(reply) &&
                               state.output[length - 4] != '\0')
                        {
                            reply[length] = state.output[length - 4];
                            ++length;
                        }
                        reply[length] = '\0';
                        add_message(reply);
                    }
                    else
                    {
                        add_message("AI: Generation failed.");
                    }
                    state.generating = false;
                }
            }
            replace_input("");
            return;
        }

        if (key == '\b')
        {
            if (state.input_cursor > 0)
            {
                for (uint32_t index = state.input_cursor - 1;
                     index < state.input_length; ++index)
                {
                    state.input[index] = state.input[index + 1];
                }
                --state.input_cursor;
                --state.input_length;
            }
            return;
        }

        if (key == NAPP_KEY_LEFT)
        {
            if (state.input_cursor > 0)
            {
                --state.input_cursor;
            }
            return;
        }
        if (key == NAPP_KEY_RIGHT)
        {
            if (state.input_cursor < state.input_length)
            {
                ++state.input_cursor;
            }
            return;
        }
        if (key >= 0x20 && key < 0x7F)
        {
            if (state.input_length + 1 >= sizeof(state.input))
            {
                return;
            }
            char character = key;
            if (state.shift_pressed &&
                character >= 'a' && character <= 'z')
            {
                character = static_cast<char>(character - 'a' + 'A');
            }
            for (uint32_t index = state.input_length;
                 index > state.input_cursor; --index)
            {
                state.input[index] = state.input[index - 1];
            }
            state.input[state.input_cursor++] = character;
            ++state.input_length;
            state.input[state.input_length] = '\0';
        }
    }
}

int _start(const napp_api* api)
{
    if (api == nullptr || api->gui == nullptr ||
        api->gui->open_window == nullptr ||
        api->gui->fill_block == nullptr ||
        api->gui->draw_text == nullptr)
    {
        return 1;
    }
    if (api->enable_fpu == nullptr || !api->enable_fpu())
    {
        return 1;
    }

    state.api = api;
    state.status = inspect_model(api);
    state.input_length = 0;
    state.input_cursor = 0;
    state.input[0] = '\0';
    state.message_count = 0;
    state.message_head = 0;
    state.shift_pressed = false;
    state.generating = false;
    state.job_id = 0;
    state.thinking_slot = 0;
    for (uint32_t index = 0; index < MAX_MESSAGES; ++index)
    {
        state.messages[index][0] = '\0';
    }

    const napp_window_config config =
    {
        "NasuaAI", 720, 520, true, true, nullptr,
        draw_nai, nullptr, nullptr, nullptr, nai_tick, 100, nai_key_event, nullptr, true
    };
    return api->gui->open_window(&config) ? 0 : 1;
}
