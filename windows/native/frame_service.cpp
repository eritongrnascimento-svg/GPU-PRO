// gpupro-frames: contador de quadros do GPU-PRO para Windows, sem programas externos.
//
// Lê eventos de apresentação do próprio Windows (ETW) e entrega, a cada 250 ms, o tempo entre
// quadros de cada processo pelo pipe \\.\pipe\GPU-PRO-frames (somente saída, uma linha JSON por envio).
//   - Microsoft-Windows-DXGI, evento 42 (Present_Start): chamada de Present do DirectX 10/11/12.
//   - Microsoft-Windows-D3D9, evento 1 (Present_Start): DirectX 9.
//   - Microsoft-Windows-DxgKrnl, evento 184 (Present_Info): uma vez por quadro em qualquer API
//     (DirectX, OpenGL e Vulkan); usado quando o processo não emite os eventos anteriores.
// IDs conferidos com as definições públicas do PresentMon (MIT) e medidos com DirectX 11, OpenGL e
// Vulkan a 60 Hz. O Windows só permite essa leitura a administradores, por isso o programa roda como
// serviço (instalado uma vez) ou como processo elevado ligado à sessão do aplicativo.
//
// Uso: --service (SCM) · --console [--parent PID] · --install · --uninstall · --probe SEGUNDOS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <sddl.h>
#include <shlobj.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

const wchar_t *kService = L"GPUPROFrames";
const wchar_t *kDisplay = L"GPU-PRO · Contador de quadros";
const wchar_t *kSession = L"GPU-PRO-Frames";
const wchar_t *kPipe = L"\\\\.\\pipe\\GPU-PRO-frames";
const GUID kDxgi = {0xCA11C036, 0x0102, 0x4A2D, {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}};
const GUID kD3d9 = {0x783ACA0A, 0x790E, 0x4D7F, {0x84, 0x51, 0xAA, 0x85, 0x05, 0x11, 0xC6, 0xB9}};
const GUID kKernel = {0x802EC45A, 0x1E99, 0x4B83, {0x99, 0x20, 0x87, 0xC9, 0x82, 0x77, 0xBA, 0x9D}};
constexpr unsigned long long kTicks = 10000000ull;  // marcas de tempo do ETW em unidades de 100 ns

struct Process {
  unsigned long long lastApi = 0, last = 0, seen = 0;
  std::vector<float> pending;
  const char *source = "";
  unsigned long long frames = 0;
};

std::mutex procMutex;
std::unordered_map<ULONG, Process> procs;
std::map<ULONG, std::string> names;

void Count(ULONG pid, unsigned long long ts, bool api, const char *source) {
  if (pid <= 4) return;
  std::lock_guard lock(procMutex);
  auto &p = procs[pid];
  // DXGI/D3D9 marcam o instante da chamada de Present; com eles presentes, o evento do kernel
  // do mesmo quadro é ignorado para não contar duas vezes.
  if (!api && p.lastApi && ts - p.lastApi < kTicks) return;
  if (api) p.lastApi = ts;
  if (p.last && ts > p.last) {
    double ms = (ts - p.last) / 10000.0;
    if (ms < 5000 && p.pending.size() < 4096) p.pending.push_back(static_cast<float>(ms));
  }
  p.last = p.seen = ts;
  p.source = source;
  ++p.frames;
}

