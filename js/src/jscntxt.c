/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS execution context.
 */
#include "jsstddef.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsprf.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsexn.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscan.h"
#include "jsscript.h"
#include "jsstr.h"

JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    JSContext *cx;
    JSBool ok, first;

    cx = (JSContext *) malloc(sizeof *cx);
    if (!cx)
        return NULL;
    memset(cx, 0, sizeof *cx);

    cx->runtime = rt;
#ifdef JS_THREADSAFE
    js_InitContextForLocking(cx);
#endif

    JS_LOCK_GC(rt);
    for (;;) {
        first = (rt->contextList.next == &rt->contextList);
        if (rt->state == JSRTS_UP) {
            JS_ASSERT(!first);
            break;
        }
        if (rt->state == JSRTS_DOWN) {
            JS_ASSERT(first);
            rt->state = JSRTS_LAUNCHING;
            break;
        }
        JS_WAIT_CONDVAR(rt->stateChange, JS_NO_TIMEOUT);
    }
    JS_APPEND_LINK(&cx->links, &rt->contextList);
    JS_UNLOCK_GC(rt);

    /*
     * First we do the infallible, every-time per-context initializations.
     * Should a later, fallible initialization (js_InitRegExpStatics, e.g.,
     * or the stuff under 'if (first)' below) fail, at least the version
     * and arena-pools will be valid and safe to use (say, from the last GC
     * done by js_DestroyContext).
     */
    cx->version = JSVERSION_DEFAULT;
    cx->jsop_eq = JSOP_EQ;
    cx->jsop_ne = JSOP_NE;
    JS_InitArenaPool(&cx->stackPool, "stack", stackChunkSize, sizeof(jsval));
    JS_InitArenaPool(&cx->codePool, "code", 1024, sizeof(jsbytecode));
    JS_InitArenaPool(&cx->notePool, "note", 1024, sizeof(jssrcnote));
    JS_InitArenaPool(&cx->tempPool, "temp", 1024, sizeof(jsdouble));

#if JS_HAS_REGEXPS
    if (!js_InitRegExpStatics(cx, &cx->regExpStatics)) {
        js_DestroyContext(cx, JS_NO_GC);
        return NULL;
    }
#endif
#if JS_HAS_EXCEPTIONS
    cx->throwing = JS_FALSE;
#endif

    /*
     * If cx is the first context on this runtime, initialize well-known atoms,
     * keywords, numbers, and strings.  If one of these steps should fail, the
     * runtime will be left in a partially initialized state, with zeroes and
     * nulls stored in the default-initialized remainder of the struct.  We'll
     * clean the runtime up under js_DestroyContext, because cx will be "last"
     * as well as "first".
     */
    if (first) {
        ok = (rt->atomState.liveAtoms == 0)
             ? js_InitAtomState(cx, &rt->atomState)
             : js_InitPinnedAtoms(cx, &rt->atomState);
        if (ok)
            ok = js_InitScanner(cx);
        if (ok)
            ok = js_InitRuntimeNumberState(cx);
        if (ok)
            ok = js_InitRuntimeStringState(cx);
        if (!ok) {
            js_DestroyContext(cx, JS_NO_GC);
            return NULL;
        }

        JS_LOCK_GC(rt);
        rt->state = JSRTS_UP;
        JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
        JS_UNLOCK_GC(rt);
    }

    return cx;
}

