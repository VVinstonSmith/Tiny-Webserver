#include "util.h"
#include "../log/log.h"
#include <iostream>
#include <cstring>
#include "write_html.h"
#include "../lock/locker.h"

locker lock;

void split_str(const char* str, char** substr, int n){
  int k=0, i=0;
  while(str[i]!='\0' && k<n){
    int j = 0;
    while(str[i]!='=') i++;
    i++;
    while(str[i]!='&' && str[i]!='\0'){
      substr[k][j++] = str[i++];
    }
    substr[k][j] = '\0';
    k++;
  }
}

/* INSERT user, item, deal */
void gen_sql_insert_user(char* sql_insert, const char* name, const char* password, const char* role){
  strcpy(sql_insert, "INSERT INTO user(username, passwd, userrole) VALUES(");
  strcat(sql_insert, "'");
  strcat(sql_insert, name);
  strcat(sql_insert, "', '");
  strcat(sql_insert, password);
  strcat(sql_insert, "', '");
  strcat(sql_insert, role);
  strcat(sql_insert, "')");
}

void gen_sql_insert_item(char* sql_insert, const char* name, const char* seller, const char* price){
  strcpy(sql_insert, "INSERT INTO item(itemname, sellername, price) VALUES(");
  strcat(sql_insert, "'");
  strcat(sql_insert, name);
  strcat(sql_insert, "', '");
  strcat(sql_insert, seller);
  strcat(sql_insert, "', '");
  strcat(sql_insert, price);
  strcat(sql_insert, "')");
}

void gen_sql_insert_deal(char* sql_insert, const char* buyer, const char* itemID, const char* num){
  strcpy(sql_insert, "INSERT INTO deal(buyername, itemID, num) VALUES(");
  strcat(sql_insert, "'");
  strcat(sql_insert, buyer);
  strcat(sql_insert, "', '");
  strcat(sql_insert, itemID);
  strcat(sql_insert, "', '");
  strcat(sql_insert, num);
  strcat(sql_insert, "')");
}

/* DELETE */
void gen_sql_delete(char* sql_delete, const char* table, const char* column, const char* target){
  strcpy(sql_delete, "DELETE FROM ");
  strcat(sql_delete, table);
  strcat(sql_delete, " WHERE ");
  strcat(sql_delete, column);
  strcat(sql_delete, " = ");
  strcat(sql_delete, target);
}

/* SELECT item, deal */
void gen_sql_select_item(char* sql_select, const char* itemname, const char* sellername, const char* LP, const char* HP){
  bool first_flag = true;
  strcpy(sql_select, "SELECT * FROM item");
  if(strlen(itemname)!=0){
    if(first_flag){
      strcat(sql_select, " WHERE ");
      first_flag = false;
    }else strcat(sql_select, " and ");
    strcat(sql_select, "itemname='");
    strcat(sql_select, itemname);
    strcat(sql_select, "'");
  }
  if(strlen(sellername)!=0){
    if(first_flag){
      strcat(sql_select, " WHERE ");
      first_flag = false;
    }else strcat(sql_select, " and ");
    strcat(sql_select, "sellername='");
    strcat(sql_select, sellername);
    strcat(sql_select, "'");
  }
  if(strlen(LP)!=0){
    if(first_flag){
      strcat(sql_select, " WHERE ");
      first_flag = false;
    }else strcat(sql_select, " and ");
    strcat(sql_select, "price>=");
    strcat(sql_select, LP);
  }
  if(strlen(HP)!=0){
    if(first_flag){
      strcat(sql_select, " WHERE ");
      first_flag = false;
    }else strcat(sql_select, " and ");
    strcat(sql_select, "price<=");
    strcat(sql_select, HP);
  }
}

void gen_sql_select_deal(char* sql_select, const char* role, const char* name){
  strcpy(sql_select, "SELECT deal.id, item.itemname, deal.buyername, item.sellername, item.price, deal.num ");
  strcat(sql_select, "FROM deal JOIN item ON deal.itemID = item.id WHERE ");
  strcat(sql_select, role);
  strcat(sql_select, "name='");
  strcat(sql_select, name);
  strcat(sql_select, "'");
}
/*
SELECT deal.id, item.itemname, deal.buyername, item.sellername, item.price, deal.num
FROM deal JOIN item
ON deal.itemID = item.id
WHERE buyername = "buyer0" 
*/

