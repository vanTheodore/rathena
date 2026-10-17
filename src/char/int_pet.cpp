// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "int_pet.hpp"

#include <cstdlib>
#include <cstring>

#include <common/malloc.hpp>
#include <common/mmo.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/sql.hpp>
#include <common/strlib.hpp>
#include <common/utils.hpp>

#include "char.hpp"
#include "inter.hpp"

struct s_pet *pet_pt;
int32 mapif_load_pet(int32 fd, uint32 account_id, uint32 char_id, int32 pet_id);

//---------------------------------------------------------
int32 inter_pet_tosql(int32 pet_id, struct s_pet* p)
{
	//`pet` (`pet_id`, `class`,`name`,`account_id`,`char_id`,`level`,`egg_id`,`equip`,`intimate`,`hungry`,`rename_flag`,`incubate`,`autofeed`)
	char esc_name[NAME_LENGTH*2+1];// escaped pet name

	Sql_EscapeStringLen(sql_handle, esc_name, p->name, strnlen(p->name, NAME_LENGTH));
	p->hungry = cap_value(p->hungry, 0, 100);
	p->intimate = cap_value(p->intimate, 0, 1000);

	if( pet_id == -1 )
	{// New pet.
		if( SQL_ERROR == Sql_Query(sql_handle, "INSERT INTO `%s` "
			"(`class`,`name`,`account_id`,`char_id`,`level`,`egg_id`,`equip`,`intimate`,`hungry`,`rename_flag`,`incubate`,`autofeed`) "
			"VALUES ('%d', '%s', '%d', '%d', '%d', '%u', '%u', '%d', '%d', '%d', '%d', '%d')",
			schema_config.pet_db, p->class_, esc_name, p->account_id, p->char_id, p->level, p->egg_id,
			p->equip, p->intimate, p->hungry, p->rename_flag, p->incubate, p->autofeed) )
		{
			Sql_ShowDebug(sql_handle);
			return 0;
		}
		p->pet_id = (int32)Sql_LastInsertId(sql_handle);
	}
	else
	{// Update pet.
		if( SQL_ERROR == Sql_Query(sql_handle, "UPDATE `%s` SET `class`='%d',`name`='%s',`account_id`='%d',`char_id`='%d',`level`='%d',`egg_id`='%u',`equip`='%u',`intimate`='%d',`hungry`='%d',`rename_flag`='%d',`incubate`='%d',`autofeed`='%d' WHERE `pet_id`='%d'",
			schema_config.pet_db, p->class_, esc_name, p->account_id, p->char_id, p->level, p->egg_id,
			p->equip, p->intimate, p->hungry, p->rename_flag, p->incubate, p->autofeed, p->pet_id) )
		{
			Sql_ShowDebug(sql_handle);
			return 0;
		}
	}

	if (charserv_config.save_log)
		ShowInfo("Pet saved %d - %s.\n", pet_id, p->name);
	return 1;
}

