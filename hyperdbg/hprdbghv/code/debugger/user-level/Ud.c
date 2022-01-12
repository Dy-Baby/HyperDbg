/**
 * @file Ud.c
 * @author Sina Karvandi (sina@rayanfam.com)
 * @brief Routines related to user mode debugging
 * @details 
 * @version 0.1
 * @date 2022-01-06
 * 
 * @copyright This project is released under the GNU Public License v3.
 * 
 */
#include "..\hprdbghv\pch.h"

/**
 * @brief initialize user debugger
 * @details this function should be called on vmx non-root
 * 
 * @return VOID 
 */
VOID
UdInitializeUserDebugger()
{
    //
    // Check if it's already initialized or not, we'll ignore it if it's
    // previously initialized
    //
    if (g_UserDebuggerState)
    {
        return;
    }

    //
    // Initialize attaching mechanism
    //
    if (!AttachingInitialize())
    {
        return FALSE;
    }

    //
    // Start the seed of user-mode debugging thread
    //
    g_SeedOfUserDebuggingDetails = DebuggerThreadDebuggingTagStartSeed;

    //
    // Initialize the thread debugging details list
    //
    InitializeListHead(&g_ThreadDebuggingDetailsListHead);

    //
    // Enable vm-exit on Hardware debug exceptions and breakpoints
    // so, intercept #DBs and #BP by changing exception bitmap (one core)
    //
    BroadcastEnableDbAndBpExitingAllCores();

    //
    // Indicate that the user debugger is active
    //
    g_UserDebuggerState = TRUE;
}

/**
 * @brief uninitialize user debugger
 * @details this function should be called on vmx non-root
 *
 * @return VOID 
 */
VOID
UdUninitializeUserDebugger()
{
    if (g_UserDebuggerState)
    {
        //
        // Indicate that the user debugger is not active
        //
        g_UserDebuggerState = FALSE;

        //
        // Free and deallocate all the buffers (pools) relating to
        // thread debugging details
        //
        AttachingRemoveAndFreeAllThreadDebuggingDetails();
    }
}

/**
 * @brief Continue the process
 * 
 * @param ThreadDebuggingDetails
 * 
 * @return VOID 
 */
VOID
UdContinueThread(PUSERMODE_DEBUGGING_THREADS_DETAILS ThreadDebuggingDetails)
{
    //
    // Configure the RIP and RSP again
    //
    __vmx_vmwrite(GUEST_RIP, ThreadDebuggingDetails->GuestRip);
    __vmx_vmwrite(GUEST_RSP, ThreadDebuggingDetails->GuestRsp);

    //
    // Continue the current instruction won't pass it
    //
    g_GuestState[KeGetCurrentProcessorNumber()].IncrementRip = FALSE;

    //
    // It's not paused anymore!
    //
    ThreadDebuggingDetails->IsPaused = FALSE;
}

/**
 * @brief Perform the user-mode commands
 * 
 * @param ThreadDebuggingDetails
 * @param UserAction
 * @param OptionalParam1
 * @param OptionalParam2
 * @param OptionalParam3
 * @param OptionalParam4
 * 
 * @return BOOLEAN 
 */
BOOLEAN
UdPerformCommand(PUSERMODE_DEBUGGING_THREADS_DETAILS ThreadDebuggingDetails,
                 DEBUGGER_UD_COMMAND_ACTION_TYPE     UserAction,
                 UINT64                              OptionalParam1,
                 UINT64                              OptionalParam2,
                 UINT64                              OptionalParam3,
                 UINT64                              OptionalParam4)
{
    //
    // Perform the command
    //
    switch (UserAction)
    {
    case DEBUGGER_UD_COMMAND_ACTION_TYPE_CONTINUE:

        //
        // Continue the thread normally
        //
        UdContinueThread(ThreadDebuggingDetails);

        break;

    default:

        //
        // Invalid user action
        //
        return FALSE;
        break;
    }

    return TRUE;
}

/**
 * @brief Check for the user-mode commands
 *
 * @return BOOLEAN 
 */
