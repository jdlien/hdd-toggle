#pragma once
// COM utilities for HDD Toggle
// Shared smart pointers and RAII wrappers for COM objects

#ifndef HDD_CORE_COM_H
#define HDD_CORE_COM_H

#include <windows.h>
#include <objbase.h>

namespace hdd {
namespace core {

// Simple COM smart pointer template
// Use this instead of raw COM pointers for automatic Release() on scope exit
template<typename T>
class ComPtr {
public:
    ComPtr() : m_ptr(nullptr) {}
    ~ComPtr() { Release(); }

    T** operator&() { return &m_ptr; }
    T* operator->() { return m_ptr; }
    T* Get() const { return m_ptr; }
    operator bool() const { return m_ptr != nullptr; }

    void Release() {
        if (m_ptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

private:
    // Non-copyable
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* m_ptr;
};

// RAII wrapper for COM initialization
// Calls CoInitializeEx on construction and CoUninitialize on destruction
class ComInitializer {
public:
    ComInitializer(DWORD dwCoInit = COINIT_MULTITHREADED) : m_initialized(false) {
        HRESULT hr = CoInitializeEx(0, dwCoInit);
        m_initialized = SUCCEEDED(hr);
    }

    ~ComInitializer() {
        if (m_initialized) {
            CoUninitialize();
        }
    }

    bool IsInitialized() const { return m_initialized; }

    // Non-copyable
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

private:
    bool m_initialized;
};

} // namespace core
} // namespace hdd

#endif // HDD_CORE_COM_H