int32 inter_pet_fromsql(int32 pet_id, struct s_pet* p)
{
	char* data;
	size_t len;

#ifdef NOISY
	ShowInfo("Loading pet (%d)...\n",pet_id);
#endif
	memset(p, 0, sizeof(struct s_pet));

	//`pet` (`pet_id`, `class`,`name`,`account_id`,`char_id`,`level`,`egg_id`,`equip`,`intimate`,`hungry`,`rename_flag`,`incubate`,`autofeed`)

	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `pet_id`, `class`,`name`,`account_id`,`char_id`,`level`,`egg_id`,`equip`,`intimate`,`hungry`,`rename_flag`,`incubate`,`autofeed` FROM `%s` WHERE `pet_id`='%d'", schema_config.pet_db, pet_id) )
	{
		Sql_ShowDebug(sql_handle);
		return 0;
	}

	if( SQL_SUCCESS == Sql_NextRow(sql_handle) )
	{
		p->pet_id = pet_id;
		Sql_GetData(sql_handle,  1, &data, nullptr); p->class_ = a2i(data);
		Sql_GetData(sql_handle,  2, &data, &len); memcpy(p->name, data, zmin(len, NAME_LENGTH));
		Sql_GetData(sql_handle,  3, &data, nullptr); p->account_id = a2i(data);
		Sql_GetData(sql_handle,  4, &data, nullptr); p->char_id = a2i(data);
		Sql_GetData(sql_handle,  5, &data, nullptr); p->level = a2i(data);
		Sql_GetData(sql_handle,  6, &data, nullptr); p->egg_id = strtoul(data, nullptr, 10);
		Sql_GetData(sql_handle,  7, &data, nullptr); p->equip = strtoul(data, nullptr, 10);
		Sql_GetData(sql_handle,  8, &data, nullptr); p->intimate = a2i(data);
		Sql_GetData(sql_handle,  9, &data, nullptr); p->hungry = a2i(data);
		Sql_GetData(sql_handle, 10, &data, nullptr); p->rename_flag = a2i(data);
		Sql_GetData(sql_handle, 11, &data, nullptr); p->incubate = a2i(data);
		Sql_GetData(sql_handle, 12, &data, nullptr); p->autofeed = a2i(data) != 0;

		Sql_FreeResult(sql_handle);

		p->hungry = cap_value(p->hungry, 0, 100);
		p->intimate = cap_value(p->intimate, 0, 1000);

		if( charserv_config.save_log )
			ShowInfo("Pet loaded (%d - %s).\n", pet_id, p->name);
	}
	return 0;
}
//----------------------------------------------

int32 inter_pet_sql_init(void){
	//memory alloc
	pet_pt = (struct s_pet*)aCalloc(sizeof(struct s_pet), 1);
	return 0;
}
void inter_pet_sql_final(void){
	if (pet_pt) aFree(pet_pt);
	return;
}
//----------------------------------
int32 inter_pet_delete(int32 pet_id){
	ShowInfo("delete pet request: %d...\n",pet_id);

	if( SQL_ERROR == Sql_Query(sql_handle, "DELETE FROM `%s` WHERE `pet_id`='%d'", schema_config.pet_db, pet_id) )
		Sql_ShowDebug(sql_handle);
	return 0;
}
//------------------------------------------------------
int32 mapif_pet_created(int32 fd, uint32 account_id, struct s_pet *p)
{
	WFIFOHEAD(fd, 12);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16/uint32 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3880;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &aid, sizeof(aid));
	}
	if(p!=nullptr){
		int16 pet_class = p->class_;
		int32 pet_id = p->pet_id;
		memcpy(WFIFOP(fd,6), &pet_class, sizeof(pet_class));
		memcpy(WFIFOP(fd,8), &pet_id, sizeof(pet_id));
		ShowInfo("int_pet: created pet %d - %s\n", p->pet_id, p->name);
	}else{
		WFIFOB(fd, 6) = 0;
		uint32 pet_id_zero = 0;
		memcpy(WFIFOP(fd,8), &pet_id_zero, sizeof(pet_id_zero));
	}
	WFIFOSET(fd, 12);

	return 0;
}

int32 mapif_pet_info(int32 fd, uint32 account_id, struct s_pet *p){
	WFIFOHEAD(fd, sizeof(struct s_pet) + 9);
	// Same treatment for the reply builder (odd fifo positions possible).
	{
		uint16 op = 0x3881;
		uint16 packet_len = sizeof(struct s_pet) + 9;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
		memcpy(WFIFOP(fd,4), &aid, sizeof(aid));
	}
	WFIFOB(fd, 8)=0;
	memcpy(WFIFOP(fd, 9), p, sizeof(struct s_pet));
	WFIFOSET(fd, sizeof(struct s_pet) + 9);

	return 0;
}

