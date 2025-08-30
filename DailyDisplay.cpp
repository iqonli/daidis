#include <windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <unordered_map>

// 全局常量定义
#define APP_NAME _T("DailyDisplay")
#define APP_NAME_SHORT _T("daidis")
#define WND_CLASS_NAME _T("DailyDisplayClass")
#define TRAY_ICON_ID 1003
#define SHUTDOWN_WND_CLASS_NAME _T("ShutdownWindowClass")
#define SHUTDOWN_WND_ID 1004

// 自定义消息ID
#define WM_TRAY_MESSAGE WM_USER + 1
#define WM_TIMER_REFRESH_TIME WM_USER + 2
#define WM_TIMER_RELOAD_CONFIG WM_USER + 3
#define WM_SHUTDOWN_COUNTDOWN WM_USER + 4
#define WM_SHUTDOWN_CANCEL WM_USER + 5
#define WM_TIMER_SHUTDOWN_REMIND WM_USER + 6
#define WM_TIMER_SHUTDOWN_CHECK WM_USER + 8
#define WM_TIMER_SHUTDOWN_TOPMOST WM_USER + 9
#define WM_TIMER_DAILY_RESET WM_USER + 10

using namespace std;

// 全局变量
string nameee = "DailyDisplay";//名称
string verrr = "0.1";//版本
string iqonli = "IQ Online Studio";//作者
string iqonliURL = "github.com/iqonli/daidis";//网址
string iqonliINFO = "by IQ Online Studio, github.com/iqonli/daidis";//版权
char nulll;
static HFONT g_hDefaultFont = NULL;
static NOTIFYICONDATA g_nid = {0};
static BOOL g_minimizeToTray = TRUE;
static HWND g_hMainWnd = NULL;
static HWND g_hShutdownWnd = NULL;
static string g_configFilePath = "config.txt"; // 修改为相对路径，更容易访问
static vector<string> g_configLines;
static string g_configLastModified = "";
static SYSTEMTIME g_currentTime;
static int g_refreshTimeInterval = 50; // 50ms刷新时间信息
static int g_reloadConfigInterval = 500; // 500ms重新读取配置
static int g_shutdownWindowRefreshTop = 500; // 500ms刷新关机窗口置顶
static COLORREF g_backColor = RGB(0, 0, 0); // 修改为黑色背景，确保文本可见
static COLORREF g_defaultFontColor = RGB(255, 255, 255); // 默认白色文本
static int g_defaultFontSize = 16; // 默认字号16
static string g_defaultFontName = "Microsoft YaHei UI";// 默认字体微软雅黑
static int g_shutdownCountdown = 0; // 当前关机倒计时（秒）
// 修正全局变量初始化，避免随机值
static BOOL g_isReloading = FALSE;
// 用于检测[start]配置变化
static string g_prevStartConfig = "";

// 结构体定义
struct DisplayLine
{
	string content;
	int fontSize;
	COLORREF fontColor;
	string fontName; // 新增：字体名称
	bool valid;
	string errorMsg;
};

struct ShutdownPlan
{
	SYSTEMTIME shutdownTime; // 关机时间（仅时分秒有效，日期每日更新）
	int warningSeconds; // 关机前提醒时间（秒）
	string message; // 关机提示消息
	BOOL isActive; // 是否激活（当天是否有效）
	BOOL isDaily; // 是否每日重复（默认TRUE）
	BOOL isCancelledToday; // 当天是否已取消（默认FALSE）
};

static vector<DisplayLine> g_displayLines;
static vector<ShutdownPlan> g_shutdownPlans; // 关机计划列表
static string g_shutdownMessage; // 关机提示消息（临时使用）

// 用于缓存[app]和[appline]命令的输出结果
struct AppOutputCache {
    string lastOutput;       // 上次获取的输出
    DWORD lastUpdateTime;         // 上次更新时间（毫秒）
    DWORD refreshInterval;        // 刷新间隔（毫秒，默认-1表示每次都刷新）
};

// 存储不同命令的缓存，键为命令字符串（包含参数）
static unordered_map<string, AppOutputCache> g_appOutputCache;

// 全局变量新增：行间距（按font字号自动计算）
static int g_lineSpacing = 8; // 默认行间距
// 新增：字体间距参数
static int g_fontTopMargin = 10; // 默认字体上间距
static int g_fontLeftMargin = 5; // 默认字体左间距
static int g_fontRightMargin = 5; // 默认字体右间距
// 新增：窗口标题相关
static string g_finalWindowTitle = ""; // 最终窗口标题
#define WM_TITLE_CHANGE_TIMER WM_USER + 11 // 标题切换定时器ID

// 全局辅助函数：数字补零（转为2位字符串）
string toTwoDigits(int num)
{
	return num < 10 ? "0" + to_string(num) : to_string(num);
}

double fabs(double a)
{
	if(a >= 0.00)
	{
		return a;
	}
	else
	{
		return -a;
	}
}

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ShutdownWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL RegisterMainWindowClass(HINSTANCE hInst);
BOOL RegisterShutdownWindowClass(HINSTANCE hInst);
HWND CreateMainWindow(HINSTANCE hInst);
HWND CreateShutdownWindow(HINSTANCE hInst);
void AddTrayIcon(HWND hWnd, HICON hIcon, const TCHAR* tip);
void RemoveTrayIcon(HWND hWnd);
void ShowTrayBalloon(HWND hWnd, const TCHAR* title, const TCHAR* message, DWORD iconType = NIIF_INFO);
void HideConsoleWindow(BOOL hide);
string LoadConfigFile();
void ParseConfigFile(const string& configContent);
void RefreshDisplay();
string ProcessSpecialValues(const string& line);
string GetCurrentDateTimeString();
BOOL ParseStartCommand(const string& params, HWND* hWnd, HINSTANCE hInst);
BOOL ParseShutdownCommand(const string& params);
BOOL ParseStyleCommand(const string& params, DisplayLine& line);
BOOL ParseFontCommand(const string& params);
BOOL ParseBackColorCommand(const string& params);
string ReplaceTimeVariables(const string& input);
string ProcessToCommand(const string& command, const string& params);
string ProcessCycleCommand(const string& command, const string& params);
string ProcessDateDistanceCommand(const string& command, const string& params);
string ProcessTimeDistanceCommand(const string& command, const string& params);
string ProcessDateTimeDistanceCommand(const string& command, const string& params);
void ScheduleShutdown();
void CancelScheduledShutdown();
wstring StringToWString(const string& s);
string WStringToString(const wstring& ws);
void StartASyncAPP(const string& appPath);
string StartAppAndGetOutput(const string& appPath);
FILETIME GetPlanEffectiveTime(const ShutdownPlan& plan, const SYSTEMTIME& now);

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// 隐藏控制台窗口
	HideConsoleWindow(TRUE);
	
	// 注册窗口类
	if (!RegisterMainWindowClass(hInstance) || !RegisterShutdownWindowClass(hInstance))
	{
		MessageBox(NULL, _T("窗口类注册失败！"), _T("错误"), MB_ICONERROR | MB_OK);
		return 1;
	}

	// 加载配置文件
	string configContent = LoadConfigFile();
	if (configContent.empty())
	{
		// 如果配置文件不存在，创建默认配置
		ofstream configFile(g_configFilePath);
		if (configFile.is_open())
		{
			configFile << "[start]{0,0,800,600,255,0,1,0,0,0,0,0,1,0,0,0,0,-1}\n";
			configFile << "[yyyy]-[mm]-[dd] [h]:[m]:[s]\n";
			configFile << iqonliINFO << endl;
			configFile.close();
			configContent = LoadConfigFile();
		}
	}

	// 解析配置并创建主窗口
	ParseConfigFile(configContent);
	if (!g_hMainWnd)
	{
		// 如果配置中没有[start]指令，创建默认窗口
		g_hMainWnd = CreateMainWindow(hInstance);
	}

	if (!g_hMainWnd)
	{
		MessageBox(NULL, _T("创建主窗口失败！"), _T("错误"), MB_ICONERROR | MB_OK);
		return 1;
	}

	// 添加托盘图标
	HICON hIcon = LoadIcon(NULL, IDI_APPLICATION);
	AddTrayIcon(g_hMainWnd, hIcon, APP_NAME);

	// 设置定时器
	SetTimer(g_hMainWnd, WM_TIMER_REFRESH_TIME, g_refreshTimeInterval, NULL);
	SetTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG, g_reloadConfigInterval, NULL);

	// 初始化每日重置定时器（零点恢复每日计划）
	SYSTEMTIME now;
	GetLocalTime(&now);
	SYSTEMTIME nextResetTime = now;
	nextResetTime.wHour = 0;
	nextResetTime.wMinute = 0;
	nextResetTime.wSecond = 0;
	nextResetTime.wMilliseconds = 0;
	FILETIME ftNext, ftNow;
	SystemTimeToFileTime(&nextResetTime, &ftNext);
	SystemTimeToFileTime(&now, &ftNow);
	ULARGE_INTEGER ulNext, ulNow;
	ulNext.LowPart = ftNext.dwLowDateTime;
	ulNext.HighPart = ftNext.dwHighDateTime;
	ulNow.LowPart = ftNow.dwLowDateTime;
	ulNow.HighPart = ftNow.dwHighDateTime;
	
	// 若当前已过零点，重置为明天零点
	if (ulNow.QuadPart > ulNext.QuadPart)
	{
		ulNext.QuadPart += 86400LL * 10000000LL;
	}
	
	// 计算延迟（毫秒）
	DWORD delay = (DWORD)((ulNext.QuadPart - ulNow.QuadPart) / 10000);
	SetTimer(g_hMainWnd, WM_TIMER_DAILY_RESET, delay, NULL);
	
	// 启动消息循环
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 清理资源
	if (g_hDefaultFont)
	{
		DeleteObject(g_hDefaultFont);
		g_hDefaultFont = NULL;
	}
	RemoveTrayIcon(g_hMainWnd);
	UnregisterClass(WND_CLASS_NAME, hInstance);
	UnregisterClass(SHUTDOWN_WND_CLASS_NAME, hInstance);

	return (int)msg.wParam;
}

// 隐藏或显示控制台窗口
void HideConsoleWindow(BOOL hide)
{
	HWND hConsole = GetConsoleWindow();
	if (hConsole)
	{
		LONG style = GetWindowLongPtr(hConsole, GWL_EXSTYLE);
		if (hide)
		{
			// 隐藏窗口并从Alt+Tab中移除
			style |= WS_EX_TOOLWINDOW;
			style &= ~WS_EX_APPWINDOW;
			ShowWindow(hConsole, SW_HIDE);
		}
		else
		{
			// 显示窗口并添加到Alt+Tab
			style &= ~WS_EX_TOOLWINDOW;
			style |= WS_EX_APPWINDOW;
			ShowWindow(hConsole, SW_SHOW);
		}
		SetWindowLongPtr(hConsole, GWL_EXSTYLE, style);
	}
}

// 注册主窗口类
BOOL RegisterMainWindowClass(HINSTANCE hInst)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = WND_CLASS_NAME;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		return FALSE;
	}
	return TRUE;
}

// 注册关机提示窗口类
BOOL RegisterShutdownWindowClass(HINSTANCE hInst)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = ShutdownWndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = SHUTDOWN_WND_CLASS_NAME;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hIcon = LoadIcon(NULL, IDI_WARNING);
	wc.hIconSm = LoadIcon(NULL, IDI_WARNING);

	if (!RegisterClassEx(&wc))
	{
		return FALSE;
	}
	return TRUE;
}

// 创建主窗口
HWND CreateMainWindow(HINSTANCE hInst)
{
	// 默认窗口设置
	int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
	int width = 800, height = 600;
	BYTE alphaValue = 255;
	BOOL enableMouseThrough = FALSE;
	BOOL hideTaskbar = TRUE;
	BOOL hideAltTab = FALSE;
	BOOL partialTopmost = FALSE;
	BOOL fullTopmost = TRUE;
	BOOL hasMinButton = FALSE;
	BOOL hasMaxButton = FALSE;
	BOOL hasCloseButton = FALSE;
	BOOL showTitleBar = FALSE;
	BOOL startMaximized = FALSE;
	BOOL startMinimized = FALSE;
	int autoTopmostInterval = -1;

	DWORD exStyle = WS_EX_CLIENTEDGE;
	DWORD style = WS_THICKFRAME | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	if (enableMouseThrough)
	{
		exStyle |= WS_EX_TRANSPARENT;
	}
	exStyle |= WS_EX_LAYERED;

	if (showTitleBar)
	{
		style |= WS_CAPTION;
	}
	if (hasMinButton) style |= WS_MINIMIZEBOX;
	if (hasMaxButton) style |= WS_MAXIMIZEBOX;
	if (hasCloseButton) style |= WS_SYSMENU;

	if (hideAltTab || hideTaskbar)
	{
		exStyle |= WS_EX_TOOLWINDOW;
		if (!hideTaskbar)
		{
			exStyle |= WS_EX_APPWINDOW;
		}
	}

	if (fullTopmost)
	{
		exStyle |= WS_EX_TOPMOST;
		partialTopmost = FALSE;
	}

	HWND hWnd = CreateWindowEx(
	                exStyle,
	                WND_CLASS_NAME,
	                APP_NAME,
	                style,
	                x, y, width, height,
	                NULL,
	                NULL,
	                hInst,
	                NULL
	            );

	if (!hWnd)
	{
		return NULL;
	}

	// 设置窗口透明度
	SetLayeredWindowAttributes(hWnd, 0, alphaValue, LWA_ALPHA);

	// 部分置顶设置
	if (partialTopmost && !fullTopmost)
	{
		SetWindowPos(
		    hWnd,
		    HWND_TOP,
		    0, 0, 0, 0,
		    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
		);
	}

	// 初始化默认字体
	g_hDefaultFont = CreateFontA(
	                     14, 0, 0, 0, FW_NORMAL,
	                     FALSE, FALSE, FALSE,
	                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
	                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
	                     DEFAULT_PITCH | FF_DONTCARE,
	                     "Microsoft YaHei UI"
	                 );

	// 显示窗口
	int showCmd = SW_SHOW;
	if (startMaximized) showCmd = SW_SHOWMAXIMIZED;
	else if (startMinimized)
	{
		showCmd = SW_SHOWMINIMIZED;
		if (g_minimizeToTray)
		{
			ShowWindow(hWnd, SW_HIDE);
		}
	}

	ShowWindow(hWnd, showCmd);
	UpdateWindow(hWnd);

	return hWnd;
}

// 创建关机提示窗口
HWND CreateShutdownWindow(HINSTANCE hInst)
{
	if (g_hShutdownWnd)
	{
		DestroyWindow(g_hShutdownWnd);
		g_hShutdownWnd = NULL;
	}
	
	// 获取屏幕尺寸
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int wndWidth = 1000;
	int wndHeight = 600;
	int x = (screenWidth - wndWidth) / 2;  // 保持居中计算
	int y = (screenHeight - wndHeight) / 2;// 保持居中计算
	
	DWORD exStyle = WS_EX_CLIENTEDGE | WS_EX_TOPMOST;  // 保留强制置顶扩展样式
	DWORD style = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	
	HWND hWnd = CreateWindowEx(
							   exStyle,
							   SHUTDOWN_WND_CLASS_NAME,
							   _T("关机提示"),
							   style,
							   x, y, wndWidth, wndHeight,
							   NULL,
							   NULL,
							   hInst,
							   NULL
							   );
	
	if (!hWnd)
	{
		return NULL;
	}
	
	// 设置窗口图标
	HICON hIcon = LoadIcon(NULL, IDI_WARNING);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	
	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	SetForegroundWindow(hWnd);
	
	// 开始倒计时定时器（原逻辑保留）
	SetTimer(hWnd, WM_SHUTDOWN_COUNTDOWN, 1000, NULL);
	// 新增：500ms刷新一次置顶状态
	SetTimer(hWnd, WM_TIMER_SHUTDOWN_TOPMOST, g_shutdownWindowRefreshTop, NULL);
	
	return hWnd;
}

