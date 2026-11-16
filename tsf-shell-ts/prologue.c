/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2025-2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Test Suite
 *
 * Test Suite prologue.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "prologue"

#include "te_config.h"
#include "tapi_test.h"

int
main(int argc, char **argv)
{
    TEST_START;

    TEST_STEP("Initialize test suite");
    /* Each test opens its own session to the agent and generates its
     * own fixtures, so there is nothing to do here. */

    TEST_SUCCESS;

cleanup:

    TEST_END;
}
