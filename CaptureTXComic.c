#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <json.h>
#include <curl/curl.h>

typedef unsigned char BYTE;
typedef char BOOL;
typedef char CHAR;
typedef int INT32;
typedef unsigned int UINT32;

#define CTXC_PRINT_COLOR_RESET "\33[0m"
#define CTXC_PRINT_COLOR_CYAN "\33[36m"
#define CTXC_PRINT_COLOR_RED "\33[31m"
#define CTXC_PRINT_COLOR_MAGENTA "\33[35m"
#define CTXC_RELEASE_PROGRAM 

#if defined(CTXC_RELEASE_PROGRAM)
#define CTXC_PRINT(COLOR, MODULE, FMT, ...)  \
    printf(COLOR); \
    printf(FMT, ## __VA_ARGS__); printf(CTXC_PRINT_COLOR_RESET);
#else
#define CTXC_PRINT(COLOR, MODULE, FMT, ...)  \
    printf(COLOR); printf("%s (%s, %d) : ", MODULE, __PRETTY_FUNCTION__, __LINE__); \
    printf(FMT, ## __VA_ARGS__); printf(CTXC_PRINT_COLOR_RESET);
#endif //#if defined(CTXC_RELEASE_PROGRAM)

#define DEB_CAPTURETXCOMIC_MODULE "CaptureTXComic.c"
#define DEB_CAPTURETXCOMIC_ENABEL
#ifdef DEB_CAPTURETXCOMIC_ENABEL
#define DEB_CAPTURETXCOMIC_PRINT(ARGS...) CTXC_PRINT(CTXC_PRINT_COLOR_CYAN, DEB_CAPTURETXCOMIC_MODULE, ARGS)
#else
#define DEB_CAPTURETXCOMIC_PRINT(ARGS...)
#endif //#ifdef DEB_CAPTURETXCOMIC_ENABEL

#define URL_JSON_COMICINFO ".ComicInfo.json"
#define URL_JSON_CHAPTERLIST ".ChapterList.json"
#define URL_JSON_PICTUREHASH ".PictureHash.json"
#define URL_CACHE_SIZE 1024
#define URL_COMIC_INFO "http://m.ac.qq.com/GetData/getComicInfo?id=%d"
#define URL_COMIC_CHAPTER "http://m.ac.qq.com/GetData/getChapterList?id=%d"
#define URL_USER_AGENT "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:36.0) Gecko/20100101 Firefox/36.0"
#define URL_COOKIE_INFO "Cookie: ac_refer=http://m.ac.qq.com/Comic/comicInfo/id/%d"
#define URL_REQUEST_REFERER "http://m.ac.qq.com/Comic/view/id/%d/cid/%d"
#define URL_PICTURE_HASH "http://m.ac.qq.com/View/mGetPicHash?id=%d&cid=%d"
#define URL_PICTURE_DOWNLOAD "http://ac.tc.qq.com/store_file_download?buid=15017&uin=%d&dir_path=%s&name=%s"
#define __USE_IP_PROXY__
#define __USE_FORK_PROCESS__
//#if defined(__USE_IP_PROXY__)
//#define URL_IP_PROXY "114.112.91.97:90"
//#endif // #if defined(__USE_IP_PROXY__)

#define CURL_PARAM_VERBOSE 0L // Display Curl log, 1 to on, 0 to off 
#define NAME_DOWNLOAD_LIST ".DownloadList.txt"
#define NAME_COMPLETE_FOLDER "完成"
#define NAME_WHOLE_COMIC ".ComicInfo.txt"
#define NAME_STRING_ON  "ON"
#define NAME_STRING_OFF "OFF"

static BYTE sm_cCurlLogOption = CURL_PARAM_VERBOSE;
static BYTE sm_cChapterFolderOption = 0;
static INT32 sm_nComicPictureId = 0;
static CHAR sm_cTitleName[128];
static CHAR sm_cIPProxy[32]="NONE";
static CHAR sm_cDownloadComplete = 0xff;

static INT32 si_ctxcIsComicExist(CHAR *pcComicTitle);

static size_t Si_ctxcWriteDataFile(void * pvBuffer, size_t nSize, size_t nMemb, void *pvStream)
{
  size_t nWritten = fwrite(pvBuffer, nSize, nMemb, (FILE *)pvStream);
  return nWritten;
}

static void si_ctxcGenerateComicUrl(CHAR *pcString, INT32 nLength, INT32 nID)
{
    if(pcString != NULL)
        snprintf(pcString, nLength,  URL_COMIC_INFO, nID);
}

static void si_ctxcGenerateComicChapterUrl(CHAR *pcString, INT32 nLength, INT32 nID)
{
    if(pcString != NULL)
        snprintf(pcString, nLength, URL_COMIC_CHAPTER, nID);
}

static void si_ctxcGenerateComicCookieUrl(CHAR *pcString, INT32 nLength, INT32 nID)
{
    if(pcString != NULL) 
        snprintf(pcString, nLength,  URL_COOKIE_INFO, nID);
}

static void si_ctxcGenerateRequestRefereUrl(CHAR *pcString, INT32 nLength,
        INT32 nID, INT32 nCID)
{
    if(pcString != NULL)
        snprintf(pcString, nLength, URL_REQUEST_REFERER, nID, nCID);
}

static void si_ctxcGeneratePictureHashUrl(CHAR *pcString, INT32 nLength, INT32 nID, INT32 nCID)
{
    if(pcString != NULL)
        snprintf(pcString, nLength, URL_PICTURE_HASH, nID, nCID);
}

static void si_ctxcGeneratePictureDownloadUrl(CHAR *pcString, INT32 nLength,
        INT32 nUIN, const CHAR *pcDirPath, const CHAR *pcName)
{
    if(pcString != NULL && pcDirPath != NULL && pcName != NULL)
        snprintf(pcString, nLength, URL_PICTURE_DOWNLOAD, nUIN, pcDirPath, pcName);
}

static void si_ctxcDownloadComicFile(const CHAR *pcUrl, const CHAR *pcFileName)
{
    CURL *curl;
    FILE * psCacheFile;
    CURLcode res;
    struct curl_slist *psHeaders=NULL;

    if(pcUrl == NULL)
    {
        return ;
    }
    
    curl = curl_easy_init();

    psHeaders = curl_slist_append(psHeaders, URL_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, psHeaders);
#if defined(__USE_IP_PROXY__)
    if(0 != strcmp(sm_cIPProxy, "NONE"))
        curl_easy_setopt(curl, CURLOPT_PROXY, sm_cIPProxy);
    //curl_easy_setopt(curl, CURLOPT_PROXY, URL_IP_PROXY);
#endif //#if defined(__USE_IP_PROXY__)
    curl_easy_setopt(curl, CURLOPT_URL, pcUrl);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, sm_cCurlLogOption);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Si_ctxcWriteDataFile);
    
    //write to given file
    psCacheFile = fopen(pcFileName, "wb");
    if (psCacheFile)
    {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, psCacheFile);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) 
        {
            DEB_CAPTURETXCOMIC_PRINT("Curl perform failed: %s\n", curl_easy_strerror(res));
            return ;
        }
        fclose(psCacheFile);
        
        curl_easy_cleanup(curl);
    }
}

