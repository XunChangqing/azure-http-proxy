#include <thread>
#include "boost/format.hpp"
#include <boost/bind.hpp>
#include "misc.hpp"
//#include "parse_domain.hpp"
#include "curl.h"
#include "url_database.h"

namespace azure_proxy{

//const char* kInsertPornPic = "insert or replace into porn_pics (id, url, type) values ((select id from porn_pics where url=:url), :url, :type)";
//const char* kInsertPornPage = "insert or replace into porn_pages (id, domain_name, page_url, porn_pic_id) values ((select id from porn_pages where page_url=:page_url and porn_pic_id=:porn_pic_id), :domain_name, :page_url, :porn_pic_id)";
//const char* kInsertIntoDomainList = "insert or ignore into :list_name (domain_name) values (:domain_name)";
//
//const char* kGetItemID = "select id from :table_name (:arg_name) values (:arg_value)";
//const char* kGetItemCount = "select count(*) from :table_name (:arg_name) values (:arg_value)";

const char* kInsertPornPic = "insert or replace into porn_pics (id, url, type) values ((select id from porn_pics where url=\"%1%\"), \"%1%\", \"%2%\")";
const char* kInsertPornPage = "insert or replace into porn_pages (id, domain_name, page_url, porn_pic_id) values ((select id from porn_pages where page_url=\"%2%\" and porn_pic_id=%3%), \"%1%\", \"%2%\", %3%)";
const char* kInsertIntoBlockedPages = "insert or ignore into blocked_pages (url) values (\"%1%\")";
const char* kInsertIntoDomainList = "insert or ignore into %1% (domain_name) values (\"%2%\")";

const char* kGetItemID = "select id from %1% where %2%=\"%3%\"";
const char* kGetItemCount = "select count(*) from %1% where %2%=\"%3%\"";

const char* kPornPicsTable = "porn_pics";
const char* kPornPagesTable = "porn_pages";
const char* kBlockedPagesTable = "blocked_pages";
const char* kTmpBlackListTable = "tmp_black_table";
const char* kBlackListTable = "black_table";
const char* kWhiteListTable = "white_table";
const char* kClearTable = "delete from %%1";
const char* kClearAllTable = "delete from porn_pics; delete from porn_pages; delete from blocked_pages; delete from tmp_black_list; delete from black_list; delete from white_list;";

//const char* kGetPornPicID = "select id from porn_pics where url = :url";
//const char* kGetPornPicNumOfDomainName = "select count(*) from porn_pages where domain_name = :domain_name";
//const char* kInsertTmpBlackList = "insert or ignore into tmp_black_list (domain_name) values (:domain_name)";
//const char* kGetCountTmpBlackList = "select count(*) from tmp_black_list where domain_name=:domain_name";
//const char* kInsertBlackList = "insert or ignore into black_list (domain_name) values (:domain_name)";
//const char* kGetCountBlackList = "select count(*) from black_list where domain_name=:domain_name";
//const char* kInsertWhiteList = "insert or ignore into white_list (domain_name) values (:domain_name)";
//const char* kGetCountWhiteList = "select count(*) from white_list where domain_name=:domain_name";
//const char* kInsertListItemWithDomainName = "insert or ignore into :list_name (domain_name) values (:domain_name)";
//const char* kGetCountItemWithDomainName = "select count(*) from :list_name where domain_name=:domain_name";
//const char* kDeleteUrlItemWithID = "delete from :list_name where id=:id";
//const char* kGetAllListItems = "select * from :list_name";

void UrlDatabase::InitAndCreate(){
	if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD)){
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't config sqlite3 as multithread!"));
	}
	//sqlite3 *db;
	//if (sqlite3_open_v2(kUrlDatabaseName, &db, SQLITE_OPEN_READWRITE, NULL)){
	//	BOOST_LOG_TRIVIAL(info) << "Create new database: " << sqlite3_errmsg(db);
	//	//create new db
	//	//init the db, create tables, indexes
	//	if (sqlite3_open_v2(kUrlDatabaseName, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)){
	//		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't create database!"));
	//	}
	//	//create tables, indexes
	//	if (sqlite3_exec(db, kCreateStatements, NULL, NULL, NULL)){
	//		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't init databse: ") << MessageInfo(sqlite3_errmsg(db)));
	//	}
	//}
	//sqlite3_close(db);
}

