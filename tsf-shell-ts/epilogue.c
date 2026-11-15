/** @file
 * @brief Test Suite
 *
 * Test Suite epilogue.
 *
 * Copyright (C) 2025-2026 Interpretica Unipessoal Lda
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_TEST_NAME    "epilogue"

#include "te_config.h"
#include "tapi_test.h"

int
main(int argc, char **argv)
{
    TEST_START;

    TEST_STEP("Finalize test suite");
    /* No actions required */

    TEST_SUCCESS;

cleanup:

    TEST_END;
}
