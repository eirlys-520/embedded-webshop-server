#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sqlite3.h>
void urldecode(char *str);
// 根据文件名后缀返回对应的mime类型
char* get_mime(char *filename)
{
    int len = strlen(filename);
    if(len >=4 && 0 == strcmp(filename + len -4,".jpg"))
    {
        return "image/jpeg";
    }
    if(len >=4 && 0 == strcmp(filename + len -4,".png"))
    {
        return "image/png";
    }
    // 默认html
    return "text/html;charset=utf-8";
}

// 修改send_head：传入文件名，自动匹配mime
int send_head(int conn, char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (NULL == fp)
    {
        perror("send_head fopen");
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    char buf_all[512]={0};
    char *mime = get_mime(filename);
    sprintf(buf_all,"HTTP/1.1 200 OK\r\n"
                    "Date: Tue, 09 Sep 2026 08:00:00 GMT\r\n"
                    "Content-Type: %s\r\n"
                    "Server: PTSERVER\r\n"
                    "Content-Length: %ld\r\n"
                    "Connection: closed\r\n\r\n",mime,size);
    send(conn, buf_all, strlen(buf_all),0);
    return 0;
}

int send_file(int conn, char *filename)
{
    printf("send_file open:[%s]\n",filename);
    if(send_head(conn, filename) !=0 )
    {
        return 1;
    }
    int fd = open(filename, O_RDONLY);
    if (-1 == fd)
    {
        perror("send_file open");
        return 1;
    }
    while (1)
    {
        char buf[1024] = {0};
        int rd_ret = read(fd, buf, sizeof(buf));
        if (rd_ret <= 0)
        {
            break;
        }
        send(conn, buf, rd_ret, 0);
    }
    close(fd);
    return 0;
}

int send_redirect(int conn, char *location)
{
    char buf[256]={0};
    sprintf(buf,"HTTP/1.1 302 Found\r\n"
                "Location: %s\r\n"
                "Connection: close\r\n\r\n",location);
    send(conn,buf,strlen(buf),0);
    return 0;
}

int callback(void *arg,int argc,char **argv,char **name)
{
    int *is_ok = (int*)arg;
    *is_ok = 1;
    return 0;
}

int check_user_from_db(char *username,char *password)
{
    sqlite3 *db;
    char *errmsg = NULL;
    int ret = sqlite3_open("user.db",&db);
    if (ret != SQLITE_OK)
    {
        printf("sqlite3 open fail\n");
        sqlite3_close(db);
        return 0;
    }
    char sql[1024] = {0};
    sprintf(sql,"SELECT * FROM user WHERE username = '%s' AND password='%s';",username,password);
    int login_flag = 0;
    ret = sqlite3_exec(db,sql,callback,(void *)&login_flag,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("sqlite_exec error : %s\n",errmsg);
        sqlite3_free(errmsg);
    }
    sqlite3_close(db);
    return login_flag;
}

int register_user(char *username,char *password)
{
    sqlite3 *db;
    char *errmsg = NULL;
    int ret = sqlite3_open("user.db",&db);
    if(ret != SQLITE_OK){
        sqlite3_close(db);
        return 0;
    }
    char sql[1024]={0};
    sprintf(sql,"INSERT INTO user(username,password) VALUES ('%s','%s');",username,password);
    ret = sqlite3_exec(db,sql,NULL,NULL,&errmsg);
    if(ret != SQLITE_OK){
        printf("register err: %s\n",errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

void init_db_table()
{
    sqlite3 *db;
    char *errmsg=NULL;
    sqlite3_open("user.db",&db);
    char *create_sql="CREATE TABLE IF NOT EXISTS user(username TEXT PRIMARY KEY,password TEXT);";
    sqlite3_exec(db,create_sql,NULL,NULL,&errmsg);
    if(errmsg)
    {
        sqlite3_free(errmsg);
    }
    sqlite3_close(db);
}

int write_5html(void *arg, int col, char **result, char **title)
{
    FILE *fp = (FILE *)arg;
    // result[0]=goods_id，result[1]=goods_img
    fprintf(fp,
        "<div style='display:inline-block;margin:10px;text-align:center;'>"
        "<a href='/detail?id=%s'><img src='/img/%s' width='140'></a>"
        "<p><a href='/detail?id=%s'>%s</a></p>"
        "</div>\n",
        result[0], result[2], result[0], result[1]);
    fflush(fp);
    return 0;
}

int write_buf_html(void *arg, int col, char **result, char **title)
{
    char *buf = (char *)arg;
    char tmp[512]={0};
    sprintf(tmp,
        "<div style='display:inline-block;margin:10px;text-align:center;'>"
        "<a href='/detail?id=%s'><img src='/img/%s' width='140'></a>"
        "<p><a href='/detail?id=%s'>%s</a></p>"
        "</div>\n",
        result[0], result[2], result[0], result[1]);
    strcat(buf, tmp);
    return 0;
}


int genrate_5html(char *filename, char *want_goods)
{
    FILE *fp = fopen(filename, "w");
    if (NULL == fp)
    {
        perror("genrate_5html fopen");
        return 1;
    }
    fprintf(fp,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset='utf-8'>\n"
        "<title>器件搜索</title>\n"
        "</head>\n"
        "<body>\n"
        "<h2 align='center'>电子器件搜索</h2>\n"
        "<div align='center'>\n"
        "<form method='get' action='/goods_list'>\n"
        "输入器件：<input type='text' name='key' value='%s'>\n"
        "<input type='submit' value='查询'>\n"
        "</form>\n"
        "</div>\n", want_goods);

    sqlite3 *db;
    int ret = sqlite3_open("component.db", &db);
    if (SQLITE_OK != ret)
    {
        fprintf(fp,"<p>component.db打开失败</p>");
        sqlite3_close(db);
        fclose(fp);
        return 1;
    }

    char *errmsg = NULL;
    char sql_cmd[512]={0};
    sprintf(sql_cmd,
        "select goods_id,goods_name,goods_img from goods where goods_name like '%%%s%%';",
        want_goods);

    ret = sqlite3_exec(db, sql_cmd, write_5html, fp, &errmsg);
    if(ret != SQLITE_OK)
    {
        fprintf(fp,"<p>查询出错</p>");
        fprintf(stderr,"sql:%s err:%s\n",sql_cmd,errmsg);
        sqlite3_free(errmsg);
    }

    sqlite3_close(db);
    fprintf(fp,"</body>\n</html>");
    fclose(fp);
    return 0;
}

//详情页回调：把查到的单条商品写入缓冲区
int detail_cb(void *arg, int argc, char **argv, char **azColName)
{
    char *html = (char*)arg;
    // argv[0] goods_name
    // argv[1] good_spec
    // argv[2] shop_price
    // argv[3] goods_img
    char tmp[2048]={0};
    snprintf(tmp,sizeof(tmp),
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><meta charset='utf-8'><title>商品详情</title></head>\n"
            "<body align='center'>\n"
            "<h2>商品详情</h2>\n"
            "<img src='/img/%s' width='240'><br><br>\n"
            "<p>器件名称：%s</p>\n"
            "<p>规格参数：%s</p>\n"
            "<p>售价：¥%s</p>\n"
            "<br>\n"
            "<a href='/home.html'>← 返回首页</a>\n"
            "</body>\n</html>",
    argv[3], argv[0], argv[1], argv[2]);

    strcpy(html, tmp);
    return 0;
}

int home_goods_cb(void *arg, int col, char **result, char **title)
{
    FILE *fp = (FILE*)arg;
    fprintf(fp,
"<div style='display:inline-block;margin:10px;text-align:center;'>"
"<a href=\"/detail?id=%s\">"
"<img src=\"/img/%s\" width=\"120\"><br>"
"<p>%s</p>"
"</a>"
"</div>\n",
    result[0], result[2], result[1]);
    fflush(fp);
    return 0;
}


int main(void)
{
    init_db_table();
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket error");
        return -1;
    }
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    struct sockaddr_in seraddr;
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(80);
    seraddr.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr));
    if (-1 == ret)
    {
        perror("bind error");
        return -1;
    }
    ret = listen(sockfd, 10);
    if (-1 == ret)
    {
        perror("listen error");
        return -1;
    }
    while (1)
    {
        int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
        if (-1 == connfd)
        {
            perror("accept error");
            close(connfd);
            continue;
        }
        char buff[1024] = {0};
        memset(buff, 0, sizeof(buff));
        ssize_t size = recv(connfd, buff, sizeof(buff), 0);
        if (size <= 0)
        {
            close(connfd);
            continue;
        }
        printf("req buff: %s\n", buff);
        char *method = NULL;
        char *url = NULL;
        char *ver = NULL;
        method = strtok(buff, " ");
        url = strtok(NULL, " ");
        ver = strtok(NULL, "\r");

        if(0 == strcmp(url,"/"))
        {
            send_file(connfd,"login.html");
        }
        else if(0 == strcmp(url,"/home.html"))
        {
            send_file(connfd,"home.html");
        }
        else if(strcmp(url,"/home_goods") ==0)
        {
            FILE *tmpf = tmpfile();
            sqlite3 *db;
            sqlite3_open("component.db",&db);
            char *errmsg=NULL;
            sqlite3_exec(db,"select goods_id,goods_name,goods_img from goods;", home_goods_cb, tmpf, &errmsg);
            sqlite3_free(errmsg);
            sqlite3_close(db);

            fseek(tmpf,0,SEEK_END);
            long len = ftell(tmpf);
            rewind(tmpf);
            char goods_html[4096]={0};
            fread(goods_html,1,len,tmpf);
            fclose(tmpf);

            char header[512];
            snprintf(header,sizeof(header),
                "HTTP/1.1 200 OK\r\nContent-Type:text/html;charset=utf-8\r\nContent-Length:%ld\r\nConnection:close\r\n\r\n",
                strlen(goods_html));
            send(connfd,header,strlen(header),0);
            send(connfd,goods_html,strlen(goods_html),0);

            close(connfd);
            continue;
        }
        else if(0 == strncmp(url,"/register",9))
        {
            send_file(connfd,"register.html");
        }
        else if(0 == strcmp(url,"/log_error"))
        {
            send_file(connfd,"log_error.html");
        }
        // ==========图片请求处理重点==========
        else if(0 == strncmp(url,"/img/",5))
        {
            // url="/img/xxx.jpg" → url+1 → "img/xxx.jpg"
            send_file(connfd, url + 1);
        }
        else if(0 == strcmp(url,"/favicon.ico"))
        {
            send_file(connfd,"img/2.jpg");
        }
        else if(0 == strncmp(url,"/do_register",12))
        {
            char *name = strchr(url,'=');
            if(!name) 
            { 
                close(connfd); 
                continue;
            }
            name++;
            char *end = strchr(name,'&');
            if(!end) 
            { 
                close(connfd);
                continue;
            }
            *end = '\0';
            char *pass = strchr(end+1,'=');
            if(!pass) 
            { 
                close(connfd);
                continue;
            }
            pass++;
            urldecode(name);
            urldecode(pass);
            int ret = register_user(name,pass);
            if(ret == 1)
            {
                send_redirect(connfd,"/");
            }
            else
            {
                send_file(connfd,"reg_err.html");
            }
            close(connfd);
            continue;
        }
        else if(0 == strncmp(url, "/login",6))
        {
            char *name = strchr(url, '=');
            if(!name)
            {
                send_file(connfd,"login.html");
                close(connfd);
                continue;
            }
            name += 1;
            char *end = strchr(name, '&');
            if(!end)
            {
                send_file(connfd,"login.html");
                close(connfd);
                continue;
            }
            *end = '\0';
            char *pass = strchr(end + 1, '=');
            if(!pass)
            {
                send_file(connfd,"login.html");
                close(connfd);
                continue;
            }
            pass += 1;
            urldecode(name);
            urldecode(pass);
            if(check_user_from_db(name,pass)==1)
            {
                send_redirect(connfd,"/home.html");
            }
            else
            {
                send_redirect(connfd,"/log_error");
            }
            close(connfd);
            continue;
        }
        else if(0 == strncmp(url,"/search",7))
        {
            send_file(connfd,"home.html");
        }
        else if(0 == strncmp(url,"/goods_list",11))
        {
            char want_buf[128]={0};
            char *p_q = strchr(url,'?');
            if(p_q != NULL)
            {
                char *p_key = strstr(p_q,"key=");
                if(p_key != NULL)
                {
                    p_key +=4;
                    char *p_amp = strchr(p_key,'&');
                    if(p_amp) *p_amp = '\0';
                    strncpy(want_buf,p_key,sizeof(want_buf)-1);
                    urldecode(want_buf);
                }
            }
            genrate_5html("5.html", want_buf);
            send_file(connfd,"5.html");
            close(connfd);
            continue;
        }
        else if(0 == strncmp(url,"/detail",7))
        {
            char id_buf[64]={0};
            //解析url /detail?id=xx
            char *p_q = strchr(url,'?');
            if(p_q != NULL)
            {
                char *p_id = strstr(p_q,"id=");
                if(p_id != NULL)
                {
                    p_id +=3;
                    char *p_amp = strchr(p_id,'&');
                    if(p_amp) *p_amp = '\0';
                    strncpy(id_buf, p_id, sizeof(id_buf)-1);
                }
            }

            char html_buf[4096]={0};
            sqlite3 *db;
            int ret = sqlite3_open("component.db",&db);
            if(ret != SQLITE_OK)
            {
                strcpy(html_buf,"<html><body><h2>数据库打开失败</h2><a href='/home.html'>返回</a></body></html>");
            }
            else
            {
                char sql[512]={0};
                sprintf(sql,"SELECT goods_name,good_spec,shop_price,goods_img FROM goods WHERE goods_id=%s;",id_buf);
                char *errmsg=NULL;
                //查询单条商品，结果写到html_buf
                ret = sqlite3_exec(db, sql, detail_cb, html_buf, &errmsg);
                sqlite3_free(errmsg);
                sqlite3_close(db);

                //查无数据
                if(strlen(html_buf)==0)
                {
                    strcpy(html_buf,"<html><body><h2>该商品不存在</h2><a href='/home.html'>返回</a></body></html>");
                }
            }

            //发送动态生成的详情html给浏览器
            char header[512]={0};
            snprintf(header,sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type:text/html;charset=utf-8\r\n"
                "Content-Length:%d\r\n"
                "Connection:close\r\n\r\n",
                (int)strlen(html_buf));

            send(connfd, header, strlen(header),0);
            send(connfd, html_buf, strlen(html_buf),0);

            close(connfd);
            continue;
        }
            //====温度传感器 temp_sensor.html====
        else if(strcmp(url,"/temp_sensor.html")==0)
        {
            sqlite3 *db;
            char *err=0;
            char sql[512];
            sprintf(sql,"select goods_id,goods_img from goods where goods_name like '%%%%MCP9700AT‑%%' OR goods_name like '%%DS18B20(UMW)%%'");
            sqlite3_open("component.db",&db);
            char buf[2048]={0};
            sprintf(buf,"<html><head><meta charset='utf-8'><title>温度传感器</title></head><body><h2>温度传感器</h2><a href='/home.html'>返回首页</a><br>");
            sqlite3_exec(db,sql,write_buf_html,buf,&err);
            strcat(buf,"</body></html>");
            sqlite3_close(db);
            char h[512];
            sprintf(h,"HTTP/1.1 200 OK\r\nContent-Type:text/html;charset=utf-8\r\nContent-Length:%d\r\n\r\n",(int)strlen(buf));
            send(connfd,h,strlen(h),0);
            send(connfd,buf,strlen(buf),0);
            close(connfd);
            continue;
        }
            //====运算放大器 op_amp.html====
        else if(strcmp(url,"/op_amp.html")==0)
        {
            sqlite3 *db;
            char *err=0;
            char sql[512];
            sprintf(sql,"select goods_id,goods_img from goods where goods_name like '%%LM358DR%%' OR goods_name like '%%MCP6002T‑I%%'");
            sqlite3_open("component.db",&db);
            char buf[2048]={0};
            sprintf(buf,"<html><head><meta charset='utf-8'><title>运算放大器</title></head><body><h2>运算放大器</h2><a href='/home.html'>返回首页</a><br>");
            sqlite3_exec(db,sql,write_buf_html,buf,&err);
            strcat(buf,"</body></html>");
            sqlite3_close(db);
            char h[512];
            sprintf(h,"HTTP/1.1 200 OK\r\nContent-Type:text/html;charset=utf-8\r\nContent-Length:%d\r\n\r\n",(int)strlen(buf));
            send(connfd,h,strlen(h),0);
            send(connfd,buf,strlen(buf),0);
            close(connfd);
            continue;
        }

        else
        {
            printf("未知url:%s\n",url);
            send_file(connfd,"login.html");
        }
        close(connfd);
    }
    close(sockfd);
    return 0;
}