UrlDatabase::UrlDatabase(boost::asio::io_service &webaccess_service):
webaccess_service_(webaccess_service)
{
	if (sqlite3_open_v2(GlobalConfig::GetInstance()->GetPornDBPath().c_str(), &db_, SQLITE_OPEN_READWRITE, NULL)){
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't open database: "+GlobalConfig::GetInstance()->GetPornDBPath()));
	}
	//if (sqlite3_prepare_v2(db_, kInsertPornPic, -1, &insert_porn_pic_stmt_, NULL))
	//	BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!" + std::string(sqlite3_errmsg(db_))));
	//if (sqlite3_prepare_v2(db_, kInsertPornPage, -1, &insert_porn_page_stmt_, NULL))
	//	BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!" + std::string(sqlite3_errmsg(db_))));
	//if (sqlite3_prepare_v2(db_, kInsertIntoDomainList, -1, &insert_domain_stmt_, NULL))
	//	BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!" + std::string(sqlite3_errmsg(db_))));
	//if (sqlite3_prepare_v2(db_, kGetItemID, -1, &get_item_id_stmt_, NULL))
	//	BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!" + std::string(sqlite3_errmsg(db_))));
	//if (sqlite3_prepare_v2(db_, kGetItemCount, -1, &get_item_count_stmt_, NULL))
	//	BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!" + std::string(sqlite3_errmsg(db_))));
}

static void FinalizeStatement(sqlite3_stmt *stmt){
	if (stmt)
		sqlite3_finalize(stmt);
}

UrlDatabase::~UrlDatabase(){
	//FinalizeStatement(insert_porn_pic_stmt_);
	//FinalizeStatement(insert_porn_page_stmt_);
	//FinalizeStatement(insert_domain_stmt_);
	//FinalizeStatement(get_item_id_stmt_);
	//FinalizeStatement(get_item_count_stmt_);
	if (db_)
		sqlite3_close(db_);
}

void UrlDatabase::InsertPornPic(std::string url, int type){
	boost::format fmt(kInsertPornPic);
	fmt%url; fmt%type;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	if (sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL))
		BOOST_LOG_TRIVIAL(warning) << "Failed to insert porn pic";
}

void UrlDatabase::InsertPornPage(std::string domain_name, std::string porn_url, std::string porn_pic_url){
	int porn_pic_id = GetPornPicID(porn_pic_url);
	if (porn_pic_id){
		boost::format fmt(kInsertPornPage);
		fmt%domain_name; fmt%porn_url; fmt%porn_pic_id;
		//BOOST_LOG_TRIVIAL(debug) << fmt.str();
		if (sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL))
			BOOST_LOG_TRIVIAL(warning) << "Failed to insert porn page";
	}
}

void UrlDatabase::InsertBlockedPage(std::string url){
	boost::format fmt(kInsertIntoBlockedPages);
	fmt%url;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	if (sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL))
		BOOST_LOG_TRIVIAL(warning) << "Failed to insert list item";
}

void UrlDatabase::InsertIntoTmpBlackList(std::string domain_name){
	InsertListItem("tmp_black_list", domain_name);
	//async post to web server
	webaccess_service_.post(boost::bind<void>([domain_name](){
		CURL *curl;
		CURLcode res;
		/* get a curl handle */
		curl = curl_easy_init();
		if (curl) {
			/* First set the URL that is about to receive our POST. This URL can
			just as well be a https:// URL if that is what should receive the
			data. */
			curl_easy_setopt(curl, CURLOPT_URL, kCreateTmpDomainNameUrl);
			/* Now specify the POST data */
			std::string param_domain_name = "domain_name=" + domain_name;
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, param_domain_name.c_str());

			/* Perform the request, res will get the return code */
			res = curl_easy_perform(curl);
			/* Check for errors */
			if (res != CURLE_OK)
				BOOST_LOG_TRIVIAL(warning) << "Failed to post tmp black list to web server!";
			else
				BOOST_LOG_TRIVIAL(info) << "Post tmp black to web server: " << domain_name;
				//fprintf(stderr, "curl_easy_perform() failed: %s\n",
				//curl_easy_strerror(res));


			/* always cleanup */
			curl_easy_cleanup(curl);
		}
	}
	));
}
void UrlDatabase::InsertIntoBlackList(std::string domain_name){
	InsertListItem("black_list", domain_name);
}
void UrlDatabase::InsertIntoWhiteList(std::string domain_name){
	InsertListItem("white_list", domain_name);
}

bool UrlDatabase::InsertListItem(std::string list_name, std::string domain_name){
	boost::format fmt(kInsertIntoDomainList);
	fmt%list_name; fmt%domain_name;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	if (sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL)){
		BOOST_LOG_TRIVIAL(warning) << "Failed to insert list item";
		return false;
	}
	else
		return true;
}

static int RetIntCallback(void* retint, int cols, char** col_values, char** col_names){
	if (cols < 1)
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Number of cols less than 1"));
	int * ret = (int*)retint;
	*ret = std::stoi(col_values[0]);
	return SQLITE_OK;
}

