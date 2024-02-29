#include "write_html.h"

using namespace std;

void write_html_head(ofstream& ofs){
  ofs<<"<!DOCTYPE html>"<<endl;
  ofs<<"<html lang=\"en\">"<<endl;
  ofs<<"  <meta charset=\"UTF-8\">"<<endl;
  ofs<<"  <title>webserver</title>"<<endl;
  ofs<<"</head>"<<endl;
  ofs<<"<body>\n"<<endl;
}

void write_itemList_html(vector<item>& items, const char* role){
  ofstream ofs;
  if(strcmp(role, "buyer")==0){
    ofs.open("root/login-itemList-buyer.html", ios::out);
  }else{
    ofs.open("root/login-itemList-seller.html", ios::out);
  }
  
  /* html head */
  write_html_head(ofs);

  /* html body */
  ofs<<"<div id=\"content\">"<<endl;
  ofs<<"  <table width=\"95%\" border=\"0\">"<<endl;
  if(strcmp(role, "seller")==0){
    ofs<<"  <form action=\"login-unshelve\" method=\"post\">"<<endl;
  }
  ofs<<"  <caption>"<<endl;
  ofs<<"    <p align=\"center\"><font size=\"5\"> <strong>商品列表</strong> </font></p>"<<endl;
  ofs<<"    <br/><hr width=\"1000\" align=\"center\"><br/>"<<endl;
  ofs<<"  </caption>"<<endl;

  ofs<<"  <thead height=\"70\">"<<endl;
  ofs<<"  <th width=\"10%\">名称</th>"<<endl;
  ofs<<"  <th width=\"10%\">图片</th>"<<endl;
  if(strcmp(role, "buyer")==0){
    ofs<<"  <th width=\"10%\">卖家</th>"<<endl;
  }
  ofs<<"  <th width=\"10%\">单价</th>"<<endl;
  if(strcmp(role, "buyer")==0){
    ofs<<"  <th width=\"10%\">数量</th>"<<endl;
    ofs<<"  <th width=\"10%\">操作</th>"<<endl;
  }else{
    ofs<<"  <th width=\"10%\">取消商品</th>"<<endl;
  }
  ofs<<"  </thead>"<<endl;

  for(auto& it : items){
    ofs<<"<tbody>"<<endl;
    ofs<<"<tr bgcolor=\"D3D3D3\" height=\"80\">"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    "<<it.itemname<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    <img src=\"pictures/"<<it.itemname<<".jpg\" width=\"100\" height=\"80\"/>"<<endl;
    ofs<<"  </p></td>"<<endl;
    if(strcmp(role, "buyer")==0){ // 显示卖家
      ofs<<"  <td><p align=\"center\">"<<endl;
      ofs<<"    "<<it.sellername<<endl;
      ofs<<"  </p></td>"<<endl;
    }
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    ￥"<<it.price<<"</td>"<<endl;
    ofs<<"  </p></td>"<<endl;
    if(strcmp(role, "buyer")==0){ // 输入购买数量，点击下单
      ofs<<"  <form action=\"login-order&itemID="<<it.id<<"\" method=\"post\">"<<endl;
      ofs<<"    <td><p align=\"center\">"<<endl;
      ofs<<"      <input type=\"text\" name=\"num\" placeholder=\"数量\" style=\"height:20px;width:40px\"  required=\"required\">"<<endl;
      
      ofs<<"    </p></td>"<<endl;
      ofs<<"    <td><p align=\"center\">"<<endl;
      ofs<<"      <div align=\"center\"><button type=\"submit\" >下单</button></div>"<<endl;
      ofs<<"    </p></td>"<<endl;
      ofs<<"  </form>"<<endl;
    }else if(strcmp(role, "seller")==0){ // 勾选取消商品
      ofs<<"  <td><p align=\"center\">"<<endl;
      ofs<<"    <input type=\"checkbox\" name=\"itemID\" value=\""<<it.id<<"\"/>"<<endl;
      ofs<<"  </p></td>"<<endl;
    }
    ofs<<"</tr>"<<endl;
    ofs<<"</tbody>"<<endl;
    ofs<<endl;
  }
  ofs<<"  </table>"<<endl;  
  ofs<<"  <br/><hr width=\"1000\" align=\"center\"><br/>"<<endl;

  if(strcmp(role, "seller")==0){
    ofs<<"<td><div align=\"center\">"<<endl;
    ofs<<"  <button type=\"submit\" >确定</button>"<<endl;
    ofs<<"</div></td>"<<endl;
    ofs<<"</form>"<<endl;
    ofs<<"</div>"<<endl;
    ofs<<"<br/>"<<endl;
  }

  if(strcmp(role, "buyer")==0){
    ofs<<"  <form action=\"login-search_item.html\" method=\"post\">"<<endl;
  }else{
    ofs<<"  <form action=\"login-function-seller.html\" method=\"post\">"<<endl;
  }
  ofs<<"    <div align=\"center\"><button type=\"submit\" >返回</button></div>"<<endl;
  ofs<<"  </form>"<<endl;
  ofs<<"  </div>"<<endl;
  ofs<<"</body>"<<endl;
  ofs<<"</html>"<<endl;

  ofs.close();
}

