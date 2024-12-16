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

game_locals_t	game;
level_locals_t	level;
game_import_t	gi;
game_export_t	globals;
spawn_temp_t	st;

int	sm_meat_index;
int	snd_fry;
int meansOfDeath;

edict_t		*g_edicts;

cvar_t	*deathmatch;
cvar_t	*coop;
cvar_t	*dmflags;
cvar_t	*skill;
cvar_t	*fraglimit;
cvar_t	*timelimit;
cvar_t	*password;
cvar_t	*spectator_password;
cvar_t	*maxclients;
cvar_t	*maxspectators;
cvar_t	*maxentities;
cvar_t	*g_select_empty;
cvar_t	*dedicated;

cvar_t	*filterban;

cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;

cvar_t	*sv_rollspeed;
cvar_t	*sv_rollangle;
cvar_t	*gun_x;
cvar_t	*gun_y;
cvar_t	*gun_z;

cvar_t	*run_pitch;
cvar_t	*run_roll;
cvar_t	*bob_up;
cvar_t	*bob_pitch;
cvar_t	*bob_roll;

cvar_t	*sv_cheats;

cvar_t	*flood_msgs;
cvar_t	*flood_persecond;
cvar_t	*flood_waitdelay;

cvar_t	*sv_maplist;

void SpawnEntities (char *mapname, char *entities, char *spawnpoint);
void ClientThink (edict_t *ent, usercmd_t *cmd);
qboolean ClientConnect (edict_t *ent, char *userinfo);
void ClientUserinfoChanged (edict_t *ent, char *userinfo);
void ClientDisconnect (edict_t *ent);
void ClientBegin (edict_t *ent);
void ClientCommand (edict_t *ent);
void RunEntity (edict_t *ent);
void WriteGame (char *filename, qboolean autosave);
void ReadGame (char *filename);
void WriteLevel (char *filename);
void ReadLevel (char *filename);
void InitGame (void);
void G_RunFrame (void);


//===================================================================


void ShutdownGame (void)
{
	gi.dprintf ("==== ShutdownGame ====\n");

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	gi.error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.dprintf ("%s", text);
}

#endif

//======================================================================


/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse || !ent->client)
			continue;
		ClientEndServerFrame (ent);
	}

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent;

	ent = G_Spawn ();
	ent->classname = "target_changelevel";
	Com_sprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
	ent->map = level.nextmap;
	return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel (void)
{
	edict_t		*ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	// stay on same level flag
	if ((int)dmflags->value & DF_SAME_LEVEL)
	{
		BeginIntermission (CreateTargetChangeLevel (level.mapname) );
		return;
	}

	// see if it's in the map list
	if (*sv_maplist->string) {
		s = strdup(sv_maplist->string);
		f = NULL;
		t = strtok(s, seps);
		while (t != NULL) {
			if (Q_stricmp(t, level.mapname) == 0) {
				// it's in the list, go to the next one
				t = strtok(NULL, seps);
				if (t == NULL) { // end of list, go to first one
					if (f == NULL) // there isn't a first one, same level
						BeginIntermission (CreateTargetChangeLevel (level.mapname) );
					else
						BeginIntermission (CreateTargetChangeLevel (f) );
				} else
					BeginIntermission (CreateTargetChangeLevel (t) );
				free(s);
				return;
			}
			if (!f)
				f = t;
			t = strtok(NULL, seps);
		}
		free(s);
	}

	if (level.nextmap[0]) // go to a specific map
		BeginIntermission (CreateTargetChangeLevel (level.nextmap) );
	else {	// search for a changelevel
		ent = G_Find (NULL, FOFS(classname), "target_changelevel");
		if (!ent)
		{	// the map designer didn't include a changelevel,
			// so create a fake ent that goes back to the same level
			BeginIntermission (CreateTargetChangeLevel (level.mapname) );
			return;
		}
		BeginIntermission (ent);
	}
}

/*
=================
CheckDMRules
=================
*/
void CheckDMRules (void)
{
	int			i;
	gclient_t	*cl;

	if (level.intermissiontime)
		return;

	if (!deathmatch->value)
		return;

	if (timelimit->value)
	{
		if (level.time >= timelimit->value*60)
		{
			gi.bprintf (PRINT_HIGH, "Timelimit hit.\n");
			EndDMLevel ();
			return;
		}
	}

	if (fraglimit->value)
	{
		for (i=0 ; i<maxclients->value ; i++)
		{
			cl = game.clients + i;
			if (!g_edicts[i+1].inuse)
				continue;

			if (cl->resp.score >= fraglimit->value)
			{
				gi.bprintf (PRINT_HIGH, "Fraglimit hit.\n");
				EndDMLevel ();
				return;
			}
		}
	}
}