BOOLEAN
UdCheckForCommand()
{
    PUSERMODE_DEBUGGING_THREADS_DETAILS ThreadDebuggingDetails;

    ThreadDebuggingDetails =
        AttachingFindThreadDebuggingDetailsByProcessIdAndThreadId(PsGetCurrentProcessId(),
                                                                  PsGetCurrentThreadId());

    if (!ThreadDebuggingDetails)
    {
        return FALSE;
    }

    //
    // If we reached here, the current thread is in debugger attached mechanism
    // now we check whether it's a regular CPUID or a debugger paused thread CPUID
    //
    if (!ThreadDebuggingDetails->IsPaused)
    {
        return FALSE;
    }

    //
    // Here, we're sure that this thread is looking for command, let
    // see if we find anything
    //
    for (size_t i = 0; i < MAX_USER_ACTIONS_FOR_THREADS; i++)
    {
        if (ThreadDebuggingDetails->UdAction[i].ActionType != DEBUGGER_UD_COMMAND_ACTION_TYPE_NONE)
        {
            //
            // Perform the command
            //
            UdPerformCommand(ThreadDebuggingDetails,
                             ThreadDebuggingDetails->UdAction[i].ActionType,
                             ThreadDebuggingDetails->UdAction[i].OptionalParam1,
                             ThreadDebuggingDetails->UdAction[i].OptionalParam2,
                             ThreadDebuggingDetails->UdAction[i].OptionalParam3,
                             ThreadDebuggingDetails->UdAction[i].OptionalParam4);

            //
            // Remove the command
            //
            ThreadDebuggingDetails->UdAction[i].OptionalParam1 = NULL;
            ThreadDebuggingDetails->UdAction[i].OptionalParam2 = NULL;
            ThreadDebuggingDetails->UdAction[i].OptionalParam3 = NULL;
            ThreadDebuggingDetails->UdAction[i].OptionalParam4 = NULL;

            //
            // At last disable it
            //
            ThreadDebuggingDetails->UdAction[i].ActionType = DEBUGGER_UD_COMMAND_ACTION_TYPE_NONE;

            //
            // only one command at a time
            //
            break;
        }
    }

    //
    // Won't change the registers for cpuid
    //
    return TRUE;
}

/**
 * @brief Dispatch the user-mode commands
 *
 * @param ActionRequest
 * @return BOOLEAN 
 */
BOOLEAN
UdDispatchUsermodeCommands(PDEBUGGER_UD_COMMAND_PACKET ActionRequest)
{
    PUSERMODE_DEBUGGING_THREADS_DETAILS ThreadDebuggingDetails;
    BOOLEAN                             CommandApplied = FALSE;

    //
    // Find the thread debugging detail of the thread
    //
    ThreadDebuggingDetails = AttachingFindThreadDebuggingDetailsByToken(ActionRequest->ThreadDebuggingDetailToken);

    if (!ThreadDebuggingDetails)
    {
        //
        // Token not found!
        //
        return FALSE;
    }

    //
    // Apply the command
    //
    for (size_t i = 0; i < MAX_USER_ACTIONS_FOR_THREADS; i++)
    {
        if (ThreadDebuggingDetails->UdAction[i].ActionType == DEBUGGER_UD_COMMAND_ACTION_TYPE_NONE)
        {
            //
            // Set the action
            //
            ThreadDebuggingDetails->UdAction[i].OptionalParam1 = ActionRequest->UdAction.OptionalParam1;
            ThreadDebuggingDetails->UdAction[i].OptionalParam2 = ActionRequest->UdAction.OptionalParam2;
            ThreadDebuggingDetails->UdAction[i].OptionalParam3 = ActionRequest->UdAction.OptionalParam3;
            ThreadDebuggingDetails->UdAction[i].OptionalParam4 = ActionRequest->UdAction.OptionalParam4;

            //
            // At last we set the action type to make it valid
            //
            ThreadDebuggingDetails->UdAction[i].ActionType = ActionRequest->UdAction.ActionType;

            CommandApplied = TRUE;
            break;
        }
    }

    return CommandApplied;
}

/**
 * @brief Spin on nop sled in user-mode to halt the debuggee
 *
 * @param Token
 * @return BOOLEAN 
 */
BOOLEAN
UdSpinThreadOnNop(UINT64 Token)
{
    PUSERMODE_DEBUGGING_THREADS_DETAILS ThreadDebuggingDetails;

    //
    // Find the entry
    //
    ThreadDebuggingDetails = AttachingFindThreadDebuggingDetailsByToken(Token);

    if (!ThreadDebuggingDetails)
    {
        //
        // Token not found!
        //
        return FALSE;
    }

    //
    // Save the RIP and RSP for previous return
    //
    __vmx_vmread(GUEST_RIP, &ThreadDebuggingDetails->GuestRip);
    __vmx_vmread(GUEST_RSP, &ThreadDebuggingDetails->GuestRsp);

    //
    // Set the rip to new spinning address
    //
    __vmx_vmwrite(GUEST_RIP, ThreadDebuggingDetails->UsermodeReservedBuffer);

    //
    // Indicate that it's spinning
    //
    ThreadDebuggingDetails->IsPaused = TRUE;
}

