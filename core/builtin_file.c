/*
 * builtin_file.c - TCL file Command Implementation
 *
 * Implements all file subcommands for TCL.
 */

#include "internal.h"

TclResult tclCmdFile(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"file subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* ===== file atime ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "atime", 5) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"file atime name ?time?\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int64_t atime = host->fileAtime(path);
        if (atime < 0) {
            tclSetError(interp, "could not get access time", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newInt(atime));
        return TCL_OK;
    }

    /* ===== file channels ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "channels", 8) == 0) {
        const char *pattern = NULL;
        if (objc >= 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }
        TclObj *result = host->chanNames(interp->hostCtx, pattern);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file copy ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "copy", 4) == 0) {
        int force = 0;
        int argIdx = 2;

        while (argIdx < objc - 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 6 && tclStrncmp(opt, "-force", 6) == 0) {
                force = 1;
                argIdx++;
            } else if (optLen == 2 && tclStrncmp(opt, "--", 2) == 0) {
                argIdx++;
                break;
            } else {
                break;
            }
        }

        if (objc - argIdx < 2) {
            tclSetError(interp, "wrong # args: should be \"file copy ?-force? ?--? source target\"", -1);
            return TCL_ERROR;
        }

        size_t srcLen, dstLen;
        const char *src = host->getStringPtr(objv[argIdx], &srcLen);
        const char *dst = host->getStringPtr(objv[argIdx + 1], &dstLen);

        if (host->fileCopy(src, dst, force) != 0) {
            tclSetError(interp, "error copying file", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* ===== file delete ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "delete", 6) == 0) {
        int force = 0;
        int argIdx = 2;

        while (argIdx < objc) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 6 && tclStrncmp(opt, "-force", 6) == 0) {
                force = 1;
                argIdx++;
            } else if (optLen == 2 && tclStrncmp(opt, "--", 2) == 0) {
                argIdx++;
                break;
            } else {
                break;
            }
        }

        for (int i = argIdx; i < objc; i++) {
            size_t pathLen;
            const char *path = host->getStringPtr(objv[i], &pathLen);
            host->fileDelete(path, force);
        }
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* ===== file dirname ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "dirname", 7) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file dirname name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileDirname(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file executable ===== */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "executable", 10) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file executable name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileExecutable(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file exists ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "exists", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file exists name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileExists(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file extension ===== */
    if (subcmdLen == 9 && tclStrncmp(subcmd, "extension", 9) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file extension name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileExtension(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file home ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "home", 4) == 0) {
        const char *user = NULL;
        if (objc >= 3) {
            size_t userLen;
            user = host->getStringPtr(objv[2], &userLen);
        }
        TclObj *result = host->fileHome(user);
        if (!result) {
            tclSetError(interp, "couldn't find home directory", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file isdirectory ===== */
    if (subcmdLen == 11 && tclStrncmp(subcmd, "isdirectory", 11) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file isdirectory name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileIsDir(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file isfile ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "isfile", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file isfile name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileIsFile(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file join ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "join", 4) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"file join name ?name ...?\"", -1);
            return TCL_ERROR;
        }
        TclObj *result = host->fileJoin(&objv[2], objc - 2);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file link ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "link", 4) == 0) {
        int linkType = TCL_LINK_SYMBOLIC;  /* Default */
        int argIdx = 2;

        if (objc >= 3) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 9 && tclStrncmp(opt, "-symbolic", 9) == 0) {
                linkType = TCL_LINK_SYMBOLIC;
                argIdx++;
            } else if (optLen == 5 && tclStrncmp(opt, "-hard", 5) == 0) {
                linkType = TCL_LINK_HARD;
                argIdx++;
            }
        }

        if (objc - argIdx == 1) {
            /* Read link value */
            size_t linkLen;
            const char *linkName = host->getStringPtr(objv[argIdx], &linkLen);
            TclObj *result = host->fileReadlink(linkName);
            if (!result) {
                tclSetError(interp, "could not read link", -1);
                return TCL_ERROR;
            }
            tclSetResult(interp, result);
            return TCL_OK;
        } else if (objc - argIdx == 2) {
            /* Create link */
            size_t linkLen, targetLen;
            const char *linkName = host->getStringPtr(objv[argIdx], &linkLen);
            const char *target = host->getStringPtr(objv[argIdx + 1], &targetLen);
            if (host->fileLink(linkName, target, linkType) != 0) {
                tclSetError(interp, "could not create link", -1);
                return TCL_ERROR;
            }
            tclSetResult(interp, host->newString(target, targetLen));
            return TCL_OK;
        } else {
            tclSetError(interp, "wrong # args: should be \"file link ?-symbolic|-hard? linkName ?target?\"", -1);
            return TCL_ERROR;
        }
    }

    /* ===== file lstat ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "lstat", 5) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"file lstat name ?varName?\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileLstat(path);
        if (!result) {
            tclSetError(interp, "could not lstat file", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file mkdir ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "mkdir", 5) == 0) {
        for (int i = 2; i < objc; i++) {
            size_t pathLen;
            const char *path = host->getStringPtr(objv[i], &pathLen);
            if (host->fileMkdir(path) != 0) {
                tclSetError(interp, "couldn't create directory", -1);
                return TCL_ERROR;
            }
        }
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* ===== file mtime ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "mtime", 5) == 0) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"file mtime name ?time?\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int64_t mtime = host->fileMtime(path);
        if (mtime < 0) {
            tclSetError(interp, "could not get modification time", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newInt(mtime));
        return TCL_OK;
    }

    /* ===== file nativename ===== */
    if (subcmdLen == 10 && tclStrncmp(subcmd, "nativename", 10) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file nativename name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileNativename(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file normalize ===== */
    if (subcmdLen == 9 && tclStrncmp(subcmd, "normalize", 9) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file normalize name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileNormalize(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file owned ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "owned", 5) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file owned name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileOwned(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file pathtype ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "pathtype", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file pathtype name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int pt = host->filePathtype(path);
        const char *result;
        switch (pt) {
            case TCL_PATH_ABSOLUTE: result = "absolute"; break;
            case TCL_PATH_RELATIVE: result = "relative"; break;
            case TCL_PATH_VOLUMERELATIVE: result = "volumerelative"; break;
            default: result = "relative"; break;
        }
        tclSetResult(interp, host->newString(result, tclStrlen(result)));
        return TCL_OK;
    }

    /* ===== file readable ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "readable", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file readable name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileReadable(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* ===== file readlink ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "readlink", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file readlink name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileReadlink(path);
        if (!result) {
            tclSetError(interp, "could not read link", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file rename ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "rename", 6) == 0) {
        int force = 0;
        int argIdx = 2;

        while (argIdx < objc - 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[argIdx], &optLen);
            if (optLen == 6 && tclStrncmp(opt, "-force", 6) == 0) {
                force = 1;
                argIdx++;
            } else if (optLen == 2 && tclStrncmp(opt, "--", 2) == 0) {
                argIdx++;
                break;
            } else {
                break;
            }
        }

        if (objc - argIdx < 2) {
            tclSetError(interp, "wrong # args: should be \"file rename ?-force? ?--? source target\"", -1);
            return TCL_ERROR;
        }

        size_t srcLen, dstLen;
        const char *src = host->getStringPtr(objv[argIdx], &srcLen);
        const char *dst = host->getStringPtr(objv[argIdx + 1], &dstLen);

        if (host->fileRename(src, dst, force) != 0) {
            tclSetError(interp, "error renaming file", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* ===== file rootname ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "rootname", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file rootname name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileRootname(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file separator ===== */
    if (subcmdLen == 9 && tclStrncmp(subcmd, "separator", 9) == 0) {
        TclObj *result = host->fileSeparator();
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file size ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "size", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file size name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int64_t size = host->fileSize(path);
        if (size < 0) {
            tclSetError(interp, "could not read file: no such file or directory", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, host->newInt(size));
        return TCL_OK;
    }

    /* ===== file split ===== */
    if (subcmdLen == 5 && tclStrncmp(subcmd, "split", 5) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file split name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileSplit(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file stat ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "stat", 4) == 0) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"file stat name ?varName?\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileStat(path);
        if (!result) {
            tclSetError(interp, "could not stat file", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file system ===== */
    if (subcmdLen == 6 && tclStrncmp(subcmd, "system", 6) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file system name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileSystem(path);
        if (!result) {
            tclSetError(interp, "could not get file system", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file tail ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "tail", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file tail name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileTail(path);
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file tempdir ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "tempdir", 7) == 0) {
        const char *tmpl = NULL;
        if (objc >= 3) {
            size_t tmplLen;
            tmpl = host->getStringPtr(objv[2], &tmplLen);
        }
        TclObj *result = host->fileTempdir(tmpl);
        if (!result) {
            tclSetError(interp, "couldn't create temporary directory", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file tempfile ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "tempfile", 8) == 0) {
        const char *tmpl = NULL;
        if (objc >= 3) {
            size_t tmplLen;
            tmpl = host->getStringPtr(objv[2], &tmplLen);
        }
        TclObj *result = host->fileTempfile(interp->hostCtx, tmpl);
        if (!result) {
            tclSetError(interp, "couldn't create temporary file", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file type ===== */
    if (subcmdLen == 4 && tclStrncmp(subcmd, "type", 4) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file type name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        TclObj *result = host->fileType(path);
        if (!result) {
            tclSetError(interp, "could not get file type", -1);
            return TCL_ERROR;
        }
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file volumes ===== */
    if (subcmdLen == 7 && tclStrncmp(subcmd, "volumes", 7) == 0) {
        TclObj *result = host->fileVolumes();
        tclSetResult(interp, result);
        return TCL_OK;
    }

    /* ===== file writable ===== */
    if (subcmdLen == 8 && tclStrncmp(subcmd, "writable", 8) == 0) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"file writable name\"", -1);
            return TCL_ERROR;
        }
        size_t pathLen;
        const char *path = host->getStringPtr(objv[2], &pathLen);
        int result = host->fileWritable(path);
        tclSetResult(interp, host->newInt(result));
        return TCL_OK;
    }

    /* Unknown subcommand */
    tclSetError(interp, "unknown or ambiguous subcommand: must be atime, channels, copy, delete, dirname, executable, exists, extension, home, isdirectory, isfile, join, link, lstat, mkdir, mtime, nativename, normalize, owned, pathtype, readable, readlink, rename, rootname, separator, size, split, stat, system, tail, tempdir, tempfile, type, volumes, or writable", -1);
    return TCL_ERROR;
}
