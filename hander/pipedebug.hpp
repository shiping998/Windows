/*
 * This source code was published under GPL v3
 *
 * Copyright (C) 2016 Too-Naive E-mail:sometimes.naive@hotmail.com
 *
 */
#pragma once
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

extern const TCHAR * pipeName;
HANDLE hdPipe=INVALID_HANDLE_VALUE;
extern void ___Func_pipeCallBack(TCHAR const *);
namespace __Dpipe{
//for pipe debug
	const DWORD PIPE_TIMEOUT=5000;
	const size_t BUFSIZE=4096;
	typedef struct{
		OVERLAPPED oOverlap;
		HANDLE hPipeInst;
		TCHAR chRequest[BUFSIZE];
		DWORD cbRead;
		TCHAR chReply[BUFSIZE];
		DWORD cbToWrite;
	} PIPEINST, *LPPIPEINST;
//end
	HANDLE ___pipeopen();
	inline DWORD ___pipeclose();
	DWORD __stdcall OpenPipeService(LPVOID);
	DWORD ___pipesentmessage(const TCHAR *);
//pipe debug area
	void DisconnectAndClose(LPPIPEINST);
	BOOL CreateAndConnectInstance(LPOVERLAPPED);
	BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
	inline void GetAnswerToRequest(LPPIPEINST);
	void WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
	void WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);
