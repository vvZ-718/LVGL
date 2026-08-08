#ifndef GAME_2048_H
#define GAME_2048_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void game_2048_start(void);

extern void (*game_2048_on_exit)(void);

#ifdef __cplusplus
}
#endif

#endif /* GAME_2048_H */
