/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * A snapshot has to carry everything that decides how later output is
 * rendered. Two terminals are brought into the same state; one is then
 * replaced by the terminal decoded from its own snapshot. The two must
 * look the same right away and must still look the same after the same
 * further output.
 */

#define TE_TEST_NAME    "shell/snapshot"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/* Number of lines of @p a and @p b that differ (0 when they are equal). */
static int
lines_differ(const char *a, const char *b)
{
    int n = 0;

    while (*a != '\0' || *b != '\0')
    {
        size_t la = strcspn(a, "\n");
        size_t lb = strcspn(b, "\n");

        if (la != lb || strncmp(a, b, la) != 0)
            n++;
        a += la + (a[la] == '\n');
        b += lb + (b[lb] == '\n');
    }

    return n;
}

/* Chart how many lines of the two descriptions differ at each stage. */
static void
chart_diff(const char *title, int before, int after)
{
    te_mi_logger *logger = NULL;

    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "stage", TE_MI_MEAS_AGGR_SINGLE, 0,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "differing_lines", TE_MI_MEAS_AGGR_SINGLE, before,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "stage", TE_MI_MEAS_AGGR_SINGLE, 1,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                          "differing_lines", TE_MI_MEAS_AGGR_SINGLE, after,
                          TE_MI_MEAS_MULTIPLIER_PLAIN);
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "diff", title);
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "diff",
                               TE_MI_GRAPH_AXIS_X, "stage");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "diff", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "differing_lines");
    te_mi_logger_destroy(logger);
}

int
main(int argc, char **argv)
{
    tsapi_shell_sess       sess;
    const char            *backend;
    const char            *seq;
    const tsapi_shell_seq *sq;
    tapi_shell_session     live;
    tapi_shell_session     restored;
    bool                   live_opened = false;
    bool                   restored_opened = false;
    te_string              a = TE_STRING_INIT;
    te_string              b = TE_STRING_INIT;
    int                    diff_before;
    int                    diff_after;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);
    TEST_GET_STRING_PARAM(seq);

    sq = tsapi_shell_seq_find(seq);
    if (sq == NULL)
        TEST_FAIL("There is no sequence named '%s'", seq);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_snapshot"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Bring two terminals into the same state");
    CHECK_RC(tsapi_shell_open_idle(&sess, sq->cols, sq->rows, &live));
    live_opened = true;
    CHECK_RC(tsapi_shell_open_idle(&sess, sq->cols, sq->rows, &restored));
    restored_opened = true;
    CHECK_RC(tapi_shell_feed_str(&live, sq->before));
    CHECK_RC(tapi_shell_feed_str(&restored, sq->before));

    TEST_STEP("Replace one of them by its own snapshot");
    RPC_AWAIT_ERROR(sess.pco);
    rc = tapi_shell_snapshot_restore(&restored);
    if (TE_RC_GET_ERROR(rc) == TE_EOPNOTSUPP)
        TEST_SKIP("The backend has no snapshot format");
    if (rc != 0)
        TEST_VERDICT("the snapshot could not be taken and decoded: %r", rc);

    TEST_STEP("The two look the same right after the restore");
    CHECK_RC(tsapi_shell_state(&live, &a));
    CHECK_RC(tsapi_shell_state(&restored, &b));
    diff_before = lines_differ(a.ptr, b.ptr);
    if (diff_before != 0)
        RING("live terminal:\n%s\nrestored terminal:\n%s", a.ptr, b.ptr);
    te_string_reset(&a);
    te_string_reset(&b);

    TEST_STEP("The two look the same after the same further output");
    CHECK_RC(tapi_shell_feed_str(&live, sq->after));
    CHECK_RC(tapi_shell_feed_str(&restored, sq->after));
    CHECK_RC(tsapi_shell_state(&live, &a));
    CHECK_RC(tsapi_shell_state(&restored, &b));
    diff_after = lines_differ(a.ptr, b.ptr);
    if (diff_after != 0)
        RING("live terminal:\n%s\nrestored terminal:\n%s", a.ptr, b.ptr);

    chart_diff("Lines differing between the live and the restored terminal "
               "(0 = after restore, 1 = after further output)",
               diff_before, diff_after);

    if (diff_after != 0)
        TEST_VERDICT("the restored terminal renders further output "
                     "differently from the live one");
    if (diff_before != 0)
        TEST_VERDICT("the restored terminal differs from the live one "
                     "right after the restore");

    TEST_SUCCESS;

cleanup:
    te_string_free(&a);
    te_string_free(&b);
    if (live_opened)
        tapi_shell_close(&live);
    if (restored_opened)
        tapi_shell_close(&restored);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
