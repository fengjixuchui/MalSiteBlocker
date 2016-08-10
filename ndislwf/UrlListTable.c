#include "precomp.h"
#include "UrlListTable.h"
#include "FilterFunction.h"

LIST_ENTRY gUrlListHead;
FILTER_LOCK gUrlListLock;

#define __FILENUMBER    'PNPF'

VOID InitializeUrlListTable()
{
    InitializeListHead(&gUrlListHead);
    FILTER_INIT_LOCK(&gUrlListLock);
}

VOID ClearUrlListTable()
{
    PLIST_ENTRY listEntry;
    struct UrlInfo* urlInfo;

    while (gUrlListHead.Flink != &gUrlListHead)
    {
        FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
        listEntry = RemoveHeadList(&gUrlListHead);
        FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);
        urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        if (urlInfo->isCopiedReceivePacket)
        {
            FreeNetBufferLists(urlInfo->netBufferList);
        }
        FILTER_FREE_MEM(urlInfo->url);
        FILTER_FREE_MEM(urlInfo);
    }
}

VOID UninitializeUrlListTable()
{
    ClearUrlListTable();
    RemoveHeadList(&gUrlListHead);
    FILTER_FREE_LOCK(&gUrlListLock);
}

VOID InsertUrl(_In_ NDIS_HANDLE ndisHandle, _In_ char* urlString, _In_ UINT32 urlLength, _In_ USHORT srcPort, _In_ BOOLEAN isDispatchLevel)
{
    struct UrlInfo* urlInfo = NULL;
    urlInfo = FILTER_ALLOC_MEM(ndisHandle, sizeof(struct UrlInfo));

    urlInfo->url = urlString;
    urlInfo->urlLength = urlLength;
    urlInfo->localPort = srcPort;
    urlInfo->isSendToUser = FALSE;
    urlInfo->scanResult = kUnknown;
    KeQuerySystemTime(&urlInfo->packetRequestTime);
    urlInfo->isCopiedReceivePacket = FALSE;

    FILTER_ACQUIRE_LOCK(&gUrlListLock, isDispatchLevel);
    InsertTailList(&gUrlListHead, &urlInfo->listEntry);
    FILTER_RELEASE_LOCK(&gUrlListLock, isDispatchLevel);
}

BOOLEAN IsScanningUrlPort(_In_ UINT16 port, _Outptr_ struct UrlInfo** urlInfo)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY listEntry = gUrlListHead.Flink;

    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    while (listEntry != &gUrlListHead)
    {
        *urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        if ((*urlInfo)->localPort == port)
        {
            result = TRUE;
            break;
        }
        listEntry = listEntry->Flink;
    }
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);

    return result;
}

BOOLEAN GetUrlInfo(_Outptr_ struct UrlInfo** urlInfo)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY listEntry = gUrlListHead.Flink;

    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    while (listEntry != &gUrlListHead)
    {
        *urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        if ((*urlInfo)->isSendToUser == FALSE)
        {
            (*urlInfo)->isSendToUser = TRUE;
            result = TRUE;
            break;
        }
        listEntry = listEntry->Flink;
    }
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);

    return result;
}

BOOLEAN GetNeedToDeleteListEntry(_Outptr_ struct UrlInfo** urlInfo)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY listEntry = gUrlListHead.Flink;
    TIME currentTime;
    KeQuerySystemTime(&currentTime);

    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    while (listEntry != &gUrlListHead)
    {
        *urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        // ��Ŷ�� ������ 30�� �̻� ������, ��ĵ�� �Ϸ������.
        if (currentTime.QuadPart - (*urlInfo)->packetRequestTime.QuadPart > 300000000 && // ��Ŷ ��û �ð��� 30�� �̻� �����ٸ�
            (*urlInfo)->scanResult != kUnknown)
        {
            result = TRUE;
            break;
        }
        listEntry = listEntry->Flink;
    }
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);

    return result;
}

BOOLEAN GetNeedToSendPacketListEntry(_Outptr_ struct UrlInfo** urlInfo)
{
    BOOLEAN result = FALSE;
    PLIST_ENTRY listEntry = gUrlListHead.Flink;
    TIME currentTime;
    KeQuerySystemTime(&currentTime);

    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    while (listEntry != &gUrlListHead)
    {
        *urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        // ��Ŷ�� �˻� �Ϸ�ư�, ����� ��Ŷ�� �����ϸ�
        if ((*urlInfo)->scanResult != kUnknown &&
            (*urlInfo)->isCopiedReceivePacket == TRUE)
        {
            result = TRUE;
            break;
        }
        listEntry = listEntry->Flink;
    }
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);

    return result;
}

/*
 * Description : �����κ��� ���� ��ĵ ����� �ش��ϴ� UrlInfo ����ü�� �����Ѵ�.
 * Arguemnts : scanInfo - ������ ������ ��ĵ����
*/
VOID SaveScanResult(_In_ struct ScanInfo* scanInfo)
{
    PLIST_ENTRY listEntry = gUrlListHead.Flink;
    struct UrlInfo* urlInfo;

    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    while (listEntry != &gUrlListHead)
    {
        urlInfo = CONTAINING_RECORD(listEntry, struct UrlInfo, listEntry);
        if (urlInfo->localPort == scanInfo->localPort)
        {
            urlInfo->scanResult = scanInfo->scanResult;
            break;
        }
        listEntry = listEntry->Flink;
    }
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);
}

VOID DeleteUrlInfo(_In_ struct UrlInfo* urlInfo)
{
    FILTER_ACQUIRE_LOCK(&gUrlListLock, FALSE);
    RemoveEntryList(&urlInfo->listEntry);
    FILTER_RELEASE_LOCK(&gUrlListLock, FALSE);

    if (urlInfo->isCopiedReceivePacket)
    {
        FreeNetBufferLists(urlInfo->netBufferList);
        urlInfo->isCopiedReceivePacket = FALSE;
    }

    FILTER_FREE_MEM(urlInfo->url);
    FILTER_FREE_MEM(urlInfo);
}