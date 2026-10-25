#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ben10_keyboard_focus(int focused);
void ben10_keyboard_event(unsigned scancode, int pressed);
int ben10_keyboard_snapshot(uint8_t* keys, size_t count);
#ifdef __cplusplus
}
#endif
