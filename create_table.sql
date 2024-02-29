drop database if exists online_shop;
create database online_shop;
USE online_shop;

CREATE TABLE user(
  username char(50) NULL,
  passwd char(50) NULL,
  userrole char(50) NULL
)ENGINE=InnoDB;

INSERT INTO user(username, passwd, userrole) VALUES('FockeWulf', '1', 'seller');
INSERT INTO user(username, passwd, userrole) VALUES('seller0', '1', 'seller');
INSERT INTO user(username, passwd, userrole) VALUES('buyer0', '1', 'buyer');

CREATE TABLE item(
  id INT(4) PRIMARY KEY AUTO_INCREMENT,
  itemname char(50) NULL,
  sellername char(50) NULL,
  price float(10,2) NULL
)ENGINE=InnoDB;

INSERT INTO item(itemname, sellername, price) VALUES('FW190A8', 'FockeWulf', 10000000);
INSERT INTO item(itemname, sellername, price) VALUES('apple', 'seller0', 4);
INSERT INTO item(itemname, sellername, price) VALUES('banana', 'seller0', 2.5);
INSERT INTO item(itemname, sellername, price) VALUES('blueberry', 'seller0', 10);
INSERT INTO item(itemname, sellername, price) VALUES('lemon', 'seller0', 2.5);
INSERT INTO item(itemname, sellername, price) VALUES('orange', 'seller0', 2.0);
INSERT INTO item(itemname, sellername, price) VALUES('peach', 'seller0', 5.0);
INSERT INTO item(itemname, sellername, price) VALUES('strawberry', 'seller0', 5.5);
INSERT INTO item(itemname, sellername, price) VALUES('watermelon', 'seller0', 1.5);

CREATE TABLE deal(
  id INT(4) PRIMARY KEY AUTO_INCREMENT,
  buyername char(50) NULL,
  itemID INT(4) NULL,
  num INT(5) NULL
)ENGINE=InnoDB;

INSERT INTO deal(buyername, itemID, num) VALUES('buyer0', 1, 1);

