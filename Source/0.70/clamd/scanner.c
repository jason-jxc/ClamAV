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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <clamav.h>

#include "cfgparser.h"
#include "others.h"
#include "scanner.h"
#include "defaults.h"
#include "memory.h"
#include "shared.h"
#include "output.h"

int checksymlink(const char *path)
{
	struct stat statbuf;

    if(stat(path, &statbuf) == -1)
	return -1;

    if(S_ISDIR(statbuf.st_mode))
	return 1;

    if(S_ISREG(statbuf.st_mode))
	return 2;

    return 0;
}

/* :set nowrap, if you don't like this style ;)) */
int dirscan(const char *dirname, const char **virname, unsigned long int *scanned, const struct cl_node *root, const struct cl_limits *limits, int options, const struct cfgstruct *copt, int odesc, unsigned int *reclev, short contscan)
{
	DIR *dd;
	struct dirent *dent;
	struct stat statbuf;
	struct cfgstruct *cpt;
	char *fname;
	int ret = 0, scanret = 0;

    if((cpt = cfgopt(copt, "MaxDirectoryRecursion"))) {
	if(cpt->numarg) {
	    if(*reclev > cpt->numarg) {
		logg("*Directory recursion limit exceeded at %s\n", dirname);
		return 0;
	    }
	    (*reclev)++;
	}
    }

    if((dd = opendir(dirname)) != NULL) {
	while((dent = readdir(dd))) {
	    if (!is_fd_connected(odesc)) {
		logg("Client disconnected\n");
		closedir(dd);
		return 1;
	    }
	    if(dent->d_ino) {
		if(strcmp(dent->d_name, ".") && strcmp(dent->d_name, "..")) {
		    /* build the full name */
		    fname = (char *) mcalloc(strlen(dirname) + strlen(dent->d_name) + 2, sizeof(char));
		    sprintf(fname, "%s/%s", dirname, dent->d_name);

		    /* stat the file */
		    if(lstat(fname, &statbuf) != -1) {
			if((S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) || (S_ISLNK(statbuf.st_mode) && (checksymlink(fname) == 1) && cfgopt(copt, "FollowDirectorySymlinks"))) {
			    if(dirscan(fname, virname, scanned, root, limits, options, copt, odesc, reclev, contscan) == 1) {
				free(fname);
				closedir(dd);
				return 1;
			    }
			} else {
			    if(S_ISREG(statbuf.st_mode) || (S_ISLNK(statbuf.st_mode) && (checksymlink(fname) == 2) && cfgopt(copt, "FollowFileSymlinks"))) {
				if((scanret = cl_scanfile(fname, virname, scanned, root, limits, options)) == CL_VIRUS) {
				    mdprintf(odesc, "%s: %s FOUND\n", fname, *virname);
				    logg("%s: %s FOUND\n", fname, *virname);
				    virusaction(*virname, copt);
				    if(!contscan) {
					closedir(dd);
					free(fname);
					return 1;
				    } else
					ret = 2;
				} else if(scanret != CL_CLEAN) {
				    mdprintf(odesc, "%s: %s ERROR\n", fname, cl_strerror(scanret));
				    logg("%s: %s ERROR\n", fname, cl_strerror(scanret));
				} else if (logok) {
				    logg("%s: OK\n", fname);
				}
			    }
			}
		    }

		    free(fname);
		}
	    }
	}
	closedir(dd);
    } else {
	return -1;
    }

    (*reclev)--;
    return ret;

}

int scan(const char *filename, unsigned long int *scanned, const struct cl_node *root, const struct cl_limits *limits, int options, const struct cfgstruct *copt, int odesc, short contscan)
{
	struct stat sb;
	int ret = 0, reclev = 0;
	const char *virname;


    /* check permissions  */
    if(access(filename, R_OK)) {
	mdprintf(odesc, "%s: Can't access the file ERROR\n", filename);
	return -1;
    }

    /* stat file */

    if(lstat(filename, &sb) == -1) {
	mdprintf(odesc, "%s: Can't lstat() the file ERROR\n", filename);
	return -1;
    }

    switch(sb.st_mode & S_IFMT) {
	case S_IFLNK:
	    if(!cfgopt(copt, "FollowFileSymlinks"))
		break;
	    /* else go to the next case */
	case S_IFREG: 
	    if(sb.st_size == 0) { /* empty file */
		mdprintf(odesc, "%s: Empty file\n", filename);
		return 0;
	    }
	    ret = cl_scanfile(filename, &virname, scanned, root, limits, options);
	    if(ret == CL_VIRUS) {
		mdprintf(odesc, "%s: %s FOUND\n", filename, virname);
		logg("%s: %s FOUND\n", filename, virname);
		virusaction(virname, copt);
	    } else if(ret != CL_CLEAN) {
		mdprintf(odesc, "%s: %s ERROR\n", filename, cl_strerror(ret));
		logg("%s: %s ERROR\n", filename, cl_strerror(ret));
	    } else if (logok) {
		logg("%s: OK\n", filename);
	    }
	    break;
	case S_IFDIR:
	    ret = dirscan(filename, &virname, scanned, root, limits, options, copt, odesc, &reclev, contscan);
	    break;
	default:
	    mdprintf(odesc, "%s: Not supported file type ERROR\n", filename);
	    return -1;
    }

    if(!ret)
	mdprintf(odesc, "%s: OK\n", filename);

    return ret;
}

