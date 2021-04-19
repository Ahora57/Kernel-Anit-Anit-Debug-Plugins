#pragma once
#include "NewFunc.h"

#include "Get_SSDT.h"


NewFunc *NewFunc::_This = nullptr;

bool NewFunc::Init(Message_Init *message)
{
	if (_Init == true)
	{
		return true;
	}
	//��ʼ��SSDT����
	NtProtectVirtualMemory = (_NtProtectVirtualMemory)GetSSDTFuncCurAddrByIndex(NtSysAPI_SSDT_NtProtectVirtualMemory);
	if (NtProtectVirtualMemory == nullptr)
	{
		return false;
	}

	//��ʼ�����ź���
	DbgkpWakeTarget = (_DbgkpWakeTarget)message->DbgkpWakeTarget;
	PsResumeThread = (_PsResumeThread)message->PsResumeThread;
	PsSuspendThread = (_PsSuspendThread)message->PsSuspendThread;
	PsGetNextProcessThread = (_PsGetNextProcessThread)message->PsGetNextProcessThread;
	DbgkpSectionToFileHandle = (_DbgkpSectionToFileHandle)message->DbgkpSectionToFileHandle;
	MmGetFileNameForAddress = (_MmGetFileNameForAddress)message->MmGetFileNameForAddress;
	KiDispatchException = (_KiDispatchException)message->KiDispatchException;
	DbgkForwardException = (_DbgkForwardException)message->DbgkForwardException;
	DbgkpSuspendProcess = (_DbgkpSuspendProcess)message->DbgkpSuspendProcess;
	KeThawAllThreads = (_KeThawAllThreads)message->KeThawAllThreads;
	DbgkCreateThread = (_DbgkCreateThread)message->DbgkCreateThread;
	DbgkMapViewOfSection = (_DbgkMapViewOfSection)message->DbgkMapViewOfSection;
	DbgkUnMapViewOfSection = (_DbgkUnMapViewOfSection)message->DbgkUnMapViewOfSection;
	NtCreateUserProcess = (_NtCreateUserProcess)message->NtCreateUserProcess;
	DbgkpMarkProcessPeb = (_DbgkpMarkProcessPeb)message->DbgkpMarkProcessPeb;
	DbgkpSuppressDbgMsg = (_DbgkpSuppressDbgMsg)message->DbgkpSuppressDbgMsg;

	//ȫ�ֱ��� ˫��ָ��
	_DbgkDebugObjectType = (POBJECT_TYPE*)message->DbgkDebugObjectType;
	//ntdll sysdll sysload��ʱ��͹̶�λ���� ֱ�Ӵ�R3��һ������
	_PsSystemDllBase = (void*)message->PsSystemDllBase;

	NewKiDispatchExceptionHookInfo.OriginalFunction = (ULONG_PTR)KiDispatchException;
	NewKiDispatchExceptionHookInfo.NewFunction = (ULONG_PTR)NewKiDispatchException;
	if (!HookFunction(&NewKiDispatchExceptionHookInfo))
	{
		return false;
	}
	
	NewDbgkForwardExceptionHookInfo.OriginalFunction = (ULONG_PTR)DbgkForwardException;
	NewDbgkForwardExceptionHookInfo.NewFunction = (ULONG_PTR)NewDbgkForwardException;
	if (!HookFunction(&NewDbgkForwardExceptionHookInfo))
	{
		return false;
	}

	NewDbgkCreateThreadHookInfo.OriginalFunction = (ULONG_PTR)DbgkCreateThread;
	NewDbgkCreateThreadHookInfo.NewFunction = (ULONG_PTR)NewDbgkCreateThread;
	if (!HookFunction(&NewDbgkCreateThreadHookInfo))
	{
		return false;
	}

	NewDbgkMapViewOfSectionHookInfo.OriginalFunction = (ULONG_PTR)DbgkMapViewOfSection;
	NewDbgkMapViewOfSectionHookInfo.NewFunction = (ULONG_PTR)NewDbgkMapViewOfSection;
	if (!HookFunction(&NewDbgkMapViewOfSectionHookInfo))
	{
		return false;
	}

	NewDbgkUnMapViewOfSectionHookInfo.OriginalFunction = (ULONG_PTR)DbgkUnMapViewOfSection;
	NewDbgkUnMapViewOfSectionHookInfo.NewFunction = (ULONG_PTR)NewDbgkUnMapViewOfSection;
	if (!HookFunction(&NewDbgkUnMapViewOfSectionHookInfo))
	{
		return false;
	}

	NewNtCreateUserProcessHookInfo.OriginalFunction = (ULONG_PTR)NtCreateUserProcess;
	NewNtCreateUserProcessHookInfo.NewFunction = (ULONG_PTR)NewNtCreateUserProcess;
	if (!HookFunction(&NewNtCreateUserProcessHookInfo))
	{
		return false;
	}
	_Init = true;
	return true;
}

NTSTATUS NewFunc::NewNtReadWriteVirtualMemory(Message_NtReadWriteVirtualMemory *message)
{
	HANDLE ProcessHandle = message->ProcessHandle;
	PVOID BaseAddress = message->BaseAddress;
	void* Buffer = message->Buffer;
	SIZE_T BufferSize = message->BufferBytes;
	PSIZE_T NumberOfBytesWritten = message->ReturnBytes;

	SIZE_T BytesCopied;
	KPROCESSOR_MODE PreviousMode;
	PEPROCESS Process;
	NTSTATUS Status;
	PETHREAD CurrentThread;

	PAGED_CODE();

	CurrentThread = PsGetCurrentThread();
	PreviousMode = KeGetPreviousMode();
	if (PreviousMode != KernelMode) 
	{

		if (((PCHAR)BaseAddress + BufferSize < (PCHAR)BaseAddress) ||
			((PCHAR)Buffer + BufferSize < (PCHAR)Buffer) ||
			((PVOID)((PCHAR)BaseAddress + BufferSize) > MM_HIGHEST_USER_ADDRESS) ||
			((PVOID)((PCHAR)Buffer + BufferSize) > MM_HIGHEST_USER_ADDRESS)) 
		{
			return STATUS_ACCESS_VIOLATION;
		}

		if (ARGUMENT_PRESENT(NumberOfBytesWritten)) 
		{
			__try 
			{
				ProbeForWrite(NumberOfBytesWritten, sizeof(PSIZE_T), sizeof(ULONG));
			} __except(EXCEPTION_EXECUTE_HANDLER) 
			{
				return GetExceptionCode();
			}
		}
	}

	BytesCopied = 0;
	Status = STATUS_SUCCESS;
	if (BufferSize != 0)
	{
		do
		{
			PEPROCESS temp_process;
			Status = PsLookupProcessByProcessId((HANDLE)message->ProcessId, &temp_process);
			if (!NT_SUCCESS(Status))
			{
				break;
			}
			if (message->Read)
			{
				Status = MmCopyVirtualMemory(temp_process, (PVOID)message->BaseAddress, PsGetCurrentProcess(),
					message->Buffer, message->BufferBytes, PreviousMode, &BytesCopied);
			}
			else
			{
				Status = MmCopyVirtualMemory(PsGetCurrentProcess(), message->Buffer, temp_process,
					(PVOID)message->BaseAddress, message->BufferBytes, PreviousMode, &BytesCopied);
			}
			ObDereferenceObject(temp_process);
		} while (false);
	}

	if (ARGUMENT_PRESENT(NumberOfBytesWritten)) 
	{
		__try 
		{
			*NumberOfBytesWritten = BytesCopied;

		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			NOTHING;
		}
	}
	
	return Status;
}

