/**
 * $Id: tunnel-client.cpp,v 1.8 2008/04/29 08:14:56 pensil Exp $
 * Copyright (c) 2008 Pensil - www.pensil.jp
 * 
 * MHP�p �g���l���N���C�A���g
 */
#define _BITTYPES_H

#include "tunnel-common.h"

#include <pcap.h>

#include <iostream>
#include <string.h>
#include <conio.h>

using namespace std;


// kBufferSize must be larger than the length of kpcEchoMessage.
const int kBufferSize = 4096;
#define maxiMacAddr 8

pcap_t *adhandle;
char errbuf[PCAP_ERRBUF_SIZE];

// INI�ݒ���e
TCHAR szServer[256];
TCHAR szPort[10];
TCHAR szDevice[256];
TCHAR szNickName[30];

struct USER_INFO {
	clock_t last;
	TCHAR szNickName[30];
};

// ��������T�[�o�[�Ƀg���l������MAC�A�h���X�̃��X�g
MAC_ADDRESS mac[maxMacAddr];

// �g���l�����Ȃ�MAC�A�h���X�̃��X�g(���[�v�h�~�̂���)
MAC_ADDRESS ignoreMac[maxiMacAddr];

// �T�[�o�[�ւ̐ڑ�
SOCKET_EX gsd;

u_long nRemoteAddress;
int nPort;

bool needReconnect = false;

/**
 * ��M�����p�P�b�g����������R�[���o�b�N�֐�
 * sdex - �ʐM���e
 * dh - ��M�f�[�^�w�b�_
 * data - ��M�f�[�^
 */
void DoCommand(SOCKET_EX * sdex, DATA_HEADER * dh, char * data)
{
	//printf("%d ���� '%c'�R�}���h (%d �o�C�g) ��M�B\n", (int)dh.doption, dh.dtype, (int)dh.dsize);
	if (dh->dtype == 't') {

		// Type �� 88 C8 �Ȃ�A������4��܂ŔF������
		int findMac = -1;
		for (int i = 0; i < maxiMacAddr; i++) {
			if (ignoreMac[i].used > 0) {
				if (memcmp((char *)&ignoreMac[i], &data[6], 6) == 0) {
					findMac = i;
					break;
				}
			}
		}
		if (findMac == -1) {
			for (int i = 0; i < maxiMacAddr; i++) {
				if (ignoreMac[i].used == 0) {
					memcpy((char *)&ignoreMac[i], &data[6], 6);
					ignoreMac[i].used = 1;
					printf("���uPSP�F��(%d): MAC�A�h���X = %0X:%0X:%0X:%0X:%0X:%0X\n", i + 1, ignoreMac[i].addr[0], ignoreMac[i].addr[1], ignoreMac[i].addr[2], ignoreMac[i].addr[3], ignoreMac[i].addr[4], ignoreMac[i].addr[5]);
					break;
				}
			}
		}

		pcap_sendpacket(adhandle, data, (int)dh->dsize);
	} else if (dh->dtype == 'c') {
		data[(int)dh->dsize] = 0;
		printf("%s (S%d->R%d S%d/R%d)\n", data, sdex->sendCount, dh->recvCount, SendPacketSize(sdex), RecvPacketSize(sdex));
    } else if (dh->dtype == 'P') {
    	dh->dtype = 'p';
        if (SendCommand(sdex, dh, data)) {
//			printf("ping����(%d/%d)\n", sdex->sendCount, sdex->recvCount);
        } else {
//			printf("ping�������s\n");
        }
//		printf("ping(S%d/%d R%d/%d)\n", dh->sendCount, sdex->sendCount, dh->recvCount ,sdex->recvCount);
	} else if (dh->dtype == 'p') {
//		data[(int)dh->dsize] = 0;
//		printf("ping(S%d->R%d) (Last: S%d R%d)\n", sdex->sendCount, dh->recvCount, SendPacketSize(sdex), RecvPacketSize(sdex));

	} else if (dh->dtype == 'u') {

		// ���[�U�[�m�F�R�}���h�ɑ΂��鉞��
		USER_INFO ui;
		memcpy(&ui.last, data, sizeof(clock_t));
		strcpy(ui.szNickName, szNickName);
		dh->dtype = 'I';
		dh->dsize = (short)sizeof(USER_INFO);
		SendCommand(sdex, dh, (char *)&ui);

	} else if (dh->dtype == 'i') {

		// ���[�U�[�m�F�R�}���h�̌��ʎ�M
		USER_INFO ui;
		memcpy(&ui, data, sizeof(USER_INFO));
		clock_t now = clock();
		printf("%d: %s (Ping: %d)\n", (int)dh->doption, ui.szNickName, (now - ui.last));

	} else {
		printf("�T�|�[�g�O�̃R�}���h'%c'(%d �o�C�g)\n", dh->dtype, (int)dh->dsize);
		CloseConnection(sdex);
	}
}

