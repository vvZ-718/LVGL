#include "game_2048.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define GRID_SIZE   4
#define TARGET      2048
#define CELL_SIZE   90
#define GAP         10
#define BOARD_SIZE  (GRID_SIZE * CELL_SIZE + (GRID_SIZE + 1) * GAP)

static int       board[GRID_SIZE][GRID_SIZE];
static int       score;
static int       game_over;
static int       game_won;

static lv_obj_t * game_win;
static lv_obj_t * cells     [GRID_SIZE][GRID_SIZE];
static lv_obj_t * cell_labels[GRID_SIZE][GRID_SIZE];
static lv_obj_t * score_label;
static lv_obj_t * status_label;

void (*game_2048_on_exit)(void) = NULL;

/* ---- 颜色 ---- */

static lv_color_t cell_bg_color(int value)
{
    if(value == 0) return lv_color_hex(0xCDC1B4);
    int idx = 0, v = value;
    while(v > 1) { v >>= 1; idx++; }
    static const uint32_t colors[] = {
        0xEEE4DA, 0xEDE0C8, 0xF2B179, 0xF59563,
        0xF67C5F, 0xF65E3B, 0xEDCF72, 0xEDCC61,
        0xEDC850, 0xEDC53F, 0xEDC22E,
    };
    if(idx >= 1 && idx <= 11) return lv_color_hex(colors[idx - 1]);
    return lv_color_hex(0x3C3A32);
}

static lv_color_t cell_text_color(int value)
{
    return (value <= 4) ? lv_color_hex(0x776E65) : lv_color_hex(0xF9F6F2);
}

static const lv_font_t * cell_font(int value)
{
    if(value < 100)   return &lv_font_montserrat_48;
    if(value < 1000)  return &lv_font_montserrat_30;
    return &lv_font_montserrat_22;
}

/* ---- 核心算法 ---- */

static void compress_row(int row[GRID_SIZE])
{
    int pos = 0;
    for(int i = 0; i < GRID_SIZE; i++)
        if(row[i] != 0) row[pos++] = row[i];
    while(pos < GRID_SIZE) row[pos++] = 0;
}

static void merge_row(int row[GRID_SIZE])
{
    for(int i = 0; i < GRID_SIZE - 1; i++) {
        if(row[i] != 0 && row[i] == row[i + 1]) {
            row[i] *= 2;
            score += row[i];
            row[i + 1] = 0;
        }
    }
}

static int move_left(void)
{
    int changed = 0;
    for(int r = 0; r < GRID_SIZE; r++) {
        int row[GRID_SIZE];
        memcpy(row, board[r], sizeof(row));
        compress_row(row);
        merge_row(row);
        compress_row(row);
        for(int c = 0; c < GRID_SIZE; c++) {
            if(board[r][c] != row[c]) changed = 1;
            board[r][c] = row[c];
        }
    }
    return changed;
}

static int move_right(void)
{
    int changed = 0;
    for(int r = 0; r < GRID_SIZE; r++) {
        int row[GRID_SIZE];
        for(int c = 0; c < GRID_SIZE; c++)
            row[GRID_SIZE - 1 - c] = board[r][c];
        compress_row(row);
        merge_row(row);
        compress_row(row);
        for(int c = 0; c < GRID_SIZE; c++) {
            int val = row[GRID_SIZE - 1 - c];
            if(board[r][c] != val) changed = 1;
            board[r][c] = val;
        }
    }
    return changed;
}

static int move_up(void)
{
    int changed = 0;
    for(int c = 0; c < GRID_SIZE; c++) {
        int col[GRID_SIZE];
        for(int r = 0; r < GRID_SIZE; r++) col[r] = board[r][c];
        compress_row(col);
        merge_row(col);
        compress_row(col);
        for(int r = 0; r < GRID_SIZE; r++) {
            if(board[r][c] != col[r]) changed = 1;
            board[r][c] = col[r];
        }
    }
    return changed;
}

static int move_down(void)
{
    int changed = 0;
    for(int c = 0; c < GRID_SIZE; c++) {
        int col[GRID_SIZE];
        for(int r = 0; r < GRID_SIZE; r++)
            col[GRID_SIZE - 1 - r] = board[r][c];
        compress_row(col);
        merge_row(col);
        compress_row(col);
        for(int r = 0; r < GRID_SIZE; r++) {
            int val = col[GRID_SIZE - 1 - r];
            if(board[r][c] != val) changed = 1;
            board[r][c] = val;
        }
    }
    return changed;
}

static int can_merge(void)
{
    for(int r = 0; r < GRID_SIZE; r++)
        for(int c = 0; c < GRID_SIZE; c++) {
            if(board[r][c] == 0) return 1;
            if(c + 1 < GRID_SIZE && board[r][c] == board[r][c + 1]) return 1;
            if(r + 1 < GRID_SIZE && board[r][c] == board[r + 1][c]) return 1;
        }
    return 0;
}