// 添加托盘图标
void AddTrayIcon(HWND hWnd, HICON hIcon, const TCHAR* tip)
{
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hWnd;
	g_nid.uID = TRAY_ICON_ID;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAY_MESSAGE;
	g_nid.hIcon = hIcon;
	_tcscpy_s(g_nid.szTip, tip);

	Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// 移除托盘图标
void RemoveTrayIcon(HWND hWnd)
{
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hWnd;
	g_nid.uID = TRAY_ICON_ID;
	Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// 显示托盘气泡提示
void ShowTrayBalloon(HWND hWnd, const TCHAR* title, const TCHAR* message, DWORD iconType)
{
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hWnd;
	g_nid.uID = TRAY_ICON_ID;
	g_nid.uFlags = NIF_INFO;
	_tcscpy_s(g_nid.szInfoTitle, title);
	_tcscpy_s(g_nid.szInfo, message);
	g_nid.dwInfoFlags = iconType;

	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// 加载配置文件
string LoadConfigFile()
{
	string content;
	ifstream file(g_configFilePath); // 尝试打开配置文件

	// 1. 检查文件是否成功打开 + 是否可读
	if (file.is_open())
	{
		// 额外检查文件状态：避免文件存在但损坏/不可读的情况
		if (!file.good())
		{
			file.close(); // 先关闭异常文件
			MessageBoxA(NULL, "配置文件存在但不可读（可能损坏或被占用）", "配置加载错误", MB_ICONERROR | MB_OK);
			return content; // 返回空内容，触发后续默认配置创建
		}

		// 统一读取逻辑：用 istreambuf_iterator 读取，保留原文件格式（不含多余换行）
		content.assign(
		    (istreambuf_iterator<char>(file)),
		    istreambuf_iterator<char>()
		);
		file.close(); // 及时关闭文件，避免句柄泄漏
	}
	else
	{
		// 2. 配置文件不存在：创建默认配置文件
		ofstream defaultConfigFile(g_configFilePath);
		if (defaultConfigFile.is_open())
		{
			// 写入默认配置内容（格式与用户示例一致，避免语法错误）
			defaultConfigFile << "[start]{0,0,800,600,255,0,1,0,0,1,0,0,0,0,0,0,0,-1}\n";
			defaultConfigFile << "[yyyy]-[mm]-[dd] [h]:[m]:[s]\n";
			defaultConfigFile << "[yyyy]/[0m]/[0d] [h]:[m]:[s]\n";
			defaultConfigFile << "by IQ Online Studio, github.com/iqonli/daidis\n";
			defaultConfigFile << "[backcolor]{0,0,0}\n";
			defaultConfigFile << "[font]{16,255,255,255}\n";
			defaultConfigFile.close(); // 关闭默认配置文件

			// 重新打开刚创建的默认配置，用统一逻辑读取
			ifstream newConfigFile(g_configFilePath);
			if (newConfigFile.is_open() && newConfigFile.good())
			{
				content.assign(
				    (istreambuf_iterator<char>(newConfigFile)),
				    istreambuf_iterator<char>()
				);
				newConfigFile.close();
			}
			else
			{
				MessageBoxA(NULL, "默认配置文件创建失败（可能无写入权限）", "配置创建错误", MB_ICONERROR | MB_OK);
			}
		}
		else
		{
			MessageBoxA(NULL, "无法创建默认配置文件（可能无写入权限）", "配置创建错误", MB_ICONERROR | MB_OK);
		}
	}

	// 3. 记录配置文件最后修改时间（用于后续检测是否需要重新加载）
	WIN32_FILE_ATTRIBUTE_DATA fileInfo = {0};
	if (GetFileAttributesExA(g_configFilePath.c_str(), GetFileExInfoStandard, &fileInfo))
	{
		FILETIME lastWriteFT = fileInfo.ftLastWriteTime;
		SYSTEMTIME lastWriteST = {0};
		FileTimeToSystemTime(&lastWriteFT, &lastWriteST); // 转换为本地时间

		// 格式化修改时间字符串（精确到毫秒，避免重复触发重新加载）
		stringstream ss;
		ss << lastWriteST.wYear << "."
		   << toTwoDigits(lastWriteST.wMonth) << "."  // 补零，确保格式统一
		   << toTwoDigits(lastWriteST.wDay) << " "
		   << toTwoDigits(lastWriteST.wHour) << ":"
		   << toTwoDigits(lastWriteST.wMinute) << ":"
		   << toTwoDigits(lastWriteST.wSecond) << "."
		   << lastWriteST.wMilliseconds;
		g_configLastModified = ss.str(); // 更新全局修改时间变量
	}
	else
	{
		// 无法获取修改时间：重置为空，确保下次强制重新加载
		g_configLastModified = "";
	}

	return content;
}
// 从配置内容的pos位置开始，读取到匹配的右大括号（支持跨行吗）
size_t ReadBraceBlock(const string& configContent, size_t startPos, string& outBlock)
{
	outBlock.clear();
	int braceCount = 1; // 修复：初始值为1（已跳过1个左括号）
	size_t len = configContent.size();
	for (size_t i = startPos; i < len; ++i)
	{
		char c = configContent[i];
		if (c == '{')
			braceCount++;  // 若有嵌套左括号，计数+1
		else if (c == '}')
		{
			braceCount--;  // 遇到右括号，计数-1
			if (braceCount == 0)  // 计数归0，说明找到匹配的右括号
			{
				outBlock = configContent.substr(startPos, i - startPos); // 提取括号内参数（不含右括号）
				return i; // 返回右括号的位置，供后续跳过
			}
		}
		// 忽略换行符（支持参数跨行），但保留其他字符
		if (c != '\n' && c != '\r')
			outBlock += c;
	}
	return string::npos; // 遍历结束仍未找到匹配的右括号，返回错误
}

// 解析配置文件
void ParseConfigFile(const string& configContent)
{
	g_configLines.clear();
	g_displayLines.clear();

	size_t pos = 0;
	size_t len = configContent.size();
	while (pos < len)
	{
		// 跳过空白和换行（保留缩进，支持跨行参数）
		while (pos < len && (configContent[pos] == ' ' || configContent[pos] == '\t' || configContent[pos] == '\n' || configContent[pos] == '\r'))
			pos++;
		if (pos >= len) break;

		// 检查是否为注释行（开头为[]）
		if (pos + 1 < len && configContent[pos] == '[' && configContent[pos + 1] == ']')
		{
			// 跳过整行注释（直到换行）
			while (pos < len && configContent[pos] != '\n') pos++;
			continue;
		}

		// 检查是否为特殊值（以[开头）：区分“独立特殊值”和“文本内嵌特殊值”
		if (configContent[pos] == '[')
		{
			size_t endBracket = configContent.find(']', pos);
			if (endBracket == string::npos)
			{
				// 错误：中括号不匹配（保留残缺内容，避免整行丢失）
				DisplayLine errLine;
				errLine.content = configContent.substr(pos, len - pos);
				errLine.valid = false;
				errLine.errorMsg = "中括号不匹配（残缺内容）";
				errLine.fontSize = g_defaultFontSize;
				errLine.fontColor = g_defaultFontColor;
				g_displayLines.push_back(errLine);
				pos = len;
				continue;
			}

			string command = configContent.substr(pos + 1, endBracket - pos - 1);
			size_t afterBracketPos = endBracket + 1; // 中括号后的位置

			// 检查是否为含参特殊值（紧跟{，支持跨行）
			string params;
			size_t braceEnd = string::npos;
			size_t tempPos = afterBracketPos;
			// 跳过{前的空白/换行（支持跨行参数）
			while (tempPos < len && (configContent[tempPos] == ' ' || configContent[tempPos] == '\t' || configContent[tempPos] == '\n' || configContent[tempPos] == '\r'))
				tempPos++;
			if (tempPos < len && configContent[tempPos] == '{')
			{
				// 读取跨行参数块
				braceEnd = ReadBraceBlock(configContent, tempPos + 1, params);
				if (braceEnd == string::npos)
				{
					// 错误：大括号不匹配（保留已读内容）
					DisplayLine errLine;
					errLine.content = "[" + command + "]{" + configContent.substr(tempPos + 1, len - (tempPos + 1));
					errLine.valid = false;
					errLine.errorMsg = "大括号不匹配（命令：" + command + "）";
					errLine.fontSize = g_defaultFontSize;
					errLine.fontColor = g_defaultFontColor;
					g_displayLines.push_back(errLine);
					pos = len;
					continue;
				}
				pos = braceEnd + 1; // 推进到右大括号后
			}
			else
			{
				// 无参特殊值：检查是否为必须含参的命令
				if (command == "shutdown" || command == "start" || command == "style" || command == "font" || command == "backcolor" || command == "line")
				{
					// 必须含参但无参数：标记错误（保留命令格式）
					DisplayLine errLine;
					errLine.content = "[" + command + "]";
					errLine.valid = false;
					errLine.errorMsg = "特殊值需参数（命令：" + command + "）";
					errLine.fontSize = g_defaultFontSize;
					errLine.fontColor = g_defaultFontColor;
					g_displayLines.push_back(errLine);
					pos = afterBracketPos;
					continue;
				}
				// 非必须含参的无参特殊值（如[123]）：保留内容，不报错
				DisplayLine dl;
				dl.content = "[" + command + "]";
				dl.fontSize = g_defaultFontSize;
				dl.fontColor = g_defaultFontColor;
				dl.valid = true;
				g_displayLines.push_back(dl);
				pos = afterBracketPos;
				continue;
			}

			// 处理已知含参命令
			if (command == "start")
			{
				ParseStartCommand(params, &g_hMainWnd, (HINSTANCE)GetModuleHandle(NULL));
			}
			else if (command == "shutdown")
			{
				// 新格式：[shutdown]{time,rest,"message"}，参数全部在大括号内
				string shutdownParams = "{" + params + "}";
				ParseShutdownCommand(shutdownParams);
			}
			else if (command == "font")
			{
				ParseFontCommand(params);
			}
			else if (command == "backcolor")
			{
				ParseBackColorCommand(params);
			}
			else if (command == "line")
			{
				DisplayLine dl;
				dl.content = "[" + command + "]{" + params + "}";
				dl.fontSize = g_defaultFontSize;
				dl.fontColor = g_defaultFontColor;
				dl.valid = true;
				g_displayLines.push_back(dl);
			}
			else if (command == "shutdowntext")
			{
				DisplayLine dl;
				dl.content = "[shutdowntext]";
				dl.fontSize = g_defaultFontSize;
				dl.fontColor = g_defaultFontColor;
				dl.valid = true;
				g_displayLines.push_back(dl);
			}
			else if (command == "style")
			{
				DisplayLine dl;
				if (!ParseStyleCommand(params, dl))
				{
					dl.valid = false;
					dl.errorMsg = "style参数错误（格式：字号, R, G, B, 字体, \"文本\"）";
					dl.content = "[" + command + "]{" + params + "}";
				}
				dl.fontSize = (dl.valid && dl.fontSize > 0) ? dl.fontSize : g_defaultFontSize;
				dl.fontColor = dl.valid ? dl.fontColor : g_defaultFontColor;
				g_displayLines.push_back(dl);
			}
			else
			{
				// 未知含参特殊值：保留完整格式，不报错（供后续扩展）
				DisplayLine dl;
				dl.content = "[" + command + "]{" + params + "}";
				dl.fontSize = g_defaultFontSize;
				dl.fontColor = g_defaultFontColor;
				dl.valid = true;
				g_displayLines.push_back(dl);
			}
		}
		else
		{
			// 普通文本行：允许包含[]（关键修改！），读取到换行为止
			size_t lineEnd = configContent.find('\n', pos);
			if (lineEnd == string::npos) lineEnd = len;
			// 保留行内所有内容（包括[]和空白）
			string line = configContent.substr(pos, lineEnd - pos);
			pos = lineEnd + 1;

			// 新增：过滤空行（仅含空白字符的行）
			string trimmedLine = line;
			trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t\n\r"));
			trimmedLine.erase(trimmedLine.find_last_not_of(" \t\n\r") + 1);
			if (trimmedLine.empty())
			{
				continue; // 跳过空行
			}

			// 正常添加有效行
			DisplayLine dl;
			dl.content = line;
			dl.fontSize = g_defaultFontSize;
			dl.fontColor = g_defaultFontColor;
			dl.valid = true;
			g_displayLines.push_back(dl);
		}
	}
}

// 解析start指令
BOOL ParseStartCommand(const string& params, HWND* hWnd, HINSTANCE hInst)
{
	// 保存原始start配置用于比较
	static string currentStartConfig;
	currentStartConfig = params;

	// 解析19个参数（新增：第19个参数为主窗口标题）
	vector<string> paramList;
	stringstream ss(params);
	string param;
	while (getline(ss, param, ','))
	{
		param.erase(0, param.find_first_not_of(" \t\n\r"));
		param.erase(param.find_last_not_of(" \t\n\r") + 1);
		paramList.push_back(param);
	}
	// 不足19个参数时补全默认值
	while (paramList.size() < 19) paramList.push_back("");

	// ------------- 修复：带异常处理的参数解析 -------------
	int x = CW_USEDEFAULT;
	if (!paramList[0].empty()) try
		{
			x = stoi(paramList[0]);
		}
		catch (...)
		{
			x = CW_USEDEFAULT;
		}

	int y = CW_USEDEFAULT;
	if (!paramList[1].empty()) try
		{
			y = stoi(paramList[1]);
		}
		catch (...)
		{
			y = CW_USEDEFAULT;
		}

	int width = 800;
	if (!paramList[2].empty()) try
		{
			width = stoi(paramList[2]);
		}
		catch (...)
		{
			width = 800;
		}

	int height = 600;
	if (!paramList[3].empty()) try
		{
			height = stoi(paramList[3]);
		}
		catch (...)
		{
			height = 600;
		}

	BYTE alphaValue = 255;
	if (!paramList[4].empty()) try
		{
			alphaValue = (BYTE)stoi(paramList[4]);
		}
		catch (...)
		{
			alphaValue = 255;
		}

	BOOL enableMouseThrough = FALSE;
	if (!paramList[5].empty()) try
		{
			enableMouseThrough = (stoi(paramList[5]) != 0);
		}
		catch (...)
		{
			enableMouseThrough = FALSE;
		}

	BOOL hideTaskbar = TRUE;
	if (!paramList[6].empty()) try
		{
			hideTaskbar = (stoi(paramList[6]) != 0);
		}
		catch (...)
		{
			hideTaskbar = TRUE;
		}

	BOOL hideAltTab = FALSE;
	if (!paramList[7].empty()) try
		{
			hideAltTab = (stoi(paramList[7]) != 0);
		}
		catch (...)
		{
			hideAltTab = FALSE;
		}

	BOOL partialTopmost = FALSE;
	if (!paramList[8].empty()) try
		{
			partialTopmost = (stoi(paramList[8]) != 0);
		}
		catch (...)
		{
			partialTopmost = FALSE;
		}

	BOOL fullTopmost = TRUE;
	if (!paramList[9].empty()) try
		{
			fullTopmost = (stoi(paramList[9]) != 0);
		}
		catch (...)
		{
			fullTopmost = TRUE;
		}

	BOOL hasMinButton = FALSE;
	if (!paramList[10].empty()) try
		{
			hasMinButton = (stoi(paramList[10]) != 0);
		}
		catch (...)
		{
			hasMinButton = FALSE;
		}

	BOOL hasMaxButton = FALSE;
	if (!paramList[11].empty()) try
		{
			hasMaxButton = (stoi(paramList[11]) != 0);
		}
		catch (...)
		{
			hasMaxButton = FALSE;
		}

	BOOL hasCloseButton = FALSE;
	if (!paramList[12].empty()) try
		{
			hasCloseButton = (stoi(paramList[12]) != 0);
		}
		catch (...)
		{
			hasCloseButton = FALSE;
		}

	BOOL showTitleBar = FALSE;
	if (!paramList[13].empty()) try
		{
			showTitleBar = (stoi(paramList[13]) != 0);
		}
		catch (...)
		{
			showTitleBar = FALSE;
		}

	BOOL startMaximized = FALSE;
	if (!paramList[14].empty()) try
		{
			startMaximized = (stoi(paramList[14]) != 0);
		}
		catch (...)
		{
			startMaximized = FALSE;
		}

	BOOL startMinimized = FALSE;
	if (!paramList[15].empty()) try
		{
			startMinimized = (stoi(paramList[15]) != 0);
		}
		catch (...)
		{
			startMinimized = FALSE;
		}

	g_minimizeToTray = FALSE;
	if (!paramList[16].empty()) try
		{
			g_minimizeToTray = (stoi(paramList[16]) != 0);
		}
		catch (...)
		{
			g_minimizeToTray = FALSE;
		}

	int autoTopmostInterval = -1;
	if (!paramList[17].empty()) try
		{
			autoTopmostInterval = stoi(paramList[17]);
		}
		catch (...)
		{
			autoTopmostInterval = -1;
		}

	// 解析第19个参数：主窗口标题
	string windowTitle = paramList[18];
	// 如果标题为空，则使用默认标题：nameee + " " + verrr
	if (windowTitle.empty())
	{
		windowTitle = nameee + " " + verrr;
	}
	// 处理带引号的标题
	if (windowTitle.size() >= 2 && windowTitle[0] == '"' && windowTitle.back() == '"')
	{
		windowTitle = windowTitle.substr(1, windowTitle.size() - 2);
	}

	// ------------- 修复：窗口销毁与重建 -------------
	if (*hWnd)
	{
		// ① 销毁Shutdown子窗口
		if (g_hShutdownWnd)
		{
			KillTimer(g_hShutdownWnd, WM_SHUTDOWN_COUNTDOWN);
			DestroyWindow(g_hShutdownWnd);
			g_hShutdownWnd = NULL;
		}

		// ② 停止所有主窗口定时器
		KillTimer(*hWnd, WM_TIMER_REFRESH_TIME);
		KillTimer(*hWnd, WM_TIMER_RELOAD_CONFIG);
		KillTimer(*hWnd, WM_TIMER_SHUTDOWN_CHECK);
		KillTimer(*hWnd, WM_USER + 7);

		// ③ 释放旧字体资源（避免GDI泄漏）
		if (g_hDefaultFont)
		{
			DeleteObject(g_hDefaultFont);
			g_hDefaultFont = NULL;
		}

		// ④ 销毁旧主窗口
		DestroyWindow(*hWnd);
		*hWnd = NULL;
	}

	// 窗口样式计算（原逻辑保留 + 修复：确保最大化/最小化按钮显示）
	DWORD exStyle = WS_EX_CLIENTEDGE | WS_EX_LAYERED;
	DWORD style = WS_THICKFRAME | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	if (enableMouseThrough) exStyle |= WS_EX_TRANSPARENT;
	if (showTitleBar) style |= WS_CAPTION;
	if (hasMinButton) style |= WS_MINIMIZEBOX;
	if (hasMaxButton) style |= WS_MAXIMIZEBOX;
	if (hasCloseButton || hasMinButton || hasMaxButton) style |= WS_SYSMENU; // 修复：最大化/最小化按钮需要WS_SYSMENU才能显示
	if (hideAltTab || hideTaskbar)
	{
		exStyle |= WS_EX_TOOLWINDOW;
		if (!hideTaskbar) exStyle |= WS_EX_APPWINDOW;
	}
	if (fullTopmost)
	{
		exStyle |= WS_EX_TOPMOST;
		partialTopmost = FALSE;
	}

	// 保存最终窗口标题
	g_finalWindowTitle = windowTitle;

	// 创建窗口（使用临时标题iqonliINFO）
	*hWnd = CreateWindowEx(
	            exStyle, WND_CLASS_NAME, iqonliINFO.c_str(), style,
	            x, y, width, height, NULL, NULL, hInst, NULL
	        );

	// 设置定时器，5秒后切换到最终标题
	if (*hWnd)
	{
		SetTimer(*hWnd, WM_TITLE_CHANGE_TIMER, 5000, NULL);
	}
	if (!*hWnd)
	{
		MessageBox(NULL, _T("重建主窗口失败！"), _T("错误"), MB_ICONERROR | MB_OK);
		g_isReloading = FALSE; // 重置重入锁
		return FALSE;
	}

	// ------------- 修复：初始化默认字体（原CreateMainWindow逻辑） -------------
	HDC hScreenDC = GetDC(NULL);
	int fontHeight = -MulDiv(g_defaultFontSize, GetDeviceCaps(hScreenDC, LOGPIXELSY), 72);
	ReleaseDC(NULL, hScreenDC);
	// 检查字号有效性（避免异常）
	if (g_defaultFontSize < 1) g_defaultFontSize = 16;
	if (g_hDefaultFont) DeleteObject(g_hDefaultFont);
	g_hDefaultFont = CreateFontA(
	                     fontHeight, 0, 0, 0, FW_NORMAL,
	                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
	                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
	                     g_defaultFontName.c_str()
	                 );
	if (!g_hDefaultFont)
	{
		MessageBox(NULL, _T("创建字体失败！请检查字体名称是否存在（如：Microsoft YaHei UI）"), _T("错误"), MB_ICONERROR | MB_OK);
		DestroyWindow(*hWnd);
		*hWnd = NULL;
		g_isReloading = FALSE; // 重置锁（关键）
		return FALSE;
	}

	// ------------- 修复：处理自动置顶参数（第18个参数） -------------
	if (autoTopmostInterval > 0)
	{
		SetTimer(*hWnd, WM_USER + 7, autoTopmostInterval, NULL); // 新定时器：自动刷新置顶
	}

	// 窗口显示与透明度设置（原逻辑保留）
	SetLayeredWindowAttributes(*hWnd, 0, alphaValue, LWA_ALPHA);
	if (partialTopmost && !fullTopmost)
	{
		SetWindowPos(*hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}
	int showCmd = SW_SHOW;
	if (startMaximized) showCmd = SW_SHOWMAXIMIZED;
	else if (startMinimized)
	{
		showCmd = SW_SHOWMINIMIZED;
		if (g_minimizeToTray) ShowWindow(*hWnd, SW_HIDE);
	}
	ShowWindow(*hWnd, showCmd);
	UpdateWindow(*hWnd);

	// 将当前start配置保存到全局变量
	g_prevStartConfig = currentStartConfig;

	g_isReloading = FALSE;
	return TRUE;
}

// 解析shutdown指令
BOOL ParseShutdownCommand(const string& params)
{
	// 检查是否为标准格式：{...}
	if (!params.empty() && params[0] == '{' && params.find('}') != string::npos)
	{
		size_t endBracket = params.rfind('}');
		if (endBracket == string::npos)
		{
			MessageBox(NULL, _T("endBracket == string::npos"), _T("shutdown指令报错"), MB_ICONERROR | MB_OK);
			return FALSE;
		}
		string innerParams = params.substr(1, endBracket - 1);

		// 新格式：参数必须在大括号内，不能为空
		if (innerParams.empty())
		{
			MessageBox(NULL, _T("shutdown指令参数不能为空"), _T("shutdown指令报错"), MB_ICONERROR | MB_OK);
			return FALSE;
		}

		// 解析参数列表 - 参考ProcessToCommand的实现，保留所有参数
		vector<string> paramList;
		string currentParam;
		bool inQuote = false;

		for (char c : innerParams)
		{
			// 允许处理非ASCII字符（如中文）
			// 不再跳过非ASCII字符，确保中文等字符能正确显示

			// 处理引号
			if (c == '"')
			{
				inQuote = !inQuote;
				currentParam += c;
			}
			// 仅当不在引号内且遇到逗号时，才分割参数
			else if (c == ',' && !inQuote)
			{
				// 保存当前参数（即使为空）
				paramList.push_back(currentParam);
				currentParam.clear();
			}
			// 其他字符直接添加
			else
			{
				currentParam += c;
			}
		}

		// 添加最后一个参数（即使为空）
		paramList.push_back(currentParam);

		// 清理参数引号和空白（参考ProcessToCommand）
		for (auto& p : paramList)
		{
			// 去除前后空白
			size_t start = p.find_first_not_of(" \t\n\r");
			size_t end = p.find_last_not_of(" \t\n\r");
			if (start != string::npos && end != string::npos && start <= end)
			{
				p = p.substr(start, end - start + 1);
			}
			else
			{
				p = "";
			}
			// 去除前后引号（仅当成对时）
			if (p.size() >= 2 && p[0] == '"' && p.back() == '"')
			{
				p = p.substr(1, p.size() - 2);
			}
		}

		// 检查引号是否闭合
		if (inQuote)
		{
			MessageBox(NULL, _T("shutdown指令引号不匹配"), _T("shutdown指令报错"), MB_ICONERROR | MB_OK);
			return FALSE;
		}

		// 确保至少有2个参数，如果不足则补空参数
		while (paramList.size() < 2)
		{
			paramList.push_back("");
		}

		// 显示参数数量信息，保留所有报错以便排查
		stringstream ss;
		ss << "paramList.size() = " << paramList.size();
		if (paramList.size() < 2)
		{
			MessageBoxA(NULL, ss.str().c_str(), "shutdown指令参数数量小于2", MB_ICONERROR | MB_OK);
		}
		else
		{
			// 显示第一个参数内容，帮助调试
			// 114514 (占位用请勿删除)
//			string debugMsg = "参数1: " + paramList[0] + "\n参数2: " + paramList[1];
//			MessageBoxA(NULL, debugMsg.c_str(), "shutdown指令参数详情", MB_ICONINFORMATION | MB_OK);
		}

		try
		{
			// ------------- 修复：支持4位（HHMM）和6位（HHMMSS）时间 -------------
			string timeStr = paramList[0];
			if (timeStr.size() == 4) timeStr += "00"; // 补全秒为00
			else if (timeStr.size() != 6) return FALSE;

			// 解析时间（带范围校验）
			SYSTEMTIME now;
			GetLocalTime(&now);
			ShutdownPlan plan;
			plan.shutdownTime = now;
			plan.shutdownTime.wHour = stoi(timeStr.substr(0, 2));
			plan.shutdownTime.wMinute = stoi(timeStr.substr(2, 2));
			plan.shutdownTime.wSecond = stoi(timeStr.substr(4, 2));
			// 校验时间合法性
			if (plan.shutdownTime.wHour > 23 || plan.shutdownTime.wMinute > 59 || plan.shutdownTime.wSecond > 59)
			{
				return FALSE;
			}

			// 解析警告时间（带范围校验）
			plan.warningSeconds = stoi(paramList[1]);
			plan.warningSeconds = (plan.warningSeconds < 0) ? 0 : (plan.warningSeconds > 3600) ? 3600 : plan.warningSeconds;

			// 解析提示消息
			plan.message = (paramList.size() >= 3) ? paramList[2] : "即将关机！";
			if (!plan.message.empty() && plan.message[0] == '"' && plan.message.back() == '"')
			{
				plan.message = plan.message.substr(1, plan.message.size() - 2);
			}

			plan.isActive = TRUE;
			plan.isDaily = TRUE; // 默认每日重复（可后续扩展参数控制）
			plan.isCancelledToday = FALSE;
			g_shutdownPlans.push_back(plan); // 修复：只添加一次，避免重复计划

			// 按时间顺序排序关机计划
			sort(g_shutdownPlans.begin(), g_shutdownPlans.end(), [](const ShutdownPlan& a, const ShutdownPlan& b)
			{
				// 将时间转换为秒数进行比较
				int timeA = a.shutdownTime.wHour * 3600 + a.shutdownTime.wMinute * 60 + a.shutdownTime.wSecond;
				int timeB = b.shutdownTime.wHour * 3600 + b.shutdownTime.wMinute * 60 + b.shutdownTime.wSecond;
				return timeA < timeB;
			});

			ScheduleShutdown();
			return TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}
}

// 解析style指令
BOOL ParseStyleCommand(const string& params, DisplayLine& line)
{
	vector<string> paramList;
	string currentParam;
	int quoteCount = 0;

	for (char c : params)
	{
		if (c == '"')
		{
			quoteCount++;
			currentParam += c; // 保留引号，后续统一处理
		}
		else if (c == ',' && quoteCount % 2 == 0)
		{
			// 仅当引号成对（不在字符串内）时，才分割参数
			paramList.push_back(currentParam);
			currentParam.clear();
		}
		else
		{
			currentParam += c;
		}
	}
	paramList.push_back(currentParam); // 加入最后一个参数

	if (paramList.size() < 6) // 风格指令需6个参数：字号,R,G,B,"字体","文本"
	{
		line.errorMsg = "style参数不足（需6个：字号,R,G,B,\"字体\",\"文本\"）";
		return FALSE;
	}

	try
	{
		// 清理参数前后空格
		auto trim = [](string& s)
		{
			s.erase(0, s.find_first_not_of(" \t\n\r"));
			s.erase(s.find_last_not_of(" \t\n\r") + 1);
		};

		trim(paramList[0]); // 字号
		trim(paramList[1]); // R
		trim(paramList[2]); // G
		trim(paramList[3]); // B
		trim(paramList[4]); // 字体（含引号）
		trim(paramList[5]); // 文本（含引号）

		// 解析字号
		line.fontSize = stoi(paramList[0]);
		// 解析颜色（范围校验）
		int r = clamp(stoi(paramList[1]), 0, 255);
		int g = clamp(stoi(paramList[2]), 0, 255);
		int b = clamp(stoi(paramList[3]), 0, 255);
		line.fontColor = RGB(r, g, b);
		// 解析字体（去除引号）
		if (paramList[4].size() >= 2 && paramList[4][0] == '"' && paramList[4].back() == '"')
		{
			line.fontName = paramList[4].substr(1, paramList[4].size() - 2);
		}
		else
		{
			line.fontName = paramList[4];
		}
		// 解析文本（去除引号）
		if (paramList[5].size() >= 2 && paramList[5][0] == '"' && paramList[5].back() == '"')
		{
			line.content = paramList[5].substr(1, paramList[5].size() - 2);
		}
		else
		{
			line.content = paramList[5];
		}

		line.valid = true;
		return true;
	}
	catch (...)
	{
		line.errorMsg = "style参数格式错误（字号/R/G/B需为数字）";
		return false;
	}
}

// 解析font指令 - 支持格式：[font]{16,0,0,0,10,5,5,"Consolas"}
BOOL ParseFontCommand(const string& params)
{
	try
	{
		// 提取大括号内的内容
		string innerParams = params;
		size_t start = params.find('{');
		size_t end = params.rfind('}');
		if (start != string::npos && end != string::npos && start < end) {
			innerParams = params.substr(start + 1, end - start - 1);
		}

		stringstream ss(innerParams);
		int fontSize;
		int r, g, b;
		char comma;
		string fontFamily;
		int topMargin = 10; // 默认字体上间距
		int leftMargin = 5; // 默认字体左间距
		int rightMargin = 5; // 默认字体右间距

		// 解析：字号, R, G, B, 上间距, 左间距, 右间距, "字体名称"
		ss >> fontSize >> comma >> r >> comma >> g >> comma >> b >> comma >> topMargin >> comma >> leftMargin >> comma >> rightMargin >> comma;
		
		// 读取引号内的字体名称
		string tempFontFamily;
		getline(ss, tempFontFamily);
		// 移除首尾的空格和引号
		fontFamily = tempFontFamily;
		fontFamily.erase(0, fontFamily.find_first_not_of(" \t\""));
		fontFamily.erase(fontFamily.find_last_not_of(" \t\"") + 1);

		// 更新全局字体配置 + 自动计算行间距（字号的50%，可调整比例）
		g_defaultFontSize = fontSize;
		g_lineSpacing = fontSize * 0.5; // 核心：行间距关联字号
		g_defaultFontColor = RGB(r, g, b);
		if (!fontFamily.empty()) {
			g_defaultFontName = fontFamily;
		}
		// 更新全局字体间距参数
		g_fontTopMargin = topMargin;
		g_fontLeftMargin = leftMargin;
		g_fontRightMargin = rightMargin;

		// 重新创建默认字体（应用新字号和字体名称）
		if (g_hDefaultFont) DeleteObject(g_hDefaultFont);
		g_hDefaultFont = CreateFontA(
			                     -MulDiv(g_defaultFontSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72), // 正确计算字体高度
			                     0, 0, 0, FW_NORMAL,
			                     FALSE, FALSE, FALSE,
			                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			                     DEFAULT_PITCH | FF_DONTCARE,
			                     g_defaultFontName.c_str()
			                 );

		return TRUE;
	}
	catch (...)
	{
		g_backColor = RGB(30, 30, 30); // 解析失败时默认深色背景
		return FALSE;
	}
}


// 解析backcolor指令
BOOL ParseBackColorCommand(const string& params)
{
	try
	{
		stringstream ss(params);
		int r, g, b;
		char comma;
		ss >> r >> comma >> g >> comma >> b;
		g_backColor = RGB(r, g, b);
		return TRUE;
	}
	catch (...)
	{
		// 修复：如果参数解析失败，设置一个默认的深色背景
		g_backColor = RGB(30, 30, 30);
		return FALSE;
	}
}

// 刷新显示
void RefreshDisplay()
{
	if (!g_hMainWnd)
	{
		return;
	}

	InvalidateRect(g_hMainWnd, NULL, TRUE);
	UpdateWindow(g_hMainWnd);
}

// 处理特殊值
string ProcessSpecialValues(const string& line)
{
	string result = line;
	
	// 0. 处理[refresh]命令 - 设置刷新间隔参数
	{
		string cmdType = "[refresh]";
		size_t startPos = 0;
		while ((startPos = result.find(cmdType, startPos)) != string::npos)
		{
			size_t paramStart = result.find("{", startPos + cmdType.length());
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(startPos, cmdType.length(), "error: [refresh]缺少参数{}");
				startPos = startPos + 10;
				continue;
			}
			
			string params = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			vector<string> paramList;
			string currentParam;
			for (char c : params)
			{
				if (c == ',')
				{
					paramList.push_back(currentParam);
					currentParam.clear();
				}
				else
				{
					currentParam += c;
				}
			}
			paramList.push_back(currentParam);
			
			// 默认值
			int newRefreshTimeInterval = g_refreshTimeInterval;
			int newReloadConfigInterval = g_reloadConfigInterval;
			int newShutdownWindowRefreshTop = g_shutdownWindowRefreshTop;
			
			// 解析参数
			if (paramList.size() >= 1 && !paramList[0].empty())
			{
				try { newRefreshTimeInterval = stoi(paramList[0]); }
				catch (...) {}
			}
			if (paramList.size() >= 2 && !paramList[1].empty())
			{
				try { newReloadConfigInterval = stoi(paramList[1]); }
				catch (...) {}
			}
			if (paramList.size() >= 3 && !paramList[2].empty())
			{
				try { newShutdownWindowRefreshTop = stoi(paramList[2]); }
				catch (...) {}
			}
			
			// 更新全局变量
			g_refreshTimeInterval = newRefreshTimeInterval;
			g_reloadConfigInterval = newReloadConfigInterval;
			g_shutdownWindowRefreshTop = newShutdownWindowRefreshTop;
			
			// 重启定时器
			if (g_hMainWnd)
			{
				KillTimer(g_hMainWnd, WM_TIMER_REFRESH_TIME);
				SetTimer(g_hMainWnd, WM_TIMER_REFRESH_TIME, g_refreshTimeInterval, NULL);
				
				KillTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG);
				SetTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG, g_reloadConfigInterval, NULL);
			}
			
			// 移除[refresh]命令
			result.replace(startPos, paramEnd - startPos + 1, "");
			startPos = startPos;
		}
	}
	
	// 1. 处理[disdatetime]命令 - 某一天的时间距离（调整到最前，满足"在[disdate][distime]之前判断"）
	{
		string cmdType = "[disdatetime,";
		size_t startPos = 0;
		while ((startPos = result.find(cmdType, startPos)) != string::npos)
		{
			size_t endPos = result.find("]", startPos);
			if (endPos == string::npos)
			{
				result.replace(startPos, cmdType.length(), "error: [disdatetime]格式错误");
				break;
			}
			string command = result.substr(startPos + 1, endPos - startPos - 1);
			size_t paramStart = result.find("{", endPos);
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(startPos, endPos - startPos + 1, "error: [disdatetime]缺少参数{}");
				startPos = endPos + 1;
				continue;
			}
			string params = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			string processedResult = ProcessDateTimeDistanceCommand(command, params);
			result.replace(startPos, paramEnd - startPos + 1, processedResult);
			startPos += processedResult.length();
		}
	}
	
	// 2. 处理[disdate]命令 - 日期距离（原顺序1，现在调整为2）
	{
		string cmdType = "[disdate,";
		size_t startPos = 0;
		while ((startPos = result.find(cmdType, startPos)) != string::npos)
		{
			size_t endPos = result.find("]", startPos);
			if (endPos == string::npos)
			{
				result.replace(startPos, cmdType.length(), "error: [disdate]格式错误");
				break;
			}
			string command = result.substr(startPos + 1, endPos - startPos - 1);
			size_t paramStart = result.find("{", endPos);
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(startPos, endPos - startPos + 1, "error: [disdate]缺少参数{}");
				startPos = endPos + 1;
				continue;
			}
			string params = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			string processedResult = ProcessDateDistanceCommand(command, params);
			result.replace(startPos, paramEnd - startPos + 1, processedResult);
			startPos += processedResult.length();
		}
	}
	
	// 3. 处理[distime]命令 - 当天内时间距离（原顺序2，现在调整为3）
	{
		string cmdType = "[distime,";
		size_t startPos = 0;
		while ((startPos = result.find(cmdType, startPos)) != string::npos)
		{
			size_t endPos = result.find("]", startPos);
			if (endPos == string::npos)
			{
				result.replace(startPos, cmdType.length(), "error: [distime]格式错误");
				break;
			}
			string command = result.substr(startPos + 1, endPos - startPos - 1);
			size_t paramStart = result.find("{", endPos);
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(startPos, endPos - startPos + 1, "error: [distime]缺少参数{}");
				startPos = endPos + 1;
				continue;
			}
			string params = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			string processedResult = ProcessTimeDistanceCommand(command, params);
			result.replace(startPos, paramEnd - startPos + 1, processedResult);
			startPos += processedResult.length();
		}
	}
	
	// 4. 处理时间变量（[yyyy]、[mm]、[0m]等）
	result = ReplaceTimeVariables(result);
	
	// 5. 处理[line]指令（嵌套特殊值）
	size_t lineStart = 0;
	while ((lineStart = result.find("[line]{", lineStart)) != string::npos)
	{
		int braceCount = 1;
		size_t lineEnd = lineStart + 7;
		size_t len = result.size();
		while (lineEnd < len && braceCount > 0)
		{
			if (result[lineEnd] == '{') braceCount++;
			else if (result[lineEnd] == '}') braceCount--;
			lineEnd++;
		}
		if (braceCount != 0)// and braceCount != 1)
		{
			result.replace(lineStart, len - lineStart, "error: [line]大括号不匹配, barceCount="+to_string(braceCount));
			break;
		}
		string lineContent = result.substr(lineStart + 7, lineEnd - lineStart - 8);
		// 不替换换行符，让多行内容直接连接
		// 转义处理
		size_t escapePos = 0;
		while ((escapePos = lineContent.find("\\{", escapePos)) != string::npos)
		{
			lineContent.replace(escapePos, 2, "{");
			escapePos += 1;
		}
		escapePos = 0;
		while ((escapePos = lineContent.find("\\}", escapePos)) != string::npos)
		{
			lineContent.replace(escapePos, 2, "}");
			escapePos += 1;
		}
		// 递归处理嵌套特殊值
		string processedLine = ProcessSpecialValues(lineContent);
		result.replace(lineStart, lineEnd - lineStart, processedLine);
		lineStart += processedLine.size();
	}
	
	// 6. 处理[to]指令（支持一行多个[to]）
	size_t toStart = 0;
	while ((toStart = result.find("[to,", toStart)) != string::npos)
	{
		size_t toEnd = result.find("]", toStart);
		if (toEnd == string::npos)
		{
			result.replace(toStart, 5, "error: [to]中括号不匹配");
			break;
		}
		string toCommand = result.substr(toStart + 1, toEnd - toStart - 1);
		size_t paramStart = result.find("{", toEnd);
		size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
		
		if (paramStart == string::npos || paramEnd == string::npos)
		{
			// [to]缺少参数{}
			result.replace(toStart, toEnd - toStart + 1, "error: [to]缺少参数{}");
			toStart = toEnd + 1;
			continue;
		}
		// 提取参数（支持参数内换行）
		string toParams = result.substr(paramStart + 1, paramEnd - paramStart - 1);
		string processedTo = ProcessToCommand(toCommand, toParams);
		// 替换整个[to,...]{...}为处理结果
		result.replace(toStart, paramEnd - toStart + 1, processedTo);
		toStart += processedTo.length();
	}
	
	// 7. 处理[shutdowntext]和[rest]（关机相关）
	size_t shutdownTextPos = result.find("[shutdowntext]");
	if (shutdownTextPos != string::npos)
	{
		if (!g_shutdownPlans.empty())
		{
			// 找到最近的活动关机计划
			ShutdownPlan* nearestPlan = nullptr;
			FILETIME nearestFt = {0};
			SYSTEMTIME now;
			GetLocalTime(&now);
			ULARGE_INTEGER ulNow;
			FILETIME ftNow;
			SystemTimeToFileTime(&now, &ftNow);
			ulNow.LowPart = ftNow.dwLowDateTime;
			ulNow.HighPart = ftNow.dwHighDateTime;
			
			for (auto& plan : g_shutdownPlans)
			{
				if (plan.isActive && !plan.isCancelledToday)
				{
					FILETIME currentFt = GetPlanEffectiveTime(plan, now);
					if (!nearestPlan)
					{
						nearestPlan = &plan;
						nearestFt = currentFt;
					}
					else
					{
						// 比较哪个计划离当前时间更近
						ULARGE_INTEGER ulCurrent, ulNearest;
						ulCurrent.LowPart = currentFt.dwLowDateTime;
						ulCurrent.HighPart = currentFt.dwHighDateTime;
						ulNearest.LowPart = nearestFt.dwLowDateTime;
						ulNearest.HighPart = nearestFt.dwHighDateTime;
						
						__int64 diffCurrent = llabs((__int64)ulCurrent.QuadPart - (__int64)ulNow.QuadPart);
						__int64 diffNearest = llabs((__int64)ulNearest.QuadPart - (__int64)ulNow.QuadPart);
						
						if (diffCurrent < diffNearest)
						{
							nearestPlan = &plan;
							nearestFt = currentFt;
						}
					}
				}
			}
			
			if (nearestPlan)
			{
				SYSTEMTIME effectiveTime;
				FileTimeToSystemTime(&nearestFt, &effectiveTime);
				// 计算剩余秒数（有效时间 - 当前时间）
				ULARGE_INTEGER ulNearest;
				ulNearest.LowPart = nearestFt.dwLowDateTime;
				ulNearest.HighPart = nearestFt.dwHighDateTime;
				__int64 diffSec = (ulNearest.QuadPart - ulNow.QuadPart) / 10000000LL; // 转秒
				
				int hours = diffSec / 3600;
				int minutes = (diffSec % 3600) / 60;
				int seconds = diffSec % 60;
				stringstream ss;
				ss << "将在" << effectiveTime.wHour << ":" << toTwoDigits(effectiveTime.wMinute) << ":" << toTwoDigits(effectiveTime.wSecond)
				<< "关机，还剩" << (hours > 0 ? to_string(hours) + "时" : "")
				<< (minutes > 0 ? to_string(minutes) + "分" : "")
				<< (seconds > 0 || (hours == 0 && minutes == 0) ? to_string(seconds) + "秒" : "") << "";
				result.replace(shutdownTextPos, 14, ss.str());
			}
		}
		else
		{
			result.replace(shutdownTextPos, 14, "(无关机计划)");
		}
	}
	
	size_t restPos = result.find("[rest]");
	if (restPos != string::npos)
	{
		bool hasActiveCountdown = !g_shutdownPlans.empty() && g_shutdownCountdown > 0;
		result.replace(restPos, 6, hasActiveCountdown ? to_string(g_shutdownCountdown) : "0");
	}
	
	// 8. 处理[app]和[appline]命令
	{ 
		string cmdType = "[app]";
		string lineType = "[appline]";
		size_t appStart = result.find(cmdType, 0);
		size_t appLineStart = result.find(lineType, 0);
		
		while (appStart != string::npos || appLineStart != string::npos)
		{
			bool isAppLine = false;
			size_t startPos, endPos;
			string type;
			
			if (appStart != string::npos && (appLineStart == string::npos || appStart < appLineStart))
			{
				startPos = appStart;
				type = cmdType;
				appStart = result.find(cmdType, startPos + type.length());
			}
			else
			{
				startPos = appLineStart;
				type = lineType;
				isAppLine = true;
				appLineStart = result.find(lineType, startPos + type.length());
			}
			
			size_t paramStart = result.find("{", startPos + type.length());
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(startPos, type.length(), "error: " + type + "缺少参数{}");
				continue;
			}
			
			// 提取参数
			string appParams = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			vector<string> paramList;
			string currentParam;
			bool inQuote = false;
			
			for (char c : appParams)
			{
				if (c == '"')
				{
					inQuote = !inQuote;
					currentParam += c;
				}
				else if (c == ',' && !inQuote)
				{
					paramList.push_back(currentParam);
					currentParam.clear();
				}
				else
				{
					currentParam += c;
				}
			}
			paramList.push_back(currentParam);
			
			// 清理参数引号
			for (auto& p : paramList)
			{
				p.erase(0, p.find_first_not_of(" \t\n\r"));
				p.erase(p.find_last_not_of(" \t\n\r") + 1);
				if (p.size() >= 2 && p[0] == '"' && p.back() == '"')
				{
					p = p.substr(1, p.size() - 2);
				}
			}
			
			// 获取应用程序路径
			string appPath = paramList.empty() ? "" : paramList[0];
			string replaceStr = " "; // 默认替换字符串
			DWORD refreshInterval = 0; // 默认每次都刷新
			
			// 处理[appline]的替换字符串参数
			if (isAppLine && paramList.size() >= 2)
			{
				replaceStr = paramList[1];
			}
			
			// 处理刷新间隔参数
			// 对于[app]：第二个参数是刷新间隔
			// 对于[appline]：第三个参数是刷新间隔
			int intervalParamIndex = isAppLine ? 2 : 1;
			if (paramList.size() > intervalParamIndex)
			{
				try {
					refreshInterval = stoi(paramList[intervalParamIndex]);
				} catch (...) {
					refreshInterval = 0; // 解析失败则每次都刷新
				}
			}
			
			if (appPath.empty())
			{
				result.replace(startPos, paramEnd - startPos + 1, "error: " + type + "应用程序路径为空");
				continue;
			}
			
			// 生成唯一的缓存键
			string cacheKey = type + "{" + appParams + "}";
			
			// 获取当前时间（毫秒）
			DWORD currentTime = GetTickCount();
			string output;
			bool needUpdate = true;
			
			// 检查缓存
			auto it = g_appOutputCache.find(cacheKey);
			if (it != g_appOutputCache.end())
			{
				// 如果设置了刷新间隔且未到时间，使用缓存
				if ((refreshInterval > 0 && currentTime - it->second.lastUpdateTime < refreshInterval) || (refreshInterval == -1))
				{
					output = it->second.lastOutput;
					needUpdate = false;
				}
			}
			
			// 刷新间隔为-1时，只使用缓存（即只在启动时获取一次应用返回值）
			if (refreshInterval == -1 && it != g_appOutputCache.end())
			{
				needUpdate = false;
			}
			
			// 如果需要更新或没有缓存，执行程序获取新输出
			if (needUpdate)
			{
				output = StartAppAndGetOutput(appPath);
				
				// 如果是appline，替换换行符
				if (isAppLine)
				{
					size_t pos = 0;
					while ((pos = output.find('\n', pos)) != string::npos)
					{
						output.replace(pos, 1, replaceStr);
						pos += replaceStr.length();
					}
					// 替换回车符
					pos = 0;
					while ((pos = output.find('\r', pos)) != string::npos)
					{
						output.replace(pos, 1, replaceStr);
						pos += replaceStr.length();
					}
				}
				
				// 更新缓存
				g_appOutputCache[cacheKey] = {output, currentTime, refreshInterval};
			}
			
			// 替换整个命令为输出结果
			result.replace(startPos, paramEnd - startPos + 1, output);
		}
	}
	
	// 9. 处理[day/week/month/year]循环命令
	vector<string> cycleTypes = {"[day,", "[week,", "[month,", "[year,"};
	for (const auto& cycleType : cycleTypes)
	{
		size_t cycleStart = 0;
		while ((cycleStart = result.find(cycleType, cycleStart)) != string::npos)
		{
			size_t cycleEnd = result.find("]", cycleStart);
			if (cycleEnd == string::npos)
			{
				result.replace(cycleStart, cycleType.length(), "error: " + cycleType.substr(1) + "格式错误");
				break;
			}
			
			string cycleCommand = result.substr(cycleStart + 1, cycleEnd - cycleStart - 1);
			size_t paramStart = result.find("{", cycleEnd);
			size_t paramEnd = (paramStart != string::npos) ? result.find("}", paramStart) : string::npos;
			
			if (paramStart == string::npos || paramEnd == string::npos)
			{
				result.replace(cycleStart, cycleEnd - cycleStart + 1, "error: " + cycleType.substr(1) + "缺少参数{}");
				cycleStart = cycleEnd + 1;
				continue;
			}
			
			string cycleParams = result.substr(paramStart + 1, paramEnd - paramStart - 1);
			string processedCycle = ProcessCycleCommand(cycleCommand, cycleParams);
			result.replace(cycleStart, paramEnd - cycleStart + 1, processedCycle);
			cycleStart += processedCycle.length();
		}
	}
	
	// 10. 未知特殊值（如[123]）：保留原格式（不处理）
	return result;
}

