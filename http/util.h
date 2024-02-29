#ifndef UTIL_H
#define UTIL_H
#include "../CGImysql/sql_connection_pool.h"
#include <string>
#include <map>
#include <vector>

struct item{
  item() {}
  int id;
  string itemname;
  string sellername;
  string price;
};

struct deal{
  deal() {}
  int id;
  int itemID;
  string itemname;
  string sellername;
  string buyername;
  string price;
  string num;
};

void split_str(const char* str, char** substr, int n);

void gen_sql_insert_user(char* sql_insert, const char* name, const char* password, const char* role);
void gen_sql_insert_item(char* sql_insert, const char* name, const char* seller, const char* price);
void gen_sql_insert_deal(char* sql_insert, const char* buyer, const char* seller, const char* item, const char* price);

void gen_sql_delete(char* sql_delete, const char* table, const char* column, const char* itemname);

void gen_sql_select_item(char* sql_select, const char* itemname, const char* sellername, const char* LP, const char* HP);
void gen_sql_select_deal(char* sql_select, const char* role, const char* name);

int insert_into_itemList(const char* m_string, const char* seller, MYSQL* mysql);
int insert_into_dealList(const char* m_url, const char* m_string, const char* buyer, MYSQL* mysql);

void select_from_dealList(const char* name, const char* role, MYSQL* mysql);
void select_from_itemList(const char* content, const char* role, MYSQL* mysql);

int delete_from_table(const char* m_string, const char* tablename, const char* colname, MYSQL* mysql);
#endif