static void si_ctxcGetComicInfo(INT32 nID)
{
    CHAR cUrlNameBuffer[URL_CACHE_SIZE];
    si_ctxcGenerateComicUrl(cUrlNameBuffer, URL_CACHE_SIZE, nID);
    si_ctxcDownloadComicFile(cUrlNameBuffer, URL_JSON_COMICINFO);
}

static void si_ctxcGetComicChapter(INT32 nID)
{
    CHAR cUrlNameBuffer[URL_CACHE_SIZE];
    si_ctxcGenerateComicChapterUrl(cUrlNameBuffer, URL_CACHE_SIZE, nID);
    si_ctxcDownloadComicFile(cUrlNameBuffer, URL_JSON_CHAPTERLIST);
}

static void si_ctxcGetPictureHash(INT32 nID, INT32 nCID)
{
    CURL *curl;
    FILE * psCacheFile;
    CURLcode res;
    struct curl_slist *psHeaders=NULL;
    CHAR cBuffer[URL_CACHE_SIZE];
  
    curl = curl_easy_init();

    psHeaders = curl_slist_append(psHeaders, URL_USER_AGENT);
    si_ctxcGenerateComicCookieUrl(cBuffer, sizeof(cBuffer), nID);
    psHeaders = curl_slist_append(psHeaders, cBuffer);
    memset(cBuffer, 0x00, sizeof(cBuffer));
    

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, psHeaders);
    
    si_ctxcGenerateRequestRefereUrl(cBuffer, sizeof(cBuffer), nID, nCID);
    curl_easy_setopt(curl, CURLOPT_REFERER, cBuffer);
    memset(cBuffer, 0x00, sizeof(cBuffer));

#if defined(__USE_IP_PROXY__)
    if(0 != strcmp(sm_cIPProxy, "NONE"))
        curl_easy_setopt(curl, CURLOPT_PROXY, sm_cIPProxy);
        //curl_easy_setopt(curl, CURLOPT_PROXY, URL_IP_PROXY);
#endif //#if defined(__USE_IP_PROXY__)

    si_ctxcGeneratePictureHashUrl(cBuffer, sizeof(cBuffer), nID, nCID);
    curl_easy_setopt(curl, CURLOPT_URL, cBuffer);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, sm_cCurlLogOption);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Si_ctxcWriteDataFile);

    // write to Picturehash.json
    psCacheFile = fopen(URL_JSON_PICTUREHASH, "wb");
    if (psCacheFile)
    {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, psCacheFile);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) 
        {
            DEB_CAPTURETXCOMIC_PRINT("Curl perform failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return ;
        }
        fclose(psCacheFile);
        
        curl_easy_cleanup(curl);
    }
}



static json_object * si_ctxcJsonLinkByFile(const CHAR *pcFileName)
{
    json_object *psJsonObj = NULL;
    if(pcFileName != NULL)
    {
        psJsonObj = json_object_from_file(pcFileName);
    }
    return psJsonObj;
}

static void si_ctxcJsonDataFree(json_object * psJsonObj)
{
    json_object_put(psJsonObj);
}

