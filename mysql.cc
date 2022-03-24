#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

static const char *host = "localhost";
static const char *user = "root";
static const char *password = "1234";
static const char *database = "test";

static void finish_with_error(MYSQL *con)
{
    fprintf(stderr, "MySQL 执行错误: %s\n", mysql_error(con));
    if (con) {
        mysql_close(con);

    }
    exit(EXIT_FAILURE);

}

int main(int argc, char *argv[]) 
{
    if (argc < 1) 
    {
        fprintf(stderr, "必须指定姓名\n");
        return EXIT_FAILURE;

    }

    MYSQL *con = mysql_init(NULL);
    if (con == NULL) 
    {
        finish_with_error(con);

    }


    if (mysql_real_connect(con, host, user, password, database, 0, NULL, 0) == NULL) {
        finish_with_error(con);

    }


    if (mysql_set_character_set(con, "utf8") != 0) 
    {
        finish_with_error(con);

    }

    char query[1000];

    std::string tmp = "SELECT * FROM stu WHERE name like '%'";
    tmp.copy(query, tmp.size(), 0);
    //sprintf(query, "SELECT * FROM stu WHERE name like '%'", argv[1]);
    printf("拼接出的 SQL: %s\n", query);

    if (mysql_query(con, query)) 
    {
        finish_with_error(con);

    }

    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) 
    {
        finish_with_error(con);

    }
    int num_fields = mysql_num_fields(result);
    printf("列数: %d\n", num_fields);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long *lengths;
        lengths = mysql_fetch_lengths(result);
        for (int i = 0; i < num_fields; ++i) {
            printf("[%.*s] ", (int)lengths[i], row[i] ? row[i] : "NULL");

        }
        printf("\n");

    }

    mysql_free_result(result);

    mysql_close(con);

}
