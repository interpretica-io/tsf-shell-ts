/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica Unipessoal Lda */
/** @file
 * @brief Shell Group
 *
 * The scrollback keeps the newest lines that scrolled off the top, up
 * to the module's limit of 2000, in order and without gaps, however
 * much was printed. Numbered lines are fed straight into the emulator
 * in counts below, at and above the limit, and the scrollback is read
 * back and checked line by line.
 */

#define TE_TEST_NAME    "shell/deepscroll"

#include "te_config.h"
#include "tapi_test.h"
#include "te_string.h"
#include "te_mi_log.h"

#include "tapi_shell.h"
#include "tsapi_shell.h"

/** Lines the module keeps in scrollback, on every backend. */
#define DS_LIMIT    2000
/** Screen height: a full screen of lines is visible, not in scrollback. */
#define DS_ROWS     24
#define DS_COLS     80
/** Bytes per feed call. */
#define DS_CHUNK    16384

/* Feed lines L00001..L<n>, each followed by CR LF. */
static void
feed_numbered(tapi_shell_session *sh, int n)
{
    te_string buf = TE_STRING_INIT;
    int       i;

    for (i = 1; i <= n; i++)
    {
        te_string_append(&buf, "L%05d\r\n", i);
        if (buf.len >= DS_CHUNK || i == n)
        {
            CHECK_RC(tapi_shell_feed(sh, buf.ptr, buf.len));
            te_string_reset(&buf);
        }
    }
    te_string_free(&buf);
}

/*
 * Check that @p sb is exactly the lines L<first>..L<last>, one per
 * line; on a mismatch log where it is and return false.
 */
static bool
check_run(const char *sb, int first, int last, int *got)
{
    const char *p = sb;
    int         expect = first;

    *got = 0;
    while (p != NULL && *p != '\0')
    {
        size_t len = strcspn(p, "\n");
        char   want[16];

        snprintf(want, sizeof(want), "L%05d", expect);
        if (len != strlen(want) || strncmp(p, want, len) != 0)
        {
            ERROR("scrollback line %d is '%.*s', expected '%s'", *got,
                  (int)len, p, want);
            return false;
        }
        (*got)++;
        expect++;
        p += len + (p[len] == '\n');
    }

    if (expect - 1 != last)
    {
        ERROR("scrollback ends at L%05d, expected L%05d", expect - 1, last);
        return false;
    }
    return true;
}

int
main(int argc, char **argv)
{
    static const int   counts[] = { 100, 1000, 2000, 2023, 2024, 2025,
                                    2100, 3000, 5000 };
    tsapi_shell_sess   sess;
    const char        *backend;
    te_mi_logger      *logger = NULL;
    te_string          sb = TE_STRING_INIT;
    int                kept[TE_ARRAY_LEN(counts)];
    unsigned int       i;
    bool               failed = false;

    TEST_START;
    TEST_GET_STRING_PARAM(backend);

    TEST_STEP("Open a session; skip when the agent lacks the backend");
    CHECK_RC(tsapi_shell_sess_init(&sess, "pco_shell_deepscroll"));
    if (!tsapi_shell_sess_select(&sess, backend))
        TEST_SKIP("The agent does not have this terminal-emulator backend");

    for (i = 0; i < TE_ARRAY_LEN(counts); i++)
    {
        tapi_shell_session sh;
        int                n = counts[i];
        int                scrolled = n - (DS_ROWS - 1);
        int                expect = scrolled < DS_LIMIT ? scrolled : DS_LIMIT;
        int                got = 0;

        TEST_SUBSTEP("Print %d numbered lines and read the scrollback", n);
        CHECK_RC(tsapi_shell_open_idle(&sess, DS_COLS, DS_ROWS, &sh));
        feed_numbered(&sh, n);
        te_string_reset(&sb);
        CHECK_RC(tapi_shell_scrollback(&sh, 0, &sb));
        tapi_shell_close(&sh);

        if (!check_run(sb.ptr != NULL ? sb.ptr : "", scrolled - expect + 1,
                       scrolled, &got) || got != expect)
        {
            ERROR("after %d lines the scrollback holds %d lines, expected "
                  "L%05d..L%05d (%d lines)", n, got, scrolled - expect + 1,
                  scrolled, expect);
            failed = true;
        }
        RING("printed %d lines: scrollback holds %d (expected %d)", n, got,
             expect);
        kept[i] = got;
    }

    TEST_STEP("Chart the lines kept against the lines printed");
    CHECK_RC(te_mi_logger_meas_create("tsf-shell", &logger));
    for (i = 0; i < TE_ARRAY_LEN(counts); i++)
    {
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "printed", TE_MI_MEAS_AGGR_SINGLE, counts[i],
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
        te_mi_logger_add_meas(logger, NULL, TE_MI_MEAS_UNITLESS_VALUE,
                              "kept", TE_MI_MEAS_AGGR_SINGLE, kept[i],
                              TE_MI_MEAS_MULTIPLIER_PLAIN);
    }
    te_mi_logger_add_meas_view(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "deepscroll",
                               "Scrollback lines kept against lines printed");
    te_mi_logger_meas_graph_axis_add_name(logger, NULL,
                               TE_MI_MEAS_VIEW_LINE_GRAPH, "deepscroll",
                               TE_MI_GRAPH_AXIS_X, "printed");
    te_mi_logger_meas_graph_axis_add(logger, NULL, TE_MI_MEAS_VIEW_LINE_GRAPH,
                               "deepscroll", TE_MI_GRAPH_AXIS_Y,
                               TE_MI_MEAS_UNITLESS_VALUE, "kept");
    te_mi_logger_destroy(logger);
    logger = NULL;

    if (failed)
        TEST_VERDICT("the scrollback does not hold the newest lines that "
                     "scrolled off, in order");

    TEST_SUCCESS;

cleanup:
    te_string_free(&sb);
    if (logger != NULL)
        te_mi_logger_destroy(logger);
    tsapi_shell_sess_fini(&sess);
    TEST_END;
}
