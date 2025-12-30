#include "feather.h"
#include "error_trace.h"
#include "host.h"
#include "internal.h"
#include "namespace_util.h"

#define S(lit) (lit), feather_strlen(lit)

// Get error namespace variable
static FeatherObj get_error_var(const FeatherHostOps *ops, FeatherInterp interp,
                                const char *name) {
    FeatherObj ns = ops->string.intern(interp, S("::tcl::errors"));
    FeatherObj varName = ops->string.intern(interp, name, feather_strlen(name));
    return ops->ns.get_var(interp, ns, varName);
}

// Set error namespace variable
static void set_error_var(const FeatherHostOps *ops, FeatherInterp interp,
                          const char *name, FeatherObj value) {
    FeatherObj ns = ops->string.intern(interp, S("::tcl::errors"));
    FeatherObj varName = ops->string.intern(interp, name, feather_strlen(name));
    ops->ns.set_var(interp, ns, varName, value);
}

int feather_error_is_active(const FeatherHostOps *ops, FeatherInterp interp) {
    ops = feather_get_ops(ops);
    FeatherObj val = get_error_var(ops, interp, "active");
    return val != 0 && feather_obj_eq_literal(ops, interp, val, "1");
}

void feather_error_init(const FeatherHostOps *ops, FeatherInterp interp,
                        FeatherObj message, FeatherObj cmd, FeatherObj args) {
    ops = feather_get_ops(ops);

    // Get display name (strip :: prefix for global namespace commands)
    FeatherObj displayCmd = feather_get_display_name(ops, interp, cmd);

    // Set active = "1"
    set_error_var(ops, interp, "active", ops->string.intern(interp, S("1")));

    // Build initial errorinfo: "message\n    while executing\n\"cmd args...\""
    FeatherObj builder = ops->string.builder_new(interp, 256);
    ops->string.builder_append_obj(interp, builder, message);
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    while executing\n\"")));
    ops->string.builder_append_obj(interp, builder, displayCmd);

    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_obj(interp, builder, ops->list.at(interp, args, i));
    }
    ops->string.builder_append_byte(interp, builder, '"');

    set_error_var(ops, interp, "info", ops->string.builder_finish(interp, builder));

    // Initialize errorstack: {INNER {cmd args...}}
    FeatherObj stack = ops->list.create(interp);
    stack = ops->list.push(interp, stack, ops->string.intern(interp, S("INNER")));

    FeatherObj callEntry = ops->list.create(interp);
    callEntry = ops->list.push(interp, callEntry, displayCmd);
    for (size_t i = 0; i < argc; i++) {
        callEntry = ops->list.push(interp, callEntry, ops->list.at(interp, args, i));
    }
    stack = ops->list.push(interp, stack, callEntry);
    set_error_var(ops, interp, "stack", stack);

    // Set errorline from current frame
    size_t line = ops->frame.get_line(interp, ops->frame.level(interp));
    set_error_var(ops, interp, "line", ops->integer.create(interp, (int64_t)line));
}

void feather_error_append_frame(const FeatherHostOps *ops, FeatherInterp interp,
                                FeatherObj procName, FeatherObj args, size_t line) {
    ops = feather_get_ops(ops);

    // Get display name (strip :: prefix for global namespace commands)
    FeatherObj displayName = feather_get_display_name(ops, interp, procName);

    // Append to errorinfo
    FeatherObj currentInfo = get_error_var(ops, interp, "info");

    FeatherObj builder = ops->string.builder_new(interp, 256);
    ops->string.builder_append_obj(interp, builder, currentInfo);

    // "\n    (procedure \"procName\" line N)"
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    (procedure \"")));
    ops->string.builder_append_obj(interp, builder, displayName);
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\" line ")));

    // Convert line number to string
    char lineBuf[32];
    size_t lineLen = 0;
    size_t tmp = line;
    if (tmp == 0) {
        lineBuf[lineLen++] = '0';
    } else {
        while (tmp > 0) {
            lineBuf[lineLen++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        // Reverse
        for (size_t i = 0; i < lineLen / 2; i++) {
            char c = lineBuf[i];
            lineBuf[i] = lineBuf[lineLen - 1 - i];
            lineBuf[lineLen - 1 - i] = c;
        }
    }
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, lineBuf, lineLen));
    ops->string.builder_append_byte(interp, builder, ')');

    // "\n    invoked from within\n\"procName args...\""
    ops->string.builder_append_obj(interp, builder,
        ops->string.intern(interp, S("\n    invoked from within\n\"")));
    ops->string.builder_append_obj(interp, builder, displayName);

    size_t argc = ops->list.length(interp, args);
    for (size_t i = 0; i < argc; i++) {
        ops->string.builder_append_byte(interp, builder, ' ');
        ops->string.builder_append_obj(interp, builder, ops->list.at(interp, args, i));
    }
    ops->string.builder_append_byte(interp, builder, '"');

    set_error_var(ops, interp, "info", ops->string.builder_finish(interp, builder));

    // Append CALL to errorstack
    FeatherObj stack = get_error_var(ops, interp, "stack");
    stack = ops->list.push(interp, stack, ops->string.intern(interp, S("CALL")));

    FeatherObj callEntry = ops->list.create(interp);
    callEntry = ops->list.push(interp, callEntry, displayName);
    for (size_t i = 0; i < argc; i++) {
        callEntry = ops->list.push(interp, callEntry, ops->list.at(interp, args, i));
    }
    stack = ops->list.push(interp, stack, callEntry);
    set_error_var(ops, interp, "stack", stack);
}

void feather_error_finalize(const FeatherHostOps *ops, FeatherInterp interp) {
    ops = feather_get_ops(ops);

    // Get accumulated state
    FeatherObj info = get_error_var(ops, interp, "info");
    FeatherObj stack = get_error_var(ops, interp, "stack");
    FeatherObj line = get_error_var(ops, interp, "line");

    // Get current return options and add error fields
    FeatherObj opts = ops->interp.get_return_options(interp, TCL_ERROR);
    if (ops->list.is_nil(interp, opts)) {
        opts = ops->list.create(interp);
        opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-code")));
        opts = ops->list.push(interp, opts, ops->integer.create(interp, 1));
    }

    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorinfo")));
    opts = ops->list.push(interp, opts, info);
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorstack")));
    opts = ops->list.push(interp, opts, stack);
    opts = ops->list.push(interp, opts, ops->string.intern(interp, S("-errorline")));
    opts = ops->list.push(interp, opts, line);

    ops->interp.set_return_options(interp, opts);

    // Set global ::errorInfo variable
    FeatherObj globalNs = ops->string.intern(interp, S("::"));
    ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorInfo")), info);

    // Set global ::errorCode variable (from opts or default NONE)
    // Check if -errorcode is already in opts
    FeatherObj errorCode = ops->string.intern(interp, S("NONE"));
    size_t optsLen = ops->list.length(interp, opts);
    for (size_t i = 0; i + 1 < optsLen; i += 2) {
        FeatherObj key = ops->list.at(interp, opts, i);
        if (feather_obj_eq_literal(ops, interp, key, "-errorcode")) {
            errorCode = ops->list.at(interp, opts, i + 1);
            break;
        }
    }
    ops->ns.set_var(interp, globalNs, ops->string.intern(interp, S("errorCode")), errorCode);

    // Clear error state
    set_error_var(ops, interp, "active", ops->string.intern(interp, S("0")));
}