NTSTATUS NewFunc::NewNtProtectVirtualMemory(Message_NtProtectVirtualMemory *message)
{
	NTSTATUS status = 0;
	status = NtProtectVirtualMemory(message->ProcessHandle, message->BaseAddress, message->RegionSize, message->NewProtect, message->OldProtect);
	return status;
}

NTSTATUS NewFunc::NewNtOpenProcess(Message_NewNtOpenProcess *message)
{
	PHANDLE ProcessHandle = message->ProcessHandle;
	ACCESS_MASK DesiredAccess = message->DesiredAccess;
	POBJECT_ATTRIBUTES ObjectAttributes = message->ObjectAttributes;
	PCLIENT_ID ClientId = message->ClientId;

	HANDLE Handle;
	KPROCESSOR_MODE PreviousMode;
	NTSTATUS Status;
	PEPROCESS Process;
	PETHREAD Thread;
	CLIENT_ID CapturedCid = { 0 };
	BOOLEAN ObjectNamePresent;
	BOOLEAN ClientIdPresent;
	ACCESS_STATE AccessState;
	AUX_ACCESS_DATA AuxData;
	ULONG Attributes;

	
	PEPROCESS temp_process;
	Status = PsLookupProcessByProcessId((HANDLE)message->ClientId->UniqueProcess, &temp_process);
	if (!NT_SUCCESS(Status))
	{
		return STATUS_UNSUCCESSFUL;
	}
	ObDereferenceObject(temp_process);


	PreviousMode = KeGetPreviousMode();
	if (PreviousMode != KernelMode) 
	{
		__try 
		{
			ProbeForWriteHandle(ProcessHandle);

			ProbeForRead(ObjectAttributes,
				sizeof(OBJECT_ATTRIBUTES),
				sizeof(ULONG));
			ObjectNamePresent = (BOOLEAN)ARGUMENT_PRESENT(ObjectAttributes->ObjectName);
			Attributes = ObjectAttributes->Attributes;

			if (ARGUMENT_PRESENT(ClientId)) 
			{
				//ProbeForReadSmallStructure(ClientId, sizeof(CLIENT_ID), sizeof(ULONG));
				CapturedCid = *ClientId;
				ClientIdPresent = TRUE;
			}
			else 
			{
				ClientIdPresent = FALSE;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return GetExceptionCode();
		}
	}
	else 
	{
		ObjectNamePresent = (BOOLEAN)ARGUMENT_PRESENT(ObjectAttributes->ObjectName);
		Attributes = ObjectAttributes->Attributes;
		if (ARGUMENT_PRESENT(ClientId)) 
		{
			CapturedCid = *ClientId;
			ClientIdPresent = TRUE;
		}
		else 
		{
			ClientIdPresent = FALSE;
		}
	}

	if (ObjectNamePresent && ClientIdPresent) 
	{
		return STATUS_INVALID_PARAMETER_MIX;
	}

	Status = SeCreateAccessState(
		&AccessState,
		&AuxData,
		DesiredAccess,
		&(*PsProcessType)->TypeInfo.GenericMapping);

	if (!NT_SUCCESS(Status)) 
	{
		return Status;
	}

	//http://www.rohitab.com/discuss/topic/39981-kernel-hack-hooking-sesingleprivilegecheck-to-bypass-privilege-checks/
	AccessState.PreviouslyGrantedAccess |= PROCESS_ALL_ACCESS;//ֱ�Ӹ����Ȩ��
	/*if (SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode))
	{
		if (AccessState.RemainingDesiredAccess & MAXIMUM_ALLOWED)
		{
			AccessState.PreviouslyGrantedAccess |= PROCESS_ALL_ACCESS;
		}
		else
		{
			AccessState.PreviouslyGrantedAccess |= (AccessState.RemainingDesiredAccess);
		}
		AccessState.RemainingDesiredAccess = 0;
	}*/

	if (ObjectNamePresent) 
	{
		Status = ObOpenObjectByName(
			ObjectAttributes,
			*PsProcessType,
			PreviousMode,
			&AccessState,
			0,
			NULL,
			&Handle);//�򲻿�ֻ�ܸľ���� ��BEѭ��ȥȨ�޵��޽� ���ľ������ܻ᲻֧��CE����
		SeDeleteAccessState(&AccessState);
		if (NT_SUCCESS(Status)) 
		{
			__try
			{
				*ProcessHandle = Handle;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return GetExceptionCode();
			}
		}

		return Status;
	}

	if (ClientIdPresent) 
	{
		Thread = NULL;
		if (CapturedCid.UniqueThread) 
		{
			Status = PsLookupProcessThreadByCid(&CapturedCid,&Process,&Thread);
			if (!NT_SUCCESS(Status))
			{
				SeDeleteAccessState(&AccessState);
				return Status;
			}
		}
		else 
		{
			Status = PsLookupProcessByProcessId(CapturedCid.UniqueProcess,&Process);
			if (!NT_SUCCESS(Status)) {
				SeDeleteAccessState(&AccessState);
				return Status;
			}
		}
		Status = ObOpenObjectByPointer(
			Process,
			Attributes,
			&AccessState,
			0,
			*PsProcessType,
			PreviousMode,
			&Handle
		);
		SeDeleteAccessState(&AccessState);
		if (Thread) 
		{
			ObDereferenceObject(Thread);
		}
		ObDereferenceObject(Process);
		if (NT_SUCCESS(Status)) 
		{
			__try
			{
				*ProcessHandle = Handle;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return GetExceptionCode();
			}
		}
		return Status;
	}
	return STATUS_INVALID_PARAMETER_MIX;
}

NTSTATUS NewFunc::NewNtCreateDebugObject(Message_NewNtCreateDebugObject *message)
{
	PHANDLE DebugObjectHandle = message->DebugObjectHandle;
	ACCESS_MASK DesiredAccess = message->DesiredAccess;
	POBJECT_ATTRIBUTES ObjectAttributes = message->ObjectAttributes;
	ULONG Flags = message->Flags;

	NTSTATUS Status;
	HANDLE Handle = nullptr;
	KPROCESSOR_MODE PreviousMode;
	PDEBUG_OBJECT DebugObject = nullptr;

	PreviousMode = KeGetPreviousMode();

	__try 
	{
		if (PreviousMode != KernelMode)
		{
			ProbeForWriteHandle(DebugObjectHandle);
		}
		*DebugObjectHandle = NULL;//�ж��ڴ��Ƿ��д ʵ���ϲ����� ����Ҫ�ڴ�Ҳ��
	} __except(1) //ExSystemExceptionFilter()
	{ // If previous mode is kernel then don't handle the exception
		return GetExceptionCode();
	}

	if (Flags & ~DEBUG_KILL_ON_CLOSE) 
	{
		return STATUS_INVALID_PARAMETER;
	}

	Status = ObCreateObject(PreviousMode,
		*_DbgkDebugObjectType,
		ObjectAttributes,
		PreviousMode,
		NULL,
		sizeof(DEBUG_OBJECT),
		0,
		0,
		(PVOID*)&DebugObject);
	if (!NT_SUCCESS(Status)) 
	{
		return Status;
	}

	ExInitializeFastMutex(&DebugObject->Mutex);
	InitializeListHead(&DebugObject->EventList);
	KeInitializeEvent(&DebugObject->EventsPresent, NotificationEvent, FALSE);

	if (Flags & DEBUG_KILL_ON_CLOSE) 
	{
		DebugObject->Flags = DEBUG_OBJECT_KILL_ON_CLOSE;
	}
	else 
	{
		DebugObject->Flags = 0;
	}

	Status = ObInsertObject(DebugObject,
		NULL,
		DesiredAccess,
		0,
		NULL,
		&Handle);
	if (!NT_SUCCESS(Status)) 
	{
		return Status;
	}

	//__try 
	//{
	//	*DebugObjectHandle = Handle;
	//} __except(ExSystemExceptionFilter()) 
	//{
	//	//����ʱ�����ؾ������ �����������Ч��
	//	Status = GetExceptionCode();
	//}
	*message->DebugObjectHandle = Handle;//���ؾ��

	DebugInfomation *temp_debuginfo = new DebugInfomation();
	temp_debuginfo->SourceProcessId = PsGetCurrentProcessId();
	temp_debuginfo->DebugObject = DebugObject;
	temp_debuginfo->DebugObjectHandle = Handle;

	_DebugInfomationVector.push_back(temp_debuginfo);

	return Status;
}

NTSTATUS NewFunc::NewNtDebugActiveProcess(Message_NewNtDebugActiveProcess *message)
{
	HANDLE ProcessHandle = message->ProcessHandle;
	HANDLE DebugObjectHandle = message->DebugObjectHandle;

	NTSTATUS Status;
	KPROCESSOR_MODE PreviousMode;
	PDEBUG_OBJECT DebugObject = nullptr;
	PEPROCESS Process;
	PETHREAD LastThread = nullptr;

	PreviousMode = KeGetPreviousMode();

	Status = PsLookupProcessByProcessId((HANDLE)message->ProcessId, &Process);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	if (Process == PsGetCurrentProcess() || Process == PsInitialSystemProcess)
	{
		ObDereferenceObject(Process);
		return STATUS_ACCESS_DENIED;
	}

	HANDLE temp_pid = PsGetCurrentProcessId();
	for (auto x : _DebugInfomationVector)
	{
		if (x->SourceProcessId == temp_pid)
		{
			x->TargetProcessId = message->ProcessId;
			DebugObject = x->DebugObject;
		}
	}

	if (NT_SUCCESS(Status))
	{
		if (ExAcquireRundownProtection(PrivateGetProcessRundownProtect(Process)))
		{
			Status = PrivateDbgkpPostFakeProcessCreateMessages(Process, DebugObject, &LastThread);
			Status = PrivateDbgkpSetProcessDebugObject(Process, DebugObject, Status, LastThread);
			ExReleaseRundownProtection(PrivateGetProcessRundownProtect(Process));
		}
		else
		{
			Status = STATUS_PROCESS_IS_TERMINATING;
		}
		//ObDereferenceObject(DebugObject);//����Ҫ������
	}
	ObDereferenceObject(Process);

	return Status;
}

NTSTATUS NTAPI NewFunc::NewNtRemoveProcessDebug(Message_NewNtRemoveProcessDebug *message)
{
	return STATUS_SUCCESS;
}
//---------------






//---------------
NTSTATUS NTAPI NewFunc::PrivateDbgkpPostFakeProcessCreateMessages(
	IN PEPROCESS Process,
	IN PDEBUG_OBJECT DebugObject,
	IN PETHREAD *pLastThread)
{
	NTSTATUS Status;
	KAPC_STATE ApcState;
	PETHREAD Thread;
	PETHREAD LastThread;

	PAGED_CODE();

	KeStackAttachProcess((PKPROCESS)Process, &ApcState);
	Status = PrivateDbgkpPostFakeThreadMessages(Process, DebugObject, NULL, &Thread, &LastThread);
	if (NT_SUCCESS(Status)) 
	{
		Status = PrivateDbgkpPostFakeModuleMessages(Process, Thread, DebugObject);
		if (!NT_SUCCESS(Status)) 
		{
			ObDereferenceObject(LastThread);
			LastThread = NULL;
		}
		ObDereferenceObject(Thread);
	}
	else 
	{
		LastThread = NULL;
	}
	KeUnstackDetachProcess(&ApcState);
	*pLastThread = LastThread;
	return Status;
}

NTSTATUS NTAPI NewFunc::PrivateDbgkpPostFakeThreadMessages(
	IN PEPROCESS Process,
	IN PDEBUG_OBJECT DebugObject,
	IN PETHREAD StartThread,
	OUT PETHREAD *pFirstThread,
	OUT PETHREAD *pLastThread)
{
	NTSTATUS Status;
	PETHREAD Thread, FirstThread, LastThread;
	DBGKM_APIMSG ApiMsg;
	BOOLEAN First = TRUE;
	PIMAGE_NT_HEADERS NtHeaders;
	ULONG Flags;
	NTSTATUS Status1;

	PAGED_CODE();

	LastThread = FirstThread = NULL;
	Status = STATUS_UNSUCCESSFUL;

	if (StartThread != NULL) 
	{
		First = FALSE;
		FirstThread = StartThread;
		ObReferenceObject(FirstThread);
	}
	else 
	{
		StartThread = PsGetNextProcessThread(Process, NULL);
		First = TRUE;
	}

	for (Thread = StartThread; Thread != NULL; Thread = PsGetNextProcessThread(Process, Thread))
	{
		Flags = DEBUG_EVENT_NOWAIT;
		if (LastThread != NULL) 
		{
			ObDereferenceObject(LastThread);
		}
		LastThread = Thread;
		ObReferenceObject(LastThread);

		if (ExAcquireRundownProtection(PrivateGetThreadRundownProtect(Thread)))
		{
			Flags |= DEBUG_EVENT_RELEASE;
			if (!IS_SYSTEM_THREAD(Thread)) 
			{
				Status1 = PsSuspendThread(Thread, NULL);
				if (NT_SUCCESS(Status1)) 
				{
					Flags |= DEBUG_EVENT_SUSPEND;
				}
			}
		}
		else 
		{
			Flags |= DEBUG_EVENT_PROTECT_FAILED;
		}

		RtlZeroMemory(&ApiMsg, sizeof(ApiMsg));
		if (First) 
		{
			ApiMsg.ApiNumber = DbgKmCreateProcessApi;
			if (PrivateGetProcessSectionObject(Process) != NULL) 
			{ // system process doesn't have one of these!
				ApiMsg.u.CreateProcessInfo.FileHandle = DbgkpSectionToFileHandle(PrivateGetProcessSectionObject(Process));
			}
			else 
			{
				ApiMsg.u.CreateProcessInfo.FileHandle = NULL;
			}
			ApiMsg.u.CreateProcessInfo.BaseOfImage = PsGetProcessSectionBaseAddress(Process);
			__try 
			{
				NtHeaders = RtlImageNtHeader(PsGetProcessSectionBaseAddress(Process));
				if (NtHeaders) 
				{
					ApiMsg.u.CreateProcessInfo.InitialThread.StartAddress = NULL; // Filling this in breaks MSDEV!
																				  //                        (PVOID)(NtHeaders->OptionalHeader.ImageBase + NtHeaders->OptionalHeader.AddressOfEntryPoint);
					ApiMsg.u.CreateProcessInfo.DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
					ApiMsg.u.CreateProcessInfo.DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
				}
			} 
			__except(EXCEPTION_EXECUTE_HANDLER) 
			{
				ApiMsg.u.CreateProcessInfo.InitialThread.StartAddress = NULL;
				ApiMsg.u.CreateProcessInfo.DebugInfoFileOffset = 0;
				ApiMsg.u.CreateProcessInfo.DebugInfoSize = 0;
			}
		}
		else 
		{
			ApiMsg.ApiNumber = DbgKmCreateThreadApi;
			ApiMsg.u.CreateThread.StartAddress = PrivateGetThreadStartAddress(Thread);
		}

		Status = PrivateDbgkpQueueMessage(Process, Thread, &ApiMsg, Flags, DebugObject);

		if (!NT_SUCCESS(Status)) 
		{
			if (Flags & DEBUG_EVENT_SUSPEND) 
			{
				PsResumeThread(Thread, NULL);
			}
			if (Flags & DEBUG_EVENT_RELEASE) 
			{
				ExReleaseRundownProtection(PrivateGetThreadRundownProtect(Thread));
			}
			if (ApiMsg.ApiNumber == DbgKmCreateProcessApi && ApiMsg.u.CreateProcessInfo.FileHandle != NULL) 
			{
				ObCloseHandle(ApiMsg.u.CreateProcessInfo.FileHandle, KernelMode);
			}
			//PsQuitNextProcessThread(Thread);
			ObDereferenceObject(Thread);
			break;
		}
		else if (First) 
		{
			First = FALSE;
			ObReferenceObject(Thread);
			FirstThread = Thread;
		}
	}

	if (!NT_SUCCESS(Status)) 
	{
		if (FirstThread) 
		{
			ObDereferenceObject(FirstThread);
		}
		if (LastThread != NULL) 
		{
			ObDereferenceObject(LastThread);
		}
	}
	else 
	{
		if (FirstThread) 
		{
			*pFirstThread = FirstThread;
			*pLastThread = LastThread;
		}
		else 
		{
			Status = STATUS_UNSUCCESSFUL;
		}
	}
	return Status;
}

NTSTATUS NTAPI NewFunc::PrivateDbgkpSetProcessDebugObject(
	IN PEPROCESS Process,
	IN PDEBUG_OBJECT DebugObject,
	IN NTSTATUS MsgStatus,
	IN PETHREAD LastThread)
{
	NTSTATUS Status;
	PETHREAD ThisThread;
	LIST_ENTRY TempList;
	PLIST_ENTRY Entry;
	PDEBUG_EVENT DebugEvent;
	BOOLEAN First = TRUE;
	PETHREAD Thread;
	BOOLEAN GlobalHeld;
	PETHREAD FirstThread;

	ThisThread = PsGetCurrentThread();
	InitializeListHead(&TempList);
	First = TRUE;
	GlobalHeld = FALSE;
	if (!NT_SUCCESS(MsgStatus)) 
	{
		LastThread = NULL;
		Status = MsgStatus;
	}
	else 
	{
		Status = STATUS_SUCCESS;
	}

	if (NT_SUCCESS(Status)) 
	{
		while (true) 
		{
			GlobalHeld = TRUE;
			ObReferenceObject(LastThread);
			Thread = PsGetNextProcessThread(Process, LastThread);
			if (Thread != NULL) 
			{
				GlobalHeld = FALSE;
				ObDereferenceObject(LastThread);
				Status = PrivateDbgkpPostFakeThreadMessages(Process,
					DebugObject,
					Thread,
					&FirstThread,
					&LastThread);
				if (!NT_SUCCESS(Status)) 
				{
					LastThread = NULL;
					break;
				}
				ObDereferenceObject(FirstThread);
			}
			else 
			{
				break;
			}
		}
	}

	ExAcquireFastMutex(&DebugObject->Mutex);
	if (NT_SUCCESS(Status)) 
	{
		if ((DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING) == 0) 
		{
			//PS_SET_BITS(PrivateGetProcessFlags(Process), PS_PROCESS_FLAGS_NO_DEBUG_INHERIT);
			ObReferenceObject(DebugObject);
		}
		else 
		{
			//Process->DebugPort = NULL;
			Status = STATUS_DEBUGGER_INACTIVE;
		}
	}

	for (Entry = DebugObject->EventList.Flink; Entry != &DebugObject->EventList;)
	{
		DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);
		Entry = Entry->Flink;
		if ((DebugEvent->Flags & DEBUG_EVENT_INACTIVE) != 0 && DebugEvent->BackoutThread == ThisThread) 
		{
			Thread = DebugEvent->Thread;
			/*if (NT_SUCCESS(Status)
				&& Thread->GrantedAccess != 0
				&& !((ULONG)((*(char*)Thread) + NtSysAPI_ETHREAD_CrossThreadFlags_X64_Win7 & PS_CROSS_THREAD_FLAGS_SYSTEM) != 0))*/
			if (NT_SUCCESS(Status) && !IS_SYSTEM_THREAD(Thread))
			{
				if ((DebugEvent->Flags & DEBUG_EVENT_PROTECT_FAILED) != 0) 
				{
					PS_SET_BITS(PrivateGetThreadCrossThreadFlagsPoint(Thread), PS_CROSS_THREAD_FLAGS_SKIP_TERMINATION_MSG);
					RemoveEntryList(&DebugEvent->EventList);
					InsertTailList(&TempList, &DebugEvent->EventList);
				}
				else 
				{
					if (First) 
					{
						DebugEvent->Flags &= ~DEBUG_EVENT_INACTIVE;
						KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);
						First = FALSE;
					}
					DebugEvent->BackoutThread = NULL;
					PS_SET_BITS(PrivateGetThreadCrossThreadFlagsPoint(Thread), PS_CROSS_THREAD_FLAGS_SKIP_CREATION_MSG);
				}
			}
			else 
			{
				RemoveEntryList(&DebugEvent->EventList);
				InsertTailList(&TempList, &DebugEvent->EventList);
			}

			if (DebugEvent->Flags & DEBUG_EVENT_RELEASE) 
			{
				DebugEvent->Flags &= ~DEBUG_EVENT_RELEASE;
				ExReleaseRundownProtection(PrivateGetThreadRundownProtect(Thread));
			}

		}
	}
	ExReleaseFastMutex(&DebugObject->Mutex);
	if (LastThread != NULL)
	{
		ObDereferenceObject(LastThread);
	}

	while (!IsListEmpty(&TempList)) 
	{
		Entry = RemoveHeadList(&TempList);
		DebugEvent = CONTAINING_RECORD(Entry, DEBUG_EVENT, EventList);
		DbgkpWakeTarget(DebugEvent);
	}
	return Status;
}