int32 mapif_pet_noinfo(int32 fd, uint32 account_id){
	WFIFOHEAD(fd, sizeof(struct s_pet) + 9);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3881;
		uint16 packet_len = sizeof(struct s_pet) + 9;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
		memcpy(WFIFOP(fd,4), &aid, sizeof(aid));
	}
	WFIFOB(fd, 8)=1;
	memset(WFIFOP(fd, 9), 0, sizeof(struct s_pet));
	WFIFOSET(fd, sizeof(struct s_pet) + 9);

	return 0;
}

int32 mapif_save_pet_ack(int32 fd, uint32 account_id, int32 flag){
	WFIFOHEAD(fd, 7);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3882;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &aid, sizeof(aid));
	}
	WFIFOB(fd, 6) =flag;
	WFIFOSET(fd, 7);

	return 0;
}

int32 mapif_delete_pet_ack(int32 fd, int32 flag){
	WFIFOHEAD(fd, 3);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3883;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
	}
	WFIFOB(fd, 2) =flag;
	WFIFOSET(fd, 3);

	return 0;
}

int32 mapif_create_pet(int32 fd, uint32 account_id, uint32 char_id, int16 pet_class, int16 pet_lv, t_itemid pet_egg_id, t_itemid pet_equip, int16 intimate, int16 hungry, char rename_flag, char incubate, char *pet_name)
{
	memset(pet_pt, 0, sizeof(struct s_pet));
	safestrncpy(pet_pt->name, pet_name, NAME_LENGTH);
	if(incubate == 1)
		pet_pt->account_id = pet_pt->char_id = 0;
	else {
		pet_pt->account_id = account_id;
		pet_pt->char_id = char_id;
	}
	pet_pt->class_ = pet_class;
	pet_pt->level = pet_lv;
	pet_pt->egg_id = pet_egg_id;
	pet_pt->equip = pet_equip;
	pet_pt->intimate = intimate;
	pet_pt->hungry = hungry;
	pet_pt->rename_flag = rename_flag;
	pet_pt->incubate = incubate;
	pet_pt->autofeed = 0;

	if(pet_pt->hungry < 0)
		pet_pt->hungry = 0;
	else if(pet_pt->hungry > 100)
		pet_pt->hungry = 100;
	if(pet_pt->intimate < 0)
		pet_pt->intimate = 0;
	else if(pet_pt->intimate > 1000)
		pet_pt->intimate = 1000;

	pet_pt->pet_id = -1; //Signal NEW pet.
	if (inter_pet_tosql(pet_pt->pet_id,pet_pt)){
		if( pet_pt->incubate ){
			mapif_pet_created(fd, account_id, pet_pt);
		}else{
			mapif_load_pet(fd, account_id, char_id, pet_pt->pet_id);
		}
	}else	//Failed...
		mapif_pet_created(fd, account_id, nullptr);

	return 0;
}

int32 mapif_load_pet(int32 fd, uint32 account_id, uint32 char_id, int32 pet_id){
	memset(pet_pt, 0, sizeof(struct s_pet));

	inter_pet_fromsql(pet_id, pet_pt);

	if(pet_pt!=nullptr) {
		if(pet_pt->incubate == 1) {
			pet_pt->account_id = pet_pt->char_id = 0;
			mapif_pet_info(fd, account_id, pet_pt);
		}
		else if(account_id == pet_pt->account_id && char_id == pet_pt->char_id)
			mapif_pet_info(fd, account_id, pet_pt);
		else
			mapif_pet_noinfo(fd, account_id);
	}
	else
		mapif_pet_noinfo(fd, account_id);

	return 0;
}

