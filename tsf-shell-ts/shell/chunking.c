/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * What a terminal shows must not depend on how its input was cut into
 * writes. The same bytes are fed to one terminal in two pieces and to
 * another one byte at a time; the two must look the same at the cut
 * and at the end.
 */

#define TE_TEST_NAME    "shell/chunking"

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

/* Feed @p str to @p sh one byte per call. */
static void
feed_bytewise(tapi_shell_session *sh, const char *str)
{
    size_t i;
    size_t len = strlen(str);

    for (i = 0; i < len; i++)
        CHECK_RC(tapi_shell_feed(sh, str + i, 1));
}

int
main(int argc, char **argv)
{
    tsapi_shell_sess       sess;
    const char            *backend;
    const char            *seq;
    const tsapi_shell_seq *sq;
    tapi_shell_session     whole;
    tapi_shell_session     bytewise;
    bool                   whole_opened = false;
    bool                   bytewise_opened = false;
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
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_chunking"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    TEST_STEP("Feed the first part in one write and one byte at a time");
    CHECK_RC(tsapi_shell_open_idle(&sess, sq->cols, sq->rows, &whole));
    whole_opened = true;
    CHECK_RC(tsapi_shell_open_idle(&sess, sq->cols, sq->rows, &bytewise));
    bytewise_opened = true;
    CHECK_RC(tapi_shell_feed_str(&whole, sq->before));
    feed_bytewise(&bytewise, sq->before);

    CHECK_RC(tsapi_shell_state(&whole, &a));
    CHECK_RC(tsapi_shell_state(&bytewise, &b));
    diff_before = lines_differ(a.ptr, b.ptr);
    if (diff_before != 0)
        RING("one write:\n%s\none byte at a time:\n%s", a.ptr, b.ptr);
    te_string_reset(&a);
    te_string_reset(&b);

    TEST_STEP("Feed the rest the same two ways");
    CHECK_RC(tapi_shell_feed_str(&whole, sq->after));
    feed_bytewise(&bytewise, sq->after);

    CHECK_RC(tsapi_shell_state(&whole, &a));
    CHECK_RC(tsapi_shell_state(&bytewise, &b));
    diff_after = lines_differ(a.ptr, b.ptr);
    if (diff_after != 0)
        RING("one write:\n%s\none byte at a time:\n%s", a.ptr, b.ptr);

    chart_diff("Lines differing between one write and byte-wise input "
               "(0 = at the cut, 1 = at the end)",
               diff_before, diff_after);

    if (diff_before != 0 || diff_after != 0)
        TEST_VERDICT("the terminal looks different when its input is fed "
                     "one byte at a time");

    TEST_SUCCESS;

cleanup:
    te_string_free(&a);
    te_string_free(&b);
    if (whole_opened)
        tapi_shell_close(&whole);
    if (bytewise_opened)
        tapi_shell_close(&bytewise);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