NTSTATUS NewFunc::PrivateDbgkpQueueMessage(
	IN PEPROCESS Process,
	IN PETHREAD Thread,
	IN OUT PDBGKM_APIMSG ApiMsg,
	IN ULONG Flags,
	IN PDEBUG_OBJECT TargetDebugObject)
{
	PDEBUG_EVENT DebugEvent;
	DEBUG_EVENT StaticDebugEvent;
	PDEBUG_OBJECT DebugObject = nullptr;
	NTSTATUS Status;

	PAGED_CODE();

	if (Flags & DEBUG_EVENT_NOWAIT) 
	{
		DebugEvent = (PDEBUG_EVENT)ExAllocatePoolWithQuotaTag(
			(POOL_TYPE)(NonPagedPool | POOL_QUOTA_FAIL_INSTEAD_OF_RAISE),
			sizeof(*DebugEvent), 'EgbD');

		if (DebugEvent == NULL) 
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		DebugEvent->Flags = Flags | DEBUG_EVENT_INACTIVE;
		ObReferenceObject(Process);
		ObReferenceObject(Thread);
		DebugEvent->BackoutThread = PsGetCurrentThread();
		DebugObject = TargetDebugObject;
	}
	else 
	{
		DebugEvent = &StaticDebugEvent;
		DebugEvent->Flags = Flags;

		/*ExAcquireFastMutex(&DbgkpProcessDebugPortMutex);
		DebugObject = Process->DebugPort;*/
		HANDLE temp_pid = PsGetCurrentProcessId();
		for (auto x : _DebugInfomationVector)
		{
			if (x->SourceProcessId == temp_pid || x->TargetProcessId == temp_pid)//����ֻ��ҪĿ��id
			{
				DebugObject = x->DebugObject;
				break;
			}
		}

		if (ApiMsg->ApiNumber == DbgKmCreateThreadApi || ApiMsg->ApiNumber == DbgKmCreateProcessApi) 
		{
			if (PrivateGetThreadCrossThreadFlags(Thread) & PS_CROSS_THREAD_FLAGS_SKIP_CREATION_MSG)//��debug
			{
				DebugObject = NULL;
			}
		}
		if (ApiMsg->ApiNumber == DbgKmExitThreadApi || ApiMsg->ApiNumber == DbgKmExitProcessApi) 
		{
			if (PrivateGetThreadCrossThreadFlags(Thread) & PS_CROSS_THREAD_FLAGS_SKIP_TERMINATION_MSG)
			{
				DebugObject = NULL;
			}
		}
	}

	KeInitializeEvent(&DebugEvent->ContinueEvent, SynchronizationEvent, FALSE);
	DebugEvent->Process = Process;
	DebugEvent->Thread = Thread;
	DebugEvent->ApiMsg = *ApiMsg;
	DebugEvent->ClientId = { PsGetThreadProcessId(Thread) ,PsGetThreadId(Thread) };

	if (DebugObject == NULL) 
	{
		Status = STATUS_PORT_NOT_SET;
	}
	else 
	{
		ExAcquireFastMutex(&DebugObject->Mutex);
		if ((DebugObject->Flags & DEBUG_OBJECT_DELETE_PENDING) == 0) 
		{
			InsertTailList(&DebugObject->EventList, &DebugEvent->EventList);
			if ((Flags & DEBUG_EVENT_NOWAIT) == 0) 
			{
				KeSetEvent(&DebugObject->EventsPresent, 0, FALSE);
			}
			Status = STATUS_SUCCESS;
		}
		else 
		{
			Status = STATUS_DEBUGGER_INACTIVE;
		}
		ExReleaseFastMutex(&DebugObject->Mutex);
	}


	if ((Flags & DEBUG_EVENT_NOWAIT) == 0) 
	{
		if (NT_SUCCESS(Status)) 
		{
			KeWaitForSingleObject(&DebugEvent->ContinueEvent,
				Executive,
				KernelMode,
				FALSE,
				NULL);

			Status = DebugEvent->Status;
			*ApiMsg = DebugEvent->ApiMsg;
		}
	}
	else 
	{
		if (!NT_SUCCESS(Status)) 
		{
			ObDereferenceObject(Process);
			ObDereferenceObject(Thread);
			ExFreePool(DebugEvent);
		}
	}
	return Status;
}

