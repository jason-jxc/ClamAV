/*
 *  Copyright (C) 1999 - 2004 Tomasz Kojm <tkojm@clamav.net>
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

#ifndef __OTHERS_H
#define __OTHERS_H

int fileinfo(const char *filename, short i);
int readaccess(const char *path, const char *username);
int writeaccess(const char *path, const char *username);
int filecopy(const char *src, const char *dest);
int isnumb(const char *str);


/* njh@bandsman.co.uk: for BeOS */
/* TODO: configure should see if sete[ug]id is set on the target */
#if defined(C_BEOS) || defined(C_HPUX)
#define       seteuid(u)      (-1)
#define       setegid(g)      (-1)
#endif

#endif