void WINAPI OnEvent(PEVENT_RECORD e) {
  const auto &h = e->EventHeader;
  const unsigned long long ts = static_cast<unsigned long long>(h.TimeStamp.QuadPart);
  if (IsEqualGUID(h.ProviderId, kDxgi) && h.EventDescriptor.Id == 42) {
    const size_t pointer = (h.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8;
    if (e->UserDataLength >= pointer + 4) {
      UINT32 flags;
      memcpy(&flags, static_cast<const char *>(e->UserData) + pointer, 4);
      if (flags & 0x1) return;  // DXGI_PRESENT_TEST: só testa oclusão, não apresenta quadro
    }
    Count(h.ProcessId, ts, true, "DXGI");
  } else if (IsEqualGUID(h.ProviderId, kD3d9) && h.EventDescriptor.Id == 1) {
    Count(h.ProcessId, ts, true, "D3D9");
  } else if (IsEqualGUID(h.ProviderId, kKernel) && h.EventDescriptor.Id == 184) {
    Count(h.ProcessId, ts, false, "Kernel");
  }
}

// --- Sessão ETW -------------------------------------------------------------------------------

struct Session {
  TRACEHANDLE session = 0, trace = INVALID_PROCESSTRACE_HANDLE;
  std::vector<char> props;
  std::thread worker;

  EVENT_TRACE_PROPERTIES *Props() {
    props.assign(sizeof(EVENT_TRACE_PROPERTIES) + 1024, 0);
    auto *p = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(props.data());
    p->Wnode.BufferSize = static_cast<ULONG>(props.size());
    p->Wnode.ClientContext = 1;  // QPC
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    p->BufferSize = 64;
    p->MinimumBuffers = 16;
    p->FlushTimer = 1;
    return p;
  }

  static ULONG Enable(TRACEHANDLE s, const GUID &g, ULONGLONG keywords, USHORT id) {
    std::vector<char> buffer(sizeof(EVENT_FILTER_EVENT_ID), 0);
    auto *f = reinterpret_cast<EVENT_FILTER_EVENT_ID *>(buffer.data());
    f->FilterIn = TRUE;
    f->Count = 1;
    f->Events[0] = id;
    EVENT_FILTER_DESCRIPTOR desc{reinterpret_cast<ULONGLONG>(buffer.data()), static_cast<ULONG>(buffer.size()), EVENT_FILTER_TYPE_EVENT_ID};
    ENABLE_TRACE_PARAMETERS params{};
    params.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    params.EnableFilterDesc = &desc;
    params.FilterDescCount = 1;
    return EnableTraceEx2(s, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, keywords, 0, 0, &params);
  }

  ULONG Start() {
    ControlTraceW(0, kSession, Props(), EVENT_TRACE_CONTROL_STOP);  // sessão órfã de execução anterior
    ULONG status = StartTraceW(&session, kSession, Props());
    if (status != ERROR_SUCCESS) return status;
    Enable(session, kDxgi, ~0ull, 42);
    Enable(session, kD3d9, ~0ull, 1);
    // Base | Present, mais o canal Performance onde o evento é publicado.
    if ((status = Enable(session, kKernel, 0x1ull | 0x8000000ull | 0x4000000000000000ull, 184)) != ERROR_SUCCESS) {
      Stop();
      return status;
    }
    EVENT_TRACE_LOGFILEW log{};
    log.LoggerName = const_cast<LPWSTR>(kSession);
    log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    log.EventRecordCallback = OnEvent;
    trace = OpenTraceW(&log);
    if (trace == INVALID_PROCESSTRACE_HANDLE) {
      status = GetLastError();
      Stop();
      return status;
    }
    worker = std::thread([this] { ProcessTrace(&trace, 1, nullptr, nullptr); });
    return ERROR_SUCCESS;
  }

  void Stop() {
    if (session) ControlTraceW(session, nullptr, Props(), EVENT_TRACE_CONTROL_STOP);
    session = 0;
    if (trace != INVALID_PROCESSTRACE_HANDLE) CloseTrace(trace);
    trace = INVALID_PROCESSTRACE_HANDLE;
    if (worker.joinable()) worker.join();
  }
};

// --- Nomes de processo e JSON -----------------------------------------------------------------

std::string Utf8(const std::wstring &w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

std::string Name(ULONG pid) {
  if (auto it = names.find(pid); it != names.end()) return it->second;
  std::string name;
  if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
    wchar_t path[MAX_PATH * 2];
    DWORD size = MAX_PATH * 2;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
      std::wstring full(path, size);
      name = Utf8(full.substr(full.find_last_of(L'\\') + 1));
    }
    CloseHandle(h);
  }
  return names[pid] = name;
}

