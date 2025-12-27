

#include <iostream>
#include <thread>
#include <sstream>
#include <iomanip>
#include <filesystem>

#include "Memory/MemoryManager.h" 

#include "Misc/Colours.h"
#include "Misc/Console.h"

#include "OverlayHook/OverlayHook.h"

#include "Hacks/misc.h"
#include "Hacks/movement.h"

#include "RBX/PlayerCache.h"
#include "RBX/PlayerObjectsCache.h"
#include "RBX/TPHandler.h"

#include "Globals.h"

#include "RPC/Discord.h"

bool IsGameRunning(const wchar_t* windowTitle)
{
    HWND hwnd = FindWindowW(NULL, windowTitle);
    return hwnd != NULL;
}

std::string GetExecutableDir()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    return exePath.parent_path().string();
}

namespace fs = std::filesystem;

void RobloxVersion() {
    char username[256];
    DWORD size = sizeof(username);
    GetUserNameA(username, &size);
    std::string versionsPath = "C:\\Users\\" + std::string(username) + "\\AppData\\Local\\Roblox\\Versions";
    std::string latestVersionName = "Unknown";
    FILETIME latestTime = { 0, 0 };

    try {
        for (const auto& entry : fs::directory_iterator(versionsPath)) {
            if (entry.is_directory()) {
                FILETIME ft;
                HANDLE hFile = CreateFileA(
                    entry.path().string().c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    nullptr
                );

                if (hFile != INVALID_HANDLE_VALUE) {
                    if (GetFileTime(hFile, nullptr, nullptr, &ft)) {
                        if (CompareFileTime(&ft, &latestTime) > 0) {
                            latestTime = ft;
                            latestVersionName = entry.path().filename().string();
                        }
                    }
                    CloseHandle(hFile);
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error reading Roblox versions: " << e.what() << std::endl;
    }

    std::string title = "Voodoo | " + latestVersionName;
    SetConsoleTitleA(title.c_str());
}

int main()
{

    offsets::autoupdate(); 

    if (!IsGameRunning(L"Roblox"))
    {
        log("Couldnt Find Roblox!", 2);
        log("Please Open Roblox", 0);
        while (!IsGameRunning(L"Roblox"))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    log("Voodoo Found Roblox", 1);

    log("Attaching!", 0);
    if (!Memory->attachToProcess("RobloxPlayerBeta.exe"))
    {
        log("Sorry Voodoo Couldnt Attach", 3);
        log("Press any key to exit...", 0);
        std::cin.get();
        return -1;
    }
    
    if (Memory->getProcessId("RobloxPlayerBeta.exe") == 0)
    {
        log("Couldn't Get Roblox's PID", 3);
        log("Press any key to exit...", 0);
        std::cin.get();
        return -1;
    }
    log("If DataModel Didnt Show, YOUR ON THE WRONG VERSION!", 2);
    log(" --- DEBUG ---", 0);
    log(std::string("PID: " + std::to_string(Memory->getProcessId())), 0);
    log(std::string("Base: 0x" + toHexString(std::to_string(Memory->getBaseAddress()), false, true)), 0);

    Globals::executablePath = GetExecutableDir();

    struct stat buffer;

    Globals::configsPath = Globals::executablePath + "\\configs";

    if (stat(Globals::configsPath.c_str(), &buffer) != 0)
    {
        std::filesystem::create_directory(Globals::configsPath);
    }

    auto fakeDataModel = Memory->read<uintptr_t>(Memory->getBaseAddress() + offsets::FakeDataModelPointer);
    auto dataModel = RobloxInstance(Memory->read<uintptr_t>(fakeDataModel + offsets::FakeDataModelToDataModel));

    while (dataModel.Name() != "Ugc")
    {
        fakeDataModel = Memory->read<uintptr_t>(Memory->getBaseAddress() + offsets::FakeDataModelPointer);
        dataModel = RobloxInstance(Memory->read<uintptr_t>(fakeDataModel + offsets::FakeDataModelToDataModel));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    Globals::Roblox::DataModel = dataModel;

    auto visualEngine = Memory->read<uintptr_t>(Memory->getBaseAddress() + offsets::VisualEnginePointer);

    while (visualEngine == 0)
    {
        visualEngine = Memory->read<uintptr_t>(Memory->getBaseAddress() + offsets::VisualEnginePointer);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    Globals::Roblox::VisualEngine = visualEngine;

    Globals::Roblox::Workspace = Globals::Roblox::DataModel.FindFirstChildWhichIsA("Workspace");
    Globals::Roblox::Players = Globals::Roblox::DataModel.FindFirstChildWhichIsA("Players");
    Globals::Roblox::Camera = Globals::Roblox::Workspace.FindFirstChildWhichIsA("Camera");

    Globals::Roblox::LocalPlayer = RobloxInstance(Memory->read<uintptr_t>(Globals::Roblox::Players.address + offsets::LocalPlayer));

    Globals::Roblox::lastPlaceID = Memory->read<int>(Globals::Roblox::DataModel.address + offsets::PlaceId);;
    log(std::string("DataModel: 0x" + toHexString(std::to_string(Globals::Roblox::DataModel.address), false, true)), 0);
    log(std::string("Render Engine: 0x" + toHexString(std::to_string(Globals::Roblox::VisualEngine), false, true)), 0);

    log(std::string("Workspace: 0x" + toHexString(std::to_string(Globals::Roblox::Workspace.address), false, true)), 0);
    log(std::string("Players: 0x" + toHexString(std::to_string(Globals::Roblox::Players.address), false, true)), 0);
    log(std::string("Camera: 0x" + toHexString(std::to_string(Globals::Roblox::Camera.address), false, true)), 0);
    log("FINISHED DEBUGGING, Voodoo has Loaded [Delete to Open/Close Menu]", 1);
    log("Welcome to Voodoo, " + Globals::Roblox::LocalPlayer.Name(), 0);
    

    std::thread(RobloxVersion).detach();
    std::thread(ShowImgui).detach();
    std::thread(CachePlayers).detach();
    std::thread(CachePlayerObjects).detach();
    std::thread(TPHandler).detach();
    std::thread(MiscLoop).detach();
	std::thread(Fly).detach();

	DiscordRPC rpc("1441911643133968555"); // your app id

    while (true) {
        rpc.pumpCallbacks();

        static auto last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (now - last > std::chrono::seconds(2)) {
            rpc.updatePresence(true);
            last = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cin.get();
    return 1;
    
}

// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync
// made by zync