int UrlDatabase::GetPornPicNumOfDomainName(std::string domain_name){
	int ret = 0;
	boost::format fmt(kGetItemCount);
	fmt%"porn_pages"; fmt%"domain_name"; fmt%domain_name;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	sqlite3_exec(db_, fmt.str().c_str(), RetIntCallback, &ret, NULL);
	return ret;
}

int UrlDatabase::GetPornPicID(std::string url){
	int ret = 0;
	boost::format fmt(kGetItemID);
	fmt%"porn_pics"; fmt%"url"; fmt%url;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	sqlite3_exec(db_, fmt.str().c_str(), RetIntCallback, &ret, NULL);
	return ret;
}

int UrlDatabase::GetIdTmpBlackList(std::string domain_name){
	int ret = 0;
	boost::format fmt(kGetItemID);
	fmt%"tmp_black_list"; fmt%"domain_name"; fmt%domain_name;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	sqlite3_exec(db_, fmt.str().c_str(), RetIntCallback, &ret, NULL);
	return ret;
}

int UrlDatabase::GetIdBlackList(std::string domain_name){
	int ret = 0;
	boost::format fmt(kGetItemID);
	fmt%"black_list"; fmt%"domain_name"; fmt%domain_name;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	sqlite3_exec(db_, fmt.str().c_str(), RetIntCallback, &ret, NULL);
	return ret;
}

int UrlDatabase::GetIdWhiteList(std::string domain_name){
	int ret = 0;
	boost::format fmt(kGetItemID);
	fmt%"white_list"; fmt%"domain_name"; fmt%domain_name;
	//BOOST_LOG_TRIVIAL(debug) << fmt.str();
	sqlite3_exec(db_, fmt.str().c_str(), RetIntCallback, &ret, NULL);
	return ret;
}

void UrlDatabase::ClearAllTables(){
	//boost::format fmt(kClearTable);
	//fmt%kPornPicsTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	//fmt.clear();
	//fmt%kPornPagesTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	//fmt.clear();
	//fmt%kBlockedPagesTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	//fmt.clear();
	//fmt%kTmpBlackListTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	//fmt.clear();
	//fmt%kBlackListTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	//fmt.clear();
	//fmt%kWhiteListTable;
	//sqlite3_exec(db_, fmt.str().c_str(), NULL, NULL, NULL);
	sqlite3_exec(db_, kClearAllTable, NULL, NULL, NULL);
}

