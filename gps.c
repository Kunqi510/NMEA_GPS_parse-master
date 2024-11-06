#include "gps.h"

static GPS gps_all;
static char gps_src_backup[1024];

// 数据分割，可以分割两个连续的分隔符
static char* strsplit(char** stringp, const char* delim)
{
    char* start = *stringp;
    char* p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
    {
        *stringp = NULL;
    }
    else
    {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

// 统计字符串在另一个字符串中出现的次数
static int strstr_cnt(char *str, char *substr)
{
    char *srcStr = str;
    int count = 0;

    do
    {
        srcStr = strstr(srcStr, substr);
        if(srcStr != NULL)
        {
            count++;
            srcStr = srcStr + strlen(substr);
        }
        else
        {
            break;
        }
    }while (*srcStr != '\0');

    return count;
}

#if ENABLE_GGA
// GGA数据解析
static GGA gga_data_parse(char *gga_data)
{
    GGA gga;
    unsigned char times = 0;
    char *p;
    char end[16];

    p = strsplit(&gga_data, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // UTC
                strcpy(gga.utc, p);
                break;
            case 2:   // lat
                gga.lat = strtod(p, NULL);
                break;
            case 3:   // lat dir
                gga.lat_dir = p[0];
                break;
            case 4:   // lon
                gga.lon = strtod(p, NULL);
                break;
            case 5:   // lon dir
                gga.lon_dir = p[0];
                break;
            case 6:   // quality
                gga.quality = (unsigned char)strtol(p, NULL, 10);
                break;
            case 7:   // sats
                gga.sats = (unsigned char)strtol(p, NULL, 10);
                break;
            case 8:   // hdop
                gga.hdop = strtod(p, NULL);;
                break;
            case 9:   // alt
                gga.alt = strtod(p, NULL);
                break;
            case 11:  // undulation
                gga.undulation = strtod(p, NULL);
                break;
            case 13:  // age
                gga.age = (unsigned char)strtol(p, NULL, 10);
                break;
            case 14:  // stn_ID
                strncpy(end, p, strlen(p)-3);
                end[strlen(p)-3] = '\0';
                gga.stn_ID = (unsigned short )strtol(end, NULL, 10);
                break;
            default:
                break;
        }
        p = strsplit(&gga_data, ",");
        times++;
    }
    return gga;
}
#endif

#if ENABLE_GLL
// GLL数据解析
static GLL gll_data_parse(char *gll_data)
{
    GLL gll;
    unsigned char times = 0;
    char *p;
    char *s = gll_data;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // lat
                gll.lat = strtod(p, NULL);
                break;
            case 2:   // lat dir
                gll.lat_dir = p[0];
                break;
            case 3:   // lon
                gll.lon = strtod(p, NULL);
                break;
            case 4:   // lon dir
                gll.lon_dir = p[0];
                break;
            case 5:   // lon dir
                strcpy(gll.utc, p);
                break;
            case 6:  // data status
                gll.data_status = p[0];
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    return gll;
}
#endif

#if ENABLE_GSA
// 得到GSA数据中的信道信息
static void get_prn_data(char *gps_data, GSA_PRN *gsa_prn)
{
    unsigned char times = 0;
    unsigned char i;
    unsigned char sentences_index = 0;  // 累计找到gsa字段的个数
    char *p;
    char gsa_data_buffer[128];
    char *s = gsa_data_buffer;
    char *sentences;
    int gsa_count;

    // 统计GSA字段的个数
    gsa_count = strstr_cnt(gps_data, PRE_GSA);

    (void)memset(gsa_prn, 0, sizeof(GSA_PRN) * MAX_GSA_CHANNELS);
    sentences = strtok(gps_data, "\r\n");
    while (sentences)
    {
        if (strstr(sentences, PRE_GSA))
        {
            sentences_index++;
            s = gsa_data_buffer;
            (void)memcpy(s, sentences, strlen(sentences));
            p = strsplit(&s, ",");
            while (p)
            {
                if (times == 2)
                {
                    for (i=0; i<12; i++)
                    {
                        p = strsplit(&s, ",");
                        (gsa_prn+i+(sentences_index-1)*12)->total = (unsigned char)gsa_count * 12;
                        (gsa_prn+i+(sentences_index-1)*12)->prn_ID = i + (sentences_index - 1) * 12;
                        (gsa_prn+i+(sentences_index-1)*12)->prn = (unsigned char)strtol(p, NULL, 10);
                    }
                }
                p = strsplit(&s, ",");
                times++;
            }
            times = 0;
        }
        sentences = strtok(NULL, "\r\n");
    }
}

// GSA数据解析
static GSA gsa_data_parse(char *gsa_data, char *gpsdata)
{
    GSA gsa;
    unsigned char times = 0;
    char *p;
    char end[16];
    char *s = gsa_data;
    char *alldata = gpsdata;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // mode_MA
                gsa.mode_MA = p[0];
                break;
            case 2:   // mode_123
                gsa.mode_123 = p[0];
                break;
            case 3:   // prn
                get_prn_data(alldata, gsa.gsa_prn);
                break;
            case 15:  // pdop
                gsa.pdop = strtod(p, NULL);
                break;
            case 16:  // hdop
                gsa.hdop = strtod(p, NULL);
                break;
            case 17:  // vdop
                strncpy(end, p, strlen(p)-3);
                end[strlen(p)-3] = '\0';
                gsa.vdop = strtod(end, NULL);
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }

    return gsa;
}
#endif

