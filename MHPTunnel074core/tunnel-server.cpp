/**
 * $Id: tunnel-server.cpp,v 1.24 2008/06/06 11:38:37 pensil Exp $
 * Copyright (c) 2008 Pensil - www.pensil.jp
 * 
 * MHP�p �g���l���T�[�o�[�\�t�g
 */
#define DLLAPI extern "C" __declspec(dllexport)

#include "tunnel-server.h"
#include "tunnel-common.h"

extern bool LocalConnect(SOCKET_EX *);

#include <iostream>

//#include <tlhelp32.h>
#include <conio.h>

using namespace std;


const int kBufferSize = 32767;

//TCHAR szServer[256];
//TCHAR szPort[10];

const int maxOfSessions = 100;
const int maxOfUsers = 100;

SOCKET_EX sessions[maxOfSessions];

void AcceptConnections(SOCKET ListeningSocket);
bool EchoIncomingPackets(SOCKET_EX* sdex);
int GetIdolSession(void);

char sb[4096];
char macchar[64];
bool useSSID = true;

void DefaultServerLogHandler(const char *)
{
}

server_log_handler slog2 = DefaultServerLogHandler;

void slog(const char *logText)
{
	ServerLogOut(logText);
	slog2(logText);
}

DLLAPI void _stdcall SetServerLogHandler(server_log_handler handler)
{
	slog2 = handler;
}

void PrintMac(MAC_ADDRESS* mac)
{
	snprintf(macchar, sizeof(macchar), "%02X:%02X:%02X:%02X:%02X:%02X", mac->addr[0], mac->addr[1], mac->addr[2], mac->addr[3], mac->addr[4], mac->addr[5]);
}

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
 * ��M�R�}���h�ɑ΂��鏈��
 */