// 处理to指令
string ProcessToCommand(const string& command, const string& params)
{
	try
	{
		// 解析command：to,time1,time2
		stringstream ssCommand(command);
		string part;
		vector<string> commandParts;
		while (getline(ssCommand, part, ','))
		{
			part.erase(0, part.find_first_not_of(" \t\n\r"));
			part.erase(part.find_last_not_of(" \t\n\r") + 1);
			commandParts.push_back(part);
		}
		if (commandParts.size() < 3)
		{
			return "error: [to]格式错误（需to,time1,time2）";
		}
		string time1 = commandParts[1];
		string time2 = commandParts[2];

		// 解析参数：支持空参数（如{"测试",}）
		vector<string> paramList;
		string currentParam;
		int quoteCount = 0;
		bool inQuote = false;

		for (char c : params)
		{
			if (c == '"')
			{
				inQuote = !inQuote;
				currentParam += c;
			}
			else if (c == ',' && !inQuote)
			{
				// 分割参数（保留空参数）
				paramList.push_back(currentParam);
				currentParam.clear();
			}
			else
			{
				currentParam += c;
			}
		}
		// 加入最后一个参数（即使为空）
		paramList.push_back(currentParam);

		// 清理参数引号和空白
		for (auto& p : paramList)
		{
			// 去除前后空白
			p.erase(0, p.find_first_not_of(" \t\n\r"));
			p.erase(p.find_last_not_of(" \t\n\r") + 1);
			// 去除前后引号（仅当成对时）
			if (p.size() >= 2 && p[0] == '"' && p.back() == '"')
			{
				p = p.substr(1, p.size() - 2);
			}
		}

		// 检查引号是否闭合
		if (inQuote)
		{
			return "error: [to]引号不匹配";
		}

		// 确保至少2个参数（允许空参数）
		while (paramList.size() < 2)
		{
			paramList.push_back(""); // 补空参数
		}

		// 时间有效性校验
		auto isTimeValid = [](const string& timeStr)
		{
			if (timeStr.size() != 6) return false;
			try
			{
				int h = stoi(timeStr.substr(0, 2));
				int m = stoi(timeStr.substr(2, 2));
				int s = stoi(timeStr.substr(4, 2));
				return h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60;
			}
			catch (...)
			{
				return false;
			}
		};

		if (!isTimeValid(time1) || !isTimeValid(time2))
		{
			return "error: [to]时间无效（需HHMMSS，如194100）";
		}

		// 计算当前时间（HHMMSS）
		SYSTEMTIME now;
		GetLocalTime(&now);
		string currentTime = toTwoDigits(now.wHour) + toTwoDigits(now.wMinute) + toTwoDigits(now.wSecond);

		// 判断时间范围
		bool inRange = false;
		if (time1 <= time2)
		{
			inRange = (currentTime >= time1 && currentTime <= time2);
		}
		else
		{
			inRange = (currentTime >= time1 || currentTime <= time2);
		}

		// 递归处理参数中的特殊值
		string result = inRange ? paramList[0] : paramList[1];
		return ProcessSpecialValues(result);
	}
	catch (...)
	{
		return "error: [to]解析失败";
	}
}