#if ENABLE_RMC
// RMC数据解析
static RMC rmc_data_parse(char *rmc_data)
{
    RMC rmc;
    unsigned char times = 0;
    char *p;
    char *s = rmc_data;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // UTC
                strcpy(rmc.utc, p);
                break;
            case 2:   // pos status
                rmc.pos_status = p[0];
                break;
            case 3:   // lat
                rmc.lat = strtod(p, NULL);
                break;
            case 4:   // lat dir
                rmc.lat_dir = p[0];
                break;
            case 5:   // lon
                rmc.lon = strtod(p, NULL);
                break;
            case 6:   // lon dir
                rmc.lon_dir = p[0];
                break;
            case 7:   // speen Kn
                rmc.speed_Kn = strtod(p, NULL);
                break;
            case 8:   // track true
                rmc.track_true = strtod(p, NULL);
                break;
            case 9:   // date
                strcpy(rmc.date, p);
                break;
            case 10:  // mag var
                rmc.mag_var = strtod(p, NULL);
                break;
            case 11:  // var dir
                rmc.var_dir = p[0];
                break;
            case 12:  // mode ind
                rmc.mode_ind = p[0];
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }

    return rmc;
}
#endif

#if ENABLE_ROT
static ROT rot_data_parse(char *rot_data)
{
    ROT rot;
    unsigned char times = 0;
    char *p;
    char *s = rot_data;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // Rate of Turn
                rot.rate_of_turn = strtod(p, NULL);
                break;
            case 2:
                rot.status = p[0];
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }
    return rot;
}
#endif

#if ENABLE_VTG
// VTG数据解析
static VTG vtg_data_parse(char *vtg_data)
{
    VTG vtg;
    unsigned char times = 0;
    char *p;
    char *s = vtg_data;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // track true
                vtg.track_true = strtod(p, NULL);
                break;
            case 3:   // track mag
                vtg.track_mag = strtod(p, NULL);
                break;
            case 5:   // speed Kn
                vtg.speed_Kn = strtod(p, NULL);
                break;
            case 7:   // speed Km
                vtg.speed_Km = strtod(p, NULL);
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }

    return vtg;
}
#endif

#if ENABLE_GSV
/*
 * function:  获取GSV字段中的GPS信息
 * gps_data:  最原始的GPS字符串
 * stas:      卫星数量
 * prefix:    GSV信息字段前缀
 * sats_info: 存放卫星信息
*/
static void get_sats_info(char *gps_data, unsigned char sats, char *prefix, SAT_INFO *sats_info)
{
    unsigned char times = 0;
    unsigned char msgs = 0;
    unsigned char msg = 0;
    unsigned char for_times;
    unsigned char i;
    char *p;
    char gsv_data_buffer[128];
    char *s = gsv_data_buffer;
    char *sentences;

    (void)memset(sats_info, 0, sizeof(SAT_INFO) * (sats+1));
    sentences = strtok(gps_data, "\r\n");
    while (sentences)
    {
        if (strstr(sentences, prefix))
        {
            s = gsv_data_buffer;
            (void)memcpy(s, sentences, strlen(sentences));
            p = strsplit(&s, ",");
            while (p)
            {
                switch (times)
                {
                    case 1:   // msgs
                        msgs = (unsigned char) strtol(p, NULL, 10);
                        break;
                    case 2:   // msg
                        msg = (unsigned char) strtol(p, NULL, 10);
                        break;
                    case 3:   // sat info
                        for_times = (msgs == msg) ? ((sats % 4) ? sats % 4 : 4) : 4;
                        for (i = 0; i < for_times; i++)
                        {
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->prn = (unsigned char) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->elev = (unsigned char) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->azimuth = (unsigned short) strtol(p, NULL, 10);
                            p = strsplit(&s, ",");
                            (sats_info+(msg-1)*4+i)->SNR = (unsigned char) strtol(p, NULL, 10);
                        }
                        break;
                    default:
                        break;
                }
                p = strsplit(&s, ",");
                times++;
            }
            times = 0;
        }
        sentences = strtok(NULL, "\r\n");
    }
}