static json_object *si_ctxcJsonDataGet(json_object *psJsonObj, const CHAR *pcKey)
{
    json_object *psValue = NULL;
    enum json_type eType;

    if(psJsonObj == NULL || !error_ptr(psJsonObj))
    {
        DEB_CAPTURETXCOMIC_PRINT("Attention! Get NULL Pointer, please check\n");
        return psJsonObj;
    }
    psValue = json_object_object_get(psJsonObj, pcKey);
#if 0
    if(NULL != psValue)
    {
        eType = json_object_get_type(psValue);
        switch(eType)
        {
            case json_type_string:
                DEB_CAPTURETXCOMIC_PRINT("Key:%s  value: %s\n", pcKey, json_object_get_string(psValue));
                break;

            case json_type_int:
                DEB_CAPTURETXCOMIC_PRINT("Key:%s  value: %d\n", pcKey, json_object_get_int(psValue));
                break;

            case json_type_object:
                DEB_CAPTURETXCOMIC_PRINT("Get another Json object \n");
                break;

            default:
                DEB_CAPTURETXCOMIC_PRINT("Get Json object  Default Type is %d \n", eType);
                break;
        }
    }
#endif //#if 0

    return psValue;
}

static INT32 si_ctxcCheckIllegleChar(CHAR * pcDirPath)
{
    INT32 nStatus = 0;
    INT32 nIndex = 0;
    INT32 nCount = 0;

    for(nIndex = 0; nIndex < strlen(pcDirPath); nIndex++)
    {
        if(pcDirPath[nIndex] == '/')
        {
            if(nCount != 0)
            {
                nStatus = 1;
                pcDirPath[nIndex] = '_';
            }
            nCount++;
        }
    }
    return nStatus;
}

static INT32 si_ctxcCreateFolder(CHAR * pcDirPath)
{
    INT32 nDirStatus = -1;
    if(pcDirPath != NULL)
    {
        if( access(pcDirPath, R_OK) != 0)
        {
            if( mkdir(pcDirPath, 0755) == -1)
            {
                //Check if the sub-folder have illiegle charactor
                si_ctxcCheckIllegleChar(pcDirPath);

                // Recreate again
                if(access(pcDirPath, R_OK) != 0)
                {
                    if( mkdir(pcDirPath, 0755) == -1)
                    {
                        DEB_CAPTURETXCOMIC_PRINT("Create Folder %s failed!\n", pcDirPath);
                    }
                    else
                    {
                        nDirStatus = 1;
                    }
                }
                else
                {
                    nDirStatus = 0;
                }
            }
            else
            {
                nDirStatus = 0;
            }
        }
    }
    return nDirStatus;
}

static void si_ctxcCreateComicDownloadList(INT32 nID, INT32 nCID, CHAR *pcDirPath, INT32 nLength)
{
    si_ctxcGetPictureHash( nID, nCID);
    json_object *psJsonObj = si_ctxcJsonLinkByFile(URL_JSON_PICTUREHASH);
 
    if(psJsonObj == NULL)
    {
        DEB_CAPTURETXCOMIC_PRINT("Create DownloadList error!! nID = %d, nCID = %d \n", nID, nCID);
        return ;
    }
    json_object *psValue = si_ctxcJsonDataGet(psJsonObj, "pCount");
    if(psValue == NULL)
    {
        return ;
    }
    INT32 nTotalPictureCount = json_object_get_int(psValue);
    psValue = si_ctxcJsonDataGet(psJsonObj, "pHash");
    if(psValue == NULL)
    {
        return ;
    }
   
    // Write to the DownloadList.txt
    FILE * psFile;
    INT32 nFileExit = 0;
    pcDirPath[nLength]= '/';
    strcpy(&pcDirPath[nLength+1], NAME_DOWNLOAD_LIST);
    if(access(pcDirPath, F_OK) == 0 && sm_cChapterFolderOption != 0)
    {
        nFileExit = 1;
    }
    else
    {
        if(sm_cChapterFolderOption)
        {
            psFile = fopen(pcDirPath, "wb");
        }
        else
        {
            psFile = fopen(pcDirPath, "a+");
        }
        if(psFile == NULL)
        {
            return ;
        }
    }

    INT32 nPID = 0;
    INT32 nSeq = 0;
    INT32 nIndex = 1;

    while(nIndex <= nTotalPictureCount)
    {
        json_object_object_foreach(psValue, psjKey, psjVal)
        {
            // Find Next json object
            if(json_object_is_type(psjVal, json_type_object))
            {
                // Find seq and pid
                json_object * psNewObj = si_ctxcJsonDataGet(psjVal, "pid");
                if(psNewObj == NULL)
                {
                    continue;
                }
                nPID = json_object_get_int(psNewObj);

                psNewObj = si_ctxcJsonDataGet(psjVal, "seq");
                if(psNewObj == NULL)
                {
                    continue;
                }
                nSeq = json_object_get_int(psNewObj);

                if(nSeq == nIndex)
                {
                    nIndex++;
                    sm_nComicPictureId++;
                    if(nFileExit == 1)
                    {
                        continue ;
                    }
                    CHAR cUrlPath[URL_CACHE_SIZE];
                    CHAR cPath[128];
                    CHAR cFileName[32];
                    CHAR cPicId[]="00000000.jpeg";
                    INT32 nUIN = nID + nCID + nPID;
                    INT32 nUrlLen = 0;

                    nUIN = (nUIN > 10001) ? nUIN : 10001;
                    snprintf(cPath, sizeof(cPath), "/mif800/%d/%d/%d/%d/", (nID%1000)/100, nID%100, nID, nCID);
                    snprintf(cFileName, sizeof(cFileName), "%d.mif2", nPID);
                    si_ctxcGeneratePictureDownloadUrl(&cUrlPath[0], sizeof(cUrlPath), nUIN, cPath, cFileName);
                    nUrlLen = strlen(cUrlPath);
                    
                    snprintf(&cPicId[0], sizeof(cPicId),"%08d.jpeg", sm_nComicPictureId);
                    fwrite(cPicId, 13, 1, psFile);
                    fwrite("\t", 1, 1, psFile);
                    fwrite(cUrlPath, nUrlLen, 1, psFile);
                    fwrite("\n", 1, 1, psFile);
                }
            }
        }
    }
    if(nFileExit != 1)
    {
        fclose(psFile);
    }
    si_ctxcJsonDataFree(psJsonObj);
}

