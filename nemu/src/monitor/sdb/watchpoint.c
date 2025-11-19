/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

static word_t used_wp[NR_WP] = {};

void init_wp_pool() {
    int i;
    for (i = 0; i < NR_WP; i++) {
        wp_pool[i].NO = i;
        wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    }

    head = NULL;
    free_ = wp_pool;
}

WP *new_wp() {
    if (free_ == NULL) {
        printf("Error: No free watchpoints available.\n");
        assert(0);
    }

    WP *wp = free_;
    free_ = free_->next;
    wp->next = head;
    head = wp;

    for (int i = 0; i < NR_WP; i++) {
        if (used_wp[i] == 0) {
            wp->NO = i + 1;
            break;
        }
    }

    return wp;
}

void free_wp(int free_no) {
    WP *wp = NULL;

    if (head == NULL) {
        printf("Error: No active watchpoints to free.\n");
        return;
    }

    if (head->NO == free_no) {
        wp = head;
        head = head->next;
    } else {
        WP *prev = head;
        while (prev->next != NULL && prev->next->NO != free_no) {
            prev = prev->next;
        }

        if (prev->next != NULL && prev->next->NO == free_no) {
            wp = prev->next;
            prev->next = prev->next->next;
        } else {
            printf("Error: Watchpoint %d is not in the active list.\n", free_no);
            // assert(0);
            return;
        }
    }
    if (wp != NULL) {
        wp->next = free_;
        free_ = wp;
        used_wp[wp->NO - 1] = 0;
        wp->expr[0] = '\0';
        printf("Success: Watchpoint %d freed.\n", free_no);
    }
}

int scan_watch_points() {
    WP *wp = head;
    int found_change = 0;

    while (wp != NULL) {
        bool isSuccess;
        word_t new_val = expr(wp->expr, &isSuccess);

        if (isSuccess && new_val != wp->val) {
            // printf("Hit watchpoint %d: %s\n", wp->NO, wp->expr);
            // printf("Old value = %d\n", wp->val);
            // printf("New value = %d\n", new_val);
            wp->old_val = wp->val;
            wp->val = new_val;
            found_change = 1;
        }

        wp = wp->next;
    }

    return found_change;
}

void watch_points_display() {
    WP *wp = head;
    if (wp == NULL) {
        printf("No watchpoints.\n");
        return;
    }

    printf("%-4s %-20s %-20s %-10s\n", "NO", "EXPR", "Value", "Old-Value");
    while (wp != NULL) {
        printf("%-4d %-20s 0x%-18x 0x%-18x\n", wp->NO, wp->expr, wp->val, wp->old_val);
        wp = wp->next;
    }
}