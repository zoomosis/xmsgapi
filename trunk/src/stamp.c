#include <string.h>
#include <time.h>

#include "prog.h"
#include "msgapi.h"

static int is_dst = -1;

/* Find out the current status of daylight savings time */

static void InitCvt(void)
{
    time_t tnow;
    tnow = time(NULL);
    is_dst = !!(localtime(&tnow)->tm_isdst);
}

/* Convert a DOS-style bitmapped date into a 'struct tm'-type date. */

struct tm *DosDate_to_TmDate(union stamp_combo *dosdate,
  struct tm *tmdate)
{
    if (is_dst == -1)
    {
        InitCvt();
    }

    if (dosdate->ldate == 0)
    {
        time_t t = 0;
        struct tm *tm;
        tm = gmtime(&t);
        memcpy(tmdate, tm, sizeof(*tm));
        return tmdate;
    }

    tmdate->tm_mday = dosdate->msg_st.date.da;
    tmdate->tm_mon = dosdate->msg_st.date.mo - 1;
    tmdate->tm_year = dosdate->msg_st.date.yr + 80;

    tmdate->tm_hour = dosdate->msg_st.time.hh;
    tmdate->tm_min = dosdate->msg_st.time.mm;
    tmdate->tm_sec = dosdate->msg_st.time.ss << 1;

    tmdate->tm_isdst = is_dst;

    return tmdate;
}

/* Convert a 'struct tm'-type date into an Opus/DOS bitmapped date */

union stamp_combo *TmDate_to_DosDate(struct tm *tmdate,
  union stamp_combo *dosdate)
{
    dosdate->msg_st.date.da = tmdate->tm_mday;
    dosdate->msg_st.date.mo = tmdate->tm_mon + 1;
    dosdate->msg_st.date.yr = tmdate->tm_year - 80;

    dosdate->msg_st.time.hh = tmdate->tm_hour;
    dosdate->msg_st.time.mm = tmdate->tm_min;
    dosdate->msg_st.time.ss = tmdate->tm_sec >> 1;

    return dosdate;
}

static void print02d(char **str, int i)
{
    *(*str)++ = (char) ((i / 10) + '0');
    *(*str)++ = (char) ((i % 10) + '0');
}

char *sc_time(union stamp_combo *sc, char *string)
{
    if (sc->msg_st.date.yr == 0)
    {
        *string = '\0';
    }
    else
    {
        print02d(&string, sc->msg_st.date.da);
        *string++ = ' ';
        strcpy(string, months_ab[sc->msg_st.date.mo - 1]);
        string += strlen(string);
        *string++ = ' ';
        print02d(&string, (sc->msg_st.date.yr + 80) % 100);
        *string++ = ' ';
        *string++ = ' ';
        print02d(&string, sc->msg_st.time.hh);
        *string++ = ':';
        print02d(&string, sc->msg_st.time.mm);
        *string++ = ':';
        print02d(&string, sc->msg_st.time.ss << 1);
        *string = '\0';
    }

    return string;
}

char *fts_time(char *string, struct tm *tmdate)
{
    union stamp_combo dosdate;
    return sc_time(TmDate_to_DosDate(tmdate, &dosdate), string);
}

/* Date couldn't be determined, so set it to Jan 1st, 1980 */

static void StandardDate(union stamp_combo *d_written)
{
    d_written->msg_st.date.yr = 0;
    d_written->msg_st.date.mo = 1;
    d_written->msg_st.date.da = 1;

    d_written->msg_st.time.hh = 0;
    d_written->msg_st.time.mm = 0;
    d_written->msg_st.time.ss = 0;
}

void ASCII_Date_To_Binary(char *msgdate, union stamp_combo *d_written)
{
    char temp[80];

    int dd, yy, mo, hh, mm, ss, x;

    time_t timeval;
    struct tm *tim;

    timeval = time(NULL);
    tim = localtime(&timeval);

    if (*msgdate == '\0')
    {
        if (d_written->msg_st.date.yr == 0)
        {
            timeval = time(NULL);
            tim = localtime(&timeval);

            /* Insert today's date */
            fts_time(msgdate, tim);

            StandardDate(d_written);
        }
        else
        {
            if (d_written->msg_st.date.mo == 0 || d_written->msg_st.date.mo > 12)
            {
                d_written->msg_st.date.mo = 1;
            }
            
            sprintf(msgdate, "%02d %s %02d  %02d:%02d:%02d",
              d_written->msg_st.date.da, months_ab[d_written->msg_st.date.mo - 1],
              (d_written->msg_st.date.yr + 80) % 100, d_written->msg_st.time.hh,
              d_written->msg_st.time.mm, d_written->msg_st.time.ss);
        }
        return;
    }

    if (sscanf(msgdate, "%d %s %d %d:%d:%d", &dd, temp, &yy, &hh, &mm, &ss) == 6)
    {
        x = 1;
    }
    else if (sscanf(msgdate, "%d %s %d %d:%d", &dd, temp, &yy, &hh, &mm) == 5)
    {
        ss = 0;
        x = 1;
    }
    else if (sscanf(msgdate, "%*s %d %s %d %d:%d", &dd, temp, &yy, &hh, &mm) == 5)
    {
        x = 2;
    }
    else if (sscanf(msgdate, "%d/%d/%d %d:%d:%d", &mo, &dd, &yy, &hh, &mm, &ss) == 6)
    {
        x = 3;
    }
    else
    {
        StandardDate(d_written);
        return;
    }

    if (x == 1 || x == 2)
    {
        /* Formats one and two have ASCII date, so compare to list */

        for (x = 0; x < 12; x++)
        {
            if (eqstri(temp, months_ab[x]))
            {
                d_written->msg_st.date.mo = x + 1;
                break;
            }
        }

        if (x == 12)
        {
            /* Invalid month, use January instead. */
            d_written->msg_st.date.mo = 1;
        }
    }
    else
    {
        /* Format 3 don't need no ASCII month */
        d_written->msg_st.date.mo = mo;
    }

    /* Use sliding window technique to interprete the year number */

    while (yy <= tim->tm_year - 50)
    {
        yy += 100;
    }

    while (yy > tim->tm_year + 50)
    {
        yy -= 100;
    }

    d_written->msg_st.date.yr = yy - 80;
    d_written->msg_st.date.da = dd;

    d_written->msg_st.time.hh = hh;
    d_written->msg_st.time.mm = mm;
    d_written->msg_st.time.ss = ss >> 1;
}

union stamp_combo *Get_Dos_Date(union stamp_combo *st)
{
    time_t now;
    struct tm *tm;
    now = time(NULL);
    tm = localtime(&now);
    return TmDate_to_DosDate(tm, st);
}
