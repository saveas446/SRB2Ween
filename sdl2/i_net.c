#include "../i_net.h"
#include "../doomtype.h"
#include "../doomstat.h"
#include "../z_zone.h"

// Not sure what the purpose of these three functions is considering none of the existing backend do anything with them.
void Internal_Get(void)
{
     I_Error("Get without netgame\n");
}

void Internal_Send(void)
{
     I_Error("Send without netgame\n");
}

void Internal_FreeNodenum(int nodenum)
{}

void I_InitNetwork(void) {
    int netgamepar;


    I_NetGet = Internal_Get;
    I_NetSend = Internal_Send;
    I_NetShutdown = NULL;
    I_NetFreeNodenum = Internal_FreeNodenum;

    netgamepar = M_CheckParm("-net");
    if (!netgamepar)
    {
        doomcom = Z_Malloc(sizeof(doomcom_t), PU_STATIC, NULL);
        memset(doomcom, 0, sizeof(doomcom_t));
        doomcom->id = DOOMCOM_ID;

        //if (!I_InitTcpNetwork())
        //{
            netgame = false;
            server = true;
            multiplayer = false;

            doomcom->numplayers = doomcom->numnodes = 1;
            doomcom->deathmatch = false;
            doomcom->consoleplayer = 0;
            doomcom->ticdup = 1;
            doomcom->extratics = 0;
            return;
        //}
    } // else net game
    else
    {
        I_Error("External driver not yet implemented\n");
    }
    // net game

    netgame = true;
    multiplayer = true;
}
