#ifndef HEADER_URL_DATABASE
#define HEADER_URL_DATABASE

#include <string>
#include <vector>
#include <sqlite3.h>

namespace azure_proxy{
class UrlDatabase{
public:
	static void InitAndCreate();
	UrlDatabase();
	~UrlDatabase();// { if (db_) sqlite3_close(db_); }
	void InsertPornPic(std::string url);
	int GetPornPicID(std::string url);
	void InsertPornPage(std::string domain_name, std::string porn_url, std::string porn_pic_url);
	int GetPornPicNumOfDomainName(std::string domain_name);
	void InsertIntoTmpBlackList(std::string domain_name);
	int GetCountTmpBlackList(std::string domain_name);
	void InsertIntoBlackList(std::string domain_name);
	int GetCountBlackList(std::string domain_name);
	void InsertIntoWhiteList(std::string domain_name);
	int GetCountWhiteList(std::string domain_name);
	//std::vector<int> GetPornPicIDsOfDomainName(std::string domain_name);
private:
	sqlite3 *db_;
	sqlite3_stmt* insert_porn_pic_stmt_;
	sqlite3_stmt* get_porn_pic_id_stmt_;
	sqlite3_stmt *insert_porn_page_stmt_;
	sqlite3_stmt* get_porn_pic_num_stmt_;
	sqlite3_stmt* insert_tmp_black_list_stmt_;
	sqlite3_stmt* get_count_tmp_black_list_stmt_;
	sqlite3_stmt* insert_black_list_stmt_;
	sqlite3_stmt* get_count_black_list_stmt_;
	sqlite3_stmt* insert_white_list_stmt_;
	sqlite3_stmt* get_count_white_list_stmt_;

	//helpers
	void InsertARowWithAString(sqlite3_stmt* stmt, std::string argname, std::string argvalue);
	int GetIntWithAString(sqlite3_stmt* stmt, std::string argname, std::string argvalue, int retindex);
};

}
#endif