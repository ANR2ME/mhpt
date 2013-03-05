/**
 * $Id: tunnel-server.cpp,v 1.7 2008/04/29 08:14:56 pensil Exp $
 * Copyright (c) 2008 Pensil - www.pensil.jp
 * 
 * MHP�p �g���l���T�[�o�[�\�t�g
 */
#include "tunnel-common.h"

#include <iostream>

#include <tlhelp32.h>
#include <conio.h>

using namespace std;


const int kBufferSize = 32767;

TCHAR szServer[256];
TCHAR szPort[10];

const int maxOfSessions = 64;
const int maxOfUsers = 12;

struct USER_EX {
	int no;
	char userName[32];
	int session;
	int status;
	int room;
};

SOCKET_EX sessions[maxOfSessions];
USER_EX users[maxOfUsers];

SOCKET SetUpListener(const char* pcAddress, int nPort);
void AcceptConnections(SOCKET ListeningSocket);
bool EchoIncomingPackets(SOCKET_EX* sdex);
int GetIdolSession(void);

/**
 * �󂫃Z�b�V�������擾����
 */
int GetIdolSession(void) {
	for (int i = 0; i < maxOfSessions; i++) {
		if (sessions[i].state == STATE_IDOL) {
			return i;
		}
	}
	return -1;
}

/**
 * ���X�i�[�̏�����
 */
SOCKET SetUpListener(const char* pcAddress, int nPort)
{
    u_long nInterfaceAddr = inet_addr(pcAddress);
    if (nInterfaceAddr != INADDR_NONE) {
        SOCKET sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd != INVALID_SOCKET) {
            sockaddr_in sinInterface;
            sinInterface.sin_family = AF_INET;
            sinInterface.sin_addr.s_addr = nInterfaceAddr;
            sinInterface.sin_port = nPort;
            if (bind(sd, (sockaddr*)&sinInterface, 
                    sizeof(sockaddr_in)) != SOCKET_ERROR) {
                listen(sd, SOMAXCONN);
                return sd;
            }
            else {
                cerr << WSAGetLastErrorMessage("bind() failed") <<
                        endl;
            }
        }
    }

    return INVALID_SOCKET;
}


/**
 * ��M�R�}���h�ɑ΂��鏈��
 */