//helpers
void UrlDatabase::Test()
{
	using namespace boost;
	using namespace boost::asio;
	boost::asio::io_service webaccess_service;
	boost::asio::io_service *pwebaccess_service = &webaccess_service;
	optional<io_service::work> webaccess_work(
		in_place(ref(webaccess_service)));

	std::vector<std::thread> webaccess_td_vec;

	for (auto i = 0u; i < 2; ++i) {
		webaccess_td_vec.emplace_back([pwebaccess_service]() {
			try {
				pwebaccess_service->run();
			}
			catch (const boost::exception &e){
				BOOST_LOG_TRIVIAL(fatal) << boost::diagnostic_information(e);
				exit(EXIT_FAILURE);
			}
			catch (const std::exception &e) {
				BOOST_LOG_TRIVIAL(fatal) << e.what();
				exit(EXIT_FAILURE);
			}
		});
	}

	for (int i = 0; i < 10; ++i){
		UrlDatabase db(webaccess_service);
		int tmp;
		db.InsertBlockedPage("bbb.com/sdfsdf");
		db.InsertBlockedPage("bbb.com/sdfsdf");
		db.InsertBlockedPage("ccc.com/sdfsdf");
		//insert porn pics
		db.InsertPornPic("porn1", 2);
		db.InsertPornPic("porn1", 2);
		db.InsertPornPic("porn2", 3);
		//get porn id
		tmp = db.GetPornPicID("porn1");
		if (!tmp)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Faied to get porn ID"));
		tmp = db.GetPornPicID("porn3");
		if (tmp)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Faied to get porn ID"));
		//inesrt porn pages
		db.InsertPornPage("aaa.com", "sdfsdfiij", "porn1");
		db.InsertPornPage("bbb.com", "sdfsdfiij", "porn1");
		db.InsertPornPage("bbb.com", "sdfsdfsdfiij", "porn2");
		db.InsertPornPage("bbb.com", "sdfsdfsdfiij", "porn2");
		db.InsertPornPage("bbb.com", "sdfsdfsdfiij", "porn2");
		//get domain number
		tmp = db.GetPornPicNumOfDomainName("aaa.com");
		if (tmp != 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Porn number error"));
		tmp = db.GetPornPicNumOfDomainName("bbb.com");
		if (tmp != 2)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Porn number error"));
		tmp = db.GetPornPicNumOfDomainName("ccc.com");
		if (tmp != 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Porn number error"));

		//insert into tmp_black_list
		db.InsertIntoTmpBlackList("xxx.com");
		db.InsertIntoTmpBlackList("xxx.com");
		db.InsertIntoTmpBlackList("yyy.com");
		tmp = db.GetIdTmpBlackList("xxx.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("tmp number error"));
		tmp = db.GetIdTmpBlackList("yyy.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("tmp number error"));
		tmp = db.GetIdTmpBlackList("zzz.com");
		if (tmp != 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("tmp number error"));
		//insert into black_list
		db.InsertIntoBlackList("xxx.com");
		db.InsertIntoBlackList("xxx.com");
		db.InsertIntoBlackList("yyy.com");
		tmp = db.GetIdBlackList("xxx.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("black number error"));
		tmp = db.GetIdBlackList("yyy.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("black number error"));
		tmp = db.GetIdBlackList("zzz.com");
		if (tmp != 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("black number error"));
		//insert into white_list
		db.InsertIntoWhiteList("xxx.com");
		db.InsertIntoWhiteList("xxx.com");
		db.InsertIntoWhiteList("yyy.com");
		tmp = db.GetIdWhiteList("xxx.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("white number error"));
		tmp = db.GetIdWhiteList("yyy.com");
		if (tmp == 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("white number error"));
		tmp = db.GetIdWhiteList("zzz.com");
		if (tmp != 0)
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("white number error"));

		db.ClearAllTables();
		//get item from tmp_black_list

		//get item from black_list

		//get item from white_list
	}

	webaccess_work = boost::none;
	for (auto &td : webaccess_td_vec) {
		td.join();
	}

	//void UrlDatabase::InsertIntoListWithDomainName(std::string list_name, std::string domain_name){
	//	if ( sqlite3_bind_text(insert_list_with_domain_name_stmt_, sqlite3_bind_parameter_index(insert_list_with_domain_name_stmt_, "list_name"), list_name.c_str(), list_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	if ( sqlite3_bind_text(insert_list_with_domain_name_stmt_, sqlite3_bind_parameter_index(insert_list_with_domain_name_stmt_, "domain_name"), domain_name.c_str(), domain_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	for (;;){
	//		int rc = sqlite3_step(insert_list_with_domain_name_stmt_);
	//		if (SQLITE_DONE == rc)
	//			break;
	//		//error handle
	//	}
	//	sqlite3_reset(insert_list_with_domain_name_stmt_);
	//}
	//
	//int UrlDatabase::GetCountListItemWithDomainName(std::string list_name, std::string domain_name){
	//	sqlite3_stmt *stmt = insert_list_with_domain_name_stmt_;
	//	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "list_name"), list_name.c_str(), list_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "domain_name"), domain_name.c_str(), domain_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	int ret;
	//	for (;;){
	//		int rc = sqlite3_step(stmt);
	//		if (rc == SQLITE_ROW){
	//			ret = sqlite3_column_int(stmt, 0);
	//			break;
	//			//const unsigned char *url = sqlite3_column_text(stmt, 1);
	//			//int type = sqlite3_column_type(stmt, 2);
	//			//const unsigned char *created_at = sqlite3_column_text(stmt, 2);
	//		}
	//		else if (rc == SQLITE_DONE){
	//			ret = 0;
	//			break;
	//		}
	//	}
	//	sqlite3_reset(stmt);
	//	return ret;
	//}
	//
	//void UrlDatabase::DeleteListItemWithID(std::string list_name, int id){
	//	sqlite3_stmt *stmt = delete_list_item_with_id_stmt_;
	//	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "list_name"), list_name.c_str(), list_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	std::string idstr = std::to_string(id);
	//	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "id"), idstr.c_str(), idstr.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	for (;;){
	//		int rc = sqlite3_step(stmt);
	//		if (SQLITE_DONE == rc)
	//			break;
	//	}
	//	sqlite3_reset(stmt);
	//}
	//
	//std::vector<url_item> UrlDatabase::GetALlListItems(std::string list_name){
	//	sqlite3_stmt *stmt = get_all_list_items_stmt_;
	//	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, "list_name"), list_name.c_str(), list_name.size(), SQLITE_STATIC)){
	//			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	//	}
	//	std::vector<url_item> ret_items;
	//	int ret;
	//	for (;;){
	//		int rc = sqlite3_step(stmt);
	//		if (rc == SQLITE_ROW){
	//			url_item new_item;
	//			new_item.id = sqlite3_column_int(stmt, 0);
	//			new_item.domain_name = (const char*)sqlite3_column_text(stmt, 1);
	//			new_item.created_at = (const char*)sqlite3_column_text(stmt, 2);
	//			ret_items.push_back(new_item);
	//		}
	//		else if (rc == SQLITE_DONE){
	//			break;
	//		}
	//	}
	//	sqlite3_reset(stmt);
	//	return ret_items;
	//}

}

}