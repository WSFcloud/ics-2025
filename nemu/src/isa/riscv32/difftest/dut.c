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

#include <isa.h>
#include <cpu/difftest.h>
#include "../local-include/reg.h"

const char *difftest_riscv32_regs[] = {
    "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
    bool res = true;

    for (int i = 0; i < ARRLEN(difftest_riscv32_regs); i++) {
        if (gpr(i) != ref_r->gpr[i]) {
            res = false;
        }
    }

    if (!res) {
        printf("================================================================\n");
        printf("REF Registers:\n");
        for (int i = 0; i < ARRLEN(difftest_riscv32_regs); i++) {
            printf("%-3s: %-10u\t", difftest_riscv32_regs[i], ref_r->gpr[i]);
            if ((i + 1) % 4 == 0) {
                printf("\n");
            }
        }
        printf("================================================================\n");
    }

    return res;
}

void isa_difftest_attach() {
}