int insert_into_itemList(const char* m_string, const char* seller, MYSQL* mysql){
  // m_string形如："name=x&price=x"
  /* 准备参数 */
  char name[20]="", price[20]="";
  char* substr[] = {name, price};
  split_str(m_string, substr, 2);
  /* 插入 */
  char sql_insert[200];
  gen_sql_insert_item(sql_insert, name, seller, price);
  // std::cout<<sql_insert<<std::endl;
  lock.lock();
  int res = mysql_query(mysql, sql_insert);
  lock.unlock();
  return res;
}

int insert_into_dealList(const char* m_url, const char* m_string, const char* buyer, MYSQL* mysql){
  // m_url形如："/login-order&itemID=x"
  // m_string形如："num=x"
  /* 准备参数 */
  while(*m_url!='=') m_url++;
  while(*m_string!='=') m_string++;
  /* 插入 */
  char sql_insert[200];
  gen_sql_insert_deal(sql_insert, buyer, m_url+1, m_string+1);
  lock.lock();
  int res = mysql_query(mysql, sql_insert);
  lock.unlock();
  return res;
}

/* 查看订单(买家&卖家) */
void select_from_dealList(const char* name, const char* role, MYSQL* mysql){
  /* 检索 */
  char sql_select[200];
  gen_sql_select_deal(sql_select, role, name);
  mysql_query(mysql, sql_select);
  MYSQL_RES* result = mysql_store_result(mysql);
  int n_rows = mysql_num_rows(result);
  /* 将结果装入容器 */
  vector<deal> deals(n_rows);
  auto it = deals.begin();
  while(MYSQL_ROW row = mysql_fetch_row(result)){
    it->id = atoi(row[0]);
    it->itemname = string(row[1]);
    it->buyername = string(row[2]);
    it->sellername = string(row[3]);
    it->price = string(row[4]);
    it->num = string(row[5]);
    ++it;
  }
  /* 生成html文档 */
  write_dealList_html(deals, role);
}

/* 查找商品(买家&卖家) */
void select_from_itemList(const char* content, const char* role, MYSQL* mysql){
  /* 准备参数 */
  char itemname[20]="", sellername[20]="", lowest_price[20]="", highest_price[20]="";
  if(strcmp(role, "buyer")==0){
    /* post报文拆分 */
    char* substr[] = {itemname, lowest_price, highest_price};
    split_str(content, substr, 3);
  }else{ // seller
    strcpy(sellername, content);
  }

  /* 生成mysql检索语句，并检索 */
  char sql_select[200];
  gen_sql_select_item(sql_select, itemname, sellername, lowest_price, highest_price);
  // std::cout<<sql_select<<std::endl;
  mysql_query(mysql, sql_select);
  MYSQL_RES* result = mysql_store_result(mysql);
  int rowcount = mysql_num_rows(result); //

  /* 将检索结果放入适当的容器 */
  vector<item> items(rowcount);
  auto it = items.begin();
  while(MYSQL_ROW row = mysql_fetch_row(result)){
    it->id = atoi(row[0]);
    it->itemname = string(row[1]);
    it->sellername = string(row[2]);
    it->price = string(row[3]);
    ++it;
  }

  /* 生成html文档 */
  write_itemList_html(items, role);
}

/* delete deal from dealList */
int delete_from_table(const char* m_string, const char* tablename, const char* colname, MYSQL* mysql){
  // m_string形如：xxID=1&xxID=2&xxID=3
  char sql_delete[300] = "DELETE FROM ";
  strcat(sql_delete, tablename);
  strcat(sql_delete, " WHERE ");
  strcat(sql_delete, colname);
  strcat(sql_delete, " IN (");

  /* 准备数据 */
  int i = 0, count = 0;
  while(m_string[i]!='\0'){
    while(m_string[i]!='=') i++;
    int j = i+1;
    while(m_string[j]!='\0' && m_string[j]!='&') j++;
    if(count!=0)
      strcat(sql_delete,", ");
    strncat(sql_delete, m_string+i+1, j-i-1);
    count++;
    i = j;
  }
  strcat(sql_delete, ")");
  // std::cout<<sql_delete<<std::endl;

  /* 执行删除 */
  lock.lock();
  int res = mysql_query(mysql, sql_delete);
  lock.unlock();
  return res;
}