void doServerCommand(SOCKET_EX * sdex, const DATA_HEADER * _dh, const char * _data)
{
	DATA_HEADER rdh;
	DATA_HEADER * dh = &rdh;
	memcpy(dh, _dh, sizeof(DATA_HEADER));
	char data[4096];
	memcpy(data, _data, (int)dh->dsize);
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
					snprintf(sb, sizeof(sb), "%d �� %d �ɓ]�����s�B�ؒf���ꂽ�����B (%d�o�C�g)", sdex->number, i, (int)dh->dsize);
					slog(sb);
					//sessions[i].type = 0;
                    //return false;
                }
	        }
        }

		data[(int)dh->dsize] = 0;
		snprintf(sb, sizeof(sb), "%d: %s (S%d->R%d) (Last: S%d R%d)", (int)dh->doption , data, sdex->sendCount, dh->recvCount, SendPacketSize(sdex), RecvPacketSize(sdex));
		slog(sb);

    } else if (dh->dtype == 'D') {
		// �R�}���h D:tell �ɑ�������R�}���h
		// �w�肵������ɂ̂ݒʒm����
		
		char commandType[30];
		char tellTo[30];
		char text[4096];
		char buffer[4096];
		data[(int)dh->dsize] = 0;
		sscanf(data, "/%s %s %4096s", commandType, tellTo, text);
		
		dh->doption = (char)sdex->number;
		dh->dtype = 'c';
    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {

			// �����̑��M���W�b�N�̓X���b�h������ׂ����Ǝv�����E�E�E
			// ���Ă݂�
			if (sessions[i].state == STATE_CONNECTED) {

				if (strcmp(sessions[i].info.szNickName, tellTo) == 0) {

					snprintf(buffer, sizeof(buffer), "%s << %s", tellTo, text);
					dh->dsize = strlen(buffer);
			        SendCommand(sdex, dh, buffer);

					snprintf(buffer, sizeof(buffer), "%s >> %s", sdex->info.szNickName, text);
					dh->dsize = strlen(buffer);
		            SendCommand(&sessions[i], dh, buffer);
					snprintf(sb, sizeof(sb), "%s >> %s", sdex->info.szNickName, text);
					slog(sb);
				}
	        }
        }

    } else if (dh->dtype == 'Y') {
		// �R�}���h Y:party �ɑ�������R�}���h
		// �w�肵������ɂ̂ݒʒm����
		
		char commandType[30];
		//char tellTo[30];
		char text[4096];
		data[(int)dh->dsize] = 0;
		sscanf(data, "/%s %4096s", commandType, text);
		
		dh->doption = (char)sdex->number;
		dh->dtype = 'c';
    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {

			// �����̑��M���W�b�N�̓X���b�h������ׂ����Ǝv�����E�E�E
			// ���Ă݂�
			if (sessions[i].state == STATE_CONNECTED) {

				if (strcmp(sessions[i].info.szSSID, sdex->info.szSSID) == 0) {

					char buffer[4096];
					snprintf(buffer, sizeof(buffer), "(%s) %s", sdex->info.szNickName, text);
					dh->dsize = strlen(buffer);
		            SendCommand(&sessions[i], dh, buffer);
					snprintf(sb, sizeof(sb), "(%s) %s", sdex->info.szNickName, text);
					slog(sb);
				}
	        }
        }

    } else if (dh->dtype == 'T') {
		// �R�}���h T:�g���l���p�P�b�g
		// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
		dh->doption = (char)sdex->number;
		dh->dtype = 't';
    
		// ���M����MAC�A�h���X�����X�g�ɒǉ�����
		findMac = -1;
		for (int i = 0; i < maxMacAddr; i++) {
			if (sdex->mac[i].active) {
				if (memcmp((char *)&sdex->mac[i], &data[6], 6) == 0) {
					findMac = i;
					break;
				}
			}
		}
		if (findMac == -1) {
			for (int i = 0; i < maxMacAddr; i++) {
				if (!sdex->mac[i].active) {
					memcpy((char *)&sdex->mac[i], &data[6], 6);
					sdex->mac[i].active = true;
					PrintMac(&sdex->mac[i]);
					snprintf(sb, sizeof(sb), "PSP�F��(%d %d): MAC�A�h���X = %s", sdex->number, i, macchar);
					slog(sb);
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

				// SSID�����������m�Ƀp�P�b�g���g���l������
				if (!useSSID || strcmp(sdex->info.szSSID, sessions[i].info.szSSID) == 0) {
					// �܂��A�u���[�h�L���X�g���M�����_�C���N�g����
					if (memcmp(&bc, data, 6) == 0) {
						yesSend = true;
					} else {
						
						// ���M���MAC�A�h���X�����X�g�ɑ��݂��邩�m�F����
						for (int m = 0; m < maxMacAddr; m++) {
							if (sessions[i].mac[m].active) {
								if (memcmp((char *)&sessions[i].mac[m], data, 6) == 0) {
									yesSend = true;
									break;
								}
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
						snprintf(sb, sizeof(sb), "%d �� %d �ɓ]�����s�B�ؒf���ꂽ�����B (%d�o�C�g)", sdex->number, i, (int)dh->dsize);
						slog(sb);
						//sessions[i].type = 0;
	                    //return false;
	                }
	            }
	        }
        }

    } else if (dh->dtype == 'A') {

		// �R�}���h A:�V���[�U�[�ꗗ�m�F�R�}���h
		dh->dtype = 'a';		
		int count = 0;    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {
			if (sessions[i].state == STATE_CONNECTED) {
				count++;
	        }
        }        
        dh->doption = (char)count;
        dh->dsize = sizeof(USER_INFO) * count;
        USER_INFO * ui;
        ui = (USER_INFO *)malloc(sizeof(USER_INFO)*count);    
    	count = 0;
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {
			if (sessions[i].state == STATE_CONNECTED) {				
				memcpy(&ui[count], &sessions[i].info, sizeof(USER_INFO));
				count++;
	        }
        }        
        SendCommand(sdex, dh, (char *)ui);
        free(ui);
 
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

    } else if (dh->dtype == 'S' || dh->dtype == 'I') {
    	
    	int ping = sdex->info.ping;
		memcpy(&sdex->info, data, sizeof(USER_INFO));
    	sdex->info.ping = ping;
		if (sdex->info.majorVersion != MAJOR_VERSION || sdex->info.minorVersion != MINOR_VERSION) {
			DATA_HEADER dht;
	    	dht.dtype = 'c';
	    	char serverComment[512];
	    	snprintf(serverComment, sizeof(serverComment), "�o�[�W��������v���܂���: �T�[�o�[Ver%d.%d �N���C�A���gVer%d.%d", MAJOR_VERSION, MINOR_VERSION, sdex->info.majorVersion, sdex->info.minorVersion);
	    	dht.dsize = strlen(serverComment);
	        SendCommand(sdex, &dht, serverComment);
		}

		if (dh->dtype == 'I') {	
			// �R�}���h U:���[�U�[�ꗗ�m�F�R�}���h
			// ��M�p�P�b�g�����̂܂܃��_�C���N�g����
			int i = (int)dh->doption;
			dh->dtype = 'i';
			dh->doption = (char)sdex->number;
			
			if (sessions[i].state == STATE_CONNECTED) {
				SendCommand(&sessions[i], dh, data);
	        }
		} else {
	    	// �L���ȃZ�b�V�����S����Ώۂɂ���
//			snprintf(sb, sizeof(sb), "%d: %s SSID ->%s USERID ->%s", sdex->number, sdex->info.szNickName, sdex->info.szSSID, sdex->info.szUID);
//			slog(sb);

			// �R�}���h A:�V���[�U�[�ꗗ�m�F�R�}���h
			dh->dtype = 'a';		
			int count = 0;    
	    	// �L���ȃZ�b�V�����S����Ώۂɂ���
	    	for (int i = 0; i < maxOfSessions; i++) {
				if (sessions[i].state == STATE_CONNECTED) {
					count++;
		        }
	        }        
	        USER_INFO * ui;
	        ui = (USER_INFO *)malloc(sizeof(USER_INFO)*count);    
	        dh->doption = (char)count;
	        dh->dsize = sizeof(USER_INFO) * count;
	    	count = 0;
	    	// �L���ȃZ�b�V�����S����Ώۂɂ���
	    	for (int i = 0; i < maxOfSessions; i++) {
				if (sessions[i].state == STATE_CONNECTED) {				
					memcpy(&ui[count], &sessions[i].info, sizeof(USER_INFO));
					count++;
		        }
	        }

	    	// �L���ȃZ�b�V�����S����Ώۂɂ���
	    	for (int i = 0; i < maxOfSessions; i++) {
	
				if (sessions[i].state == STATE_CONNECTED) {
	
			        SendCommand(&sessions[i], dh, (char *)ui);
		        }
	        }

	        free(ui);
		}

    } else if (dh->dtype == 'P') {
    	dh->dtype = 'p';
        if (SendCommand(sdex, dh, data)) {
//			printf("ping����(%d/%d)\n", sdex->sendCount, sdex->recvCount);
        } else {
//			printf("ping�������s\n");
        }
	} else if (dh->dtype == 'p') {
//		if ((int)dh->dsize == sizeof(clock_t)) {
		clock_t pingClock;
		memcpy((char *)&pingClock, data, sizeof(int));
		sdex->info.ping = clock() - pingClock;
//		snprintf(sb, sizeof(sb), "now:%d ping:%d result:%d sdex->info.ping:%d", clock(), pingClock, (clock() - pingClock), sdex->info.ping);
//		slog(sb);
//		}

		// �R�}���h A:�V���[�U�[�ꗗ�m�F�R�}���h
		dh->dtype = 'a';		
		int count = 0;    
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {
			if (sessions[i].state == STATE_CONNECTED) {
				count++;
	        }
        }        
        dh->doption = (char)count;
        dh->dsize = sizeof(USER_INFO) * count;
        USER_INFO * ui;
        ui = (USER_INFO *)malloc(sizeof(USER_INFO)*count);    
    	count = 0;
    	// �L���ȃZ�b�V�����S����Ώۂɂ���
    	for (int i = 0; i < maxOfSessions; i++) {
			if (sessions[i].state == STATE_CONNECTED) {				
				memcpy(&ui[count], &sessions[i].info, sizeof(USER_INFO));
				count++;
	        }
        }        
        SendCommand(sdex, dh, (char *)ui);
        free(ui);

//		printf("%d : ping(S%d->R%d S%d->R%d)\n", sdex->number , sdex->sendCount, dh->recvCount, dh->sendCount ,sdex->recvCount);
    } else {
		snprintf(sb, sizeof(sb), " �� �T�|�[�g�O�̃R�}���h [%c]", dh->dtype);
		slog(sb);
        //return false;
    }
}

void doServerConnect(SOCKET_EX *)
{
}

void doServerClose(SOCKET_EX * sdex)
{
	DATA_HEADER dh;
	char data[256];
	
	snprintf(data, sizeof(data), "%d ���ؒf���܂���", sdex->number);
	dh.dtype = 'z';
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

DWORD WINAPI DoListening(void * _Socket)  
{
	SOCKET ListeningSocket = * ((SOCKET *)_Socket);
    if (ListeningSocket == INVALID_SOCKET) {
        //printf("\n%s\n", WSAGetLastErrorMessage("���X�i�[���������G���["));
        return 3;
    }

	// �Ђ�����ڑ��҂�
    while (1) {
	    slog("�ڑ���҂��Ă��܂�...");
		int i = GetIdolSession();
		while(i == -1) {
			// �A�C�h���Z�b�V�������Ȃ��̂� 5�b�҂��Ă݂�
			Sleep(5000);
			i = GetIdolSession();
		};
		if (AcceptConnection(&sessions[i], ListeningSocket)) {
			snprintf(sb, sizeof(sb), "�ڑ��Z�b�V�����ԍ� : %d", i);
			slog(sb);
//            cout << "�N���C�A���g " <<
//                    inet_ntoa(sessions[i].sinRemote.sin_addr) << ":" <<
//                    ntohs(sessions[i].sinRemote.sin_port) << "." <<
//                    endl;
        }
        else {
        	snprintf(sb, sizeof(sb), "%s", WSAGetLastErrorMessage("accept() failed"));
        	slog(sb);
        }
    }
}

HANDLE hThreads[20];
SOCKET listenSocket[20];
int numOfThread;
ARENA * _arena;

DLLAPI bool _stdcall OpenServer(struct ARENA * arena)
{
	_arena = arena;

	int nPort = arena->port;
	if (!InitTunnel()) {
		return false;
	}

	int port = htons(nPort);
//	slog("471:OpenServer");

    // �Z�b�V�������X�g�̏�����
	for (int i = 0; i < maxOfSessions; i++) {
		sessions[i].sd = INVALID_SOCKET;
		sessions[i].state = STATE_IDOL;
		sessions[i].number = i;
		sessions[i].doClose = doServerClose;
		sessions[i].doCommand = doServerCommand;
		sessions[i].doConnect = doServerConnect;
	}

	// �����̃z�X�g���𓾂�
//	slog("484:OpenServer");
	char ac[80];
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {

    	// �����̃z�X�g�����擾�ł��܂���ł���
    	snprintf(sb, sizeof(sb), "���[�J���z�X�g���� %s �ł��B", ac);
    	slog(sb);
        return 1;
    }

    // �����̃z�X�g������A������IP�A�h���X�𓾂�
//	slog("495:OpenServer");
    struct hostent *phe = gethostbyname(ac);
    if (phe == 0) {

    	// ������IP�A�h���X���擾�ł��܂���ł���
        return 2;
    }

    // �S�Ă�IP�A�h���X�ő҂��󂯂�
//	slog("504:OpenServer");
    numOfThread = 0;
    for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
        listenSocket[numOfThread] = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket[numOfThread] != INVALID_SOCKET) {
            sockaddr_in sinInterface;
            sinInterface.sin_family = AF_INET;
		    struct in_addr addr;
		    memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
	    	memcpy(&sinInterface.sin_addr.s_addr, phe->h_addr_list[i], sizeof(struct in_addr));
            sinInterface.sin_port = port;
	    	snprintf(sb, sizeof(sb), "�A�h���X�� %s:%d �ł��B", inet_ntoa(addr), nPort);
	    	slog(sb);
            if (bind(listenSocket[numOfThread], (sockaddr*)&sinInterface, 
                    sizeof(sockaddr_in)) != SOCKET_ERROR) {
                listen(listenSocket[numOfThread], SOMAXCONN);
                hThreads[numOfThread] = CreateThread(0, 0, DoListening, (void*)&listenSocket[numOfThread], 0, NULL);
                numOfThread++;
            } else {
            	// bind���s
            }
        }
        Sleep(100);
    }

//	slog("528:OpenServer");
	return 0;
}

DLLAPI void _stdcall WaitServer()
{
    // http://msdn.microsoft.com/ja-jp/library/cc429423.aspx
	WaitForMultipleObjects(numOfThread, hThreads, true, INFINITE);
}

DLLAPI bool _stdcall CloseServer()
{
	for (int i = 0; i < numOfThread; i++) {
		TerminateThread(hThreads[numOfThread], 3);
	}
	return true;
}

bool LocalConnect(SOCKET_EX * sdex)
{
	int i = GetIdolSession();
	if(i == -1) {
		return false;
	};
	InitConnection(&sessions[i], 256);
	sessions[i].doClose = doServerClose;
	sessions[i].doCommand = doServerCommand;
	sessions[i].doConnect = doServerConnect;
	
	sessions[i].localConnect = sdex;
	sdex->localConnect = &sessions[i];
    StartConnection(&sessions[i]);
    StartConnection(sdex);
	return true;
}

void UseSSIDFilter(bool value)
{
	useSSID = value;
}

DLLAPI void _stdcall GetServerSetting(struct ARENA * arena)
{
	char szPort[10];
	GetSetting("Port" , "443", szPort , 10);

	// �|�[�g�ԍ�
    arena->port = atoi(szPort);
    arena->majorVersion = MAJOR_VERSION;
    arena->minorVersion = MINOR_VERSION;
    
}