void
js_DestroyContext(JSContext *cx, JSGCMode gcmode)
{
    JSRuntime *rt;
    JSBool last;
    JSArgumentFormatMap *map;

    rt = cx->runtime;

    /* Remove cx from context list first. */
    JS_LOCK_GC(rt);
    JS_ASSERT(rt->state == JSRTS_UP || rt->state == JSRTS_LAUNCHING);
    JS_REMOVE_LINK(&cx->links);
    last = (rt->contextList.next == &rt->contextList);
    if (last)
        rt->state = JSRTS_LANDING;
    JS_UNLOCK_GC(rt);

    if (last) {
#ifdef JS_THREADSAFE
        /*
         * If cx is not in a request already, begin one now so that we wait
         * for any racing GC started on a not-last context to finish, before
         * we plow ahead and unpin atoms.  Note that even though we begin a
         * request here if necessary, we end all requests on cx below before
         * forcing a final GC.  This lets any not-last context destruction
         * racing in another thread try to force or maybe run the GC, but by
         * that point, rt->state will not be JSRTS_UP, and that GC attempt
         * will return early.
         */
        if (cx->requestDepth == 0)
            JS_BeginRequest(cx);
#endif

        /* Unpin all pinned atoms before final GC. */
        js_UnpinPinnedAtoms(&rt->atomState);

        /* Unlock and clear GC things held by runtime pointers. */
        js_FinishRuntimeNumberState(cx);
        js_FinishRuntimeStringState(cx);

        /* Clear debugging state to remove GC roots. */
        JS_ClearAllTraps(cx);
        JS_ClearAllWatchPoints(cx);
    }

#if JS_HAS_REGEXPS
    /*
     * Remove more GC roots in regExpStatics, then collect garbage.
     * XXX anti-modularity alert: we rely on the call to js_RemoveRoot within
     * XXX this function call to wait for any racing GC to complete, in the
     * XXX case where JS_DestroyContext is called outside of a request on cx
     */
    js_FreeRegExpStatics(cx, &cx->regExpStatics);
#endif

#ifdef JS_THREADSAFE
    /*
     * Destroying a context implicitly calls JS_EndRequest().  Also, we must
     * end our request here in case we are "last" -- in that event, another
     * js_DestroyContext that was not last might be waiting in the GC for our
     * request to end.  We'll let it run below, just before we do the truly
     * final GC and then free atom state.
     *
     * At this point, cx must be inaccessible to other threads.  It's off the
     * rt->contextList, and it should not be reachable via any object private
     * data structure.
     */
    while (cx->requestDepth != 0)
        JS_EndRequest(cx);
#endif

    if (last) {
        /* Always force, so we wait for any racing GC to finish. */
        js_ForceGC(cx, GC_LAST_CONTEXT);

        /* Iterate until no finalizer removes a GC root or lock. */
        while (rt->gcPoke)
            js_GC(cx, GC_LAST_CONTEXT);

        /* Try to free atom state, now that no unrooted scripts survive. */
        if (rt->atomState.liveAtoms == 0)
            js_FreeAtomState(cx, &rt->atomState);

        /* Take the runtime down, now that it has no contexts or atoms. */
        JS_LOCK_GC(rt);
        rt->state = JSRTS_DOWN;
        JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
        JS_UNLOCK_GC(rt);
    } else {
        if (gcmode == JS_FORCE_GC)
            js_ForceGC(cx, 0);
        else if (gcmode == JS_MAYBE_GC)
            JS_MaybeGC(cx);
    }

    /* Free the stuff hanging off of cx. */
    JS_FinishArenaPool(&cx->stackPool);
    JS_FinishArenaPool(&cx->codePool);
    JS_FinishArenaPool(&cx->notePool);
    JS_FinishArenaPool(&cx->tempPool);
    if (cx->lastMessage)
        free(cx->lastMessage);

    /* Remove any argument formatters. */
    map = cx->argumentFormatMap;
    while (map) {
        JSArgumentFormatMap *temp = map;
        map = map->next;
        JS_free(cx, temp);
    }

    /* Destroy the resolve recursion damper. */
    if (cx->resolvingTable) {
        JS_DHashTableDestroy(cx->resolvingTable);
        cx->resolvingTable = NULL;
    }

    /* Finally, free cx itself. */
    free(cx);
}

JSBool
js_ValidContextPointer(JSRuntime *rt, JSContext *cx)
{
    JSCList *cl;

    for (cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next) {
        if (cl == &cx->links)
            return JS_TRUE;
    }
    JS_RUNTIME_METER(rt, deadContexts);
    return JS_FALSE;
}

JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp)
{
    JSContext *cx = *iterp;

    if (unlocked)
        JS_LOCK_GC(rt);
    if (!cx)
        cx = (JSContext *)&rt->contextList;
    cx = (JSContext *)cx->links.next;
    if (&cx->links == &rt->contextList)
        cx = NULL;
    *iterp = cx;
    if (unlocked)
        JS_UNLOCK_GC(rt);
    return cx;
}