// 处理cycle指令
string ProcessCycleCommand(const string& command, const string& params)
{
	try
	{
		// 1. 解析command（格式：type,n,date）
		stringstream ssCommand(command);
		string part;
		vector<string> commandParts;
		while (getline(ssCommand, part, ','))
		{
			part.erase(0, part.find_first_not_of(" \t\n\r"));
			part.erase(part.find_last_not_of(" \t\n\r") + 1);
			commandParts.push_back(part);
		}
		if (commandParts.size() != 3)
			return "error: [cycle]指令参数数量错误（需3个：type,n,date）";

		string type = commandParts[0];
		int n = stoi(commandParts[1]);
		string startDateStr = commandParts[2];
		if (n <= 0) return "error: [cycle]的n必须为正数";
		if (startDateStr.size() != 8) return "error: [cycle]的date格式错误（需8位：YYYYMMDD）";

		// 2. 解析开始日期和当前日期
		SYSTEMTIME startDate = {0}, now = {0};
		GetLocalTime(&now);
		int year = stoi(startDateStr.substr(0, 4));
		int month = stoi(startDateStr.substr(4, 2));
		int day = stoi(startDateStr.substr(6, 2));
		startDate.wYear = year;
		startDate.wMonth = month;
		startDate.wDay = day;

		// 检查是否是"每"的情况（部分为0000）
		bool isEveryYear = (year == 0);
		bool isEveryMonth = (month == 0);
		bool isEveryDay = (day == 0);

		// 检查开始日期有效性
		FILETIME ftTemp;
		if (!SystemTimeToFileTime(&startDate, &ftTemp))
			return "error: [cycle]的startDate无效（不存在该日期）";

		// 3. 计算日期差异（仅当当前日期 >= 开始日期时处理）
		FILETIME ftStart, ftNow;
		SystemTimeToFileTime(&startDate, &ftStart);
		SystemTimeToFileTime(&now, &ftNow);
		ULARGE_INTEGER ulStart, ulNow;
		ulStart.LowPart = ftStart.dwLowDateTime;
		ulStart.HighPart = ftStart.dwHighDateTime;
		ulNow.LowPart = ftNow.dwLowDateTime;
		ulNow.HighPart = ftNow.dwHighDateTime;
		if (ulNow.QuadPart < ulStart.QuadPart)
			return ""; // 当前日期在开始日期前：不显示

		// 4. 解析参数列表（移动到循环索引计算之前）
		vector<string> paramList;  // 在此处声明paramList
		string currentParam;
		int quoteCount = 0;
		for (char c : params)
		{
			if (c == '"') quoteCount++;
			if (c == ',' && quoteCount % 2 == 0)
			{
				size_t start = currentParam.find_first_not_of(" \t\n\r\"");
				size_t end = currentParam.find_last_not_of(" \t\n\r\"");
				currentParam = (start != string::npos && end != string::npos && start <= end)
				               ? currentParam.substr(start, end - start + 1) : "";
				paramList.push_back(currentParam);
				currentParam = "";
			}
			else
			{
				currentParam += c;
			}
		}
		// 处理最后一个参数
		if (!currentParam.empty())
		{
			size_t start = currentParam.find_first_not_of(" \t\n\r\"");
			size_t end = currentParam.find_last_not_of(" \t\n\r\"");
			currentParam = (start != string::npos && end != string::npos && start <= end)
			               ? currentParam.substr(start, end - start + 1) : "";
			paramList.push_back(currentParam);
		}

		// 引号不匹配检测（现在paramList已声明）
		if (quoteCount % 2 != 0)
		{
			paramList.clear();
			return "error: [cycle]指令引号不匹配";
		}

		if (paramList.empty())
			return "error: [cycle]的参数列表为空";

		// 5. 按类型计算循环索引（现在可以安全使用paramList了）
		int cycleIndex = -1;
		if (type == "day")
		{
			__int64 diffDays = (ulNow.QuadPart - ulStart.QuadPart) / (10000000LL * 60 * 60 * 24);
			cycleIndex = (diffDays / n) % paramList.size();
		}
		else if (type == "week")
		{
			__int64 diffDays = (ulNow.QuadPart - ulStart.QuadPart) / (10000000LL * 60 * 60 * 24);
			int weekDiff = diffDays / 7;
			// 计算周数差，并根据n计算循环索引
			cycleIndex = (weekDiff / n) % paramList.size();
		}
		else if (type == "month")
		{
			// 处理常规情况和"每"的情况
			int monthDiff = (now.wYear - startDate.wYear) * 12 + (now.wMonth - startDate.wMonth);
			
			// 检查是否是相同的日期或者是"每"的情况
			bool isSameDay = (now.wDay == startDate.wDay);
			bool shouldShow = isSameDay || isEveryDay;
			
			if (shouldShow)
			{
				// 检查是否是n月的倍数
				if (monthDiff % n == 0)
				{
					cycleIndex = (monthDiff / n) % paramList.size();
				}
			}
		}
		else if (type == "year")
		{
			// 处理常规情况和"每"的情况
			int yearDiff = now.wYear - startDate.wYear;
			
			// 检查是否是相同的月日或者是"每"的情况
			bool isSameMonthDay = (now.wMonth == startDate.wMonth && now.wDay == startDate.wDay);
			bool shouldShow = isSameMonthDay || isEveryMonth || isEveryDay;
			
			if (shouldShow)
			{
				// 检查是否是n年的倍数
				if (yearDiff % n == 0)
				{
					cycleIndex = (yearDiff / n) % paramList.size();
				}
			}
		}
		else
		{
			return "error: [cycle]的type错误（仅支持day/week/month/year）";
		}

		// 6. 返回对应内容
		if (cycleIndex < 0 || cycleIndex >= paramList.size())
			return "";
		return paramList[cycleIndex];
	}
	catch (...)
	{
		return "error: [cycle]指令解析失败";
	}
}