int32 mapif_save_pet(int32 fd, uint32 account_id, struct s_pet *data) {
	//here process pet save request.
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint16 packet_len = 0;
	memcpy(&packet_len, RFIFOP(fd,2), sizeof(packet_len));
	int32 len = packet_len;
	if(sizeof(struct s_pet)!=len-8) {
		ShowError("inter pet: data size error %" PRIuPTR " %d\n", sizeof(struct s_pet), len-8);
	}

	else{
		if(data->hungry < 0)
			data->hungry = 0;
		else if(data->hungry > 100)
			data->hungry = 100;
		if(data->intimate < 0)
			data->intimate = 0;
		else if(data->intimate > 1000)
			data->intimate = 1000;
		inter_pet_tosql(data->pet_id,data);
		mapif_save_pet_ack(fd, account_id, 0);
	}

	return 0;
}

int32 mapif_delete_pet(int32 fd, int32 pet_id){
	mapif_delete_pet_ack(fd, inter_pet_delete(pet_id));

	return 0;
}

int32 mapif_parse_CreatePet(int32 fd){
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 account_id = 0, char_id = 0;
	uint16 pet_class_tmp = 0, pet_lv_tmp = 0;
	t_itemid pet_egg_id = 0, pet_equip = 0;
	uint16 intimate_tmp = 0, hungry_tmp = 0;
	memcpy(&account_id, RFIFOP(fd,2), sizeof(account_id));
	memcpy(&char_id, RFIFOP(fd,6), sizeof(char_id));
	memcpy(&pet_class_tmp, RFIFOP(fd,10), sizeof(pet_class_tmp));
	memcpy(&pet_lv_tmp, RFIFOP(fd,12), sizeof(pet_lv_tmp));
	memcpy(&pet_egg_id, RFIFOP(fd,14), sizeof(pet_egg_id));
	memcpy(&pet_equip, RFIFOP(fd,18), sizeof(pet_equip));
	memcpy(&intimate_tmp, RFIFOP(fd,22), sizeof(intimate_tmp));
	memcpy(&hungry_tmp, RFIFOP(fd,24), sizeof(hungry_tmp));
	mapif_create_pet(fd, account_id, char_id, (int16)pet_class_tmp, (int16)pet_lv_tmp, pet_egg_id, pet_equip, (int16)intimate_tmp,
		(int16)hungry_tmp, RFIFOB(fd, 26), RFIFOB(fd, 27), RFIFOCP(fd, 28));
	return 0;
}

int32 mapif_parse_LoadPet(int32 fd){
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 account_id = 0, char_id = 0, pet_id_tmp = 0;
	memcpy(&account_id, RFIFOP(fd,2), sizeof(account_id));
	memcpy(&char_id, RFIFOP(fd,6), sizeof(char_id));
	memcpy(&pet_id_tmp, RFIFOP(fd,10), sizeof(pet_id_tmp));
	mapif_load_pet(fd, account_id, char_id, (int32)pet_id_tmp);
	return 0;
}

int32 mapif_parse_SavePet(int32 fd){
	// Byte-buffer read at an odd read position: memcpy, same value.
	uint32 account_id = 0;
	memcpy(&account_id, RFIFOP(fd,4), sizeof(account_id));
	mapif_save_pet(fd, account_id, (struct s_pet *) RFIFOP(fd, 8));
	return 0;
}

int32 mapif_parse_DeletePet(int32 fd){
	// Byte-buffer read at an odd read position: memcpy, same value.
	uint32 pet_id_tmp = 0;
	memcpy(&pet_id_tmp, RFIFOP(fd,2), sizeof(pet_id_tmp));
	mapif_delete_pet(fd, (int32)pet_id_tmp);
	return 0;
}

int32 inter_pet_parse_frommap(int32 fd){
	uint16 pet_op = 0;
	memcpy(&pet_op, RFIFOP(fd,0), sizeof(pet_op));
	switch((int32)pet_op){
	case 0x3080: mapif_parse_CreatePet(fd); break;
	case 0x3081: mapif_parse_LoadPet(fd); break;
	case 0x3082: mapif_parse_SavePet(fd); break;
	case 0x3083: mapif_parse_DeletePet(fd); break;
	default:
		return 0;
	}
	return 1;
}
