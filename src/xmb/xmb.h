#ifndef XMB_H
#define XMB_H

#define XMB_MAX_CATEGORIES 8
#define XMB_MAX_ITEMS      16

typedef struct {
    const char* name;
    const char* icon_path;
    const char* items[XMB_MAX_ITEMS];
    int item_count;
} XMBCategory;

typedef struct {
    XMBCategory categories[XMB_MAX_CATEGORIES];
    int category_count;
    int current_category;
    int current_item;
} XMBMenu;

void xmb_init(XMBMenu* menu);
void xmb_handle_input(XMBMenu* menu, int input);
void xmb_render(const XMBMenu* menu);

#endif // XMB_H