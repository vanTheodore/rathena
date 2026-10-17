// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "int_auction.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>

#include <common/malloc.hpp>
#include <common/mmo.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/sql.hpp>
#include <common/strlib.hpp>
#include <common/timer.hpp>
#include <common/utilities.hpp>

#include "char.hpp"
#include "char_mapif.hpp"
#include "inter.hpp"
#include "int_mail.hpp"

using namespace rathena;

// int32 auction_id -> struct auction_data*
static std::unordered_map<uint32, std::shared_ptr<struct auction_data>> auction_db;

void auction_delete( std::shared_ptr<struct auction_data> auction );
TIMER_FUNC(auction_end_timer);

int32 auction_count(uint32 char_id, bool buy)
{
	int32 i = 0;

	for( const auto& pair : auction_db ){
		std::shared_ptr<struct auction_data> auction = pair.second;

		if( ( buy && auction->buyer_id == char_id ) || ( !buy && auction->seller_id == char_id ) ){
			i++;
		}
	}

	return i;
}

void auction_save( std::shared_ptr<struct auction_data> auction ){
	int32 j;
	StringBuf buf;
	SqlStmt stmt{ *sql_handle };

	if( !auction )
		return;

	StringBuf_Init(&buf);
	StringBuf_Printf(&buf, "UPDATE `%s` SET `seller_id` = '%d', `seller_name` = ?, `buyer_id` = '%d', `buyer_name` = ?, `price` = '%d', `buynow` = '%d', `hours` = '%d', `timestamp` = '%lu', `nameid` = '%u', `item_name` = ?, `type` = '%d', `refine` = '%d', `attribute` = '%d', `enchantgrade` ='%d'",
		schema_config.auction_db, auction->seller_id, auction->buyer_id, auction->price, auction->buynow, auction->hours, (unsigned long)auction->timestamp, auction->item.nameid, auction->type, auction->item.refine, auction->item.attribute, auction->item.enchantgrade);
	for( j = 0; j < MAX_SLOTS; j++ )
		StringBuf_Printf(&buf, ", `card%d` = '%u'", j, auction->item.card[j]);
	for (j = 0; j < MAX_ITEM_RDM_OPT; j++) {
		StringBuf_Printf(&buf, ", `option_id%d` = '%d'", j, auction->item.option[j].id);
		StringBuf_Printf(&buf, ", `option_val%d` = '%d'", j, auction->item.option[j].value);
		StringBuf_Printf(&buf, ", `option_parm%d` = '%d'", j, auction->item.option[j].param);
	}
	StringBuf_Printf(&buf, " WHERE `auction_id` = '%d'", auction->auction_id);

	if( SQL_SUCCESS != stmt.PrepareStr(StringBuf_Value(&buf))
	||  SQL_SUCCESS != stmt.BindParam(0, SQLDT_STRING, auction->seller_name, strnlen(auction->seller_name, NAME_LENGTH))
	||  SQL_SUCCESS != stmt.BindParam(1, SQLDT_STRING, auction->buyer_name, strnlen(auction->buyer_name, NAME_LENGTH))
	||  SQL_SUCCESS != stmt.BindParam(2, SQLDT_STRING, auction->item_name, strnlen(auction->item_name, ITEM_NAME_LENGTH))
	||  SQL_SUCCESS != stmt.Execute() )
	{
		SqlStmt_ShowDebug(stmt);
	}
}