// GSV数据解析
static GSV gsv_data_parse(char *gsv_data, char *gps_data, char *prefix)
{
    GSV gsv;
    unsigned char times = 0;
    char *p;
    char *s = gsv_data;
    char *src_data = gps_data;

    p = strsplit(&s, ",");
    while (p)
    {
        switch (times)
        {
            case 1:   // msgs
                gsv.msgs = (unsigned char)strtol(p, NULL, 10);
                break;
            case 2:   // msg
                gsv.msg = (unsigned char)strtol(p, NULL, 10);
                break;
            case 3:   // sats
                gsv.sats = (unsigned char)strtol(p, NULL, 10);
                get_sats_info(src_data, gsv.sats, prefix, gsv.sat_info);
                break;
            default:
                break;
        }
        p = strsplit(&s, ",");
        times++;
    }

    return gsv;
}
#endif

#if ENABLE_UTC
// UTC数据解析
static UTC utc_parse(char *date, char *time)
{
    UTC utc_data;
    unsigned int date_int;
    double time_float;

    date_int = (unsigned int)strtol(date, NULL, 10);
    utc_data.DD = date_int / 10000;
    utc_data.MM = date_int % 10000 / 100;
    utc_data.YY = date_int % 100;
    time_float = strtod(time, NULL);
    utc_data.hh = (unsigned int)time_float / 10000;
    utc_data.mm = (unsigned int)time_float % 10000 / 100;
    utc_data.ss = (unsigned int)time_float % 100;
    utc_data.ds = (unsigned short)(time_float - (unsigned int)time_float);

    return utc_data;
}
#endif

// 解析全部的GPS数据
GPS gps_data_parse(const char* gps_src, const unsigned short gps_src_len)
{
    // GGA数据解析
#if ENABLE_GGA
    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    GGA default_gga_data = {"\0",0.0,'N',0.0,'S',0,0,0,0,0,0,0};
    gps_all.gga_data = strstr(gps_src, PRE_GGA) ? gga_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GGA), "\r\n")) : default_gga_data;
#endif

#if ENABLE_ROT
    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    ROT default_rot_data = {0.0,0.0};
    gps_all.rot_data = strstr(gps_src, PRE_ROT) ? rot_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_ROT), "\r\n")) : default_rot_data;
#endif

    // GLL数据解析
#if ENABLE_GLL
    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    GLL default_gll_data = {0.0,'\0',0.0,'\0',"\0",'\0'};
    gps_all.gll_data = strstr(gps_src, PRE_GLL) ? gll_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GLL), "\r\n")) : default_gll_data;
#endif

    // GSA数据解析
#if ENABLE_GSA
    GSA_PRN default_gsa_prn_data = {0,0,0};
    GSA default_gsa_data = {'\0','\0',0.0,0.0,0.0, default_gsa_prn_data};

    (void)memcpy(gps_src_backup, gps_src, gps_src_len);
    gps_all.gsa_data = strstr(gps_src, PRE_GSA) ? gsa_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GSA), "\r\n"), gps_src_backup) : default_gsa_data;
#endif

    // RMC数据解析
#if ENABLE_RMC
    RMC default_rmc_data = {"\0",'\0',0.0,'\0',0.0,'\0',0.0,0.0,"\0",0.0,'\0','\0'};

    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    gps_all.rmc_data = strstr(gps_src, PRE_RMC) ? rmc_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_RMC), "\r\n")) : default_rmc_data;
#endif

    // VTG数据解析
#if ENABLE_VTG
    VTG default_vtg_data = {0.0,0.0,0.0,0.0};

    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    gps_all.vtg_data = strstr(gps_src, PRE_VTG) ? vtg_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_VTG), "\r\n")) : default_vtg_data;
#endif

    // GSV数据解析
