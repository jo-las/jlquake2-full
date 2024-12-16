/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "g_local.h"
#include "m_player.h"


char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[512];

	value[0] = 0;

	if (!ent->client)
		return value;

	strcpy(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if ((int)(dmflags->value) & DF_SKINTEAMS)
	return ++p;
}

qboolean OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [512];
	char	ent2Team [512];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return false;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return true;
	return false;
}


void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}


//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t		*it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_stricmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_stricmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO))
				continue;
			ent->client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem (name);
		if (!it)
		{
			gi.cprintf (ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.cprintf (ent, PRINT_HIGH, "non-pickup item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop (ent, it);
}


/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f (edict_t *ent)
{
	int			i;
	gclient_t	*cl;

	cl = ent->client;

	cl->showscores = false;
	cl->showhelp = false;

	if (cl->showinventory)
	{
		cl->showinventory = false;
		return;
	}

	cl->showinventory = true;

	gi.WriteByte (svc_inventory);
	for (i=0 ; i<MAX_ITEMS ; i++)
	{
		gi.WriteShort (cl->pers.inventory[i]);
	}
	gi.unicast (ent, true);
}

/*
=================
Cmd_InvUse_f
=================
*/
void Cmd_InvUse_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
void Cmd_WeapPrev_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	it->drop (ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f (edict_t *ent)
{
	if((level.time - ent->client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f (edict_t *ent)
{
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;
}


int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
void Cmd_Players_f (edict_t *ent)
{
	int		i;
	int		count;
	char	small[64];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0 ; i < maxclients->value ; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort (index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0 ; i < count ; i++)
	{
		Com_sprintf (small, sizeof(small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen (small) + strlen(large) > sizeof(large) - 100 )
		{	// can't print all of them in one packet
			strcat (large, "...\n");
			break;
		}
		strcat (large, small);
	}

	gi.cprintf (ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f (edict_t *ent)
{
	int		i;

	i = atoi (gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch (i)
	{
	case 0:
		gi.cprintf (ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = FRAME_flip01-1;
		ent->client->anim_end = FRAME_flip12;
		break;
	case 1:
		gi.cprintf (ent, PRINT_HIGH, "salute\n");
		ent->s.frame = FRAME_salute01-1;
		ent->client->anim_end = FRAME_salute11;
		break;
	case 2:
		gi.cprintf (ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = FRAME_taunt01-1;
		ent->client->anim_end = FRAME_taunt17;
		break;
	case 3:
		gi.cprintf (ent, PRINT_HIGH, "wave\n");
		ent->s.frame = FRAME_wave01-1;
		ent->client->anim_end = FRAME_wave11;
		break;
	case 4:
	default:
		gi.cprintf (ent, PRINT_HIGH, "point\n");
		ent->s.frame = FRAME_point01-1;
		ent->client->anim_end = FRAME_point12;
		break;
	}
}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, qboolean team, qboolean arg0)
{
	int		i, j;
	edict_t	*other;
	char	*p;
	char	text[2048];
	gclient_t *cl;

	if (gi.argc () < 2 && !arg0)
		return;

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = false;

	if (team)
		Com_sprintf (text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		strcat (text, gi.argv(0));
		strcat (text, " ");
		strcat (text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	strcat(text, "\n");

	if (flood_msgs->value) {
		cl = ent->client;

        if (level.time < cl->flood_locktill) {
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time));
            return;
        }
        i = cl->flood_whenhead - flood_msgs->value + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && 
			level.time - cl->flood_when[i] < flood_persecond->value) {
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
            return;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

void Cmd_PlayerList_f(edict_t *ent)
{
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// connect time, ping, score, name
	*text = 0;
	for (i = 0, e2 = g_edicts + 1; i < maxclients->value; i++, e2++) {
		if (!e2->inuse)
			continue;

		sprintf(st, "%02d:%02d %4d %3d %s%s\n",
			(level.framenum - e2->client->resp.enterframe) / 600,
			((level.framenum - e2->client->resp.enterframe) % 600)/10,
			e2->client->ping,
			e2->client->resp.score,
			e2->client->pers.netname,
			e2->client->resp.spectator ? " (spectator)" : "");
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			gi.cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	gi.cprintf(ent, PRINT_HIGH, "%s", text);
}


/*
=================
ClientCommand
=================
*/
void ClientCommand (edict_t *ent)
{
	char	*cmd;

	if (!ent->client)
		return;		// not fully in game yet

	cmd = gi.argv(0);

	if (Q_stricmp (cmd, "players") == 0)
	{
		Cmd_Players_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (Q_stricmp (cmd, "score") == 0)
	{
		Cmd_Score_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}
	// Handle "plantcrop" command; made to handle multiple kinds of crops
	if (Q_stricmp(cmd, "plantcrop") == 0)
	{
		if (gi.argc() < 2) {
			gi.cprintf(ent, PRINT_HIGH, "Usage: plantcrop <crop_type>\n");
			return;
		}
		char* crop_type_str = gi.argv(1); //parsing the command
		int crop_type;
		// Convert crop type string to integer
		if (Q_stricmp(crop_type_str, "wheat") == 0) {
			crop_type = CROPTYPE_WHEAT;
		}
		else if (Q_stricmp(crop_type_str, "corn") == 0) {
			crop_type = CROPTYPE_CORN;
		}
		else if (Q_stricmp(crop_type_str, "tomato") == 0) {
			crop_type = CROPTYPE_TOMATO;
		}
		else if (Q_stricmp(crop_type_str, "potato") == 0) {
			crop_type = CROPTYPE_POTATO;
		}
		else if (Q_stricmp(crop_type_str, "carrot") == 0) {
			crop_type = CROPTYPE_CARROT;
		}
		else if (Q_stricmp(crop_type_str, "bean") == 0) {
			crop_type = CROPTYPE_BEAN;
		}
		else if (Q_stricmp(crop_type_str, "pepper") == 0) {
			crop_type = CROPTYPE_PEPPER;
		}
		else if (Q_stricmp(crop_type_str, "cabbage") == 0) {
			crop_type = CROPTYPE_CABBAGE;
		}
		else if (Q_stricmp(crop_type_str, "onion") == 0) {
			crop_type = CROPTYPE_ONION;
		}
		else if (Q_stricmp(crop_type_str, "garlic") == 0) {
			crop_type = CROPTYPE_GARLIC;
		}
		else {
			gi.cprintf(ent, PRINT_HIGH, "Invalid crop type! Valid types are: wheat, corn, tomato, potato, carrot, bean, pepper, cabbage, onion, garlic.\n");
			return;
		}
		Cmd_PlantCrop_f(ent, crop_type);  // Call function for planting crops
		return;
	}
	// Handle "harvestcrop" command
	if (Q_stricmp(cmd, "harvestcrop") == 0)
	{
		Cmd_HarvestCrop_f(ent);  // Call function for harvesting crops
		return;
	}
	//Handle "spawnfield" command
	if (Q_stricmp(cmd, "spawnfield") == 0)
	{
		Cmd_SpawnField_f(ent);  // Call function for spawning field
		return;
	}
	//Handle "displayresources" command
	if (Q_stricmp(cmd, "displayresources") == 0)
	{
		Cmd_DisplayResources_f(ent);  // Call function to display resources
		return;
	}
	//Handle "addseeds" command
	if (Q_stricmp(cmd, "addseeds") == 0)
	{
		Cmd_AddSeeds_f(ent);  // Call function for adding seeds
		return;
	}
	//Handle spawnflyer command
	if (Q_stricmp(cmd, "spawnpest") == 0)
	{
		Cmd_SpawnFlyer_f(ent);  // Call function for spawning pests
		return;
	}

	//Handle shopkeeper interaction
	if (Q_stricmp(cmd, "spawnshopkeep") == 0) {
		Cmd_SpawnShopkeeper_f(ent);
		return;
	}
	if (Q_stricmp(cmd, "interact_shopkeeper") == 0) {
		Cmd_InteractShopkeeper_f(ent);
		return;
	}
	if (Q_stricmp(cmd, "shopkeeper_choice") == 0) {
		int choice = atoi(gi.argv(1));
		Cmd_HandleShopkeeperInput_f(ent, choice);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_stricmp (cmd, "use") == 0)
		Cmd_Use_f (ent);
	else if (Q_stricmp (cmd, "drop") == 0)
		Cmd_Drop_f (ent);
	else if (Q_stricmp (cmd, "give") == 0)
		Cmd_Give_f (ent);
	else if (Q_stricmp (cmd, "god") == 0)
		Cmd_God_f (ent);
	else if (Q_stricmp (cmd, "notarget") == 0)
		Cmd_Notarget_f (ent);
	else if (Q_stricmp (cmd, "noclip") == 0)
		Cmd_Noclip_f (ent);
	else if (Q_stricmp (cmd, "inven") == 0)
		Cmd_Inven_f (ent);
	else if (Q_stricmp (cmd, "invnext") == 0)
		SelectNextItem (ent, -1);
	else if (Q_stricmp (cmd, "invprev") == 0)
		SelectPrevItem (ent, -1);
	else if (Q_stricmp (cmd, "invnextw") == 0)
		SelectNextItem (ent, IT_WEAPON);
	else if (Q_stricmp (cmd, "invprevw") == 0)
		SelectPrevItem (ent, IT_WEAPON);
	else if (Q_stricmp (cmd, "invnextp") == 0)
		SelectNextItem (ent, IT_POWERUP);
	else if (Q_stricmp (cmd, "invprevp") == 0)
		SelectPrevItem (ent, IT_POWERUP);
	else if (Q_stricmp (cmd, "invuse") == 0)
		Cmd_InvUse_f (ent);
	else if (Q_stricmp (cmd, "invdrop") == 0)
		Cmd_InvDrop_f (ent);
	else if (Q_stricmp (cmd, "weapprev") == 0)
		Cmd_WeapPrev_f (ent);
	else if (Q_stricmp (cmd, "weapnext") == 0)
		Cmd_WeapNext_f (ent);
	else if (Q_stricmp (cmd, "weaplast") == 0)
		Cmd_WeapLast_f (ent);
	else if (Q_stricmp (cmd, "kill") == 0)
		Cmd_Kill_f (ent);
	else if (Q_stricmp (cmd, "putaway") == 0)
		Cmd_PutAway_f (ent);
	else if (Q_stricmp (cmd, "wave") == 0)
		Cmd_Wave_f (ent);
	else if (Q_stricmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(ent);
	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f (ent, false, true);
}

void Cmd_PlantCrop_f(edict_t* ent, int crop_type) {
	edict_t* field = NULL;

	// Find the field the player is interacting with
	for (int i = 1; i < globals.num_edicts; i++) {
		edict_t* other = &g_edicts[i];
		if (!other->inuse || strcmp(other->classname, "trigger_field") != 0) {
			continue; // Skip invalid or non-field entities
		}

		// Check if the player overlaps the field bounds
		if (ent->absmin[0] > other->absmax[0] || ent->absmax[0] < other->absmin[0] ||
			ent->absmin[1] > other->absmax[1] || ent->absmax[1] < other->absmin[1] ||
			ent->absmin[2] > other->absmax[2] || ent->absmax[2] < other->absmin[2]) {
			continue; // Not touching this field
		}

		field = other;
		break;
	}

	// Check if a valid field was found
	if (!field) {
		gi.cprintf(ent, PRINT_HIGH, "You are not on a valid field!\n");
		return;
	}

	// Check if the field already has a crop
	if (field->plantedcrop) {
		gi.cprintf(ent, PRINT_HIGH, "This field already has a crop!\n");
		return;
	}

	// Deduct 1 seed from the player's seed stock
	ent->client->seed_stock -= 1;
	gi.cprintf(ent, PRINT_HIGH, "Planted a crop. Remaining seeds: %d\n", ent->client->seed_stock);

	// Debug log
	gi.dprintf("Planting on field at position (%f, %f, %f)\n", field->s.origin[0], field->s.origin[1], field->s.origin[2]);

	// Plant a new crop based on the specified crop type
	SpawnCrop(field, crop_type);

	// Provide feedback to the player
	switch (crop_type) {
	case CROPTYPE_WHEAT:
		gi.cprintf(ent, PRINT_HIGH, "You planted a wheat crop!\n");
		break;
	case CROPTYPE_CORN:
		gi.cprintf(ent, PRINT_HIGH, "You planted a corn crop!\n");
		break;
	case CROPTYPE_TOMATO:
		gi.cprintf(ent, PRINT_HIGH, "You planted a tomato crop!\n");
		break;
	case CROPTYPE_POTATO:
		gi.cprintf(ent, PRINT_HIGH, "You planted a potato crop!\n");
		break;
	case CROPTYPE_CARROT:
		gi.cprintf(ent, PRINT_HIGH, "You planted a carrot crop!\n");
		break;
	case CROPTYPE_BEAN:
		gi.cprintf(ent, PRINT_HIGH, "You planted a bean crop!\n");
		break;
	case CROPTYPE_PEPPER:
		gi.cprintf(ent, PRINT_HIGH, "You planted a pepper crop!\n");
		break;
	case CROPTYPE_CABBAGE:
		gi.cprintf(ent, PRINT_HIGH, "You planted a cabbage crop!\n");
		break;
	case CROPTYPE_ONION:
		gi.cprintf(ent, PRINT_HIGH, "You planted an onion crop!\n");
		break;
	case CROPTYPE_GARLIC:
		gi.cprintf(ent, PRINT_HIGH, "You planted a garlic crop!\n");
		break;
	default:
		gi.cprintf(ent, PRINT_HIGH, "You planted an unknown crop!\n");
		break;
	}
}

void Cmd_HarvestCrop_f(edict_t* ent) {
	gi.dprintf("Cmd_HarvestCrop_f: Function called.\n");
	edict_t* field = NULL;

	// Iterate through all entities to find the field the player is touching
	for (int i = 1; i < globals.num_edicts; i++) {
		edict_t* other = &g_edicts[i];
		if (!other->inuse || strcmp(other->classname, "trigger_field") != 0) {
			continue; // Skip invalid or non-field entities
		}

		// Check if the player overlaps the field bounds
		if (ent->absmin[0] > other->absmax[0] || ent->absmax[0] < other->absmin[0] ||
			ent->absmin[1] > other->absmax[1] || ent->absmax[1] < other->absmin[1] ||
			ent->absmin[2] > other->absmax[2] || ent->absmax[2] < other->absmin[2]) {
			continue; // Not touching this field
		}

		field = other;
		break;
	}

	gi.dprintf("Cmd_HarvestCrop_f: Field found.\n");

	// Check if a valid field was found
	if (!field) {
		gi.cprintf(ent, PRINT_HIGH, "You are not on a valid field!\n");
		return;
	}

	// Check if the field has a crop
	if (!field->plantedcrop) {
		gi.cprintf(ent, PRINT_HIGH, "No crop is planted here to harvest.\n");
		return;
	}

	gi.dprintf("Cmd_HarvestCrop_f: Crop found.\n");

	// Check if the crop is fully grown
	crop_t* crop = field->plantedcrop;
	if (!crop) {
		gi.cprintf(ent, PRINT_HIGH, "Error: Invalid crop pointer.\n");
		return;
	}

	if (crop->growth_stage < 2) { // Adjust the growth stage check as needed
		gi.cprintf(ent, PRINT_HIGH, "The crop is not fully grown yet!\n");
		return;
	}

	if (!field->plantedcrop || field->plantedcrop->growth_stage < 0) {  // Add sanity check
		gi.cprintf(ent, PRINT_HIGH, "Invalid crop state.\n");
		return;
	}

	gi.dprintf("Cmd_HarvestCrop_f: Harvesting crop.\n");

	// Determine the currency reward and feedback based on the crop type
	int currency_reward = 0;
	const char* crop_name = "Unknown crop";
	switch (crop->type) {
	case CROPTYPE_WHEAT:
		currency_reward = 10;
		crop_name = "Wheat";
		break;
	case CROPTYPE_CORN:
		currency_reward = 12;
		crop_name = "Corn";
		break;
	case CROPTYPE_TOMATO:
		currency_reward = 8;
		crop_name = "Tomato";
		break;
	case CROPTYPE_POTATO:
		currency_reward = 15;
		crop_name = "Potato";
		break;
	case CROPTYPE_CARROT:
		currency_reward = 9;
		crop_name = "Carrot";
		break;
	case CROPTYPE_BEAN:
		currency_reward = 7;
		crop_name = "Bean";
		break;
	case CROPTYPE_PEPPER:
		currency_reward = 11;
		crop_name = "Pepper";
		break;
	case CROPTYPE_CABBAGE:
		currency_reward = 14;
		crop_name = "Cabbage";
		break;
	case CROPTYPE_ONION:
		currency_reward = 13;
		crop_name = "Onion";
		break;
	case CROPTYPE_GARLIC:
		currency_reward = 10;
		crop_name = "Garlic";
		break;
		// Add more cases for other crop types
	default:
		gi.cprintf(ent, PRINT_HIGH, "Unknown crop type! No currency awarded.\n");
		return;
	}

	// Harvest the crop and award currency
	gi.cprintf(ent, PRINT_HIGH, "You harvested a %s crop!\n", crop_name);
	ent->client->currency += currency_reward;
	gi.cprintf(ent, PRINT_HIGH, "You earned %d currency. Current balance: %d\n", currency_reward, ent->client->currency);
	gi.dprintf("Player harvested %s crop at field (%f, %f, %f)\n",
		crop_name, field->s.origin[0], field->s.origin[1], field->s.origin[2]);

	// Free the crop and mark the field as available for planting again
	if (crop) {
		gi.dprintf("Cmd_HarvestCrop_f: Freeing crop entity.\n");
		//G_FreeEdict(crop);
		field->plantedcrop = NULL;
	}

	// Ensure the field's plantedcrop is set to NULL
	if (field->plantedcrop) {
		gi.dprintf("Cmd_HarvestCrop_f: Error - field's plantedcrop is not NULL after harvesting.\n");
		field->plantedcrop = NULL;
	}

	gi.dprintf("Cmd_HarvestCrop_f: Crop harvested successfully.\n");
}

void Cmd_SpawnField_f(edict_t* ent) {
	if (!ent || !ent->client) {
		return; // Ensure it's a valid player entity
	}

	gi.cprintf(ent, PRINT_HIGH, "Spawning field trigger...\n");

	// Spawn a field trigger near the player
	SpawnFieldNearPlayer(ent);

	// Notify the player that the field has been spawned
	gi.cprintf(ent, PRINT_HIGH, "Field trigger spawned near you!\n");
}

void Cmd_DisplayResources_f(edict_t* ent)
{
	// Hide other UI elements if needed (e.g., inventory or score)
	ent->client->showinventory = false;
	ent->client->showscores = false;

	// Display the currency and seed stock directly on the screen
	gi.cprintf(ent, PRINT_HIGH, "Currency: %d\n", ent->client->currency);
	gi.cprintf(ent, PRINT_HIGH, "Seed Stock: %d\n", ent->client->seed_stock);
}

void Cmd_AddSeeds_f(edict_t* ent)
{
	int amount = 10;  // Default amount to add 

	// Check if a specific amount was passed with the command
	if (gi.argc() > 1)
	{
		amount = atoi(gi.argv(1));  // Parse the argument (if any) into an integer
	}

	// Add seeds to the player's seed stock
	ent->client->seed_stock += amount;

	// Make sure the seed stock doesn't go negative
	if (ent->client->seed_stock < 0)
	{
		ent->client->seed_stock = 0;
	}

	// Print a message to the player confirming the addition
	gi.cprintf(ent, PRINT_HIGH, "Added %d seeds. You now have %d seeds.\n", amount, ent->client->seed_stock);
}

void Cmd_SpawnFlyer_f(edict_t* ent) {
	if (!ent || !ent->client) {
		return; // Ensure it's a valid player entity
	}

	gi.cprintf(ent, PRINT_HIGH, "Spawning flyer monster...\n");

	// Spawn a flyer monster near the player
	SpawnFlyerNearPlayer(ent);

	// Notify the player that the flyer has been spawned
	gi.cprintf(ent, PRINT_HIGH, "Flyer monster spawned near you!\n");
}

void Cmd_SpawnShopkeeper_f(edict_t* ent) {
	edict_t* shopkeeper_entity;

	// Create a new entity for the shopkeeper
	shopkeeper_entity = G_Spawn();
	if (!shopkeeper_entity) {
		gi.cprintf(ent, PRINT_HIGH, "Failed to spawn shopkeeper.\n");
		return;
	}

	// Set the shopkeeper's position to the player's location
	VectorCopy(ent->s.origin, shopkeeper_entity->s.origin);
	shopkeeper_entity->s.origin[2] += 24;  // Slightly above the ground

	// Set the shopkeeper entity properties
	shopkeeper_entity->classname = "shopkeeper";
	shopkeeper_entity->movetype = MOVETYPE_NONE;  // Shopkeepers don’t move
	shopkeeper_entity->solid = SOLID_BBOX;
	shopkeeper_entity->model = "models/monsters/boss2/tris.md2";  
	shopkeeper_entity->s.modelindex = gi.modelindex(shopkeeper_entity->model);
	VectorSet(shopkeeper_entity->mins, -16, -16, -24);  // Adjust bounding box
	VectorSet(shopkeeper_entity->maxs, 16, 16, 32);

	// Link the entity into the world
	gi.linkentity(shopkeeper_entity);

	// Output shopkeeper details to the console (for debugging)
	gi.cprintf(ent, PRINT_HIGH, "Shopkeeper '%s' spawned at your location.\n", shopkeeper.name);
}

void Cmd_InteractShopkeeper_f(edict_t* ent) {
	edict_t* shopkeeper_ent = NULL;
	// Iterate through all entities to find the shopkeeper the player is touching
	for (int i = 1; i < globals.num_edicts; i++) {
		edict_t* other = &g_edicts[i];
		if (!other->inuse || strcmp(other->classname, "shopkeeper") != 0) {
			continue; // Skip invalid or non-shopkeeper entities
		}
		// Check if the player overlaps the shopkeeper bounds
		if (ent->absmin[0] > other->absmax[0] || ent->absmax[0] < other->absmin[0] ||
			ent->absmin[1] > other->absmax[1] || ent->absmax[1] < other->absmin[1] ||
			ent->absmin[2] > other->absmax[2] || ent->absmax[2] < other->absmin[2]) {
			continue; // Not touching this shopkeeper
		}

		shopkeeper_ent = other;
		break;
	}

	// Check if a valid shopkeeper was found
	if (!shopkeeper_ent) {
		gi.cprintf(ent, PRINT_HIGH, "You are not near a shopkeeper!\n");
		return;
	}

	// Display shopkeeper menu
	gi.cprintf(ent, PRINT_HIGH, "Welcome to the shop! What would you like to buy?\n");
	gi.cprintf(ent, PRINT_HIGH, "1. Buy Seeds (%d currency for 5 seeds)\n", shopkeeper.seed_price);
	gi.cprintf(ent, PRINT_HIGH, "2. Buy Tools (%d currency for a shotgun)\n", shopkeeper.tool_price);
	gi.cprintf(ent, PRINT_HIGH, "3. Exit\n");

	// Set interaction flag
	ent->client->interacting_with_shopkeeper = true;
}

void Cmd_HandleShopkeeperInput_f(edict_t* ent, int choice) {
	if (!ent->client->interacting_with_shopkeeper) {
		gi.cprintf(ent, PRINT_HIGH, "You are not interacting with a shopkeeper!\n");
		return;
	}

	switch (choice) {
	case 1:
		if (ent->client->currency >= shopkeeper.seed_price) {
			ent->client->currency -= shopkeeper.seed_price;
			ent->client->seed_stock += 5;
			gi.cprintf(ent, PRINT_HIGH, "You bought 5 seeds! Remaining currency: %d\n", ent->client->currency);
		}
		else {
			gi.cprintf(ent, PRINT_HIGH, "You don't have enough currency to buy seeds!\n");
		}
		break;
	case 2:
		if (ent->client->currency >= shopkeeper.tool_price) {
			ent->client->currency -= shopkeeper.tool_price;
			// Check if the player already has the shotgun
			if (!(ent->client->pers.inventory[ITEM_INDEX(FindItem("Shotgun"))])) {
				// Add the shotgun to the player's inventory
				gitem_t* shotgun = FindItem("Shotgun");
				ent->client->pers.inventory[ITEM_INDEX(shotgun)] = 1; // Add one shotgun
				// Add some initial ammo for the shotgun
				gitem_t* shells = FindItem("Shells");
				Add_Ammo(ent, shells, 10); // Add 10 shells

				gi.cprintf(ent, PRINT_HIGH, "You bought a shotgun! Remaining currency: %d\n", ent->client->currency);
			}
			else {
				gi.cprintf(ent, PRINT_HIGH, "You already own a shotgun!\n");
				ent->client->currency += shopkeeper.tool_price; // Refund
			}
		}
		else {
			gi.cprintf(ent, PRINT_HIGH, "You don't have enough currency to buy a shotgun!\n");
		}
		break;
	case 3:
		gi.cprintf(ent, PRINT_HIGH, "Thank you for visiting the shop!\n");
		ent->client->interacting_with_shopkeeper = false;
		break;
	default:
		gi.cprintf(ent, PRINT_HIGH, "Invalid choice!\n");
		break;
	}
}