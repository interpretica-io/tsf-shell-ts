/** @file
 * @brief Shell Group
 *
 * The cursor-visibility mode (DECTCEM) is tracked: hiding the cursor
 * with CSI ?25l reports it hidden, showing it with CSI ?25h reports it
 * shown. Two sessions, one per state, are run and the reported
 * visibility is asserted and charted.
 *
 * Copyright (C) 2026 Interpretica Unipessoal Lda
 */

#define TE_TEST_NAME    "shell/cursorvis"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

int
main(int argc, char **argv)
{
    tsapi_shell_sess sess;
    te_mi_logger    *logger = NULL;
    unsigned int     i;
    static const struct {
        const char *script;
        bool        want_visible;
        const char *label;
    } cases[] = {
        { "printf '\\033[?25l'", false, "hidden" },
        { "printf '\\033[?25h'", true,  "shown" },
    };

    TEST_START;

    TEST_STEP("Open a session; skip when no backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_cursorvis"));
    if (!tapi_shell_available(sess.pco))
        TEST_SKIP("The agent has no terminal-emulator backend");

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));

    TEST_STEP("Each visibility mode is reported as set");
    for (i = 0; i < TE_ARRAY_LEN(cases); i++)
    {
        tapi_shell_session sh;
        tapi_shell_status  status;
        int                cx = -1, cy = -1;
        bool               vis = false;

        CHECK_RC(tsapi_shell_open_sh(&sess, cases[i].script, 80, 24, &sh));
        CHECK_RC(tapi_shell_wait(&sh, 5000, &status));
        rc = tapi_shell_cursor(&sh, &cx, &cy, &vis);
        if (rc != 0)
        {
            tapi_shell_close(&sh);
            CHECK_RC(rc);
        }
        RING("case '%s': visible=%d", cases[i].label, (int)vis);
        if (vis != cases[i].want_visible)
        {
            tapi_shell_close(&sh);
            TEST_VERDICT("case '%s' reported visible=%d, expected %d",
                         cases[i].label, (int)vis,
                         (int)cases[i].want_visible);
        }

        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "case", TE_MI_MEAS_AGGR_SINGLE, i,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "visible", TE_MI_MEAS_AGGR_SINGLE, (int)vis,
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        tapi_shell_close(&sh);
    }

    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "vis", "Reported cursor visibility per case");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "vis",
                               TE_MI_GRAPH_AXIS_X, "case");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "vis", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "visible");
    te_mi_logger_destroy(logger);
    logger = NULL;

    TEST_SUCCESS;

cleanup:
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
