/**
 * @file start.cpp
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief .start command
 * @details
 * @version 0.1
 * @date 2022-01-06
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "..\hprdbgctrl\pch.h"

//
// Global Variables
//
extern std::wstring g_StartCommandPath;
extern std::wstring g_StartCommandPathAndArguments;

/**
 * @brief help of .start command
 *
 * @return VOID
 */
VOID
CommandStartHelp()
{
    ShowMessages(".start : run a user-mode process.\n\n");
    ShowMessages("syntax : \t.start [path Path (string)] [Parameters (string)]\n");
    ShowMessages("\t\te.g : .start path c:\\users\\sina\\reverse eng\\my_file.exe\n");
}

/**
 * @brief .start command handler
 *
 * @param SplittedCommand
 * @param Command
 * @return VOID
 */
VOID
CommandStart(vector<string> SplittedCommand, string Command)
{
    vector<string> PathAndArgs;
    string         Arguments = "";

    //
    // Show a message that the user debugger is still in the experimental version
    //
    ShowMessages("in contrast with the kernel debugger, the user debugger is still very basic "
                 "and needs a lot of tests and improvements. It's highly recommended not to run the "
                 "user debugger in your bare metal system. Instead, run it on a supported virtual "
                 "machine to won't end up with a Blue Screen of Death (BSOD) in your primary device. "
                 "Please keep reporting the issues to improve the user debugger\n\n");

    if (SplittedCommand.size() <= 2)
    {
        ShowMessages("incorrect use of '.start'\n\n");
        CommandStartHelp();
        return;
    }

    if (!SplittedCommand.at(1).compare("path"))
    {
        //
        // *** It's a run of target PE file ***
        //

        //
        // Trim the command
        //
        Trim(Command);

        //
        // Remove .start from it
        //
        Command.erase(0, 6);

        //
        // Remove path + space
        //
        Command.erase(0, 4 + 1);

        //
        // Trim it again
        //
        Trim(Command);

        //
        // Split Path and args
        //
        SplitPathAndArgs(PathAndArgs, Command);

        //
        // Convert path to wstring
        //
        StringToWString(g_StartCommandPath, PathAndArgs.at(0));

        if (PathAndArgs.size() != 1)
        {
            //
            // There are arguments to this command
            //

            for (auto item : PathAndArgs)
            {
                //
                // Append the arguments
                //
                // ShowMessages("Arg : %s\n", item.c_str());
                Arguments += item + " ";
            }

            //
            // Remove the latest space
            //
            Arguments.pop_back();

            //
            // Convert arguments to wstring
            //
            StringToWString(g_StartCommandPathAndArguments, Arguments);
        }
    }
    else
    {
        ShowMessages("err, couldn't resolve error at '%s'\n\n",
                     SplittedCommand.at(1).c_str());
        CommandStartHelp();
        return;
    }

    //
    // Perform run of the target file
    //
    if (Arguments.empty())
    {
        UdAttachToProcess(NULL,
                          g_StartCommandPath.c_str(),
                          NULL);
    }
    else
    {
        UdAttachToProcess(NULL,
                          g_StartCommandPath.c_str(),
                          (WCHAR *)g_StartCommandPathAndArguments.c_str());
    }
}