void DoClose(SOCKET_EX *)
{
	if (needReconnect == false) {
		return;
	}
	do {
		// �ؒf���̏���
		if (gsd.state != STATE_WAITTOCONNECT) {
//			printf("�Đڑ����܂�...\n");
		}
	    //gsd = EstablishConnection(&gsd, nRemoteAddress, htons(nPort));
	    if (!EstablishConnection(&gsd, nRemoteAddress, htons(nPort))) {
//	        cerr << WSAGetLastErrorMessage("�Đڑ����s�B") << 
//	                endl;
		}
	    Sleep(10);
	} while (gsd.state != STATE_CONNECTED);
}

/**
 * �R���\�[���ւ̓��͂𑗐M����X���b�h(�`���b�g�p)
 */ 
DWORD WINAPI ConsoleSender(void *) 
{
	SOCKET_EX * sdex = &gsd;
    //int nRetval = 0;
    char acReadBuffer[kBufferSize];
    int nReadBytes;
 
    DATA_HEADER dh;

	printf("\n");
    while (1) {
		fgets( acReadBuffer, kBufferSize , stdin );  /* �W�����͂������ */
		if (strcmp(acReadBuffer, "/logout\n") == 0) {
			
			// ���O�A�E�g�R�}���h

			needReconnect = false;

			char sendData[kBufferSize];
			snprintf(sendData, sizeof(sendData), "%s �����O�A�E�g���܂���", szNickName);
			dh.dtype = 'C';
			dh.dsize = (short) strlen(sendData);
			SendCommand(&gsd, &dh, sendData);

			printf("\n�ؒf���Ă��܂�...\n");
			CloseConnection(sdex);
			return 3;
		
		} else if (strcmp(acReadBuffer, "/users\n") == 0) {
			
			// Users�R�}���h

			clock_t pingClock = clock();
			char sendData[sizeof(clock_t)];
			
			dh.dtype = 'U';
			dh.dsize = sizeof(int);
			
			memcpy(sendData, (char *)&pingClock, sizeof(clock_t));
			
			printf("�ڑ����̃��[�U�[ : \n");
			SendCommand(&gsd, &dh, sendData);

		} else {
	
			// �`���b�g���M

			char sendData[kBufferSize];
			snprintf(sendData, sizeof(sendData), "<%s> %s", szNickName, acReadBuffer);

			dh.dtype = 'C';
			dh.dsize = (short) strlen(sendData) - 1;

//			if (strlen(acReadBuffer) > 1) {
				SendCommand(sdex, &dh, sendData);
//			}
		}
    };
}

/** 
 * WinPcap�ŃL���v�`���[�����p�P�b�g����������R�[���o�b�N�֐�
 */