/*
=============
ExitLevel
=============
*/
void ExitLevel (void)
{
	int		i;
	edict_t	*ent;
	char	command [256];

	Com_sprintf (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString (command);
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissiontime = 0;
	ClientEndServerFrames ();

	// clear some things before going to next level
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse)
			continue;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
	}

}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame (void)
{
	int		i;
	edict_t	*ent;

	level.framenum++;
	level.time = level.framenum*FRAMETIME;

	// choose a client for monsters to target this frame
	AI_SetSightClient ();

	// exit intermissions

	if (level.exitintermission)
	{
		ExitLevel ();
		return;
	}

	//
	// treat each object in turn
	// even the world gets a chance to think
	//
	ent = &g_edicts[0];
	for (i=0 ; i<globals.num_edicts ; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		level.current_entity = ent;

		VectorCopy (ent->s.origin, ent->s.old_origin);

		// if the ground entity moved, make sure we are still on it
		if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount))
		{
			ent->groundentity = NULL;
			if ( !(ent->flags & (FL_SWIM|FL_FLY)) && (ent->svflags & SVF_MONSTER) )
			{
				M_CheckGround (ent);
			}
		}

		if (i > 0 && i <= maxclients->value)
		{
			ClientBeginServerFrame (ent);
			continue;
		}

		G_RunEntity (ent);
	}

	// see if it is time to end a deathmatch
	CheckDMRules ();

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();
}

//JL START FARMING FUNCTIONS
//note: most of these 

//crop attribute init
crop_attributes_t crop_attributes[] = {
	{ CROPTYPE_WHEAT, "Wheat", WHEAT_MODEL, 10.0f, 10.0f },
	{ CROPTYPE_CORN, "Corn", CORN_MODEL, 12.0f, 12.0f },
	{ CROPTYPE_TOMATO, "Tomato", TOMATO_MODEL, 8.0f, 8.0f },
	{ CROPTYPE_POTATO, "Potato", POTATO_MODEL, 15.0f, 15.0f },
	{ CROPTYPE_CARROT, "Carrot", CARROT_MODEL, 9.0f, 9.0f },
	{ CROPTYPE_BEAN, "Bean", BEAN_MODEL, 7.0f, 7.0f },
	{ CROPTYPE_PEPPER, "Pepper", PEPPER_MODEL, 11.0f, 11.0f },
	{ CROPTYPE_CABBAGE, "Cabbage", CABBAGE_MODEL, 14.0f, 14.0f },
	{ CROPTYPE_ONION, "Onion", ONION_MODEL, 13.0f, 13.0f },
	{ CROPTYPE_GARLIC, "Garlic", GARLIC_MODEL, 10.0f, 10.0f }
};

void InitCrop(edict_t* crop, edict_t* field, int type) {
	crop_t* new_crop = gi.TagMalloc(sizeof(crop_t), TAG_GAME);
	if (!new_crop) {
		gi.dprintf("InitCrop: Failed to allocate memory for crop data!\n");
		G_FreeEdict(crop);
		return;
	}

	// Find the crop attributes based on the crop type
	crop_attributes_t* attributes = NULL;
	for (int i = 0; i < sizeof(crop_attributes) / sizeof(crop_attributes[0]); i++) {
		if (crop_attributes[i].type == type) {
			attributes = &crop_attributes[i];
			break;
		}
	}

	// Handle case where no attributes are found
	if (!attributes) {
		gi.dprintf("InitCrop: Unknown crop type!\n");
		G_FreeEdict(crop);
		return;
	}

	new_crop->type = type;
	new_crop->growth_stage = 0;
	new_crop->grow_time = level.time + attributes->initial_grow_time; // Initial growth time
	field->plantedcrop = new_crop;

	crop->owner = field; // Link crop to its field
	crop->movetype = MOVETYPE_NONE;
	crop->solid = SOLID_BBOX;
	crop->s.modelindex = gi.modelindex(attributes->model);

	VectorCopy(field->s.origin, crop->s.origin);

	gi.linkentity(crop); // Add crop to the game world

	crop->think = CropThink;
	crop->nextthink = level.time + 0.1f;

	gi.dprintf("InitCrop: %s initialized successfully at (%f, %f, %f).\n",
		attributes->name, field->s.origin[0], field->s.origin[1], field->s.origin[2]);
}