std::string Escape(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) out += c;
  }
  return out;
}

// Retira os tempos acumulados desde o último envio e descarta processos parados há 10 s.
std::string Drain(unsigned long long now) {
  std::string line = "{\"v\":1,\"p\":[";
  bool first = true;
  std::lock_guard lock(procMutex);
  for (auto it = procs.begin(); it != procs.end();) {
    auto &[pid, p] = *it;
    if (now > p.seen && now - p.seen > 10 * kTicks) {
      names.erase(pid);
      it = procs.erase(it);
      continue;
    }
    if (!p.pending.empty()) {
      char head[96];
      snprintf(head, sizeof head, "%s{\"pid\":%lu,\"s\":\"%s\",\"n\":\"", first ? "" : ",", pid, p.source);
      line += head + Escape(Name(pid)) + "\",\"f\":[";
      for (size_t i = 0; i < p.pending.size(); ++i) {
        char v[24];
        snprintf(v, sizeof v, i ? ",%.3f" : "%.3f", p.pending[i]);
        line += v;
      }
      line += "]}";
      first = false;
      p.pending.clear();
    }
    ++it;
  }
  return line + "]}\n";
}

unsigned long long Now() {
  FILETIME ft;
  GetSystemTimePreciseAsFileTime(&ft);
  return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// --- Servidor do pipe -------------------------------------------------------------------------

std::mutex lineMutex;
std::condition_variable lineReady;
std::string latest;
unsigned long long sequence = 0;
std::atomic<int> clients{0};
std::atomic<bool> stopping{false};
HANDLE stopEvent = nullptr;

void Client(HANDLE pipe) {
  ++clients;
  unsigned long long seen = 0;
  while (!stopping) {
    std::string line;
    {
      std::unique_lock lock(lineMutex);
      lineReady.wait_for(lock, std::chrono::seconds(1), [&] { return sequence != seen || stopping; });
      if (sequence == seen) continue;
      seen = sequence;
      line = latest;
    }
    DWORD written = 0;
    if (!WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr)) break;
  }
  DisconnectNamedPipe(pipe);
  CloseHandle(pipe);
  --clients;
}

void Ticker() {
  Session session;
  bool active = false;
  unsigned long long idleSince = 0;
  while (WaitForSingleObject(stopEvent, 250) == WAIT_TIMEOUT) {
    const unsigned long long now = Now();
    if (clients > 0 && !active) {
      active = session.Start() == ERROR_SUCCESS;  // só lê eventos enquanto há alguém conectado
      idleSince = 0;
    } else if (clients == 0 && active) {
      if (!idleSince) idleSince = now;
      if (now - idleSince > 10 * kTicks) {
        session.Stop();
        active = false;
        std::lock_guard lock(procMutex);
        procs.clear();
      }
    }
    std::string line = active ? Drain(now) : std::string("{\"v\":1,\"p\":[],\"erro\":\"sessao\"}\n");
    {
      std::lock_guard lock(lineMutex);
      latest = std::move(line);
      ++sequence;
    }
    lineReady.notify_all();
  }
  stopping = true;
  lineReady.notify_all();
  session.Stop();
}