void write_dealList_html(vector<deal>& deals, const char* role){
  ofstream ofs;
  if(strcmp(role, "buyer")==0){
    ofs.open("root/login-dealList-buyer.html", ios::out);
  }else{
    ofs.open("root/login-dealList-seller.html", ios::out);
  }

  /* html head */
  write_html_head(ofs);

  /* html body */
  ofs<<"<div id=\"content\">"<<endl;
  ofs<<"  <table width=\"95%\" border=\"0\">"<<endl;
  if(strcmp(role, "buyer")==0){
    ofs<<"  <form action=\"login-cancelDeal\" method=\"post\">"<<endl;
  }else{
    ofs<<"  <form action=\"login-cancelDeal\" method=\"post\">"<<endl;
  }
  ofs<<"  <caption>"<<endl;
  ofs<<"    <p align=\"center\"><font size=\"5\"> <strong>订单列表</strong> </font></p>"<<endl;
  ofs<<"    <br/><hr width=\"1000\" align=\"center\"><br/>"<<endl;
  ofs<<"  </caption>"<<endl;

  ofs<<"  <thead height=\"70\">"<<endl;
  ofs<<"  <th width=\"10%\">名称</th>"<<endl;
  ofs<<"  <th width=\"10%\">图片</th>"<<endl;
  if(strcmp(role, "buyer")==0){
    ofs<<"  <th width=\"10%\">卖家</th>"<<endl;
  }else{
    ofs<<"  <th width=\"10%\">买家</th>"<<endl;
  }
  ofs<<"  <th width=\"10%\">单价</th>"<<endl;
  ofs<<"  <th width=\"10%\">数量</th>"<<endl;
  ofs<<"  <th width=\"10%\">取消订单</th>"<<endl;
  ofs<<"  </thead>"<<endl;

  for(auto& it : deals){
    ofs<<"<tbody>"<<endl;
    ofs<<"<tr bgcolor=\"D3D3D3\" height=\"80\">"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    "<<it.itemname<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    <img src=\"pictures/"<<it.itemname<<".jpg\" width=\"100\" height=\"80\"/>"<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    if(strcmp(role, "buyer")==0){
      ofs<<"    "<<it.sellername<<endl;
    }else{
      ofs<<"    "<<it.buyername<<endl;
    }
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    ￥"<<it.price<<"</td>"<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    "<<it.num<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"  <td><p align=\"center\">"<<endl;
    ofs<<"    <input type=\"checkbox\" name=\"dealID\" value=\""<<it.id<<"\"/>"<<endl;
    ofs<<"  </p></td>"<<endl;
    ofs<<"</tr>"<<endl;
    ofs<<"</tbody>"<<endl;
    ofs<<endl;
  }
  ofs<<"  </table>"<<endl;  
  ofs<<"  <br/><hr width=\"1000\" align=\"center\"><br/>"<<endl;
  ofs<<"<td><div align=\"center\">"<<endl;
  ofs<<"  <button type=\"submit\" >确定</button>"<<endl;
  ofs<<"</div></td>"<<endl;
  ofs<<"</form>"<<endl;
  ofs<<"</div>"<<endl;
  ofs<<"<br/>"<<endl;

  if(strcmp(role, "buyer")==0){
    ofs<<"  <form action=\"login-function-buyer.html\" method=\"post\">"<<endl;
  }else{
    ofs<<"  <form action=\"login-function-seller.html\" method=\"post\">"<<endl;
  }
  ofs<<"    <div align=\"center\"><button type=\"submit\" >返回</button></div>"<<endl;
  ofs<<"  </form>"<<endl;
  ofs<<"  </div>"<<endl;
  ofs<<"</body>"<<endl;
  ofs<<"</html>"<<endl;

  ofs.close();
}