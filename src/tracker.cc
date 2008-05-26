#include <stdio.h>
#include <time.h>
#include <queue>
#include <list>
#include <winsock.h>
#include <assert.h>
#include "fhandle.h"
#include "fsocket.h"
#include "upeer.h"
#include "netmgr.h"
#include "session.h"
#include "peer.h"
#include "tracke.h"
#include "listener.h"

char *get_info_hash();
char *get_peer_id();
int add_announce(const char *value);

static const char *HTTP_HEADER = {
    "GET /%s?info_hash=%s&peer_id=%s&port=%u&uploaded=%u&downloaded=%u"
    "&left=%u%s&compact=1 HTTP/1.0\r\n"
    "Connection: close\r\n\r\n"
};

class HttpURL  {
public:
    HttpURL(char *Url);
    ~HttpURL();
    int IsBadUrl();
    int GetUrlPort();
    const char *GetUrlHost();
    const char *GetUrlPath();
private:
    int urlBad;
    int urlPort;
    char *urlContext;
    const char *urlHost;
    const char *urlPath;
};

HttpURL::HttpURL(char *url)
{
    char *path;
    urlBad = 1;
    urlPort = 80;
    urlHost = NULL;
    urlPath = NULL;
    urlContext = NULL;
    if (strncmp(url, "http://", 7) != 0) {
        return;
    }
    urlContext = strdup(url);
    urlHost = urlContext + 7;
    path = strstr(urlHost, "/");
    if (path != NULL) {
        *path++ = 0;
        urlPath = path;
    }
    path = strstr(urlHost, ":");
    if (path != NULL) {
        *path++ = 0;
        urlPort = atoi(path);
    }
}

HttpURL::~HttpURL()
{
    free(urlContext);
}

int HttpURL::IsBadUrl()
{
    return urlBad;
}

int HttpURL::GetUrlPort()
{
    return urlPort;
}

const char *HttpURL::GetUrlHost()
{
    return urlHost;
}

const char *HttpURL::GetUrlPath()
{
    return urlPath;
}

class TrackeProcess:public fHandle  {
public:
    TrackeProcess(char *url);
    virtual ~ TrackeProcess();
    int Abort();
    int RunActive();
private:
    int retrys;
    fSocket file;
    int nextTime;
    int state;
    int offset;
    int total;
    int local_size;
    char *local_buff;
    HttpURL announce;
    char *redirectURL;
    char buffer[8192];
};

TrackeProcess::TrackeProcess(char *url):announce(url)
{
    state = 0;
    retrys = 0;
    local_size = 4096;
    local_buff = (char *) malloc(local_size);
    nextTime = time(NULL);
    redirectURL = NULL;
}

TrackeProcess::~TrackeProcess()
{
    free(local_buff);
}

int TrackeProcess::Abort()
{

    printf("Tracke::Abort: \n");
    state = 5;
    Wakeup();
    return 0;
}

int get_left();
int get_uploaded();
int get_downloaded();

static const char *get_event(int retry)
{
    static const char *_[]={"&event=started", ""};
    return _[retry>0];
}