void Serve() {
  stopping = false;
  std::thread ticker(Ticker);
  // Sistema e administradores: total; usuários interativos: somente leitura.
  PSECURITY_DESCRIPTOR sd = nullptr;
  ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;IU)", SDDL_REVISION_1, &sd, nullptr);
  SECURITY_ATTRIBUTES sa{sizeof sa, sd, FALSE};
  HANDLE connected = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  while (!stopping) {
    HANDLE pipe = CreateNamedPipeW(kPipe, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
                                   PIPE_UNLIMITED_INSTANCES, 65536, 0, 0, &sa);
    if (pipe == INVALID_HANDLE_VALUE) {
      if (WaitForSingleObject(stopEvent, 1000) == WAIT_OBJECT_0) break;
      continue;
    }
    OVERLAPPED ov{};
    ov.hEvent = connected;
    ResetEvent(connected);
    BOOL ok = ConnectNamedPipe(pipe, &ov);
    DWORD error = GetLastError();
    if (!ok && error == ERROR_IO_PENDING) {
      HANDLE waits[] = {connected, stopEvent};
      if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0) {
        CancelIo(pipe);
        CloseHandle(pipe);
        break;
      }
    } else if (!ok && error != ERROR_PIPE_CONNECTED) {
      CloseHandle(pipe);
      continue;
    }
    std::thread(Client, pipe).detach();
  }
  SetEvent(stopEvent);
  ticker.join();
  CloseHandle(connected);
  LocalFree(sd);
}

// --- Serviço do Windows -----------------------------------------------------------------------

SERVICE_STATUS_HANDLE statusHandle = nullptr;
SERVICE_STATUS status{SERVICE_WIN32_OWN_PROCESS};

void Report(DWORD state) {
  status.dwCurrentState = state;
  status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
  SetServiceStatus(statusHandle, &status);
}

DWORD WINAPI Control(DWORD code, DWORD, LPVOID, LPVOID) {
  if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN) {
    Report(SERVICE_STOP_PENDING);
    SetEvent(stopEvent);
  }
  return NO_ERROR;
}

void WINAPI ServiceMain(DWORD, LPWSTR *) {
  statusHandle = RegisterServiceCtrlHandlerExW(kService, Control, nullptr);
  if (!statusHandle) return;
  stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  Report(SERVICE_RUNNING);
  Serve();
  Report(SERVICE_STOPPED);
}

bool Elevated() {
  HANDLE token;
  TOKEN_ELEVATION elevation{};
  DWORD size = 0;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  GetTokenInformation(token, TokenElevation, &elevation, sizeof elevation, &size);
  CloseHandle(token);
  return elevation.TokenIsElevated != 0;
}

std::wstring InstallPath() {
  wchar_t base[MAX_PATH];
  GetEnvironmentVariableW(L"ProgramW6432", base, MAX_PATH) || GetEnvironmentVariableW(L"ProgramFiles", base, MAX_PATH);
  return std::wstring(base) + L"\\GPU-PRO\\FrameService";
}

bool StopService(SC_HANDLE service) {
  SERVICE_STATUS s{};
  ControlService(service, SERVICE_CONTROL_STOP, &s);
  for (int i = 0; i < 50 && QueryServiceStatus(service, &s) && s.dwCurrentState != SERVICE_STOPPED; ++i) Sleep(100);
  return s.dwCurrentState == SERVICE_STOPPED;
}