static void si_ctxcCreateComicChapterData(INT32 nID, CHAR *pcDirPath, INT32 nLength)
{
    json_object *psJsonObj = si_ctxcJsonLinkByFile(URL_JSON_CHAPTERLIST);
    json_object * psValue = si_ctxcJsonDataGet(psJsonObj, "length");    
    if( psValue == NULL || nLength >= URL_CACHE_SIZE)
    {
        return ;
    }
    INT32 nTotalChapter = json_object_get_int(psValue);
    INT32 nIndex = 0;
    INT32 nChapterStringLength = 0;
    INT32 nSeq = 0;
    INT32 nCID = 0;

    if(sm_cChapterFolderOption == 0)
    {
        // Delete old download list
        BYTE cTempPath[URL_CACHE_SIZE];
        snprintf(cTempPath, sizeof(cTempPath), "%s/%s", pcDirPath, NAME_DOWNLOAD_LIST);
        if(access(cTempPath, F_OK) == 0)
        {
            remove(cTempPath);
        }
    }

    while(nIndex < nTotalChapter)  // Need to fix here
    {
        json_object_object_foreach(psJsonObj, psjKey, psjVal)
        {
            if( json_object_is_type(psjVal, json_type_object) )
            {
                nCID = atoi(psjKey);
                json_object * psChapterObj = si_ctxcJsonDataGet(psjVal, "t");
                if(psChapterObj == NULL)
                {
                    continue;
                }
                nChapterStringLength = strlen(json_object_get_string(psChapterObj)) + 1;
                nChapterStringLength = (nChapterStringLength+nLength < URL_CACHE_SIZE) ? nChapterStringLength : (URL_CACHE_SIZE - nLength);
                memset(&pcDirPath[nLength], 0x00, nChapterStringLength);
                json_object * psSeq = si_ctxcJsonDataGet(psjVal, "seq");
                if(psSeq == NULL)
                {
                    continue;
                }
                nSeq = json_object_get_int(psSeq);

                if(nSeq == nIndex)
                {
                    nIndex++;

                    if(sm_cChapterFolderOption)
                    {
                        // Create chapter folder by use chapter name
                        snprintf(&pcDirPath[nLength], URL_CACHE_SIZE, "/%s", json_object_get_string(psChapterObj));
                        si_ctxcCreateFolder(pcDirPath);
                        // Create Download List
                        si_ctxcCreateComicDownloadList(nID, nCID, pcDirPath, nLength+nChapterStringLength);
                    }
                    else
                    {
                        // Create Chapter file 
                        FILE * psFile;
                        CHAR cTemp[URL_CACHE_SIZE];
                        snprintf(cTemp, sizeof(cTemp), "%s/%08dChapter:%s CID:%d", pcDirPath, sm_nComicPictureId, json_object_get_string(psChapterObj), nCID);
                        // Check illiegle chapter charactor
                        si_ctxcCheckIllegleChar(cTemp);
                        psFile = fopen(cTemp, "wb+");
                        if(psFile)
                            fclose(psFile);
                        si_ctxcCreateComicDownloadList(nID, nCID, pcDirPath, nLength);
                    }
                }
            }
        }
    }

    si_ctxcJsonDataFree(psJsonObj);  
}

static void si_ctxcCreateComicFolderData(INT32 nID)
{
    CHAR cDirPath[URL_CACHE_SIZE];
    json_object *psJsonObj = si_ctxcJsonLinkByFile(URL_JSON_COMICINFO);
    json_object * psValue = si_ctxcJsonDataGet(psJsonObj, "title");    
    if(psValue == NULL)
    {
        return ;
    }

    // Create a folder by use title name
    snprintf(cDirPath, sizeof(cDirPath), "%s", json_object_get_string(psValue));
    snprintf(sm_cTitleName, sizeof(sm_cTitleName), "%s", cDirPath);
    
    
    if( 0 != si_ctxcIsComicExist(sm_cTitleName) )
    {
    
        DEB_CAPTURETXCOMIC_PRINT("The Comic %s has already been download in %s folder \n", sm_cTitleName, NAME_COMPLETE_FOLDER);
        return ;
    }

    si_ctxcCreateFolder(cDirPath);

    // Download the extra-cover jpeg
    psValue = si_ctxcJsonDataGet(psJsonObj, "cover_url");
    if(psValue == NULL)
    {
        return ;
    }
    INT32 nLength = strlen(cDirPath);
    strcpy(&cDirPath[nLength], "/封面.jpeg");    
    si_ctxcDownloadComicFile(json_object_get_string(psValue), cDirPath);   
    memset(&cDirPath[nLength], 0x00, 12);

    // Write the brief introduction of comic
    psValue = si_ctxcJsonDataGet(psJsonObj, "brief_intrd");
    if(psValue == NULL)
    {
        return ;
    }
    FILE * psFile;
    strcpy(&cDirPath[nLength], "/简介.txt");
    
    psFile = fopen(cDirPath, "wb");
    if(psFile)
    {
        INT32 nBriefLength = strlen(json_object_get_string(psValue));
        fwrite(json_object_get_string(psValue), nBriefLength, 1, psFile);        
        fclose(psFile);
    }

    memset(&cDirPath[nLength], 0x00, 12);

    si_ctxcJsonDataFree(psJsonObj);


    DEB_CAPTURETXCOMIC_PRINT("Checking end, the download start \n");
    si_ctxcCreateComicChapterData(nID, &cDirPath[0], nLength);
}