int TrackeProcess::RunActive()
{
    int error = 0;
    int nextState = state;
    sockaddr_in inaddr;

    /* attention static should move to class member! */
    while (error != -1) {
        state = nextState++;
        switch (state) {
        case 0:
            fprintf(stderr, "tracker start: %s %d!\n", announce.GetUrlHost(),
                   announce.GetUrlPort());
            file.setBlockopt(0);
            inaddr.sin_family = AF_INET;
            inaddr.sin_port = htons(announce.GetUrlPort());
            if (inet_addr(announce.GetUrlHost()) != INADDR_NONE) {
                inaddr.sin_addr.s_addr = inet_addr(announce.GetUrlHost());
            } else {
                hostent * phost = gethostbyname(announce.GetUrlHost());
                if (phost != NULL) {
                    inaddr.sin_addr.s_addr =
                        *(unsigned long *) phost->h_addr;
                    fprintf(stderr, "address: %s\n", inet_ntoa(inaddr.sin_addr));
                }
            }
            nextTime += 30;
            file.Connect((sockaddr *) & inaddr, sizeof(inaddr));
            break;
        case 1:
            total = 0;
            offset = 0;
#if 0
            "GET /%s?info_hash=%s&peer_id=%s&port=%d&uploaded=%d&downloaded=%d"
            "&left=%d%s&compact=1 HTTP/1.0\r\n"
#endif
            sprintf(buffer, HTTP_HEADER, announce.GetUrlPath(),
                    get_info_hash(), get_peer_id(), getListenPort(),
                    get_uploaded(), get_downloaded(), get_left(), get_event(retrys));
            error = file.Connected();
            break;
        case 2:
            error = 0;
            while (error != -1 && buffer[offset + error]) {
                offset += error;
                error =
                    file.Send(buffer + offset, strlen(buffer + offset), 0);
            }
            printf("Tracker Requesting: %s\n", buffer);
            break;
        case 3:
            error = 0;

            do {
                total += error;
                if (total + 1 >= local_size) {
                    local_buff =
                        (char *) realloc(local_buff,
                                         local_size * 2 + 4096);
                    assert(local_buff != NULL);
                    local_size = 2 * local_size + 4096;
                }
                error =
                    file.Recv(local_buff + total, local_size - total, 0);
            }
            while (error > 0);
            break;
        case 4:
            local_buff[total] = 0;
            if (strncmp("HTTP/1.1 200", local_buff, 12) == 0
                    ||strncmp("HTTP/1.0 200", local_buff, 12) == 0) {
                char *start = strstr(local_buff, "\r\n\r\n");
                if (start != NULL) {
                    int interval = LoadTorrentPeer(start + 4,
                                                   local_buff + total - start - 4);
                    int time_add = (interval>45?interval:45);
                    if (time_add > 200) {
                        time_add /= 5;
                    }
#if 1
                    if (get_comunicate_count() > 10){
                           nextTime += time_add;
                    }
                    if (interval == -1) {
                        file.Close();
                        free(redirectURL);
                        redirectURL = NULL;
                        Abort();
                        return 0;
                    }
#endif
                }
            } else if (strncmp("HTTP/1.0 302", local_buff, 12) == 0
                       ||strncmp("HTTP/1.0 302", local_buff, 12) == 0) {
                char *location = strstr(local_buff, "Location: ");
                if (location != NULL) {
                    char *cr = strstr(location, "\r\n");
                    if (cr != NULL) {
                        *cr = 0;
                    }
                    redirectURL = strdup(location + 10);
                }
            }
            break;
        case 5:
            error = 0;
            file.Close();
            if (redirectURL == NULL) {
                nextState = 100;
            }
            break;
        case 6:
        {
            HttpURL redirect(redirectURL);
            file.setBlockopt(0);
            inaddr.sin_family = AF_INET;
            inaddr.sin_port = htons(redirect.GetUrlPort());
            if (inet_addr(redirect.GetUrlHost()) != INADDR_NONE) {
                inaddr.sin_addr.s_addr =
                    inet_addr(redirect.GetUrlHost());
            } else {
                hostent * phost =
                    gethostbyname(redirect.GetUrlHost());
                if (phost != NULL) {
                    inaddr.sin_addr.s_addr =
                        *(unsigned long *) phost->h_addr;
                }
            } error =
                file.Connect((sockaddr *) & inaddr, sizeof(inaddr));
            sprintf(buffer,
                    "GET /%s HTTP/1.0\r\nConnection: Close\r\n\r\n",
                    redirect.GetUrlPath());
        }
        break;
        case 7:
            total = 0;
            offset = 0;
            error = file.Connected();
            break;
        case 8:
            error = 0;
            while (error != -1 && buffer[offset + error]) {
                offset += error;
                error =
                    file.Send(buffer + offset, strlen(buffer + offset), 0);
            }
            break;
        case 9:
            error = 0;

            do {
                total += error;
                if (total + 1 >= local_size) {
                    local_buff =
                        (char *) realloc(local_buff,
                                         local_size * 2 + 4096);
                    assert(local_buff != NULL);
                    local_size = 2 * local_size + 4096;
                }
                error =
                    file.Recv(local_buff + total, local_size - total, 0);
            }
            while (error > 0);
            break;
        case 10:
            local_buff[total] = 0;
            if (strncmp("HTTP/1.1 200", local_buff, 12) == 0
                    ||strncmp("HTTP/1.0 200", local_buff, 12) == 0) {
                char *start = strstr(local_buff, "\r\n\r\n");
                if (start != NULL) {
                    int interval = LoadTorrentPeer(start + 4,
                                                   local_buff + total - start - 4);
                    int time_add = (interval>45?interval:45);
                    if (time_add > 200) {
                        time_add /= 5;
                    }
#if 1
                    if (get_comunicate_count()>10){ 
                        nextTime += time_add;
                    }
                    if (interval == -1) {
                        Abort();
                        file.Close();
                        free(redirectURL);
                        redirectURL = NULL;
                        return 0;
                    }
#endif
                }
            }
            file.Close();
            free(redirectURL);
            redirectURL = NULL;
            break;
        default:
            retrys ++;
            error = WaitTime(nextTime);
            nextState = 0;
            break;
        }
    }
    return error;
}
int get_announce(char *announce, int len);
int WakeupTracke()
{
    char buffer[8192];
    while (get_announce(buffer, 8192) != -1) {
        TrackeProcess * process = new TrackeProcess(buffer);
        process->Wakeup();
    }
    return 0;
}


