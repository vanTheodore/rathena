// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "int_homun.hpp"

#include <cstdlib>
#include <cstring>

#include <common/mmo.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/sql.hpp>
#include <common/strlib.hpp>
#include <common/utils.hpp>

#include "char.hpp"
#include "inter.hpp"

int32 inter_homunculus_sql_init(void)
{
	return 0;
}
void inter_homunculus_sql_final(void)
{
	return;
}

void mapif_homunculus_created(int32 fd, uint32 account_id, struct s_homunculus *sh, unsigned char flag)
{
	WFIFOHEAD(fd, sizeof(struct s_homunculus)+9);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16/uint32 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3890;
		uint16 packet_len = sizeof(struct s_homunculus)+9;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
		memcpy(WFIFOP(fd,4), &aid, sizeof(aid));
	}
	WFIFOB(fd,8)= flag;
	memcpy(WFIFOP(fd,9),sh,sizeof(struct s_homunculus));
	WFIFOSET(fd, sizeof(struct s_homunculus)+9);
}

void mapif_homunculus_deleted(int32 fd, int32 flag)
{
	WFIFOHEAD(fd, 3);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3893;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
	}
	WFIFOB(fd,2) = flag; //Flag 1 = success
	WFIFOSET(fd, 3);
}

void mapif_homunculus_loaded(int32 fd, uint32 account_id, struct s_homunculus *hd)
{
	WFIFOHEAD(fd, sizeof(struct s_homunculus)+9);
	// Same treatment for the reply builder (odd fifo positions possible).
	{
		uint16 op = 0x3891;
		uint16 packet_len = sizeof(struct s_homunculus)+9;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
		memcpy(WFIFOP(fd,4), &aid, sizeof(aid));
	}
	if( hd != nullptr )
	{
		WFIFOB(fd,8) = 1; // success
		memcpy(WFIFOP(fd,9), hd, sizeof(struct s_homunculus));
	}
	else
	{
		WFIFOB(fd,8) = 0; // not found.
		memset(WFIFOP(fd,9), 0, sizeof(struct s_homunculus));
	}
	WFIFOSET(fd, sizeof(struct s_homunculus)+9);
}

void mapif_homunculus_saved(int32 fd, uint32 account_id, bool flag)
{
	WFIFOHEAD(fd, 7);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3892;
		uint32 aid = account_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &aid, sizeof(aid));
	}
	WFIFOB(fd,6) = flag; // 1:success, 0:failure
	WFIFOSET(fd, 7);
}

void mapif_homunculus_renamed(int32 fd, uint32 account_id, uint32 char_id, unsigned char flag, char* name)
{
	WFIFOHEAD(fd, NAME_LENGTH+12);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3894;
		uint32 aid = account_id;
		uint32 cid = char_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &aid, sizeof(aid));
		memcpy(WFIFOP(fd,6), &cid, sizeof(cid));
	}
	WFIFOB(fd,10) = flag;
	safestrncpy(WFIFOCP(fd,11), name, NAME_LENGTH);
	WFIFOSET(fd, NAME_LENGTH+12);
}