static void si_ctxcGetComicDownloadListProcess(INT32 nID)
{
    sm_nComicPictureId = 0;
    si_ctxcGetComicInfo( nID); 
    si_ctxcGetComicChapter(nID);
    DEB_CAPTURETXCOMIC_PRINT("Checking the download url, Please wait...\n");
    si_ctxcCreateComicFolderData(nID);
}

#define THREAD_NUM_MAX 10
static INT32 sm_nThreadID[THREAD_NUM_MAX];
static CHAR sm_cDownloadCache[THREAD_NUM_MAX][URL_CACHE_SIZE];
static CHAR sm_cDownloadName[URL_CACHE_SIZE];
static INT32 sm_nDownloadNameLen;
static INT32 sm_nThreadNum = THREAD_NUM_MAX;

static void *si_ctxcDownloadComicFileMulti(void * argv)
{
    INT32 nID = *(INT32 *) argv;
    
    if(nID < sm_nThreadNum)
    {
        CHAR cFileName[URL_CACHE_SIZE];
        
        memcpy(&cFileName[0], &sm_cDownloadName[0], sm_nDownloadNameLen);
        cFileName[sm_nDownloadNameLen] = '/';
        memcpy(&cFileName[sm_nDownloadNameLen+1], &sm_cDownloadCache[nID][0], 13);
        cFileName[sm_nDownloadNameLen+14] = 0x00;
        
        if(access(cFileName, F_OK) == 0 )
        {
            // File has been already download, then skip it.
        }
        else
        {
            si_ctxcDownloadComicFile(&sm_cDownloadCache[nID][14], &cFileName[0]);
            DEB_CAPTURETXCOMIC_PRINT("%d | %s --> %s |\n", nID, cFileName, &sm_cDownloadCache[nID][14]);
        }
    }
    else
    {
        DEB_CAPTURETXCOMIC_PRINT("Attention, Thread ERROR, ID = %d \n", nID);
    }
}


static void si_ctxcGetComicDownloadThread(INT32 nCount)
{
    pthread_t pthId[THREAD_NUM_MAX];
    INT32 nIndex;
    INT32 nError;
    INT32 nMaxThread = (nCount >= sm_nThreadNum) ? sm_nThreadNum : nCount;

    for(nIndex=0; nIndex< nMaxThread; nIndex++) 
    {    
        sm_nThreadID[nIndex] = nIndex;
        nError = pthread_create(&pthId[nIndex],
                NULL,
                si_ctxcDownloadComicFileMulti,
                (void *)&sm_nThreadID[nIndex]);
        if(0 != nError)
        {
            DEB_CAPTURETXCOMIC_PRINT("Couldn't run thread number %d, errno %d\n", nIndex, nError);
        }
    }

    // now wait for all threads to terminate 
    for(nIndex=0; nIndex< nMaxThread; nIndex++) 
    {
        nError = pthread_join(pthId[nIndex], NULL);
    }
}

static void si_ctxcGetComicDownloadFileToStart(CHAR *pcDownloadFile)
{
    // Open the DownloadFile
    FILE *psFile;
    if(pcDownloadFile == NULL)
    {
        return;
    }
    psFile = fopen(pcDownloadFile, "r");
    if(psFile)
    {
        INT32 nEof = 0;
        INT32 nIndex = 0;
        while(1)
        {
            for(nIndex = 0; nIndex < sm_nThreadNum; )
            {

                if(fgets(&sm_cDownloadCache[nIndex][0], URL_CACHE_SIZE, psFile) == NULL)
                {
                    nEof = 1;
                    //DEB_CAPTURETXCOMIC_PRINT("ReadEnd\n");
                    break;
                }
                INT32 nLength = strlen(&sm_cDownloadCache[nIndex][0]);
                if(nLength > 14)
                {
                    //delete the \n by the tail
                    sm_cDownloadCache[nIndex][nLength - 1] = 0x00;
                    nIndex++;
                }
            }
            si_ctxcGetComicDownloadThread(nIndex);
            if(nEof == 1)
            {
                break;
            }
        }
        fclose(psFile);
    }
}

static void si_ctxcGetCurrentPath(CHAR *pcPath)
{
    if(pcPath != NULL)
  //      getwd(pcPath);
        getcwd(pcPath, URL_CACHE_SIZE);
}

static INT32 si_ctxcIsComicExist(CHAR *pcComicTitle)
{   
    INT32 nStatus = 0;
    DIR * psDir = NULL;
    struct dirent * psDirEntry;
    CHAR cCurrentPath[URL_CACHE_SIZE];
    CHAR cCompletePath[URL_CACHE_SIZE];
   
    // Get current Path
    si_ctxcGetCurrentPath(&cCurrentPath[0]); 

    snprintf(cCompletePath, sizeof(cCompletePath), "%s/%s", cCurrentPath, NAME_COMPLETE_FOLDER);
    if(access(cCompletePath, R_OK) != 0)
    {
        // Doesn't have the complete file
        return nStatus;
    }
    
    if( (psDir = opendir(&cCompletePath[0])) == NULL)
    {
        DEB_CAPTURETXCOMIC_PRINT("Open dir Failed !\n");
    }
    else
    {
        while(psDirEntry = readdir(psDir))
        {
            if(psDirEntry->d_type & DT_DIR)
            {
                if(strcmp(psDirEntry->d_name,".") ==0
                        || strcmp(psDirEntry->d_name, "..") ==0)
                {
                    continue;
                }
                else if(strcmp(psDirEntry->d_name, pcComicTitle) ==0)
                {
                    nStatus = 1;
                    break;
                }
            } 
        }
    }
    return nStatus;
}