static void
ReportError(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    /*
     * Check the error report, and set a JavaScript-catchable exception
     * if the error is defined to have an associated exception.  If an
     * exception is thrown, then the JSREPORT_EXCEPTION flag will be set
     * on the error report, and exception-aware hosts should ignore it.
     */
    if (reportp && reportp->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)
        reportp->flags |= JSREPORT_EXCEPTION;

#if JS_HAS_ERROR_EXCEPTIONS
    /*
     * Call the error reporter only if an exception wasn't raised.
     *
     * If an exception was raised, then we call the debugErrorHook
     * (if present) to give it a chance to see the error before it
     * propagates out of scope.  This is needed for compatability
     * with the old scheme.
     */
    if (!js_ErrorToException(cx, message, reportp)) {
        js_ReportErrorAgain(cx, message, reportp);
    } else if (cx->runtime->debugErrorHook && cx->errorReporter) {
        JSDebugErrorHook hook = cx->runtime->debugErrorHook;
        /* test local in case debugErrorHook changed on another thread */
        if (hook)
            hook(cx, message, reportp, cx->runtime->debugErrorHookData);
    }
#else
    js_ReportErrorAgain(cx, message, reportp);
#endif
}

/*
 * We don't post an exception in this case, since doing so runs into
 * complications of pre-allocating an exception object which required
 * running the Exception class initializer early etc.
 * Instead we just invoke the errorReporter with an "Out Of Memory"
 * type message, and then hope the process ends swiftly.
 */
void
js_ReportOutOfMemory(JSContext *cx, JSErrorCallback callback)
{
    JSStackFrame *fp;
    JSErrorReport report;
    JSErrorReporter onError = cx->errorReporter;

    /* Get the message for this error, but we won't expand any arguments. */
    const JSErrorFormatString *efs = callback(NULL, NULL, JSMSG_OUT_OF_MEMORY);
    const char *msg = efs ? efs->format : "Out of memory";

    /* Fill out the report, but don't do anything that requires allocation. */
    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = JSREPORT_ERROR;
    report.errorNumber = JSMSG_OUT_OF_MEMORY;

    /*
     * Walk stack until we find a frame that is associated with some script
     * rather than a native frame.
     */
    for (fp = cx->fp; fp; fp = fp->down) {
        if (fp->script && fp->pc) {
            report.filename = fp->script->filename;
            report.lineno = js_PCToLineNumber(cx, fp->script, fp->pc);
            break;
        }
    }

    /*
     * If debugErrorHook is present then we give it a chance to veto
     * sending the error on to the regular ErrorReporter.
     */
    if (onError) {
        JSDebugErrorHook hook = cx->runtime->debugErrorHook;
        if (hook &&
            !hook(cx, msg, &report, cx->runtime->debugErrorHookData)) {
            onError = NULL;
        }
    }

    if (onError)
        onError(cx, msg, &report);
}

JSBool
js_ReportErrorVA(JSContext *cx, uintN flags, const char *format, va_list ap)
{
    char *last;
    JSStackFrame *fp;
    JSErrorReport report;
    JSBool warning;

    if ((flags & JSREPORT_STRICT) && !JS_HAS_STRICT_OPTION(cx))
        return JS_TRUE;

    last = JS_vsmprintf(format, ap);
    if (!last)
        return JS_FALSE;

    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = flags;

    /* Find the top-most active script frame, for best line number blame. */
    for (fp = cx->fp; fp; fp = fp->down) {
        if (fp->script && fp->pc) {
            report.filename = fp->script->filename;
            report.lineno = js_PCToLineNumber(cx, fp->script, fp->pc);
            break;
        }
    }

    warning = JSREPORT_IS_WARNING(report.flags);
    if (warning && JS_HAS_WERROR_OPTION(cx)) {
        report.flags &= ~JSREPORT_WARNING;
        warning = JS_FALSE;
    }

    ReportError(cx, last, &report);
    free(last);
    return warning;
}

