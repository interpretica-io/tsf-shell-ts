/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#define TE_LGR_USER "TSAPI SHELL"

#include "te_config.h"

#include <stdlib.h>
#include <string.h>

#include "logger_api.h"
#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"

#include "tsapi_shell.h"

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_sess_init(tsapi_shell_sess *sess, const char *name)
{
    te_errno rc;

    memset(sess, 0, sizeof(*sess));
    sess->ta = TSAPI_SHELL_TA;

    rc = rcf_rpc_server_create(sess->ta, name, &sess->pco);
    if (rc != 0)
    {
        ERROR("Cannot create the RPC server '%s' on %s: %r", name, sess->ta,
              rc);
        return rc;
    }

    return 0;
}

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_open_sh(tsapi_shell_sess *sess, const char *script, int cols,
                    int rows, tapi_shell_session *sh)
{
    const char          *argv[] = { "sh", "-c", script, NULL };
    tapi_shell_open_opts opts = { .backend = sess->backend };

    return tapi_shell_open_ex(sess->pco, argv, cols, rows, &opts, sh);
}

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_open_idle(tsapi_shell_sess *sess, int cols, int rows,
                      tapi_shell_session *sh)
{
    /* Quiet and long-lived: the test closes the session when done. */
    return tsapi_shell_open_sh(sess, "stty -echo; exec sleep 600", cols, rows,
                               sh);
}

/* See description in tsapi_shell.h */
bool
tsapi_shell_sess_select(tsapi_shell_sess *sess, const char *backend)
{
    if (strcmp(backend, "ghostty") == 0)
        sess->backend = TAPI_SHELL_BACKEND_GHOSTTY;
    else if (strcmp(backend, "vterm") == 0)
        sess->backend = TAPI_SHELL_BACKEND_VTERM;
    else
        sess->backend = 0;

    if (sess->backend == 0)
        return tapi_shell_available(sess->pco);

    return tapi_shell_supports(sess->pco, sess->backend);
}

/*
 * Each sequence stops, at the cut, in a state that decides where or how
 * the next bytes land: a filled last column, a moved margin, a changed
 * mode, a saved cursor.
 */
static const tsapi_shell_seq seqs[] = {
    /* The last column was just filled: the next character wraps. */
    { "wrap_pending", 4, 5, "abcd", "X" },
    /* ... then the cursor is moved left, which cancels the wrap. */
    { "cub_pending", 4, 5, "abcd\033[2D", "X" },
    { "cr_pending", 4, 5, "abcd\r", "X" },
    { "cha_pending", 4, 5, "abcd\033[2G", "X" },
    { "bs_pending", 4, 5, "abcd\b", "X" },
    /* ... or back-tabbed to the first column. */
    { "cbt_pending", 4, 5, "abcd\033[Z", "X" },
    /* A forward tab at the last column does not move. */
    { "ht_pending", 4, 5, "abcd\t", "X" },
    /* The cursor is saved while a wrap is pending and restored later. */
    { "decrc_pending", 4, 5, "abcd\0337\033[1;1H\0338", "X" },
    /* A wide glyph fills the last two columns. */
    { "wide_pending", 4, 3, "ab\xe4\xb8\xad", "X" },
    /* A wide glyph does not fit in the one column that is left. */
    { "wide_split", 4, 3, "abc", "\xe4\xb8\xad" "d" },
    /* Autowrap is off: output piles up in the last column. */
    { "noautowrap", 4, 3, "\033[?7labcd", "XY" },
    /* A scrolling region confines the scroll that follows. */
    { "scroll_region", 10, 5,
      "\033[2;4r\033[4;1Hone\r\ntwo\r\nthree", "\r\nfour\r\nfive" },
    /* Origin mode makes cursor addresses relative to the region. */
    { "origin", 10, 5, "\033[2;4r\033[?6h", "\033[1;1HX\033[9;9HY" },
    /* Left and right margins confine the line that follows. */
    { "margins", 10, 3, "\033[?69h\033[3;7s\033[1;3H", "abcdefgh" },
    /* Custom tab stops decide where the tabs that follow go. */
    { "tabstops", 20, 3,
      "\033[3g\033[1;5H\033H\033[1;11H\033H\033[1;1H", "a\tb\tc" },
    /* Insert mode shifts the row instead of overwriting it. */
    { "insert", 10, 3, "abcdef\033[4h\033[1;3H", "XY" },
    /* The line-drawing character set is selected. */
    { "charset", 10, 3, "\033(0", "lqk" },
    /* Colours and attributes are set for the text that follows. */
    { "sgr", 10, 3, "\033[1;4;31;44m", "styled\033[0m plain" },
    /* The alternate screen is shown; leaving it brings the main one back. */
    { "altscreen", 10, 3, "main\033[?1049h", "alt\033[?1049lX" },
    /* Lines have scrolled off the top into the scrollback. */
    { "scrollback", 6, 2, "one\r\ntwo\r\nthree\r\nfour", "\r\nfive" },
    /* The cursor is hidden. */
    { "cursor_hidden", 10, 3, "ab\033[?25l", "cd" },
    /* An escape sequence is cut in the middle. */
    { "split_csi", 10, 3, "ab\033[1;", "3HX" },
    /* A UTF-8 character is cut in the middle. */
    { "split_utf8", 10, 3, "ab\xe4\xb8", "\xad" "c" },
};

/* See description in tsapi_shell.h */
const tsapi_shell_seq *
tsapi_shell_seq_find(const char *name)
{
    size_t i;

    for (i = 0; i < TE_ARRAY_LEN(seqs); i++)
    {
        if (strcmp(seqs[i].name, name) == 0)
            return &seqs[i];
    }

    return NULL;
}

/* See description in tsapi_shell.h */
te_errno
tsapi_shell_state(tapi_shell_session *sh, te_string *out)
{
    tapi_shell_screen_t screen;
    te_string           part = TE_STRING_INIT;
    te_errno            rc;
    int                 y;

    memset(&screen, 0, sizeof(screen));
    rc = tapi_shell_screen(sh, &screen);
    if (rc != 0)
        return rc;

    te_string_append(out, "size %dx%d cursor (%d,%d) %s wrap %s\n",
                     screen.cols, screen.rows, screen.cursor_x,
                     screen.cursor_y,
                     screen.cursor_visible ? "shown" : "hidden",
                     screen.pending_wrap < 0 ? "unknown" :
                     screen.pending_wrap > 0 ? "pending" : "none");
    tapi_shell_screen_free(&screen);

    for (y = 0; y < screen.rows && rc == 0; y++)
    {
        rc = tapi_shell_row_text(sh, y, &part);
        if (rc != 0)
            break;
        te_string_append(out, "row %d '%s'\n", y,
                         part.ptr != NULL ? part.ptr : "");
        te_string_reset(&part);

        rc = tapi_shell_row_attrs(sh, y, &part);
        if (rc != 0)
            break;
        te_string_append(out, "%s\n", part.ptr != NULL ? part.ptr : "");
        te_string_reset(&part);
    }

    if (rc == 0)
    {
        rc = tapi_shell_scrollback(sh, 0, &part);
        if (rc == 0)
            te_string_append(out, "scrollback '%s'\n",
                             part.ptr != NULL ? part.ptr : "");
    }

    te_string_free(&part);
    return rc;
}

/* See description in tsapi_shell.h */
void
tsapi_shell_sess_fini(tsapi_shell_sess *sess)
{
    if (sess->pco != NULL)
    {
        rcf_rpc_server_destroy(sess->pco);
        sess->pco = NULL;
    }
}
