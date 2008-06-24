/* $Id$ */
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <string>
#include <fstream>
#include <queue>

#include "sha1.h"
#include "bthread.h"
#include "biothread.h"
#include "bdhtnet.h"
#include "btcodec.h"
#include "btracker.h"

#ifndef NDEBUG
#include "bsocket.h"
#endif


class bclock: public bthread
{
    public:
        bclock(const char *text, int second);
        virtual int bdocall(time_t timeout);
        ~bclock();

    private:
        int b_second;
        int last_time;
        std::string ident_text;
};

bclock::bclock(const char *text, int second):
    ident_text("bclock")
{
    b_second = second;
    last_time = time(NULL);
    ident_text = strdup(text);
}

bclock::~bclock()
{
}

int
bclock::bdocall(time_t timeout)
{
    time_t now;
    time(&now);
#if 0
    fprintf(stderr, "\rbcall(%s): %s", ident_text, ctime(&now));
#endif
    while(-1 != btime_wait(last_time+b_second)){
        last_time = time(NULL);
    }
    return -1;
}

int
bttracker_start(const char *url, const unsigned char info_hash[20], const unsigned char peer_id[20])
{
    int i;

    static const char *http_encode[256] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07", "%08", "%09", "%0a", "%0b", "%0c", "%0d", "%0e", "%0f", 
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17", "%18", "%19", "%1a", "%1b", "%1c", "%1d", "%1e", "%1f", 
        "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27", "%28", "%29", "%2a", "%2b", "%2c", "%2d", "%2e", "%2f", 
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "%3a", "%3b", "%3c", "%3d", "%3e", "%3f", 
        "%40", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", 
        "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "%5b", "%5c", "%5d", "%5e", "%5f", 
        "%60", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", 
        "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "%7b", "%7c", "%7d", "%7e", "%7f", 
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87", "%88", "%89", "%8a", "%8b", "%8c", "%8d", "%8e", "%8f", 
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97", "%98", "%99", "%9a", "%9b", "%9c", "%9d", "%9e", "%9f", 
        "%a0", "%a1", "%a2", "%a3", "%a4", "%a5", "%a6", "%a7", "%a8", "%a9", "%aa", "%ab", "%ac", "%ad", "%ae", "%af", 
        "%b0", "%b1", "%b2", "%b3", "%b4", "%b5", "%b6", "%b7", "%b8", "%b9", "%ba", "%bb", "%bc", "%bd", "%be", "%bf", 
        "%c0", "%c1", "%c2", "%c3", "%c4", "%c5", "%c6", "%c7", "%c8", "%c9", "%ca", "%cb", "%cc", "%cd", "%ce", "%cf", 
        "%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6", "%d7", "%d8", "%d9", "%da", "%db", "%dc", "%dd", "%de", "%df", 
        "%e0", "%e1", "%e2", "%e3", "%e4", "%e5", "%e6", "%e7", "%e8", "%e9", "%ea", "%eb", "%ec", "%ed", "%ee", "%ef", 
        "%f0", "%f1", "%f2", "%f3", "%f4", "%f5", "%f6", "%f7", "%f8", "%f9", "%fa", "%fb", "%fc", "%fd", "%fe", "%ff"
    };

    std::string bturl(url);
    if (-1 == bturl.find('?')){
        bturl += "?info_hash=";
    }else{
        bturl += "&info_hash=";
    }
    for (i=0; i<20; i++){
        bturl += http_encode[info_hash[i]];
    }
    bturl += "&peer_id=";
    for (i=0; i<20; i++){
        bturl += http_encode[peer_id[i]];
    }
    bturl += "&port=15180";
    bturl += "&uploaded=0";
    bturl += "&downloaded=0";
    bturl += "&left=732854576";
    bturl += "&event=started";
    bturl += "&key=1785265";
    bturl += "&compact=1";
    bturl += "&numwant=200";
    printf("T: %s\n", bturl.c_str());
    burlthread *get = new burlthread(bturl.c_str(), 30);
    get->bwakeup();
    return 0;
}