uint32 auction_create( std::shared_ptr<struct auction_data> auction ){
	int32 j;
	StringBuf buf;
	SqlStmt stmt{ *sql_handle };

	if( !auction )
		return false;

	auction->timestamp = time(nullptr) + (auction->hours * 3600);

	StringBuf_Init(&buf);
	StringBuf_Printf(&buf, "INSERT INTO `%s` (`seller_id`,`seller_name`,`buyer_id`,`buyer_name`,`price`,`buynow`,`hours`,`timestamp`,`nameid`,`item_name`,`type`,`refine`,`attribute`,`unique_id`,`enchantgrade`", schema_config.auction_db);
	for( j = 0; j < MAX_SLOTS; j++ )
		StringBuf_Printf(&buf, ",`card%d`", j);
	for (j = 0; j < MAX_ITEM_RDM_OPT; ++j) {
		StringBuf_Printf(&buf, ", `option_id%d`", j);
		StringBuf_Printf(&buf, ", `option_val%d`", j);
		StringBuf_Printf(&buf, ", `option_parm%d`", j);
	}
	StringBuf_Printf(&buf, ") VALUES ('%d',?,'%d',?,'%d','%d','%d','%lu','%u',?,'%d','%d','%d','%" PRIu64 "','%d'",
		auction->seller_id, auction->buyer_id, auction->price, auction->buynow, auction->hours, (unsigned long)auction->timestamp, auction->item.nameid, auction->type, auction->item.refine, auction->item.attribute, auction->item.unique_id, auction->item.enchantgrade);
	for( j = 0; j < MAX_SLOTS; j++ )	
		StringBuf_Printf(&buf, ",'%u'", auction->item.card[j]);
	for (j = 0; j < MAX_ITEM_RDM_OPT; ++j) {
		StringBuf_Printf(&buf, ", '%d'", auction->item.option[j].id);
		StringBuf_Printf(&buf, ", '%d'", auction->item.option[j].value);
		StringBuf_Printf(&buf, ", '%d'", auction->item.option[j].param);
	}
	StringBuf_AppendStr(&buf, ")");

	if( SQL_SUCCESS != stmt.PrepareStr(StringBuf_Value(&buf))
	||  SQL_SUCCESS != stmt.BindParam(0, SQLDT_STRING, auction->seller_name, strnlen(auction->seller_name, NAME_LENGTH))
	||  SQL_SUCCESS != stmt.BindParam(1, SQLDT_STRING, auction->buyer_name, strnlen(auction->buyer_name, NAME_LENGTH))
	||  SQL_SUCCESS != stmt.BindParam(2, SQLDT_STRING, auction->item_name, strnlen(auction->item_name, ITEM_NAME_LENGTH))
	||  SQL_SUCCESS != stmt.Execute() )
	{
		SqlStmt_ShowDebug(stmt);
		auction->auction_id = 0;
	}
	else
	{
		t_tick tick = auction->hours * 3600000;

		auction->item.amount = 1;
		auction->item.identify = 1;
		auction->item.expire_time = 0;

		auction->auction_id = (uint32)stmt.LastInsertId();
		auction->auction_end_timer = add_timer( gettick() + tick , auction_end_timer, auction->auction_id, 0);
		ShowInfo("New Auction %u | time left %" PRtf " ms | By %s.\n", auction->auction_id, tick, auction->seller_name);

		auction_db[auction->auction_id] = auction;
	}

	return auction->auction_id;
}

void mapif_Auction_message(uint32 char_id, unsigned char result)
{
	unsigned char buf[74];

	WBUFW(buf,0) = 0x3854;
	WBUFL(buf,2) = char_id;
	WBUFL(buf,6) = result;
	chmapif_sendall(buf,7);
}

TIMER_FUNC(auction_end_timer){
	std::shared_ptr<struct auction_data> auction = util::umap_find( auction_db, static_cast<uint32>( id ) );

	if( auction != nullptr ){
		if( auction->buyer_id )
		{
			mail_sendmail(0, msg_txt(200), auction->buyer_id, auction->buyer_name, msg_txt(201), msg_txt(202), 0, &auction->item, 1);
			mapif_Auction_message(auction->buyer_id, 6); // You have won the auction
			mail_sendmail(0, msg_txt(200), auction->seller_id, auction->seller_name, msg_txt(201), msg_txt(203), auction->price, nullptr, 0);
		}
		else
			mail_sendmail(0, msg_txt(200), auction->seller_id, auction->seller_name, msg_txt(201), msg_txt(204), 0, &auction->item, 1);

		ShowInfo("Auction End: id %u.\n", auction->auction_id);

		auction->auction_end_timer = INVALID_TIMER;
		auction_delete(auction);
	}

	return 0;
}