/*
 * The arguments from ap need to be packaged up into an array and stored
 * into the report struct.
 *
 * The format string addressed by the error number may contain operands
 * identified by the format {N}, where N is a decimal digit. Each of these
 * is to be replaced by the Nth argument from the va_list. The complete
 * message is placed into reportp->ucmessage converted to a JSString.
 *
 * Returns true if the expansion succeeds (can fail if out of memory).
 */
JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const uintN errorNumber,
                        char **messagep, JSErrorReport *reportp,
                        JSBool *warningp, JSBool charArgs, va_list ap)
{
    const JSErrorFormatString *efs;
    int i;
    int argCount;

    *warningp = JSREPORT_IS_WARNING(reportp->flags);
    if (*warningp && JS_HAS_WERROR_OPTION(cx)) {
        reportp->flags &= ~JSREPORT_WARNING;
        *warningp = JS_FALSE;
    }

    *messagep = NULL;
    if (callback) {
        efs = callback(userRef, NULL, errorNumber);
        if (efs) {
            size_t totalArgsLength = 0;
            size_t argLengths[10]; /* only {0} thru {9} supported */
            argCount = efs->argCount;
            JS_ASSERT(argCount <= 10);
            if (argCount > 0) {
                /*
                 * Gather the arguments into an array, and accumulate
                 * their sizes. We allocate 1 more than necessary and
                 * null it out to act as the caboose when we free the
                 * pointers later.
                 */
                reportp->messageArgs = (const jschar **)
                    JS_malloc(cx, sizeof(jschar *) * (argCount + 1));
                if (!reportp->messageArgs)
                    return JS_FALSE;
                reportp->messageArgs[argCount] = NULL;
                for (i = 0; i < argCount; i++) {
                    if (charArgs) {
                        char *charArg = va_arg(ap, char *);
                        reportp->messageArgs[i]
                            = js_InflateString(cx, charArg, strlen(charArg));
                        if (!reportp->messageArgs[i])
                            goto error;
                    }
                    else
                        reportp->messageArgs[i] = va_arg(ap, jschar *);
                    argLengths[i] = js_strlen(reportp->messageArgs[i]);
                    totalArgsLength += argLengths[i];
                }
                /* NULL-terminate for easy copying. */
                reportp->messageArgs[i] = NULL;
            }
            /*
             * Parse the error format, substituting the argument X
             * for {X} in the format.
             */
            if (argCount > 0) {
                if (efs->format) {
                    const char *fmt;
                    const jschar *arg;
                    jschar *out;
                    int expandedArgs = 0;
                    size_t expandedLength
                        = strlen(efs->format)
                            - (3 * argCount) /* exclude the {n} */
                            + totalArgsLength;
                    /*
                     * Note - the above calculation assumes that each argument
                     * is used once and only once in the expansion !!!
                     */
                    reportp->ucmessage = out = (jschar *)
                        JS_malloc(cx, (expandedLength + 1) * sizeof(jschar));
                    if (!out)
                        goto error;
                    fmt = efs->format;
                    while (*fmt) {
                        if (*fmt == '{') {
                            if (isdigit(fmt[1])) {
                                int d = JS7_UNDEC(fmt[1]);
                                JS_ASSERT(expandedArgs < argCount);
                                arg = reportp->messageArgs[d];
                                js_strncpy(out, arg, argLengths[d]);
                                out += argLengths[d];
                                fmt += 3;
                                expandedArgs++;
                                continue;
                            }
                        }
                        /*
                         * is this kosher?
                         */
                        *out++ = (unsigned char)(*fmt++);
                    }
                    JS_ASSERT(expandedArgs == argCount);
                    *out = 0;
                    *messagep =
                        js_DeflateString(cx, reportp->ucmessage,
                                         (size_t)(out - reportp->ucmessage));
                    if (!*messagep)
                        goto error;
                }
            } else {
                /*
                 * Zero arguments: the format string (if it exists) is the
                 * entire message.
                 */
                if (efs->format) {
                    *messagep = JS_strdup(cx, efs->format);
                    if (!*messagep)
                        goto error;
                    reportp->ucmessage
                        = js_InflateString(cx, *messagep, strlen(*messagep));
                    if (!reportp->ucmessage)
                        goto error;
                }
            }
        }
    }
    if (*messagep == NULL) {
        /* where's the right place for this ??? */
        const char *defaultErrorMessage
            = "No error message available for error number %d";
        size_t nbytes = strlen(defaultErrorMessage) + 16;
        *messagep = (char *)JS_malloc(cx, nbytes);
        if (!*messagep)
            goto error;
        JS_snprintf(*messagep, nbytes, defaultErrorMessage, errorNumber);
    }
    return JS_TRUE;

error:
    if (reportp->messageArgs) {
        i = 0;
        while (reportp->messageArgs[i])
            JS_free(cx, (void *)reportp->messageArgs[i++]);
        JS_free(cx, (void *)reportp->messageArgs);
        reportp->messageArgs = NULL;
    }
    if (reportp->ucmessage) {
        JS_free(cx, (void *)reportp->ucmessage);
        reportp->ucmessage = NULL;
    }
    if (*messagep) {
        JS_free(cx, (void *)*messagep);
        *messagep = NULL;
    }
    return JS_FALSE;
}