static void si_ctxcGetComicDownloadListProcessCore(CHAR * pcDirPath)
{
    DIR * psDir = NULL;
    struct dirent * psDirEntry;
    if(pcDirPath == NULL)
    {
        return ;
    }
    if( (psDir = opendir(&pcDirPath[0])) == NULL)
    {
        DEB_CAPTURETXCOMIC_PRINT("Open dir Failed !\n");
    }
    else
    {
        CHAR cSubFolderName[512];
        while(psDirEntry = readdir(psDir))
        {
            if(psDirEntry->d_type & DT_DIR)
            {
                if(strcmp(psDirEntry->d_name,".") ==0
                        || strcmp(psDirEntry->d_name, "..") ==0)
                {
                    continue;
                }
                snprintf(cSubFolderName, sizeof(cSubFolderName),"%s/%s", pcDirPath, psDirEntry->d_name);
                si_ctxcGetComicDownloadListProcessCore(cSubFolderName);
            }
            else
            {
                if(strcmp(psDirEntry->d_name, NAME_DOWNLOAD_LIST) == 0)
                {

                    strcpy(sm_cDownloadName, pcDirPath);
                    sm_nDownloadNameLen = strlen(pcDirPath);
                    strcpy(&cSubFolderName[0], pcDirPath);
                    cSubFolderName[sm_nDownloadNameLen] = '/';
                    strcpy(&cSubFolderName[sm_nDownloadNameLen+1], NAME_DOWNLOAD_LIST);
                    si_ctxcGetComicDownloadFileToStart(cSubFolderName);
                }
            }
        }
    }
}

void si_ctxcGetComicDownloadByPath(CHAR *pcPath, CHAR *pcCompletePath, BOOL bCurrentFolder)
{
    DIR * psDir = NULL;
    struct dirent * psDirEntry;
    CHAR cCompletePath[URL_CACHE_SIZE];
    
    if(pcPath == NULL || pcCompletePath == NULL)
    {
        return ;
    }
 
    if(access(pcCompletePath, R_OK) != 0)
    {
        if(mkdir(pcCompletePath, 0755) == -1)
        {
            DEB_CAPTURETXCOMIC_PRINT("Create complete path failed \n");
        }
    }
    else if(access(pcPath, R_OK) != 0)
    {
        return ;
    }

    // Get current Path
    DEB_CAPTURETXCOMIC_PRINT("Get Current Path is %s\n", pcPath); 
    

    if(bCurrentFolder)
    {
        CHAR cRename[URL_CACHE_SIZE];
        sm_cDownloadComplete = 0;
        si_ctxcGetComicDownloadListProcessCore(pcPath);
        snprintf(cRename, sizeof(cRename), "%s/%s", pcCompletePath, sm_cTitleName);
        rename(pcPath, cRename);
        sm_cDownloadComplete = 1;
    }
    else if( (psDir = opendir(pcPath)) == NULL)
    {
        DEB_CAPTURETXCOMIC_PRINT("Open dir Failed !\n");
    }
    else
    {
        CHAR cRename[URL_CACHE_SIZE];
        CHAR cSubFolderName[URL_CACHE_SIZE];
        
        sm_cDownloadComplete = 0;
        while(psDirEntry = readdir(psDir))
        {
            if(psDirEntry->d_type & DT_DIR)
            {
                if(strcmp(psDirEntry->d_name,".") ==0
                        || strcmp(psDirEntry->d_name, "..") ==0
                        || strcmp(psDirEntry->d_name, NAME_COMPLETE_FOLDER) ==0)
                {
                    continue;
                }
                snprintf(cSubFolderName, sizeof(cSubFolderName),"%s/%s", pcPath, psDirEntry->d_name);
                
                // Find the Downloadlist
                si_ctxcGetComicDownloadListProcessCore(cSubFolderName);

                // Move the file to the  foloder 完成
                snprintf(cRename, sizeof(cRename), "%s/%s", pcCompletePath, psDirEntry->d_name);
                rename(cSubFolderName, cRename);
            }
        }
        sm_cDownloadComplete = 1;
    }
}

void si_ctxcGetCurrentComicDownloadProcess(void)
{
    CHAR cCurrentPath[URL_CACHE_SIZE];
    CHAR cCompletePath[URL_CACHE_SIZE];
    
    // Get current Path
    si_ctxcGetCurrentPath(&cCurrentPath[0]);
    snprintf(cCompletePath, sizeof(cCompletePath), "%s/%s", cCurrentPath, NAME_COMPLETE_FOLDER);
    strcat(cCurrentPath, "/");
    strcat(cCurrentPath, sm_cTitleName);
    si_ctxcGetComicDownloadByPath(&cCurrentPath[0], &cCompletePath[0], 1);
}

