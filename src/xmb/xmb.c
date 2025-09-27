#include "xmb.h"
#include <stdio.h>

void xmb_init(XMBMenu* menu) {
    menu->category_count = 0;
    menu->current_category = 0;
    menu->current_item = 0;
}

void xmb_handle_input(XMBMenu* menu, int input) {
    // input: 0=left, 1=right, 2=up, 3=down
    if (input == 0) { // left
        menu->current_category = (menu->current_category - 1 + menu->category_count) % menu->category_count;
        menu->current_item = 0;
    } else if (input == 1) { // right
        menu->current_category = (menu->current_category + 1) % menu->category_count;
        menu->current_item = 0;
    } else if (input == 2) { // up
        int items = menu->categories[menu->current_category].item_count;
        menu->current_item = (menu->current_item - 1 + items) % items;
    } else if (input == 3) { // down
        int items = menu->categories[menu->current_category].item_count;
        menu->current_item = (menu->current_item + 1) % items;
    }
}

void xmb_render(const XMBMenu* menu) {
    printf("XMB Menu:\n");
    for (int i = 0; i < menu->category_count; ++i) {
        if (i == menu->current_category) printf(" > ");
        else printf("   ");
        printf("%s\n", menu->categories[i].name);
        if (i == menu->current_category) {
            for (int j = 0; j < menu->categories[i].item_count; ++j) {
                printf("    %c %s\n", (j == menu->current_item) ? '*' : ' ', menu->categories[i].items[j]);
            }
        }
    }
}