#if ENABLE_GSV
    SAT_INFO default_sat_info_data = {0,0,0,0};
    GSV default_gsv_data = {0, 0, 0, default_sat_info_data};

    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    (void)memcpy(gps_src_backup, gps_src, gps_src_len);
    // GPGSV数据段解析
    gps_all.gpgsv_data = strstr(gps_src, PRE_GPGSV) ? gsv_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GPGSV), "\r\n"), gps_src_backup, PRE_GPGSV) : default_gsv_data;
    // GNGSV数据段解析
    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    (void)memcpy(gps_src_backup, gps_src, gps_src_len);
    gps_all.gngsv_data = strstr(gps_src, PRE_GNGSV) ? gsv_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GNGSV), "\r\n"), gps_src_backup, PRE_GNGSV) : default_gsv_data;
    // GLGSV数据段解析
    (void)memset(gps_all.gps_buffer_data, 0u, sizeof(gps_all.gps_buffer_data));
    (void)memcpy(gps_all.gps_buffer_data, gps_src, gps_src_len);
    (void)memcpy(gps_src_backup, gps_src, gps_src_len);
    gps_all.glgsv_data = strstr(gps_src, PRE_GLGSV) ? gsv_data_parse(strtok(strstr(gps_all.gps_buffer_data, PRE_GLGSV), "\r\n"), gps_src_backup, PRE_GLGSV) : default_gsv_data;
#endif

    // UTC数据解析，UTC数据取自RMC段数据
#if ENABLE_UTC && ENABLE_RMC
    gps_all.utc = utc_parse(gps_all.rmc_data.date, gps_all.rmc_data.utc);
#endif

    return gps_all;
}