// 处理[disdate]命令 - 日期距离
string ProcessDateDistanceCommand(const string& command, const string& params)
{
	try
	{
		// 解析command参数：disdate,targetDate,showNegative,showAbsolute
		stringstream ssCommand(command);
		string part;
		vector<string> commandParts;
		while (getline(ssCommand, part, ','))
		{
			part.erase(0, part.find_first_not_of(" \t\n\r"));
			part.erase(part.find_last_not_of(" \t\n\r") + 1);
			commandParts.push_back(part);
		}
		
		if (commandParts.size() < 1 || commandParts[0] != "disdate")
		{
			return "error: [disdate]格式错误";
		}
		
		// 获取目标日期、是否显示负数、是否显示绝对值参数
		string targetDateStr = (commandParts.size() >= 2) ? commandParts[1] : "";
		bool showNegative = (commandParts.size() >= 3) ? (commandParts[2] == "1") : false;
		bool showAbsolute = (commandParts.size() >= 4) ? (commandParts[3] == "1") : false;
		
		// 检查目标日期格式
		if (targetDateStr.size() != 8)
		{
			return "error: [disdate]日期格式错误（需8位：YYYYMMDD）";
		}
		
		// 解析目标日期
		SYSTEMTIME targetDate = {0}, now = {0};
		GetLocalTime(&now);
		
		int year = stoi(targetDateStr.substr(0, 4));
		int month = stoi(targetDateStr.substr(4, 2));
		int day = stoi(targetDateStr.substr(6, 2));
		
		// 如果前4位或5-6位或6-8位为0，意为"每"
		bool isEveryYear = (year == 0);
		bool isEveryMonth = (month == 0);
		bool isEveryDay = (day == 0);
		
		// 计算日期差异
		SYSTEMTIME effectiveTarget = now;
		if (!isEveryYear) effectiveTarget.wYear = year;
		if (!isEveryMonth) effectiveTarget.wMonth = month;
		if (!isEveryDay) effectiveTarget.wDay = day;
		
		// 转换为FILETIME进行计算
		FILETIME ftTarget, ftNow;
		if (!SystemTimeToFileTime(&effectiveTarget, &ftTarget) || !SystemTimeToFileTime(&now, &ftNow))
		{
			return "error: [disdate]日期无效";
		}
		
		ULARGE_INTEGER ulTarget, ulNow;
		ulTarget.LowPart = ftTarget.dwLowDateTime;
		ulTarget.HighPart = ftTarget.dwHighDateTime;
		ulNow.LowPart = ftNow.dwLowDateTime;
		ulNow.HighPart = ftNow.dwHighDateTime;
		
		// 判断目标日期是否早于当前日期
		bool isPast = (ulTarget.QuadPart < ulNow.QuadPart);

		// 如果日期已过且不显示负数和绝对值，则不显示
		if (isPast && !showNegative && !showAbsolute)
		{
			return "";
		}

		// 处理"每"的情况：计算下一个满足条件的日期
		if (isEveryYear || isEveryMonth || isEveryDay)
		{
			effectiveTarget = now;
			bool found = false;
			int maxTries = 365 * 5; // 最多查找5年内的日期
			for (int i = 0; i < maxTries; i++)
			{
				// 增加一天
				FILETIME ftTemp;
				if (!SystemTimeToFileTime(&effectiveTarget, &ftTemp)) break;
				ULARGE_INTEGER ulTemp;
				ulTemp.LowPart = ftTemp.dwLowDateTime;
				ulTemp.HighPart = ftTemp.dwHighDateTime;
				ulTemp.QuadPart += 10000000LL * 60 * 60 * 24; // 加一天
				ftTemp.dwLowDateTime = ulTemp.LowPart;
				ftTemp.dwHighDateTime = ulTemp.HighPart;
				if (!FileTimeToSystemTime(&ftTemp, &effectiveTarget)) break;

				// 检查是否满足条件
				bool yearMatch = isEveryYear || (effectiveTarget.wYear == year);
				bool monthMatch = isEveryMonth || (effectiveTarget.wMonth == month);
				bool dayMatch = isEveryDay || (effectiveTarget.wDay == day);

				if (yearMatch && monthMatch && dayMatch)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return "error: [disdate]无法找到符合条件的日期";
			}

			// 重新转换为FILETIME进行计算
			if (!SystemTimeToFileTime(&effectiveTarget, &ftTarget))
			{
				return "error: [disdate]日期无效";
			}
			ulTarget.LowPart = ftTarget.dwLowDateTime;
			ulTarget.HighPart = ftTarget.dwHighDateTime;

			// 重新判断目标日期是否早于当前日期
			isPast = (ulTarget.QuadPart < ulNow.QuadPart);

			// 如果日期已过且不显示负数和绝对值，则不显示
			if (isPast && !showNegative && !showAbsolute)
			{
				return "";
			}
		}

		// 计算总天数差异（取绝对值）
		__int64 diffDays = (isPast ? (ulNow.QuadPart - ulTarget.QuadPart) : (ulTarget.QuadPart - ulNow.QuadPart)) / (10000000LL * 60 * 60 * 24);

		// 处理符号
		int sign = (isPast && showNegative && !showAbsolute) ? -1 : 1;
		if (showAbsolute) sign = 1;
		
		// 计算年、月、日差异
		int years = 0, months = 0, days = 0;
		if (diffDays != 0)
		{
			// 根据日期先后顺序选择正确的计算方向
			SYSTEMTIME startDate = isPast ? effectiveTarget : now;
			SYSTEMTIME endDate = isPast ? now : effectiveTarget;
			
			// 计算整数年差异
			SYSTEMTIME tempDate = startDate;
			while (true)
			{
				SYSTEMTIME nextYear = tempDate;
				nextYear.wYear++;
				FILETIME ftNextYear;
				if (!SystemTimeToFileTime(&nextYear, &ftNextYear)) break;
				ULARGE_INTEGER ulNextYear;
				ulNextYear.LowPart = ftNextYear.dwLowDateTime;
				ulNextYear.HighPart = ftNextYear.dwHighDateTime;
				
				FILETIME ftEndDate;
				SystemTimeToFileTime(&endDate, &ftEndDate);
				ULARGE_INTEGER ulEndDate;
				ulEndDate.LowPart = ftEndDate.dwLowDateTime;
				ulEndDate.HighPart = ftEndDate.dwHighDateTime;
				
				if (ulNextYear.QuadPart > ulEndDate.QuadPart)
				{
					break;
				}
				tempDate = nextYear;
				years++;
			}
			
			// 计算整数月差异
			while (true)
			{
				SYSTEMTIME nextMonth = tempDate;
				nextMonth.wMonth++;
				if (nextMonth.wMonth > 12)
				{
					nextMonth.wMonth = 1;
					nextMonth.wYear++;
				}
				FILETIME ftNextMonth;
				if (!SystemTimeToFileTime(&nextMonth, &ftNextMonth)) break;
				ULARGE_INTEGER ulNextMonth;
				ulNextMonth.LowPart = ftNextMonth.dwLowDateTime;
				ulNextMonth.HighPart = ftNextMonth.dwHighDateTime;
				
				FILETIME ftEndDate;
				SystemTimeToFileTime(&endDate, &ftEndDate);
				ULARGE_INTEGER ulEndDate;
				ulEndDate.LowPart = ftEndDate.dwLowDateTime;
				ulEndDate.HighPart = ftEndDate.dwHighDateTime;
				
				if (ulNextMonth.QuadPart > ulEndDate.QuadPart)
				{
					break;
				}
				tempDate = nextMonth;
				months++;
			}
			
			// 计算剩余天数差异
			FILETIME ftTemp;
			SystemTimeToFileTime(&tempDate, &ftTemp);
			ULARGE_INTEGER ulTemp;
			ulTemp.LowPart = ftTemp.dwLowDateTime;
			ulTemp.HighPart = ftTemp.dwHighDateTime;
			
			FILETIME ftEndDate;
			SystemTimeToFileTime(&endDate, &ftEndDate);
			ULARGE_INTEGER ulEndDate;
			ulEndDate.LowPart = ftEndDate.dwLowDateTime;
			ulEndDate.HighPart = ftEndDate.dwHighDateTime;
			
			days = (int)((ulEndDate.QuadPart - ulTemp.QuadPart) / (10000000LL * 60 * 60 * 24));
			// 确保天数不为负数
			if (days < 0) days = 0;
		}
		
		// 处理显示格式
		string result = params;
		// 替换[dy] [dmo] [dd] [sdy] [sdmo] [sdd]等特殊值
		result = regex_replace(result, regex("\\[dy\\]"), to_string(years * sign));
		result = regex_replace(result, regex("\\[dmo\\]"), to_string(months * sign));
		result = regex_replace(result, regex("\\[dd\\]"), to_string(days * sign));
		
		// 计算单一年、月、日（去末尾0的3位小数），并正确处理符号
		string sdy = to_string(fabs((double)diffDays / 365.25));
		sdy = sdy.substr(0, sdy.find_last_not_of('0') + 1);
		if (sdy.back() == '.') sdy.pop_back();

		string sdmo = to_string(fabs((double)diffDays / 30.4375));
		sdmo = sdmo.substr(0, sdmo.find_last_not_of('0') + 1);
		if (sdmo.back() == '.') sdmo.pop_back();

		string sdd = to_string(diffDays * sign);

		// 处理负数情况
		if (isPast && showNegative && !showAbsolute)
		{
			sdy = (sdy.empty() || sdy == "0") ? "0" : "-" + sdy;
			sdmo = (sdmo.empty() || sdmo == "0") ? "0" : "-" + sdmo;
		}

		result = regex_replace(result, regex("\\[sdy\\]"), sdy);
		result = regex_replace(result, regex("\\[sdmo\\]"), sdmo);
		result = regex_replace(result, regex("\\[sdd\\]"), sdd);
		
		return ProcessSpecialValues(result);
	}
	catch (...)
	{
		return "error: [disdate]解析失败";
	}
}