static void spawn_tile(void)
{
    int empty[GRID_SIZE * GRID_SIZE][2];
    int count = 0;
    for(int r = 0; r < GRID_SIZE; r++)
        for(int c = 0; c < GRID_SIZE; c++)
            if(board[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
    if(count == 0) return;
    int idx = rand() % count;
    board[empty[idx][0]][empty[idx][1]] = (rand() % 10 == 0) ? 4 : 2;
}

static void check_status(void)
{
    if(!game_won) {
        for(int r = 0; r < GRID_SIZE; r++)
            for(int c = 0; c < GRID_SIZE; c++)
                if(board[r][c] >= TARGET) {
                    game_won = 1;
                    lv_label_set_text(status_label, "You Win!");
                    lv_obj_remove_flag(status_label, LV_OBJ_FLAG_HIDDEN);
                    return;
                }
    }
    if(!can_merge()) {
        game_over = 1;
        lv_label_set_text(status_label, "Game Over");
        lv_obj_remove_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---- UI ---- */

static void reset_game(void)
{
    memset(board, 0, sizeof(board));
    score = 0;
    game_over = 0;
    game_won = 0;
    srand((unsigned int)time(NULL));
    spawn_tile();
    spawn_tile();
    lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_display(void)
{
    for(int r = 0; r < GRID_SIZE; r++) {
        for(int c = 0; c < GRID_SIZE; c++) {
            int val = board[r][c];
            lv_obj_t * cell = cells[r][c];
            lv_obj_t * lbl  = cell_labels[r][c];
            if(val == 0) {
                lv_obj_add_flag(cell, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(cell, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_color(cell, cell_bg_color(val), 0);
                lv_label_set_text_fmt(lbl, "%d", val);
                lv_obj_set_style_text_color(lbl, cell_text_color(val), 0);
                lv_obj_set_style_text_font(lbl, cell_font(val), 0);
            }
        }
    }
    lv_label_set_text_fmt(score_label, "Score: %d", score);
}

static void gesture_cb(lv_event_t * e)
{
    if(game_over) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    int moved = 0;
    switch(dir) {
        case LV_DIR_LEFT:   moved = move_left();  break;
        case LV_DIR_RIGHT:  moved = move_right(); break;
        case LV_DIR_TOP:    moved = move_up();    break;
        case LV_DIR_BOTTOM: moved = move_down();  break;
        default: return;
    }
    if(moved) {
        spawn_tile();
        update_display();
        check_status();
    }
}

static void back_cb(lv_event_t * e)
{
    if(game_win) {
        lv_obj_delete(game_win);
        game_win = NULL;
    }
    if(game_2048_on_exit) game_2048_on_exit();
}

static void new_game_cb(lv_event_t * e)
{
    reset_game();
    update_display();
}

static void create_ui(void)
{
    game_win = lv_obj_create(lv_screen_active());
    lv_obj_set_size(game_win, 1024, 600);
    lv_obj_set_pos(game_win, 0, 0);
    lv_obj_set_style_bg_color(game_win, lv_color_hex(0xFAF8EF), 0);
    lv_obj_set_style_radius(game_win, 0, 0);

    /* Back */
    lv_obj_t * btn_back = lv_button_create(game_win);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_set_pos(btn_back, 10, 10);
    lv_obj_t * lb_back = lv_label_create(btn_back);
    lv_label_set_text(lb_back, "Back");
    lv_obj_center(lb_back);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);

    /* Score */
    score_label = lv_label_create(game_win);
    lv_label_set_text(score_label, "Score: 0");
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_30, 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 15);

    /* New */
    lv_obj_t * btn_new = lv_button_create(game_win);
    lv_obj_set_size(btn_new, 100, 40);
    lv_obj_set_pos(btn_new, 1024 - 110, 10);
    lv_obj_t * lb_new = lv_label_create(btn_new);
    lv_label_set_text(lb_new, "New");
    lv_obj_center(lb_new);
    lv_obj_add_event_cb(btn_new, new_game_cb, LV_EVENT_CLICKED, NULL);

    /* Board */
    int bx = (1024 - BOARD_SIZE) / 2;
    int by = (600 - BOARD_SIZE) / 2 + 25;
    lv_obj_t * board_bg = lv_obj_create(game_win);
    lv_obj_set_size(board_bg, BOARD_SIZE, BOARD_SIZE);
    lv_obj_set_pos(board_bg, bx, by);
    lv_obj_set_style_bg_color(board_bg, lv_color_hex(0xBBADA0), 0);
    lv_obj_set_style_radius(board_bg, 8, 0);
    lv_obj_add_event_cb(board_bg, gesture_cb, LV_EVENT_GESTURE, NULL);

    for(int r = 0; r < GRID_SIZE; r++) {
        for(int c = 0; c < GRID_SIZE; c++) {
            int x = GAP + c * (CELL_SIZE + GAP);
            int y = GAP + r * (CELL_SIZE + GAP);
            cells[r][c] = lv_obj_create(board_bg);
            lv_obj_set_size(cells[r][c], CELL_SIZE, CELL_SIZE);
            lv_obj_set_pos(cells[r][c], x, y);
            lv_obj_set_style_radius(cells[r][c], 6, 0);
            lv_obj_set_style_border_width(cells[r][c], 0, 0);
            lv_obj_set_style_bg_color(cells[r][c], lv_color_hex(0xCDC1B4), 0);
            lv_obj_add_flag(cells[r][c], LV_OBJ_FLAG_GESTURE_BUBBLE);
            cell_labels[r][c] = lv_label_create(cells[r][c]);
            lv_label_set_text(cell_labels[r][c], "");
            lv_obj_center(cell_labels[r][c]);
        }
    }

    status_label = lv_label_create(game_win);
    lv_label_set_text(status_label, "");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x776E65), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
}

void game_2048_start(void)
{
    if(game_win) {
        lv_obj_delete(game_win);
        game_win = NULL;
    }
    create_ui();
    reset_game();
    update_display();
}