bool mapif_homunculus_save(struct s_homunculus* hd)
{
	char esc_name[NAME_LENGTH*2+1];

	Sql_EscapeStringLen(sql_handle, esc_name, hd->name, strnlen(hd->name, NAME_LENGTH));

	if( hd->hom_id == 0 )
	{// new homunculus
		if( SQL_ERROR == Sql_Query(sql_handle, "INSERT INTO `%s` "
			"(`char_id`, `class`,`prev_class`,`name`,`level`,`exp`,`intimacy`,`hunger`, `str`, `agi`, `vit`, `int`, `dex`, `luk`, `hp`,`max_hp`,`sp`,`max_sp`,`skill_point`, `rename_flag`, `vaporize`, `autofeed`) "
			"VALUES ('%d', '%d', '%d', '%s', '%d', '%" PRIu64 "', '%u', '%d', '%d', %d, '%d', '%d', '%d', '%d', '%u', '%u', '%u', '%u', '%d', '%d', '%d', '%d')",
			schema_config.homunculus_db, hd->char_id, hd->class_, hd->prev_class, esc_name, hd->level, hd->exp, hd->intimacy, hd->hunger, hd->str, hd->agi, hd->vit, hd->int_, hd->dex, hd->luk,
			hd->hp, hd->max_hp, hd->sp, hd->max_sp, hd->skillpts, hd->rename_flag, hd->vaporize, hd->autofeed) )
		{
			Sql_ShowDebug(sql_handle);
			return false;
		}
		else
		{
			hd->hom_id = (int32)Sql_LastInsertId(sql_handle);
		}
	}
	else
	{
		if( SQL_ERROR == Sql_Query(sql_handle, "UPDATE `%s` SET `char_id`='%d', `class`='%d',`prev_class`='%d',`name`='%s',`level`='%d',`exp`='%" PRIu64 "',`intimacy`='%u',`hunger`='%d', `str`='%d', `agi`='%d', `vit`='%d', `int`='%d', `dex`='%d', `luk`='%d', `hp`='%u',`max_hp`='%u',`sp`='%u',`max_sp`='%u',`skill_point`='%d', `rename_flag`='%d', `vaporize`='%d', `autofeed`='%d' WHERE `homun_id`='%d'",
			schema_config.homunculus_db, hd->char_id, hd->class_, hd->prev_class, esc_name, hd->level, hd->exp, hd->intimacy, hd->hunger, hd->str, hd->agi, hd->vit, hd->int_, hd->dex, hd->luk,
			hd->hp, hd->max_hp, hd->sp, hd->max_sp, hd->skillpts, hd->rename_flag, hd->vaporize, hd->autofeed, hd->hom_id) )
		{
			Sql_ShowDebug(sql_handle);
			return false;
		}
	}

	SqlStmt stmt{ *sql_handle };

	// Save skills
	if( SQL_ERROR == Sql_Query( sql_handle, "DELETE FROM `%s` WHERE `homun_id` = '%d'", schema_config.skill_homunculus_db, hd->hom_id )
	 || SQL_ERROR == stmt.Prepare( "INSERT INTO `%s` (`homun_id`, `id`, `lv`) VALUES (%d, ?, ?)", schema_config.skill_homunculus_db, hd->hom_id ) ){
		SqlStmt_ShowDebug(stmt);
		return false;
	}

	for (uint16 i = 0; i < MAX_HOMUNSKILL; ++i) {
		if (hd->hskill[i].id > 0 && hd->hskill[i].lv != 0) {
			if (SQL_ERROR == stmt.BindParam(0, SQLDT_UINT16, &hd->hskill[i].id, 0)
				|| SQL_ERROR == stmt.BindParam(1, SQLDT_UINT16, &hd->hskill[i].lv, 0)
				|| SQL_ERROR == stmt.Execute()) {
				SqlStmt_ShowDebug(stmt);
				return false;
			}
		}
	}

	// Save skill cooldowns
	if( SQL_ERROR == Sql_Query( sql_handle, "DELETE FROM `%s` WHERE `homun_id` = '%d'", schema_config.skillcooldown_homunculus_db, hd->hom_id )
	 || SQL_ERROR == stmt.Prepare( "INSERT INTO `%s` (`homun_id`, `skill`, `tick`) VALUES (%d, ?, ?)", schema_config.skillcooldown_homunculus_db, hd->hom_id ) ){
		SqlStmt_ShowDebug(stmt);
		return false;
	}

	for (uint16 i = 0; i < MAX_SKILLCOOLDOWN; ++i) {
		if (hd->scd[i].skill_id == 0) {
			continue;
		}

		if (hd->scd[i].tick == 0) {
			continue;
		}

		if (SQL_ERROR == stmt.BindParam(0, SQLDT_UINT16, &hd->scd[i].skill_id, 0)
			|| SQL_ERROR == stmt.BindParam(1, SQLDT_LONGLONG, &hd->scd[i].tick, 0)
			|| SQL_ERROR == stmt.Execute()) {
			SqlStmt_ShowDebug(stmt);
			return false;
		}
	}

	return true;
}



