#include "precomp.h"
#include "FilterFunction.h"
#include "WhiteListTable.h"

#define __FILENUMBER    'PNPF'

#define ISDOUBLE_ENTER(data, idx) (data[idx] == '\x0d' && data[idx + 1] == '\x0a' && data[idx + 2] == '\x0d' && data[idx + 3] == '\x0a')
#define IS_ENTER(data, idx) (data[idx] == '\x0d' && data[idx + 1] == '\x0a')

//CHAR gAcceptTextString[37] = "Accept: application/x-ms-application"; // internet explorer 8���� ������ ��û��
CHAR gAcceptTextString[18] = "Accept: text/html";
CHAR gRefererTextString[10] = "Referer: ";
CHAR gHostString[7] = "Host: ";

BOOLEAN IsWhiteListUrl(_In_ PCHAR urlString, _In_ UINT32 hostLength)
{
    BOOLEAN result = FALSE;

    // ȭ��Ʈ����Ʈ �˻�. ���� ��޽����� �ϱ� ���ؼ��� list�� �־ Ȯ�����ָ� ��.
    if (IsInWhiteList(urlString, hostLength))
    {
        result = TRUE;
    }

    return result;
}

BOOLEAN AnalyzePacketAndParseUrl(_In_ PCHAR httpPacket, _In_ NDIS_HANDLE ndisHandle, _Outptr_ PCHAR* urlString, _Outptr_ UINT32* hostLength, _Outptr_ UINT32* urlLength)
{
    BOOLEAN result = FALSE;
    BOOLEAN isAcceptText = FALSE;
    int packetIdx = 4;
    int hostIdx = 0;
    PCHAR urlStringStartPoint = NULL;
    int urlIdx = 0;

    if (httpPacket[0] == 'G' && httpPacket[1] == 'E' && httpPacket[2] == 'T') // check GET METHOD
    {
        // �߰� url �κ��� �����´�.
        urlStringStartPoint = &httpPacket[4];
        while (httpPacket[packetIdx] != ' ')
        {
            ++packetIdx;
            ++urlIdx;
        }

        // �� ������ ���ڰ��� '/' �̶�� ��� �Ǵ� ���̹Ƿ� ��������. ex) www.daum.net/
        if (httpPacket[packetIdx - 1] == '/')
        {
            --urlIdx;
        }

        // ��Ŷ�� ���� "\x0d\x0a\x0d\x0a" ���� Ȯ���Ѵ�.
        while (!ISDOUBLE_ENTER(httpPacket, packetIdx))
        {
            if (IS_ENTER(httpPacket, packetIdx))
            {
                packetIdx += 2;
            }

            // Accept ����� text/html ���� Ȯ���Ѵ�.
            if (NdisEqualMemory(&httpPacket[packetIdx], gAcceptTextString, sizeof(gAcceptTextString) - 1) == 1)
            {
                isAcceptText = TRUE;
            }
            // Host ����� üũ�� �� ���� �����´�.
            else if (NdisEqualMemory(&httpPacket[packetIdx], gHostString, sizeof(gHostString) - 1) == 1)
            {
                packetIdx += 6;
                while (!IS_ENTER(httpPacket, packetIdx))
                {
                    ++packetIdx;
                    ++hostIdx;
                }

                *urlString = (PCHAR)FILTER_ALLOC_MEM(ndisHandle, hostIdx + urlIdx + 1);
                if (*urlString != NULL)
                {
                    NdisMoveMemory(*urlString, &httpPacket[packetIdx - hostIdx], hostIdx);
                    NdisMoveMemory(*urlString + hostIdx, urlStringStartPoint, urlIdx);
                    (*urlString)[hostIdx + urlIdx] = 0;
                    *urlLength = hostIdx + urlIdx + 1;
                    *hostLength = hostIdx;
                }
            }
            // ���������� �Ϸ��� �õ����� Ȯ���Ѵ�.
            else if (NdisEqualMemory(&httpPacket[packetIdx], gRefererTextString, sizeof(gRefererTextString) - 1) == 1)
            {
                if (NdisEqualMemory(&httpPacket[packetIdx + 9], "http://127.0.0.1:8012/", 22) == 1)
                {
                    isAcceptText = FALSE;
                    break;
                }
            }

            // ��Ŷ�� ��������� �� ������ �̵��Ѵ�. "\x0d\x0a"
            while (!IS_ENTER(httpPacket, packetIdx))
            {
                ++packetIdx;
            }
        }
    }

    if (*urlString)
    {
        if (isAcceptText)
        {
            result = TRUE;
        }
        else
        {
            FILTER_FREE_MEM(*urlString);
            *urlString = NULL;
        }
    }

    return result;
}