void DoCommand(SOCKET_EX* sdex, DATA_HEADER * dh, char * data)
{
	static const u_char bc[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

	int findMac;

	//printf("%d ���� '%c'�R�}���h (%d �o�C�g) ��M�B\n", sdex->number, dh->dtype, (int)dh->dsize);
    if (dh->dtype == 'C') {
		// �R�}���h C:�`���b�g T:�g���l���p�P�b�g
		// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
		dh->doption = (char)sdex->number;
		dh->dtype = 'c';
    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {

			// �����̑��M���W�b�N�̓X���b�h������ׂ����Ǝv�����E�E�E
			// ���Ă݂�
			if (sessions[i].state == STATE_CONNECTED) {

				// �R�}���h���M
                //if (SendCommandByThread(sessions[i].sd, &dh, acReadBuffer)) {
	            //int nTemp = SendCommand(sessions[i].sd, &dh, data);
	            if (SendCommand(&sessions[i], dh, data)) {
					//printf("%d �� %d �ɓ]�� (%d�o�C�g)\n", sdex->number, i, (int)dh->dsize);
					//sessions[i].sendSize += nReadBytes;
                } else {
					printf("%d �� %d �ɓ]�����s�B�ؒf���ꂽ�����B (%d�o�C�g)\n", sdex->number, i, (int)dh->dsize);
					//sessions[i].type = 0;
                    //return false;
                }
	        }
        }

		data[(int)dh->dsize] = 0;
		printf("%d: %s (S%d->R%d) (Last: S%d R%d)\n", (int)dh->doption , data, sdex->sendCount, dh->recvCount, SendPacketSize(sdex), RecvPacketSize(sdex));

    } else if (dh->dtype == 'T') {
		// �R�}���h T:�g���l���p�P�b�g
		// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
		dh->doption = (char)sdex->number;
		dh->dtype = 't';
    
		// ���M����MAC�A�h���X�����X�g�ɒǉ�����
		findMac = -1;
		for (int i = 0; i < maxMacAddr; i++) {
			if (sdex->mac[i].used > 0) {
				if (memcmp((char *)&sdex->mac[i], &data[6], 6) == 0) {
					findMac = i;
					break;
				}
			}
		}
		if (findMac == -1) {
			for (int i = 0; i < maxMacAddr; i++) {
				if (sdex->mac[i].used == 0) {
					memcpy((char *)&sdex->mac[i], &data[6], 6);
					sdex->mac[i].used = 1;
					printf("PSP�F��(%d %d): MAC�A�h���X = ", sdex->number, i);
					PrintMac(&sdex->mac[i]);
					printf("\n");
					break;
				}
			}
		}

    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {

			// �����̑��M���W�b�N�̓X���b�h������ׂ����Ǝv�����E�E�E
			// ���Ă݂�
			if (sessions[i].state == STATE_CONNECTED && sessions[i].number != sdex->number) {

				bool yesSend = false;

				// �܂��A�u���[�h�L���X�g���M�����_�C���N�g����
				if (memcmp(&bc, data, 6) == 0) {
					yesSend = true;
				} else {
					
					// ���M���MAC�A�h���X�����X�g�ɑ��݂��邩�m�F����
					for (int m = 0; m < maxMacAddr; m++) {
						if (sessions[i].mac[m].used > 0) {
							if (memcmp((char *)&sessions[i].mac[m], data, 6) == 0) {
								yesSend = true;
								break;
							}
						}
					}
				}
				
				// �R�}���h���M
                //if (SendCommandByThread(sessions[i].sd, &dh, acReadBuffer)) {
	            //int nTemp = SendCommand(sessions[i].sd, &dh, data);
	            if (yesSend) {
		            if (SendCommand(&sessions[i], dh, data)) {
//						printf("%d �� %d �ɓ]�� (%d�o�C�g)\n", sdex->number, i, (int)dh->dsize);
						//sessions[i].sendSize += nReadBytes;
	                } else {
						printf("%d �� %d �ɓ]�����s�B�ؒf���ꂽ�����B (%d�o�C�g)\n", sdex->number, i, (int)dh->dsize);
						//sessions[i].type = 0;
	                    //return false;
	                }
	            }
	        }
        }

    } else if (dh->dtype == 'U') {

		// �R�}���h U:���[�U�[�ꗗ�m�F�R�}���h
		// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
		dh->doption = (char)sdex->number;
		dh->dtype = 'u';
    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {

			// �����̑��M���W�b�N�̓X���b�h������ׂ����Ǝv�����E�E�E
			// ���Ă݂�
			if (sessions[i].state == STATE_CONNECTED) {

	            SendCommand(&sessions[i], dh, data);
	        }
        }

    } else if (dh->dtype == 'I') {

		// �R�}���h U:���[�U�[�ꗗ�m�F�R�}���h
		// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
		int i = (int)dh->doption;
		dh->dtype = 'i';
		dh->doption = (char)sdex->number;
		
		if (sessions[i].state == STATE_CONNECTED) {
			SendCommand(&sessions[i], dh, data);
        }

    } else if (dh->dtype == 'P') {
    	dh->dtype = 'p';
        if (SendCommand(sdex, dh, data)) {
//			printf("ping����(%d/%d)\n", sdex->sendCount, sdex->recvCount);
        } else {
//			printf("ping�������s\n");
        }
	} else if (dh->dtype == 'p') {
		data[(int)dh->dsize] = 0;
		printf("%d : ping(S%d->R%d S%d->R%d)\n", sdex->number , sdex->sendCount, dh->recvCount, dh->sendCount ,sdex->recvCount);
    } else {
		printf(" �� �T�|�[�g�O�̃R�}���h [%c]\n", dh->dtype);
        //return false;
    }
}

void DoClose(SOCKET_EX * sdex)
{
	DATA_HEADER dh;
	char data[256];
	
	snprintf(data, sizeof(data), "%d ���ؒf���܂���", sdex->number);
	dh.dtype = 'c';
	dh.dsize = (short)strlen(data);

	// �L���ȃZ�b�V�����S����Ώۂɂ���
	for (int i = 0; i < maxOfSessions; i++) {

		if (sessions[i].state == STATE_CONNECTED) {

			// �R�}���h���M
            //if (SendCommand(&sessions[i], &dh, data)) {
            //}
        }
	}
}

DWORD WINAPI DoMain(void *)  
{
    printf("MHP Tunnel Server Ver%d.%d by Pensil\n\n", MAJOR_VERSION, MINOR_VERSION);

	GetSetting(_T("Server"), _T(""), szServer , 256);
	GetSetting(_T("Port") , _T("443"), szPort , 10);

	// �|�[�g�ԍ�
    int nPort = atoi(szPort);

    // �Z�b�V�������X�g�̏�����
	for (int i = 0; i < maxOfSessions; i++) {
		sessions[i].sd = INVALID_SOCKET;
		sessions[i].state = STATE_IDOL;
		sessions[i].number = i;
	}

    // ���X�i�[�̏�����
    printf("���X�i�[�����������Ă��܂�...\n");
    SOCKET ListeningSocket = SetUpListener(szServer, htons(nPort));
    if (ListeningSocket == INVALID_SOCKET) {
        printf("\n%s\n", WSAGetLastErrorMessage("���X�i�[���������G���["));
        return 3;
    }

	// �Ђ�����ڑ��҂�
    while (1) {
	    printf("�ڑ���҂��Ă��܂�...\n");
		int i = GetIdolSession();
		while(i == -1) {
			// �A�C�h���Z�b�V�������Ȃ��̂� 5�b�҂��Ă݂�
			Sleep(5000);
			i = GetIdolSession();
		};
		if (AcceptConnection(&sessions[i], ListeningSocket)) {
			printf("�ڑ��Z�b�V�����ԍ� : %d\n", i);
            cout << "�N���C�A���g " <<
                    inet_ntoa(sessions[i].sinRemote.sin_addr) << ":" <<
                    ntohs(sessions[i].sinRemote.sin_port) << "." <<
                    endl;
        }
        else {
        	printf("%s", WSAGetLastErrorMessage("accept() failed"));
        }
    }
}