NTSTATUS NTAPI NewFunc::PrivateDbgkpPostFakeModuleMessages(
	IN PEPROCESS Process,
	IN PETHREAD Thread,
	IN PDEBUG_OBJECT DebugObject)
{
	PPEB Peb = PsGetProcessPeb(Process);
	if (Peb == NULL)
	{
		return STATUS_SUCCESS;
	}

	PPEB_LDR_DATA Ldr;
	PLIST_ENTRY LdrHead, LdrNext;
	PLDR_DATA_TABLE_ENTRY LdrEntry;
	DBGKM_APIMSG ApiMsg;
	ULONG i;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING Name;
	PIMAGE_NT_HEADERS NtHeaders;
	NTSTATUS Status;
	IO_STATUS_BLOCK iosb;

	PAGED_CODE();

	__try 
	{
		Ldr = Peb->Ldr;
		LdrHead = &Ldr->InLoadOrderModuleList;
		ProbeForReadSmallStructure(LdrHead, sizeof(LIST_ENTRY), sizeof(UCHAR));

		for (LdrNext = LdrHead->Flink, i = 0; LdrNext != LdrHead && i < 500; LdrNext = LdrNext->Flink, i++)
		{
			if (i > 0) 
			{
				RtlZeroMemory(&ApiMsg, sizeof(ApiMsg));

				LdrEntry = CONTAINING_RECORD(LdrNext, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
				ProbeForReadSmallStructure(LdrEntry, sizeof(LDR_DATA_TABLE_ENTRY), sizeof(UCHAR));

				ApiMsg.ApiNumber = DbgKmLoadDllApi;
				ApiMsg.u.LoadDll.BaseOfDll = LdrEntry->DllBase;

				ProbeForReadSmallStructure(ApiMsg.u.LoadDll.BaseOfDll, sizeof(IMAGE_DOS_HEADER), sizeof(UCHAR));

				NtHeaders = RtlImageNtHeader(ApiMsg.u.LoadDll.BaseOfDll);
				if (NtHeaders) 
				{
					ApiMsg.u.LoadDll.DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
					ApiMsg.u.LoadDll.DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
				}
				Status = MmGetFileNameForAddress(NtHeaders, &Name);
				if (NT_SUCCESS(Status)) 
				{
					InitializeObjectAttributes(&oa,
						&Name,
						OBJ_FORCE_ACCESS_CHECK | OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
						NULL,
						NULL);

					Status = ZwOpenFile(&ApiMsg.u.LoadDll.FileHandle,
						GENERIC_READ | SYNCHRONIZE,
						&oa,
						&iosb,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						FILE_SYNCHRONOUS_IO_NONALERT);
					if (!NT_SUCCESS(Status)) 
					{
						ApiMsg.u.LoadDll.FileHandle = NULL;
					}
					ExFreePool(Name.Buffer);
				}
				Status = PrivateDbgkpQueueMessage(Process,
					Thread,
					&ApiMsg,
					DEBUG_EVENT_NOWAIT,
					DebugObject);
				if (!NT_SUCCESS(Status) && ApiMsg.u.LoadDll.FileHandle != NULL) 
				{
					ObCloseHandle(ApiMsg.u.LoadDll.FileHandle, KernelMode);
				}

			}
			ProbeForReadSmallStructure(LdrNext, sizeof(LIST_ENTRY), sizeof(UCHAR));
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) 
	{
	}
	
#if defined(_WIN64)
	if (PrivateGetProcessWow64Process(Process) != NULL && PrivateGetProcessWow64Process(Process)->Wow64 != NULL) 
	{
		PPEB32 Peb32;
		PPEB_LDR_DATA32 Ldr32;
		PLIST_ENTRY32 LdrHead32, LdrNext32;
		PLDR_DATA_TABLE_ENTRY32 LdrEntry32;
		PWCHAR pSys;

		Peb32 = (PPEB32)PrivateGetProcessWow64Process(Process)->Wow64;

		__try 
		{
			Ldr32 = (PPEB_LDR_DATA32)UlongToPtr(Peb32->Ldr);

			LdrHead32 = &Ldr32->InLoadOrderModuleList;

			ProbeForReadSmallStructure(LdrHead32, sizeof(LIST_ENTRY32), sizeof(UCHAR));
			for (LdrNext32 = (PLIST_ENTRY32)UlongToPtr(LdrHead32->Flink), i = 0;
				LdrNext32 != LdrHead32 && i < 500;
				LdrNext32 = (PLIST_ENTRY32)UlongToPtr(LdrNext32->Flink), i++)
			{

				if (i > 0) 
				{
					RtlZeroMemory(&ApiMsg, sizeof(ApiMsg));

					LdrEntry32 = CONTAINING_RECORD(LdrNext32, LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);
					ProbeForReadSmallStructure(LdrEntry32, sizeof(LDR_DATA_TABLE_ENTRY32), sizeof(UCHAR));

					ApiMsg.ApiNumber = DbgKmLoadDllApi;
					ApiMsg.u.LoadDll.BaseOfDll = (PVOID)UlongToPtr(LdrEntry32->DllBase);

					ProbeForReadSmallStructure(ApiMsg.u.LoadDll.BaseOfDll, sizeof(IMAGE_DOS_HEADER), sizeof(UCHAR));

					NtHeaders = RtlImageNtHeader(ApiMsg.u.LoadDll.BaseOfDll);
					if (NtHeaders) {
						ApiMsg.u.LoadDll.DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
						ApiMsg.u.LoadDll.DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
					}

					Status = MmGetFileNameForAddress(NtHeaders, &Name);
					if (NT_SUCCESS(Status)) 
					{
						//ASSERT(sizeof(L"SYSTEM32") == sizeof(WOW64_SYSTEM_DIRECTORY_U));
						pSys = wcsstr(Name.Buffer, L"\\SYSTEM32\\");
						if (pSys != NULL) 
						{
							RtlCopyMemory(pSys + 1, L"SysWOW64", sizeof(L"SysWOW64") - sizeof(UNICODE_NULL));
						}

						InitializeObjectAttributes(&oa,
							&Name,
							OBJ_FORCE_ACCESS_CHECK | OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
							NULL,
							NULL);

						Status = ZwOpenFile(&ApiMsg.u.LoadDll.FileHandle,
							GENERIC_READ | SYNCHRONIZE,
							&oa,
							&iosb,
							FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
							FILE_SYNCHRONOUS_IO_NONALERT);
						if (!NT_SUCCESS(Status)) {
							ApiMsg.u.LoadDll.FileHandle = NULL;
						}
						ExFreePool(Name.Buffer);
					}

					Status = PrivateDbgkpQueueMessage(Process, Thread, &ApiMsg, DEBUG_EVENT_NOWAIT, DebugObject);
					if (!NT_SUCCESS(Status) && ApiMsg.u.LoadDll.FileHandle != NULL) 
					{
						ObCloseHandle(ApiMsg.u.LoadDll.FileHandle, KernelMode);
					}
				}

				ProbeForReadSmallStructure(LdrNext32, sizeof(LIST_ENTRY32), sizeof(UCHAR));
			}

		} __except(EXCEPTION_EXECUTE_HANDLER) 
		{
		}
	}

#endif
	return STATUS_SUCCESS;
}

NTSTATUS NTAPI NewFunc::PrivateDbgkpSendApiMessage(
	IN OUT PDBGKM_APIMSG ApiMsg,
	IN BOOLEAN SuspendProcess)
{
	NTSTATUS st;
	PEPROCESS Process;
	PAGED_CODE();
	if (SuspendProcess) 
	{
		SuspendProcess = DbgkpSuspendProcess();
	}
	ApiMsg->ReturnedStatus = STATUS_PENDING;
	Process = PsGetCurrentProcess();
	PS_SET_BITS(PrivateGetProcessFlags(Process), PS_PROCESS_FLAGS_CREATE_REPORTED);
	st = PrivateDbgkpQueueMessage(Process, PsGetCurrentThread(), ApiMsg, 0, NULL);
	ZwFlushInstructionCache(NtCurrentProcess(), NULL, 0);
	if (SuspendProcess) 
	{
		KeThawAllThreads();
	}
	return st;
}
//----------




//---------------
#ifdef _AMD64_
VOID NTAPI NewFunc::NewKiDispatchException(
	IN PEXCEPTION_RECORD ExceptionRecord,
	IN PKEXCEPTION_FRAME ExceptionFrame,
	IN PKTRAP_FRAME TrapFrame,
	IN KPROCESSOR_MODE PreviousMode,
	IN BOOLEAN FirstChance)
#else
VOID NTAPI NewFunc::NewKiDispatchException(
	IN PEXCEPTION_RECORD ExceptionRecord,
	IN void *ExceptionFrame,
	IN void *TrapFrame,
	IN KPROCESSOR_MODE PreviousMode,
	IN BOOLEAN FirstChance)
#endif // _AMD64_
{
	if (PreviousMode == KernelMode)
	{
	}
	else
	{
		//�û�ģʽҲ��һ�ν���KiDebugRoutine�Ļ���.
		//Kernel

		//User
		//if (FirstChance == true)
		{
			HANDLE temp_pid = PsGetCurrentProcessId();
			for (auto x : _This->_DebugInfomationVector)
			{
				if (x->TargetProcessId == temp_pid)
				{

					/*if ((_This->PrivateGetProcessWow64Process(PsGetCurrentProcess()) != NULL) &&
						(ExceptionRecord->ExceptionCode == STATUS_DATATYPE_MISALIGNMENT) &&
						((TrapFrame->EFlags & EFLAGS_AC_MASK) != 0))
					{
						TrapFrame->EFlags &= ~EFLAGS_AC_MASK;
						break;
					}*/

					if ((TrapFrame->SegCs & 0xfff8) == KGDT64_R3_CMCODE) 
					{
						switch (ExceptionRecord->ExceptionCode) 
						{
						case STATUS_BREAKPOINT:
							ExceptionRecord->ExceptionCode = STATUS_WX86_BREAKPOINT;
							break;
						case STATUS_SINGLE_STEP:
							ExceptionRecord->ExceptionCode = STATUS_WX86_SINGLE_STEP;
							break;
						}
					}

					//����ExceptionForwarded�ж�(debugobj
					//debugobjectΪ��ʱ,�ú������ᱻֱ�� ���Բ���Ŀ����̺�ֱ��ת����ȥ
					if (NewDbgkForwardException(ExceptionRecord, TRUE, FALSE))
					{
						//�������������ɹ� ֱ�ӷ��� ������������ ��ֻ��һ�λ���.
						//return; ������ģʽר�θ������������쳣
					}

					if ((TrapFrame->SegCs & 0xfff8) == KGDT64_R3_CMCODE)
					{
						switch (ExceptionRecord->ExceptionCode)
						{
						case STATUS_WX86_BREAKPOINT:
							ExceptionRecord->ExceptionCode = STATUS_BREAKPOINT;
							break;
						case STATUS_WX86_SINGLE_STEP:
							ExceptionRecord->ExceptionCode = STATUS_SINGLE_STEP;
							break;
						}
					}
					break;
				}
			}
		}
	}



	_KiDispatchException func = (_KiDispatchException)_This->NewKiDispatchExceptionHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();//��������
	}
	//ʧ�ܻ��������� ���ٴ�ִ��
	return func(ExceptionRecord, ExceptionFrame, TrapFrame, PreviousMode, FirstChance);
}

BOOLEAN NTAPI NewFunc::NewDbgkForwardException(
	IN PEXCEPTION_RECORD ExceptionRecord,
	IN BOOLEAN DebugException,
	IN BOOLEAN SecondChance)
{
	PDBGKM_EXCEPTION args;
	DBGKM_APIMSG m;
	NTSTATUS st;
	HANDLE temp_pid = PsGetCurrentProcessId();
	for (auto x : _This->_DebugInfomationVector)
	{
		if (x->TargetProcessId == temp_pid)
		{
			//����ȷ��ΪĿ�����
			//����PS_CROSS_THREAD_FLAGS_HIDEFROMDBG
			//����Process->DebugPort
			//����LpcPort = FALSE;
			if (DebugException == FALSE)
			{
				DbgBreakPoint();//�������� �ò�����Ӧ��Ϊfalse
			}
			//������Ϣ

			args = &m.u.Exception;

			DBGKM_FORMAT_API_MSG(m, DbgKmExceptionApi, sizeof(*args));

			args->ExceptionRecord = *ExceptionRecord;
			args->FirstChance = !SecondChance;

			st = _This->PrivateDbgkpSendApiMessage(&m, DebugException);
			if (!NT_SUCCESS(st) || ((DebugException) &&
				(m.ReturnedStatus == DBG_EXCEPTION_NOT_HANDLED || !NT_SUCCESS(m.ReturnedStatus))))
			{
				return FALSE;//����ʧ��
			}
			return TRUE;//δ�������� ֱ�ӷ��� ���������µ���
		}
	}

	_DbgkForwardException func = (_DbgkForwardException)_This->NewDbgkForwardExceptionHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();//������...
	}
	//��Ŀ����� ����ִ�к���
	return func(ExceptionRecord, DebugException, SecondChance);
}