void packet_handler(u_char *, const struct pcap_pkthdr *header, const u_char *pkt_data)
{
	int doSend = -1;
	for (int i = 0; i < maxMacAddr; i++) {
		if (mac[i].used > 0) {
			if (memcmp((char *)&mac[i], &pkt_data[6], 6) == 0) {
				doSend = i;
			}
		}
	}

	if ((int)pkt_data[12] == 136 && (int)pkt_data[13] == 200 && doSend == -1) {

		bool ignore = false;

		// Type �� 88 C8 �Ȃ�A������4��܂ŔF������
		for (int i = 0; i < maxiMacAddr; i++) {
			if (ignoreMac[i].used > 0) {
				if (memcmp((char *)&ignoreMac[i], &pkt_data[6], 6) == 0) {
//		printf("�X���[: to %0X:%0X:%0X:%0X:%0X:%0X from %0X:%0X:%0X:%0X:%0X:%0X %d �o�C�g\n", pkt_data[0], pkt_data[1], pkt_data[2], pkt_data[3], pkt_data[4], pkt_data[5], pkt_data[6], pkt_data[7], pkt_data[8], pkt_data[9], pkt_data[10], pkt_data[11], (int)header->len);
					ignore = true;
					break;
				}
			}
		}
		
		if (!ignore) {
			for (int i = 0; i < maxMacAddr; i++) {
				if (mac[i].used == 0) {
					memcpy((char *)&mac[i], &pkt_data[6], 6);
					mac[i].used = 1;
					doSend = i;
					printf("PSP�F��(%d): MAC�A�h���X = %0X:%0X:%0X:%0X:%0X:%0X\n", i + 1, mac[i].addr[0], mac[i].addr[1], mac[i].addr[2], mac[i].addr[3], mac[i].addr[4], mac[i].addr[5]);
					break;
				}
			}
		}
	}

	if (doSend > -1) {
		// ���M�������_�C���N�g�Ώۂł��A���M��܂Ń��_�C���N�g�ΏۂȂ�΁A����K�v�͖���
		for (int i = 0; i < maxMacAddr; i++) {
			if (mac[i].used > 0) {
				if (memcmp((char *)&mac[i], &pkt_data[0], 6) == 0) {
//					printf("���G���A�����M�̂��߃X���[ : PSP(%d) -> PSP(%d)\n", doSend + 1, i + 1);
					doSend = -1;
					break;
				}
			}
		}
	}

	if (doSend > -1) {
		
		DATA_HEADER dh;
		dh.dtype = 'T';
		dh.dsize = (short)header->len;

		do {
			if (SendCommand(&gsd, &dh, (char *)pkt_data)) {
//				printf("���M(%d) %d �o�C�g (S%d R%d)\n", doSend, header->len, gsd.sendCount, gsd.recvCount);
			} else {
//				printf("�ؒf�����̂ōĐڑ����܂��B\n");
			    //gsd = EstablishConnection(&gsd, nRemoteAddress, htons(nPort));
//			    if (!EstablishConnection(&gsd, nRemoteAddress, htons(nPort))) {
//			        cerr << WSAGetLastErrorMessage("�Đڑ����s�B") << 
//			                endl;
//				}
//		        Sleep(100);
			}
		} while (gsd.state != STATE_CONNECTED);
	} else {
//		printf("�X���[: to %0X:%0X:%0X:%0X:%0X:%0X from %0X:%0X:%0X:%0X:%0X:%0X %d �o�C�g\n", pkt_data[0], pkt_data[1], pkt_data[2], pkt_data[3], pkt_data[4], pkt_data[5], pkt_data[6], pkt_data[7], pkt_data[8], pkt_data[9], pkt_data[10], pkt_data[11], (int)header->len);
	}
}