int scanstream(int odesc, unsigned long int *scanned, const struct cl_node *root, const struct cl_limits *limits, int options, const struct cfgstruct *copt)
{
	int ret, portscan = CL_DEFAULT_MAXPORTSCAN, sockfd, port, acceptd, tmpd, bread, retval, timeout;
	long int size = 0, maxsize = 0;
	short bound = 0;
	const char *virname;
	char buff[FILEBUFF];
	struct sockaddr_in server;
	struct hostent *he;
	struct cfgstruct *cpt;
	FILE *tmp = NULL;

    while(!bound && portscan--) {
	if((port = cl_rndnum(60000)) < 1024)
	    port += 2139;

	memset((char *) &server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if((cpt = cfgopt(copt, "TCPAddr"))) {
	    if ((he = gethostbyname(cpt->strarg)) == 0) {
		logg("!gethostbyname(%s) error: %s\n", cpt->strarg);
		mdprintf(odesc, "gethostbyname(%s) ERROR\n", cpt->strarg);
		return -1;
	    }
	    server.sin_addr = *(struct in_addr *) he->h_addr_list[0];
	} else
	    server.sin_addr.s_addr = INADDR_ANY;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	    continue;

	if(bind(sockfd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) == -1)
	    close(sockfd);
	else
	    bound = 1;

    }
    
    if((cpt = cfgopt(copt, "ReadTimeout"))) {
	timeout = cpt->numarg;
    } else {
	timeout = CL_DEFAULT_SCANTIMEOUT;
    }
    if (timeout == 0) {
    	timeout = -1;
    }

    if(!bound && !portscan) {
	logg("!ScanStream: Can't find any free port.\n");
	mdprintf(odesc, "Can't find any free port ERROR\n");
	return -1;
    } else {
	listen(sockfd, 1);
	mdprintf(odesc, "PORT %d\n", port);
    }

    retval = poll_fd(sockfd, timeout);
    switch (retval) {
    case 0: /* timeout */
	mdprintf(sockfd, "Accept timeout ERROR\n");
	logg("!ScanStream: accept timeout.\n");
	return -1;
    case -1:
	mdprintf(sockfd, "accept poll ERROR\n");
	logg("!ScanStream: accept poll failed.\n");
	return -1;
    }

    if((acceptd = accept(sockfd, NULL, NULL)) == -1) {
	close(sockfd);
	mdprintf(odesc, "accept() ERROR\n");
	logg("!ScanStream: accept() failed.\n");
	return -1;
    }


    logg("*Accepted connection on port %d, fd %d\n", port, acceptd);

    /* StreamSaveToDisk is enforced, to ensure timeoute */
    /*if(cfgopt(copt, "StreamSaveToDisk")) {	*/
	if((tmp = tmpfile()) == NULL) {
	    shutdown(sockfd, 2);
	    close(sockfd);
	    close(acceptd);
	    mdprintf(odesc, "Temporary file ERROR\n");
	    logg("!ScanStream: Can't create temporary file.\n");
	    return -1;
	}
	tmpd = fileno(tmp);

	if((cpt = cfgopt(copt, "StreamMaxLength")))
	    maxsize = cpt->numarg;

	while((retval = poll_fd(acceptd, timeout)) == 1) {
	    bread = read(acceptd, buff, sizeof(buff));
	    if (bread <= 0) {
		break;
	    }
	    size += bread;

	    if(maxsize && (size + sizeof(buff)) > maxsize) {
		shutdown(sockfd, 2);
		close(sockfd);
		close(acceptd);
		mdprintf(odesc, "Size exceeded ERROR\n");
		logg("^ScanStream: Size exceeded (stopped at %d, max: %d)\n", size, maxsize);
		if(tmp)
		    fclose(tmp);
		return -1;
	    }

	    if(writen(tmpd, buff, bread) != bread) {
		shutdown(sockfd, 2);
		close(sockfd);
		close(acceptd);
		mdprintf(odesc, "Temporary file -> write ERROR\n");
		logg("!ScanStream: Can't write to temporary file.\n");
		if(tmp)
		    fclose(tmp);
		return -1;
	    }

	}

	lseek(tmpd, 0, SEEK_SET);
	ret = cl_scandesc(tmpd, &virname, scanned, root, limits, options);
	if(tmp)
	    fclose(tmp);

    /* } else
	ret = cl_scandesc(acceptd, &virname, scanned, root, limits, 0);
	*/

    close(acceptd);
    close(sockfd);

    if(ret == CL_VIRUS) {
	mdprintf(odesc, "stream: %s FOUND\n", virname);
	logg("stream: %s FOUND\n", virname);
	virusaction(virname, copt);
    } else if(ret != CL_CLEAN) {
	mdprintf(odesc, "stream: %s ERROR\n", cl_strerror(ret));
	logg("stream: %s ERROR\n", cl_strerror(ret));
    } else {
	mdprintf(odesc, "stream: OK\n");
        if (logok)
	    logg("stream: OK\n"); 
    }

    return ret;
}