VOID NTAPI NewFunc::NewDbgkCreateThread(PETHREAD Thread, PVOID StartAddress)
{
	PVOID Port;
	DBGKM_APIMSG m;
	PDBGKM_CREATE_THREAD CreateThreadArgs;
	PDBGKM_CREATE_PROCESS CreateProcessArgs;
	PEPROCESS Process = PsGetCurrentProcess();
	HANDLE ProcessId = PsGetCurrentProcessId();
	PDBGKM_LOAD_DLL LoadDllArgs;
	NTSTATUS Status;
	OBJECT_ATTRIBUTES Obja;
	IO_STATUS_BLOCK IoStatusBlock;
	PIMAGE_NT_HEADERS NtHeaders;
	PTEB Teb;

	PAGED_CODE();


	for (auto x : _This->_DebugInfomationVector)
	{
		if (x->TargetProcessId == ProcessId)
		{
			//����PsCallImageNotifyRoutines �˹��̺͵����޹�
			//����DebugPort
			//if (_This->PrivateGetProcessUserTime(Process))
			{
				//PS_SET_BITS(_This->PrivateGetProcessFlags(Process), PS_PROCESS_FLAGS_CREATE_REPORTED | PS_PROCESS_FLAGS_IMAGE_NOTIFY_DONE);
			}

			auto temp_result = PS_TEST_SET_BITS(_This->PrivateGetProcessFlags(Process), 0x400001);

			if ((temp_result & PS_PROCESS_FLAGS_CREATE_REPORTED) == 0)
			//if (*_This->PrivateGetProcessFlags(Process) & PS_PROCESS_FLAGS_CREATE_REPORTED)
			{
				CreateThreadArgs = &m.u.CreateProcessInfo.InitialThread;
				CreateThreadArgs->SubSystemKey = 0;

				CreateProcessArgs = &m.u.CreateProcessInfo;
				CreateProcessArgs->SubSystemKey = 0;
				CreateProcessArgs->FileHandle = _This->DbgkpSectionToFileHandle(_This->PrivateGetProcessSectionObject(Process));
				CreateProcessArgs->BaseOfImage = _This->PrivateGetProcessSectionBaseAddress(Process);
				CreateThreadArgs->StartAddress = NULL;
				CreateProcessArgs->DebugInfoFileOffset = 0;
				CreateProcessArgs->DebugInfoSize = 0;

				__try
				{
					NtHeaders = RtlImageNtHeader(_This->PrivateGetProcessSectionBaseAddress(Process));
					if (NtHeaders)
					{
						if (_This->PrivateGetProcessWow64Process(PsGetCurrentProcess()) != NULL) 
						{
							CreateThreadArgs->StartAddress = UlongToPtr(DBGKP_FIELD_FROM_IMAGE_OPTIONAL_HEADER((PIMAGE_NT_HEADERS32)NtHeaders, ImageBase) +
								DBGKP_FIELD_FROM_IMAGE_OPTIONAL_HEADER((PIMAGE_NT_HEADERS32)NtHeaders, AddressOfEntryPoint));
						}
						else {
							CreateThreadArgs->StartAddress = (PVOID)(DBGKP_FIELD_FROM_IMAGE_OPTIONAL_HEADER(NtHeaders, ImageBase) +
								DBGKP_FIELD_FROM_IMAGE_OPTIONAL_HEADER(NtHeaders, AddressOfEntryPoint));
						}
						/*CreateThreadArgs->StartAddress = (PVOID)(
							NtHeaders->OptionalHeader.ImageBase +
							NtHeaders->OptionalHeader.AddressOfEntryPoint);*/
						CreateProcessArgs->DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
						CreateProcessArgs->DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					CreateThreadArgs->StartAddress = NULL;
					CreateProcessArgs->DebugInfoFileOffset = 0;
					CreateProcessArgs->DebugInfoSize = 0;
				}

				DBGKM_FORMAT_API_MSG(m, DbgKmCreateProcessApi, sizeof(*CreateProcessArgs));
				_This->PrivateDbgkpSendApiMessage(&m, FALSE);
				if (CreateProcessArgs->FileHandle != NULL)
				{
					ObCloseHandle(CreateProcessArgs->FileHandle, KernelMode);
				}

				LoadDllArgs = &m.u.LoadDll;
				LoadDllArgs->BaseOfDll = _This->_PsSystemDllBase;
				LoadDllArgs->DebugInfoFileOffset = 0;
				LoadDllArgs->DebugInfoSize = 0;

				Teb = NULL;
				__try
				{
					NtHeaders = RtlImageNtHeader(_This->_PsSystemDllBase);
					if (NtHeaders)
					{
						LoadDllArgs->DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
						LoadDllArgs->DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
					}

					Teb = (PTEB)PsGetThreadTeb(Thread);
					if (Teb != NULL)
					{
						Teb->NtTib.ArbitraryUserPointer = Teb->StaticUnicodeBuffer;
						wcsncpy(Teb->StaticUnicodeBuffer,
							L"ntdll.dll",
							sizeof(Teb->StaticUnicodeBuffer) / sizeof(Teb->StaticUnicodeBuffer[0]));
						LoadDllArgs->NamePointer = &Teb->NtTib.ArbitraryUserPointer;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					LoadDllArgs->DebugInfoFileOffset = 0;
					LoadDllArgs->DebugInfoSize = 0;
					LoadDllArgs->NamePointer = NULL;
				}

				InitializeObjectAttributes(
					&Obja,
					(PUNICODE_STRING)&PsNtDllPathName,
					OBJ_CASE_INSENSITIVE | OBJ_FORCE_ACCESS_CHECK | OBJ_KERNEL_HANDLE,
					NULL,
					NULL
				);

				Status = ZwOpenFile(
					&LoadDllArgs->FileHandle,
					(ACCESS_MASK)(GENERIC_READ | SYNCHRONIZE),
					&Obja,
					&IoStatusBlock,
					FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
					FILE_SYNCHRONOUS_IO_NONALERT
				);

				if (!NT_SUCCESS(Status)) 
				{
					LoadDllArgs->FileHandle = NULL;
				}

				DBGKM_FORMAT_API_MSG(m, DbgKmLoadDllApi, sizeof(*LoadDllArgs));
				_This->PrivateDbgkpSendApiMessage(&m, TRUE);

				if (LoadDllArgs->FileHandle != NULL)
				{
					ObCloseHandle(LoadDllArgs->FileHandle, KernelMode);
				}

				if (Teb != NULL)
				{
					__try
					{
						Teb->NtTib.ArbitraryUserPointer = NULL;
					}
					__except (EXCEPTION_EXECUTE_HANDLER) {}
				}
			}
			else
			{
				CreateThreadArgs = &m.u.CreateThread;
				CreateThreadArgs->SubSystemKey = 0;
				CreateThreadArgs->StartAddress = _This->PrivateGetThreadStartAddress(Thread);
				DBGKM_FORMAT_API_MSG(m, DbgKmCreateThreadApi, sizeof(*CreateThreadArgs));
				_This->PrivateDbgkpSendApiMessage(&m, TRUE);
			}
			break;
		}
	}

	_DbgkCreateThread func = (_DbgkCreateThread)_This->NewDbgkCreateThreadHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();
	}
	//������degport�ᱻ���� �����ػص���Ҫ����ִ��
	return func(Thread, StartAddress);
}


#ifdef _AMD64_
VOID NTAPI NewFunc::NewDbgkMapViewOfSection(
	PEPROCESS Process,
	void *SectionObject,
	void *BaseAddress,
	unsigned int SectionOffset,
	unsigned __int64 ViewSize)
#else
VOID NTAPI NewFunc::NewDbgkMapViewOfSection(
	IN HANDLE SectionObject,
	IN PVOID BaseAddress,
	IN ULONG SectionOffset,
	IN ULONG_PTR ViewSize)
#endif // _AMD64_
{
	DBGKM_APIMSG m;
	PDBGKM_LOAD_DLL LoadDllArgs;
	PIMAGE_NT_HEADERS NtHeaders;

	PAGED_CODE();

	if (KeGetPreviousMode() == KernelMode) 
	{
		return;
	}

	for (auto x : _This->_DebugInfomationVector)
	{
		if (x->TargetProcessId == PsGetCurrentProcessId())
		{
			PTEB temp_teb = (PTEB)PsGetThreadTeb(PsGetCurrentThread());
			if (temp_teb == nullptr)
			{
				break;
			}

			NTSTATUS status = _This->DbgkpSuppressDbgMsg(temp_teb);
			if (!NT_SUCCESS(status))
			{
				break;
			}

			LoadDllArgs = &m.u.LoadDll;
			LoadDllArgs->FileHandle = _This->DbgkpSectionToFileHandle(SectionObject);
			LoadDllArgs->BaseOfDll = BaseAddress;
			LoadDllArgs->DebugInfoFileOffset = 0;
			LoadDllArgs->DebugInfoSize = 0;


			LoadDllArgs->NamePointer = temp_teb->NtTib.ArbitraryUserPointer;

			__try
			{
				NtHeaders = RtlImageNtHeader(BaseAddress);
				if (NtHeaders)
				{
					LoadDllArgs->DebugInfoFileOffset = NtHeaders->FileHeader.PointerToSymbolTable;
					LoadDllArgs->DebugInfoSize = NtHeaders->FileHeader.NumberOfSymbols;
				}
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				LoadDllArgs->DebugInfoFileOffset = 0;
				LoadDllArgs->DebugInfoSize = 0;
			}

			DBGKM_FORMAT_API_MSG(m, DbgKmLoadDllApi, sizeof(*LoadDllArgs));

			_This->PrivateDbgkpSendApiMessage(&m, TRUE);
			if (LoadDllArgs->FileHandle != NULL)
			{
				ObCloseHandle(LoadDllArgs->FileHandle, KernelMode);
			}
			return;//�ԴﵽĿ�Ĳ���������ִ���� ����ִ��Ҳ��ֱ�ӱ�������
		}
	}

	_DbgkMapViewOfSection func = (_DbgkMapViewOfSection)_This->NewDbgkMapViewOfSectionHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();
	}
	//��Ŀ�����
#ifdef _AMD64_
	return func(Process, SectionObject, BaseAddress, SectionOffset, ViewSize);
#else
	return func(SectionObject, BaseAddress, SectionOffset, ViewSize);
#endif // _AMD64_
}