// Load an homunculus
bool mapif_homunculus_load(int32 homun_id, struct s_homunculus* hd)
{
	char* data;
	size_t len;

	memset(hd, 0, sizeof(*hd));

	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `homun_id`,`char_id`,`class`,`prev_class`,`name`,`level`,`exp`,`intimacy`,`hunger`, `str`, `agi`, `vit`, `int`, `dex`, `luk`, `hp`,`max_hp`,`sp`,`max_sp`,`skill_point`,`rename_flag`, `vaporize`, `autofeed` FROM `%s` WHERE `homun_id`='%u'",
		schema_config.homunculus_db, homun_id) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}

	if( !Sql_NumRows(sql_handle) )
	{	//No homunculus found.
		Sql_FreeResult(sql_handle);
		return false;
	}
	if( SQL_SUCCESS != Sql_NextRow(sql_handle) )
	{
		Sql_ShowDebug(sql_handle);
		Sql_FreeResult(sql_handle);
		return false;
	}

	hd->hom_id = homun_id;
	Sql_GetData(sql_handle,  1, &data, nullptr); hd->char_id = a2i(data);
	Sql_GetData(sql_handle,  2, &data, nullptr); hd->class_ = a2i(data);
	Sql_GetData(sql_handle,  3, &data, nullptr); hd->prev_class = a2i(data);
	Sql_GetData(sql_handle,  4, &data, &len); safestrncpy(hd->name, data, sizeof(hd->name));
	Sql_GetData(sql_handle,  5, &data, nullptr); hd->level = a2i(data);
	Sql_GetData(sql_handle,  6, &data, nullptr); hd->exp = strtoull( data, nullptr, 10 );
	Sql_GetData(sql_handle,  7, &data, nullptr); hd->intimacy = (uint32)strtoul(data, nullptr, 10);
	Sql_GetData(sql_handle,  8, &data, nullptr); hd->hunger = a2i(data);
	Sql_GetData(sql_handle,  9, &data, nullptr); hd->str = a2i(data);
	Sql_GetData(sql_handle, 10, &data, nullptr); hd->agi = a2i(data);
	Sql_GetData(sql_handle, 11, &data, nullptr); hd->vit = a2i(data);
	Sql_GetData(sql_handle, 12, &data, nullptr); hd->int_ = a2i(data);
	Sql_GetData(sql_handle, 13, &data, nullptr); hd->dex = a2i(data);
	Sql_GetData(sql_handle, 14, &data, nullptr); hd->luk = a2i(data);
	Sql_GetData(sql_handle, 15, &data, nullptr); hd->hp = strtoul(data, nullptr, 10);
	Sql_GetData(sql_handle, 16, &data, nullptr); hd->max_hp = strtoul(data, nullptr, 10);
	Sql_GetData(sql_handle, 17, &data, nullptr); hd->sp = strtoul(data, nullptr, 10);
	Sql_GetData(sql_handle, 18, &data, nullptr); hd->max_sp = strtoul(data, nullptr, 10);
	Sql_GetData(sql_handle, 19, &data, nullptr); hd->skillpts = a2i(data);
	Sql_GetData(sql_handle, 20, &data, nullptr); hd->rename_flag = a2i(data);
	Sql_GetData(sql_handle, 21, &data, nullptr); hd->vaporize = a2i(data);
	Sql_GetData(sql_handle, 22, &data, nullptr); hd->autofeed = a2i(data) != 0;
	Sql_FreeResult(sql_handle);

	hd->intimacy = umin(hd->intimacy,100000);
	hd->hunger = cap_value(hd->hunger, 0, 100);

	// Load Homunculus Skill
	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `id`,`lv` FROM `%s` WHERE `homun_id`=%d", schema_config.skill_homunculus_db, homun_id) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}
	while( SQL_SUCCESS == Sql_NextRow(sql_handle) )
	{
		int32 i;
		// id
		Sql_GetData(sql_handle, 0, &data, nullptr);
		i = a2i(data);
		if( i < HM_SKILLBASE || i >= HM_SKILLBASE + MAX_HOMUNSKILL )
			continue;// invalid skill id
		i = i - HM_SKILLBASE;
		hd->hskill[i].id = (uint16)a2i(data);

		// lv
		Sql_GetData(sql_handle, 1, &data, nullptr);
		hd->hskill[i].lv = (unsigned char)a2i(data);
	}
	Sql_FreeResult(sql_handle);

	// Load Homunuclus Skill Cooldown
	if (SQL_ERROR == Sql_Query(sql_handle, "SELECT `skill`,`tick` FROM `%s` WHERE `homun_id`=%d", schema_config.skillcooldown_homunculus_db, homun_id)) {
		Sql_ShowDebug(sql_handle);
		return false;
	}

	uint16 count = 0;

	while (SQL_SUCCESS == Sql_NextRow(sql_handle)) {
		if (count == MAX_SKILLCOOLDOWN) {
			ShowWarning("Too many skillcooldowns for homunculus %d, skipping.\n", homun_id);
			break;
		}

		// Skill
		Sql_GetData(sql_handle, 0, &data, nullptr);
		uint16 skill_id = static_cast<uint16>(strtoul(data, nullptr, 10));

		if (skill_id < HM_SKILLBASE || skill_id >= HM_SKILLBASE + MAX_HOMUNSKILL)
			continue; // invalid skill ID
		hd->scd[count].skill_id = skill_id;

		// Tick
		Sql_GetData(sql_handle, 1, &data, nullptr);
		hd->scd[count].tick = strtoll(data, nullptr, 10);

		count++;
	}
	Sql_FreeResult(sql_handle);

	if( charserv_config.save_log )
		ShowInfo("Homunculus loaded (ID: %d - %s / Class: %d / CID: %d).\n", hd->hom_id, hd->name, hd->class_, hd->char_id);

	return true;
}

