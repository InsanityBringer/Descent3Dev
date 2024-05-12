/*
* Descent 3
* Copyright (C) 2024 Parallax Software
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Inventory.h"

#include "mono.h"
#include "player.h"
#include "pserror.h"
#include "objinfo.h"
#include "room.h"
#include "weapon.h"
#include "game.h"
#include "multi.h"
#include "osiris_dll.h"
#include "mem.h"
#include "ObjScript.h"
#include "osiris_share.h"
#include "stringtable.h"
#include "hud.h"
#include "hlsoundlib.h"
#include "sounds.h"
#include "AIMain.h"
#include "levelgoal.h"

//constructor
Inventory::Inventory(void)
{
	//mprintf((0,"Inventory System: Initialize\n"));
	root = NULL;
	count = 0;
	pos = NULL;
}

//destructor
Inventory::~Inventory(void)
{
	Reset(false);
}

//Resets the inventory, cleaning it out
//in_game: set to true if this is being called from during gameplay
// reset_stage:
//		INVRESET_ALL:			Reset _EVERYTHING_
//		INVRESET_LEVELCHANGE:	Remove everything except those that last across levels
//		INVRESET_DEATHSPEW:		Remove everything except those that do not spew (Default)
void Inventory::Reset(bool in_game, int reset_stage)
{
	if (reset_stage < 0 || reset_stage>2)
		reset_stage = 2;

	inven_item* current = root, * next, * new_root;

	int item_count = count;
	new_root = NULL;
	object* obj;

	bool remove_nonspewers = false;
	bool remove_levelchangers = false;

	switch (reset_stage)
	{
	case INVRESET_ALL:	//everything (mission over)
		remove_nonspewers = true;
		remove_levelchangers = true;
		break;
	case INVRESET_LEVELCHANGE:	//leave level static (level change)
		remove_nonspewers = true;
		remove_levelchangers = false;
		break;
	case INVRESET_DEATHSPEW:	//leave non spewers (player death spew reset)
		remove_nonspewers = false;
		remove_levelchangers = true;
		break;
	}

	bool no_spew, leave_across_level, should_spew;

	while (item_count)
	{
		no_spew = false;
		leave_across_level = false;
		should_spew = true;

		next = current->next;

		if (current->iflags & INVF_NOTSPEWABLE)
			no_spew = true;
		if (current->iflags & INVF_MISSIONITEM)
			leave_across_level = true;

		//determine if we should spew
		if (leave_across_level && !remove_levelchangers && (!(current->iflags & INVF_OBJECT)))
			should_spew = false;
		if (no_spew && !remove_nonspewers)
			should_spew = false;

		if (in_game && current->iflags & INVF_OBJECT && should_spew)
		{
			//unmark this object as being in an inventory
			obj = ObjGet(current->type);
			if (obj)
			{
				obj->flags &= ~OF_INPLAYERINVENTORY;
			}
		}

		if (should_spew)
			RemoveNode(current);

		item_count--;
		current = next;
	}
	ValidatePos(true);
}

//adds an object to the inventory (marked by it's objhandle)
bool Inventory::AddObject(int object_handle, int flags, char* description)
{
	//make sure we can fit another object
	if (count >= MAX_UNIQUE_INVEN_ITEMS)
	{
		mprintf((0, "Max unique count hit on add to inventory\n"));
		return false;
	}

	object* obj = ObjGet(object_handle);
	if (!obj)
	{
		mprintf((0, "INVEN: Invalid object trying to be added\n"));
		return false;
	}

	if (obj->flags & OF_INFORM_DESTROY_TO_LG)
	{
		Level_goals.Inform(LIT_OBJECT, LGF_COMP_DESTROY, obj->handle);
	}

	bool in_as_dummy = false;
	if (obj->type == OBJ_DUMMY)
	{
		//type coming in is already dummy, un-dummy before adding it
		ObjUnGhostObject(OBJNUM(obj));
		in_as_dummy = true;
	}

	ASSERT(obj->type == OBJ_BUILDING || obj->type == OBJ_ROBOT || obj->type == OBJ_POWERUP || obj->type == OBJ_CLUTTER);

	inven_item* current = root, * prev = root, * newnode;

	if (count == 0)
	{
		//there are no items in the list...time to add
		root = new inven_item;
		newnode = root;
		root->next = root;
		root->prev = root;
		newnode->iflags = INVF_OBJECT;
		newnode->count = 1;
	}
	else
	{
		newnode = new inven_item;

		prev = root->prev;

		newnode->prev = prev;
		prev->next = newnode;
		newnode->next = root;
		root->prev = newnode;

		newnode->iflags = INVF_OBJECT;
		newnode->count = 1;
	}

	newnode->type = object_handle;
	newnode->id = -1;
	newnode->flags = 0;
	newnode->otype = obj->type;
	newnode->oid = obj->id;

	if (Object_info[newnode->oid].description)
	{
		newnode->description = mem_strdup(Object_info[newnode->oid].description);
	}
	else
	{
		newnode->description = (char*)mem_malloc(sizeof(char));
		newnode->description[0] = 0;
	}

	if (Object_info[newnode->oid].flags & OIF_INVEN_SELECTABLE)
		newnode->iflags |= INVF_SELECTABLE;
	//if(!(Object_info[newnode->oid].flags & OIF_INVEN_NONUSEABLE))
	//	newnode->iflags |= INVF_USEABLE;
	if (Object_info[newnode->oid].flags & OIF_INVEN_TYPE_MISSION)
		newnode->iflags |= INVF_MISSIONITEM;
	if (Object_info[newnode->oid].flags & OIF_INVEN_NOREMOVE)
		newnode->iflags |= INVF_NOREMOVEONUSE;
	if (Object_info[newnode->oid].flags & OIF_INVEN_VISWHENUSED)
		newnode->iflags |= INVF_VISWHENUSED;

	if (flags & INVAF_NOTSPEWABLE)
		newnode->iflags |= INVF_NOTSPEWABLE;
	if (flags & INVAF_TIMEOUTONSPEW)
		newnode->iflags |= INVF_TIMEOUTONSPEW;
	if (flags & INVAF_LEVELLAST)
		newnode->iflags |= INVF_MISSIONITEM;


	obj->flags |= OF_INPLAYERINVENTORY;

	if (in_as_dummy || (!(Game_mode & GM_MULTI)) || Netgame.local_role == LR_SERVER)
	{
		ObjGhostObject(OBJNUM(obj));

		if (Game_mode & GM_MULTI && Netgame.local_role == LR_SERVER)
		{
			MultiSendGhostObject(obj, true);
		}
	}

	newnode->icon_name = mem_strdup(Object_info[newnode->oid].icon_name);

	if (description)
	{
		newnode->name = mem_strdup(description);
	}
	else
	{
		newnode->name = mem_strdup(Object_info[newnode->oid].name);
	}

	count++;

	if (newnode->iflags & INVF_SELECTABLE)
		pos = newnode;

	return true;
}

//adds a new type/id item to the inventory
bool Inventory::Add(int type, int id, object* parent, int aux_type, int aux_id, int flags, char* description)
{
	//make sure we can fit another object
	if (count >= MAX_UNIQUE_INVEN_ITEMS)
	{
		mprintf((0, "Max unique count hit on add to inventory\n"));
		return false;
	}

	if ((type < 0) || (type == OBJ_NONE))
	{
		mprintf((0, "Invalid type on add to inventory\n"));
		return false;
	}

	if (type != OBJ_WEAPON)
	{
		ASSERT(type == OBJ_BUILDING || type == OBJ_ROBOT || type == OBJ_POWERUP || type == OBJ_CLUTTER);
		return AddObjectItem(type, id, (aux_type != -1) ? aux_type : type, (aux_id != -1) ? aux_id : id, flags, description);
	}
	else
	{
		//special case for countermeasures
		return AddCounterMeasure(id, aux_type, aux_id, flags, description);
	}

	return false;
}

//adds a special cased CounterMeasure into the inventory
bool Inventory::AddCounterMeasure(int id, int aux_type, int aux_id, int flags, char* description)
{
	//make sure we can fit another object
	if (count >= MAX_UNIQUE_INVEN_ITEMS)
	{
		mprintf((0, "Hit max unique in counter measure add\n"));
		return false;
	}

	inven_item* current = root, * prev = root, * newnode;

	if (count == 0)
	{
		//there are no items in the list...time to add
		root = new inven_item;
		newnode = root;
		root->next = root;
		root->prev = root;
		newnode->count = 1;
	}
	else
	{
		newnode = FindItem(OBJ_WEAPON, id);

		if (!newnode)
		{
			//NEEDTODO: adjust so it adds in order

			newnode = new inven_item;

			prev = root->prev;

			newnode->prev = prev;
			prev->next = newnode;
			newnode->next = root;
			root->prev = newnode;

			newnode->count = 1;
		}
		else
		{
			//there is an item of that type/id already, just increase it's count
			newnode->count++;
			//mprintf((0,"Inventory: Item #%d (%s) Count increased to %d\n",count,newnode->name,newnode->count));
		}
	}

	//its a new item type/id, so fill in its info
	if (newnode->count == 1)
	{
		newnode->type = OBJ_WEAPON;
		newnode->id = id;
		newnode->flags = 0;
		newnode->iflags = 0;
		newnode->otype = aux_type;
		newnode->oid = aux_id;

		if ((aux_type != -1) && (aux_id != -1) && (Object_info[aux_id].description))
		{
			newnode->description = mem_strdup(Object_info[aux_id].description);
		}
		else
		{
			newnode->description = mem_strdup(Weapons[id].name);
		}

		newnode->iflags |= INVF_SELECTABLE | INVF_USEABLE | INVF_MISSIONITEM | INVF_TIMEOUTONSPEW;

		if (Weapons[id].icon_handle >= 0)
		{
			newnode->icon_name = (char*)mem_malloc(strlen(GameBitmaps[GameTextures[Weapons[id].icon_handle].bm_handle].name) + 1);
			strcpy(newnode->icon_name, GameBitmaps[GameTextures[Weapons[id].icon_handle].bm_handle].name);
		}
		else
			newnode->icon_name = nullptr;

		if (description)
			newnode->name = mem_strdup(description);
		else
			newnode->name = mem_strdup(Weapons[id].name);
		count++;
		//mprintf((0,"Inventory: Item #%d Added Countermeasure (%s) ID=%d\n",count,newnode->name,newnode->id));
	}

	pos = newnode;
	return true;
}

//adds an object to the inventory
bool Inventory::AddObjectItem(int otype, int oid, int oauxt, int oauxi, int flags, char* description)
{
	//make sure we can fit another object
	if (count >= MAX_UNIQUE_INVEN_ITEMS)
		return false;

	inven_item* current = root, * prev = root, * newnode;

	if (count == 0)
	{
		//there are no items in the list...time to add
		root = new inven_item;
		newnode = root;
		root->next = root;
		root->prev = root;
		newnode->count = 1;
	}
	else
	{
		newnode = FindItem(otype, oid);

		if (!newnode)
		{
			//NEEDTODO: adjust so it adds in order

			newnode = new inven_item;

			prev = root->prev;

			newnode->prev = prev;
			prev->next = newnode;
			newnode->next = root;
			root->prev = newnode;

			newnode->count = 1;
		}
		else
		{
			//there is an item of that type/id already, just increase it's count
			newnode->count++;
		}
	}

	//its a new item type/id, so fill in its info
	if (newnode->count == 1)
	{
		newnode->type = otype;
		newnode->id = oid;
		newnode->flags = 0;
		newnode->iflags = 0;
		newnode->otype = oauxt;
		newnode->oid = oauxi;

		if (Object_info[oid].description)
		{
			newnode->description = (char*)mem_malloc(strlen(Object_info[oid].description) + 1);
			strcpy(newnode->description, Object_info[oid].description);
		}
		else
		{
			newnode->description = (char*)mem_malloc(sizeof(char));
			newnode->description[0] = 0;
		}

		if (Object_info[oid].flags & OIF_INVEN_SELECTABLE)
			newnode->iflags |= INVF_SELECTABLE;
		//if(!(Object_info[oid].flags & OIF_INVEN_NONUSEABLE))
		//	newnode->iflags |= INVF_USEABLE;
		if (Object_info[oid].flags & OIF_INVEN_TYPE_MISSION)
			newnode->iflags |= INVF_MISSIONITEM;
		if (Object_info[oid].flags & OIF_INVEN_NOREMOVE)
			newnode->iflags |= INVF_NOREMOVEONUSE;
		if (Object_info[oid].flags & OIF_INVEN_VISWHENUSED)
			newnode->iflags |= INVF_VISWHENUSED;

		if (flags & INVAF_NOTSPEWABLE)
			newnode->iflags |= INVF_NOTSPEWABLE;
		if (flags & INVAF_TIMEOUTONSPEW)
			newnode->iflags |= INVF_TIMEOUTONSPEW;
		if (flags & INVAF_LEVELLAST)
			newnode->iflags |= INVAF_LEVELLAST;

		newnode->icon_name = (char*)mem_malloc(strlen(Object_info[oid].icon_name) + 1);
		strcpy(newnode->icon_name, Object_info[oid].icon_name);

		if (description)
			newnode->name = mem_strdup(description);
		else
			newnode->name = mem_strdup(Object_info[oid].name);

		count++;
	}

	if (newnode->iflags & INVF_SELECTABLE)
		pos = newnode;

	return true;
}

//uses an item in the inventory (returns false if the item doesn't exist)
bool Inventory::Use(int type, int id, object* parent)
{
	inven_item* node;

	node = FindItem(type, id);

	if (!node)
		return false;

	if (!(node->iflags & INVF_USEABLE))
		return false;

	bool multiplayer = (bool)((Game_mode & GM_MULTI) != 0);
	bool client;
	bool server;
	bool ret = false;

	if (multiplayer)
	{
		if (Netgame.local_role & LR_SERVER)
		{
			client = false;
			server = true;
		}
		else
		{
			client = true;
			server = false;
		}
	}
	else {
		client = false;
		server = false;
	}

	if (client)
	{
		//OK, we're a client in a multiplayer game, so send a request to the server to use this Item
		SendRequestToServerToUse(node->type, node->id);

		return false;
	}

	//If we got here, then we are either the server in a multiplayer game, or in a single player game


	//get player object (needed when we recreate the object)
	object* player;
	ASSERT(parent);
	player = parent;

	if (player == NULL)
	{
		Int3();
		return false;
	}

	//if type is OBJ_WEAPON then it's a countermeasure
	if (type == OBJ_WEAPON)
	{
		mprintf((0, "CounterMeasures: Use\n"));
		//countermeasure
		CreateCountermeasureFromObject(player, id);
		Remove(node->type, node->id);
		ret = true;
	}
	else
	{
		mprintf((0, "Inventory: Use\n"));
		//regular
		//recreate the object
		int objnum;
		int roomnum;
		bool remove_on_use;
		bool vis_when_created = false;

		if (node->iflags & INVF_NOREMOVEONUSE)
			remove_on_use = false;
		else
			remove_on_use = true;

		if (node->iflags & INVF_VISWHENUSED)
			vis_when_created = true;
		else
			vis_when_created = false;

		roomnum = player->roomnum;

		if (node->iflags & INVF_OBJECT)
		{
			//don't recreate the object..it already exists
			object* obj = ObjGet(node->type);
			if (!obj)
			{
				Int3();	//object no longer exists
				return false;
			}
			objnum = OBJNUM(obj);

			obj->flags &= ~OF_INPLAYERINVENTORY;

			if (vis_when_created)
			{
				ObjUnGhostObject(objnum);
				MultiSendGhostObject(obj, false);
			}

		}
		else
		{

			objnum = ObjCreate(node->type, node->id, roomnum, &player->pos, NULL, player->handle);
			if (objnum == -1)
			{
				Int3();
				return false;
			}

			if (!vis_when_created)
			{
				if (Objects[objnum].control_type != CT_AI)
					SetObjectControlType(&Objects[objnum], CT_NONE);

				Objects[objnum].movement_type = MT_NONE;
				Objects[objnum].render_type = RT_NONE;
			}

			Objects[objnum].flags = node->flags;

			if (server)//if we're the server, then we need to send this object to the clients
				MultiSendObject(&Objects[objnum], 0);

			InitObjectScripts(&Objects[objnum]);

		}

		tOSIRISEventInfo ei;
		ei.evt_use.it_handle = player->handle;

		// zar: node might be invalid after CallEvent.
		int type = node->type, id = node->id;
		if (Osiris_CallEvent(&Objects[objnum], EVT_USE, &ei))
		{
			//if we're the server tell the clients to remove this item from their inventory
			Remove(type, id);
			ret = true;
		}
		else
		{
			if (node->iflags & INVF_OBJECT)
				Objects[objnum].flags |= OF_INPLAYERINVENTORY;	//mark as being in inventory
		}

		if (remove_on_use)
		{
			//now we need to kill the object
			SetObjectDeadFlag(&Objects[objnum], true);
		}
	}
	return ret;
}

//sends a request to the server to use a particular item in the inventory
void Inventory::SendRequestToServerToUse(int type, int id)
{
	//mprintf((0,"Sending request to server for T=%d ID=%d\n",type,id));
	inven_item* node = FindItem(type, id);
	if (node)
		MultiSendClientInventoryUseItem(type, id);
	else
		mprintf((0, "Sorry couldn't find it in your inventory\n"));
}

//searches the inventory for the specified type/id, sets the pos to it
bool Inventory::FindPos(int type, int id)
{
	int oldt, oldi;
	int ttype, tid;

	//save current pos
	GetPosTypeID(oldt, oldi);

	//try to move to the specified pos
	GotoPos(type, id);

	//see if we got there
	GetPosTypeID(ttype, tid);
	if ((ttype == type) && (tid == id))
		return true;
	else
	{
		//nope, so restore the old pos
		GotoPos(oldt, oldi);
		return false;
	}
}

//uses an item in the inventory (currently selected one) (returns false if the item doesn't exist)
bool Inventory::UsePos(object* parent)
{
	if (pos)
		return Use(pos->type, pos->id, parent);
	else
		return false;
}

bool Inventory::Use(int objhandle, object* parent)
{
	return Use(objhandle, -1, parent);
}

//removes an item from inventory, without using it (returns true on success, false if object didn't exist)
bool Inventory::Remove(int type, int id)
{
	inven_item* node;

	node = FindItem(type, id);

	if (!node)
		return false;

	if (node->iflags & INVF_OBJECT)
	{
		//always remove
		object* obj = ObjGet(type);
		ASSERT(obj);
		if (obj)
			obj->flags &= ~OF_INPLAYERINVENTORY;

		RemoveNode(node);
	}
	else
	{
		node->count--;
		mprintf((0, "Inventory System: Remove\n"));

		if (node->count <= 0)
			RemoveNode(node);
	}

	return true;
}

//removes a node from the list, decrementing the count
void Inventory::RemoveNode(inven_item* node)
{
	if (!node)
		return;

	bool movepos = false;
	count--;
	inven_item* prev, * next;

	prev = node->prev;
	next = node->next;

	if (node->description)
		mem_free(node->description);
	if (node->icon_name)
		mem_free(node->icon_name);
	if (node->name)
		mem_free(node->name);

	if (pos == node)
	{
		if (pos->next != node)
		{
			movepos = true;
			pos = pos->next;
		}
		else
			pos = NULL;
	}


	if (node == root)
	{
		if (root->next != root)
		{
			inven_item* n = root->next;
			inven_item* p = root->prev;
			delete node;
			root = n;
			root->prev = p;
			p->next = root;
		}
		else
		{
			delete node;
			root = NULL;
		}
	}
	else
	{
		delete node;
		prev->next = next;
		next->prev = prev;
	}

	if (movepos)
		ValidatePos();
}


//given a type and id, it returns the first matching inventory item
inven_item* Inventory::FindItem(int type, int id)
{
	inven_item* current = root;

	if (count == 0)
		return NULL;	//there are no items, don't even bother

	int counter = count;

	while (counter)
	{
		if ((current->type == type) && (current->id == id))	//we got a match
		{
			//mprintf((0,"Inventory: FindItem found Type(%d) ID(%d)\n",type,id));
			return current;
		}

		current = current->next;
		counter--;
	}

	//mprintf((0,"Inventory: FindItem couldn't find Type(%d) ID(%d)\n",type,id));
	return NULL;
}


//returns how many items are in the inventory
int Inventory::Size(void)
{
	//mprintf((0,"Inventory System: Size\n"));
	return count;
}

//returns true if there is an item in the inventory with the given type/id
bool Inventory::CheckItem(int type, int id)
{
	//mprintf((0,"Inventory System: CheckItem\n"));
	if (FindItem(type, id))
		return true;
	else
		return false;
}

//saves the inventory to the file (returns number of bytes written)
int Inventory::SaveInventory(CFILE* file)
{
	int num_items = Size();

	int start_pos = cftell(file);

	int pos_pos = 0;
	int pos_count = 0;

	cf_WriteInt(file, num_items);
	if (num_items > 0)
	{
		inven_item* curr;
		curr = root;

		while (num_items > 0)
		{
			if (pos == curr)
			{
				pos_pos = pos_count;
			}

			if (curr->id == -1)
			{
				//make sure it is a valid object
				object* obj = ObjGet(curr->type);
				ASSERT(obj);
				if (!obj)
				{
					mprintf((0, "Invalid object saving inventory\n"));
					curr = curr->next;
					num_items--;
					pos_count++;
					continue;
				}
			}

			cf_WriteInt(file, curr->type);
			cf_WriteInt(file, curr->otype);
			cf_WriteInt(file, curr->id);
			cf_WriteInt(file, curr->oid);
			cf_WriteInt(file, curr->flags);
			cf_WriteInt(file, curr->count);

			if (curr->description)
				cf_WriteString(file, curr->description);
			else
				cf_WriteByte(file, 0);

			if (curr->icon_name)
				cf_WriteString(file, curr->icon_name);
			else
				cf_WriteByte(file, 0);

			if (curr->name)
				cf_WriteString(file, curr->name);
			else
				cf_WriteByte(file, 0);

			cf_WriteInt(file, curr->iflags);

			curr = curr->next;
			num_items--;
			pos_count++;
		}
	}

	cf_WriteInt(file, pos_pos);

	int end_pos = cftell(file);

	return (end_pos - start_pos);
}


//restores the inventory from file (returns number of bytes read)
int Inventory::ReadInventory(CFILE* file)
{
	int start_pos = cftell(file);

	int num_items = cf_ReadInt(file);
	count = num_items;
	root = NULL;
	char temp[512];

	int t, i, otype;

	if (num_items > 0)
	{
		inven_item* item, * prev;

		while (num_items > 0)
		{
			t = cf_ReadInt(file);
			otype = cf_ReadInt(file);
			i = cf_ReadInt(file);

			//make sure the object is valid
			if (i == -1)
			{
				object* obj = ObjGet(t);
				ASSERT(obj);
				if (!obj)
				{
					mprintf((0, "Invalid object restoring inventory\n"));
					//skip this object
					cf_ReadInt(file);
					cf_ReadInt(file);
					cf_ReadInt(file);
					cf_ReadString(temp, 512, file);
					cf_ReadString(temp, 512, file);
					cf_ReadString(temp, 512, file);
					cf_ReadInt(file);
					num_items--;
					count--;
					continue;
				}
			}

			if (root == NULL)
			{
				//there are no items in the list...time to add
				root = new inven_item;
				item = root;
				root->next = root;
				root->prev = root;
			}
			else
			{
				item = new inven_item;

				prev = root->prev;

				item->prev = prev;
				prev->next = item;
				item->next = root;
				root->prev = item;
			}

			item->type = t;
			item->id = i;
			item->otype = otype;
			item->oid = cf_ReadInt(file);
			item->flags = cf_ReadInt(file);
			item->count = cf_ReadInt(file);

			cf_ReadString(temp, 512, file);
			item->description = mem_strdup(temp);

			cf_ReadString(temp, 512, file);
			item->icon_name = mem_strdup(temp);

			cf_ReadString(temp, 512, file);
			item->name = mem_strdup(temp);

			item->iflags = cf_ReadInt(file);

			num_items--;
		}
	}

	int pos_index = cf_ReadInt(file);
	GotoPos(pos_index);
	ValidatePos();

	int end_pos = cftell(file);

	return (end_pos - start_pos);
}


//resets the position pointer in the list to the beginning
void Inventory::ResetPos(void)
{
	pos = root;
}

//moves the position pointer to the next inventory item
void Inventory::NextPos(bool skip)
{
	if ((pos) && (pos->next))
		pos = pos->next;
	else if (!pos)
		pos = root;
	else
		return;

	if (!skip)
		ValidatePos();
}

//moves the position pointer to the previous inventory item
void Inventory::PrevPos(bool skip)
{
	if ((pos) && (pos->prev))
		pos = pos->prev;
	else if (!pos)
		pos = root;
	else
		return;

	if (!skip)
		ValidatePos(false);
}

//returns true if the position pointer is at the begining of the inventory list
bool Inventory::AtBeginning(void)
{
	if (!pos)
		return true;
	if (pos == root)
		return true;
	else
		return false;
}

//returns false if the position pointer is at the end of the inventory list
bool Inventory::AtEnd(void)
{
	if (!pos)
		return true;
	if (pos->next == root)
		return true;
	else
		return false;
}

//returns the type/id of the item at the current position
//returns true if the pos is a real object in the game
//returns false if the pos is just a type/id inventory item
bool Inventory::GetPosTypeID(int& type, int& id)
{
	if (!pos)
	{
		type = id = 0;
		return false;
	}

	type = pos->type;
	id = pos->id;

	if (pos->iflags & INVF_OBJECT)
		return true;
	else
		return false;
}

//returns the aux type/id of the item
//returns true if the pos is a real object in the game
//returns false if the pos is just a type/id inventory item
bool Inventory::GetAuxPosTypeID(int& type, int& id)
{
	if (!pos) {
		type = id = 0;
		return false;
	}
	type = pos->otype;
	id = pos->oid;

	ASSERT(type != OBJ_NONE);

	if (pos->iflags & INVF_OBJECT)
		return true;
	else
		return false;
}

//returns the description of the item at the current position 
char* Inventory::GetPosDescription(void)
{
	//mprintf((0,"Getting Pos Description (%s)\n",pos->description));
	if (!pos)
		return NULL;
	return pos->description;
}

//returns the name of the item at the current position
char* Inventory::GetPosName(void)
{
	if (!pos)
		return NULL;
	return pos->name;
}

//returns the icon name of the item at the current position
char* Inventory::GetPosIconName(void)
{
	if (!pos)
		return NULL;
	return pos->icon_name;
}

//returns the count of the item at the current position
int Inventory::GetPosCount(void)
{
	//mprintf((0,"Getting Pos Count (%d)\n",pos->count));
	if (!pos)
		return 0;

	if (pos->iflags & INVF_OBJECT)
		return 1;

	return pos->count;
}

//return information about the current position item
//returns true if the pos is a real object in the game
//returns false if the pos is just a type/id inventory item
bool Inventory::GetPosInfo(ushort& iflags, int& flags)
{
	if (!pos)
	{
		iflags = 0;
		flags = 0;
		return false;
	}

	iflags = pos->iflags;
	flags = pos->flags;

	if (pos->iflags & INVF_OBJECT)
		return true;
	else
		return false;
}


//goes to a position in the list
void Inventory::GotoPos(int newpos)
{
	//mprintf((0,"Going to Pos (%d)\n",newpos));
	ResetPos();
	int i;
	for (i = 0; i < newpos; i++)
	{
		if ((pos) && (pos->next != NULL))
			pos = pos->next;
	}
}

//moves the current item pointer to the specified type/id
void Inventory::GotoPos(int type, int id)
{
	inven_item* node = FindItem(type, id);

	if (node)
		pos = node;
}

//returns the "index" position of the current item
int Inventory::GetPos(void)
{
	int type, id;
	int ctype, cid;
	int count = 0;
	bool done = false;

	if (!Size())
		return -1;

	bool is_object, cis_object;

	is_object = GetPosTypeID(type, id);
	ResetPos();

	done = AtEnd();

	while (!done)
	{
		cis_object = GetPosTypeID(ctype, cid);
		if ((ctype == type) && (cid == id) && (cis_object == is_object))
			return count;

		count++;
		done = AtEnd();
		NextPos(false);
	}
	return 0;
}

//moves to the next/prev item in the inventory list (forward==TRUE means forward, forward==FALSE means go backwards)
void InventorySwitch(bool forward)
{
	int ctype, cid;

	Players[Player_num].inventory.GetPosTypeID(ctype, cid);

	if (ctype != 0)
	{
		if (forward)
			Players[Player_num].inventory.NextPos();
		else
			Players[Player_num].inventory.PrevPos();

		int ntype, nid;
		Players[Player_num].inventory.GetPosTypeID(ntype, nid);

		if (ntype != ctype || nid != cid)
		{
			//AddHUDMessage(TXT_WPNSELECT, Players[Player_num].inventory.GetPosName());
			Sound_system.Play2dSound(SOUND_CHANGE_INVENTORY);

			ain_hear hear;
			hear.f_directly_player = true;
			hear.hostile_level = 0.0f;
			hear.curiosity_level = 0.3f;
			hear.max_dist = AI_SOUND_SHORT_DIST;
			AINotify(&Objects[Players[Player_num].objnum], AIN_HEAR_NOISE, (void*)&hear);
		}
	}
}

//moves to the next/prev item in the counter measures list (forward==TRUE means forward, forward==FALSE means go backwards)
void CounterMeasuresSwitch(bool forward)
{
	int ctype, cid;

	Players[Player_num].counter_measures.GetPosTypeID(ctype, cid);

	if (ctype != 0)
	{
		if (forward)
			Players[Player_num].counter_measures.NextPos();
		else
			Players[Player_num].counter_measures.PrevPos();

		int ntype, nid;
		Players[Player_num].counter_measures.GetPosTypeID(ntype, nid);

		if (ntype != ctype || nid != cid)
		{
			AddHUDMessage(TXT_WPNSELECT, Players[Player_num].counter_measures.GetPosName());
			Sound_system.Play2dSound(SOUND_CHANGE_COUNTERMEASURE);

			ain_hear hear;
			hear.f_directly_player = true;
			hear.hostile_level = 0.0f;
			hear.curiosity_level = 0.3f;
			hear.max_dist = AI_SOUND_SHORT_DIST;
			AINotify(&Objects[Players[Player_num].objnum], AIN_HEAR_NOISE, (void*)&hear);
		}
	}
}

//repositions the pos so its in the correct spot
void Inventory::ValidatePos(bool forward)
{
	if (!pos)
		return;
	if (pos->iflags & INVF_SELECTABLE)
		return;

	inven_item* node;
	if (forward)
		node = pos->next;
	else
		node = pos->prev;

	while (node != pos)
	{
		if (node->iflags & INVF_SELECTABLE)
		{
			pos = node;
			return;
		}

		if (forward)
			node = node->next;
		else
			node = node->prev;
	}
	pos = NULL;
}

//returns how many of a type/id exists in the inventory
int Inventory::GetTypeIDCount(int type, int id)
{
	inven_item* node = FindItem(type, id);

	if (!node)
		return 0;
	if (node->flags & INVF_OBJECT)
		return 1;

	return node->count;
}

//determines whether the position is selectable
bool Inventory::IsSelectable(void)
{
	if (!pos)
		return false;

	return ((pos->iflags & INVF_SELECTABLE) != 0);
}

//determines whether the position is selectable
bool Inventory::IsUsable(void)
{
	if (!pos)
		return false;

	return ((pos->iflags & INVF_USEABLE) != 0);
}


//gets a detailed list of information about what is in the inventory
//returns the number of items filled in.
int Inventory::GetInventoryItemList(tInvenList* list, int max_amount, int* cur_sel)
{
	ASSERT(cur_sel);
	*cur_sel = -1;

	if (max_amount <= 0)
		return 0;

	inven_item* current = root;
	if (count == 0)
		return 0;	//there are no items, don't even bother

	int counter = count;
	int cur_count = 0;

	while (counter)
	{
		if (cur_count < max_amount)
		{
			if (current == pos)
			{
				//current selected
				*cur_sel = cur_count;
			}

			list[cur_count].amount = current->count;
			list[cur_count].hud_name = current->name;
			list[cur_count].selectable = (current->iflags & INVF_SELECTABLE) ? true : false;
			cur_count++;
		}

		current = current->next;
		counter--;
	}

	return count;
}


//use the currently selected inventory item
bool UseInventoryItem()
{
	int type, id;

	Players[Player_num].inventory.GetPosTypeID(type, id);
	if (!type && !id)
		return false;

	if (Players[Player_num].inventory.UsePos(&Objects[Players[Player_num].objnum]))
	{
		if (Game_mode & GM_MULTI && (Netgame.local_role == LR_SERVER))
			MultiSendInventoryRemoveItem(Player_num, type, id);
	}
	return true;
}

//use the currently selected countermeasure
bool UseCountermeasure()
{
	int type, id;

	Players[Player_num].counter_measures.GetPosTypeID(type, id);
	if (!type && !id)
		return false;

	if (Players[Player_num].counter_measures.UsePos(&Objects[Players[Player_num].objnum]))
	{
		if (Game_mode & GM_MULTI && (Netgame.local_role == LR_SERVER))
			MultiSendInventoryRemoveItem(Player_num, type, id);
	}
	return true;
}

// Checks for an object in any of the players inventorys and removes it 
void InventoryRemoveObject(int objhandle)
{
	object* obj = ObjGet(objhandle);
	ASSERT(obj);
	if (!obj)
		return;

	if (!(obj->flags & OF_INPLAYERINVENTORY))	//not in the player's inventory
		return;

	//go through all the players and look for the object
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (Players[i].inventory.CheckItem(objhandle, -1))
		{
			//this player has it!!
			mprintf((0, "INVEN: Removing dead object from %d\n", i));
			Players[i].inventory.Remove(objhandle, -1);
			return;
		}
	}
}