VOID NTAPI NewFunc::NewDbgkUnMapViewOfSection(IN PVOID BaseAddress)
{
	PVOID Port;
	DBGKM_APIMSG m;
	PDBGKM_UNLOAD_DLL UnloadDllArgs;
	PEPROCESS Process;

	PAGED_CODE();

	Process = PsGetCurrentProcess();

	if (KeGetPreviousMode() == KernelMode) 
	{
		return;//ֱ�Ӿ��ж��� ����������
	}

	for (auto x : _This->_DebugInfomationVector)
	{
		if (x->TargetProcessId == PsGetCurrentProcessId())
		{
			//ͬ�Ϻ���PS_CROSS_THREAD_FLAGS_HIDEFROMDBG
			UnloadDllArgs = &m.u.UnloadDll;
			UnloadDllArgs->BaseAddress = BaseAddress;
			DBGKM_FORMAT_API_MSG(m, DbgKmUnloadDllApi, sizeof(*UnloadDllArgs));
			_This->PrivateDbgkpSendApiMessage(&m, TRUE);
			return;//ͬ�Ϸ���
		}
	}

	_DbgkUnMapViewOfSection func = (_DbgkUnMapViewOfSection)_This->NewDbgkUnMapViewOfSectionHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();
	}
	//��Ŀ�����
	return func(BaseAddress);
}