bool mapif_homunculus_delete(int32 homun_id)
{
	if( SQL_ERROR == Sql_Query( sql_handle, "DELETE FROM `%s` WHERE `homun_id` = '%u'", schema_config.skillcooldown_homunculus_db, homun_id )
	 || SQL_ERROR == Sql_Query( sql_handle, "DELETE FROM `%s` WHERE `homun_id` = '%u'", schema_config.skill_homunculus_db, homun_id )
	 || SQL_ERROR == Sql_Query( sql_handle, "DELETE FROM `%s` WHERE `homun_id` = '%u'", schema_config.homunculus_db, homun_id ) ){
		Sql_ShowDebug(sql_handle);
		return false;
	}
	ShowInfo("Homunculus '%d' has been deleted.\n", homun_id);
	return true;
}

bool mapif_homunculus_rename(char *name)
{
	int32 i;

	// Check Authorised letters/symbols in the name of the homun
	if( charserv_config.char_config.char_name_option == 1 )
	{// only letters/symbols in char_name_letters are authorised
		for( i = 0; i < NAME_LENGTH && name[i]; i++ )
			if( strchr(charserv_config.char_config.char_name_letters, name[i]) == nullptr )
				return false;
	} else
	if( charserv_config.char_config.char_name_option == 2 )
	{// letters/symbols in char_name_letters are forbidden
		for( i = 0; i < NAME_LENGTH && name[i]; i++ )
			if( strchr(charserv_config.char_config.char_name_letters, name[i]) != nullptr )
				return false;
	}

	return true;
}


