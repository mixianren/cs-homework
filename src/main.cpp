/* ============================================================================
 *  src/main.cpp  --  程序入口
 * ----------------------------------------------------------------------------
 *  医院门诊预约与智能导诊系统
 *  数据结构课程设计 · 第 1 题
 *
 *  ----------------------------------------------------------------------------
 *  启动流程（对应设计说明书 3.3 节）
 *
 *      1. 设置控制台编码为 UTF-8，保证中文正常显示
 *      2. 从 files/ 目录加载全部数据到 DataStore
 *      3. 关键文件缺失时打印错误并退出
 *      4. 打印加载统计
 *      5. 进入主菜单循环
 *
 *  本文件只做这五件事，不含任何业务逻辑 ——
 *  这样"换一个界面"只需要改 menu/，入口逻辑基本不动。
 * ==========================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>   /* SetConsoleOutputCP / SetConsoleCP —— 切换控制台代码页。
                          注意：不要用 system("chcp 65001")，理由见下方注释。 */
#endif

#include "data/DataStore.h"
#include "file/FileLoader.h"
#include "menu/Menu.h"
#include "util/Console.h"

/* ==========================================================================
 *  控制台编码
 * --------------------------------------------------------------------------
 *  Windows 控制台默认使用本地代码页（简体中文是 GBK）。本工程的源码与数据
 *  文件都是 UTF-8，不切换代码页输出的中文就是乱码。
 *
 *  【这里有一个踩过的坑，务必不要改回 system("chcp 65001")】
 *
 *  std::system() 在 Windows 上通过启动 cmd.exe 执行命令。子进程会继承标准
 *  句柄，结果是父进程 CRT 里 stdin 的内部状态被破坏 —— 表现为之后**任何**
 *  读入都立刻失败：
 *
 *      调用 system("chcp 65001 > nul") 后：fgets -> NULL, feof=16（异常值）
 *      不调用 system()：                    fgets -> ok
 *
 *  更麻烦的是这个破坏**无法用 _dup2 恢复**：把文件描述符复制回来之后，
 *  fgets 依然失败，因为坏掉的是 FILE 对象内部的状态而不是描述符本身。
 *  （实测：_dup(0)=3 → system → _dup2(3,0)=0 成功 → fgets 仍然 NULL。）
 *
 *  症状是"主菜单一进去就提示输入无效然后退出"，而单独写个只调用 readLine
 *  的小程序却完全正常 —— 很难一眼看出是 chcp 造成的。
 *
 *  解决办法：用 Win32 API 直接设置代码页，不启动任何子进程。
 *  SetConsoleOutputCP 管输出，SetConsoleCP 管输入。
 *  输出被重定向时这两个调用会失败，但那不影响程序运行，所以忽略返回值。
 * ==========================================================================*/
static void setupConsoleEncoding()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);   /* 输出用 UTF-8 */
    SetConsoleCP(65001);         /* 输入用 UTF-8 */
#endif
}

/* ==========================================================================
 *  数据目录定位
 * --------------------------------------------------------------------------
 *  数据文件放在工程根目录的 files\ 下。程序启动时**从可执行文件所在目录开
 *  始向上查找**这个目录，最多向上 5 层，返回找到的绝对路径。
 *
 *  【为什么要从 exe 的位置找，而不是从当前工作目录找】
 *  双击 bin\hospital.exe 时工作目录是 bin\；从别处用完整路径启动时工作目录
 *  是那个"别处"。这两种情况下工作目录都与数据文件的位置无关，从工作目录
 *  向上找必然失败（实测：从 C:\HOMEWORK 启动时找不到）。
 *
 *  而可执行文件的位置是确定的：它总在 <工程根>\bin\ 下。
 *  从它开始向上走一层就是工程根，无论使用者怎么启动都能找到数据。
 *
 *  找不到时返回 false，由调用方打印排查提示 —— 不会静默跑一个空系统。
 * ==========================================================================*/
static const char* DATA_DIR_NAME = "files";
static const int   MAX_UP_LEVELS = 5;

/* 判断某个目录下是否有数据文件（用 departments.txt 作为标志）。
   注意：char* 与 const char* 这里都用 const 版本，避免类型转换。 */
static bool hasDataFileIn(const char* dir)
{
    char probe[1024];
    std::snprintf(probe, sizeof(probe), "%s\\%s\\departments.txt", dir, DATA_DIR_NAME);

    std::FILE* fp = std::fopen(probe, "rb");
    if (fp == 0) return false;
    std::fclose(fp);
    return true;
}

/* 从可执行文件所在目录开始向上查找数据目录。
   找到返回 true 并把绝对路径写入 out（形如 C:\HOMEWORK\HospitalMS\files）。 */
