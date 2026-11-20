#pragma once
// Утилита: выполнить лямбду на UI-потоке CEF.

#include <functional>
#include <utility>

#include "include/cef_task.h"

namespace crumb {

class FunctionTask final : public CefTask {
 public:
  explicit FunctionTask(std::function<void()> fn) : fn_(std::move(fn)) {}
  void Execute() override {
    if (fn_) fn_();
  }

 private:
  std::function<void()> fn_;
  IMPLEMENT_REFCOUNTING(FunctionTask);
};

inline void postToCefUi(std::function<void()> fn) {
  if (CefCurrentlyOn(TID_UI))
    fn();
  else
    CefPostTask(TID_UI, new FunctionTask(std::move(fn)));
}

}  // namespace crumb