DWORD WINAPI DoMain(void *)  
{
    printf("MHP Tunnel Client Ver%d.%d by Pensil\n\n", MAJOR_VERSION, MINOR_VERSION);

	GetSetting(_T("Server"), _T(""), szServer, sizeof(szServer));
	GetSetting(_T("Port"), _T("443"), szPort, sizeof(szPort));
	GetSetting(_T("Device"), _T(""), szDevice, sizeof(szDevice));
	GetSetting(_T("NickName"), _T(""), szNickName, sizeof(szNickName));
	
	while (strlen(szNickName) == 0) {
		InputFromUser("�j�b�N�l�[��: ", szNickName, sizeof(szNickName));
	}
	SetSetting(_T("NickName"), szNickName);

    // Get host and (optionally) port from the command line
    nPort = atoi(szPort);

	pcap_if_t *alldevs;
	pcap_if_t *d;
	int inum;
	int i=0;

	/* �f�o�C�X�ւ̃n���h�����쐬 */
	if ((adhandle= pcap_open_live(szDevice,	// name of the device
							 65536,			// portion of the packet to capture. 
											// 65536 grants that the whole packet will be captured on all the MACs.
							 1,				// promiscuous mode (nonzero means promiscuous)
							 100,			// read timeout
							 errbuf			// error buffer
							 )) == NULL)
	{
	    
		/* �@�탊�X�g�𓾂� */
		if(pcap_findalldevs(&alldevs, errbuf) == -1)
		{
			fprintf(stderr,"Error in pcap_findalldevs: %s\n", errbuf);
			exit(1);
		}
	
		/* �@�탊�X�g�̕\�� */
		for(d=alldevs; d; d=d->next)
		{
			printf("%d. %s", ++i, d->name);
			if (d->description)
				printf(" (%s)\n", d->description);
			else
				printf(" (No description available)\n");
		}
		
		if(i==0)
		{
			printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
			return -1;
		}
		
		printf("�g�p���閳���C���^�[�t�F�C�X�̔ԍ���I�����Ă������� (1-%d): ",i);
		char strTmp[32];
		InputFromUser("", strTmp, sizeof(strTmp));
		sscanf(strTmp, "%d", &inum);

		if(inum < 1 || inum > i)
		{
			printf("\nInterface number out of range.\n");
			/* Free the device list */
			pcap_freealldevs(alldevs);
			return -1;
		}
		
		/* Jump to the selected adapter */
		for(d=alldevs, i=0; i< inum-1 ;d=d->next, i++);
		
		/* Open the device */
		/* Open the adapter */
		if ((adhandle= pcap_open_live(d->name,	// name of the device
								 65536,			// portion of the packet to capture. 
												// 65536 grants that the whole packet will be captured on all the MACs.
								 1,				// promiscuous mode (nonzero means promiscuous)
								 100,			// read timeout
								 errbuf			// error buffer
								 )) == NULL)
		{
			fprintf(stderr,"\nUnable to open the adapter. %s is not supported by WinPcap\n", d->name);
			/* Free the device list */
			pcap_freealldevs(alldevs);
			return -1;
		}
			
		SetSetting(_T("Device") , d->name);
		/* At this point, we don't need any more the device list. Free it */
		pcap_freealldevs(alldevs);
	}
	
    // Find the server's address
    bool connected = false;
    do {
    	if (strlen(szServer) > 0) {
		    cout << "���O���������܂�..." << flush;
		    nRemoteAddress = LookupAddress(szServer);
		    if (nRemoteAddress == INADDR_NONE) {
		        cerr << endl << WSAGetLastErrorMessage("lookup address") << 
		                endl;
		    } else {
			    in_addr Address;
			    memcpy(&Address, &nRemoteAddress, sizeof(u_long)); 
			    cout << inet_ntoa(Address) << ":" << nPort << endl;
			    
			    // Connect to the server
			    cout << "�T�[�o�[�ɐڑ����ł�..." << flush;
			    if (!EstablishConnection(&gsd, nRemoteAddress, htons(nPort))) {
				    cerr << endl << WSAGetLastErrorMessage("�ڑ��Ɏ��s���܂����B") << endl;
			    } else {
			    	connected = true;
			    }
		    }
    	}
    	if (!connected) {
    		while (InputFromUser("�ڑ���T�[�o�[�A�h���X: ", szServer, sizeof(szServer)) == 0) {
    		}
    	}
    } while (!connected);
    cout << "�ڑ����܂����B\n���[�J���|�[�g�� " << gsd.sd << " �ł��B" << endl;

	printf("\n ���I������ɂ� /logout �Ɠ��͂��Ă�������\n\n");
    
    needReconnect = true;
    SetSetting(_T("Server"), szServer);
    
    Sleep(1);

    DATA_HEADER dh;
	char sendData[kBufferSize];
	snprintf(sendData, sizeof(sendData), "%s �����O�C�����܂���", szNickName);
	dh.dtype = 'C';
	dh.dsize = (short) strlen(sendData);
	SendCommand(&gsd, &dh, sendData);

    Sleep(100);

	printf("\n�ڑ����̃��[�U�[ :\n");
	dh.dtype = 'U';
	dh.dsize = (short) sizeof(clock_t);
	clock_t now = clock();
	memcpy(sendData, (char *)&now, sizeof(clock_t));
	SendCommand(&gsd, &dh, sendData);

    DWORD nThreadID;
    //HANDLE hEchoHandler = CreateThread(0, 0, EchoHandler, (void*)sd, 0, &nThreadID);
    CreateThread(0, 0, ConsoleSender, (void*)&gsd, 0, &nThreadID);
    //gsd = sd;
	
	bool usePcapLoop = false;
	if (usePcapLoop) {

		// WinPcap�� pcap_loop ���g�������B
		// �ǂ���Sleep(100)�������ɓ����Ă�C������̂ŁA�g��Ȃ������ŁB
		while (1) {
			/* start the capture */
			pcap_loop(adhandle, 0, packet_handler, NULL);
	  	//cout << "�ؒf����܂����B�Đڑ����܂��B" << endl;
		}

	} else {

		// WinPcap�� pcap_next_ex ���g������
		// Sleep(0)��������ĂȂ����Ǒ��v���ȁE�E�E
		int ret;
		const u_char *pkt_data;
		struct pcap_pkthdr *pkt_header;
		while(needReconnect) {
			if(0<=(ret=pcap_next_ex(adhandle,&pkt_header, &pkt_data))){
			    if(ret==1){
			    	//�p�P�b�g���؂�Ȃ��ǂݍ��܂ꂽ
			    	// ���̎��_�� pkt_header->caplen ��
			        // ��M�o�C�g�� pkt_data �Ɏ�M�����f�[�^���܂܂�Ă��܂��B
					packet_handler(NULL, pkt_header, pkt_data);
					Sleep(0);
			    }
			}
		};
	}
	return 0;
}