int
btseed_load(const char *buf, int len)
{
    int i;
    size_t eln;
    btcodec codec;
    unsigned char digest[20];
    codec.bload(buf, len);
    const char *info = codec.bget().bget("info").b_str(&eln);
    SHA1Hash(digest, (unsigned char*)info, eln);
    const char *list = codec.bget().bget("announce-list").b_str(&eln);
    if (list != NULL){
        const char *urlbuf = NULL;
        bentity& en = codec.bget().bget("announce-list");
        for (i=0;urlbuf=en.bget(i).bget(0).c_str(&eln);i++){
            std::string url(urlbuf, eln);
            bttracker_start(url.c_str(), digest, digest);
        }
    }else{
        const char *urlbuf = codec.bget().bget("announce").c_str(&eln);
        std::string url(urlbuf, eln);
        bttracker_start(url.c_str(), digest, digest);
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int i;
    std::queue<burlthread*> burlqueue;
    for (i=1; i<argc; i++){
        if (strncmp(argv[i], "http://", 7)==0){
            burlthread *get = new burlthread(argv[i], 30);
            burlqueue.push(get);
            get->bwakeup();
        }else{
            std::string btseed;
            std::ifstream ifs(argv[i], std::ios::in|std::ios::binary);
            std::copy(std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>(),
                    std::back_inserter(btseed));
            btseed_load(btseed.c_str(), btseed.size());
        }
    }
#if 0
    bdhtnet_node("208.72.193.175", 6881);
    bdhtnet_node("195.56.193.72",40148);
    bdhtnet_node("90.154.212.140",10383);
    bdhtnet_node("151.205.102.164",50590);
    bdhtnet_node("86.220.27.202",45454);
    bdhtnet_node("122.107.166.67",11597);
    bdhtnet_node("222.186.13.91",27781);
    bdhtnet_node("91.82.71.148",16844);
    bdhtnet_node("61.92.120.178",55677);
    bdhtnet_node("91.123.159.137",61986);
    bdhtnet_node("88.118.64.253",28996);
    bdhtnet_node("88.5.91.190",46440);
    bdhtnet_node("220.191.110.248",1071);
    bdhtnet_node("89.223.193.64",19949);
    bdhtnet_node("87.196.78.70",22385);
    bdhtnet_node("212.178.34.162",17324);
    bdhtnet_node("41.221.16.184",52222);
    bdhtnet_node("87.202.71.30",11008);
    bdhtnet_node("62.195.210.75",62559);
    bdhtnet_node("96.240.66.218",50771);
    bdhtnet_node("221.217.83.166",16001);
    bdhtnet_node("68.93.90.195",61615);
    bdhtnet_node("194.1.160.209",25724);
    bdhtnet_node("125.91.21.48",16001);
    bdhtnet_node("77.253.118.249",25112);
    bdhtnet_node("201.246.47.194",17266);
    bdhtnet_node("80.98.245.57",6881);
    bdhtnet_node("125.94.225.237",18351);
    bdhtnet_node("75.157.0.119",1201);
    bdhtnet_node("222.70.32.150",26039);
    bdhtnet_node("71.76.250.200",5805);
    bdhtnet_node("86.61.126.44",62060);
    bdhtnet_node("92.82.17.3",52596);
    bdhtnet_node("75.30.69.110",13053);
    bdhtnet_node("78.82.104.248",11584);
    bdhtnet_node("221.219.112.133",32107);
    bdhtnet_node("219.52.48.30",33344);
    bdhtnet_node("89.211.120.120",7731);
    bdhtnet_node("121.22.188.101",16001);
    bdhtnet_node("59.178.178.74",50678);
    bdhtnet_node("220.227.147.152",5251);
    bdhtnet_node("143.106.87.30",62953);
    bdhtnet_node("219.77.168.32",20367);
    bdhtnet_node("193.90.54.207",31947);
    bdhtnet_node("84.223.245.78",50778);
    bdhtnet_node("213.140.11.129",58072);
    bdhtnet_node("69.112.23.84",27720);
    bdhtnet_node("89.42.80.169",62611);
    bdhtnet_node("212.76.37.156",53979);
    bdhtnet_node("118.140.74.233",11441);
    bdhtnet_node("201.1.62.109",13249);
    bdhtnet_node("222.57.241.64",14091);
    bdhtnet_node("122.13.31.53",50947);
    bdhtnet_node("77.70.99.136",51067);
    bdhtnet_node("66.68.249.123",45682);
    bdhtnet_node("58.38.146.253",14333);
    bdhtnet_node("121.234.223.160",16001);
    bdhtnet_node("77.235.105.252",11931);
    bdhtnet_node("68.41.46.215",47277);
    bdhtnet_node("69.236.79.77",1046);
    bdhtnet_node("77.49.177.155",34396);
    bdhtnet_node("124.150.75.153",11782);
    bdhtnet_node("75.83.36.21",60359);
    bdhtnet_node("81.190.153.150",14788);
    bdhtnet_node("219.78.178.132",17849);
    bdhtnet_node("84.223.4.148",29664);
    bdhtnet_node("116.227.145.133",7445);
    bdhtnet_node("79.98.8.1",11346);
    bdhtnet_node("85.140.187.203",37745);
    bdhtnet_node("190.30.134.169",64813);
    bdhtnet_node("220.165.70.60",16001);
    bdhtnet_node("68.54.25.25",12559);
    bdhtnet_node("90.24.38.167",25266);
    bdhtnet_node("190.232.76.221",23014);
    bdhtnet_node("60.163.14.6",26987);
    bdhtnet_node("61.170.128.57",32384);
    bdhtnet_node("85.110.174.238",7241);
    bdhtnet_node("218.18.46.31",4911);
    bdhtnet_node("117.26.210.186",16001);
    bdhtnet_node("83.108.252.74",25302);
    bdhtnet_node("222.130.60.12",16001);
    bdhtnet_node("219.139.247.221",47777);
    bdhtnet_node("122.225.123.14",48118);
    bdhtnet_node("88.115.182.250",14508);
    bdhtnet_node("190.244.0.61",35902);
    bdhtnet_node("76.209.240.110",34531);
    bdhtnet_node("85.201.113.79",54282);
    bdhtnet_node("89.143.122.23",60357);
    bdhtnet_node("60.48.247.255",10088);
    bdhtnet_node("77.204.156.145",14832);
    bdhtnet_node("81.36.14.67",29356);
    bdhtnet_node("83.28.62.198",9307);
    bdhtnet_node("83.49.15.87",7138);
    bdhtnet_node("124.78.41.132",25461);
    bdhtnet_node("89.130.207.104",20348);
    bdhtnet_node("220.163.73.12",17930);
    bdhtnet_node("84.77.9.30",22222);
    bdhtnet_node("79.27.157.159",6881);
    bdhtnet_node("62.16.245.76",49221);
    bdhtnet_node("92.81.46.211",21063);
    bdhtnet_node("117.47.22.217",47137);
    bdhtnet_node("82.207.119.150",62543);
    bdhtnet_node("222.188.183.16",19144);
    bdhtnet_node("78.83.211.155",7647);
    bdhtnet_node("123.114.246.67",18930);
    bdhtnet_node("75.36.186.135",40923);
    bdhtnet_node("116.49.51.99",13376);
    bdhtnet_node("222.211.38.136",13701);
    bdhtnet_node("58.173.217.110",44349);
    bdhtnet_node("99.227.88.164",47802);
    bdhtnet_node("194.54.56.29",10799);
    bdhtnet_node("222.131.242.55",6881);
    bdhtnet_node("200.109.28.232",13919);
    bdhtnet_node("74.186.122.47",50466);
    bdhtnet_node("213.10.37.97",10300);
    bdhtnet_node("61.224.120.245",20367);
    bdhtnet_node("118.170.27.95",7184);
    bdhtnet_node("64.180.190.29",10655);
    bdhtnet_node("88.122.55.107",25322);
    bdhtnet_node("77.49.19.214",13499);
    bdhtnet_node("89.216.210.254",53);
    bdhtnet_node("85.195.54.42",57677);
    bdhtnet_node("78.101.251.4",21357);
    bdhtnet_node("219.77.126.126",11226);
    bdhtnet_node("24.168.31.87",60426);
    bdhtnet_node("24.30.11.86",59886);
    bdhtnet_node("87.203.220.79",18059);
    bdhtnet_node("80.99.112.145",55290);
    bdhtnet_node("125.60.241.154",37883);
    bdhtnet_node("222.126.99.13",25739);
    bdhtnet_node("68.45.182.140",62868);
    bdhtnet_node("116.21.96.142",17372);
    bdhtnet_node("93.80.238.88",20924);
    bdhtnet_node("87.23.30.251",16615);
    bdhtnet_node("217.159.213.122",23518);
    bdhtnet_node("92.241.235.29",55448);
    bdhtnet_node("68.225.43.201",61679);
    bdhtnet_node("68.149.54.185",55016);
    bdhtnet_node("86.126.24.229",24100);
    bdhtnet_node("77.46.166.73",63497);
    bdhtnet_node("85.127.97.2",39131);
    bdhtnet_node("121.193.136.220",15946);
    bdhtnet_node("88.146.133.215",24371);
    bdhtnet_node("77.76.130.229",22270);
    bdhtnet_node("217.17.254.119",11044);
    bdhtnet_node("84.23.119.132",7798);
    bdhtnet_node("202.138.136.130",63088);
    bdhtnet_node("84.60.54.3",62560);
    bdhtnet_node("62.194.24.72",52357);
    bdhtnet_node("200.171.228.125",50000);
    bdhtnet_node("92.11.169.158",21570);
    bdhtnet_node("84.124.220.180",32201);
    bdhtnet_node("71.191.61.253",50977);
    bdhtnet_node("77.108.97.241",2710);
    bdhtnet_node("67.204.14.110",18479);
    bdhtnet_node("84.238.197.91",10875);
    bdhtnet_node("218.162.102.240",6890);
    bdhtnet_node("59.120.76.230",24423);
    bdhtnet_node("85.156.91.114",9837);
    bdhtnet_node("221.194.213.225",10586);
    bdhtnet_node("77.105.218.104",50005);
    bdhtnet_node("85.231.192.203",25557);
    bdhtnet_node("218.162.193.214",20099);
    bdhtnet_node("201.208.120.122",58620);
    bdhtnet_node("201.68.37.203",49132);
    bdhtnet_node("83.20.226.9",3501);
    bdhtnet_node("84.126.1.183",16642);
    bdhtnet_node("189.32.110.71",61470);
    bdhtnet_node("89.212.35.113",29066);
    bdhtnet_node("71.50.152.33",7341);
    bdhtnet_node("218.68.149.106",15442);
    bdhtnet_node("62.201.64.82",63726);
    bdhtnet_node("62.57.103.245",24572);
    bdhtnet_node("116.14.66.93",13097);
    bdhtnet_node("85.59.180.84",24038);
    bdhtnet_node("61.223.99.63",51726);
    bdhtnet_node("68.62.123.157",18967);
    bdhtnet_node("74.64.30.9",26113);
    bdhtnet_node("84.90.45.37",56344);
    bdhtnet_node("121.206.149.123",16810);
    bdhtnet_node("90.190.162.170",55971);
    bdhtnet_node("81.88.114.200",64116);
    bdhtnet_node("60.47.133.54",32918);
    bdhtnet_node("85.155.248.74",49673);
    bdhtnet_node("217.52.112.130",62332);
    bdhtnet_node("79.206.122.41",27448);
    bdhtnet_node("89.223.193.64",19949);
#endif

    bclock c("SYS", 14), d("DDD", 19), k("UFO", 19), e("XDD", 17), f("ODD", 13);
    c.bwakeup(); d.bwakeup(); k.bwakeup(); e.bwakeup(); f.bwakeup();
    bthread *j;
    time_t timeout;
    while (-1 != bthread::bpoll(&j, &timeout)){
        bwait_cancel();
        if (-1 == j->bdocall(timeout)){
            j->bwait();
        }
    }
    while (!burlqueue.empty()){
        delete burlqueue.front();
        burlqueue.pop();
    }
    return 0;
}
