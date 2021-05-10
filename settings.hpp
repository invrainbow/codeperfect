#pragma once

struct Settings {
    // styling
    float status_padding_x = 6;
    float status_padding_y = 3;
    float line_number_margin_left = 4;
    float line_number_margin_right = 10;
    // float icon_size = 16;
    float autocomplete_menu_padding = 4;
    float autocomplete_menu_margin_y = 4;
    float autocomplete_item_padding_x = 6;
    float autocomplete_item_padding_y = 2;

    float tabs_offset = 50;

    int scrolloff = 2;

    float parameter_hint_margin_y = 4;
    float parameter_hint_padding_x = 2;
    float parameter_hint_padding_y = 2;
    float editor_margin_x = 5;
    float editor_margin_y = 5;
    float line_height = 1.2;
};

extern Settings settings;