/**
 * @brief Handle #DBs and #BPs for kernel debugger
 * @details This function can be used in vmx-root 
 * 
 * @param CurrentCore
 * @param ThreadDebuggingToken
 * @param GuestRegs
 * @param Reason
 * @param EventDetails
 * @return BOOLEAN 
 */
BOOLEAN
UdHandleBreakpointAndDebugBreakpoints(UINT32                            CurrentCore,
                                      UINT64                            ThreadDebuggingToken,
                                      PGUEST_REGS                       GuestRegs,
                                      DEBUGGEE_PAUSING_REASON           Reason,
                                      PDEBUGGER_TRIGGERED_EVENT_DETAILS EventDetails)
{
    DEBUGGEE_UD_PAUSED_PACKET PausePacket;
    ULONG                     ExitInstructionLength  = 0;
    UINT64                    SizeOfSafeBufferToRead = 0;
    RFLAGS                    Rflags                 = {0};

    //
    // Breaking only supported in vmx-root mode
    //
    if (!g_GuestState[CurrentCore].IsOnVmxRootMode)
    {
        return FALSE;
    }

    //
    // *** Fill the pausing structure ***
    //

    RtlZeroMemory(&PausePacket, sizeof(DEBUGGEE_UD_PAUSED_PACKET));

    //
    // Set the RIP and mode of execution
    //
    PausePacket.Rip            = g_GuestState[CurrentCore].LastVmexitRip;
    PausePacket.Is32BitAddress = KdIsGuestOnUsermode32Bit();

    //
    // Set rflags for finding the results of conditional jumps
    //
    __vmx_vmread(GUEST_RFLAGS, &Rflags);
    PausePacket.Rflags.Value = Rflags.Value;

    //
    // Set the event tag (if it's an event)
    //
    if (EventDetails != NULL)
    {
        PausePacket.EventTag = EventDetails->Tag;
    }

    //
    // Read the instruction len
    //
    if (g_GuestState[CurrentCore].DebuggingState.InstructionLengthHint != 0)
    {
        ExitInstructionLength = g_GuestState[CurrentCore].DebuggingState.InstructionLengthHint;
    }
    else
    {
        //
        // Reading instruction length proved to provide wrong results,
        // so we won't use it anymore
        //
        // __vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &ExitInstructionLength);
        //

        //
        // Compute the amount of buffer we can read without problem
        //
        SizeOfSafeBufferToRead = g_GuestState[CurrentCore].LastVmexitRip & 0xfff;
        SizeOfSafeBufferToRead += MAXIMUM_INSTR_SIZE;

        if (SizeOfSafeBufferToRead >= PAGE_SIZE)
        {
            SizeOfSafeBufferToRead = SizeOfSafeBufferToRead - PAGE_SIZE;
            SizeOfSafeBufferToRead = MAXIMUM_INSTR_SIZE - SizeOfSafeBufferToRead;
        }
        else
        {
            SizeOfSafeBufferToRead = MAXIMUM_INSTR_SIZE;
        }

        //
        // Set the length to notify debuggee
        //
        ExitInstructionLength = SizeOfSafeBufferToRead;
    }

    //
    // Set the reading length of bytes (for instruction disassembling)
    //
    PausePacket.ReadInstructionLen = ExitInstructionLength;

    //
    // Find the current instruction
    //
    MemoryMapperReadMemorySafeOnTargetProcess(g_GuestState[CurrentCore].LastVmexitRip,
                                              &PausePacket.InstructionBytesOnRip,
                                              ExitInstructionLength);

    //
    // Copy registers to the pause packet
    //
    RtlCopyMemory(&PausePacket.GuestRegs, GuestRegs, sizeof(GUEST_REGS));

    //
    // Send the pause packet, along with RIP and an indication
    // to pause to the user debugger
    //
    LogSendBuffer(OPERATION_NOTIFICATION_FROM_USER_DEBUGGER_PAUSE,
                  &PausePacket,
                  sizeof(DEBUGGEE_UD_PAUSED_PACKET),
                  TRUE);

    //
    // Halt the thread on nop sleds
    //
    return UdSpinThreadOnNop(ThreadDebuggingToken);
}
