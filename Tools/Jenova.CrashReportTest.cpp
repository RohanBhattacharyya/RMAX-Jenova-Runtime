/*-------------------------------------------------------------+
|                                                              |
|                   _________   ______ _    _____              |
|                  / / ____/ | / / __ \ |  / /   |             |
|             __  / / __/ /  |/ / / / / | / / /| |             |
|            / /_/ / /___/ /|  / /_/ /| |/ / ___ |             |
|            \____/_____/_/ |_/\____/ |___/_/  |_|             |
|                                                              |
|                        Jenova Runtime                        |
|                   Developed by Hamid.Memar                   |
|                                                              |
+-------------------------------------------------------------*/

/*
	Fatal Signal Reporting Test [Linux]

	A C++ script that dereferences null used to take the editor or the game down without
	printing anything at all, which is indistinguishable from a clean exit. This checks the
	handler that replaced that silence: a fault raised while a script is executing must name
	the signal, the script, the function and a probable cause on stderr, and must still let
	the process die with the right signal rather than swallowing it.

	Loads the real built runtime and needs no Godot.

		g++ -std=c++20 -o /tmp/jenova_crash_test Tools/Jenova.CrashReportTest.cpp -ldl
		/tmp/jenova_crash_test ./Linux64/Jenova.Runtime.Linux64.so
*/

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

// Mangled names, because the runtime exports these as ordinary C++ symbols.
#define JENOVA_SYMBOL_INSTALL_HANDLERS		"_ZN6jenova26InstallFatalSignalHandlersEv"
#define JENOVA_SYMBOL_REMOVE_HANDLERS		"_ZN6jenova25RemoveFatalSignalHandlersEv"
#define JENOVA_SYMBOL_EXECUTING_SCRIPT		"_ZN6jenova13GlobalStorage15ExecutingScriptE"

// Mirrors jenova::ScriptExecutionIdentity. The runtime publishes a pointer to one of these
// per call; the handler reads the two names straight out of it.
struct ScriptExecutionIdentity { const char* scriptPath; const char* functionName; };

int main(int argc, char** argv)
{
	if (argc < 2) { fprintf(stderr, "usage: %s <path to Jenova.Runtime.Linux64.so>\n", argv[0]); return 2; }

	void* runtime = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
	assert(runtime && "Runtime Failed to Load.");

	auto installHandlers = (bool(*)())dlsym(runtime, JENOVA_SYMBOL_INSTALL_HANDLERS);
	auto removeHandlers = (bool(*)())dlsym(runtime, JENOVA_SYMBOL_REMOVE_HANDLERS);
	auto executingScript = (const ScriptExecutionIdentity**)dlsym(runtime, JENOVA_SYMBOL_EXECUTING_SCRIPT);
	assert(installHandlers && removeHandlers && executingScript && "Diagnostic Symbols Missing.");

	// Removing before installing, and installing twice, are both no-ops.
	assert(removeHandlers());
	assert(installHandlers());
	assert(installHandlers());
	assert(removeHandlers());

	// Fault in a child with an execution context set, and read back what it printed.
	int reportPipe[2];
	assert(pipe(reportPipe) == 0);
	pid_t child = fork();
	assert(child >= 0);
	if (child == 0)
	{
		dup2(reportPipe[1], STDERR_FILENO);
		close(reportPipe[0]);
		close(reportPipe[1]);
		installHandlers();
		static const ScriptExecutionIdentity identity = { "res://src/hud.cpp", "_ready" };
		*executingScript = &identity;
		volatile int* nullNode = nullptr;
		*nullNode = 1;					// What GetSelf() returning null leads to one line later.
		_exit(0);
	}
	close(reportPipe[1]);

	std::string report;
	char chunk[512];
	ssize_t received;
	while ((received = read(reportPipe[0], chunk, sizeof(chunk))) > 0) report.append(chunk, size_t(received));
	close(reportPipe[0]);

	int childStatus = 0;
	waitpid(child, &childStatus, 0);

	assert(report.find("SIGSEGV") != std::string::npos && "Report Does Not Name the Signal.");
	assert(report.find("res://src/hud.cpp") != std::string::npos && "Report Does Not Name the Script.");
	assert(report.find("_ready") != std::string::npos && "Report Does Not Name the Function.");
	assert(report.find("Probable Cause") != std::string::npos && "Report Does Not Explain a Null Dereference.");
	assert(report.find("Backtrace") != std::string::npos && "Report Carries No Backtrace.");
	// No script call armed a recovery point here, so the fault must still kill the process
	// rather than being swallowed. Recovery is only for faults raised inside a script call.
	assert(WIFSIGNALED(childStatus) && WTERMSIG(childStatus) == SIGSEGV && "Fault Was Swallowed Instead of Killing the Process.");

	printf("Jenova Fatal Signal Reporting : OK\n");
	return 0;
}