int Install() {
  if (!Elevated()) return fwprintf(stderr, L"É preciso executar como administrador.\n"), 5;
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
  if (!scm) return static_cast<int>(GetLastError());
  if (SC_HANDLE old = OpenServiceW(scm, kService, SERVICE_ALL_ACCESS)) {
    StopService(old);
    CloseServiceHandle(old);
  }
  // O binário do serviço fica em Arquivos de Programas, que só administradores podem alterar;
  // um serviço do sistema nunca executa um arquivo de uma pasta gravável pelo usuário.
  const std::wstring dir = InstallPath(), target = dir + L"\\gpupro-frames.exe";
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  wchar_t self[MAX_PATH * 2];
  GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
  if (_wcsicmp(self, target.c_str()) && !CopyFileW(self, target.c_str(), FALSE)) {
    DWORD e = GetLastError();
    CloseServiceHandle(scm);
    return static_cast<int>(e);
  }
  const std::wstring command = L"\"" + target + L"\" --service";
  SC_HANDLE service = OpenServiceW(scm, kService, SERVICE_ALL_ACCESS);
  if (service) {
    ChangeServiceConfigW(service, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, kDisplay);
  } else {
    service = CreateServiceW(scm, kService, kDisplay, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                             command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);
  }
  if (!service) {
    DWORD e = GetLastError();
    CloseServiceHandle(scm);
    return static_cast<int>(e);
  }
  SERVICE_DESCRIPTIONW description{const_cast<LPWSTR>(L"Conta os quadros apresentados pelos jogos para o FPS do GPU-PRO. Só lê eventos do Windows "
                                                      L"enquanto o GPU-PRO está aberto; não altera jogos nem drivers.")};
  ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description);
  SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
  ChangeServiceConfig2W(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);
  SC_ACTION actions[] = {{SC_ACTION_RESTART, 5000}, {SC_ACTION_RESTART, 30000}, {SC_ACTION_NONE, 0}};
  SERVICE_FAILURE_ACTIONSW failure{86400, nullptr, nullptr, 3, actions};
  ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);
  BOOL started = StartServiceW(service, 0, nullptr) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  return started ? 0 : static_cast<int>(GetLastError());
}

int Uninstall() {
  if (!Elevated()) return fwprintf(stderr, L"É preciso executar como administrador.\n"), 5;
  if (SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS)) {
    if (SC_HANDLE service = OpenServiceW(scm, kService, SERVICE_ALL_ACCESS)) {
      StopService(service);
      DeleteService(service);
      CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
  }
  const std::wstring dir = InstallPath();
  DeleteFileW((dir + L"\\gpupro-frames.exe").c_str());
  RemoveDirectoryW(dir.c_str());
  std::wstring parent = dir.substr(0, dir.find_last_of(L'\\'));
  RemoveDirectoryW(parent.c_str());  // só remove se estiver vazia
  return 0;
}

// Diagnóstico: mede por alguns segundos e mostra FPS médio por processo.
int Probe(int seconds) {
  Session session;
  if (ULONG e = session.Start()) return fwprintf(stderr, L"Sessão ETW recusada (%lu). Execute como administrador.\n", e), static_cast<int>(e);
  Sleep(seconds * 1000);
  session.Stop();
  std::lock_guard lock(procMutex);
  for (auto &[pid, p] : procs)
    if (p.frames > static_cast<unsigned long long>(seconds))
      printf("%6lu %-32s %-6s %6llu quadros  %.1f FPS\n", pid, Name(pid).c_str(), p.source, p.frames, p.frames / static_cast<double>(seconds));
  return 0;
}

}  // namespace

int wmain(int argc, wchar_t **argv) {
  std::wstring mode = argc > 1 ? argv[1] : L"";
  if (mode == L"--service") {
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(kService), ServiceMain}, {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(table) ? 0 : static_cast<int>(GetLastError());
  }
  if (mode == L"--install") return Install();
  if (mode == L"--uninstall") return Uninstall();
  if (mode == L"--probe") return Probe(argc > 2 ? std::max(1, _wtoi(argv[2])) : 10);
  if (mode == L"--console") {
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    // Ligado à sessão do aplicativo: encerra quando o GPU-PRO que o abriu fechar.
    if (argc > 3 && std::wstring(argv[2]) == L"--parent") {
      if (HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(_wtoi(argv[3])))) {
        std::thread([parent] {
          WaitForSingleObject(parent, INFINITE);
          SetEvent(stopEvent);
        }).detach();
      }
    }
    SetConsoleCtrlHandler([](DWORD) -> BOOL { SetEvent(stopEvent); return TRUE; }, TRUE);
    Serve();
    return 0;
  }
  fwprintf(stderr, L"gpupro-frames --service | --console [--parent PID] | --install | --uninstall | --probe SEGUNDOS\n");
  return 2;
}
