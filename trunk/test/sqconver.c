#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prog.h"
#include "msgapi.h"

#define BUFSIZE 4096

int main(int argc, char **argv)
{
    XMSG msg;
    MSGA *in_area, *out_area;
    MSGH *in_msg, *out_msg;
    byte *ctrl, *buffer;
    dword offset, msgn;
    long got;
    word t1, t2;
    int ctrllen;
    struct _minf mi;

    if (argc < 6)
    {
        printf("Format:  sqconver <from_name> <from_type> <to_name>  <to_type> <default_zone>\n");
        printf("Example: sqconver /msg/foo    *.msg       /msg/foosq squish    3\n");
        return EXIT_FAILURE;
    }

    printf("Converting area %s...\n", argv[1]);

    if (eqstri(argv[2], "*.msg"))
    {
        t1 = MSGTYPE_SDM;
    }
    else if (eqstri(argv[2], "squish"))
    {
        t1 = MSGTYPE_SQUISH;
    }
#ifdef USE_JAM
    else if (eqstri(argv[2], "jam"))
    {
        t1 = MSGTYPE_JAM;
    }
#endif
    else
    {
        t1 = atoi(argv[2]);
    }

    mi.def_zone = atoi(argv[5]);

    MsgOpenApi(&mi);

    in_area = MsgOpenArea((byte *) argv[1], MSGAREA_NORMAL, t1);
    
    if (in_area == NULL)
    {
        printf("Error opening area `%s' (type %s) for read!\n",
          argv[1], argv[2]);
        return EXIT_FAILURE;
    }

    MsgLock(in_area);

    if (eqstri(argv[4], "*.msg"))
    {
        t2 = MSGTYPE_SDM;
    }
    else if (eqstri(argv[4], "squish"))
    {
        t2 = MSGTYPE_SQUISH;
    }
#ifdef USE_JAM
    else if (eqstri(argv[4], "jam"))
    {
        t2 = MSGTYPE_JAM;
    }
#endif
    else
    {
        t2 = atoi(argv[4]);
    }

    out_area = MsgOpenArea((byte *) argv[3], MSGAREA_CRIFNEC, t2);

    if (out_area == NULL)
    {
        printf("Error opening area `%s' (type %s) for write!\n",
          argv[3], argv[4]);
        return EXIT_FAILURE;
    }

    MsgLock(out_area);

    buffer = malloc(BUFSIZE);

    if (buffer == NULL)
    {
        puts("Error!  Ran out of memory...");
        return EXIT_FAILURE;
    }

    for (msgn = 1L; msgn <= MsgHighMsg(in_area); msgn++)
    {
        if ((msgn % 5) == 0)
        {
            printf("Msg: %ld\r", msgn);
        }

        in_msg = MsgOpenMsg(in_area, MOPEN_READ, msgn);

        if (in_msg == NULL)
        {
            continue;
        }

        out_msg = MsgOpenMsg(out_area, MOPEN_CREATE, 0L);

        if (out_msg == NULL)
        {
            printf("Error writing to output area; msg#%ld (%d).\n", msgn, msgapierr);
            MsgCloseMsg(in_msg);
            continue;
        }

        ctrllen = (int)MsgGetCtrlLen(in_msg);

        ctrl = malloc(ctrllen);

        if (ctrl == NULL)
        {
            ctrllen = 0;
        }

        MsgReadMsg(in_msg, &msg, 0L, 0L, NULL, ctrllen, ctrl);

        msg.attr |= MSGSCANNED;

        msg.replyto = 0L;
        memset(msg.replies, '\0', sizeof(msg.replies));

        MsgWriteMsg(out_msg, FALSE, &msg, NULL, 0L, MsgGetTextLen(in_msg),
          ctrllen, ctrl);

        for (offset = 0L; offset < MsgGetTextLen(in_msg);)
        {
            got = MsgReadMsg(in_msg, NULL, offset, BUFSIZE, buffer, 0L, NULL);

            if (got <= 0)
            {
                break;
            }

            MsgWriteMsg(out_msg, TRUE, NULL, buffer, got,
            MsgGetTextLen(in_msg), 0L, NULL);

            offset += got;
        }

        if (ctrl)
        {
            free(ctrl);
        }

        MsgCloseMsg(out_msg);
        MsgCloseMsg(in_msg);
    }

    free(buffer);
    MsgCloseArea(out_area);
    MsgCloseArea(in_area);
    MsgCloseApi();

    printf("\nDone!\n");

    return EXIT_SUCCESS;
}