int main()
{
    GPS gps;
    unsigned char i;
    char gps_data[] = "$GNRMC,013300.00,A,2240.84105,N,11402.70763,E,0.007,,220319,,,D*69\r\n"
                        "$GNVTG,,T,,M,0.007,N,0.014,K,D*3A\r\n"
                        "$GNROT,,V*18\r\n"
                        "$GNGGA,013300.00,2240.84105,N,11402.70763,E,2,12,0.59,70.5,M,-2.5,M,,0000*68\r\n"
                        "$GNGSA,A,3,10,12,14,20,25,31,32,26,29,40,41,22,1.09,0.59,0.91*1F\r\n"
                        "$GNGSA,A,3,74,70,73,80,69,,,,,,,,1.09,0.59,0.91*17\r\n"
                        "$GPGSV,4,1,16,01,00,300,,10,56,178,51,12,12,038,38,14,47,345,48*79\r\n"
                        "$GPGSV,4,2,16,16,00,207,,18,06,275,30,20,28,165,43,22,10,319,43*76\r\n"
                        "$GPGSV,4,3,16,25,46,050,47,26,29,205,44,29,13,108,45,31,50,296,52*7E\r\n"
                        "$GPGSV,4,4,16,32,56,010,52,40,20,257,40,41,46,237,48,42,46,123,42*77\r\n"
                        "$GLGSV,2,1,06,69,27,136,49,70,76,057,50,71,34,338,50,73,64,276,55*6B\r\n"
                        "$GLGSV,2,2,06,74,24,231,46,80,35,019,46*60\r\n"
                        "$GNGLL,2240.84105,N,11402.70763,E,013300.00,A,D*7C\r\n";
    gps = gps_data_parse(gps_data, sizeof(gps_data));

#if ENABLE_GGA
    printf("----------GGA DATA----------\n");
    printf("utc:%s\n", gps.gga_data.utc);
    printf("lat:%f\n", gps.gga_data.lat);
    printf("lat_dir:%c\n", gps.gga_data.lat_dir);
    printf("lon:%f\n", gps.gga_data.lon);
    printf("lon_dir:%c\n", gps.gga_data.lon_dir);
    printf("quality:%d\n", gps.gga_data.quality);
    printf("sats:%d\n", gps.gga_data.sats);
    printf("hdop:%f\n", gps.gga_data.hdop);
    printf("alt:%f\n", gps.gga_data.alt);
    printf("undulation:%f\n", gps.gga_data.undulation);
    printf("age:%d\n", gps.gga_data.age);
    printf("stn_ID:%d\n", gps.gga_data.stn_ID);
#endif

#if ENABLE_ROT
    printf("----------ROT DATA----------\n");
    printf("rate:%f\n", gps.rot_data.rate_of_turn);
    printf("status:%c\n", gps.rot_data.status);
#endif

#if ENABLE_GLL
    printf("----------GLL DATA----------\n");
    printf("utc:%s\n", gps.gll_data.utc);
    printf("lat:%f\n", gps.gll_data.lat);
    printf("lat_dir:%c\n", gps.gll_data.lat_dir);
    printf("lon:%f\n", gps.gll_data.lon);
    printf("lon_dir:%c\n", gps.gll_data.lon_dir);
    printf("data_status:%c\n", gps.gll_data.data_status);
#endif
#if ENABLE_GSA
    printf("----------GSA DATA----------\n");
    printf("mode_MA:%c\n", gps.gsa_data.mode_MA);
    printf("mode_123:%c\n", gps.gsa_data.mode_123);
    printf("total:%d\n", gps.gsa_data.gsa_prn[0].total);
    for (i=0; i<gps.gsa_data.gsa_prn[0].total; i++)
    {
        printf("prn%d:%d\n", (i+1), gps.gsa_data.gsa_prn[i].prn);
    }
    printf("pdop:%f\n", gps.gsa_data.pdop);
    printf("hdop:%f\n", gps.gsa_data.hdop);
    printf("vdop:%f\n", gps.gsa_data.vdop);
#endif
#if ENABLE_RMC
    printf("----------RMC DATA----------\n");
    printf("utc:%s\n", gps.rmc_data.utc);
    printf("pos_status:%c\n", gps.rmc_data.pos_status);
    printf("lat:%f\n", gps.rmc_data.lat);
    printf("lat_dir:%c\n", gps.rmc_data.lat_dir);
    printf("lon:%f\n", gps.rmc_data.lon);
    printf("lon_dir:%c\n", gps.rmc_data.lon_dir);
    printf("speed_Kn:%f\n", gps.rmc_data.speed_Kn);
    printf("track_true:%f\n", gps.rmc_data.track_true);
    printf("date:%s\n", gps.rmc_data.date);
    printf("mag_var:%f\n", gps.rmc_data.mag_var);
    printf("var_dir:%c\n", gps.rmc_data.var_dir);
    printf("mode_ind:%c\n", gps.rmc_data.mode_ind);
#endif
#if ENABLE_VTG
    printf("----------VTG DATA----------\n");
    printf("track_true:%f\n", gps.vtg_data.track_true);
    printf("track_mag:%f\n", gps.vtg_data.track_mag);
    printf("speen_Kn:%f\n", gps.vtg_data.speed_Kn);
    printf("speed_Km:%f\n", gps.vtg_data.speed_Km);
#endif
#if ENABLE_GSV
    printf("----------GPGSV DATA----------\n");
    printf("msgs:%d\n", gps.gpgsv_data.msgs);
    printf("msg:%d\n", gps.gpgsv_data.msg);
    printf("sats:%d\n", gps.gpgsv_data.sats);
    for (i=0;i<gps.gpgsv_data.sats; i++)
    {
        printf("prn%d:%d\n", i+1, gps.gpgsv_data.sat_info[i].prn);
        printf("evel%d:%d\n", i+1, gps.gpgsv_data.sat_info[i].elev);
        printf("azimuth%d:%d\n", i+1, gps.gpgsv_data.sat_info[i].azimuth);
        printf("SNR%d:%d\n", i+1, gps.gpgsv_data.sat_info[i].SNR);
    }

    printf("----------GNGSV DATA----------\n");
    printf("msgs:%d\n", gps.gngsv_data.msgs);
    printf("msg:%d\n", gps.gngsv_data.msg);
    printf("sats:%d\n", gps.gngsv_data.sats);
    for (i=0; i<gps.gngsv_data.sats; i++)
    {
        printf("prn%d:%d\n", i+1, gps.gngsv_data.sat_info[i].prn);
        printf("evel%d:%d\n", i+1, gps.gngsv_data.sat_info[i].elev);
        printf("azimuth%d:%d\n", i+1, gps.gngsv_data.sat_info[i].azimuth);
        printf("SNR%d:%d\n", i+1, gps.gngsv_data.sat_info[i].SNR);
    }

    printf("----------GLGSV DATA----------\n");
    printf("msgs:%d\n", gps.glgsv_data.msgs);
    printf("msg:%d\n", gps.glgsv_data.msg);
    printf("sats:%d\n", gps.glgsv_data.sats);
    for (i=0;i<gps.glgsv_data.sats; i++)
    {
        printf("prn%d:%d\n", i+1, gps.glgsv_data.sat_info[i].prn);
        printf("evel%d:%d\n", i+1, gps.glgsv_data.sat_info[i].elev);
        printf("azimuth%d:%d\n", i+1, gps.glgsv_data.sat_info[i].azimuth);
        printf("SNR%d:%d\n", i+1, gps.glgsv_data.sat_info[i].SNR);
    }
#endif
#if ENABLE_UTC && ENABLE_RMC
    printf("----------UTC DATA----------\n");
    printf("year:20%02d\n", gps.utc.YY);
    printf("month:%02d\n", gps.utc.MM);
    printf("date:%02d\n", gps.utc.DD);
    printf("hour:%02d\n", gps.utc.hh);
    printf("minutes:%02d\n", gps.utc.mm);
    printf("second:%02d\n", gps.utc.ss);
    printf("ds:%02d\n", gps.utc.ds);
#endif

return 0;
}