void auction_delete( std::shared_ptr<struct auction_data> auction ){
	uint32 auction_id = auction->auction_id;

	if( SQL_ERROR == Sql_Query(sql_handle, "DELETE FROM `%s` WHERE `auction_id` = '%d'", schema_config.auction_db, auction_id) )
		Sql_ShowDebug(sql_handle);

	if( auction->auction_end_timer != INVALID_TIMER )
		delete_timer(auction->auction_end_timer, auction_end_timer);

	auction_db.erase( auction_id );
}

void inter_auctions_fromsql(void)
{
	int32 i;
	char *data;
	StringBuf buf;
	t_tick tick = gettick(), endtick;
	time_t now = time(nullptr);

	StringBuf_Init(&buf);
	StringBuf_AppendStr(&buf, "SELECT `auction_id`,`seller_id`,`seller_name`,`buyer_id`,`buyer_name`,"
		"`price`,`buynow`,`hours`,`timestamp`,`nameid`,`item_name`,`type`,`refine`,`attribute`,`unique_id`,`enchantgrade`");
	for( i = 0; i < MAX_SLOTS; i++ )
		StringBuf_Printf(&buf, ",`card%d`", i);
	for (i = 0; i < MAX_ITEM_RDM_OPT; ++i) {
		StringBuf_Printf(&buf, ", `option_id%d`", i);
		StringBuf_Printf(&buf, ", `option_val%d`", i);
		StringBuf_Printf(&buf, ", `option_parm%d`", i);
	}
	StringBuf_Printf(&buf, " FROM `%s` ORDER BY `auction_id` DESC", schema_config.auction_db);

	if( SQL_ERROR == Sql_Query(sql_handle, StringBuf_Value(&buf)) )
		Sql_ShowDebug(sql_handle);

	while( SQL_SUCCESS == Sql_NextRow(sql_handle) )
	{
		struct item *item;
		std::shared_ptr<struct auction_data> auction = std::make_shared<struct auction_data>();

		Sql_GetData(sql_handle, 0, &data, nullptr); auction->auction_id = a2i(data);
		Sql_GetData(sql_handle, 1, &data, nullptr); auction->seller_id = a2i(data);
		Sql_GetData(sql_handle, 2, &data, nullptr); safestrncpy(auction->seller_name, data, NAME_LENGTH);
		Sql_GetData(sql_handle, 3, &data, nullptr); auction->buyer_id = a2i(data);
		Sql_GetData(sql_handle, 4, &data, nullptr); safestrncpy(auction->buyer_name, data, NAME_LENGTH);
		Sql_GetData(sql_handle, 5, &data, nullptr); auction->price	= a2i(data);
		Sql_GetData(sql_handle, 6, &data, nullptr); auction->buynow = a2i(data);
		Sql_GetData(sql_handle, 7, &data, nullptr); auction->hours = a2i(data);
		Sql_GetData(sql_handle, 8, &data, nullptr); auction->timestamp = a2i(data);

		item = &auction->item;
		Sql_GetData(sql_handle, 9, &data, nullptr); item->nameid = strtoul(data, nullptr, 10);
		Sql_GetData(sql_handle,10, &data, nullptr); safestrncpy(auction->item_name, data, ITEM_NAME_LENGTH);
		Sql_GetData(sql_handle,11, &data, nullptr); auction->type = a2i(data);

		Sql_GetData(sql_handle,12, &data, nullptr); item->refine = a2i(data);
		Sql_GetData(sql_handle,13, &data, nullptr); item->attribute = a2i(data);
		Sql_GetData(sql_handle,14, &data, nullptr); item->unique_id = strtoull(data, nullptr, 10);
		Sql_GetData(sql_handle,15, &data, nullptr); item->enchantgrade = a2i(data);

		item->identify = 1;
		item->amount = 1;
		item->expire_time = 0;

		for( i = 0; i < MAX_SLOTS; i++ )
		{
			Sql_GetData(sql_handle, 16 + i, &data, nullptr);
			item->card[i] = strtoul(data, nullptr, 10);
		}

		for (i = 0; i < MAX_ITEM_RDM_OPT; i++) {
			Sql_GetData(sql_handle, 16 + MAX_SLOTS + i*3, &data, nullptr);
			item->option[i].id = a2i(data);
			Sql_GetData(sql_handle, 17 + MAX_SLOTS + i*3, &data, nullptr);
			item->option[i].value = a2i(data);
			Sql_GetData(sql_handle, 18 + MAX_SLOTS + i*3, &data, nullptr);
			item->option[i].param = a2i(data);
		}

		if( auction->timestamp > now )
			endtick = ((uint32)(auction->timestamp - now) * 1000) + tick;
		else
			endtick = tick + 10000; // 10 Second's to process ended auctions

		auction->auction_end_timer = add_timer(endtick, auction_end_timer, auction->auction_id, 0);

		auction_db[auction->auction_id] = auction;
	}

	Sql_FreeResult(sql_handle);
}

