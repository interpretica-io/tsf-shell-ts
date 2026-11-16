/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 */
#define TE_TEST_NAME    "shell/prologue"
#include "te_config.h"
#include "tapi_test.h"
int
main(int argc, char **argv)
{
    TEST_START;
    TEST_STEP("Shell group prologue");
    TEST_SUCCESS;
cleanup:
    TEST_END;
}