// 处理[distime]命令 - 当天内时间距离
string ProcessTimeDistanceCommand(const string& command, const string& params)
{
	try
	{
		// 解析command参数：distime,targetTime,showNegative,showTomorrow
		stringstream ssCommand(command);
		string part;
		vector<string> commandParts;
		while (getline(ssCommand, part, ','))
		{
			part.erase(0, part.find_first_not_of(" \t\n\r"));
			part.erase(part.find_last_not_of(" \t\n\r") + 1);
			commandParts.push_back(part);
		}
		
		if (commandParts.size() < 1 || commandParts[0] != "distime")
		{
			return "error: [distime]格式错误";
		}
		
		// 获取目标时间、是否显示负数、是否显示明天的时间参数
		string targetTimeStr = (commandParts.size() >= 2) ? commandParts[1] : "";
		bool showNegative = (commandParts.size() >= 3) ? (commandParts[2] == "1") : false;
		bool showTomorrow = (commandParts.size() >= 4) ? (commandParts[3] == "1") : false;
		
		// 检查目标时间格式
		if (targetTimeStr.size() != 6)
		{
			return "error: [distime]时间格式错误（需6位：HHMMSS）";
		}
		
		// 解析目标时间
		SYSTEMTIME targetTime = {0}, now = {0};
		GetLocalTime(&now);
		targetTime.wYear = now.wYear;
		targetTime.wMonth = now.wMonth;
		targetTime.wDay = now.wDay;
		
		try
		{
			targetTime.wHour = stoi(targetTimeStr.substr(0, 2));
			targetTime.wMinute = stoi(targetTimeStr.substr(2, 2));
			targetTime.wSecond = stoi(targetTimeStr.substr(4, 2));
		}
		catch (...)
		{
			return "error: [distime]时间无效";
		}
		
		// 转换为FILETIME进行计算
		FILETIME ftTarget, ftNow;
		if (!SystemTimeToFileTime(&targetTime, &ftTarget) || !SystemTimeToFileTime(&now, &ftNow))
		{
			return "error: [distime]时间无效";
		}
		
		ULARGE_INTEGER ulTarget, ulNow;
		ulTarget.LowPart = ftTarget.dwLowDateTime;
		ulTarget.HighPart = ftTarget.dwHighDateTime;
		ulNow.LowPart = ftNow.dwLowDateTime;
		ulNow.HighPart = ftNow.dwHighDateTime;
		
		// 判断目标时间是否早于当前时间
		bool isPast = (ulTarget.QuadPart < ulNow.QuadPart);
		
		// 如果时间已过且不显示负数和不显示明天，则不显示
		if (isPast && !showNegative && !showTomorrow)
		{
			return "";
		}
		
		// 计算总秒数差异（取绝对值）
		__int64 diffSeconds = (isPast ? (ulNow.QuadPart - ulTarget.QuadPart) : (ulTarget.QuadPart - ulNow.QuadPart)) / 10000000LL;
		
		// 如果显示明天的时间，则加一天
		if (isPast && showTomorrow)
		{
			diffSeconds = 86400 - diffSeconds; // 计算到明天的时间差
			isPast = false;
		}
		
		// 处理符号
		int sign = (isPast && showNegative) ? -1 : 1;
		if (!isPast || showTomorrow) sign = 1;
		
		// 计算小时、分钟、秒差异（始终使用正数计算，最后添加符号）
		int hours = diffSeconds / 3600;
		int minutes = (diffSeconds % 3600) / 60;
		int seconds = diffSeconds % 60;
		
		// 处理显示格式
		string result = params;
		// 替换[dh] [dmi] [ds] [sdh] [sdmi] [sds]等特殊值
		result = regex_replace(result, regex("\\[dh\\]"), to_string(hours * sign));
		result = regex_replace(result, regex("\\[dmi\\]"), to_string(minutes * sign));
		result = regex_replace(result, regex("\\[ds\\]"), to_string(seconds * sign));
		
		// 计算单一小时、分钟、秒（去末尾0的3位小数）
		string sdh = to_string((double)diffSeconds / 3600);
		sdh = sdh.substr(0, sdh.find_last_not_of('0') + 1);
		if (sdh.back() == '.') sdh.pop_back();
		
		string sdmi = to_string((double)diffSeconds / 60);
		sdmi = sdmi.substr(0, sdmi.find_last_not_of('0') + 1);
		if (sdmi.back() == '.') sdmi.pop_back();
		
		string sds = to_string(diffSeconds);
		
		// 处理负数情况
		if (isPast && showNegative && !showTomorrow)
		{
			sdh = (sdh.empty() || sdh == "0") ? "0" : "-" + sdh;
			sdmi = (sdmi.empty() || sdmi == "0") ? "0" : "-" + sdmi;
			sds = (sds.empty() || sds == "0") ? "0" : "-" + sds;
		}
		
		result = regex_replace(result, regex("\\[sdh\\]"), sdh);
		result = regex_replace(result, regex("\\[sdmi\\]"), sdmi);
		result = regex_replace(result, regex("\\[sds\\]"), sds);
		
		return ProcessSpecialValues(result);
	}
	catch (...)
	{
		return "error: [distime]解析失败";
	}
}

// 处理[disdatetime]命令 - 某一天的时间距离
string ProcessDateTimeDistanceCommand(const string& command, const string& params)
{
	try
	{
		// 解析command参数：disdatetime,targetDate,targetTime,showNegative,showAbsolute
		stringstream ssCommand(command);
		string part;
		vector<string> commandParts;
		while (getline(ssCommand, part, ','))
		{
			part.erase(0, part.find_first_not_of(" \t\n\r"));
			part.erase(part.find_last_not_of(" \t\n\r") + 1);
			commandParts.push_back(part);
		}
		
		if (commandParts.size() < 1 || commandParts[0] != "disdatetime")
		{
			return "error: [disdatetime]格式错误";
		}
		
		// 获取目标日期、时间、是否显示负数、是否显示绝对值参数
		string targetDateStr = (commandParts.size() >= 2) ? commandParts[1] : "";
		string targetTimeStr = (commandParts.size() >= 3) ? commandParts[2] : "";
		bool showNegative = (commandParts.size() >= 4) ? (commandParts[3] == "1") : false;
		bool showAbsolute = (commandParts.size() >= 5) ? (commandParts[4] == "1") : false;
		
		// 检查目标日期和时间格式
		if (targetDateStr.size() != 8)
		{
			return "error: [disdatetime]日期格式错误（需8位：YYYYMMDD）";
		}
		if (targetTimeStr.size() != 6)
		{
			return "error: [disdatetime]时间格式错误（需6位：HHMMSS）";
		}
		
		// 解析目标日期和时间
		SYSTEMTIME targetDateTime = {0}, now = {0};
		GetLocalTime(&now);

		int year = 0, month = 0, day = 0;
		int hour = 0, minute = 0, second = 0;

		try
		{
			year = stoi(targetDateStr.substr(0, 4));
			month = stoi(targetDateStr.substr(4, 2));
			day = stoi(targetDateStr.substr(6, 2));
			hour = stoi(targetTimeStr.substr(0, 2));
			minute = stoi(targetTimeStr.substr(2, 2));
			second = stoi(targetTimeStr.substr(4, 2));

			targetDateTime.wYear = year;
			targetDateTime.wMonth = month;
			targetDateTime.wDay = day;
			targetDateTime.wHour = hour;
			targetDateTime.wMinute = minute;
			targetDateTime.wSecond = second;
		}
		catch (...)
		{
			return "error: [disdatetime]日期或时间无效";
		}
		
		// 处理"每"的情况
		bool isEveryYear = (year == 0);
		bool isEveryMonth = (month == 0);
		bool isEveryDay = (day == 0);
		bool isEveryHour = (hour == 0);
		bool isEveryMinute = (minute == 0);
		bool isEverySecond = (second == 0);

		// 转换为FILETIME进行计算
		FILETIME ftTarget, ftNow;
		if (!SystemTimeToFileTime(&targetDateTime, &ftTarget) || !SystemTimeToFileTime(&now, &ftNow))
		{
			return "error: [disdatetime]日期或时间无效";
		}

		ULARGE_INTEGER ulTarget, ulNow;
		ulTarget.LowPart = ftTarget.dwLowDateTime;
		ulTarget.HighPart = ftTarget.dwHighDateTime;
		ulNow.LowPart = ftNow.dwLowDateTime;
		ulNow.HighPart = ftNow.dwHighDateTime;

		// 判断目标日期时间是否早于当前日期时间
		bool isPast = (ulTarget.QuadPart < ulNow.QuadPart);

		// 如果日期时间已过且不显示负数和绝对值，则不显示
		if (isPast && !showNegative && !showAbsolute)
		{
			return "";
		}

		// 处理"每"的情况：计算下一个满足条件的日期时间
		if (isEveryYear || isEveryMonth || isEveryDay || isEveryHour || isEveryMinute || isEverySecond)
		{
			targetDateTime = now;
			bool found = false;
			int maxTries = 365 * 24 * 60; // 最多查找1年内的日期时间（分钟级别）
			for (int i = 0; i < maxTries; i++)
			{
				// 增加一分钟
				FILETIME ftTemp;
				if (!SystemTimeToFileTime(&targetDateTime, &ftTemp)) break;
				ULARGE_INTEGER ulTemp;
				ulTemp.LowPart = ftTemp.dwLowDateTime;
				ulTemp.HighPart = ftTemp.dwHighDateTime;
				ulTemp.QuadPart += 10000000LL * 60; // 加一分钟
				ftTemp.dwLowDateTime = ulTemp.LowPart;
				ftTemp.dwHighDateTime = ulTemp.HighPart;
				if (!FileTimeToSystemTime(&ftTemp, &targetDateTime)) break;

				// 检查是否满足条件
				bool yearMatch = isEveryYear || (targetDateTime.wYear == year);
				bool monthMatch = isEveryMonth || (targetDateTime.wMonth == month);
				bool dayMatch = isEveryDay || (targetDateTime.wDay == day);
				bool hourMatch = isEveryHour || (targetDateTime.wHour == hour);
				bool minuteMatch = isEveryMinute || (targetDateTime.wMinute == minute);
				bool secondMatch = isEverySecond || (targetDateTime.wSecond == second);

				if (yearMatch && monthMatch && dayMatch && hourMatch && minuteMatch && secondMatch)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				return "error: [disdatetime]无法找到符合条件的日期时间";
			}

			// 重新转换为FILETIME进行计算
			if (!SystemTimeToFileTime(&targetDateTime, &ftTarget))
			{
				return "error: [disdatetime]日期时间无效";
			}
			ulTarget.LowPart = ftTarget.dwLowDateTime;
			ulTarget.HighPart = ftTarget.dwHighDateTime;

			// 重新判断目标日期时间是否早于当前日期时间
			isPast = (ulTarget.QuadPart < ulNow.QuadPart);

			// 如果日期时间已过且不显示负数和绝对值，则不显示
			if (isPast && !showNegative && !showAbsolute)
			{
				return "";
			}
		}

		// 计算总秒数差异（取绝对值）
		__int64 diffSeconds = (isPast ? (ulNow.QuadPart - ulTarget.QuadPart) : (ulTarget.QuadPart - ulNow.QuadPart)) / 10000000LL;

		// 处理符号
		int sign = (isPast && showNegative && !showAbsolute) ? -1 : 1;
		if (showAbsolute) sign = 1;
		
		// 计算总天数差异和剩余秒数
		__int64 totalDays = diffSeconds / 86400;
		int remainingSeconds = diffSeconds % 86400;
		
		// 计算小时、分钟、秒差异
		int hours = remainingSeconds / 3600;
		int minutes = (remainingSeconds % 3600) / 60;
		int seconds = remainingSeconds % 60;
		
		// 计算年、月、日差异
		int years = 0, months = 0, days = 0;
		if (diffSeconds != 0)
		{
			// 根据日期时间先后顺序选择正确的计算方向
			SYSTEMTIME startTime = isPast ? targetDateTime : now;
			SYSTEMTIME endTime = isPast ? now : targetDateTime;
			
			// 检查是否是整年（同一月同一日）
			bool isSameMonthDay = (startTime.wMonth == endTime.wMonth && startTime.wDay == endTime.wDay);
			
			if (isSameMonthDay)
			{
				// 如果是同一月同一日，则按整年计算，月和日都为0
				years = abs(endTime.wYear - startTime.wYear);
				months = 0;
				days = 0;
			}
			else
			{
				// 非整年情况
				// 检查是否是整月（同一日）
				bool isSameDay = (startTime.wDay == endTime.wDay);
				
				if (isSameDay)
				{
					// 如果是同一日，则按整月计算，日为0
					// 计算月份差异（考虑跨年）
					int monthDiff = (endTime.wYear - startTime.wYear) * 12 + (endTime.wMonth - startTime.wMonth);
					months = abs(monthDiff);
					days = 0;
				}
				else
				{
					// 普通情况，使用简化计算
					// 计算剩余天数
					int remainingDays = (int)(totalDays % 365);
					months = remainingDays / 30;
					days = remainingDays % 30;
					// 确保天数不为负数
					if (days < 0) days = 0;
				}
			}
		}
		
		// 处理显示格式
		string result = params;
		// 替换各种特殊值
		result = regex_replace(result, regex("\\[dy\\]"), to_string(years * sign));
		result = regex_replace(result, regex("\\[dmo\\]"), to_string(months * sign));
		result = regex_replace(result, regex("\\[dd\\]"), to_string(days * sign));
		result = regex_replace(result, regex("\\[dh\\]"), to_string(hours * sign));
		result = regex_replace(result, regex("\\[dmi\\]"), to_string(minutes * sign));
		result = regex_replace(result, regex("\\[ds\\]"), to_string(seconds * sign));
		
		// 计算单一值（去末尾0的3位小数），并正确处理符号
		string sdy = to_string(fabs((double)diffSeconds / (365.25 * 86400)));
		sdy = sdy.substr(0, sdy.find_last_not_of('0') + 1);
		if (sdy.back() == '.') sdy.pop_back();

		string sdmo = to_string(fabs((double)diffSeconds / (30.4375 * 86400)));
		sdmo = sdmo.substr(0, sdmo.find_last_not_of('0') + 1);
		if (sdmo.back() == '.') sdmo.pop_back();

		string sdd = to_string(fabs((double)diffSeconds / 86400));
		sdd = sdd.substr(0, sdd.find_last_not_of('0') + 1);
		if (sdd.back() == '.') sdd.pop_back();

		string sdh = to_string(fabs((double)diffSeconds / 3600));
		sdh = sdh.substr(0, sdh.find_last_not_of('0') + 1);
		if (sdh.back() == '.') sdh.pop_back();

		string sdmi = to_string(fabs((double)diffSeconds / 60));
		sdmi = sdmi.substr(0, sdmi.find_last_not_of('0') + 1);
		if (sdmi.back() == '.') sdmi.pop_back();

		string sds = to_string(diffSeconds * sign);

		// 处理负数情况
		if (isPast && showNegative && !showAbsolute)
		{
			sdy = (sdy.empty() || sdy == "0") ? "0" : "-" + sdy;
			sdmo = (sdmo.empty() || sdmo == "0") ? "0" : "-" + sdmo;
			sdd = (sdd.empty() || sdd == "0") ? "0" : "-" + sdd;
			sdh = (sdh.empty() || sdh == "0") ? "0" : "-" + sdh;
			sdmi = (sdmi.empty() || sdmi == "0") ? "0" : "-" + sdmi;
		}
		
		result = regex_replace(result, regex("\\[sdy\\]"), sdy);
		result = regex_replace(result, regex("\\[sdmo\\]"), sdmo);
		result = regex_replace(result, regex("\\[sdd\\]"), sdd);
		result = regex_replace(result, regex("\\[sdh\\]"), sdh);
		result = regex_replace(result, regex("\\[sdmi\\]"), sdmi);
		result = regex_replace(result, regex("\\[sds\\]"), sds);
		
		return ProcessSpecialValues(result);
	}
	catch (...)
	{
		return "error: [disdatetime]解析失败";
	}
}