JSBool
js_ReportErrorNumberVA(JSContext *cx, uintN flags, JSErrorCallback callback,
                       void *userRef, const uintN errorNumber,
                       JSBool charArgs, va_list ap)
{
    JSStackFrame *fp;
    JSErrorReport report;
    char *message;
    JSBool warning;

    if ((flags & JSREPORT_STRICT) && !JS_HAS_STRICT_OPTION(cx))
        return JS_TRUE;

    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = flags;
    report.errorNumber = errorNumber;

    /*
     * If we can't find out where the error was based on the current frame,
     * see if the next frame has a script/pc combo we can use.
     */
    for (fp = cx->fp; fp; fp = fp->down) {
        if (fp->script && fp->pc) {
            report.filename = fp->script->filename;
            report.lineno = js_PCToLineNumber(cx, fp->script, fp->pc);
            break;
        }
    }

    if (!js_ExpandErrorArguments(cx, callback, userRef, errorNumber,
                                 &message, &report, &warning, charArgs, ap)) {
        return JS_FALSE;
    }

    ReportError(cx, message, &report);

    if (message)
        JS_free(cx, message);
    if (report.messageArgs) {
        int i = 0;
        while (report.messageArgs[i])
            JS_free(cx, (void *)report.messageArgs[i++]);
        JS_free(cx, (void *)report.messageArgs);
    }
    if (report.ucmessage)
        JS_free(cx, (void *)report.ucmessage);

    return warning;
}

JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    JSErrorReporter onError;

    if (!message)
        return;

    if (cx->lastMessage)
        free(cx->lastMessage);
    cx->lastMessage = JS_strdup(cx, message);
    if (!cx->lastMessage)
        return;
    onError = cx->errorReporter;

    /*
     * If debugErrorHook is present then we give it a chance to veto
     * sending the error on to the regular ErrorReporter.
     */
    if (onError) {
        JSDebugErrorHook hook = cx->runtime->debugErrorHook;
        if (hook &&
            !hook(cx, cx->lastMessage, reportp,
                  cx->runtime->debugErrorHookData)) {
            onError = NULL;
        }
    }
    if (onError)
        onError(cx, cx->lastMessage, reportp);
}

void
js_ReportIsNotDefined(JSContext *cx, const char *name)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_DEFINED, name);
}

#if defined DEBUG && defined XP_UNIX
/* For gdb usage. */
void js_traceon(JSContext *cx)  { cx->tracefp = stderr; }
void js_traceoff(JSContext *cx) { cx->tracefp = NULL; }
#endif

JSErrorFormatString js_ErrorFormatString[JSErr_Limit] = {
#if JS_HAS_DFLT_MSG_STRINGS
#define MSG_DEF(name, number, count, exception, format) \
    { format, count } ,
#else
#define MSG_DEF(name, number, count, exception, format) \
    { NULL, count } ,
#endif
#include "js.msg"
#undef MSG_DEF
};

const JSErrorFormatString *
js_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber)
{
    if ((errorNumber > 0) && (errorNumber < JSErr_Limit))
        return &js_ErrorFormatString[errorNumber];
    return NULL;
}
