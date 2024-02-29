#ifndef WRITE_HTML_H
#define WRITE_HTML_H
#include "util.h"
#include <fstream>

void write_itemList_html(vector<item>& items, const char* role);

void write_dealList_html(vector<deal>& deals, const char* role);

#endif