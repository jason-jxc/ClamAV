/*
 *  Copyright (C) 2002 - 2004 Tomasz Kojm <tkojm@clamav.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "options.h"
#include "others.h"
#include "shared.h"
#include "manager.h"
#include "defaults.h"
#include "treewalk.h"

#include "output.h"

#ifdef C_LINUX
#include <sys/resource.h>
#endif

void help(void);

struct s_info claminfo;
short recursion = 0, printinfected = 0, bell = 0;

int clamscan(struct optstruct *opt)
{
	int ds, dms, ret;
	double mb;
	struct timeval t1, t2;
	struct timezone tz;
	time_t starttime;


    /* initialize some important variables */

    if(optc(opt, 'v')) {
	mprintf_verbose = 1;
	logg_verbose = 1;
    }

    if(optl(opt, "quiet"))
	mprintf_quiet = 1;

    if(optl(opt, "stdout"))
	mprintf_stdout = 1;

    if(optl(opt, "debug")) {
#if defined(C_LINUX)
	    /* njh@bandsman.co.uk: create a dump if needed */
	    struct rlimit rlim;

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	if(setrlimit(RLIMIT_CORE, &rlim) < 0)
	    perror("setrlimit");
#endif
	cl_debug(); /* enable debug messages */
    }

    if(optc(opt, 'V')) {
	mprintf("clamscan / ClamAV version "VERSION"\n");
	return 0;
    }

    if(optc(opt, 'h')) {
	free_opt(opt);
    	help();
    }

    /* check other options */

    if(optc(opt, 'r'))
	recursion = 1;

    if(optc(opt, 'i'))
	printinfected = 1;

    if(optl(opt, "bell"))
	bell = 1;

    /* initialize logger */

    if(optc(opt, 'l')) {
	logg_file = getargc(opt, 'l');
	if(logg("--------------------------------------\n")) {
	    mprintf("!Problem with internal logger.\n");
	    return 1;
	}
    } else 
	logg_file = NULL;

    /* we need some pre-checks */
    if(optl(opt, "max-space"))
	if(!strchr(getargl(opt, "max-space"), 'M') && !strchr(getargl(opt, "max-space"), 'm'))
	    if(!isnumb(getargl(opt, "max-space"))) {
		mprintf("!--max-space requires natural number.\n");
		exit(40);
	    }

    if(optl(opt, "max-files"))
	if(!isnumb(getargl(opt, "max-files"))) {
	    mprintf("!--max-files requires natural number.\n");
	    exit(40);
	}

    if(optl(opt, "max-recursion"))
	if(!isnumb(getargl(opt, "max-recursion"))) {
	    mprintf("!--max-recursion requires natural number.\n");
	    exit(40);
	}


    time(&starttime);
    /* ctime() does \n, but I need it once more */
    logg("Scan started: %s\n", ctime(&starttime));

    memset(&claminfo, 0, sizeof(struct s_info));

    gettimeofday(&t1, &tz);
    ret = scanmanager(opt);

    if(!optl(opt, "disable-summary") && !optl(opt, "no-summary")) {
	gettimeofday(&t2, &tz);
	ds = t2.tv_sec - t1.tv_sec;
	dms = t2.tv_usec - t1.tv_usec;
	ds -= (dms < 0) ? (1):(0);
	dms += (dms < 0) ? (1000000):(0);
	mprintf("\n----------- SCAN SUMMARY -----------\n");
	    logg("\n-- summary --\n");
	mprintf("Known viruses: %d\n", claminfo.signs);
	    logg("Known viruses: %d\n", claminfo.signs);
	mprintf("Scanned directories: %d\n", claminfo.dirs);
	    logg("Scanned directories: %d\n", claminfo.dirs);
	mprintf("Scanned files: %d\n", claminfo.files);
	    logg("Scanned files: %d\n", claminfo.files);
	mprintf("Infected files: %d\n", claminfo.ifiles);
	    logg("Infected files: %d\n", claminfo.ifiles);
	if(claminfo.notremoved) {
	    mprintf("Not removed: %d\n", claminfo.notremoved);
		logg("Not removed: %d\n", claminfo.notremoved);
	}
	if(claminfo.notmoved) {
	    mprintf("Not moved: %d\n", claminfo.notmoved);
		logg("Not moved: %d\n", claminfo.notmoved);
	}
	mb = claminfo.blocks * (CL_COUNT_PRECISION / 1024) / 1024.0;
	mprintf("Data scanned: %2.2lf MB\n", mb);
	    logg("Data scanned: %2.2lf MB\n", mb);

	mprintf("I/O buffer size: %d bytes\n", SCANBUFF);
	    logg("I/O buffer size: %d bytes\n", SCANBUFF);
	mprintf("Time: %d.%3.3d sec (%d m %d s)\n", ds, dms/1000, ds/60, ds%60);
	    logg("Time: %d.%3.3d sec (%d m %d s)\n", ds, dms/1000, ds/60, ds%60);
    }

    return ret;
}