VOID CopyRedirectPage(_Inout_ CHAR* httpPacket, _In_ USHORT port)
{
    ULONG idx = 87;
    USHORT divideNum = 10000;

    // ����������� ������� ���ü����� �����̷�Ʈ ���ִ� �������� ����
    NdisMoveMemory(httpPacket, "HTTP/1.1 200 OK\x0d\x0a\x0d\x0a<script type='text/javascript'>location.href='http://127.0.0.1:8012/", 87);
    
    for (; divideNum != 0; divideNum /= 10)
    {
        httpPacket[idx++] = '0' + (UCHAR)(port / divideNum);
        port = port % divideNum;
    }

    NdisMoveMemory(&httpPacket[idx], "';</script>", 11);
    idx += 11;

    while (httpPacket[idx] != '\x00')
    {
        httpPacket[idx++] = '\x00';
    }
}

BOOLEAN CopyNetBufferLists(_In_ PNET_BUFFER_LIST netBufferLists, _Outptr_ PNET_BUFFER_LIST* outNetBufferList)
{
    BOOLEAN result = FALSE;
    //PNET_BUFFER_LIST pNextNetBufferList = NULL;
    //PNET_BUFFER_LIST nextNetBufferList = NULL;

    if (netBufferLists)
    {
        *outNetBufferList = NdisAllocateCloneNetBufferList(netBufferLists, NULL, NULL, NDIS_CLONE_FLAGS_USE_ORIGINAL_MDLS);

        if (*outNetBufferList)
        {
            /*nextNetBufferList = netBufferLists->Next;
            pNextNetBufferList = *outNetBufferList;
            while (nextNetBufferList)
            {
                pNextNetBufferList->Next = NdisAllocateCloneNetBufferList(nextNetBufferList, NULL, NULL, NDIS_CLONE_FLAGS_USE_ORIGINAL_MDLS);
                nextNetBufferList = nextNetBufferList->Next;
                pNextNetBufferList = pNextNetBufferList->Next;
            }*/

            NdisSetNblFlag(*outNetBufferList, 0x10000000);

            result = TRUE;
        }
    }

    return result;
}

VOID FreeNetBufferLists(_In_ PNET_BUFFER_LIST netBufferList)
{
    /*PNET_BUFFER_LIST pNextNetBufferList = NULL;
    PNET_BUFFER_LIST pPrevNetBufferList = NULL;

    while (netBufferList->Next)
    {
        pPrevNetBufferList = netBufferList;
        pNextNetBufferList = netBufferList->Next;
        while (pNextNetBufferList->Next)
        {
            pPrevNetBufferList = pNextNetBufferList;
            pNextNetBufferList = pNextNetBufferList->Next;
        }
        NdisFreeCloneNetBufferList(pNextNetBufferList, NDIS_CLONE_FLAGS_USE_ORIGINAL_MDLS);
        pPrevNetBufferList->Next = NULL;
    }*/

    NdisFreeCloneNetBufferList(netBufferList, NDIS_CLONE_FLAGS_USE_ORIGINAL_MDLS);
    //netBufferList = NULL;
}