//end.
	DWORD __stdcall OpenPipeService(LPVOID){
		HANDLE hConnectEvent;
		OVERLAPPED oConnect;
		LPPIPEINST lpPipeInst;
		DWORD dwWait, cbRet;
		BOOL fSuccess, fPendingIO;
		if (!(hConnectEvent = CreateEvent(NULL,TRUE,TRUE,NULL)))
			return 0*_tprintf(_T("CreateEvent failed with %ld.\n"), GetLastError());
		oConnect.hEvent = hConnectEvent;
		fPendingIO = CreateAndConnectInstance(&oConnect);
		while (1){
			dwWait = WaitForSingleObjectEx(hConnectEvent,INFINITE,TRUE);
			switch (dwWait){
			case 0:
				if (fPendingIO)
					if (!(fSuccess = GetOverlappedResult(hdPipe,&oConnect,&cbRet,FALSE)))
						return printf("ConnectNamedPipe (%ld)\n", GetLastError());
				if (!(lpPipeInst=(LPPIPEINST) HeapAlloc(GetProcessHeap(),0,sizeof(PIPEINST))))
					return printf("GlobalAlloc failed (%ld)\n", GetLastError());
				lpPipeInst->hPipeInst = hdPipe;
				lpPipeInst->cbToWrite = 0;
				CompletedWriteRoutine(0, 0, (LPOVERLAPPED) lpPipeInst);
				fPendingIO = CreateAndConnectInstance(&oConnect);
				break;
			case WAIT_IO_COMPLETION:
				break;
			default:
				return printf("WaitForSingleObjectEx (%ld)\n", GetLastError());
			}
		}
		return 0;
	}
	void WINAPI CompletedWriteRoutine(DWORD dwErr,DWORD cbWritten,LPOVERLAPPED lpOverLap){
		LPPIPEINST lpPipeInst;
		BOOL fRead = FALSE;
		lpPipeInst = (LPPIPEINST) lpOverLap;
		if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite)){
			fRead = ReadFileEx(lpPipeInst->hPipeInst,lpPipeInst->chRequest,
			BUFSIZE*sizeof(TCHAR),(LPOVERLAPPED) lpPipeInst,
			(LPOVERLAPPED_COMPLETION_ROUTINE) CompletedReadRoutine);
		}
		if (!fRead) DisconnectAndClose(lpPipeInst);
	}
	void WINAPI CompletedReadRoutine(DWORD dwErr,DWORD cbBytesRead,LPOVERLAPPED lpOverLap){
		LPPIPEINST lpPipeInst;
		BOOL fWrite = FALSE;
		lpPipeInst = (LPPIPEINST) lpOverLap;
		if ((dwErr == 0) && (cbBytesRead != 0)){
			GetAnswerToRequest(lpPipeInst);
			fWrite = WriteFileEx(lpPipeInst->hPipeInst,lpPipeInst->chReply,
				lpPipeInst->cbToWrite,(LPOVERLAPPED) lpPipeInst,
				(LPOVERLAPPED_COMPLETION_ROUTINE) CompletedWriteRoutine);
		}
		if (!fWrite) DisconnectAndClose(lpPipeInst);
	}
	void DisconnectAndClose(LPPIPEINST lpPipeInst){
		if (! DisconnectNamedPipe(lpPipeInst->hPipeInst))
			printf("DisconnectNamedPipe failed with %ld.\n", GetLastError());
		CloseHandle(lpPipeInst->hPipeInst);
		if (lpPipeInst != NULL)
			HeapFree(GetProcessHeap(),0, lpPipeInst);
	}
	BOOL CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
	{
		if (!(hdPipe = CreateNamedPipe(pipeName,PIPE_ACCESS_DUPLEX |FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE |	PIPE_READMODE_MESSAGE |	PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,BUFSIZE*sizeof(TCHAR),BUFSIZE*sizeof(TCHAR),
			PIPE_TIMEOUT,NULL)))
			return 0*(printf("CreateNamedPipe failed with %ld.\n", GetLastError()));
		return ConnectToNewClient(hdPipe, lpoOverlap);
	}
	BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
	{
		BOOL fConnected, fPendingIO = FALSE;
		if ((fConnected = ConnectNamedPipe(hPipe, lpo)))
			return 0*printf("ConnectNamedPipe failed with %ld.\n", GetLastError());
		switch (GetLastError()){
		case ERROR_IO_PENDING:
			fPendingIO = TRUE;
			break;
		case ERROR_PIPE_CONNECTED:
			if (SetEvent(lpo->hEvent))
				break;
		default:
				return 0*printf("ConnectNamedPipe failed with %ld.\n", GetLastError());
		}
		return fPendingIO;
	}
	inline void GetAnswerToRequest(LPPIPEINST pipe)
	{
		___Func_pipeCallBack(pipe->chRequest);
//		_tprintf( TEXT("%s"), pipe->chRequest);
		//reserved:
	/*	_tprintf( TEXT("[%p] %s"), pipe->hPipeInst, pipe->chRequest);
		lstrcpyn( pipe->chReply,  TEXT("") ,BUFSIZE);
		pipe->cbToWrite = (lstrlen(pipe->chReply)+1)*sizeof(TCHAR);*/
	}
	HANDLE ___pipeopen(){
		while (1){
			if ((hdPipe = CreateFile(pipeName,GENERIC_READ|GENERIC_WRITE,0,
				NULL,OPEN_EXISTING,0,NULL))!=INVALID_HANDLE_VALUE)
				break;
			if (GetLastError()!=ERROR_PIPE_BUSY) {
				Func_FastPMNTS(_T("%s Error! (%ld)\n"),_T("___pipeopen()"),GetLastError());
				return INVALID_HANDLE_VALUE;
			}
			WaitNamedPipe(pipeName, 2000);
		}
		return hdPipe;
	}
	extern DWORD ___OnError(const TCHAR *);
	DWORD ___pipesentmessage(const TCHAR * szSent){
		DWORD dwReserved=PIPE_READMODE_MESSAGE;
	    if (!SetNamedPipeHandleState(hdPipe,&dwReserved,NULL,NULL))
	    	___OnError(_T("WriteFile"));
//			Func_FastPMNTS(_T("SetNamedPipeHandleState() Error! (%ld)\n"),GetLastError());
	    if (!WriteFile(hdPipe,szSent,(lstrlen(szSent)+1)*sizeof(TCHAR),&dwReserved,NULL))
	    	___OnError(_T("WriteFile"));
//			Func_FastPMNTS(_T("WriteFile() Error! (%ld)\n"),GetLastError());
	    return GetLastError();
	}
	inline DWORD ___pipeclose(){
	    CloseHandle(hdPipe);
		return GetLastError();
	}
}
