#include "misc.hpp"
//#include "parse_domain.hpp"

#include "url_database.h"

namespace azure_proxy{
const char* kUrlDatabaseName = "porn.db";
const char* kCreateStatements = "\
create table porn_pics(id integer primary key autoincrement, url text not null unique, created_at datetime  default current_timestamp);\
create index porn_pics_created_at_index on porn_pics(created_at);\
create table porn_pages(id integer primary key autoincrement, domain_name text not null, page_url text not null, porn_pic_id integer references porn_pics(id) on delete set null, created_at datetime default current_timestamp, unique(page_url, porn_pic_id));\
create table white_list(id integer primary key autoincrement, domain_name text not null unique, created_at datetime default current_timestamp);\
create index white_list_created_at_index on white_list(created_at);\
create table black_list(id integer primary key autoincrement, domain_name text not null unique, created_at datetime default current_timestamp);\
create index black_list_created_at_index on black_list(created_at);\
create table tmp_black_list(id integer primary key autoincrement, domain_name text not null unique, created_at datetime default current_timestamp);\
create index tmp_black_list_created_at_index on tmp_black_list(created_at);\
";

const char* kInsertPornPic = "insert or ignore into porn_pics (url) values (:url)";
const char* kGetPornPicID = "select id from porn_pics where url = :url";
const char* kInsertPornPage = "insert or ignore into porn_pages (domain_name, page_url, porn_pic_id) values (:domain_name, :page_url, :porn_pic_id)";
const char* kGetPornPicNumOfDomainName = "select count(*) from porn_pages where domain_name = :domain_name";
const char* kInsertTmpBlackList = "insert or ignore into tmp_black_list (domain_name) values (:domain_name)";
const char* kGetCountTmpBlackList = "select count(*) from tmp_black_list where domain_name=:domain_name";
const char* kInsertBlackList = "insert or ignore into black_list (domain_name) values (:domain_name)";
const char* kGetCountBlackList = "select count(*) from black_list where domain_name=:domain_name";
const char* kInsertWhiteList = "insert or ignore into white_list (domain_name) values (:domain_name)";
const char* kGetCountWhiteList = "select count(*) from white_list where domain_name=:domain_name";

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

UrlDatabase::UrlDatabase(){
	if (sqlite3_open_v2(kUrlDatabaseName, &db_, SQLITE_OPEN_READWRITE, NULL)){
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't open database!"));
	}
	if (sqlite3_prepare_v2(db_, kInsertPornPage, -1, &insert_porn_page_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kInsertPornPic, -1, &insert_porn_pic_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kGetPornPicID, -1, &get_porn_pic_id_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kGetPornPicNumOfDomainName, -1, &get_porn_pic_num_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kInsertTmpBlackList, -1, &insert_tmp_black_list_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kGetCountTmpBlackList, -1, &get_count_tmp_black_list_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kInsertBlackList, -1, &insert_black_list_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kGetCountBlackList, -1, &get_count_black_list_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kInsertWhiteList, -1, &insert_white_list_stmt_, NULL) ||
	sqlite3_prepare_v2(db_, kGetCountWhiteList, -1, &get_count_white_list_stmt_, NULL)
	){
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo("Cann't prepare statment!"));
	}
}

UrlDatabase::~UrlDatabase(){ 
	if (insert_porn_page_stmt_)
		sqlite3_finalize(insert_porn_page_stmt_);
	if (insert_porn_pic_stmt_)
		sqlite3_finalize(insert_porn_pic_stmt_);
	if (db_)
		sqlite3_close(db_);
}

void UrlDatabase::InsertPornPic(std::string url){
	InsertARowWithAString(insert_porn_pic_stmt_, ":url", url);
}

int UrlDatabase::GetPornPicID(std::string url){
	return GetIntWithAString(get_porn_pic_id_stmt_, ":url", url, 0);
}

void UrlDatabase::InsertPornPage(std::string domain_name, std::string porn_url, std::string porn_pic_url){
	int porn_pic_id = GetPornPicID(porn_pic_url);
	if (porn_pic_id){
		if (sqlite3_bind_text(insert_porn_page_stmt_, sqlite3_bind_parameter_index(insert_porn_page_stmt_, ":domain_name"), domain_name.c_str(), domain_name.size(), SQLITE_STATIC) ||
			sqlite3_bind_text(insert_porn_page_stmt_, sqlite3_bind_parameter_index(insert_porn_page_stmt_, ":page_url"), porn_url.c_str(), porn_url.size(), SQLITE_STATIC) ||
			sqlite3_bind_int(insert_porn_page_stmt_, sqlite3_bind_parameter_index(insert_porn_page_stmt_, ":porn_pic_id"), porn_pic_id)){
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
		}
		//(SQLITE_DONE != sqlite3_step(insert_porn_page_stmt_)))
		for (;;){
			if (SQLITE_DONE == sqlite3_step(insert_porn_page_stmt_))
				break;
			//error handle
		}
		sqlite3_reset(insert_porn_page_stmt_);
	}
}

int UrlDatabase::GetPornPicNumOfDomainName(std::string domain_name){
	return GetIntWithAString(get_porn_pic_num_stmt_, ":domain_name", domain_name, 0);
}


void UrlDatabase::InsertIntoTmpBlackList(std::string domain_name){
	InsertARowWithAString(insert_tmp_black_list_stmt_, ":domain_name", domain_name);
}

int UrlDatabase::GetCountTmpBlackList(std::string domain_name){
	return GetIntWithAString(get_count_tmp_black_list_stmt_, ":domain_name", domain_name, 0);
}

void UrlDatabase::InsertIntoBlackList(std::string domain_name){
	InsertARowWithAString(insert_black_list_stmt_, ":domain_name", domain_name);
}

int UrlDatabase::GetCountBlackList(std::string domain_name){
	return GetIntWithAString(get_count_black_list_stmt_, ":domain_name", domain_name, 0);
}

void UrlDatabase::InsertIntoWhiteList(std::string domain_name){
	InsertARowWithAString(insert_white_list_stmt_, ":domain_name", domain_name);
}

int UrlDatabase::GetCountWhiteList(std::string domain_name){
	return GetIntWithAString(get_count_white_list_stmt_, ":domain_name", domain_name, 0);
}

//helpers
void UrlDatabase::InsertARowWithAString(sqlite3_stmt* stmt, std::string argname, std::string argvalue){
	if ( sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, argname.c_str()), argvalue.c_str(), argvalue.size(), SQLITE_STATIC)){
			BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	}
	for (;;){
		int rc = sqlite3_step(stmt);
		if (SQLITE_DONE == rc)
			break;
		//error handle
	}
	sqlite3_reset(stmt);
}

int UrlDatabase::GetIntWithAString(sqlite3_stmt* stmt, std::string argname, std::string argvalue, int retindex){
	if (sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, argname.c_str()), argvalue.c_str(), argvalue.size(), SQLITE_STATIC)){
		BOOST_THROW_EXCEPTION(FatalException() << MessageInfo(sqlite3_errmsg(db_)));
	}

	int ret;
	for (;;){
		int rc = sqlite3_step(stmt);
		if (rc == SQLITE_ROW){
			ret = sqlite3_column_int(stmt, retindex);
			break;
			//const unsigned char *url = sqlite3_column_text(stmt, 1);
			//int type = sqlite3_column_type(stmt, 2);
			//const unsigned char *created_at = sqlite3_column_text(stmt, 2);
		}
		else if (rc == SQLITE_DONE){
			ret = 0;
			break;
		}
	}
	sqlite3_reset(stmt);
	return ret;
}

}