// 主窗口消息处理
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rect;
			GetClientRect(hWnd, &rect);

			// 1. 绘制背景（确保画刷释放）
			HBRUSH hBrush = CreateSolidBrush(g_backColor);
			if (hBrush)   // 检查资源创建成功
			{
				FillRect(hdc, &rect, hBrush);
				DeleteObject(hBrush); // 必须释放
			}

			int yPos = g_fontTopMargin;
			for (const auto& line : g_displayLines)
			{
				// 2. 计算字体参数（确保字号有效）
				HDC hScreenDC = GetDC(NULL);
				int fontSize = (line.fontSize > 0) ? line.fontSize : g_defaultFontSize;
				if (fontSize < 1) fontSize = 16; // 防止异常字号
				int fontHeight = -MulDiv(fontSize, GetDeviceCaps(hScreenDC, LOGPIXELSY), 72);
				ReleaseDC(NULL, hScreenDC);

				// 3. 创建字体（确保释放）
				// 使用line.fontName（如果不为空），否则使用默认字体名称
				string fontName = line.fontName.empty() ? g_defaultFontName : line.fontName;
				HFONT hFont = CreateFontA(
				                  fontHeight, 0, 0, 0, FW_NORMAL,
				                  FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
				                  fontName.c_str()
				              );
				if (!hFont)   // 字体创建失败时跳过，避免崩溃
				{
					yPos += abs(fontHeight) + g_lineSpacing;
					continue;
				}

				HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
				// 4. 设置文本颜色（确保与背景有对比度）
				COLORREF textColor = line.fontColor;
				if (textColor == g_backColor || textColor == RGB(0, 0, 0) && g_backColor == RGB(0, 0, 0))
				{
					textColor = RGB(255, 255, 255); // 背景黑色时强制白色文本
				}
				SetTextColor(hdc, textColor);
				SetBkMode(hdc, TRANSPARENT);

				// 5. 绘制文本
				RECT textRect = rect;
				textRect.top = yPos;
				textRect.left = g_fontLeftMargin;
				textRect.right -= g_fontRightMargin;
				string drawText = line.valid ? ProcessSpecialValues(line.content) : ("error: " + line.errorMsg);
				if (line.valid)
				{
					SetTextColor(hdc, textColor);
				}
				else
				{
					SetTextColor(hdc, RGB(255, 0, 0)); // 错误文本红色
				}
				DrawTextA(hdc, drawText.c_str(), -1, &textRect, DT_SINGLELINE | DT_LEFT | DT_TOP);

				// 6. 释放字体资源（关键，不可遗漏）
				SelectObject(hdc, hOldFont);
				DeleteObject(hFont);

				// 7. 计算下一行位置（确保不重叠）
				yPos += abs(fontHeight) + (g_lineSpacing > 0 ? g_lineSpacing : 8); // 行间距默认8
			}

			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_TIMER:
		{
			if (wParam == WM_TIMER_REFRESH_TIME)
			{
				RefreshDisplay();
			}
			else if (wParam == WM_TITLE_CHANGE_TIMER)
			{
				// 5秒后切换到用户定义的窗口标题
				SetWindowTextA(hWnd, g_finalWindowTitle.c_str());
				KillTimer(hWnd, WM_TITLE_CHANGE_TIMER); // 销毁定时器
			}
			else if (wParam == WM_TIMER_RELOAD_CONFIG)
			{
				if (g_isReloading) return 0; // 重入保护
				g_isReloading = TRUE;

				// 关键：先停止自身定时器，避免重建中再次触发
				KillTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG);

				// 检查配置文件是否修改
				WIN32_FILE_ATTRIBUTE_DATA fileInfo;
				if (!GetFileAttributesExA(g_configFilePath.c_str(), GetFileExInfoStandard, &fileInfo))
				{
					g_isReloading = FALSE;
					SetTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG, g_reloadConfigInterval, NULL); // 重启定时器
					return 0;
				}

				// 生成新的修改时间字符串
				FILETIME ft = fileInfo.ftLastWriteTime;
				SYSTEMTIME st;
				FileTimeToSystemTime(&ft, &st);
				stringstream ssNew;
				ssNew << st.wYear << "." << toTwoDigits(st.wMonth) << "." << toTwoDigits(st.wDay) << " "
				      << toTwoDigits(st.wHour) << ":" << toTwoDigits(st.wMinute) << ":" << toTwoDigits(st.wSecond) << "." << st.wMilliseconds;
				string newModified = ssNew.str();

				// 仅当配置修改时才处理
				if (newModified != g_configLastModified)
				{
					g_configLastModified = newModified;
					string newConfig = LoadConfigFile();

					// 检查是否有[start]配置变化
					// 先备份当前的start配置
					string oldStartConfig = g_prevStartConfig;

					// 临时存储新解析的配置，避免影响当前窗口
					vector<DisplayLine> tempDisplayLines = g_displayLines;
					HWND tempMainWnd = g_hMainWnd;

					// 停止所有关联定时器
					KillTimer(g_hMainWnd, WM_TIMER_REFRESH_TIME);
					KillTimer(g_hMainWnd, WM_TIMER_SHUTDOWN_CHECK);
					KillTimer(g_hMainWnd, WM_USER + 7);

					// 解析配置
					ParseConfigFile(newConfig);

					// 无论[start]配置是否变化，直接启动新程序
					char moduleName[MAX_PATH];
					GetModuleFileNameA(NULL, moduleName, MAX_PATH);
					StartASyncAPP(moduleName);

					// 延迟关闭当前程序，确保新程序有足够时间启动
					SetTimer(g_hMainWnd, WM_USER + 8, 2000, NULL);
				}

				// 重启配置加载定时器
				SetTimer(g_hMainWnd, WM_TIMER_RELOAD_CONFIG, g_reloadConfigInterval, NULL);
				g_isReloading = FALSE; // 必须重置锁
			}
			else if (wParam == WM_TIMER_SHUTDOWN_CHECK)
			{
				if (g_shutdownPlans.empty())
				{
					KillTimer(hWnd, WM_TIMER_SHUTDOWN_CHECK);
					return 0;
				}

				// 检查所有活动的关机计划
				SYSTEMTIME now;
				GetLocalTime(&now);
				FILETIME ftNow;
				SystemTimeToFileTime(&now, &ftNow);
				ULARGE_INTEGER ulNow;
				ulNow.LowPart = ftNow.dwLowDateTime;
				ulNow.HighPart = ftNow.dwHighDateTime;
				
				bool hasActivePlan = false;
				for (auto& plan : g_shutdownPlans)
				{
					if (plan.isActive && !plan.isCancelledToday)
					{
						hasActivePlan = true;
						// 获取计划有效时间
						FILETIME planFt = GetPlanEffectiveTime(plan, now);
						SYSTEMTIME effectivePlanTime;
						FileTimeToSystemTime(&planFt, &effectivePlanTime);
						
						// 计算剩余秒数
						ULARGE_INTEGER ulPlan;
						ulPlan.LowPart = planFt.dwLowDateTime;
						ulPlan.HighPart = planFt.dwHighDateTime;
						__int64 diffSec = (ulPlan.QuadPart - ulNow.QuadPart) / 10000000LL; // 转秒
						
						// 到时间关机
						if (diffSec <= 0)
						{
							if (plan.isDaily)
							{
								plan.isActive = FALSE; // 当天停用
								plan.isCancelledToday = TRUE; // 标记当天已执行
							}
							else
							{
								plan.isActive = FALSE; // 非每日重复：永久停用
							}
							
							ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/s /t 0"), NULL, SW_HIDE);
							break;
						}
						// 显示警告窗口
						else if (diffSec <= plan.warningSeconds && !g_hShutdownWnd)
						{
							g_shutdownCountdown = static_cast<int>(diffSec);
							g_shutdownMessage = plan.message; // 记录当前计划消息（用于取消）
							g_hShutdownWnd = CreateShutdownWindow((HINSTANCE)GetModuleHandle(NULL));
						}
					}
				}

				// 清理已完成的计划
				if (!hasActivePlan)
				{
					// 只移除非每日重复且已停用的计划
					auto it = g_shutdownPlans.begin();
					while (it != g_shutdownPlans.end())
					{
						if (!it->isDaily && !it->isActive)
						{
							it = g_shutdownPlans.erase(it);
						}
						else
						{
							++it;
						}
					}
					KillTimer(hWnd, WM_TIMER_SHUTDOWN_CHECK);
				}
			}
			else if (wParam == WM_USER + 7)
			{
				BOOL partialTopmost = FALSE, fullTopmost = FALSE;
				// 重新获取置顶状态（可根据实际需求优化为全局变量）
				DWORD exStyle = GetWindowLongPtr(g_hMainWnd, GWL_EXSTYLE);
				fullTopmost = (exStyle & WS_EX_TOPMOST) != 0;
				if (!fullTopmost) partialTopmost = TRUE;

				if (partialTopmost)
				{
					SetWindowPos(g_hMainWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				}
				else if (fullTopmost)
				{
					SetWindowPos(g_hMainWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				}
			}
			else if (wParam == WM_USER + 8)
			{
				// 延迟1秒后关闭当前程序
				DestroyWindow(g_hMainWnd);
			}
			else if (wParam == WM_TIMER_DAILY_RESET)
			{
				// 零点重置每日计划状态
				SYSTEMTIME now;
				GetLocalTime(&now);
				for (auto& plan : g_shutdownPlans)
				{
					if (plan.isDaily)
					{
						plan.isCancelledToday = FALSE;
						plan.isActive = TRUE;
					}
				}
				
				// 重新计算明天零点延迟
				SYSTEMTIME nextResetTime = now;
				nextResetTime.wHour = 0;
				nextResetTime.wMinute = 0;
				nextResetTime.wSecond = 0;
				nextResetTime.wMilliseconds = 0;
				FILETIME ftNext;
				SystemTimeToFileTime(&nextResetTime, &ftNext);
				ULARGE_INTEGER ulNext;
				ulNext.LowPart = ftNext.dwLowDateTime;
				ulNext.HighPart = ftNext.dwHighDateTime;
				ulNext.QuadPart += 86400LL * 10000000LL; // 加1天
				ftNext.dwLowDateTime = ulNext.LowPart;
				ftNext.dwHighDateTime = ulNext.HighPart;
				FileTimeToSystemTime(&ftNext, &nextResetTime);
				
				// 计算新延迟
				FILETIME ftNow;
				SystemTimeToFileTime(&now, &ftNow);
				ULARGE_INTEGER ulNow;
				ulNow.LowPart = ftNow.dwLowDateTime;
				ulNow.HighPart = ftNow.dwHighDateTime;
				DWORD newDelay = (DWORD)((ulNext.QuadPart - ulNow.QuadPart) / 10000);
				
				// 重启定时器
				KillTimer(g_hMainWnd, WM_TIMER_DAILY_RESET);
				SetTimer(g_hMainWnd, WM_TIMER_DAILY_RESET, newDelay, NULL);
				
				// 重新调度关机检查
				ScheduleShutdown();
			}
			return 0;
		}

		case WM_TRAY_MESSAGE:
		{
			if (lParam == WM_LBUTTONDOWN)
			{
				// 左键点击显示窗口
				if (g_hMainWnd)
				{
					ShowWindow(g_hMainWnd, SW_RESTORE);
					SetForegroundWindow(g_hMainWnd);
				}
			}
			else if (lParam == WM_RBUTTONDOWN)
			{
				// 右键点击显示菜单
				POINT pt;
				GetCursorPos(&pt);

				HMENU hMenu = CreatePopupMenu();
				AppendMenu(hMenu, MF_STRING, 1, _T("显示信息窗口"));
				AppendMenu(hMenu, MF_STRING, 2, _T("退出"));

				SetForegroundWindow(hWnd);
				int cmd = TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD,
				                         pt.x, pt.y, 0, hWnd, NULL);
				PostMessage(hWnd, WM_NULL, 0, 0); // 确保菜单正确关闭

				if (cmd == 1)
				{
					if (g_hMainWnd)
					{
						ShowWindow(g_hMainWnd, SW_RESTORE);
						SetForegroundWindow(g_hMainWnd);
					}
				}
				else if (cmd == 2)
				{
					DestroyWindow(hWnd);
				}
				DestroyMenu(hMenu);
			}
			return 0;
		}

		case WM_DESTROY:
		{
			// 清理资源
			if (g_hDefaultFont)
			{
				DeleteObject(g_hDefaultFont);
				g_hDefaultFont = NULL;
			}

			RemoveTrayIcon(hWnd);

			// 取消关机计划
			CancelScheduledShutdown();

			PostQuitMessage(0);
			return 0;
		}

		case WM_SHUTDOWN_CANCEL:
		{
			// 处理取消关机消息
			CancelScheduledShutdown();
			return 0;
		}

		default:
		{
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
	}
}


