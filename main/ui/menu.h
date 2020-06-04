#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui/button.h"

#define MENU_ALT_PAIRINGS_MAX 8

typedef struct air_pairing_s air_pairing_t;
typedef struct rc_s rc_t;
typedef struct menu_s menu_t;

extern menu_t menu_empty;
extern menu_t menu_bind_req;
extern menu_t menu_bind_info;
extern menu_t menu_alt_pairings;

void menu_init(rc_t *rc);
void menu_update(void);
bool menu_handle_button_event(const button_event_t *ev);
void menu_push_active(menu_t *menu);
void menu_set_active(menu_t *menu);
void menu_pop_active(void);
menu_t *menu_get_active(void);
int menu_get_num_entries(const menu_t *menu);
const char *menu_get_prompt(const menu_t *menu);
const char *menu_entry_get_title(const menu_t *menu, int idx, char *buf, uint16_t bufsize);
uint8_t menu_get_entry_selected(const menu_t *menu);
bool menu_is_entry_selected(const menu_t *menu, int idx);

void menu_set_alt_pairings(air_pairing_t *pairings, size_t count);
