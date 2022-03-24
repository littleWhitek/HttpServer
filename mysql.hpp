#ifndef __MYSQL_HPP__
#define __MYSQL_HPP__

#include <mysql.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <iostream>

static const char *host = "localhost";
static const char *user = "root";
static const char *password = "1234";
static const char *database = "http";

static const char* ErrLevel[] = {
	"Normal", //正常
	"Warning", //警告
	"Error"   //错误
};

class MysqlAPI
{
    public:
        MysqlAPI();
        void DisConnectDB();
        bool ConnectDB();
        bool ExecSql(std::string&);
        bool StoreResult();
        MYSQL_ROW GetNextRow(); //获取查询结果集中的下一条记录
        void GotoRowsFirst();  // 移动到数据集的开始
        void PrintRows();     //打印记录
        void FreeResult();   //释放查询的数据集的内存资源


        ~MysqlAPI();
    private:
        MYSQL m_mysql;
        MYSQL_RES *m_query;
        MYSQL_ROW m_row;

        bool m_bConnect;
        int m_num_field;
        int m_num_count;
};

static MysqlAPI *mysql = new MysqlAPI;

#define LOG(msg,level) log(msg,level,__FILE__,__LINE__);

static void log(std::string msg,int level,std::string file,int line)
{
    char line_buf[1000] = {0};
    sprintf(line_buf,"%d",line);
    std::string sql = "insert into http(file,line,msg,level) values('";
    sql += file;
    sql += "','";
    sql += line_buf;
    sql += "','";
    sql += msg;
    sql += "','";
    sql += ErrLevel[level];
    sql += "');";
    bool ret = mysql->ExecSql(sql);
    if(ret == false)
    {
        std::cout << "ExecSql failed!" << std::endl;
    }

	std::cout << file << ": "<<line << " " <<msg << ": " << ErrLevel[level] <<std::endl;
}


static bool InitMysql()
{
    bool ret = mysql->ConnectDB();
    if(ret == false)
    {
        std::cout << "mysql ConnectDB failed!" << std::endl;
        return false;
    }
    std::string sql = "create table if not exists http(id bigint unsigned not null auto_increment primary key, \
        file varchar(30) not null,line varchar(30) not null, msg varchar(60) not null,level varchar(30) not null);";
    ret = mysql->ExecSql(sql);
    if(ret == false)
    {
        std::cout << "ExecSql failed!" << std::endl;
        return false;
    }
    return true;
}

#endif