//�ϰ� win7��ͨ���ж�createinfo ��PspInsertProcess ��dbgport���̺�PSPC�����޲��
//NTSTATUS NTAPI NewFunc::NewPspCreateProcess(
//	OUT PHANDLE ProcessHandle,
//	IN ACCESS_MASK DesiredAccess,
//	IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
//	IN HANDLE ParentProcess OPTIONAL,
//	IN ULONG Flags,
//	IN HANDLE SectionHandle OPTIONAL,
//	IN HANDLE DebugPort OPTIONAL,
//	IN HANDLE ExceptionPort OPTIONAL,
//	IN ULONG JobMemberLevel)
NTSTATUS NTAPI NewFunc::NewNtCreateUserProcess(
	PHANDLE ProcessHandle,
	PETHREAD ThreadHandle,
	ACCESS_MASK ProcessDesiredAccess,
	ACCESS_MASK ThreadDesiredAccess,
	_OBJECT_ATTRIBUTES *ProcessObjectAttributes,
	_OBJECT_ATTRIBUTES *ThreadObjectAttributes,
	ULONG ProcessFlags,
	ULONG ThreadFlags,
	_RTL_USER_PROCESS_PARAMETERS *ProcessParameters,
	void *CreateInfo,
	void *AttributeList)
{
	_NtCreateUserProcess func = (_NtCreateUserProcess)_This->NewNtCreateUserProcessHookInfo.Bridge;
	if (!func)
	{
		DbgBreakPoint();
	}
	//ִ�д�������
	NTSTATUS status = 0;
	status = func(ProcessHandle,
		ThreadHandle,
		ProcessDesiredAccess,
		ThreadDesiredAccess,
		ProcessObjectAttributes,
		ThreadObjectAttributes,
		ProcessFlags,
		ThreadFlags,
		ProcessParameters,
		CreateInfo,
		AttributeList);

	if (NT_SUCCESS(status) && ProcessHandle != nullptr)
	{
		for (auto x : _This->_DebugInfomationVector)
		{
			if (x->SourceProcessId == PsGetCurrentProcessId())
			{
				//PspCreateProcess̫����д�� ���͵���Ķ�����ȥ����
				//��͵���ķ�ʽ�ǲ�����createprocess ǿ��atttach.

				//Ŀ��ɹ����� ������̺ʹ���debugobj��ͬһ������
				//ֻ�ǲ�ѯ�� ������ע��ص��򲻿� �ͱ�����ֶ������Ķ�����
				PEPROCESS temp_process = nullptr;
				status = ObReferenceObjectByHandle(*ProcessHandle, 0x0400, *PsProcessType, KeGetPreviousMode(), (void**)&temp_process, NULL);
				if (!NT_SUCCESS(status))
				{
					return status;//��϶������ڸ�� ��û�취 Ҫ����Int3���һ������
				}
				//����Ŀ��IDΪ�����ת����ƥ��.
				HANDLE target_pid = PsGetProcessId(temp_process);
				x->TargetProcessId = target_pid;


				//�ɵ�dbgport
				//�ɵ�PEB
				//ʡ��DbgkClearProcessDebugObject�����obj��event���� �Ƕ��������� ������Ҳ�ò�����
				*_This->PrivateGetProcessDebugPort(temp_process) = 0;
				//DbgkpMarkProcessPeb�Դ�KeStackAttachProcess
				_This->DbgkpMarkProcessPeb(temp_process);
				//win7��DbgkpMarkProcessPeb�޴�仯
				//debugobj��hanlde���Ѿ����� prot��pebҲ������� ���ԷŹ�ȥ��
				return status;
			}
		}
	}

	//����Ŀ����� ֱ�ӷŹ�
	return status;
}