// 修复ShutdownWndProc（关机命令参数）
LRESULT CALLBACK ShutdownWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rect;
			GetClientRect(hWnd, &rect);
			
			// 背景色改为红色(255,0,0)
			HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
			FillRect(hdc, &rect, hBrush);
			DeleteObject(hBrush);
			
			// -------------------------- 关键修改：调整提示文本 --------------------------
			// 第一行：固定文本（包含剩余秒数）
			string firstLine = "还剩" + to_string(g_shutdownCountdown) + "秒关机，点击窗口或关闭窗口来取消关机";
			// 第二行：处理[shutdown]的参数字符串（替换[rest]为剩余秒数）
			string secondLine = ProcessSpecialValues(g_shutdownMessage);
			// 拼接两行文本（用\n换行）
			string processedMsg = firstLine + "\n" + secondLine;
			// ----------------------------------------------------------------------------
			
			// 字体改为36号+白色
			HDC hScreenDC = GetDC(NULL);
			int fontHeight = -MulDiv(36, GetDeviceCaps(hScreenDC, LOGPIXELSY), 72);
			ReleaseDC(NULL, hScreenDC);
			HFONT hFont = CreateFontA(
									  fontHeight, 0, 0, 0, FW_BOLD, 
									  FALSE, FALSE, FALSE,
									  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
									  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, g_defaultFontName.c_str()
									  );
			HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
			SetTextColor(hdc, RGB(255, 255, 255));
			SetBkMode(hdc, TRANSPARENT);
			// 注意：保留 DT_WORDBREAK 确保换行生效
			DrawTextA(hdc, processedMsg.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
			SelectObject(hdc, hOldFont);
			DeleteObject(hFont);
			
			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_TIMER:
		{
			if (wParam == WM_SHUTDOWN_COUNTDOWN)
			{
				g_shutdownCountdown--;
				if (g_shutdownCountdown <= 0)
				{
					// 1. 停止定时器（必须）
					KillTimer(hWnd, WM_SHUTDOWN_COUNTDOWN);
					// 2. 执行关机命令（使用ShellExecute避免system函数阻塞）
					ShellExecute(NULL, _T("open"), _T("shutdown.exe"), _T("/s /t 0"), NULL, SW_HIDE);
					// 3. 销毁Shutdown窗口
					DestroyWindow(hWnd);
					g_hShutdownWnd = NULL;
				}
				else
				{
					InvalidateRect(hWnd, NULL, TRUE);
					UpdateWindow(hWnd);
				}
			}
			// 新增：处理置顶刷新定时器
			else if (wParam == WM_TIMER_SHUTDOWN_TOPMOST)
			{
				// 强制保持窗口置顶（覆盖其他窗口）
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			return 0;
		}
		
		case WM_LBUTTONDOWN:
		{
			CancelScheduledShutdown(); // 取消关机计划
			DestroyWindow(hWnd);      // 关闭提示窗口
			g_hShutdownWnd = NULL;
			return 0;
		}
		
		case WM_CLOSE:
		{
			KillTimer(hWnd, WM_TIMER_SHUTDOWN_TOPMOST);
			KillTimer(hWnd, WM_SHUTDOWN_COUNTDOWN);
			CancelScheduledShutdown(); // 确保关闭窗口时取消关机
			DestroyWindow(hWnd);
			g_hShutdownWnd = NULL;
			return 0;
		}

		case WM_DESTROY:
		{
			KillTimer(hWnd, WM_TIMER_SHUTDOWN_TOPMOST);  // 新增：清理置顶定时器
			g_hShutdownWnd = NULL;
			return 0;
		}

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

// string转wstring
wstring StringToWString(const string& s)
{
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(s);
}

// wstring转string
string WStringToString(const wstring& ws)
{
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(ws);
}
// 1. 替换时间变量（[yyyy]、[mm]等）为当前时间
string ReplaceTimeVariables(const string& input)
{
	string result = input;
	SYSTEMTIME now;
	GetLocalTime(&now); // 获取当前本地时间

	// 准备时间字符串
	string yyyy = to_string(now.wYear);
	string mm = to_string(now.wMonth);
	string dd = to_string(now.wDay);
	string m0 = toTwoDigits(now.wMonth);
	string d0 = toTwoDigits(now.wDay);
	string h = to_string(now.wHour);
	string h0 = toTwoDigits(now.wHour);
	int h12 = now.wHour - ((now.wHour > 12) ? 12 : 0) + ((now.wHour == 0) ? 12 : 0);
	//wHour 0  1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
	//h12   12 1 2 3 4 5 6 7 8 9 10 11 12 1  2  3  4  5  6  7  8  9  10 11
	string h12_str = to_string(h12);
	string h12_0 = toTwoDigits(h12);
	string minute = toTwoDigits(now.wMinute);
	string second = toTwoDigits(now.wSecond);
	string ms = to_string(now.wMilliseconds);
	if(ms.size()==1)ms=("00"+ms);
	if(ms.size()==2)ms=("0"+ms);

	// 使用字符串替换代替正则表达式，避免可能的换行符问题
	size_t pos = 0;
	while ((pos = result.find("[yyyy]", pos)) != string::npos)
	{
		result.replace(pos, 6, yyyy);
		pos += yyyy.length();
	}
	pos = 0;
	while ((pos = result.find("[mm]", pos)) != string::npos)
	{
		result.replace(pos, 4, mm);
		pos += mm.length();
	}
	pos = 0;
	while ((pos = result.find("[dd]", pos)) != string::npos)
	{
		result.replace(pos, 4, dd);
		pos += dd.length();
	}
	pos = 0;
	while ((pos = result.find("[0m]", pos)) != string::npos)
	{
		result.replace(pos, 4, m0);
		pos += m0.length();
	}
	pos = 0;
	while ((pos = result.find("[0d]", pos)) != string::npos)
	{
		result.replace(pos, 4, d0);
		pos += d0.length();
	}
	pos = 0;
	while ((pos = result.find("[h]", pos)) != string::npos)
	{
		result.replace(pos, 3, h);
		pos += h.length();
	}
	pos = 0;
	while ((pos = result.find("[0h]", pos)) != string::npos)
	{
		result.replace(pos, 4, h0);
		pos += h0.length();
	}
	pos = 0;
	while ((pos = result.find("[h12]", pos)) != string::npos)
	{
		result.replace(pos, 5, h12_str);
		pos += h12_str.length();
	}
	pos = 0;
	while ((pos = result.find("[0h12]", pos)) != string::npos)
	{
		result.replace(pos, 6, h12_0);
		pos += h12_0.length();
	}
	pos = 0;
	while ((pos = result.find("[m]", pos)) != string::npos)
	{
		result.replace(pos, 3, minute);
		pos += minute.length();
	}
	pos = 0;
	while ((pos = result.find("[s]", pos)) != string::npos)
	{
		result.replace(pos, 3, second);
		pos += second.length();
	}
	pos = 0;
	while ((pos = result.find("[ms]", pos)) != string::npos)
	{
		result.replace(pos, 4, ms);
		pos += ms.length();
	}

	return result;
}

// 2. 计划关机（管理多个关机计划）
// 修复ScheduleShutdown（确保定时器触发）
// 新增定时器ID定义（全局）

void ScheduleShutdown()
{
//	CancelScheduledShutdown(); // 先取消旧计划
	if (g_shutdownPlans.empty()) return;

	// ------------- 修复：每秒检查一次时间（替代长时间定时器） ------------- 
	SetTimer(g_hMainWnd, WM_TIMER_SHUTDOWN_CHECK, 1000, NULL);

	// 找到最近的活动关机计划
	ShutdownPlan* nearestPlan = nullptr;
	SYSTEMTIME now;
	GetLocalTime(&now);

	// 首先检查当天是否有未过的计划
	ShutdownPlan* todayPlan = nullptr;
	__int64 minTodayDiff = LLONG_MAX;
	
	// 然后检查明天的计划（适用于每日重复）
	ShutdownPlan* tomorrowPlan = nullptr;
	__int64 minTomorrowDiff = LLONG_MAX;

	ULARGE_INTEGER ulNow;
	FILETIME ftNow;
	SystemTimeToFileTime(&now, &ftNow);
	ulNow.LowPart = ftNow.dwLowDateTime;
	ulNow.HighPart = ftNow.dwHighDateTime;

	for (auto& plan : g_shutdownPlans)
	{
		if (plan.isActive && !plan.isCancelledToday)
		{
			// 创建临时计划副本，用于计算当天和明天的时间
			ShutdownPlan tempPlan = plan;
			FILETIME currentFt, tomorrowFt;
			
			// 计算当天的计划时间
			SYSTEMTIME tempTime = tempPlan.shutdownTime;
			tempTime.wYear = now.wYear;
			tempTime.wMonth = now.wMonth;
			tempTime.wDay = now.wDay;
			SystemTimeToFileTime(&tempTime, &currentFt);
			
			ULARGE_INTEGER ulCurrent;
			ulCurrent.LowPart = currentFt.dwLowDateTime;
			ulCurrent.HighPart = currentFt.dwHighDateTime;
			
			// 计算明天的计划时间（如果是每日重复）
			FILETIME tomorrowFtCalc;
			if (plan.isDaily)
			{
				SYSTEMTIME tempTomorrow = tempTime;
				// 简单地将日期加1（这里简化处理，实际应用中可能需要更复杂的日期计算）
				// 对于完整解决方案，应该使用SystemTimeToFileTime、文件时间加一天、再转回系统时间
				ULARGE_INTEGER ulTemp;
				ulTemp.LowPart = currentFt.dwLowDateTime;
				ulTemp.HighPart = currentFt.dwHighDateTime;
				ulTemp.QuadPart += 86400LL * 10000000LL; // 加1天
				tomorrowFtCalc.dwLowDateTime = ulTemp.LowPart;
				tomorrowFtCalc.dwHighDateTime = ulTemp.HighPart;
			}
			
			// 检查当天是否还有未过的计划
			__int64 todayDiff = (__int64)ulCurrent.QuadPart - ulNow.QuadPart;
			if (todayDiff >= 0 && todayDiff < minTodayDiff)
			{
				minTodayDiff = todayDiff;
			todayPlan = &plan;
			}
			// 同时也记录明天的最近计划（如果是每日重复）
			else if (plan.isDaily)
			{
				ULARGE_INTEGER ulTomorrow;
				ulTomorrow.LowPart = tomorrowFtCalc.dwLowDateTime;
				ulTomorrow.HighPart = tomorrowFtCalc.dwHighDateTime;
				__int64 tomorrowDiff = ulTomorrow.QuadPart - ulNow.QuadPart;
				if (tomorrowDiff < minTomorrowDiff)
				{
					minTomorrowDiff = tomorrowDiff;
					tomorrowPlan = &plan;
				}
			}
		}
	}
	
	// 优先选择当天未过的计划，否则选择明天的计划
	if (todayPlan)
	{
		nearestPlan = todayPlan;
	}
	else if (tomorrowPlan)
	{
		nearestPlan = tomorrowPlan;
	}
	else
	{
		// 如果没有找到计划，尝试使用原来的逻辑作为备选
		for (auto& plan : g_shutdownPlans)
		{
			if (plan.isActive && !plan.isCancelledToday)
			{
				FILETIME currentFt = GetPlanEffectiveTime(plan, now);
				if (!nearestPlan)
				{
					nearestPlan = &plan;
				}
				else
				{
					FILETIME nearestFt = GetPlanEffectiveTime(*nearestPlan, now);
					ULARGE_INTEGER ulCurrent, ulNearest, ulNowFt;
					FILETIME ftNowCalc;
					SystemTimeToFileTime(&now, &ftNowCalc);
					ulNowFt.LowPart = ftNowCalc.dwLowDateTime;
					ulNowFt.HighPart = ftNowCalc.dwHighDateTime;
					ulCurrent.LowPart = currentFt.dwLowDateTime;
					ulCurrent.HighPart = currentFt.dwHighDateTime;
					ulNearest.LowPart = nearestFt.dwLowDateTime;
					ulNearest.HighPart = nearestFt.dwHighDateTime;
					
					__int64 diffCurrent = llabs((__int64)ulCurrent.QuadPart - (__int64)ulNowFt.QuadPart);
					__int64 diffNearest = llabs((__int64)ulNearest.QuadPart - (__int64)ulNowFt.QuadPart);
					
					if (diffCurrent < diffNearest)
					{
						nearestPlan = &plan;
					}
				}
			}
		}
	}

	// 托盘提示（显示最近的关机计划）
	if (nearestPlan)
	{
		TCHAR tipTitle[64], tipMsg[256];
		_stprintf_s(tipTitle, _T("已设置关机计划"));
		_stprintf_s(tipMsg, _T("将于%02d:%02d:%02d关机，提前%d秒提醒"),
		            nearestPlan->shutdownTime.wHour, nearestPlan->shutdownTime.wMinute, nearestPlan->shutdownTime.wSecond,
		            nearestPlan->warningSeconds);
		ShowTrayBalloon(g_hMainWnd, tipTitle, tipMsg, NIIF_INFO);
	}
}

// 3. 取消已计划的关机
void CancelScheduledShutdown()
{
	// 1. 停止关机提醒定时器
	KillTimer(g_hMainWnd, WM_TIMER_SHUTDOWN_REMIND);
	
	// 2. 销毁关机提示窗口
	if (g_hShutdownWnd)
	{
		KillTimer(g_hShutdownWnd, WM_SHUTDOWN_COUNTDOWN);
		DestroyWindow(g_hShutdownWnd);
		g_hShutdownWnd = NULL;
	}
	
	// 3. 只取消当前正在倒计时的计划，不影响其他计划
	SYSTEMTIME now;
	GetLocalTime(&now);
	ShutdownPlan* currentPlan = nullptr;
	__int64 minDiff = LLONG_MAX;
	ULARGE_INTEGER ulNow;
	FILETIME ftNow;
	SystemTimeToFileTime(&now, &ftNow);
	ulNow.LowPart = ftNow.dwLowDateTime;
	ulNow.HighPart = ftNow.dwHighDateTime;
	
	for (auto& plan : g_shutdownPlans)
	{
		if (plan.isActive && !plan.isCancelledToday)
		{
			FILETIME planFt = GetPlanEffectiveTime(plan, now);
			ULARGE_INTEGER ulPlan;
			ulPlan.LowPart = planFt.dwLowDateTime;
			ulPlan.HighPart = planFt.dwHighDateTime;
			__int64 diff = llabs((__int64)ulPlan.QuadPart - ulNow.QuadPart);
			
			// 优先匹配当前倒计时的计划（通过消息）
			if (!g_shutdownMessage.empty() && plan.message == g_shutdownMessage)
			{
				currentPlan = &plan;
				break;
			}
			// 其次找最近的计划
			else if (diff < minDiff)
			{
				minDiff = diff;
				currentPlan = &plan;
			}
		}
	}
	
	if (currentPlan)
	{
		// 重要：仅标记为当天取消，不影响明天的执行
		currentPlan->isCancelledToday = TRUE; 
		currentPlan->isActive = FALSE; // 当天停用该计划
	}
	
	// 重置当前倒计时状态，但保留其他计划
	g_shutdownCountdown = 0;
	g_shutdownMessage.clear();
	
	// 4. 提示用户
	ShowTrayBalloon(g_hMainWnd, _T("已取消当前关机计划"), _T("当前计划已取消，其他计划保持不变"), NIIF_INFO);
	
	// 5. 查找并准备下一个可用的关机计划
	ScheduleShutdown();
}

// 异步启动程序（appPath可带参数）
void StartASyncAPP(const string& appPath)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb=sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// 创建可修改的命令行字符串
	char cmdLine[MAX_PATH];
	strcpy(cmdLine, appPath.c_str());

	// 创建进程
	if (!CreateProcess(
	        NULL,       // 应用程序名（使用命令行）
	        cmdLine,    // 命令行
	        NULL,       // 进程句柄不可继承
	        NULL,       // 线程句柄不可继承
	        FALSE,      // 不继承句柄
	        0,          // 无创建标志
	        NULL,       // 使用父进程环境
	        NULL,       // 使用父进程工作目录
	        &si,        // STARTUPINFO 结构
	        &pi         // PROCESS_INFORMATION 结构
	    ))
	{
		// 错误处理
		DWORD err=GetLastError();
//	cerr<< "创建进程失败! 错误代码: "<<err<<endl;
		return;
	}

	// 关闭不再需要的句柄
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

// 启动程序并捕获输出（隐藏窗口）
string StartAppAndGetOutput(const string& appPath)
{
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hStdOutRead = NULL;
	HANDLE hStdOutWrite = NULL;

	// 创建匿名管道用于捕获输出
	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0))
	{
		return "error: 创建管道失败";
	}

	// 确保读句柄不被继承
	if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0))
	{
		CloseHandle(hStdOutRead);
		CloseHandle(hStdOutWrite);
		return "error: 设置句柄信息失败";
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = hStdOutWrite;
	si.hStdError = hStdOutWrite;
	si.wShowWindow = SW_HIDE; // 隐藏窗口
	ZeroMemory(&pi, sizeof(pi));

	// 创建可修改的命令行字符串
	char cmdLine[MAX_PATH];
	strcpy(cmdLine, appPath.c_str());

	// 创建进程
	if (!CreateProcess(
	        NULL,       // 应用程序名（使用命令行）
	        cmdLine,    // 命令行
	        NULL,       // 进程句柄不可继承
	        NULL,       // 线程句柄不可继承
	        TRUE,       // 继承句柄
	        0,          // 无创建标志
	        NULL,       // 使用父进程环境
	        NULL,       // 使用父进程工作目录
	        &si,        // STARTUPINFO 结构
	        &pi         // PROCESS_INFORMATION 结构
	    ))
	{
		DWORD err = GetLastError();
		CloseHandle(hStdOutRead);
		CloseHandle(hStdOutWrite);
		return "error: 创建进程失败: " + to_string(err);
	}

	// 关闭不再需要的句柄
	CloseHandle(hStdOutWrite);

	// 读取输出
	string output;
	CHAR buffer[4096];
	DWORD bytesRead;

	// 等待进程结束，但设置超时（最多等待1秒）
	DWORD waitResult = WaitForSingleObject(pi.hProcess, 1000); // 1秒超时
	if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_TIMEOUT)
	{
		// 无论进程是否超时，都尝试读取输出
		while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			output += buffer;
		}
	}

	// 关闭句柄
	CloseHandle(hStdOutRead);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return output;
}

// 辅助函数：计算计划的有效时间（已过则加1天，支持每日重复）
FILETIME GetPlanEffectiveTime(const ShutdownPlan& plan, const SYSTEMTIME& now)
{
	SYSTEMTIME planTime = plan.shutdownTime;
	// 同步计划日期为当前日期
	planTime.wYear = now.wYear;
	planTime.wMonth = now.wMonth;
	planTime.wDay = now.wDay;
	
	FILETIME ftPlan, ftNow;
	SystemTimeToFileTime(&planTime, &ftPlan);
	SystemTimeToFileTime(&now, &ftNow);
	
	ULARGE_INTEGER ulPlan, ulNow;
	ulPlan.LowPart = ftPlan.dwLowDateTime;
	ulPlan.HighPart = ftPlan.dwHighDateTime;
	ulNow.LowPart = ftNow.dwLowDateTime;
	ulNow.HighPart = ftNow.dwHighDateTime;
	
	// 只有当计划时间已过且当天没有其他更晚的计划时，才视为第二天
	// （这个判断在ScheduleShutdown函数中完成，这里仅处理单个计划的时间计算）
	if (ulPlan.QuadPart < ulNow.QuadPart && plan.isDaily && !plan.isCancelledToday)
	{
		// 查看当天是否还有其他更晚的计划（这个逻辑将在ScheduleShutdown中处理）
		// 这里保持原逻辑，但会在ScheduleShutdown中优化选择最近计划的算法
		ulPlan.QuadPart += 86400LL * 10000000LL; // 1天 = 86400秒（转纳秒）
	}
	
	FILETIME ftEffective;
	ftEffective.dwLowDateTime = ulPlan.LowPart;
	ftEffective.dwHighDateTime = ulPlan.HighPart;
	return ftEffective;
}
