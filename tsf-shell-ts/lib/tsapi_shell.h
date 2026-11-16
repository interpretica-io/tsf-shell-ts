/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/** @file
 * @brief Suite helpers
 *
 * Every test here needs the same thing: an agent with an RPC server on
 * it (tapi_shell runs the terminal emulator in that server) and a way
 * to start a known program under a PTY. This makes the session and
 * offers a small shortcut for the common case, a shell running a fixed
 * script, so a test's oracle is the script it wrote - the escape
 * sequences and the text are known exactly and asserted against.
 *
 * Nothing is committed to the repository and nothing runs on the DUT
 * but the program the test spawns: the emulation is a library linked
 * into the agent's RPC server.
 *
 * @author Maxim Menshikov <maxim.menshikov@interpretica.io>
 */

#ifndef __TSAPI_SHELL_H__
#define __TSAPI_SHELL_H__

#include "rcf_rpc.h"
#include "te_errno.h"
#include "te_string.h"

#include "tapi_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The agent this suite works on. */
#define TSAPI_SHELL_TA   "Agt_A"

/** What a test needs: an agent and an RPC server on it. */
typedef struct tsapi_shell_sess {
    const char     *ta;    /**< Agent name. */
    rcf_rpc_server *pco;    /**< RPC server on it - what tapi_shell takes. */
    tapi_shell_backend backend; /**< Emulator backend the sessions use;
                                     0 is the agent's preferred one. */
} tsapi_shell_sess;

/**
 * Open a session: an RPC server on the agent.
 *
 * @param[out] sess  Session.
 * @param[in]  name  Name for the RPC server, unique within the test.
 *
 * @return Status code.
 */
extern te_errno tsapi_shell_sess_init(tsapi_shell_sess *sess,
                                      const char *name);

/**
 * Choose the terminal-emulator backend the session's terminals use.
 *
 * Every test takes the backend as a parameter, so one test checks each
 * emulator the agent carries and a difference between them shows up as
 * a difference between two iterations.
 *
 * @param sess     Session.
 * @param backend  @c "ghostty", @c "vterm", or @c "any" for the agent's
 *                 preferred one.
 *
 * @return @c true when the agent has that backend; when it does not,
 *         the test has nothing to run on and skips.
 */
extern bool tsapi_shell_sess_select(tsapi_shell_sess *sess,
                                    const char *backend);

/**
 * Open a terminal session on an idle program, to drive the emulator
 * itself: nothing is printed by the program, the test feeds the bytes
 * to render with tapi_shell_feed().
 *
 * @param[in]  sess    Session whose RPC server to run on.
 * @param[in]  cols    Columns (<=0 -> 80).
 * @param[in]  rows    Rows (<=0 -> 24).
 * @param[out] sh      The opened terminal session.
 *
 * @return Status code.
 */
extern te_errno tsapi_shell_open_idle(tsapi_shell_sess *sess, int cols,
                                      int rows, tapi_shell_session *sh);

/**
 * Open a terminal session running @c "sh -c <script>" under a PTY.
 *
 * A shortcut over tapi_shell_open() for the common oracle: a shell that
 * runs a fixed script (printing known escape sequences, or reading and
 * echoing). The script is the test's ground truth.
 *
 * @param[in]  sess    Session whose RPC server to run on.
 * @param[in]  script  The @c sh @c -c argument.
 * @param[in]  cols    Columns (<=0 -> 80).
 * @param[in]  rows    Rows (<=0 -> 24).
 * @param[out] sh      The opened terminal session.
 *
 * @return Status code.
 */
extern te_errno tsapi_shell_open_sh(tsapi_shell_sess *sess, const char *script,
                                    int cols, int rows,
                                    tapi_shell_session *sh);

/**
 * A byte sequence for the emulator, cut in two: what brings the
 * terminal into a state, and what is printed from that state. The cut
 * is where a test does something that must not change the outcome
 * (take a snapshot, change the chunking).
 */
typedef struct tsapi_shell_seq {
    const char *name;   /**< Name, as the test parameter gives it. */
    int         cols;   /**< Screen columns. */
    int         rows;   /**< Screen rows. */
    const char *before; /**< Bytes that set the state up. */
    const char *after;  /**< Bytes printed from that state. */
} tsapi_shell_seq;

/**
 * Find a sequence by name.
 *
 * @param name  Sequence name.
 *
 * @return The sequence, or @c NULL when there is none with this name.
 */
extern const tsapi_shell_seq *tsapi_shell_seq_find(const char *name);

/**
 * Describe everything a test can see of a terminal as one string: the
 * size, the cursor (position, visibility, pending wrap), every row's
 * text and cell attributes, and the scrollback. Two terminals that
 * give the same string look the same.
 *
 * @param[in]  sh   Terminal session.
 * @param[out] out  The description is appended here.
 *
 * @return Status code.
 */
extern te_errno tsapi_shell_state(tapi_shell_session *sh, te_string *out);

/**
 * Close a session.
 *
 * @param sess  Session.
 */
extern void tsapi_shell_sess_fini(tsapi_shell_sess *sess);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TSAPI_SHELL_H__ */