static bool locateDataDir(char* out, int cap)
{
    if (out == 0 || cap <= 0) return false;
    out[0] = '\0';

    /* 取可执行文件的完整路径 */
    char exePath[1024];
    DWORD n = GetModuleFileNameA(0, exePath, (DWORD)sizeof(exePath));
    if (n == 0 || n >= sizeof(exePath))
    {
        /* 取不到就退回当前目录 */
        std::snprintf(exePath, sizeof(exePath), ".");
    }

    /* 去掉文件名，得到所在目录 */
    int len = (int)std::strlen(exePath);
    while (len > 0 && exePath[len - 1] != '\\' && exePath[len - 1] != '/') len--;
    if (len > 0) exePath[len - 1] = '\0';        /* 去掉结尾的分隔符 */
    else         std::snprintf(exePath, sizeof(exePath), ".");

    /* 从这一层开始逐级向上试 */
    char candidate[1024];
    for (int level = 0; level <= MAX_UP_LEVELS; level++)
    {
        if (level == 0)
            std::snprintf(candidate, sizeof(candidate), "%s", exePath);
        else
        {
            int clen = (int)std::strlen(candidate);

            /* 已经在根目录（形如 C: 或 C:\）就停 */
            if (clen <= 3) break;

            /* 砍掉最后一段 */
            while (clen > 0 && candidate[clen - 1] != '\\' && candidate[clen - 1] != '/') clen--;
            while (clen > 0 && (candidate[clen - 1] == '\\' || candidate[clen - 1] == '/')) clen--;
            candidate[clen] = '\0';

            if (clen == 0) break;
        }

        char full[1024];
        std::snprintf(full, sizeof(full), "%s\\%s", candidate, DATA_DIR_NAME);

        if (hasDataFileIn(candidate))
        {
            std::snprintf(out, (size_t)cap, "%s", full);
            return true;
        }
    }

    return false;
}

/* ==========================================================================
 *  main
 * ==========================================================================*/
int main()
{
    setupConsoleEncoding();

    printTitleBox("医院门诊预约与智能导诊系统");
    std::printf("  数据结构课程设计 · 第 1 题\n");

    /* ---------------- 0. 定位数据目录 ---------------- */
    char dataDir[256];
    bool dirFound = locateDataDir(dataDir, sizeof(dataDir));

    /* ---------------- 1. 加载数据 ---------------- */
    std::printf("\n");
    printRule(70);
    if (dirFound)
        std::printf("  正在加载数据（目录：%s）\n", dataDir);
    else
        std::printf("  正在加载数据（目录：未找到）\n");
    printRule(70);

    DataStore* store = new DataStore();

    /* 数据目录没找到时给出明确的排查提示 */
    if (!dirFound)
    {
        std::printf("\n");
        std::printf("  【错误】从程序所在目录向上找了 %d 层，都没有找到 %s 目录\n\n",
                    MAX_UP_LEVELS, DATA_DIR_NAME);
        std::printf("  请确认：\n");
        std::printf("    1. 工程目录结构完整，根目录下有 files\\ 子目录\n");
        std::printf("    2. files\\ 下有 departments.txt 等 6 个数据文件\n");
        std::printf("    3. 如果数据文件丢失，可在工程根目录运行  build gendata  重新生成\n\n");
        std::printf("  最省事的做法：双击工程根目录下的  运行.bat\n\n");

        delete store;
        return 1;
    }

    /* 用定位到的绝对路径构造加载器 */
    {
        FileLoader loader(dataDir);
        bool loaded = loader.loadAll(store);

        std::printf("\n");
        loader.report().print();

        if (!loaded)
        {
            std::printf("\n");
            std::printf("  【错误】关键数据缺失，程序无法继续。\n");
            std::printf("  科室表与医生表是最基本的数据，请检查上述告警。\n\n");
            delete store;
            return 1;
        }
    }

    /* ---------------- 2. 打印加载统计 ---------------- */
    std::printf("\n");
    store->printSummary();

    /* 索引一致性自检：万一数据文件被手工改坏了，这里能立刻发现 */
    {
        char report[4096];
        int problems = store->selfCheck(report, sizeof(report));
        if (problems > 0)
        {
            std::printf("\n  数据自检发现 %d 个问题：\n", problems);
            std::printf("%s", report);
            std::printf("  程序仍会继续运行，但结果可能不正确。\n");
        }
    }

    /* ---------------- 3. 进入主菜单 ---------------- */
    Menu menu(store);
    bool normalExit = menu.mainLoop();

    delete store;

    if (!normalExit)
    {
        std::printf("\n  输入流已结束，程序退出。\n\n");
    }

    return 0;
}