void si_ctxcGetAllComicDownloadProcess(void)
{
    DIR * psDir = NULL;
    struct dirent * psDirEntry;
    CHAR cCurrentPath[URL_CACHE_SIZE];
    CHAR cCompletePath[URL_CACHE_SIZE];
   
    // Get current Path
    si_ctxcGetCurrentPath(&cCurrentPath[0]); 
    snprintf(cCompletePath, sizeof(cCompletePath), "%s/%s", cCurrentPath, NAME_COMPLETE_FOLDER);
    si_ctxcGetComicDownloadByPath(&cCurrentPath[0], &cCompletePath[0], 0);
}

static void si_ctxcDeleteCacheFile(void)
{
    if(access(URL_JSON_COMICINFO, F_OK) == 0)
    {
        remove(URL_JSON_COMICINFO);
    }
    if(access(URL_JSON_CHAPTERLIST, F_OK) == 0)
    {
        remove(URL_JSON_CHAPTERLIST);
    }
    if(access(URL_JSON_PICTUREHASH, F_OK) == 0)
    {
        remove(URL_JSON_PICTUREHASH);
    }
}

static void si_ctxcGetAllComicName(INT32 nID)
{
    sm_nComicPictureId = 0;
    si_ctxcGetComicInfo( nID); 
    
    CHAR cDirPath[URL_CACHE_SIZE];
    json_object *psJsonObj = si_ctxcJsonLinkByFile(URL_JSON_COMICINFO);
    json_object * psValue = si_ctxcJsonDataGet(psJsonObj, "title");    
    if(psValue == NULL)
    {
        return ;
    }

    // get comic name
    snprintf(cDirPath, sizeof(cDirPath), "%d:%s\n", nID, json_object_get_string(psValue));
    
    FILE * psFile;
    
    psFile = fopen(NAME_WHOLE_COMIC, "a+");
    if(psFile)
    {
        INT32 nLength = strlen(cDirPath);
        fwrite(cDirPath, nLength, 1, psFile);        
        fclose(psFile);
    }
}

static INT32 si_ctxcSearchID(CHAR *pcName)
{
    INT32 nID = 0;
    CHAR cCurrentPath[URL_CACHE_SIZE];
    si_ctxcGetCurrentPath(&cCurrentPath[0]);
    
    // Check if the comic file exist       
    if(access(NAME_WHOLE_COMIC, F_OK) != 0)
    {
        DEB_CAPTURETXCOMIC_PRINT("Sorry! Could not find the File %s \n", NAME_WHOLE_COMIC);
        return nID;
    }
    
    INT32 nCount = 0;
    FILE *psFile;
    psFile = fopen(NAME_WHOLE_COMIC, "r");
    if(psFile)
    {
        INT32 nEof = 0;
        CHAR cTemp[URL_CACHE_SIZE];
        CHAR cTitle[URL_CACHE_SIZE];
        INT32 nTempID = 0;
        while(1)
        {
            if(fgets(&cTemp[0], sizeof(cTemp), psFile) == NULL)
            {
                nEof = 1;
                break;
            }
            
            sscanf(cTemp, "%08d:%s", &nTempID, cTitle);

            if(NULL != strstr(cTitle, pcName))
            {
                nCount++;
                nID = nTempID;
                printf("%d. \t%s", nCount, cTemp);
            }

            if(1 == nEof)
            {
                break;
            }
        
        }
        fclose(psFile);
    }

    if(nCount > 1)
    {
        printf("Find too much, please input a correct name \n");
        nID = 0;
    }

    return nID;
}

static void si_ctxcPrintWelcomeInfo(void)
{
    printf("------------------------------------------------------\n");
    printf("|    This program is designed by LibraLin            |\n");
    printf("|    And it is used for download tencent comic       |\n");
    printf("|    Please do not use for any commercial purpose    |\n");
    printf("|    The download will be start after 3 seconds      |\n");
    printf("|    Please enjoy it!                                |\n");
    printf("------------------------------------------------------\n");
    sleep(3);
}

typedef void (*fnEXECPROCESS)(INT32 nID);
static void si_ctxcForkProcess(fnEXECPROCESS fnExecProcess, INT32 nID)
{
    pid_t pid;
    INT32 nStatus = 0;
#if defined(__USE_FORK_PROCESS__)
    pid = fork();
    if(-1 == pid)
    {
        DEB_CAPTURETXCOMIC_PRINT("Fork Process failed \n");
    }
    else if(0 == pid)
    {
        fnExecProcess(nID);
    }
    else
    {
        waitpid(pid, &nStatus, 0);
    }
#else 
    fnExecProcess(nID);

#endif 
}

static void si_ctxcExecDownloadProcess(INT32 nID)
{
    DEB_CAPTURETXCOMIC_PRINT("Find ID = %d\n", nID);
    si_ctxcGetComicDownloadListProcess(nID);
    si_ctxcGetCurrentComicDownloadProcess();
    si_ctxcDeleteCacheFile();
}

static void si_ctxcExecGetComicNameProcess(INT32 nID)
{
    DEB_CAPTURETXCOMIC_PRINT("Input ID = %d\n", nID);
    si_ctxcGetAllComicName(nID);
    si_ctxcDeleteCacheFile();
}

static void si_ctxcExecUpdateProcess(INT32 nID)
{
    nID = nID;
    si_ctxcGetAllComicDownloadProcess();
    si_ctxcDeleteCacheFile();
}