void mapif_parse_homunculus_create(int32 fd, int32 len, uint32 account_id, struct s_homunculus* phd)
{
	bool result = mapif_homunculus_save(phd);
	mapif_homunculus_created(fd, account_id, phd, result);
}

void mapif_parse_homunculus_delete(int32 fd, int32 homun_id)
{
	bool result = mapif_homunculus_delete(homun_id);
	mapif_homunculus_deleted(fd, result);
}

void mapif_parse_homunculus_load(int32 fd, uint32 account_id, int32 homun_id)
{
	struct s_homunculus hd;
	bool result = mapif_homunculus_load(homun_id, &hd);
	mapif_homunculus_loaded(fd, account_id, ( result ? &hd : nullptr ));
}

void mapif_parse_homunculus_save(int32 fd, int32 len, uint32 account_id, struct s_homunculus* phd)
{
	bool result = mapif_homunculus_save(phd);
	mapif_homunculus_saved(fd, account_id, result);
}

void mapif_parse_homunculus_rename(int32 fd, uint32 account_id, uint32 char_id, char* name)
{
	bool result = mapif_homunculus_rename(name);
	mapif_homunculus_renamed(fd, account_id, char_id, result, name);
}

/*==========================================
 * Inter Packets
 *------------------------------------------*/
int32 inter_homunculus_parse_frommap(int32 fd)
{
	// Byte-buffer read: the read position advances by consumed packet
	// lengths (often odd), so even the plain uint16 opcode load uses memcpy.
	uint16 cmd = 0;
	memcpy(&cmd, RFIFOP(fd,0), sizeof(cmd));

	switch( cmd )
	{
		case 0x3090: {
			// Byte-buffer reads at odd read positions: memcpy into aligned locals.
			uint16 len_tmp = 0;
			uint32 account_id_tmp = 0;
			memcpy(&len_tmp, RFIFOP(fd,2), sizeof(len_tmp));
			memcpy(&account_id_tmp, RFIFOP(fd,4), sizeof(account_id_tmp));
			mapif_parse_homunculus_create(fd, (int32)len_tmp, account_id_tmp, (struct s_homunculus*)RFIFOP(fd,8));
			break;
		}
		case 0x3091: {
			uint32 account_id_tmp = 0, homun_id_tmp = 0;
			memcpy(&account_id_tmp, RFIFOP(fd,2), sizeof(account_id_tmp));
			memcpy(&homun_id_tmp, RFIFOP(fd,6), sizeof(homun_id_tmp));
			mapif_parse_homunculus_load(fd, account_id_tmp, (int32)homun_id_tmp);
			break;
		}
		case 0x3092: {
			uint16 len_tmp = 0;
			uint32 account_id_tmp = 0;
			memcpy(&len_tmp, RFIFOP(fd,2), sizeof(len_tmp));
			memcpy(&account_id_tmp, RFIFOP(fd,4), sizeof(account_id_tmp));
			mapif_parse_homunculus_save(fd, (int32)len_tmp, account_id_tmp, (struct s_homunculus*)RFIFOP(fd,8));
			break;
		}
		case 0x3093: {
			uint32 homun_id_tmp = 0;
			memcpy(&homun_id_tmp, RFIFOP(fd,2), sizeof(homun_id_tmp));
			mapif_parse_homunculus_delete(fd, (int32)homun_id_tmp);
			break;
		}
		case 0x3094: {
			uint32 account_id_tmp = 0, char_id_tmp = 0;
			memcpy(&account_id_tmp, RFIFOP(fd,2), sizeof(account_id_tmp));
			memcpy(&char_id_tmp, RFIFOP(fd,6), sizeof(char_id_tmp));
			mapif_parse_homunculus_rename(fd, account_id_tmp, char_id_tmp, RFIFOCP(fd,10));
			break;
		}
		default:
			return 0;
	}
	return 1;
}