void help(void)
{

    mprintf_stdout = 1;

    mprintf("\n");
    mprintf("                          Clam AntiVirus Scanner "VERSION"\n");
    mprintf("                (C) 2002 - 2004 Tomasz Kojm <tkojm@clamav.net>\n\n");

    mprintf("    --help                -h             Show help\n");
    mprintf("    --version             -V             Print version number and exit\n");
    mprintf("    --verbose             -v             Be verbose\n");
    mprintf("    --debug                              Enable debug messages\n");
    mprintf("    --quiet                              Be quiet - only output error messages\n");
    mprintf("    --stdout                             Write to stdout instead of stderr\n");
    mprintf("                                         (this help is always written to stdout)\n");
    mprintf("\n");
    mprintf("    --tempdir=DIRECTORY                  create temporary files in DIRECTORY\n");
    mprintf("    --database=FILE/DIR   -d FILE/DIR    Load virus database from FILE or load\n");
    mprintf("                                         all .db and .db2 files from DIR\n");
    mprintf("    --log=FILE            -l FILE        Save scan report to FILE\n");
    mprintf("    --recursive           -r             Scan directories recursively\n");
    mprintf("    --infected            -i             Print infected files only\n");
    mprintf("    --remove                             Remove infected files. Be careful.\n");
    mprintf("    --move=DIRECTORY                     Move infected files into DIRECTORY\n");
    mprintf("    --exclude=PATT                       Don't scan file names containing PATT\n");
    mprintf("    --include=PATT                       Only scan file names containing PATT\n");
    mprintf("    --bell                               Sound bell on virus detection\n");
    mprintf("    --no-summary                         Disable summary at end of scanning\n");
    mprintf("    --mbox                -m             Treat stdin as a mailbox\n");
    mprintf("\n");
    mprintf("    --no-ole2                            Disable OLE2 support\n");
    mprintf("    --no-archive                         Disable libclamav archive support\n");
    mprintf("    --block-encrypted                    Block encrypted archives.\n");
    mprintf("    --max-space=#n                       Extract first #n kilobytes only\n");
    mprintf("    --max-files=#n                       Extract first #n files only\n");
    mprintf("    --max-recursion=#n                   Maximal recursion level\n");
    mprintf("    --unzip[=FULLPATH]                   Enable support for .zip files\n");
    mprintf("    --unrar[=FULLPATH]                   Enable support for .rar files\n");
    mprintf("    --unace[=FULLPATH]                   Enable support for .ace files\n");
    mprintf("    --arj[=FULLPATH]                     Enable support for .arj files\n");
    mprintf("    --unzoo[=FULLPATH]                   Enable support for .zoo files\n");
    mprintf("    --lha[=FULLPATH]                     Enable support for .lha files\n");
    mprintf("    --jar[=FULLPATH]                     Enable support for .jar files\n");
    mprintf("    --tar[=FULLPATH]                     Enable support for .tar files\n");
    mprintf("    --deb[=FULLPATH to ar]               Enable support for .deb files,\n");
    mprintf("                                         implies --tgz , but doesn't conflict\n");
    mprintf("                                         with --tgz=FULLPATH.\n");
    mprintf("    --tgz[=FULLPATH]                     enable support for .tar.gz, .tgz files\n\n");

    exit(0);
}