static void si_ctxcExecOption(int argc, char *const *argv)
{
    INT32 nIndex;
    for( nIndex = 1 ; nIndex < argc ; nIndex++ )
    {
        CHAR *p = argv[nIndex];
            
        if( NULL != strstr(p, "-s:"))
        {
            CHAR cSearchName[64];
            sscanf(p, "-s:%s", cSearchName);
            INT32 nID = si_ctxcSearchID(cSearchName);
            if( nID != 0 )
            {
                si_ctxcForkProcess(si_ctxcExecDownloadProcess, nID);
            }
        }
        else if(NULL != strstr(p, "-i:"))
        {
            INT32 nID = 0;
            sscanf(p, "-i:%d", &nID);
            if(nID != 0)
            {
                si_ctxcForkProcess(si_ctxcExecDownloadProcess, nID);
            }
        }
        else if(NULL != strstr(p, "-t:"))
        {
            INT32 nID = 0;
            sscanf(p, "-t:%d", &nID);
            if(nID != 0)
            {
                si_ctxcForkProcess(si_ctxcExecGetComicNameProcess, nID);
            }
        }
        else if(NULL != strstr(p, "-c"))
        {
            sscanf(p, "-c:%d", &sm_cChapterFolderOption);
            DEB_CAPTURETXCOMIC_PRINT("Use the Chapter Folder %s\n", (sm_cChapterFolderOption == 0) ? NAME_STRING_OFF : NAME_STRING_ON );
        }
        else if(NULL != strstr(p, "-j"))
        {
            INT32 nThreadNum = 0;
            sscanf(p, "-j:%d", &nThreadNum);
            if(nThreadNum > 0 && nThreadNum <= THREAD_NUM_MAX)
            {
                sm_nThreadNum = nThreadNum;
                DEB_CAPTURETXCOMIC_PRINT("Change Multi-thread to %d \n", sm_nThreadNum);
            }
        }
        else if(NULL != strstr(p, "-IPProxy"))
        {
            sscanf(p, "-IPProxy:%s", &sm_cIPProxy[0]);
            DEB_CAPTURETXCOMIC_PRINT("Use the IP Proxy %s \n", sm_cIPProxy);
        }
        else if(NULL != strstr(p, "-Author"))
        {
            printf("This program is designed by LibraLin\n");
            printf("If you get any problem please mail \33[31m linwenfu8910@163.com \33[0m \n");
        }
        else if(NULL != strstr(p, "-Log"))
        {
            sscanf(p, "-Log:%d", &sm_cCurlLogOption);
            DEB_CAPTURETXCOMIC_PRINT("Use CURL Log display %s\n", (sm_cCurlLogOption == 0) ? NAME_STRING_OFF : NAME_STRING_ON);
        }
        else if(NULL != strstr(p, "-update"))
        {
            si_ctxcForkProcess(si_ctxcExecUpdateProcess, 0);
        }
        else if(NULL != strstr(p, "--help"))
        {
            printf("\n");
            printf("The Program support the option below: \n");
            printf("    -s:xxxx       This command will search the string xxxx\n");
            printf("    -c:x          This command will turn on/off the chapter folder\n");
            printf("    -i:xxxx       This command will download the ID xxxxx\n");
            printf("    -update       This command will just continue last download ID xxxx");
            printf("    -t:xxxx       This command will test the comic ID is available or not\n");
            printf("    -j:xx         This command will setting the download thread number, default is 10\n");
            printf("    -IPProxy:xxx  This command will setting the download IP Proxy\n");
            printf("\n");
        }
    }
}

typedef void (*sighandler_t)(INT32);

static void si_ctxcExitSignalProcessCore(INT32 nSignalNo)
{
    DEB_CAPTURETXCOMIC_PRINT("Catch a quit signal, Please wait some moment to quit! \n");
    if(sm_cDownloadComplete != 0xff && sm_cDownloadComplete != 1)
    {
        INT32 nIndex;        
        for(nIndex = 0; nIndex < sm_nThreadNum; nIndex++)
        {
            CHAR cTemp[URL_CACHE_SIZE];
            if(sm_cDownloadCache[nIndex][0] != 0x00)
            {
                snprintf(cTemp, sizeof(cTemp), "%s/", sm_cDownloadName);
                memcpy(&cTemp[strlen(cTemp)], sm_cDownloadCache[nIndex], 13);
                DEB_CAPTURETXCOMIC_PRINT("Delete un-complete file %s \n", cTemp);
                if(access(cTemp, F_OK) == 0)
                {
                    remove(cTemp);
                }
            }
        }
    }
    si_ctxcDeleteCacheFile();
    exit(0);
}

static void si_ctxcExitSignalProcess(void)
{
    sighandler_t sSigReturn;
    sSigReturn = signal(SIGTSTP, si_ctxcExitSignalProcessCore);
    if(SIG_ERR == sSigReturn)
    {
        DEB_CAPTURETXCOMIC_PRINT("SIGTSTP failed \n");
    }
    sSigReturn = signal(SIGQUIT, si_ctxcExitSignalProcessCore);
    if(SIG_ERR == sSigReturn)
    {
        DEB_CAPTURETXCOMIC_PRINT("SIGQUIT failed\n");
    }
    
    sSigReturn = signal(SIGINT, si_ctxcExitSignalProcessCore);
    if(SIG_ERR == sSigReturn)
    {
        DEB_CAPTURETXCOMIC_PRINT("SIGINT failed\n");
    }

}

int main(int argc, char *argv[])
{
    CURL *curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) 
    {
        //si_ctxcPrintWelcomeInfo();
        si_ctxcExitSignalProcess();
        si_ctxcExecOption(argc, argv);
    }

    return 0;
}
