/** @file
 * @brief Shell Group
 *
 * Resizing the session reaches the child - its @c stty @c size changes,
 * which means the pty window size was updated and SIGWINCH delivered -
 * and the emulator's own screen size follows. A shell loop prints
 * @c stty @c size each time it is prodded, once before and once after
 * the resize.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/resize"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* On each line read, print the tty window size ("rows cols"). */
#define STTY_SCRIPT \
    "while IFS= read -r _; do stty size; done"

int
main(int argc, char **argv)
{
    tsapi_shell_sess    sess;
    tapi_shell_session  sh;
    te_mi_logger       *logger = NULL;
    tapi_shell_screen_t screen;
    bool                opened = false;

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_resize"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    TEST_STEP("Spawn a shell that reports its tty size on demand, 80x24");
    CHECK_RC(tsapi_shell_open_sh(&sess, STTY_SCRIPT, 80, 24, &sh));
    opened = true;

    TEST_STEP("The child sees the initial 24x80");
    CHECK_RC(tapi_shell_send_key(&sh, TAPI_SHELL_KEY_ENTER, 0));
    rc = tapi_shell_expect_str(&sh, "24 80", 5000);
    if (rc != 0)
        TEST_VERDICT("the child did not report the initial size 24 80 (%r)",
                     rc);

    TEST_STEP("Resize to 100x40 and the child sees it (SIGWINCH)");
    CHECK_RC(tapi_shell_resize(&sh, 100, 40));
    CHECK_RC(tapi_shell_send_key(&sh, TAPI_SHELL_KEY_ENTER, 0));
    rc = tapi_shell_expect_str(&sh, "40 100", 5000);
    if (rc != 0)
        TEST_VERDICT("the child did not report the new size 40 100 (%r)", rc);

    TEST_STEP("The emulator's own screen size followed the resize");
    CHECK_RC(tapi_shell_screen(&sh, &screen));
    RING("emulator screen is now %dx%d", screen.cols, screen.rows);
    if (screen.cols != 100 || screen.rows != 40)
    {
        int c = screen.cols, r = screen.rows;

        tapi_shell_screen_free(&screen);
        TEST_VERDICT("emulator screen is %dx%d, expected 100x40", c, r);
    }
    tapi_shell_screen_free(&screen);

    TEST_STEP("Chart the screen size before and after the resize");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    {
        int phase;
        int cols[2] = { 80, 100 };
        int rows[2] = { 24, 40 };

        for (phase = 0; phase < 2; phase++)
        {
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "phase", TE_MI_MEAS_AGGR_SINGLE, phase,
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "cols", TE_MI_MEAS_AGGR_SINGLE, cols[phase],
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
            te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                                  "rows", TE_MI_MEAS_AGGR_SINGLE, rows[phase],
                                  TE_MI_MEAS_MULTIPLIER_PLAIN);
        }
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "size", "Screen size across the resize");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "size",
                               TE_MI_GRAPH_AXIS_X, "phase");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "size", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "cols");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "size", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "rows");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    if (opened)
        tapi_shell_close(&sh);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
