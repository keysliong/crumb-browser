#pragma once
// CefApp основного (browser) процесса.

#include "include/cef_app.h"

namespace crumb {

class CrumbCefApp final : public CefApp, public CefBrowserProcessHandler {
 public:
  CrumbCefApp() = default;

  // CefApp
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
  void OnBeforeCommandLineProcessing(const CefString& process_type,
                                     CefRefPtr<CefCommandLine> command_line) override;

  // CefBrowserProcessHandler (вызываются на UI-потоке CEF)
  void OnContextInitialized() override;
  // Повторный запуск Crumb.exe, пока он уже открыт: CEF передаёт сюда
  // командную строку второго экземпляра. Возвращаем true, иначе CEF сам
  // откроет стандартное окно Chrome.
  bool OnAlreadyRunningAppRelaunch(CefRefPtr<CefCommandLine> command_line,
                                   const CefString& current_directory) override;
  // Chromium иногда создаёт браузер сам (восстановление сессии после сбоя,
  // команды Chrome). Такие окна нам не нужны: клиент по умолчанию закрывает
  // их сразу, а адрес передаёт интерфейсу Crumb.
  CefRefPtr<CefClient> GetDefaultClient() override;

 private:
  CefRefPtr<CefClient> strayClient_;
  IMPLEMENT_REFCOUNTING(CrumbCefApp);
};

}  // namespace crumb