void mapif_Auction_sendlist(int32 fd, uint32 char_id, int16 count, int16 pages, unsigned char *buf)
{
	int32 len = (sizeof(struct auction_data) * count) + 12;

	WFIFOHEAD(fd, len);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16/uint32 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3850;
		uint16 packet_len = (uint16)len;
		uint32 cid = char_id;
		int16 cnt = count;
		int16 pgs = pages;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
		memcpy(WFIFOP(fd,4), &cid, sizeof(cid));
		memcpy(WFIFOP(fd,8), &cnt, sizeof(cnt));
		memcpy(WFIFOP(fd,10), &pgs, sizeof(pgs));
	}
	memcpy(WFIFOP(fd,12), buf, len - 12);
	WFIFOSET(fd,len);
}

void mapif_parse_Auction_requestlist(int32 fd)
{
	char searchtext[NAME_LENGTH];
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 char_id = 0;
	uint32 price_tmp = 0;
	uint16 type_tmp = 0, page_tmp = 0;
	memcpy(&char_id, RFIFOP(fd,4), sizeof(char_id));
	memcpy(&price_tmp, RFIFOP(fd,10), sizeof(price_tmp));
	memcpy(&type_tmp, RFIFOP(fd,8), sizeof(type_tmp));
	memcpy(&page_tmp, RFIFOP(fd,14), sizeof(page_tmp));
	uint32 len = sizeof(struct auction_data);
	int32 price = (int32)price_tmp;
	int16 type = (int16)type_tmp, page = max(1,(int16)page_tmp);
	unsigned char buf[5 * sizeof(struct auction_data)];
	int16 i = 0, j = 0, pages = 1;

	memcpy(searchtext, RFIFOP(fd,16), NAME_LENGTH);

	for( const auto& pair : auction_db ){
		std::shared_ptr<struct auction_data> auction = pair.second;

		if( (type == 0 && auction->type != IT_ARMOR && auction->type != IT_PETARMOR) ||
			(type == 1 && auction->type != IT_WEAPON) ||
			(type == 2 && auction->type != IT_CARD) ||
			(type == 3 && auction->type != IT_ETC) ||
			(type == 4 && !strstr(auction->item_name, searchtext)) ||
			(type == 5 && auction->price > price) ||
			(type == 6 && auction->seller_id != char_id) ||
			(type == 7 && auction->buyer_id != char_id) )
			continue;

		i++;
		if( i > 5 )
		{ // Counting Pages of Total Results (5 Results per Page)
			pages++;
			i = 1; // First Result of This Page
		}

		if( page != pages )
			continue; // This is not the requested Page

		memcpy( WBUFP( buf, j * len ), auction.get(), len );
		j++; // Found Results
	}

	mapif_Auction_sendlist(fd, char_id, j, pages, buf);
}

void mapif_Auction_register(int32 fd, struct auction_data *auction)
{
	int32 len = sizeof(struct auction_data) + 4;

	WFIFOHEAD(fd,len);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3851;
		uint16 packet_len = (uint16)len;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &packet_len, sizeof(packet_len));
	}
	memcpy(WFIFOP(fd,4), auction, sizeof(struct auction_data));
	WFIFOSET(fd,len);
}

