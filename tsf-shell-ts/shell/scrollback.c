/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * Lines that scroll off the top of the screen are not lost: they are
 * kept in scrollback. A hundred numbered lines are printed on a
 * 24-row screen; an early line is then found in scrollback and absent
 * from the visible screen, while the last line is on the screen and not
 * in scrollback.
 */

#define TE_TEST_NAME    "shell/scrollback"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* L001..L100, one per line. */
#define SB_SCRIPT   "seq -f 'L%03.0f' 1 100"
#define EARLY_LINE  "L007"
#define LAST_LINE   "L100"

int
main(int argc, char **argv)
{
    tsapi_shell_sess   sess;
    const char        *backend;
    tapi_shell_session sh;
    te_mi_logger      *logger = NULL;
    te_string          sb = TE_STRING_INIT;
    bool               opened = false;
    bool               sb_has_early;
    bool               screen_has_last;
    bool               screen_has_early;
    int                sb_lines = 0;
    const char        *p;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_scrollback"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Print a hundred numbered lines on a 24-row screen");
    CHECK_RC(tsapi_shell_open_sh(&sess, SB_SCRIPT, 80, 24, &sh));
    opened = true;

    /* Waiting for the last line guarantees everything before it was fed. */
    rc = tapi_shell_expect_str(&sh, LAST_LINE, 5000);
    if (rc != 0)
        TEST_VERDICT("the last line '%s' never arrived (%r)", LAST_LINE, rc);

    TEST_STEP("An early line is in scrollback, not on the visible screen");
    CHECK_RC(tapi_shell_scrollback(&sh, 0, &sb));
    sb_has_early = sb.ptr != NULL && strstr(sb.ptr, EARLY_LINE) != NULL;
    for (p = sb.ptr; p != NULL && *p != '\0'; p++)
        if (*p == '\n')
            sb_lines++;
    RING("scrollback holds %d lines, %zu bytes", sb_lines,
         sb.ptr != NULL ? strlen(sb.ptr) : (size_t)0);
    if (!sb_has_early)
        TEST_VERDICT("the early line '%s' is not in scrollback", EARLY_LINE);

    screen_has_early = tapi_shell_screen_contains(&sh, EARLY_LINE);
    if (screen_has_early)
        TEST_VERDICT("the early line '%s' is still on the visible screen",
                     EARLY_LINE);

    TEST_STEP("The last line is on the screen, not in scrollback");
    screen_has_last = tapi_shell_screen_contains(&sh, LAST_LINE);
    if (!screen_has_last)
        TEST_VERDICT("the last line '%s' is not on the visible screen",
                     LAST_LINE);
    if (sb.ptr != NULL && strstr(sb.ptr, LAST_LINE) != NULL)
        TEST_VERDICT("the last line '%s' leaked into scrollback", LAST_LINE);

    TEST_STEP("Chart how many lines scrollback retained");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "metric", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "scrollback_lines", TE_MI_MEAS_AGGR_SINGLE, sb_lines,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "metric", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "scrollback_lines", TE_MI_MEAS_AGGR_SINGLE, sb_lines,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sb", "Lines retained in scrollback");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "sb",
                               TE_MI_GRAPH_AXIS_X, "metric");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "sb", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "scrollback_lines");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    te_string_free(&sb);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
