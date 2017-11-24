/*
 * mktime.c -- converts a struct tm into a time_t
 *
 * Copyright (C) 1997 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/rtc.h>

static uint8_t dim[13] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static char day[] = {
    'S','u','n',0,
    'M','o','n',0,
    'T','u','e',0,
    'W','e','d',0,
    'T','h','u',0,
    'F','r','i',0,
    'S','a','t',0
};

/*
 * These time conversion functions are by Philippe De Muyter <phdm@macqel.be>
 * and can live here until we have a final one in libc
 */

static time_t mkgmtime(struct tm *t)
{
	short month, year;
	time_t result;
	static int m_to_d[12] =
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

	month = t->tm_mon;
	year = t->tm_year + month / 12 + 1900;
	month %= 12;
	if (month < 0)
	{
		year -= 1;
		month += 12;
	}
	result = (year - 1970) * 365 + (year - 1969) / 4 + m_to_d[month];
	result = (year - 1970) * 365 + m_to_d[month];
	if (month <= 1)
		year -= 1;
	result += (year - 1968) / 4;
	result -= (year - 1900) / 100;
	result += (year - 1600) / 400;
	result += t->tm_mday;
	result -= 1;
	result *= 24;
	result += t->tm_hour;
	result *= 60;
	result += t->tm_min;
	result *= 60;
	result += t->tm_sec;
	return(result);
}

/*
**  mktime -- convert tm struct to time_t
**		if tm_isdst >= 0 use it, else compute it
*/

time_t mktime(struct tm	*t)
{
	time_t	result;

	tzset();
	result = mkgmtime(t) + timezone;
	if (t->tm_isdst > 0
	|| (t->tm_isdst < 0 && localtime(&result)->tm_isdst))
		result -= 3600;
	return(result);
}

void unbc(uint8_t *p)
{
    uint8_t c = *p & 0x0F;
    c += 10 * ((*p & 0xF0) >> 4);
    *p = c;
}

int rtcdate(void)
{
    struct tm tm;
    struct cmos_rtc rtc;
    uint8_t *p;
    uint8_t i;

    int fd = open("/dev/rtc", O_RDONLY);
    if (fd == -1)
        return 0;
    if (read(fd, &rtc, sizeof(rtc)) != sizeof(rtc)) {
        close(fd);
        return 0;
    }
    close(fd);

    switch (rtc.type) {
    case CMOS_RTC_BCD:
        p = rtc.data.bytes;
        for (i = 0; i < 7; i++)
            unbc(p++);
    case CMOS_RTC_DEC:
        p = rtc.data.bytes;
        tm.tm_year = (*p + (p[1] << 8)) - 1900;
        p += 2;
        tm.tm_mon = *p++;
        tm.tm_mday = *p++;
        tm.tm_hour = *p++;
        tm.tm_min = *p++;
        tm.tm_sec = *p;
        tm.tm_isdst = 0;	/* FIXME -1 once we have dst fixed */
        rtc.data.clock = mktime(&tm);
    case CMOS_RTC_TIME:
        if (stime(&rtc.data.clock) == -1) {
            perror("stime");
            exit(1);
        }
        return 1;
    default:
        return 0;
    }
}

void usage(void)
{
    fprintf(stderr, "Usage: setdate [-a] [-u]\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    struct tm *tm;
    time_t t;
    char newval[128];
    int y,m,d,h,s;
    int set;
    int opt;
    int user = 0, autom = 0;

    while ((opt = getopt(argc, argv, "au")) != -1) {
        switch(opt) {
        case 'a':
            autom = 1;
            break;
        case 'u':
            user = 1;
            break;
        default:
            usage();
        }
    }
    if (optind != argc)
        usage();

    if (!user && rtcdate())
        exit(0);

    if (autom)
        exit(1);

    time(&t);
    tm = localtime(&t);
    if (tm) {
        printf("Current date is %s %04d-%02d-%02d\n",
            day + 4 * tm->tm_wday,
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday);
    }
redate:
    printf("Enter new date: ");
    fflush(stdout);
    if (fgets(newval, 128, stdin) && *newval != '\n') {
        if (sscanf(newval, "%d-%d-%d", &y, &m, &d) != 3 ||
            y < 1970 || m < 1 || m > 12 || d < 1 || d > dim[m]) {
            fprintf(stderr, "Invalid date.\n");
            goto redate;
        }
        tm->tm_year = y - 1900;
        tm->tm_mon = m - 1;
        tm->tm_mday = d;
        set = 1;
    }
    if (tm) {
        printf("Current time is %2d:%02d:%02d\n",
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    }
retime:
    printf("Enter new time: ");
    fflush(stdout);
    if (fgets(newval, 128, stdin) && *newval != '\n') {
        s = 0;
        if (sscanf(newval, "%d:%d:%d", &h,&m,&s) < 2 ||
            h < 1 || h > 24 || m < 1 || m > 59 || s < 0 || s > 59) {
            fprintf(stderr, "Invalid time.\n");
            goto retime;
        }
        tm->tm_hour = h;
        tm->tm_min = m;
        tm->tm_sec = s;
        set = 1;
    }
    if (!set)
        return 0;
    tm->tm_isdst = 0;	/* FIXME -1 once we have dst fixed */
    t = mktime(tm);
    if (t == (time_t) -1) {
        fprintf(stderr, "mktime: internal error.\n");
        return 255;
    }
    if (stime(&t)) {
        perror("stime");
        return 2;
    }
    return 0;
}