void mapif_parse_Auction_register(int32 fd)
{
	// Byte-buffer read at an odd read position: memcpy, same value.
	uint16 packet_len = 0;
	memcpy(&packet_len, RFIFOP(fd,2), sizeof(packet_len));
	if( packet_len != sizeof(struct auction_data) + 4 )
		return;

	struct auction_data* auction = reinterpret_cast<struct auction_data*>( RFIFOP( fd, 4 ) );

	if( auction_count( auction->seller_id, false ) < 5 ){
		std::shared_ptr<struct auction_data> auction2 = std::make_shared<struct auction_data>();

		memcpy( auction2.get(), auction, sizeof( struct auction_data ) );

		auction2->auction_id = auction_create( auction2 );

		auction = auction2.get();
	}

	mapif_Auction_register( fd, auction );
}

void mapif_Auction_cancel(int32 fd, uint32 char_id, unsigned char result)
{
	WFIFOHEAD(fd,7);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16/uint32 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3852;
		uint32 cid = char_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &cid, sizeof(cid));
	}
	WFIFOB(fd,6) = result;
	WFIFOSET(fd,7);
}

void mapif_parse_Auction_cancel(int32 fd)
{
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 char_id = 0, auction_id = 0;
	memcpy(&char_id, RFIFOP(fd,2), sizeof(char_id));
	memcpy(&auction_id, RFIFOP(fd,6), sizeof(auction_id));

	std::shared_ptr<struct auction_data> auction = util::umap_find( auction_db, auction_id );

	if( auction == nullptr ){
		mapif_Auction_cancel(fd, char_id, 1); // Bid Number is Incorrect
		return;
	}

	if( auction->seller_id != char_id )
	{
		mapif_Auction_cancel(fd, char_id, 2); // You cannot end the auction
		return;
	}

	if( auction->buyer_id > 0 )
	{
		mapif_Auction_cancel(fd, char_id, 3); // An auction with at least one bidder cannot be canceled
		return;
	}

	mail_sendmail(0, msg_txt(200), auction->seller_id, auction->seller_name, msg_txt(201), msg_txt(205), 0, &auction->item, 1);
	auction_delete(auction);

	mapif_Auction_cancel(fd, char_id, 0); // The auction has been canceled
}

void mapif_Auction_close(int32 fd, uint32 char_id, unsigned char result)
{
	WFIFOHEAD(fd,7);
	// Same treatment: the fifo write position can be odd-sized.
	{
		uint16 op = 0x3853;
		uint32 cid = char_id;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &cid, sizeof(cid));
	}
	WFIFOB(fd,6) = result;
	WFIFOSET(fd,7);
}

void mapif_parse_Auction_close(int32 fd)
{
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 char_id = 0, auction_id = 0;
	memcpy(&char_id, RFIFOP(fd,2), sizeof(char_id));
	memcpy(&auction_id, RFIFOP(fd,6), sizeof(auction_id));
	std::shared_ptr<struct auction_data> auction = util::umap_find( auction_db, auction_id );

	if( auction == nullptr ){
		mapif_Auction_close(fd, char_id, 2); // Bid Number is Incorrect
		return;
	}

	if( auction->seller_id != char_id )
	{
		mapif_Auction_close(fd, char_id, 1); // You cannot end the auction
		return;
	}

	if( auction->buyer_id == 0 )
	{
		mapif_Auction_close(fd, char_id, 1); // You cannot end the auction
		return;
	}

	// Send Money to Seller
	mail_sendmail(0, msg_txt(200), auction->seller_id, auction->seller_name, msg_txt(201), msg_txt(206), auction->price, nullptr, 0);
	// Send Item to Buyer
	mail_sendmail(0, msg_txt(200), auction->buyer_id, auction->buyer_name, msg_txt(201), msg_txt(207), 0, &auction->item, 1);
	mapif_Auction_message(auction->buyer_id, 6); // You have won the auction
	auction_delete(auction);

	mapif_Auction_close(fd, char_id, 0); // You have ended the auction
}

