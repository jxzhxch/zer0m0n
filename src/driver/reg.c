////////////////////////////////////////////////////////////////////////////
//
//	zer0m0n DRIVER
//
//  Copyright 2013 Conix Security, Nicolas Correia, Adrien Chevalier
//
//  This file is part of zer0m0n.
//
//  Zer0m0n is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Zer0m0n is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Zer0m0n.  If not, see <http://www.gnu.org/licenses/>.
//
//
//	File :		reg.h
//	Abstract :	Registry callback handling.
//	Revision : 	v1.0
//	Author :	Adrien Chevalier & Nicolas Correia
//	Email :		adrien.chevalier@conix.fr nicolas.correia@conix.fr
//	Date :		2013-12-26	  
//	Notes : 	
//
/////////////////////////////////////////////////////////////////////////////
#include "reg.h"
#include "monitor.h"
#include "main.h"
#include "comm.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Description :
//		Registry callback. Logs any registry interaction performed by monitored processes.
//	Parameters :
//		See http://msdn.microsoft.com/en-us/library/windows/hardware/ff560903(v=vs.85).aspx
//	Return value :
//		See http://msdn.microsoft.com/en-us/library/windows/hardware/ff560903(v=vs.85).aspx
//	Process :
//		Checks the operation and logs the associated data.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
NTSTATUS regCallback (PVOID CallbackContext, PVOID Argument1, PVOID Argument2)
{
	NTSTATUS status, tmpCall;
	UNICODE_STRING full_data, test;
	PUNICODE_STRING tmp = NULL;
	PUNICODE_STRING valueName = NULL;
	PREG_SET_VALUE_KEY_INFORMATION arg = NULL;
	ULONG returnedLength = 0;
	PWCHAR pwBuf = NULL;
	PWCHAR tmp_data = NULL;
	ULONG pid = (ULONG)PsGetCurrentProcessId();
	DWORD i = 0;
	
	if(!isProcessMonitoredByPid(pid) || ExGetPreviousMode() == KernelMode)
		return STATUS_SUCCESS;
	
	pwBuf = ExAllocatePoolWithTag(NonPagedPool, (MAXSIZE+1)*sizeof(WCHAR), BUFFER_TAG);
	if(pwBuf == NULL)
		return STATUS_SUCCESS;
	
	tmp = ExAllocatePoolWithTag(NonPagedPool, MAXSIZE*sizeof(WCHAR), TEMP_TAG);
	if(tmp == NULL)
	{
		ExFreePool(tmp);
		return STATUS_SUCCESS;
	}
	
	switch((REG_NOTIFY_CLASS)Argument1)
	{	
		case RegNtPreDeleteKey:
			status = ObQueryNameString(((PREG_DELETE_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call DeleteKey() \n");
				#endif
				
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", tmp)))
					sendLogs(pid, L"REGISTRY_DELETE_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_DELETE_KEY", L"1,0,s,SubKey->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_DELETE_KEY", L"0,-1,s,SubKey->ERROR");
		break;
		
		case RegNtPreSetValueKey: 
			arg = (PREG_SET_VALUE_KEY_INFORMATION)Argument2; 
			status = ObQueryNameString(arg->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call SetValueKey() \n");
				#endif
				
				switch(arg->Type)
				{
					case REG_SZ:
						tmpCall = RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,sss,SubKey->%wZ,ValueName->%wZ,Data->%ws", tmp, arg->ValueName, ((PWCHAR)arg->Data));
					break;
					default :					
						tmp_data = ExAllocatePoolWithTag(NonPagedPool, MAXSIZE, 'gnaA');
						full_data.MaximumLength = MAXSIZE;
						full_data.Buffer = ExAllocatePoolWithTag(NonPagedPool, full_data.MaximumLength, 'baaH');
						RtlZeroMemory(full_data.Buffer, full_data.MaximumLength);
						for(i=0; i<arg->DataSize; i++)
						{
							if(i==100)
								break;
							RtlStringCchPrintfW(tmp_data, MAXSIZE, L"\\x%02x", *((PWCHAR)arg->Data+i));
							if(i==0)
							{
								RtlInitUnicodeString(&test, tmp_data);
								RtlCopyUnicodeString(&full_data, &test);
							}
							if(NT_SUCCESS(RtlAppendUnicodeToString(&full_data, tmp_data)))
								RtlZeroMemory(tmp_data, MAXSIZE);
						}
						tmpCall = RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,sss,SubKey->%wZ,ValueName->%wZ,Data->%wZ", tmp, arg->ValueName, &full_data);
						ExFreePool(full_data.Buffer);
						ExFreePool(tmp_data);
					break;	
				}
				if(NT_SUCCESS(tmpCall))
					sendLogs(pid, L"REGISTRY_VALUE_KEY_SET", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_VALUE_KEY_SET", L"1,0,sss,SubKey->ERROR,ValueName->ERROR,Data->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_VALUE_KEY_SET", L"1,0,sss,SubKey->ERROR,ValueName->ERROR,Data->ERROR");
		break;
		
		case RegNtPreDeleteValueKey:
			status = ObQueryNameString(((PREG_DELETE_VALUE_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call DeleteValueKey() !\n");
				#endif
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,ss,SubKey->%wZ,ValueName->%wZ", tmp, ((PREG_DELETE_VALUE_KEY_INFORMATION)Argument2)->ValueName)))
					sendLogs(pid, L"REGISTRY_VALUE_KEY_DELETE", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_VALUE_KEY_DELETE", L"1,0,ss,SubKey->ERROR,ValueName->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_VALUE_KEY_DELETE", L"1,0,ss,SubKey->ERROR,ValueName->ERROR");
		break;

		case RegNtPreRenameKey:
			status = ObQueryNameString(((PREG_RENAME_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call RenameKey() !\n");
				#endif
				
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,ss,SubKey->%wZ,NewName->%wZ", tmp, ((PREG_RENAME_KEY_INFORMATION)Argument2)->NewName)))
					sendLogs(pid, L"REGISTRY_KEY_RENAME", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_KEY_RENAME", L"1,0,ss,SubKey->ERROR,NewName->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_KEY_RENAME", L"1,0,ss,SubKey->ERROR,NewName->ERROR");
		break;
		
		case RegNtPreEnumerateKey:
			status = ObQueryNameString(((PREG_ENUMERATE_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call EnumerateKey() !\n");
				#endif
				
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", tmp)))
					sendLogs(pid, L"REGISTRY_ENUMERATE_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_ENUMERATE_KEY", L"1,0,s,SubKey->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_ENUMERATE_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreEnumerateValueKey:
			status = ObQueryNameString(((PREG_ENUMERATE_VALUE_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call EnumerateValueKey() !\n");
				#endif
				
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", tmp)))
					sendLogs(pid, L"REGISTRY_ENUMERATE_VALUE_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_ENUMERATE_VALUE_KEY", L"1,0,s,SubKey->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_ENUMERATE_VALUE_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreQueryKey:
			status = ObQueryNameString(((PREG_QUERY_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call QueryKey()! \n");
				#endif
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", tmp)))
					sendLogs(pid, L"REGISTRY_QUERY_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_QUERY_KEY", L"1,0,s,SubKey->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_QUERY_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreQueryValueKey:
			status = ObQueryNameString(((PREG_QUERY_VALUE_KEY_INFORMATION)Argument2)->Object, (POBJECT_NAME_INFORMATION)tmp, MAXSIZE, &returnedLength);
			if(NT_SUCCESS(status))
			{
				#ifdef DEBUG
				DbgPrint("call QueryValueKey() !\n");
				#endif
				
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,ss,SubKey->%wZ,ValueName->%wZ", tmp, ((PREG_QUERY_VALUE_KEY_INFORMATION)Argument2)->ValueName)))
					sendLogs(pid, L"REGISTRY_QUERY_VALUE_KEY", pwBuf);	
				else
					sendLogs(pid, L"REGISTRY_QUERY_VALUE_KEY", L"1,0,ss,SubKey->ERROR,ValueName->ERROR");
			}
			else
				sendLogs(pid, L"REGISTRY_QUERY_VALUE_KEY", L"1,0,ss,SubKey->ERROR,ValueName->ERROR");
		break;
		
		case RegNtPreCreateKey:
			#ifdef DEBUG
			DbgPrint("call CreateKey() !\n");
			#endif
			if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", ((PREG_PRE_CREATE_KEY_INFORMATION)Argument2)->CompleteName)))
				sendLogs(pid,L"REGISTRY_CREATE_KEY", pwBuf);
			else
				sendLogs(pid, L"REGISTRY_CREATE_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreCreateKeyEx:
			#ifdef DEBUG
			DbgPrint("call CreateKeyEx() !\n");
			#endif
			if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", ((PREG_PRE_CREATE_KEY_INFORMATION)Argument2)->CompleteName)))
				sendLogs(pid,L"REGISTRY_CREATE_KEY", pwBuf);
			else
				sendLogs(pid, L"REGISTRY_CREATE_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreOpenKey:
			#ifdef DEBUG
			DbgPrint("call OpenKey() !\n");
			#endif
			
			if(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer != NULL)
			{
				if(!_wcsicmp(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer, L"SOFTWARE\\Oracle\\VirtualBox Guest Additions") || !_wcsicmp(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer, L"SOFTWARE\\VMware, Inc.\\VMware Tools"))
				{
					if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"0,0,s,SubKey->%wZ", ((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName)))
						sendLogs(pid,L"REGISTRY_OPEN_KEY", pwBuf);
					return STATUS_OBJECT_NAME_NOT_FOUND;
				}
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", ((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName)))
					sendLogs(pid,L"REGISTRY_OPEN_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_OPEN_KEY", L"1,0,s,SubKey->ERROR");	
			}
			else
				sendLogs(pid, L"REGISTRY_OPEN_KEY", L"1,0,s,SubKey->ERROR");
		break;
		
		case RegNtPreOpenKeyEx:
			#ifdef DEBUG
			DbgPrint("call OpenKeyEx() !\n");
			#endif
			
			if(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer != NULL)
			{
				if(!_wcsicmp(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer, L"SOFTWARE\\Oracle\\VirtualBox Guest Additions") || !_wcsicmp(((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName->Buffer, L"SOFTWARE\\VMware, Inc.\\VMware Tools"))
				{
					if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"0,0,s,SubKey->%wZ", ((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName)))
						sendLogs(pid,L"REGISTRY_OPEN_KEY", pwBuf);
					return STATUS_OBJECT_NAME_NOT_FOUND;
				}
				if(NT_SUCCESS(RtlStringCchPrintfW(pwBuf, MAXSIZE, L"1,0,s,SubKey->%wZ", ((PREG_OPEN_KEY_INFORMATION)Argument2)->CompleteName)))
					sendLogs(pid,L"REGISTRY_OPEN_KEY", pwBuf);
				else
					sendLogs(pid, L"REGISTRY_OPEN_KEY", L"1,0,s,SubKey->ERROR");	
			}
			else
				sendLogs(pid, L"REGISTRY_OPEN_KEY", L"1,0,s,SubKey->ERROR");
		break;

		default:
		break;
	}
	
	ExFreePool(tmp);
	ExFreePool(pwBuf);
		
	return STATUS_SUCCESS;
 }