void CropThink(edict_t* crop) {
	if (!crop || !crop->owner || !crop->owner->plantedcrop) {
		gi.dprintf("CropThink: Invalid crop or owner data! Terminating.\n");
		G_FreeEdict(crop); // Remove invalid crop
		return;
	}

	crop_t* crop_data = crop->owner->plantedcrop;

	if (!crop_data) {
		gi.dprintf("CropThink: No crop data found! Terminating.\n");
		G_FreeEdict(crop); // Remove invalid crop
		return;
	}

	// Find the crop attributes based on the crop type
	crop_attributes_t* attributes = NULL;
	for (int i = 0; i < sizeof(crop_attributes) / sizeof(crop_attributes[0]); i++) {
		if (crop_attributes[i].type == crop_data->type) {
			attributes = &crop_attributes[i];
			break;
		}
	}

	if (!attributes) {
		gi.dprintf("CropThink: Unknown crop type!\n");
		G_FreeEdict(crop); // Remove invalid crop
		return;
	}

	// Check growth stage and time
	if (level.time >= crop_data->grow_time) {
		crop_data->growth_stage++;
		crop_data->grow_time = level.time + attributes->growth_interval; // Next growth stage

		gi.dprintf("CropThink: %s grew to stage %d at (%f, %f, %f).\n",
			attributes->name, crop_data->growth_stage, crop->s.origin[0], crop->s.origin[1], crop->s.origin[2]);

		// Update model or properties based on growth stage and crop type
		switch (crop_data->type) {
		case CROPTYPE_WHEAT:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
					
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Wheat is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_CORN:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Corn is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_TOMATO:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md22");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Tomato is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_POTATO:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Potato is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_CARROT:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Carrot is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_BEAN:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Bean is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_PEPPER:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Pepper is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_CABBAGE:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Cabbage is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_ONION:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Onion is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		case CROPTYPE_GARLIC:
			if (crop_data->growth_stage == 1) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage == 2) {
				crop->s.modelindex = gi.modelindex("models/objects/tree/tris.md2");
			}
			else if (crop_data->growth_stage >= 3) {
				gi.dprintf("CropThink: Garlic is fully grown!\n");
				return; // Fully grown, stop growth
			}
			break;
		}
	}

	// Schedule the next think only if the crop is still valid
	if (crop && crop->inuse) {
		crop->nextthink = level.time + 0.1f;
	}
	else {
		gi.dprintf("CropThink: Crop is no longer valid. Terminating.\n");
	}
}

void FieldThink(edict_t* self) {
	qboolean isTouching = false;

	// Iterate through all entities in the game world
	for (int i = 1; i < globals.num_edicts; i++) {
		edict_t* other = &g_edicts[i];

		// Skip invalid entities
		if (!other->inuse || !other->client || other->health <= 0) {
			continue;
		}

		// Check if the player overlaps with the trigger field's bounds
		if (other->absmin[0] > self->absmax[0] || other->absmax[0] < self->absmin[0] ||
			other->absmin[1] > self->absmax[1] || other->absmax[1] < self->absmin[1] ||
			other->absmin[2] > self->absmax[2] || other->absmax[2] < self->absmin[2]) {
			continue; // Player is outside the field
		}

		// A player is touching the field
		isTouching = true;

		if (!self->touching_field) {
			self->touching_field = true;
			self->touching_entity = other;  // Update touching_entity
			gi.dprintf("Player %s started touching the field.\n", other->client->pers.netname);
		}
		break;
	}

	if (!isTouching) {
		// No players are touching the field anymore
		if (self->touching_field) {
			self->touching_field = false;
			self->touching_entity = NULL;  // Reset touching_entity
			gi.dprintf("No players are touching the field anymore.\n");
		}
	}

	// Schedule the next think
	self->nextthink = level.time + 0.1f;
}

// shopkeeper init
shopkeeper_t shopkeeper = {
	.name = "Shopkeeper",
	.seed_price = 5,
	.tool_price = 20
};

