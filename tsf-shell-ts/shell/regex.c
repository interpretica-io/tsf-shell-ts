/** @file
 * @brief Shell Group
 *
 * The expect primitive matches a POSIX extended regular expression on
 * the output stream, and reports a timeout when the pattern never
 * arrives. An interactive echo is driven: a regex matches its reply,
 * and a pattern that cannot appear times out with the expected error.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/regex"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Echo each line back prefixed; no input echo, so the stream is clean. */
#define REGEX_SCRIPT \
    "stty -echo; while IFS= read -r l; do echo \"VAL=$l\"; done"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    tapi_shell_session sh;
    te_mi_logger      *logger = NULL;
    bool               opened = false;
    tapi_shell_match   match = { 0 };
    int                neg_err = 0;

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_regex"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Spawn an interactive echo");
    CHECK_RC(tsapi_shell_open_sh(&sess, REGEX_SCRIPT, 80, 24, &sh));
    opened = true;

    TEST_STEP("A regex matches the reply on the stream");
    CHECK_RC(tapi_shell_send(&sh, "12345"));
    CHECK_RC(tapi_shell_send_key(&sh, TAPI_SHELL_KEY_ENTER, 0));
    {
        tapi_shell_expect_opts opts = {
            .regex = true,
            .where = TAPI_SHELL_WHERE_STREAM,
            .timeout_ms = 3000,
        };

        rc = tapi_shell_expect(&sh, "VAL=[0-9]+", &opts, &match);
        if (rc != 0)
            TEST_VERDICT("the regex 'VAL=[0-9]+' did not match (%r)", rc);
        RING("regex matched '%s' at stream offset %d",
             match.text != NULL ? match.text : "", match.offset);
        if (match.text == NULL || strstr(match.text, "VAL=") == NULL)
            TEST_VERDICT("the matched text '%s' lacks the expected prefix",
                         match.text != NULL ? match.text : "");
        tapi_shell_match_free(&match);
    }

    TEST_STEP("A pattern that cannot appear times out");
    {
        tapi_shell_expect_opts opts = {
            .where = TAPI_SHELL_WHERE_STREAM,
            .timeout_ms = 700,
        };

        rc = tapi_shell_expect(&sh, "ZZZ-NEVER-APPEARS", &opts, NULL);
        neg_err = TE_RC_GET_ERROR(rc);
        RING("absent pattern returned %r", rc);
        if (rc == 0)
            TEST_VERDICT("expect matched a pattern that was never sent");
        if (neg_err != TE_ETIMEDOUT)
            TEST_VERDICT("expect for an absent pattern returned %r, "
                         "expected ETIMEDOUT", rc);
        rc = 0;
    }

    TEST_STEP("Chart matched versus timed-out expect outcomes");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "case", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "matched", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "case", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "matched", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "rx", "Expect outcome (1 matched, 0 timed out)");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "rx",
                               TE_MI_GRAPH_AXIS_X, "case");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "rx", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "matched");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    tapi_shell_match_free(&match);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
