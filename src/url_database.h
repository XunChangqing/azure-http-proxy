#ifndef HEADER_URL_DATABASE
#define HEADER_URL_DATABASE

#include <string>
#include <vector>
#include <sqlite3.h>

namespace azure_proxy{

class UrlDatabase{
public:
	static void InitAndCreate();
	static void Test();
	UrlDatabase();
	~UrlDatabase();// { if (db_) sqlite3_close(db_); }
	void InsertPornPic(std::string url, int type);
	int GetPornPicID(std::string url);
	void InsertPornPage(std::string domain_name, std::string porn_url, std::string porn_pic_url);
	int GetPornPicNumOfDomainName(std::string domain_name);
	void InsertBlockedPage(std::string url);

	//domain_name list operations
	//void InsertIntoListWithDomainName(std::string list_name, std::string domain_name);
	//int GetCountListItemWithDomainName(std::string list_name, std::string domain_name);
	//void DeleteListItemWithID(std::string list_name, int id);
	//std::vector<url_item> GetALlListItems(std::string list_name);

	void InsertIntoTmpBlackList(std::string domain_name);
	int GetIdTmpBlackList(std::string domain_name);
	void InsertIntoBlackList(std::string domain_name);
	int GetIdBlackList(std::string domain_name);
	void InsertIntoWhiteList(std::string domain_name);
	int GetIdWhiteList(std::string domain_name);
	//std::vector<int> GetPornPicIDsOfDomainName(std::string domain_name);
private:
	sqlite3 *db_;
	sqlite3_stmt *insert_porn_pic_stmt_;
	sqlite3_stmt *insert_porn_page_stmt_;
	sqlite3_stmt *insert_domain_stmt_;
	sqlite3_stmt *get_item_id_stmt_;
	sqlite3_stmt *get_item_count_stmt_;

	//helpers
	void ExcuteNonQuery(sqlite3_stmt *stmt);
	int GetItemIDorCount(sqlite3_stmt* stmt, std::string table_name, std::string arg_name, std::string arg_value);
	int ExcuteRetInt(sqlite3_stmt* stmt);
	void InsertListItem(std::string list_name, std::string domain_name);

	void ClearAllTables();
};
}
#endif