void mapif_Auction_bid(int32 fd, uint32 char_id, int32 bid, unsigned char result)
{
	WFIFOHEAD(fd,11);
	// Byte-buffer slots: the fifo write position can be odd-sized, so even
	// plain uint16/uint32 stores use memcpy (same bytes, no unaligned store).
	{
		uint16 op = 0x3855;
		uint32 cid = char_id;
		int32 b = bid;
		memcpy(WFIFOP(fd,0), &op, sizeof(op));
		memcpy(WFIFOP(fd,2), &cid, sizeof(cid));
		memcpy(WFIFOP(fd,6), &b, sizeof(b)); // To Return Zeny
	}
	WFIFOB(fd,10) = result;
	WFIFOSET(fd,11);
}

void mapif_parse_Auction_bid(int32 fd)
{
	// Byte-buffer reads at odd read positions: memcpy into aligned locals.
	uint32 char_id = 0, auction_id = 0;
	uint32 bid_tmp = 0;
	memcpy(&char_id, RFIFOP(fd,4), sizeof(char_id));
	memcpy(&auction_id, RFIFOP(fd,8), sizeof(auction_id));
	memcpy(&bid_tmp, RFIFOP(fd,12), sizeof(bid_tmp));
	int32 bid = (int32)bid_tmp;
	std::shared_ptr<struct auction_data> auction = util::umap_find( auction_db, auction_id );

	if( auction == nullptr || auction->price >= bid || auction->seller_id == char_id ){
		mapif_Auction_bid(fd, char_id, bid, 0); // You have failed to bid in the auction
		return;
	}

	if( auction_count(char_id, true) > 4 && bid < auction->buynow && auction->buyer_id != char_id )
	{
		mapif_Auction_bid(fd, char_id, bid, 9); // You cannot place more than 5 bids at a time
		return;
	}

	if( auction->buyer_id > 0 )
	{ // Send Money back to the previous Buyer
		if( auction->buyer_id != char_id )
		{
			mail_sendmail(0, msg_txt(200), auction->buyer_id, auction->buyer_name, msg_txt(201), msg_txt(208), auction->price, nullptr, 0);
			mapif_Auction_message(auction->buyer_id, 7); // You have failed to win the auction
		}
		else
			mail_sendmail(0, msg_txt(200), auction->buyer_id, auction->buyer_name, msg_txt(201), msg_txt(209), auction->price, nullptr, 0);
	}

	auction->buyer_id = char_id;
	safestrncpy(auction->buyer_name, RFIFOCP(fd,16), NAME_LENGTH);
	auction->price = bid;

	if( bid >= auction->buynow )
	{ // Automatic won the auction
		mapif_Auction_bid(fd, char_id, bid - auction->buynow, 1); // You have successfully bid in the auction

		mail_sendmail(0, msg_txt(200), auction->buyer_id, auction->buyer_name, msg_txt(201), msg_txt(210), 0, &auction->item, 1);
		mapif_Auction_message(char_id, 6); // You have won the auction
		mail_sendmail(0, msg_txt(200), auction->seller_id, auction->seller_name, msg_txt(201), msg_txt(211), auction->buynow, nullptr, 0);

		auction_delete(auction);
		return;
	}

	auction_save(auction);

	mapif_Auction_bid(fd, char_id, 0, 1); // You have successfully bid in the auction
}

/*==========================================
 * Packets From Map Server
 *------------------------------------------*/
int32 inter_auction_parse_frommap(int32 fd)
{
	// Byte-buffer read: the read position advances by consumed packet
	// lengths (often odd), so even the plain uint16 opcode load uses memcpy.
	uint16 auction_op = 0;
	memcpy(&auction_op, RFIFOP(fd,0), sizeof(auction_op));
	switch((int32)auction_op)
	{
		case 0x3050: mapif_parse_Auction_requestlist(fd); break;
		case 0x3051: mapif_parse_Auction_register(fd); break;
		case 0x3052: mapif_parse_Auction_cancel(fd); break;
		case 0x3053: mapif_parse_Auction_close(fd); break;
		case 0x3055: mapif_parse_Auction_bid(fd); break;
		default:
			return 0;
	}
	return 1;
}

int32 inter_auction_sql_init(void)
{
	inter_auctions_fromsql();

	return 0;
}

void inter_auction_sql_final(void)
{
	auction_db.clear();

	return;
}
